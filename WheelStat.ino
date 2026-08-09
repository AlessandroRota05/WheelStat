/*
  ===========================================================================
  WheelStat - Alessandro Rota - Apache License 2.0 - v9.16
  ===========================================================================
  Telemetria per moto su ESP32: angolo di piega, impennata/stoppie, forze
  G, meteo e rischio grip, posizione e lap timing GPS. I dati si vedono in
  tempo reale su OLED (10 pagine) e sul sito servito da un access point
  WiFi, e si registrano in CSV sulla flash interna.

  STRUTTURA
    Config.h            scelta dei driver e configurazione hardware
    WheelStat.ino       questo file: il "motore". Sensori, filtro
                        anti-buca, statistiche/eventi/rischio grip, lap
                        timing, registrazione su flash, tracciati. Tutto
                        cio' che non disegna niente.
    PagineOled.ino      le pagine OLED e i loro helper di disegno
    InterfacciaWeb.ino  il sito: CSS, pagina live, tracciati, CSV/GPX
    src/driver/*.h           un file per categoria di componente (meteo,
                        display, GPS, IMU): dentro, un blocco #if per
                        chip. Ne viene compilato uno solo per categoria.

  Arduino concatena tutti i .ino della cartella in un unico programma, in
  quest'ordine: prima il file principale, poi gli altri in ordine
  alfabetico. Funzioni e variabili globali restano quindi visibili fra un
  file e l'altro. Per compilare, i file devono stare tutti nella STESSA
  cartella, chiamata esattamente "WheelStat".

  DOVE GUARDARE
    README.md                      montaggio, uso, pagine, sito, formato CSV
    docs/DRIVER.md                 come funzionano i driver e come aggiungerne uno
    docs/HARDWARE.md               componenti alternativi e trappole d'acquisto
    docs/CHANGELOG.md              storico delle versioni
  ===========================================================================
*/

// ===========================================================================
// 1. LIBRERIE
// ===========================================================================
// Le librerie dei COMPONENTI (IMU, meteo, display) non si includono qui:
// le include il file del driver selezionato, che e' l'unico a sapere che
// chip c'e' montato.

#include <Wire.h>              // bus I2C
#include <LittleFS.h>          // filesystem sulla flash interna
#include <WiFi.h>              // wifi
#include <WebServer.h>         // server HTTP
#include <Adafruit_GFX.h>      // primitive grafiche
#include <Adafruit_Sensor.h>   // tipo sensors_event_t, comune ai driver
#include <TinyGPSPlus.h>       // parsing NMEA del modulo GPS
// La libreria del sensore meteo NON si include qui: la include il file
// del driver selezionato (src/driver/Meteo.h), che e'
// l'unico posto del firmware a sapere che sensore c'e' montato.

// Tutto cio' che cambia da un esemplare all'altro. Lo include solo questo
// file: la concatenazione lo rende visibile anche agli altri due .ino.
#include "Config.h"

// I driver sono header e non .ino per un motivo pratico: solo i .ino
// devono stare nella radice dello sketch, gli header possono stare in
// sottocartelle - ed e' cosi' che stanno tutti insieme in src/driver/.
//
// L'ordine non conta: un driver puo' dipendere solo da Config.h,
// altrimenti non e' piu' sostituibile davvero.
#include "src/driver/Display.h"
#include "src/driver/Gps.h"
#include "src/driver/Imu.h"
#include "src/driver/Meteo.h"

// Unica fonte per la versione mostrata a schermo (splash OLED) e sul
// monitor seriale: il commento in cima al file resta la cronologia
// completa, questa costante e' solo "cosa sto guardando adesso".
// Ricordarsi di aggiornarla insieme al numero in cima al file.
const char FIRMWARE_VERSION[] = "v9.16";

// ===========================================================================
// 2. CONFIGURAZIONE DELL'HARDWARE -> Config.h
// ===========================================================================
// Pin, indirizzi I2C, dimensioni del display, assi dell'IMU, parametri
// GPS e credenziali dell'AP stanno tutti in Config.h: sono le uniche cose
// che cambiano montando componenti diversi.
//
// La sezione 3 qui sotto e' invece la taratura del COMPORTAMENTO (soglie,
// filtri, temporizzazioni), che con l'hardware non cambia.

// ===========================================================================
// 3. CONFIGURAZIONE
// ===========================================================================

const float GRAVITA = 9.81f;  // per passare da m/s^2 a G

// --- Filtri -----------------------------------------------------------------
// Filtro anti-buca: un valore conta per record ed eventi solo se il livello
// viene MANTENUTO per tutta la finestra. L'urto di una buca dura 20/80 ms,
const unsigned long FILTRO_ANTIBUCA_MS = 150;

// Sotto questa soglia il pitch conta come zero: senza, il rumore del
// sensore farebbe saltellare la pagina 3 tra IMPENNATA e STOPPIE.
const float ZONA_MORTA_PITCH = 3.0f;  // gradi

// --- Rischio grip (vedi calcolaRischioGrip) ---------------------------------
const float SOGLIA_UMIDITA_RISCHIO = 55.0f;  // sotto questa umidita' nessun rischio
const float PESO_UMIDITA           = 1.3f;   // punti per ogni % oltre soglia
const float SOGLIA_TEMP_RISCHIO    = 20.0f;  // sotto questa temperatura il rischio sale
const float PESO_TEMPERATURA       = 3.5f;   // punti per ogni grado sotto soglia

// Soglia dinamica di piega per l'alert di pagina 0: piu' rischio grip
// c'e', prima scatta il banner di pericolo
const float PIEGA_MAX_TEORICA       = 55.0f;  // piega massima su asfalto perfetto
const float RIDUZIONE_PIEGA_RISCHIO = 0.35f;  // gradi "persi" per ogni punto di rischio

// --- Temporizzazioni (ms) ----------------------------------------------------
const unsigned long INTERVALLO_DISPLAY   = 100;    // 10 FPS
// INTERVALLO_METEO sta in Config.h: non e' una scelta di prodotto ma il
// limite fisico del sensore montato, e cambia col sensore
const unsigned long INTERVALLO_SERIALE   = 5000;   // telemetria di debug
const unsigned long INTERVALLO_LAMPEGGIO = 500;    // asterisco "* REC"
const unsigned long INTERVALLO_LOG       = 60000;  // un record CSV al minuto
const unsigned long TEMPO_DEBOUNCE       = 250;    // tempo morto dopo ogni tasto
const unsigned long DURATA_POPUP         = 800;    // popup "CALIBRAZIONE OK"
const unsigned long DURATA_SPLASH        = 1500;   // logo all'accensione
const unsigned long PAUSA_LOOP           = 10;     // respiro per il watchdog
const unsigned long RIEPILOGO_TIMEOUT    = 30000;  // uscita automatica dal riepilogo

// Quanto si ascolta la UART del GPS all'avvio prima di dichiarare il
// modulo assente (vedi gpsAscoltaNMEA). Tarato sul caso piu' lento: un
// modulo non-u-blox resta a 1 Hz di fabbrica, quindi 1500 ms garantiscono
// una frase intera con margine. Sta qui e non in Config.h perche' copre
// tutti e tre i moduli: non cambia montandone uno diverso.
const unsigned long ATTESA_NMEA_BOOT     = 1500;

// --- Memoria ------------------------------------------------------------------
const int MAX_SESSIONI = 9999;  // tetto alla numerazione LOG_n.CSV

// Sotto questa soglia di spazio libero la registrazione non parte
const unsigned long MIN_SPAZIO_LIBERO = 32 * 1024UL;  // byte

// (SSID/password dell'access point e parametri del GPS: Config.h)

// --- Lap timing (traguardo GPS "volante") --------------------------------
const float RAGGIO_TRAGUARDO_M       = 20.0f;    // metri: sotto, il passaggio e' "sulla linea"
const unsigned long MIN_INTERVALLO_GIRO_MS = 15000UL;  // anti-doppio conteggio vicino alla linea
const int   MAX_GIRI                 = 50;       // tetto ai giri per sessione (array in RAM)

// --- Forma del tracciato e settori ----------------------------------------
// La forma e' una sequenza di punti campionati durante un giro (vedi
// rilevaGiro()), salvata nel file del tracciato quando quel giro batte
// il record di sessione. Un punto al secondo (INTERVALLO_PUNTO_FORMA_MS)
// e' piu' che sufficiente per riconoscere la forma di un circuito: non
// serve la stessa risoluzione della traccia live sul sito.
const int MAX_PUNTI_FORMA = 80;
const unsigned long INTERVALLO_PUNTO_FORMA_MS = 1000UL;

// I settori sono checkpoint GPS intermedi, oltre al traguardo, che
// dividono il giro in altrettanti tratti cronometrati separatamente.
// MAX_SETTORI e' il numero di checkpoint INTERMEDI (non conta il
// traguardo): con 2 checkpoint si hanno fino a 3 settori per giro. Un
// tracciato senza checkpoint (il caso comune, e il default per ogni
// tracciato nuovo) ha semplicemente un giro come unico settore, esatto
// come il lap timing gia' esistente.
const int MAX_SETTORI = 2;

// Tetto all'elenco sessioni di /confronta (elencaSessioni): generoso per
// l'uso tipico, tiene comunque un limite fisso alla RAM usata dall'array.
const int MAX_SESSIONI_ELENCO = 40;

// Va dichiarata QUI, prima di qualunque funzione: l'IDE Arduino genera i
// prototipi automatici in cima al file e non vedrebbe una struct definita
// piu' in basso - la compilazione fallirebbe con "'Tracciato' has not
// been declared". Solo la struct: il resto dello stato tracciati sta
// nella sezione 5 con le altre globali.
//
// Dimensione approssimativa: ~720 byte per tracciato (soprattutto la
// forma, 80 punti x 2 float x 4 byte). Con MAX_TRACCIATI=30 il tetto
// massimo e' ~22 KB: trascurabile sul ~1.5 MB di LittleFS disponibile.
struct Tracciato {
  uint32_t magic;
  char     nome[24];
  double   traguardoLat;
  double   traguardoLon;
  uint32_t migliorGiroMs;  // record di sempre SU QUESTO TRACCIATO, 0 = nessuno

  uint16_t numPuntiForma;              // 0 = forma non ancora registrata
  float    formaLat[MAX_PUNTI_FORMA];
  float    formaLon[MAX_PUNTI_FORMA];

  uint8_t  numCheckpoint;              // 0..MAX_SETTORI, 0 = nessun settore
  double   checkpointLat[MAX_SETTORI];
  double   checkpointLon[MAX_SETTORI];
};

// Struct usata da leggiInfoSessione() come tipo di ritorno (sezione 13,
// pagina web /confronta). Stesso motivo di Tracciato qui sopra: deve
// stare prima di qualunque funzione, altrimenti l'IDE Arduino genera un
// prototipo con un tipo ancora sconosciuto e la compilazione fallisce.
struct InfoSessione {
  bool          trovata;               // false se il file non ha un riepilogo valido
  String        tracciato;             // nome del tracciato di quella sessione, o "libera"
  int           numGiri;
  unsigned long tempiGiro[MAX_GIRI];
};

// Riga sintetica di una sessione (nome file + tracciato + giri), usata
// da elencaSessioni() per costruire l'elenco UNA sola volta e riusarlo
// per entrambe le select di /confronta. Stesso motivo delle struct qui
// sopra: dichiarata prima di qualunque funzione.
struct VoceSessione {
  String nome;
  String tracciato;
  int    numGiri;
};

// ===========================================================================
// 4. TABELLE DI CANALI ED EVENTI
// ===========================================================================
// Il cuore dell'organizzazione: ogni grandezza misurata e' un CANALE sempre
// positivo. Tutte le operazioni (minimo di finestra, massimo del minuto e
// della sessione, scrittura CSV) sono cicli sulla tabella: per aggiungere
// una grandezza basta una voce nell'enum, il nome di colonna e una riga
// dove si calcola il valore istantaneo in leggiIMU().

enum IndiceCanale {
  C_PIEGA_DX,   // gradi di piega verso destra
  C_PIEGA_SX,   // gradi di piega verso sinistra
  C_IMPENNATA,  // gradi di muso su
  C_STOPPIE,    // gradi di muso giu'
  C_GLAT_DX,    // G laterali verso destra
  C_GLAT_SX,    // G laterali verso sinistra
  C_G_ACCEL,    // G in accelerazione
  C_G_FRENA,    // G in frenata
  C_VELOCITA,   // km/h dal GPS (0 se il fix non e' valido, vedi leggiGPS)
  N_CANALI
};

// Nomi delle colonne CSV, nello stesso ordine dell'enum
const char *NOME_CANALE[N_CANALI] = {
  "Piega_Dx", "Piega_Sx", "Impennata", "Stoppie",
  "GLat_Dx", "GLat_Sx", "G_Accel", "G_Frena", "Vel_Kmh"
};

// Cifre decimali nel CSV: 1 per gli angoli, 2 per le G, 0 per la velocita'
// (coerente con come e' gia' mostrata sull'OLED, km/h intero)
int decimaliCanale(int c) {
  if (c <= C_STOPPIE)  return 1;  // angoli
  if (c <= C_G_FRENA)  return 2;  // forze G
  return 0;                       // velocita'
}

// Simbolo dell'unita' di misura per canale, usato dalla tabella record
// del sito web. L'OLED non ne ha bisogno: mostra le unita' inline dove
// serve (es. disegnaGPS, disegnaMeteo), non cicla sulla tabella canali.
const __FlashStringHelper *unitaCanale(int c) {
  if (c <= C_STOPPIE)  return F("&deg;");
  if (c <= C_G_FRENA)  return F(" G");
  return F(" km/h");
}

// Eventi: manovre contate quando il livello sostenuto supera la soglia.
// Per riarmarsi deve scendere sotto soglia*ISTERESI_EVENTO, cosi' una
// manovra che balla intorno alla soglia conta una volta sola.
enum IndiceEvento {
  EV_IMPENNATA,  // impennata oltre soglia
  EV_STOPPIE,    // stoppie oltre soglia
  EV_PIEGA,      // piega importante (da qualunque lato)
  EV_FRENATA,    // frenata brusca
  EV_ACCEL,      // accelerata brusca
  N_EVENTI
};

const float SOGLIA_EVENTO[N_EVENTI] = {
  15.0f,   // EV_IMPENNATA, gradi
  10.0f,   // EV_STOPPIE, gradi
  35.0f,   // EV_PIEGA, gradi
  0.70f,   // EV_FRENATA, G
  0.50f    // EV_ACCEL, G
};
const float ISTERESI_EVENTO = 0.7f;  // frazione della soglia per il riarmo

// Nomi per il CSV (senza spazi) e per la pagina OLED (allineati a colonna)
const char *NOME_EVENTO_CSV[N_EVENTI] = {
  "Impennate", "Stoppie", "Pieghe", "Frenate", "Accelerate"
};
const char *NOME_EVENTO_OLED[N_EVENTI] = {
  "Impennate    : ", "Stoppie      : ", "Pieghe       : ",
  "Fren.brusche : ", "Acc.brusche  : "
};

// True per gli eventi misurati in G (per stampare la soglia come "0.7G")
bool eventoInG(int e) {
  return (e == EV_FRENATA || e == EV_ACCEL);
}

// ===========================================================================
// 5. OGGETTI DRIVER E STATO GLOBALE
// ===========================================================================

// L'oggetto "display" e' definito nel file del driver selezionato
// (src/driver/Display.h) e dichiarato extern in
// Config.h, insieme a SCREEN_WIDTH/SCREEN_HEIGHT e ai colori
// COLORE_ON/COLORE_OFF. Da qui in poi si disegna e basta, senza sapere
// che pannello c'e' sotto.

// L'oggetto dell'IMU vive nel file del suo driver (src/driver/Imu.h): da
// qui si passa solo per imuInit(), imuLeggi() e imuCalibrazione(). La
// fusione sensoriale avviene comunque dentro al chip, quindi arrivano
// angoli in gradi gia' pronti e accelerazione senza gravita': il
// firmware non contiene nessun filtro di fusione.

// L'oggetto del sensore meteo vive nel file del suo driver
// (src/driver/Meteo.h), non qui: da questa parte del
// firmware si passa solo per meteoInit() e meteoLeggi().

// L'UART del GPS (gpsSerial) sta nel suo driver, dichiarata extern in
// Config.h. Qui resta solo il parser: TinyGPS++ e' NMEA standard, non
// dipende da quale modulo c'e' collegato.
TinyGPSPlus gps;

// Server web su porta 80
WebServer server(80);

// --- Esito del boot, per non interrogare periferiche assenti ---
bool oledOk    = false;
bool bnoOk     = false;
bool memoriaOk = false;
// Vero per tutti e otto i sensori meteo: quelli I2C rispondono o no sul
// bus, il DHT22 - che un indirizzo non ce l'ha - viene interrogato
// davvero e deve consegnare almeno una lettura non-NaN. Vedi meteoInit()
// in src/driver/Meteo.h.
bool meteoOk   = false;

// --- Interfaccia ---
// Ordine delle pagine per temi (v9.13): prima tutto cio' che interessa
// mentre si guida (sensori + GPS + lap timing), poi lo storico (record),
// poi la diagnostica di sistema (memoria, WiFi) - le pagine piu' rare
// da consultare in sella restano in fondo.
int  schermataCorrente = 0;   // 0 Piega, 1 Meteo, 2 G, 3 Impennata,
                              // 4 GPS, 5 Giri, 6 Record 1/2, 7 Record 2/2,
                              // 8 Memoria, 9 WiFi
const int totaleSchermate = 10;
bool wifiAttivo   = false;    // access point acceso e server in ascolto
bool lampeggioRec = false;    // alterna l'asterisco della scritta "* REC"

// --- Telemetria live (ultimo valore valido) ---
// Partenza meteo su valori NEUTRI (20 gradi / 50%, rischio = 0):
float temperatura         = 20.0f;
float umidita             = 50.0f;
float indiceRischio       = 0.0f;  // 0-100 %
float angoloPiega         = 0.0f;  // inclinazione laterale assoluta, gradi
float angoloImpennata     = 0.0f;  // muso su, gradi (>= 0)
float angoloStoppie       = 0.0f;  // muso giu', gradi (>= 0)
float forzaGLaterale      = 0.0f;  // + destra / - sinistra
float forzaGLongitudinale = 0.0f;  // + accelerata / - frenata
float piegaLive           = 0.0f;  // piega CON segno, per i grafici web
float pitchLive           = 0.0f;  // pitch CON segno (+imp/-stop), per il web

// --- GPS (modulo NEO-6M/8M, UART dedicata: vedi GPS_UART_NUM) --------------
// Due stati che vanno tenuti separati:
//
//   gpsOk         il MODULO risponde, cioe' manda frasi NMEA. Vedi
//                 gpsAscoltaNMEA().
//   gpsFixValido  ha AGGANCIATO i satelliti. Arriva decine di secondi
//                 dopo l'accensione e va e viene in continuazione
//                 (curve, tettoie, tunnel). Vedi leggiGPS().
//
// Modulo scollegato: gpsOk falso. Modulo collegato ma senza cielo: gpsOk
// vero e gpsFixValido falso. Fino alla v9.14 c'era solo il secondo, e la
// schermata di avvio spuntava il GPS anche col connettore vuoto.
bool    gpsOk          = false;
bool    gpsFixValido   = false;
double  latitudineGPS  = 0.0;
double  longitudineGPS = 0.0;
float   velocitaGPS    = 0.0f;  // km/h, forzata a 0 se il fix non e' valido
uint8_t satellitiGPS   = 0;     // satelliti agganciati, utile anche senza fix

// --- Lap timing: traguardo "volante" impostato dalla pagina GIRI -----------
// Per ora vive solo in RAM (nessun salvataggio su flash): si reimposta a
// ogni accensione. Il salvataggio di piu' tracciati con relativi record
// arriva in una versione futura; questo e' il primo passo, un solo
// traguardo alla volta, per validare la logica di rilevamento passaggio.
bool   traguardoImpostato = false;
double traguardoLat       = 0.0;
double traguardoLon       = 0.0;

bool eraFuoriRaggio = true;  // isteresi: serve uscire dal raggio prima di poter ricontare

unsigned long tempiGiro[MAX_GIRI] = {};  // durata di ogni giro completato, ms
int numGiri       = 0;   // giri completati nella sessione/uscita corrente
int giroMigliore  = -1;  // indice in tempiGiro del giro piu' veloce, -1 se nessuno

unsigned long inizioGiroCorrente = 0;  // millis() dell'ultimo passaggio sul traguardo
bool giroInCorso  = false;             // true dal primo passaggio in poi

// --- Forma del tracciato: punti raccolti mentre un giro e' in corso -------
// Si accumula durante UN giro (si azzera a ogni nuovo giro, vedi
// rilevaGiro()): diventa la forma salvata del tracciato quando quel
// giro batte il record di sessione, cosi' la forma memorizzata tende a
// convergere verso la linea "pulita", non un giro qualunque di prova.
float formaLat[MAX_PUNTI_FORMA];
float formaLon[MAX_PUNTI_FORMA];
int   numPuntiForma   = 0;
unsigned long ultimoPuntoForma = 0;

// --- Settori: stato di avanzamento nel giro corrente -----------------------
// prossimoCheckpoint e' l'indice del prossimo checkpoint atteso
// (0..numCheckpoint-1); quando arriva a numCheckpoint il prossimo punto
// da raggiungere torna a essere il traguardo (vedi prossimoObiettivo()).
// Su un tracciato senza checkpoint (numCheckpoint=0) questo indice resta
// sempre gia' "scaduto", quindi il comportamento e' identico al lap
// timing a settore singolo di prima: niente casi speciali da gestire.
int           prossimoCheckpoint     = 0;
unsigned long inizioSettoreCorrente  = 0;   // millis() dell'ultimo checkpoint/traguardo
// [giro][settore]: tempo di quel settore in quel giro, ms. Indicizzato
// come tempiGiro[]: stesso giro, stesso indice, cosi' CSV e web possono
// mettere in colonna il tempo totale e i settori senza doppio conteggio.
unsigned long tempiSettorePerGiro[MAX_GIRI][MAX_SETTORI + 1] = {};
unsigned long tempiSettoreMigliori[MAX_SETTORI + 1] = {};  // migliore di sessione, per settore

// --- Canali: finestra del filtro, massimi del minuto e della sessione ---
float minimiFinestra[N_CANALI]  = {};  // livello sostenuto (minimo di finestra)
float massimiMinuto[N_CANALI]   = {};  // massimi del minuto -> riga CSV
float massimiSessione[N_CANALI] = {};  // massimi della sessione -> riepilogo
bool  finestraAvviata = false;
unsigned long ultimaFinestraFiltro = 0;

// --- Eventi: conteggi e flag "manovra in corso" per l'isteresi ---
uint16_t conteggioEventi[N_EVENTI] = {};
bool     eventoInCorso[N_EVENTI]   = {};

// --- Taratura: compensa il montaggio non in bolla (tasto OK, in piano).
//     Vive in RAM: a ogni accensione si riparte da zero. ---
float offsetPiega     = 0.0f;
float offsetImpennata = 0.0f;

// --- Registrazione su memoria flash ---
bool inRegistrazione = false;
char nomeFileLog[20] = "";  // es. "/LOG_12.CSV"
unsigned long minutiRegistrati    = 0;
unsigned long inizioRegistrazione = 0;

// --- Record storici ("di sempre") ---
// I massimi mai registrati, in un file binario che sopravvive allo
// spegnimento. Si aggiornano solo alla FINE di una registrazione: se non
// e' registrato non e' un record.
//
// FILE_RECORD e' lo storico della modalita' libera; con un tracciato
// attivo si usa il suo file dedicato (vedi fileRecordAttivo). Il nome non
// inizia per "LOG_", cosi' /scarica e /elimina non possono toccarlo.
const char     FILE_RECORD[] = "/RECORD.BIN";
// "WSR2": v9.8 ha aggiunto il canale velocita', la struct e' piu' grande
const uint32_t MAGIC_RECORD  = 0x57535232;

struct RecordStorici {
  uint32_t magic;             // firma: se non torna, il file non e' mio
  float    canali[N_CANALI];  // massimo storico di ogni canale
  uint16_t sessioni;          // registrazioni completate in totale
  uint32_t minutiTotali;      // minuti loggati in tutta la vita del dispositivo
};
RecordStorici recordStorici = {};  // tutto a zero finche' non carico il file

// --- Tracciati (circuiti definiti dall'utente, per lap timing e record
//     separati per pista) ---
// Ogni tracciato e' una coppia di file: /TRACK_n.BIN (nome + traguardo) e
// /TRACK_n_REC.BIN (record storici SU quel tracciato, stessa struct
// RecordStorici della modalita' libera). "n" e' un id progressivo
// assegnato alla creazione e mai riassegnato quando un tracciato si
// elimina, cosi' un id vecchio non si confonde con uno nuovo. La struct
// Tracciato e' dichiarata prima della sezione 4 (vedi il commento li'),
// qui restano solo le costanti e lo stato del tracciato attivo.
const char     PREFISSO_TRACK[]     = "/TRACK_";
const char     SUFFISSO_TRACK_DEF[] = ".BIN";
const char     SUFFISSO_TRACK_REC[] = "_REC.BIN";
const uint32_t MAGIC_TRACCIATO      = 0x57535433;  // "WST3": struct piu' grande da v9.12 (forma+settori)
const int      MAX_TRACCIATI        = 30;          // ~720 byte/tracciato (vedi il commento sulla struct): 30 restano trascurabili

int       tracciatoAttivo   = -1;  // -1 = modalita' libera, altrimenti l'id del tracciato
Tracciato tracciatoCorrente = {};  // dati del tracciato attivo, vuoto in modalita' libera

// Flag "record appena battuto", canale per canale: accendono l'asterisco
// nel riepilogo di fine sessione. Si azzerano a ogni nuova registrazione.
bool nuovoRecord[N_CANALI] = {};

// --- Timer non bloccanti (millis dell'ultimo scatto) ---
unsigned long ultimaLetturaMeteo         = 0;
unsigned long ultimoAggiornamentoDisplay = 0;
unsigned long ultimoPrintSeriale         = 0;
unsigned long ultimoSalvataggio          = 0;
unsigned long ultimoLampeggio            = 0;
unsigned long ultimoTempoPulsante        = 0;

// --- Stato precedente dei pulsanti, per il rilevamento del fronte ---
int statoPrecSu  = HIGH;
int statoPrecGiu = HIGH;
int statoPrecOk  = HIGH;
int statoPrecLog = HIGH;

// ===========================================================================
// 6. UTILITY
// ===========================================================================

// Timer non bloccante: true (e riarma) se sono passati almeno 'intervallo' ms.
// La sottrazione tra unsigned regge l'overflow di millis() (~49 giorni).
bool trascorsi(unsigned long &ultimoScatto, unsigned long intervallo) {
  unsigned long adesso = millis();
  if (adesso - ultimoScatto >= intervallo) {
    ultimoScatto = adesso;
    return true;
  }
  return false;
}

// True solo nel ciclo esatto in cui il pulsante passa da HIGH a LOW:
// tenerlo premuto non genera altri eventi.
bool frontePressione(uint8_t pin, int &statoPrecedente) {
  int statoAttuale = digitalRead(pin);
  bool premuto = (statoAttuale == LOW && statoPrecedente == HIGH);
  statoPrecedente = statoAttuale;
  return premuto;
}

// Riporta un angolo in -180..+180. Serve dopo la sottrazione dell'offset:
// vicino al punto di wrap del sensore salterebbe di 360 gradi.
float normalizzaAngolo(float gradi) {
  while (gradi >  180.0f) gradi -= 360.0f;
  while (gradi < -180.0f) gradi += 360.0f;
  return gradi;
}

// Azzera un array di canali (minuto o sessione)
void azzeraCanali(float *canali) {
  for (int c = 0; c < N_CANALI; c++) canali[c] = 0.0f;
}

// ===========================================================================
// 7. SETUP E LOOP
// ===========================================================================

void setup() {
  Serial.begin(115200);
  delay(500);  // lascio stabilizzare l'alimentazione prima di parlare coi sensori

  Serial.println(F("=== AVVIO SISTEMA WHEELSTAT ==="));
  Serial.print(F("Firmware: "));
  Serial.println(FIRMWARE_VERSION);

  // Pulsanti verso GND con pull-up interna: a riposo leggono HIGH
  pinMode(PIN_BTN_SU,  INPUT_PULLUP);
  pinMode(PIN_BTN_GIU, INPUT_PULLUP);
  pinMode(PIN_BTN_OK,  INPUT_PULLUP);
  pinMode(PIN_BTN_LOG, INPUT_PULLUP);

  // Bus I2C condiviso da OLED e BNO055. A 400 kHz il frame OLED passa da
  // ~90 a ~23 ms: senza, i 10 FPS non ci stanno e lagga.
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(I2C_CLOCK_HZ);

  // --- OLED. Deve venire per primo, ed e' l'unico che non puo'
  //     raccontare la propria accensione: fino a qui non c'e' niente su
  //     cui scrivere. La sequenza di accensione la conosce solo il file
  //     del driver, qui interessa solo se il pannello ha risposto. ---
  Serial.print(F("Boot OLED...... "));
  oledOk = displayInit();
  Serial.println(oledOk ? F("OK.") : F("ERRORE! Controlla SDA/SCL."));

  // Logo animato, poi la lista dei componenti che si riempie mentre
  // vengono davvero inizializzati, uno per uno. I nomi li dichiarano i
  // driver stessi (displayNome(), meteoNome(), ...): a schermo compare
  // sempre la configurazione reale, anche dopo aver cambiato una macro
  // in Config.h.
  splashIntro();
  splashChecklist();

  splashRigaAttesa(0, F("OLED"), displayNome());
  splashRigaEsito(0, oledOk);

  // --- Sensore meteo: quale sia lo decide Config.h, qui si sa solo che
  //     esiste e che potrebbe non rispondere. Va DOPO Wire.begin(): se il
  //     driver selezionato e' I2C, meteoInit() parla gia' sul bus. ---
  Serial.print(F("Boot meteo..... "));
  splashRigaAttesa(1, F("METEO"), meteoNome());
  meteoOk = meteoInit();
  splashRigaEsito(1, meteoOk);
  Serial.println(meteoOk ? F("OK.") : F("ERRORE! Sensore meteo non trovato."));

  // --- IMU: modalita' operativa e rimappatura degli assi per il
  //     montaggio reale le sa il suo driver; qui interessa solo se il
  //     chip ha risposto. ---
  Serial.print(F("Boot IMU....... "));
  splashRigaAttesa(2, F("IMU"), imuNome());
  bnoOk = imuInit();
  splashRigaEsito(2, bnoOk);
  Serial.println(bnoOk ? F("OK.")
                       : F("ERRORE! IMU non trovata (prova indirizzo 0x29)."));

  // --- GPS: apro la UART, passo la parola al driver (comandi UBX per il
  //     5 Hz col NEO-6M/8M, niente con un modulo generico), poi ascolto
  //     per capire se dall'altra parte c'e' qualcuno.
  //
  //     La spunta vuol dire "il modulo manda frasi NMEA", NON "fix
  //     agganciato": il fix arriva decine di secondi dopo e ha la sua
  //     pagina. Segnare una croce perche' il satellite non c'e' ancora
  //     sarebbe fuorviante, e non serve - le frasi escono dal primo
  //     secondo, anche senza vedere un satellite. ---
  Serial.print(F("Boot GPS....... "));
  splashRigaAttesa(3, F("GPS"), gpsNome());
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
  delay(200);  // il modulo deve finire il proprio boot prima di accettare comandi
  gpsConfigura();
  gpsOk = gpsAscoltaNMEA(ATTESA_NMEA_BOOT);
  splashRigaEsito(3, gpsOk);
  Serial.println(gpsOk
      ? F("OK (in attesa del fix).")
      : F("ASSENTE! Nessuna frase NMEA: modulo scollegato, non alimentato,\n"
          "               oppure TX/RX invertiti (il TX del modulo va su GPIO 16)."));

  // --- Memoria flash (LittleFS). Il "true" formatta la partizione se al
  //     primissimo avvio non e' ancora inizializzata. ---
  Serial.print(F("Boot memoria... "));
  splashRigaAttesa(4, F("MEM"), F("LittleFS"));
  memoriaOk = LittleFS.begin(true);
  splashRigaEsito(4, memoriaOk);
  if (memoriaOk) {
    Serial.print(F("OK ("));
    Serial.print((LittleFS.totalBytes() - LittleFS.usedBytes()) / 1024);
    Serial.println(F(" KB liberi)."));
  } else {
    Serial.println(F("ERRORE! Partizione flash non montabile."));
  }

  // Record storici dal file sulla flash (se esiste): da qui in poi la
  // pagina RECORD e il sito web mostrano i migliori di tutte le sessioni
  caricaRecordStorici();


  server.on("/", webElencoFile);
  server.on("/live", webLive);
  server.on("/style.css", webStyleCss);
  server.on("/dati", webDati);
  server.on("/scarica", webScarica);
  server.on("/gpx", webGpx);
  server.on("/elimina", webElimina);
  server.on("/azzera_record", webAzzeraRecord);
  server.on("/tracciati", webTracciati);
  server.on("/tracciato_crea", webTracciatoCrea);
  server.on("/tracciato_seleziona", webTracciatoSeleziona);
  server.on("/tracciato_libera", webTracciatoLibera);
  server.on("/tracciato_elimina", webTracciatoElimina);
  server.on("/tracciato_aggiungi_settore", webTracciatoAggiungiSettore);
  server.on("/tracciato_rimuovi_settori", webTracciatoRimuoviSettori);
  server.on("/confronta", webConfronta);
  server.onNotFound([]() { server.send(404, "text/plain", "Pagina inesistente"); });

  Serial.println(F("=== SISTEMA PRONTO ==="));

  delay(DURATA_SPLASH);      // lascia leggere la lista dei componenti
  attesaCalibrazioneIMU();   // blocca tutto finche' il magnetometro non e' pronto
}

// Schermata di calibrazione imu all'avvio, saltabile premendo un tasto
void attesaCalibrazioneIMU() {
  if (!bnoOk || !oledOk) return;  // senza IMU o display non ha senso

  bool calibrata = false;
  unsigned long ultimoDisegno = 0;

  while (true) {
    // Livello di calibrazione secondo il driver (0 = niente, 3 = pronto).
    // Quale grandezza sia a decidere lo sa lui: sul BNO055 e' il
    // magnetometro, l'unico lento a salire. Un chip che si calibra da
    // solo ritorna 3 subito e questa schermata si chiude da sola.
    uint8_t calMag = imuCalibrazione();

    if (calMag >= 3) {
      calibrata = true;
      break;
    }
    // OK salta la calibrazione
    if (frontePressione(PIN_BTN_OK, statoPrecOk)) break;

    // Ridisegno a 10 FPS come il resto dell'interfaccia. Il disegno sta
    // in PagineOled.ino insieme a tutte le altre schermate.
    if (trascorsi(ultimoDisegno, INTERVALLO_DISPLAY)) disegnaCalibrazione(calMag);

    delay(PAUSA_LOOP);
  }

  // Pulizia: niente eventi o massimi ereditati dai movimenti di calibrazione
  azzeraStatistiche();

  // A calibrazione riuscita, conferma a schermo e attesa di un tasto:
  if (calibrata) {
    Serial.println(F("Magnetometro calibrato (MAG=3)."));

    disegnaCalibrazioneOk();
    attesaTastoRiepilogo();  // tasto per proseguire (timeout di sicurezza 30 s)
  } else {
    Serial.println(F("Calibrazione saltata dall'utente."));
  }

  // Il tasto premuto qui non deve anche azionare il menu subito dopo
  ultimoTempoPulsante = millis();
}

// Schedulazione cooperativa: ogni compito ha il suo timer e scatta solo
// quando e' ora. Cosi' l'IMU viene letta sempre alla massima frequenza e
// nessun compito blocca gli altri.
void loop() {
  if (trascorsi(ultimaLetturaMeteo, INTERVALLO_METEO)) leggiMeteo();
  leggiIMU();  // ad ogni giro, per non perdere i picchi
  leggiGPS();  // idem: i byte NMEA arrivano in streaming, vanno svuotati subito
  rilevaGiro();  // usa la posizione appena letta da leggiGPS()

  gestisciPulsanti();

  // Richieste HTTP dal telefono (no-op quando non ci sono client)
  if (wifiAttivo) server.handleClient();

  if (trascorsi(ultimoAggiornamentoDisplay, INTERVALLO_DISPLAY)) aggiornaDisplay();
  if (trascorsi(ultimoPrintSeriale, INTERVALLO_SERIALE)) stampaSeriale();

  if (inRegistrazione) {
    // Un record al minuto in memoria + lampeggio della scritta REC
    if (trascorsi(ultimoSalvataggio, INTERVALLO_LOG)) scriviDatiSuFlash();
    if (trascorsi(ultimoLampeggio, INTERVALLO_LAMPEGGIO)) lampeggioRec = !lampeggioRec;
  }

  delay(PAUSA_LOOP);
}

// ===========================================================================
// 8. SENSORI E FILTRO ANTI-BUCA
// ===========================================================================

// Orchestrazione, non lettura: quale sensore c'e' e come si interroga lo
// sa solo il suo driver (vedi il contratto in Config.h). Qui restano le
// due sole cose che non dipendono dal componente montato: tenere gli
// ultimi valori buoni e ricalcolare il rischio grip.
void leggiMeteo() {
  // Si parte dagli ultimi valori buoni: il driver sovrascrive solo cio'
  // che riesce a leggere davvero, quindi una lettura sporca di umidita'
  // non si porta via anche una temperatura valida.
  float t = temperatura;
  float u = umidita;

  if (meteoOk && meteoLeggi(t, u)) {
    temperatura = t;
    umidita     = u;
  }

  // Fuori dall'if di proposito: anche con una lettura fallita o il
  // sensore assente il rischio va ricalcolato, cosi' resta coerente coi
  // valori di partenza neutri introdotti in v9.2 invece di restare
  // fermo a un valore mai inizializzato.
  calcolaRischioGrip();
}

// --- Configurazione del modulo GPS -----------------------------------------
// Sta nel file del driver selezionato (src/driver/Gps.h):
// e' l'unica parte del GPS che dipende da quale modulo e' collegato.
// Quella che segue, la lettura, e' NMEA standard e vale per tutti.

// Controlla se c'e' davvero un modulo attaccato. Aprire la UART non basta
// a dirlo, si apre senza errori anche verso il vuoto; l'unica prova e' che
// ne escano dei byte. Un ricevitore alimentato manda frasi NMEA anche
// senza fix (GGA e RMC escono coi campi vuoti finche' i satelliti non sono
// agganciati), quindi ascoltare qui distingue un modulo assente da uno che
// sta ancora cercando il cielo, senza aspettare l'aggancio.
//
// Si cerca la coppia "$G" invece di un byte qualunque perche' un pin RX
// scollegato galleggia e ogni tanto produce rumore, ma non l'inizio di una
// frase NMEA. I talker GNSS cominciano tutti per G: GP, GN, GL, GA, GB.
//
// Il timeout intero si paga solo quando il modulo non c'e'.
bool gpsAscoltaNMEA(unsigned long timeoutMs) {
  unsigned long inizio = millis();
  bool dollaro = false;

  while (millis() - inizio < timeoutMs) {
    while (gpsSerial.available() > 0) {
      char c = gpsSerial.read();
      if (dollaro && c == 'G') return true;
      dollaro = (c == '$');
    }
    delay(1);  // niente attesa attiva a vuoto: lascia respirare il watchdog
  }
  return false;
}

// Svuota il buffer seriale del GPS a ogni giro di loop: i dati NMEA
// arrivano in streaming continuo, se non li consumo subito si accumulano
// e il fix che leggo non e' piu' quello attuale (stesso motivo per cui
// leggiIMU() gira a ogni ciclo invece che a intervalli fissi).
void leggiGPS() {
  while (gpsSerial.available() > 0) {
    // Un modulo che parla adesso e' presente anche se al boot non aveva
    // ancora aperto bocca (alimentazione lenta, o connettore inserito a
    // firmware gia' partito). Corregge solo falsi negativi.
    gpsOk = true;
    gps.encode(gpsSerial.read());
  }

  // Un fix e' valido solo se recente: age() cresce finche' non arriva una
  // nuova frase valida, quindi un fix "vecchio" (satelliti persi) scade
  // da solo anche se l'ultimo valore restava tecnicamente "valid".
  gpsFixValido = gps.location.isValid() &&
                 gps.location.age() < GPS_FIX_TIMEOUT_MS;

  if (gpsFixValido) {
    latitudineGPS  = gps.location.lat();
    longitudineGPS = gps.location.lng();
    velocitaGPS    = gps.speed.isValid() ? gps.speed.kmph() : 0.0f;
  } else {
    velocitaGPS = 0.0f;  // niente fix, niente velocita': evita un numero congelato
  }

  // I satelliti agganciati si aggiornano anche senza fix 3D completo:
  // utile per capire "sto per agganciare" invece di un secco si/no
  satellitiGPS = gps.satellites.isValid() ? gps.satellites.value() : 0;
}

// Lettura IMU: aggiorna i valori live per display e web, poi passa la mano
// al filtro anti-buca che alimenta record e contatori.
void leggiIMU() {
  if (!bnoOk) return;

  // Le quattro grandezze arrivano gia' riferite agli assi della moto:
  // quale asse del chip corrisponde a cosa lo sa solo il suo driver
  // (vedi il contratto in Config.h e la nota in src/driver/Imu.h).
  float piegaGrezza, impennataGrezza, accelLong, accelLat;
  if (!imuLeggi(piegaGrezza, impennataGrezza, accelLong, accelLat)) return;

  // Gravita' gia' sottratta dal driver, quindi basta dividere per 9.81:
  // i valori restano corretti anche con la moto in piega.
  forzaGLongitudinale = SEGNO_G_LONG * accelLong / GRAVITA;
  forzaGLaterale      = SEGNO_G_LAT  * accelLat  / GRAVITA;

  // Qui restano le due cose che non dipendono dal chip: la taratura di
  // zero (compensa il montaggio non in bolla) e i versi.
  piegaLive = SEGNO_PIEGA     * normalizzaAngolo(piegaGrezza - offsetPiega);
  pitchLive = SEGNO_IMPENNATA * normalizzaAngolo(impennataGrezza - offsetImpennata);

  angoloPiega = fabsf(piegaLive);

  // Stesso pitch, due manovre: muso su = impennata, muso giu' = stoppie.
  // La zona morta scarta i gradi di rumore intorno allo zero.
  if (fabsf(pitchLive) < ZONA_MORTA_PITCH) {
    angoloImpennata = 0.0f;
    angoloStoppie   = 0.0f;
  } else {
    angoloImpennata = (pitchLive >= 0) ?  pitchLive : 0.0f;
    angoloStoppie   = (pitchLive <  0) ? -pitchLive : 0.0f;
  }

  aggiornaFiltroAntibuca();
}

// Filtro anti-buca. I valori live restano non filtrati apposta (il display
// deve essere reattivo) il filtro protegge solo record e contatori.

// Idea: di ogni canale si tiene il MINIMO dentro una finestra da 150 ms,
// cioe' il livello mantenuto per tutta la finestra. Una buca alza il
// segnale per troppo poco tempo per sollevare quel minimo, una piega vera
// lo attraversa indenne. A fine finestra il livello sostenuto aggiorna i
// massimi del minuto, quelli della sessione e i contatori eventi.
void aggiornaFiltroAntibuca() {
  // Valori istantanei dei canali, tutti positivi (il segno decide il canale)
  float attuale[N_CANALI];
  attuale[C_PIEGA_DX]  = (piegaLive >= 0) ?  piegaLive : 0.0f;
  attuale[C_PIEGA_SX]  = (piegaLive <  0) ? -piegaLive : 0.0f;
  attuale[C_IMPENNATA] = angoloImpennata;
  attuale[C_STOPPIE]   = angoloStoppie;
  attuale[C_GLAT_DX]   = (forzaGLaterale >= 0) ?  forzaGLaterale : 0.0f;
  attuale[C_GLAT_SX]   = (forzaGLaterale <  0) ? -forzaGLaterale : 0.0f;
  attuale[C_G_ACCEL]   = (forzaGLongitudinale >= 0) ?  forzaGLongitudinale : 0.0f;
  attuale[C_G_FRENA]   = (forzaGLongitudinale <  0) ? -forzaGLongitudinale : 0.0f;
  // GPS aggiorna a ~1 Hz, molto piu' lento della finestra da 150 ms: la
  // maggior parte delle finestre vede semplicemente lo stesso valore
  // ripetuto (il filtro anti-buca diventa un no-op per questo canale),
  // ma restare sulla stessa tabella significa niente codice speciale e
  // niente eccezioni da spiegare altrove (record, CSV, pagine).
  attuale[C_VELOCITA]  = velocitaGPS;

  if (!finestraAvviata) {
    // Primo campione: la finestra parte da qui
    for (int c = 0; c < N_CANALI; c++) minimiFinestra[c] = attuale[c];
    finestraAvviata = true;
  } else {
    // Campioni successivi: tengo il minimo canale per canale
    for (int c = 0; c < N_CANALI; c++)
      minimiFinestra[c] = fminf(minimiFinestra[c], attuale[c]);
  }

  // Finestra chiusa: il livello sostenuto aggiorna statistiche e contatori
  if (trascorsi(ultimaFinestraFiltro, FILTRO_ANTIBUCA_MS)) {
    aggiornaMassimi(massimiMinuto);
    aggiornaMassimi(massimiSessione);
    rilevaEventi();
    finestraAvviata = false;  // il prossimo campione apre una nuova finestra
  }
}

// ===========================================================================
// 9. STATISTICHE: RECORD, EVENTI, RISCHIO GRIP
// ===========================================================================

// Azzera tutto quello che si accumula nel tempo: massimi del minuto e
// della sessione, finestra del filtro e contatori eventi. Usata all'avvio
// di ogni registrazione e alla fine della calibrazione iniziale.
void azzeraStatistiche() {
  azzeraCanali(massimiMinuto);
  azzeraCanali(massimiSessione);
  finestraAvviata = false;
  ultimaFinestraFiltro = millis();  // la prossima finestra del filtro riparte intera
  for (int e = 0; e < N_EVENTI; e++) {
    conteggioEventi[e] = 0;
    eventoInCorso[e] = false;
  }

  // Giri: si riparte da zero a ogni nuova registrazione, ma il traguardo
  // impostato resta valido finche' non lo cambi tu (vive finche' vive
  // l'accensione, vedi il commento sulla dichiarazione piu' sopra)
  numGiri = 0;
  giroMigliore = -1;
  giroInCorso = false;
  eraFuoriRaggio = true;
  inizioGiroCorrente = 0;

  // Settori: si riparte anche qui, i checkpoint del tracciato (se c'e'
  // un tracciato attivo) restano quelli di prima
  prossimoCheckpoint = 0;
  inizioSettoreCorrente = 0;
  numPuntiForma = 0;
  for (int s = 0; s <= MAX_SETTORI; s++) tempiSettoreMigliori[s] = 0;
}

// Confronta i livelli sostenuti della finestra appena chiusa coi record di
// destinazione (minuto o sessione) e tiene i massimi.
void aggiornaMassimi(float *record) {
  for (int c = 0; c < N_CANALI; c++)
    record[c] = fmaxf(record[c], minimiFinestra[c]);
}

// Un evento parte al superamento della soglia e si riarma solo quando il
// livello scende sotto soglia*ISTERESI_EVENTO: la stessa manovra che
// balla intorno alla soglia viene contata una volta sola.
void contaEvento(int e, float valore) {
  if (!eventoInCorso[e] && valore >= SOGLIA_EVENTO[e]) {
    eventoInCorso[e] = true;
    conteggioEventi[e]++;
  } else if (eventoInCorso[e] && valore < SOGLIA_EVENTO[e] * ISTERESI_EVENTO) {
    eventoInCorso[e] = false;
  }
}

// Collega i canali agli eventi. Lavorando sull'uscita del filtro anti-buca,
// un colpo secco su una buca non conta come frenata o impennata.
void rilevaEventi() {
  contaEvento(EV_IMPENNATA, minimiFinestra[C_IMPENNATA]);
  contaEvento(EV_STOPPIE,   minimiFinestra[C_STOPPIE]);
  // Per la piega conta il lato piu' inclinato: dx e sx sono lo stesso evento
  contaEvento(EV_PIEGA,     fmaxf(minimiFinestra[C_PIEGA_DX], minimiFinestra[C_PIEGA_SX]));
  contaEvento(EV_FRENATA,   minimiFinestra[C_G_FRENA]);
  contaEvento(EV_ACCEL,     minimiFinestra[C_G_ACCEL]);
}

// Distanza in metri tra la posizione GPS attuale e un punto arbitrario.
// Il chiamante deve gia' aver verificato gpsFixValido.
float distanzaDa(double lat, double lon) {
  return (float)TinyGPSPlus::distanceBetween(latitudineGPS, longitudineGPS, lat, lon);
}

// Distanza al traguardo: caso specifico di distanzaDa(), usato dalla
// pagina OLED GIRI quando non ci sono settori da inseguire.
float distanzaTraguardo() {
  return distanzaDa(traguardoLat, traguardoLon);
}

// Il prossimo punto da raggiungere per il lap timing: un checkpoint
// intermedio se il tracciato attivo ne ha e non sono ancora stati tutti
// passati in QUESTO giro, altrimenti il traguardo (sia per chiudere il
// giro, sia per armare il primissimo cronometro). "ultimo" e' true
// quando il punto restituito e' il traguardo: sia rilevaGiro() sia la
// pagina OLED GIRI usano questa stessa funzione, cosi' "cosa sto
// inseguendo adesso" e' definito in un solo posto.
void prossimoObiettivo(double &lat, double &lon, bool &ultimo) {
  if (!giroInCorso || prossimoCheckpoint >= tracciatoCorrente.numCheckpoint) {
    lat = traguardoLat;
    lon = traguardoLon;
    ultimo = true;
  } else {
    lat = tracciatoCorrente.checkpointLat[prossimoCheckpoint];
    lon = tracciatoCorrente.checkpointLon[prossimoCheckpoint];
    ultimo = false;
  }
}

// Passaggio sul prossimo obiettivo (checkpoint o traguardo). Stessa
// isteresi degli eventi ma sulla distanza GPS: bisogna uscire dal raggio
// prima di poter ricontare, cosi' il rumore vicino al punto non conta
// doppio.
//
// ATTENZIONE: MIN_INTERVALLO_GIRO_MS (15 s) si applica a OGNI settore,
// non solo al giro intero. Due checkpoint percorribili in meno di 15 s
// fanno scartare quel passaggio.
//
// Un checkpoint chiude solo il SETTORE, il traguardo chiude anche il
// giro. Senza checkpoint prossimoObiettivo() restituisce sempre il
// traguardo: nessun caso speciale da gestire qui.
void rilevaGiro() {
  if (!traguardoImpostato || !gpsFixValido) return;

  double obLat, obLon;
  bool obETraguardo;
  prossimoObiettivo(obLat, obLon, obETraguardo);

  // Campiono un punto della forma ogni tanto, mentre un giro e' in
  // corso: diventa la forma salvata del tracciato se questo giro batte
  // il record di sessione (vedi piu' sotto, salvaFormaTracciato()).
  if (giroInCorso && numPuntiForma < MAX_PUNTI_FORMA &&
      trascorsi(ultimoPuntoForma, INTERVALLO_PUNTO_FORMA_MS)) {
    formaLat[numPuntiForma] = (float)latitudineGPS;
    formaLon[numPuntiForma] = (float)longitudineGPS;
    numPuntiForma++;
  }

  float distanza = distanzaDa(obLat, obLon);

  if (distanza > RAGGIO_TRAGUARDO_M) {
    eraFuoriRaggio = true;  // pronto a contare il prossimo passaggio
    return;
  }

  // Dentro il raggio: conta solo se ero uscito e se e' passato abbastanza
  // tempo dall'ultimo checkpoint/traguardo (il tempo minimo vale anche
  // per il primissimo passaggio, inizioSettoreCorrente=0 all'avvio rende
  // il controllo innocuo)
  if (!eraFuoriRaggio) return;
  if (giroInCorso && millis() - inizioSettoreCorrente < MIN_INTERVALLO_GIRO_MS) return;

  unsigned long adesso = millis();
  eraFuoriRaggio = false;

  if (!obETraguardo) {
    // Checkpoint intermedio: chiudo solo il settore, il giro resta aperto
    unsigned long tempoSettore = adesso - inizioSettoreCorrente;
    if (numGiri < MAX_GIRI) tempiSettorePerGiro[numGiri][prossimoCheckpoint] = tempoSettore;
    if (tempiSettoreMigliori[prossimoCheckpoint] == 0 || tempoSettore < tempiSettoreMigliori[prossimoCheckpoint])
      tempiSettoreMigliori[prossimoCheckpoint] = tempoSettore;
    inizioSettoreCorrente = adesso;
    prossimoCheckpoint++;
    return;
  }

  // Traguardo: chiude l'ultimo settore (se un giro era gia' in corso,
  // cioe' questo non e' il primissimo passaggio che arma il cronometro)
  // e il giro intero
  int indiceUltimoSettore = tracciatoCorrente.numCheckpoint;
  if (giroInCorso) {
    unsigned long tempoSettore = adesso - inizioSettoreCorrente;
    if (numGiri < MAX_GIRI) tempiSettorePerGiro[numGiri][indiceUltimoSettore] = tempoSettore;
    if (tempiSettoreMigliori[indiceUltimoSettore] == 0 || tempoSettore < tempiSettoreMigliori[indiceUltimoSettore])
      tempiSettoreMigliori[indiceUltimoSettore] = tempoSettore;

    if (numGiri < MAX_GIRI) {
      unsigned long durata = adesso - inizioGiroCorrente;
      bool recordGiro = (giroMigliore < 0 || durata < tempiGiro[giroMigliore]);

      tempiGiro[numGiri] = durata;
      if (recordGiro) giroMigliore = numGiri;
      numGiri++;

      // Il giro appena chiuso e' il migliore della sessione: se e' anche
      // un tracciato salvato, i punti appena raccolti diventano la sua
      // forma di riferimento. La linea piu' veloce e' di solito anche
      // la piu' "pulita" da vedere disegnata.
      if (recordGiro && tracciatoAttivo >= 0 && numPuntiForma >= 4) {
        salvaFormaTracciato();
      }
    }
  }

  // Questo stesso passaggio apre (o riapre) il cronometro per il prossimo giro
  inizioGiroCorrente    = adesso;
  inizioSettoreCorrente = adesso;
  giroInCorso           = true;
  prossimoCheckpoint    = 0;
  numPuntiForma         = 0;
}

// Indice indicativo 0-100: umidita' oltre il 55% e temperatura sotto i 20
// gradi fanno salire il rischio, asfalto caldo e asciutto = 0.
void calcolaRischioGrip() {
  float rischio = 0.0f;

  if (umidita > SOGLIA_UMIDITA_RISCHIO)
    rischio += (umidita - SOGLIA_UMIDITA_RISCHIO) * PESO_UMIDITA;

  if (temperatura < SOGLIA_TEMP_RISCHIO)
    rischio += (SOGLIA_TEMP_RISCHIO - temperatura) * PESO_TEMPERATURA;

  indiceRischio = constrain(rischio, 0.0f, 100.0f);
}

// ===========================================================================
// 10. PULSANTI E TARATURA
// ===========================================================================

// Dopo un click qualunque ci sono 250 ms di tempo morto, che evitano
// anche pressioni combinate accidentali.
void gestisciPulsanti() {
  bool premutoSu  = frontePressione(PIN_BTN_SU,  statoPrecSu);
  bool premutoGiu = frontePressione(PIN_BTN_GIU, statoPrecGiu);
  bool premutoOk  = frontePressione(PIN_BTN_OK,  statoPrecOk);
  bool premutoLog = frontePressione(PIN_BTN_LOG, statoPrecLog);

  if (millis() - ultimoTempoPulsante < TEMPO_DEBOUNCE) return;

  // Il cambio pagina passa dalla transizione, che disegna gia' lei la
  // pagina nuova: il verso dello scorrimento e' quello del tasto, cosi'
  // si vede da che parte ci si sta muovendo nell'elenco. Il tempo morto
  // si riarma DOPO l'animazione, altrimenti tenendo premuto il tasto le
  // pressioni si accavallerebbero sopra lo scorrimento.
  if (premutoSu) {
    // Pagina precedente (il "+ totaleSchermate" evita il modulo negativo)
    schermataCorrente = (schermataCorrente + totaleSchermate - 1) % totaleSchermate;
    transizionePagina(-1);
    ultimoTempoPulsante = millis();
  }
  else if (premutoGiu) {
    // Pagina successiva, con ritorno alla prima dopo l'ultima
    schermataCorrente = (schermataCorrente + 1) % totaleSchermate;
    transizionePagina(+1);
    ultimoTempoPulsante = millis();
  }
  else if (premutoOk) {
    // OK cambia mestiere a seconda della pagina. Sulle pagine RECORD (6,
    // 7) di proposito non fa nulla: azzerare lo storico con una
    // pressione accidentale (magari coi guanti) sarebbe irreversibile,
    // quindi l'azzeramento passa solo dal sito web, con conferma.
    if (schermataCorrente == 0 || schermataCorrente == 3) taraturaZero();
    else if (schermataCorrente == 9) {
      if (wifiAttivo) fermaWiFi();
      else avviaWiFi();
    }
    else if (schermataCorrente == 5) impostaTraguardo();
    ultimoTempoPulsante = millis();
  }
  else if (premutoLog) {
    if (!inRegistrazione) avviaRegistrazione();
    else fermaRegistrazione();
    ultimoTempoPulsante = millis();
  }
}

// Memorizza l'assetto attuale come punto zero. Da fare con la moto dritta
// e in piano: compensa il montaggio della scatola non in bolla. Corregge
// solo i gradi residui (la rotazione grossa la gestisce la rimappatura).
void taraturaZero() {
  if (!bnoOk) return;

  // Le stesse quattro grandezze di leggiIMU, ma qui servono solo i due
  // angoli: l'assetto attuale diventa il nuovo zero. Le accelerazioni si
  // leggono e si buttano, il contratto e' uno solo.
  float piegaGrezza, impennataGrezza, accelLong, accelLat;
  if (!imuLeggi(piegaGrezza, impennataGrezza, accelLong, accelLat)) return;

  offsetPiega     = piegaGrezza;
  offsetImpennata = impennataGrezza;

  // Popup di conferma, la stessa cornice degli altri due messaggi (il
  // controllo su oledOk lo fa gia' popupMessaggio). E' una pausa
  // bloccante, accettabile perche' la taratura si fa sempre da fermi.
  popupMessaggio(F("CALIBRAZIONE OK"));

  Serial.println(F("Taratura zero eseguita."));
}

// Popup centrale (sfondo nero, bordo bianco) sopra la pagina corrente.
// Ci passano tutti e tre i messaggi istantanei: conferma taratura, errore
// "GPS senza fix" e conferma traguardo.
//
// Si apre crescendo dal centro, cosi' si capisce che e' appena successo
// qualcosa invece di sembrare un disturbo dello schermo. Ogni fotogramma
// e' piu' grande del precedente e centrato, quindi il suo riempimento nero
// copre da solo il bordo di quello prima: la pagina sotto non va
// ridisegnata.
void popupMessaggio(const __FlashStringHelper *testo) {
  if (!oledOk) return;

  const int MARGINE_POPUP = 9;
  const int LARGHEZZA     = SCREEN_WIDTH - 2 * MARGINE_POPUP;
  const int ALTEZZA       = 28;
  const int CENTRO_Y      = 32;
  const int PASSI_APERTURA = 4;

  for (int passo = 1; passo <= PASSI_APERTURA; passo++) {
    int w = LARGHEZZA * passo / PASSI_APERTURA;
    int h = ALTEZZA   * passo / PASSI_APERTURA;
    int x = SCREEN_WIDTH / 2 - w / 2;
    int y = CENTRO_Y - h / 2;

    display.fillRoundRect(x, y, w, h, 4, COLORE_OFF);
    display.drawRoundRect(x, y, w, h, 4, COLORE_ON);
    display.display();
  }

  display.setCursor(centraTesto(testo), CENTRO_Y - 4);
  display.print(testo);
  display.display();
  delay(DURATA_POPUP);
}

// Salva la posizione GPS attuale come traguardo per il lap timing. Va
// fatto fermi sulla linea di partenza/arrivo, col fix gia' agganciato.
// Cambiare traguardo a meta' uscita azzera anche i giri gia' contati:
// non avrebbe senso confrontarli con una linea diversa.
void impostaTraguardo() {
  if (!gpsFixValido) {
    popupMessaggio(F("GPS senza fix!"));
    Serial.println(F("Impossibile impostare il traguardo: GPS senza fix."));
    return;
  }

  traguardoLat       = latitudineGPS;
  traguardoLon       = longitudineGPS;
  traguardoImpostato = true;

  // Nuovo traguardo, nuovo conteggio: i giri gia' fatti erano su un'altra linea
  numGiri = 0;
  giroMigliore = -1;
  giroInCorso = false;
  eraFuoriRaggio = true;
  inizioGiroCorrente = 0;
  prossimoCheckpoint = 0;
  inizioSettoreCorrente = 0;
  numPuntiForma = 0;
  for (int s = 0; s <= MAX_SETTORI; s++) tempiSettoreMigliori[s] = 0;

  // Se e' selezionato un tracciato salvato, la nuova posizione diventa
  // permanente per quel tracciato (utile per affinare la linea stando
  // li' fisicamente). In modalita' libera invece resta solo in RAM,
  // esattamente come per un tracciato non salvato.
  if (tracciatoAttivo >= 0) {
    tracciatoCorrente.traguardoLat = traguardoLat;
    tracciatoCorrente.traguardoLon = traguardoLon;
    salvaTracciato(tracciatoAttivo, tracciatoCorrente);
  }

  popupMessaggio(F("TRAGUARDO OK"));
  Serial.println(F("Traguardo impostato sulla posizione attuale."));
}

// ===========================================================================
// 11. REGISTRAZIONE E RECORD STORICI SU MEMORIA FLASH
// ===========================================================================

// Trova il primo nome libero LOG_n.CSV, scrive l'intestazione delle colonne
// e azzera tutte le statistiche di sessione.
void avviaRegistrazione() {
  if (!memoriaOk) {
    // Ritento il mount: copre il raro caso di errore al boot
    memoriaOk = LittleFS.begin(true);
    if (!memoriaOk) {
      Serial.println(F("ERRORE: memoria flash non disponibile."));
      return;
    }
  }

  // Controllo dello spazio prima di partire (vedi MIN_SPAZIO_LIBERO)
  if (LittleFS.totalBytes() - LittleFS.usedBytes() < MIN_SPAZIO_LIBERO) {
    Serial.println(F("ERRORE: memoria quasi piena. Cancella i vecchi log dal sito."));
    return;
  }

  // Numerazione progressiva. Il tetto MAX_SESSIONI evita un loop
  // infinito su filesystem corrotto.
  int idSessione = 0;
  do {
    idSessione++;
    snprintf(nomeFileLog, sizeof(nomeFileLog), "/LOG_%d.CSV", idSessione);
  } while (LittleFS.exists(nomeFileLog) && idSessione < MAX_SESSIONI);

  // Numerazione esaurita: meglio non partire che sovrascrivere in
  // silenzio una sessione esistente (caso limite, ma gratis da coprire)
  if (LittleFS.exists(nomeFileLog)) {
    Serial.println(F("ERRORE: numerazione LOG esaurita. Cancella i vecchi log dal sito."));
    return;
  }

  File logFile = LittleFS.open(nomeFileLog, FILE_WRITE);
  if (!logFile) {
    Serial.println(F("ERRORE: impossibile creare il file di log."));
    return;
  }
  // Intestazione: Minuto + colonne canali (velocita' inclusa, e' un
  // canale come gli altri) + meteo + posizione GPS + giro in corso
  logFile.print(F("Minuto"));
  for (int c = 0; c < N_CANALI; c++) {
    logFile.print(',');
    logFile.print(NOME_CANALE[c]);
  }
  logFile.println(F(",Temp_C,Umid_%,Rischio_%,Lat,Lon,Giro"));
  logFile.close();

  // Sessione nuova: azzero massimi, contatori eventi, flag record e timer
  inRegistrazione  = true;
  minutiRegistrati = 0;
  azzeraStatistiche();
  for (int c = 0; c < N_CANALI; c++) nuovoRecord[c] = false;
  inizioRegistrazione = millis();
  ultimoSalvataggio = millis();

  Serial.print(F("REC AVVIATA -> File: "));
  Serial.println(nomeFileLog);
}

// Le righe al minuto restano solo per i minuti completi (il parziale viene
// scartato di proposito), ma riepilogo ed eventi coprono tutta la sessione.
void fermaRegistrazione() {
  inRegistrazione = false;
  lampeggioRec = false;
  Serial.println(F("REC ARRESTATA. File chiuso."));

  scriviRiepilogoSuFlash();
  // Entrambe PRIMA del riepilogo a schermo: cosi' gli asterischi "nuovo
  // record" (canali e giro) sono gia' pronti quando si disegna la pagina
  aggiornaRecordStorici();       // canali: sul file del tracciato attivo (o quello globale)
  aggiornaRecordGiroTracciato(); // giro migliore di sempre, solo se c'e' un tracciato attivo
  mostraRiepilogoSessione();
}

// Scrive i massimi dell'ultimo minuto e li azzera. Il file viene aperto e
// chiuso ogni volta: se salta l'alimentazione si perde al massimo un
// minuto, mai il file intero.
void scriviDatiSuFlash() {
  File logFile = LittleFS.open(nomeFileLog, FILE_APPEND);
  if (!logFile) {
    // Memoria piena o filesystem in errore: non azzero i record, ritento
    // al prossimo minuto senza perdere i massimi accumulati finora.
    Serial.println(F("ERRORE: scrittura in flash fallita (memoria piena?)."));
    return;
  }

  minutiRegistrati++;

  // Una riga CSV, stesso ordine dell'intestazione
  logFile.print(minutiRegistrati);
  for (int c = 0; c < N_CANALI; c++) {
    logFile.print(',');
    logFile.print(massimiMinuto[c], decimaliCanale(c));
  }
  logFile.print(',');
  logFile.print(temperatura, 1);  logFile.print(',');
  logFile.print(umidita, 1);      logFile.print(',');
  logFile.print(indiceRischio, 0);

  // Posizione e giro in corso a fine minuto: utili per rileggere dove si
  // era e su quale giro, mesi dopo, senza dover riguardare la traccia
  // live sul sito (che vive solo nel browser, non viene salvata). Lat/Lon
  // restano quelle dell'ultimo fix valido anche se il GPS lo ha appena
  // perso (vedi leggiGPS): un'ultima posizione nota e' piu' utile di
  // "0,0", che sarebbe in mezzo all'oceano.
  int giroAttuale = (traguardoImpostato && giroInCorso) ? (numGiri + 1) : 0;
  logFile.print(',');
  logFile.print(latitudineGPS, 6);
  logFile.print(',');
  logFile.print(longitudineGPS, 6);
  logFile.print(',');
  logFile.println(giroAttuale);

  logFile.close();

  azzeraCanali(massimiMinuto);  // il prossimo minuto riparte da zero
}

// Accoda al CSV il riepilogo nello stesso formato a colonne del resto del
// file (in Excel appare gia' incolonnato): massimi di sessione + eventi.
void scriviRiepilogoSuFlash() {
  File logFile = LittleFS.open(nomeFileLog, FILE_APPEND);
  if (!logFile) {
    Serial.println(F("ERRORE: impossibile scrivere il riepilogo in memoria."));
    return;
  }

  // Massimi dell'intera sessione
  logFile.println();  // riga vuota che separa il riepilogo dai dati

  // Tracciato attivo durante questa sessione: utile riaprendo il file
  // tra mesi, quando magari il tracciato e' stato anche rinominato
  logFile.print(F("TRACCIATO,"));
  if (tracciatoAttivo < 0) logFile.println(F("libera"));
  else logFile.println(tracciatoCorrente.nome);

  logFile.print(F("RIEPILOGO"));
  for (int c = 0; c < N_CANALI; c++) {
    logFile.print(',');
    logFile.print(NOME_CANALE[c]);
  }
  logFile.println(F(",Minuti_Tot"));

  logFile.print(F("MAX"));
  for (int c = 0; c < N_CANALI; c++) {
    logFile.print(',');
    logFile.print(massimiSessione[c], decimaliCanale(c));
  }
  logFile.print(',');
  logFile.println(minutiRegistrati);

  // Contatore eventi, con le soglie scritte nell'intestazione cosi' i
  // numeri restano interpretabili anche riaprendo il file tra mesi
  logFile.print(F("EVENTI"));
  for (int e = 0; e < N_EVENTI; e++) {
    logFile.print(',');
    logFile.print(NOME_EVENTO_CSV[e]);
    logFile.print('>');
    if (eventoInG(e)) {
      logFile.print(SOGLIA_EVENTO[e], 1);
      logFile.print('G');
    } else {
      logFile.print(SOGLIA_EVENTO[e], 0);
    }
  }
  logFile.println();

  logFile.print(F("TOT"));
  for (int e = 0; e < N_EVENTI; e++) {
    logFile.print(',');
    logFile.print(conteggioEventi[e]);
  }
  logFile.println();

  // Tempi giro, solo se era impostato un traguardo e ne e' stato
  // completato almeno uno. Stessa idea dell'asterisco "nuovo record":
  // qui marca il giro piu' veloce della sessione appena chiusa. Se il
  // tracciato ha dei checkpoint, ogni riga guadagna una colonna per
  // settore (Settore_1, Settore_2, ...): leggiInfoSessione() continua a
  // leggere solo il tempo totale subito dopo la prima virgola, quindi
  // resta compatibile con i file scritti prima che esistessero i settori.
  if (numGiri > 0) {
    int numSettoriAttivi = tracciatoCorrente.numCheckpoint + 1;

    logFile.println();
    logFile.print(F("GIRI,Tempo_s"));
    if (numSettoriAttivi > 1) {
      for (int s = 0; s < numSettoriAttivi; s++) {
        logFile.print(F(",Settore_"));
        logFile.print(s + 1);
      }
    }
    logFile.println();

    for (int g = 0; g < numGiri; g++) {
      logFile.print(F("Giro_"));
      logFile.print(g + 1);
      logFile.print(',');
      logFile.print(tempiGiro[g] / 1000.0, 2);
      if (g == giroMigliore) logFile.print('*');
      if (numSettoriAttivi > 1) {
        for (int s = 0; s < numSettoriAttivi; s++) {
          logFile.print(',');
          logFile.print(tempiSettorePerGiro[g][s] / 1000.0, 2);
        }
      }
      logFile.println();
    }
  }

  logFile.close();

  Serial.println(F("Riepilogo sessione scritto in memoria."));
}

// --- Record storici --------------------------------------------------------
// La struct viene scritta e riletta cosi' com'e' in RAM, senza parsing
// di testo e senza errori di formato. A fare da controllo c'e' la firma
// "magic", che contiene anche la
// versione del formato: se in futuro cambio la struct mi basta cambiare
// MAGIC_RECORD, i vecchi file non tornano piu' validi e si riparte da
// zero invece di leggere numeri sballati.

// File dei record ATTIVO in questo momento: quello del tracciato
// selezionato, o FILE_RECORD in modalita' libera. Un solo punto dove si
// decide "quale storico e' in RAM adesso": caricaRecordStorici(),
// salvaRecordStorici() e aggiornaRecordStorici() lo usano tutti, cosi'
// cambiare tracciato cambia da solo quale storico si legge e si scrive
// senza toccare il resto del firmware.
String fileRecordAttivo() {
  return (tracciatoAttivo < 0) ? String(FILE_RECORD) : percorsoRecordTracciato(tracciatoAttivo);
}

// Legge il file dei record del tracciato attivo (o quello globale in
// modalita' libera). Se non esiste (primissimo avvio, o un tracciato
// appena creato) o non supera i controlli, si resta coi record a zero:
// nessun errore bloccante, il sistema parte lo stesso.
void caricaRecordStorici() {
  recordStorici = RecordStorici{};  // riparto pulito: il file attivo potrebbe essere un altro
  if (!memoriaOk) return;

  File f = LittleFS.open(fileRecordAttivo(), FILE_READ);
  if (!f) {
    Serial.println(F("Nessun file record per questo storico: si parte da zero."));
    return;
  }

  RecordStorici letto;
  size_t byteLetti = f.read((uint8_t *)&letto, sizeof(letto));
  f.close();

  // Doppio controllo: dimensione giusta E firma giusta. Copre i file
  // troncati, corrotti o scritti da una versione vecchia del firmware.
  if (byteLetti == sizeof(letto) && letto.magic == MAGIC_RECORD) {
    recordStorici = letto;
    Serial.print(F("Record storici caricati ("));
    Serial.print(recordStorici.sessioni);
    Serial.println(F(" sessioni)."));
  } else {
    Serial.println(F("File record non valido: riparto da zero."));
  }
}

// Riscrive il file da capo con lo stato attuale. Succede una volta per
// sessione (piu' gli azzeramenti dal web): per l'usura della flash e'
// un carico trascurabile.
void salvaRecordStorici() {
  if (!memoriaOk) return;

  recordStorici.magic = MAGIC_RECORD;  // la firma la metto sempre io qui
  File f = LittleFS.open(fileRecordAttivo(), FILE_WRITE);
  if (!f) {
    Serial.println(F("ERRORE: impossibile salvare i record storici."));
    return;
  }
  f.write((const uint8_t *)&recordStorici, sizeof(recordStorici));
  f.close();
}

// Chiamata a fine registrazione: confronta i massimi della sessione
// appena chiusa coi record storici, aggiorna quelli battuti (accendendo
// i flag che diventano asterischi nel riepilogo) e salva su flash.
// I massimi di sessione escono dal filtro anti-buca, quindi anche i
// record ereditano la stessa protezione dai picchi spurii.
void aggiornaRecordStorici() {
  for (int c = 0; c < N_CANALI; c++) {
    nuovoRecord[c] = massimiSessione[c] > recordStorici.canali[c];
    if (nuovoRecord[c]) recordStorici.canali[c] = massimiSessione[c];
  }

  // Contatori "carriera": anche una sessione senza record fa +1
  recordStorici.sessioni++;
  recordStorici.minutiTotali += minutiRegistrati;

  salvaRecordStorici();
}

// ===========================================================================
// 11B. TRACCIATI (GESTIONE MULTI-CIRCUITO)
// ===========================================================================
// Ogni tracciato vive in un file di definizione (nome + traguardo) piu' un
// file di record separato (stessa struct RecordStorici della modalita'
// libera, vedi fileRecordAttivo() piu' sopra). Qui ci sono solo le
// funzioni di supporto: la UI vera e propria e' sulla pagina web
// /tracciati e sulla pagina OLED GIRI (che riusa i campi gia' esposti).

// Percorso del file di definizione di un tracciato dato il suo id
String percorsoTracciato(int id) {
  return String(PREFISSO_TRACK) + String(id) + String(SUFFISSO_TRACK_DEF);
}

// Percorso del file record storici di un tracciato dato il suo id
String percorsoRecordTracciato(int id) {
  return String(PREFISSO_TRACK) + String(id) + String(SUFFISSO_TRACK_REC);
}

// Legge la definizione di un tracciato dal file. true se il file esiste
// ed e' valido (stesso doppio controllo dimensione+magic di caricaRecordStorici).
bool caricaTracciato(int id, Tracciato &out) {
  File f = LittleFS.open(percorsoTracciato(id), FILE_READ);
  if (!f) return false;

  Tracciato letto;
  size_t byteLetti = f.read((uint8_t *)&letto, sizeof(letto));
  f.close();

  if (byteLetti != sizeof(letto) || letto.magic != MAGIC_TRACCIATO) return false;
  out = letto;
  return true;
}

// Scrive la definizione di un tracciato sulla flash (stesso pattern di
// salvaRecordStorici: si riscrive tutto da capo, costo trascurabile)
void salvaTracciato(int id, Tracciato &t) {
  t.magic = MAGIC_TRACCIATO;
  File f = LittleFS.open(percorsoTracciato(id), FILE_WRITE);
  if (!f) {
    Serial.println(F("ERRORE: impossibile salvare il tracciato."));
    return;
  }
  f.write((const uint8_t *)&t, sizeof(t));
  f.close();
}

// Copia i punti appena raccolti (vedi rilevaGiro, quando il giro chiuso
// e' il migliore di sessione) nella forma del tracciato attivo e la
// salva. Aggiorna solo forma+numPuntiForma: il resto della struct
// (traguardo, nome, record, checkpoint) resta quello gia' in
// tracciatoCorrente, invariato.
void salvaFormaTracciato() {
  tracciatoCorrente.numPuntiForma = numPuntiForma;
  for (int i = 0; i < numPuntiForma; i++) {
    tracciatoCorrente.formaLat[i] = formaLat[i];
    tracciatoCorrente.formaLon[i] = formaLon[i];
  }
  salvaTracciato(tracciatoAttivo, tracciatoCorrente);
  Serial.print(F("Forma del tracciato aggiornata ("));
  Serial.print(numPuntiForma);
  Serial.println(F(" punti)."));
}

// Trova il primo id libero per un nuovo tracciato: numerazione
// progressiva mai riassegnata (stessa idea di avviaRegistrazione() per
// i LOG_n.CSV), cosi' un id eliminato non si confonde con uno nuovo.
int primoIdTracciatoLibero() {
  for (int id = 1; id <= MAX_TRACCIATI; id++) {
    if (!LittleFS.exists(percorsoTracciato(id))) return id;
  }
  return -1;  // tutti gli slot occupati
}

// Ripulisce un nome arrivato da web: fuori i caratteri che romperebbero
// il JSON di /dati o il CSV di riepilogo (virgolette, backslash, virgola,
// ritorno a capo). Il nome resta leggibile, solo un po' meno "libero"
// nei simboli.
void sanitizzaNome(char *nome) {
  for (int i = 0; nome[i] != '\0'; i++) {
    char c = nome[i];
    if (c == '"' || c == '\\' || c == ',' || c == '\n' || c == '\r') nome[i] = '_';
  }
}

// Attiva un tracciato: lo carica in RAM, ricarica i suoi record storici e
// imposta il traguardo per il lap timing. id<0 torna alla modalita'
// libera (RECORD.BIN globale, traguardo "volante" azzerato). In entrambi
// i casi i contatori giro ripartono da zero: un tracciato diverso e' una
// linea diversa, i giri gia' contati non hanno piu' senso.
void selezionaTracciato(int id) {
  if (id < 0) {
    tracciatoAttivo = -1;
    tracciatoCorrente = Tracciato{};
    traguardoImpostato = false;
  } else {
    if (!caricaTracciato(id, tracciatoCorrente)) {
      Serial.println(F("ERRORE: tracciato non trovato o non valido."));
      return;
    }
    tracciatoAttivo    = id;
    traguardoLat       = tracciatoCorrente.traguardoLat;
    traguardoLon       = tracciatoCorrente.traguardoLon;
    traguardoImpostato = true;
  }

  numGiri = 0;
  giroMigliore = -1;
  giroInCorso = false;
  eraFuoriRaggio = true;
  inizioGiroCorrente = 0;
  prossimoCheckpoint = 0;
  inizioSettoreCorrente = 0;
  numPuntiForma = 0;
  for (int s = 0; s <= MAX_SETTORI; s++) tempiSettoreMigliori[s] = 0;

  // Storico canali: quello del tracciato appena attivato, o quello
  // globale in modalita' libera (la scelta la fa fileRecordAttivo())
  caricaRecordStorici();

  Serial.print(F("Tracciato attivo: "));
  if (id < 0) Serial.println(F("nessuno (modalita' libera)"));
  else Serial.println(tracciatoCorrente.nome);
}

// Confronta il giro migliore di questa sessione con il record di sempre
// del tracciato attivo, e lo salva se e' stato battuto. Senza tracciato
// selezionato non c'e' nulla da confrontare: in modalita' libera il giro
// migliore resta solo di sessione, si perde alla REC successiva.
void aggiornaRecordGiroTracciato() {
  if (tracciatoAttivo < 0 || giroMigliore < 0) return;

  unsigned long migliore = tempiGiro[giroMigliore];
  if (tracciatoCorrente.migliorGiroMs == 0 || migliore < tracciatoCorrente.migliorGiroMs) {
    tracciatoCorrente.migliorGiroMs = migliore;
    salvaTracciato(tracciatoAttivo, tracciatoCorrente);
    Serial.println(F("Nuovo giro record per questo tracciato."));
  }
}

// ===========================================================================
// 15. DEBUG SU SERIAL MONITOR
// ===========================================================================

// Telemetria ogni 5 secondi, senza di questa e' impossibile fare debug
void stampaSeriale() {
  Serial.println(F("---- TELEMETRIA LIVE ----"));
  Serial.print(F("Piega    : ")); Serial.print(angoloPiega);     Serial.println(F(" deg"));
  Serial.print(F("Impennata: ")); Serial.print(angoloImpennata);
  Serial.print(F(" deg | Stoppie: ")); Serial.print(angoloStoppie); Serial.println(F(" deg"));
  Serial.print(F("G_Lat    : ")); Serial.print(forzaGLaterale);
  Serial.print(F(" | G_Lon: "));  Serial.println(forzaGLongitudinale);
  Serial.print(F("Meteo    : ")); Serial.print(temperatura);
  Serial.print(F(" C / "));       Serial.print(umidita); Serial.println(F(" %"));

  Serial.print(F("GPS      : "));
  if (gpsFixValido) {
    Serial.print(F("fix OK, "));
    Serial.print(velocitaGPS, 0); Serial.print(F(" km/h, "));
    Serial.print(satellitiGPS);   Serial.print(F(" sat, "));
    Serial.print(latitudineGPS, 5); Serial.print(F(", "));
    Serial.println(longitudineGPS, 5);
  } else {
    Serial.print(F("NESSUN FIX ("));
    Serial.print(satellitiGPS);
    Serial.println(F(" sat agganciati)"));
  }

  Serial.print(F("Giri     : "));
  if (!traguardoImpostato) {
    Serial.println(F("traguardo non impostato"));
  } else {
    Serial.print(numGiri); Serial.print(F(" completati"));
    if (giroMigliore >= 0) {
      Serial.print(F(", migliore "));
      Serial.print(tempiGiro[giroMigliore] / 1000.0, 2);
      Serial.print('s');
    }
    Serial.println();
  }

  // Contatori eventi, tutti su una riga
  Serial.print(F("Eventi   : "));
  for (int e = 0; e < N_EVENTI; e++) {
    Serial.print(NOME_EVENTO_CSV[e]);
    Serial.print('=');
    Serial.print(conteggioEventi[e]);
    Serial.print(' ');
  }
  Serial.println();

  // Il dettaglio della calibrazione lo stampa il driver: quali contatori
  // esistano dipende dal chip (il BNO055 ne ha quattro, un altro puo'
  // averne uno solo o nessuno). Qui si sa solo che c'e' una riga da
  // stampare.
  if (bnoOk) imuStampaCalibrazione();
  else       Serial.println(F("Calibraz.: IMU ASSENTE"));
  Serial.println(F("-------------------------"));
}
