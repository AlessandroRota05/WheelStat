/*
  ===========================================================================
  WheelStat   Alessandro Rota - Apache License 2.0 - v9.2
  ===========================================================================

  WIFI
    Access point "WheelStat": dal telefono su http://192.168.4.1 si
    scaricano (e cancellano) i CSV e si vede la telemetria live.

  STORICO VERSIONI
  v6.3  rimappatura hardware BNO055 per il montaggio reale
  v6.5  statistica stoppie
  v6.6  verso impennata/stoppie corretto + zona morta 
  v6.7  contatore eventi di sessione con soglie
  v7.0  WiFi: download CSV e telemetria live con grafici dal telefono
  v7.1  password nuova ritoccata interfaccia
  v7.2  passati di commenti nuovi per sistemare il casino
  v8.0  riscrittura organizzata
  v8.1  calibrazione guidata all'avvio
  v8.2  schermata di calibrazione ridisegnata
  v8.3  interfaccia OLED uniformata: tutte le pagine live hanno titolo
        centrato con riga di separazione alla stessa quota e dati su una
        griglia  
  v9.0  logging spostato dalla MicroSD alla memoria flash interna
  v9.1  riepilogo di fine sessione riorganizzato
  v9.2  revisione di robustezza, funzionalita' invariata: valori meteo
        di partenza neutri (niente rischio 70% fittizio prima della
        prima lettura del DHT22 o a sensore guasto)
*/

// ===========================================================================
// 1. LIBRERIE
// ===========================================================================

#include <Wire.h>              // bus I2C
#include <LittleFS.h>          // filesystem sulla flash interna
#include <WiFi.h>              // wifi
#include <WebServer.h>         // server HTTP
#include <Adafruit_GFX.h>      // primitive grafiche
#include <Adafruit_SSD1306.h>  // driver del pannello OLED
#include <Adafruit_BNO055.h>   // driver dell'IMU
#include <Adafruit_Sensor.h>   // libreria complementare
#include <utility/imumaths.h>  // matematica
#include "DHT.h"               // driver della temperatura

// ===========================================================================
// 2. PIN E INDIRIZZI
// ===========================================================================

// Pulsanti attivi bassi (premuto = LOW), pull-up interna dell'ESP32:
// niente resistenze esterne, un capo al pin e l'altro a GND.
const uint8_t PIN_BTN_SU  = 13;
const uint8_t PIN_BTN_GIU = 25;  
                                
const uint8_t PIN_BTN_OK  = 14;
const uint8_t PIN_BTN_LOG = 27;

const uint8_t PIN_DHT     = 4;   
const uint8_t PIN_I2C_SDA = 21;
const uint8_t PIN_I2C_SCL = 22;

const uint8_t  I2C_ADDR_OLED = 0x3C;
const uint8_t  I2C_ADDR_BNO  = 0x28;    
const uint32_t I2C_CLOCK_HZ  = 400000;  // alzato per diminuire imput lag

// ===========================================================================
// 3. CONFIGURAZIONE
// ===========================================================================

const float GRAVITA = 9.81f;  // per passare da m/s^2 a G

//in base a come sono predisposti componenti, nel mio caso imu montata sottosopra
const Adafruit_BNO055::adafruit_bno055_axis_remap_config_t REMAP_ASSI =
    Adafruit_BNO055::REMAP_CONFIG_P5;
const Adafruit_BNO055::adafruit_bno055_axis_remap_sign_t REMAP_SEGNI =
    Adafruit_BNO055::REMAP_SIGN_P5;

// Versi di rotazione e accelerazione:
//  - inclina la scatola a destra      deve salire Piega Dx
//  - spingi la scatola in avanti      long positivo, pallino in alto
//  - spingi la scatola a sinistra     pallino a sinistra
//  - alza il davanti (muso su)        deve salire Impennata, non Stoppie
// Se un verso esce al contrario, inverti il segno corrispondente.
const float SEGNO_PIEGA     =  1.0f;
const float SEGNO_IMPENNATA = -1.0f;
const float SEGNO_G_LONG    =  1.0f;
const float SEGNO_G_LAT     = -1.0f;

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
const unsigned long INTERVALLO_METEO     = 2000;   // il DHT22 non regge di piu'
const unsigned long INTERVALLO_SERIALE   = 5000;   // telemetria di debug
const unsigned long INTERVALLO_LAMPEGGIO = 500;    // asterisco "* REC"
const unsigned long INTERVALLO_LOG       = 60000;  // un record CSV al minuto
const unsigned long TEMPO_DEBOUNCE       = 250;    // tempo morto dopo ogni tasto
const unsigned long DURATA_POPUP         = 800;    // popup "CALIBRAZIONE OK"
const unsigned long DURATA_SPLASH        = 1500;   // logo all'accensione
const unsigned long PAUSA_LOOP           = 10;     // respiro per il watchdog
const unsigned long RIEPILOGO_TIMEOUT    = 30000;  // uscita automatica dal riepilogo

// --- Memoria e WiFi -----------------------------------------------------------
const int MAX_SESSIONI = 9999;  // tetto alla numerazione LOG_n.CSV

// Sotto questa soglia di spazio libero la registrazione non parte
const unsigned long MIN_SPAZIO_LIBERO = 32 * 1024UL;  // byte

// Access point per il telefono
const char WIFI_SSID[]     = "WheelStat";
const char WIFI_PASSWORD[] = "30elode!";

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
  N_CANALI
};

// Nomi delle colonne CSV, nello stesso ordine dell'enum
const char *NOME_CANALE[N_CANALI] = {
  "Piega_Dx", "Piega_Sx", "Impennata", "Stoppie",
  "GLat_Dx", "GLat_Sx", "G_Accel", "G_Frena"
};

// Cifre decimali nel CSV: 1 per gli angoli, 2 per le G
int decimaliCanale(int c) {
  return (c <= C_STOPPIE) ? 1 : 2;
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

const uint8_t SCREEN_WIDTH  = 128;
const uint8_t SCREEN_HEIGHT = 64;
// Ultimo parametro -1: nessun pin di reset dedicato per l'OLED
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// La fusione sensoriale avviene dentro al BNO055: arrivano direttamente
// angoli in gradi e accelerazione lineare gia' senza gravita'.
Adafruit_BNO055 bno = Adafruit_BNO055(55, I2C_ADDR_BNO, &Wire);

DHT dht(PIN_DHT, DHT22);

// Server web su porta 80
WebServer server(80);

// --- Esito del boot, per non interrogare periferiche assenti ---
bool oledOk    = false;
bool bnoOk     = false;
bool memoriaOk = false;

// --- Interfaccia ---
int  schermataCorrente = 0;   // 0 Piega, 1 Meteo, 2 G, 3 Impennata,
                              // 4 Record, 5 Memoria, 6 WiFi
const int totaleSchermate = 7;
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
// I massimi mai registrati, sessione dopo sessione. Vivono in un piccolo
// file binario sulla flash (FILE_RECORD), quindi sopravvivono allo
// spegnimento. Si aggiornano solo alla FINE di ogni registrazione:
// contano solo le manovre fatte col REC attivo, se non e' registrato
// non e' un record. Il nome non inizia con "LOG_", cosi' non compare
// nell'elenco sessioni del sito e non e' toccabile da /scarica e
// /elimina, che accettano solo i file di log.
const char     FILE_RECORD[] = "/RECORD.BIN";
const uint32_t MAGIC_RECORD  = 0x57535231;  // "WSR1": firma + versione del formato

struct RecordStorici {
  uint32_t magic;             // firma: se non torna, il file non e' mio
  float    canali[N_CANALI];  // massimo storico di ogni canale
  uint16_t sessioni;          // registrazioni completate in totale
  uint32_t minutiTotali;      // minuti loggati in tutta la vita del dispositivo
};
RecordStorici recordStorici = {};  // tutto a zero finche' non carico il file

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

  // Pulsanti verso GND con pull-up interna: a riposo leggono HIGH
  pinMode(PIN_BTN_SU,  INPUT_PULLUP);
  pinMode(PIN_BTN_GIU, INPUT_PULLUP);
  pinMode(PIN_BTN_OK,  INPUT_PULLUP);
  pinMode(PIN_BTN_LOG, INPUT_PULLUP);

  // Bus I2C condiviso da OLED e BNO055. A 400 kHz il frame OLED passa da
  // ~90 a ~23 ms: senza, i 10 FPS non ci stanno e lagga.
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  Wire.setClock(I2C_CLOCK_HZ);

  // --- OLED + splash ---
  Serial.print(F("Boot OLED...... "));
  oledOk = display.begin(SSD1306_SWITCHCAPVCC, I2C_ADDR_OLED);
  if (oledOk) {
    Serial.println(F("OK."));
    display.clearDisplay();
    display.drawRoundRect(8, 12, 112, 40, 6, SSD1306_WHITE);
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 24);
    display.print(F("WHEELSTAT"));
    display.display();
  } else {
    Serial.println(F("ERRORE! Controlla SDA/SCL."));
  }

  // --- DHT22 (gli errori escono come NaN a runtime, gestiti in leggiMeteo) ---
  Serial.print(F("Boot DHT22..... "));
  dht.begin();
  Serial.println(F("OK."));

  // --- BNO055 in modalita' NDOF: fusione completa a 9 assi, assetto
  //     assoluto senza deriva. Range e filtri li gestisce il chip. ---
  Serial.print(F("Boot BNO055.... "));
  bnoOk = bno.begin(OPERATION_MODE_NDOF);
  if (bnoOk) {
    Serial.println(F("OK."));
    // Rimappatura hardware per il montaggio reale: da qui in poi il chip
    // lavora come se fosse piatto con X verso il davanti della moto, e il
    // resto del firmware non deve sapere come e' girata la scatola.
    bno.setAxisRemap(REMAP_ASSI);
    bno.setAxisSign(REMAP_SEGNI);
    bno.setExtCrystalUse(true);  
    delay(100);                  
  } else {
    Serial.println(F("ERRORE! BNO055 non trovato (prova indirizzo 0x29)."));
  }

  // --- Memoria flash (LittleFS). Il "true" formatta la partizione se al
  //     primissimo avvio non e' ancora inizializzata. ---
  Serial.print(F("Boot memoria... "));
  memoriaOk = LittleFS.begin(true);
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
  server.on("/dati", webDati);
  server.on("/scarica", webScarica);
  server.on("/elimina", webElimina);
  server.on("/azzera_record", webAzzeraRecord);
  server.onNotFound([]() { server.send(404, "text/plain", "Pagina inesistente"); });

  Serial.println(F("=== SISTEMA PRONTO ==="));

  delay(DURATA_SPLASH);      // lascia il logo a schermo per un attimo
  attesaCalibrazioneIMU();   // blocca tutto finche' il magnetometro non e' pronto
}

// Schermata di calibrazione imu all'avvio, saltabile premendo un tasto
void attesaCalibrazioneIMU() {
  if (!bnoOk || !oledOk) return;  // senza IMU o display non ha senso

  bool calibrata = false;
  unsigned long ultimoDisegno = 0;

  while (true) {
    // Stato di calibrazione letto dal chip (0 = niente, 3 = perfetto)
    uint8_t calSys, calGyro, calAcc, calMag;
    bno.getCalibration(&calSys, &calGyro, &calAcc, &calMag);

    if (calMag >= 3) {
      calibrata = true;
      break;
    }
    // OK salta la calibrazione
    if (frontePressione(PIN_BTN_OK, statoPrecOk)) break;

    // Ridisegno a 10 FPS come il resto dell'interfaccia
    if (trascorsi(ultimoDisegno, INTERVALLO_DISPLAY)) {
      display.clearDisplay();
      barraTitolo(16, F("CALIBRAZIONE IMU"));

      // disegno 8 per calibrazione
      display.drawCircle(28, 26, 10, SSD1306_WHITE);
      display.drawCircle(28, 46, 10, SSD1306_WHITE);
      // Freccia in cima: si va verso destra...
      display.fillTriangle(26, 13, 26, 19, 33, 16, SSD1306_WHITE);
      // ...e in fondo si torna verso sinistra
      display.fillTriangle(30, 53, 30, 59, 23, 56, SSD1306_WHITE);

      // A destra solo il valore che conta
      display.setTextSize(2);
      display.setCursor(58, 16);
      display.print(F("MAG"));
      display.setTextSize(3);
      display.setCursor(58, 32);
      display.print(calMag);
      display.print(F("/3"));
      display.setTextSize(1);

      display.setCursor(58, 56);
      display.print(F("[OK] salta"));

      display.display();
    }
    delay(PAUSA_LOOP);
  }

  // Pulizia: niente eventi o massimi ereditati dai movimenti di calibrazione
  azzeraStatistiche();

  // A calibrazione riuscita, conferma a schermo e attesa di un tasto:
  if (calibrata) {
    Serial.println(F("Magnetometro calibrato (MAG=3)."));

    display.clearDisplay();
    barraTitolo(16, F("CALIBRAZIONE IMU"));

    display.setTextSize(2);
    display.setCursor(10, 22);
    display.print(F("CALIBRATO"));
    display.setTextSize(1);

    display.setCursor(43, 42);
    display.print(F("MAG 3/3"));

    display.setCursor(4, 56);
    display.print(F("[tasto] per iniziare"));

    display.display();
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

// Se una lettura del DHT22 fallisce
void leggiMeteo() {
  float t = dht.readTemperature();
  float u = dht.readHumidity();

  if (!isnan(t)) temperatura = t;
  if (!isnan(u)) umidita = u;

  calcolaRischioGrip();
}

// Lettura IMU: aggiorna i valori live per display e web, poi passa la mano
// al filtro anti-buca che alimenta record e contatori.
void leggiIMU() {
  if (!bnoOk) return;

  // Due viste degli stessi dati, calcolate dal chip:
  //   VECTOR_EULER       -> angoli di assetto in gradi, gia' fusi e filtrati
  //   VECTOR_LINEARACCEL -> accelerazione in m/s^2 con la gravita' gia' tolta
  sensors_event_t orientazione, accelLineare;
  bno.getEvent(&orientazione, Adafruit_BNO055::VECTOR_EULER);
  bno.getEvent(&accelLineare, Adafruit_BNO055::VECTOR_LINEARACCEL);

  // Gravita' gia' sottratta, quindi basta dividere per 9.81: i valori
  // restano corretti anche con la moto in piega.
  forzaGLongitudinale = SEGNO_G_LONG * accelLineare.acceleration.x / GRAVITA;
  forzaGLaterale      = SEGNO_G_LAT  * accelLineare.acceleration.y / GRAVITA;

  // il BNO0 chiama "roll" (.y) la rotazione sull'asse
  // LATERALE e "pitch" (.z) quella sull'asse di MARCIA, al contrario dei
  // nomi aeronautici. Quindi .z -> piega, .y -> impennata.
  // verificato con la v6.3, che li aveva scambiati.
  piegaLive = SEGNO_PIEGA     * normalizzaAngolo(orientazione.orientation.z - offsetPiega);
  pitchLive = SEGNO_IMPENNATA * normalizzaAngolo(orientazione.orientation.y - offsetImpennata);

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

//dopo un click qualunque ci sono 250 ms di tempo morto, che evitano anche pressioni combinate accidentali.
void gestisciPulsanti() {
  bool premutoSu  = frontePressione(PIN_BTN_SU,  statoPrecSu);
  bool premutoGiu = frontePressione(PIN_BTN_GIU, statoPrecGiu);
  bool premutoOk  = frontePressione(PIN_BTN_OK,  statoPrecOk);
  bool premutoLog = frontePressione(PIN_BTN_LOG, statoPrecLog);

  if (millis() - ultimoTempoPulsante < TEMPO_DEBOUNCE) return;

  if (premutoSu) {
    // Pagina precedente (il "+ totaleSchermate" evita il modulo negativo)
    schermataCorrente = (schermataCorrente + totaleSchermate - 1) % totaleSchermate;
    ultimoTempoPulsante = millis();
  }
  else if (premutoGiu) {
    // Pagina successiva, con ritorno alla prima dopo l'ultima
    schermataCorrente = (schermataCorrente + 1) % totaleSchermate;
    ultimoTempoPulsante = millis();
  }
  else if (premutoOk) {
    // OK cambia mestiere a seconda della pagina. Sulla pagina RECORD (4)
    // di proposito non fa nulla: azzerare lo storico con una pressione
    // accidentale (magari coi guanti) sarebbe irreversibile, quindi
    // l'azzeramento passa solo dal sito web, con conferma.
    if (schermataCorrente == 0 || schermataCorrente == 3) taraturaZero();
    else if (schermataCorrente == 6) {
      if (wifiAttivo) fermaWiFi();
      else avviaWiFi();
    }
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

  sensors_event_t orientazione;
  bno.getEvent(&orientazione, Adafruit_BNO055::VECTOR_EULER);

  // Stessi canali di leggiIMU: .z = piega, .y = impennata
  offsetPiega     = orientazione.orientation.z;
  offsetImpennata = orientazione.orientation.y;

  // Popup di conferma (solo se il display c'e'). Unica pausa bloccante
  // del firmware: accettabile perche' la taratura si fa sempre da fermi.
  if (oledOk) {
    display.fillRoundRect(15, 18, 98, 28, 4, SSD1306_BLACK);
    display.drawRoundRect(15, 18, 98, 28, 4, SSD1306_WHITE);
    display.setCursor(22, 28);
    display.print(F("CALIBRAZIONE OK"));
    display.display();
    delay(DURATA_POPUP);
  }

  Serial.println(F("Taratura zero eseguita."));
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
  // Intestazione: Minuto + colonne canali + meteo
  logFile.print(F("Minuto"));
  for (int c = 0; c < N_CANALI; c++) {
    logFile.print(',');
    logFile.print(NOME_CANALE[c]);
  }
  logFile.println(F(",Temp_C,Umid_%,Rischio_%"));
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
  aggiornaRecordStorici();  // PRIMA del riepilogo: cosi' gli asterischi
                            // "nuovo record" sono gia' pronti a schermo
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
  logFile.println(indiceRischio, 0);
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
  logFile.close();

  Serial.println(F("Riepilogo sessione scritto in memoria."));
}

// --- Record storici --------------------------------------------------------
//la struct viene scritta e riletta cosi' com'e' in RAM, senza parsing di testo e senza errori di formato.
// A fare da controllo c'e' la firma "magic", che contiene anche la
// versione del formato: se in futuro cambio la struct mi basta cambiare
// MAGIC_RECORD, i vecchi file non tornano piu' validi e si riparte da
// zero invece di leggere numeri sballati.

// Legge il file dei record all'avvio. Se non esiste (primissimo avvio)
// o non supera i controlli, si resta coi record a zero: nessun errore
// bloccante, il sistema parte lo stesso.
void caricaRecordStorici() {
  if (!memoriaOk) return;

  File f = LittleFS.open(FILE_RECORD, FILE_READ);
  if (!f) {
    Serial.println(F("Nessun file record: storico che parte da zero."));
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
  File f = LittleFS.open(FILE_RECORD, FILE_WRITE);
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
// 12. RIEPILOGO DI FINE SESSIONE (OLED)
// ===========================================================================

// Attende un tasto qualsiasi; true se premuto, false allo scadere del
// timeout. I primi 500 ms si ignorano, altrimenti il rimbalzo del tasto
// appena rilasciato chiuderebbe subito la pagina.
bool attesaTastoRiepilogo() {
  unsigned long ingresso = millis();
  while (millis() - ingresso < RIEPILOGO_TIMEOUT) {
    // Il telefono deve poter navigare anche durante il riepilogo
    if (wifiAttivo) server.handleClient();

    bool tasto = frontePressione(PIN_BTN_SU,  statoPrecSu);
    tasto |= frontePressione(PIN_BTN_GIU, statoPrecGiu);
    tasto |= frontePressione(PIN_BTN_OK,  statoPrecOk);
    tasto |= frontePressione(PIN_BTN_LOG, statoPrecLog);
    if (tasto && millis() - ingresso > 500) return true;
    delay(PAUSA_LOOP);
  }
  return false;
}

// Riepilogo in due pagine: massimi di sessione, poi contatore eventi.
// Un tasto passa alla pagina successiva (l'ultima esce), il timeout
// chiude tutto. L'attesa bloccante e' accettabile: la registrazione si
// ferma sempre da fermi.
void mostraRiepilogoSessione() {
  mostraPaginaMassimi();
  if (attesaTastoRiepilogo()) {
    mostraPaginaEventi();
    attesaTastoRiepilogo();
  }

  // Il tasto premuto per uscire non deve anche azionare il menu
  ultimoTempoPulsante = millis();
}

// Barra del titolo invertita (sfondo bianco, testo nero) usata dalle
// pagine di riepilogo
void barraTitolo(int x, const __FlashStringHelper *testo) {
  display.fillRect(0, 0, 128, 11, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(x, 2);
  display.print(testo);
  display.setTextColor(SSD1306_WHITE);
}

// Riga della griglia dei riepiloghi: nome della grandezza a sinistra e
// due valori con mini-etichetta su colonne FISSE (x=42 e x=86). Le
// colonne sono le stesse su ogni riga e su ogni pagina che la usa
// (riepilogo di sessione e pagina RECORD): la griglia si impara a
// leggere una volta sola. Le larghezze sono calcolate sul font 6x8:
// il caso peggiore ("Fr 0.9*" = 7 caratteri = 42 px a x=86) finisce
// esattamente al bordo dei 128 px senza tagli.
// recA/recB accendono l'asterisco "nuovo record"; dove non serve
// (pagina RECORD) si passa false.
void rigaDoppia(int y, const __FlashStringHelper *nome,
                const __FlashStringHelper *lblA, float valA, bool recA,
                const __FlashStringHelper *lblB, float valB, bool recB,
                int decimali) {
  display.setCursor(0, y);
  display.print(nome);

  display.setCursor(42, y);
  display.print(lblA);
  display.print(valA, decimali);
  if (recA) display.print('*');

  display.setCursor(86, y);
  display.print(lblB);
  display.print(valB, decimali);
  if (recB) display.print('*');
}

// Prima pagina del riepilogo: massimi di sessione sulla griglia comune,
// una riga per coppia di canali. Angoli a 0 decimali e G a 1: sull'OLED
// piu' cifre non si leggono al volo, la precisione piena resta nel CSV.
// L'asterisco marca i valori che hanno appena battuto il record storico;
// la legenda in basso compare solo se c'e' almeno un record, altrimenti
// lascia il posto al suggerimento [tasto].
void mostraPaginaMassimi() {
  unsigned long durataSec = (millis() - inizioRegistrazione) / 1000;

  display.clearDisplay();
  barraTitolo(4, F("MASSIMI SESSIONE 1/2"));

  rigaDoppia(14, F("Piega"),
             F("Dx "), massimiSessione[C_PIEGA_DX],  nuovoRecord[C_PIEGA_DX],
             F("Sx "), massimiSessione[C_PIEGA_SX],  nuovoRecord[C_PIEGA_SX], 0);
  rigaDoppia(24, F("Imp/St"),
             F("Im "), massimiSessione[C_IMPENNATA], nuovoRecord[C_IMPENNATA],
             F("St "), massimiSessione[C_STOPPIE],   nuovoRecord[C_STOPPIE], 0);
  rigaDoppia(34, F("G lat"),
             F("Dx "), massimiSessione[C_GLAT_DX],   nuovoRecord[C_GLAT_DX],
             F("Sx "), massimiSessione[C_GLAT_SX],   nuovoRecord[C_GLAT_SX], 1);
  rigaDoppia(44, F("G lon"),
             F("Ac "), massimiSessione[C_G_ACCEL],   nuovoRecord[C_G_ACCEL],
             F("Fr "), massimiSessione[C_G_FRENA],   nuovoRecord[C_G_FRENA], 1);

  // Piede pagina: durata reale della sessione (dal cronometro, non dai
  // minuti CSV: conta anche il minuto parziale scartato dal log)
  display.setCursor(0, 56);
  display.print(F("Durata "));
  display.print(durataSec / 60); display.print(F("m"));
  display.print(durataSec % 60); display.print(F("s"));

  bool almenoUnRecord = false;
  for (int c = 0; c < N_CANALI; c++) almenoUnRecord |= nuovoRecord[c];
  display.setCursor(80, 56);
  display.print(almenoUnRecord ? F("* nuovo!") : F(" [tasto]"));

  display.display();
}

// Seconda pagina del riepilogo: quante volte ogni soglia evento e' stata
// superata nella sessione appena chiusa. E' un ciclo sulla tabella eventi.
void mostraPaginaEventi() {
  display.clearDisplay();
  barraTitolo(4, F("CONTATORE EVENTI 2/2"));

  for (int e = 0; e < N_EVENTI; e++) {
    display.setCursor(0, 15 + e * 10);
    display.print(NOME_EVENTO_OLED[e]);
    display.print(conteggioEventi[e]);
  }

  display.display();
}

// ===========================================================================
// 13. WIFI E SITO WEB
// ===========================================================================

// Pagina live (HTML+CSS+JS): il telefono e' collegato
// all'access point della moto e NON ha internet, quindi niente librerie
// esterne.
const char PAGINA_LIVE_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>WheelStat - Telemetria live</title>
<style>
/* Tema scuro: si legge meglio col telefono sotto il sole */
body{font-family:sans-serif;background:#111;color:#eee;margin:12px}
h2{margin:0 0 8px}
#rec{color:#f55;font-size:16px}                 /* spia REC rossa */
.g{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin-bottom:10px}
.c{background:#1c1c1c;border-radius:8px;padding:8px;text-align:center}
.e{font-size:11px;color:#999}                   /* etichetta piccola */
.v{font-size:22px;font-weight:bold}             /* valore grande */
canvas{width:100%;background:#1c1c1c;border-radius:8px;margin-bottom:10px}
.l{font-size:12px;color:#999;margin-bottom:2px} /* legenda dei grafici */
#ev{font-size:13px;text-align:left}
a{color:#4cf}
</style></head><body>
<h2>WheelStat <span id="rec"></span></h2>

<!-- Griglia dei valori istantanei -->
<div class="g">
<div class="c"><div class="e">Piega &deg;</div><div class="v" id="vp">--</div></div>
<div class="c"><div class="e">Imp./Stoppie &deg;</div><div class="v" id="vw">--</div></div>
<div class="c"><div class="e">Rischio %</div><div class="v" id="vr">--</div></div>
<div class="c"><div class="e">G laterale</div><div class="v" id="vgl">--</div></div>
<div class="c"><div class="e">G longitudinale</div><div class="v" id="vgn">--</div></div>
<div class="c"><div class="e">Aria &deg;C / Umid %</div><div class="v" id="vm">--</div></div>
</div>

<!-- Grafici scorrevoli: ~36 secondi di storia (120 punti x 300 ms) -->
<div class="l">Angoli (&plusmn;60&deg;): <span style="color:#4cf">piega</span> /
<span style="color:#fc4">impennata(+) stoppie(-)</span></div>
<canvas id="ca" height="130"></canvas>
<div class="l">Forze G (&plusmn;1.5): <span style="color:#4cf">laterale</span> /
<span style="color:#fc4">longitudinale</span></div>
<canvas id="cg" height="130"></canvas>

<div class="c" id="ev">Eventi: --</div>
<p><a href="/">&larr; Scarica le sessioni dalla memoria</a></p>

<script>
// Serie storiche dei grafici: N campioni, i piu' vecchi escono a sinistra
var N=120;
var sPiega=[],sPitch=[],sLat=[],sLon=[];

// Disegna due serie (a, b) su un canvas: fondo scala +/-max,
// linea dello zero al centro
function disegna(id,a,b,max){
 var cv=document.getElementById(id);
 cv.width=cv.clientWidth;             // adatta la risoluzione alla larghezza reale
 var c=cv.getContext('2d'),W=cv.width,H=cv.height;
 c.clearRect(0,0,W,H);
 c.strokeStyle='#333';c.beginPath();c.moveTo(0,H/2);c.lineTo(W,H/2);c.stroke();
 function linea(s,col){
  c.strokeStyle=col;c.lineWidth=2;c.beginPath();
  for(var i=0;i<s.length;i++){
   var x=i*W/(N-1);
   var y=H/2-(s[i]/max)*(H/2-4);      // scala il valore sull'altezza
   y=Math.max(2,Math.min(H-2,y));     // niente linee fuori dal riquadro
   if(i==0)c.moveTo(x,y);else c.lineTo(x,y);
  }
  c.stroke();
 }
 linea(a,'#4cf');linea(b,'#fc4');
}

// Accoda un campione e scarta il piu' vecchio quando la serie e' piena
function punta(s,v){s.push(v);if(s.length>N)s.shift();}

// Interroga /dati, aggiorna i riquadri e ridisegna i grafici. In caso di
// errore (WiFi perso, moto spenta) non fa nulla e riprova al giro dopo.
async function giro(){
 try{
  var d=await (await fetch('/dati')).json();
  document.getElementById('vp').textContent=d.piega.toFixed(1);
  document.getElementById('vw').textContent=d.pitch.toFixed(1);
  document.getElementById('vr').textContent=d.rischio.toFixed(0);
  document.getElementById('vgl').textContent=d.glat.toFixed(2);
  document.getElementById('vgn').textContent=d.glon.toFixed(2);
  document.getElementById('vm').textContent=d.temp.toFixed(0)+' / '+d.umid.toFixed(0);
  document.getElementById('rec').textContent=d.rec?('● REC '+d.min+' min'):'';
  document.getElementById('ev').textContent='Eventi - Impennate: '+d.evi
   +' | Stoppie: '+d.evs+' | Pieghe: '+d.evp
   +' | Frenate brusche: '+d.evf+' | Accelerate brusche: '+d.eva;
  punta(sPiega,d.piega);punta(sPitch,d.pitch);punta(sLat,d.glat);punta(sLon,d.glon);
  disegna('ca',sPiega,sPitch,60);disegna('cg',sLat,sLon,1.5);
 }catch(e){}
}
setInterval(giro,300);  // circa 3 aggiornamenti al secondo
</script></body></html>
)rawliteral";

// Accende l'access point e il server web.
void avviaWiFi() {
  WiFi.mode(WIFI_AP);
  wifiAttivo = WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
  if (wifiAttivo) {
    server.begin();
    Serial.print(F("WiFi acceso -> http://"));
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println(F("ERRORE: avvio access point fallito."));
  }
}

// Spegne server e radio WiFi
void fermaWiFi() {
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  wifiAttivo = false;
  Serial.println(F("WiFi spento."));
}

// Radice del sito: elenco dei LOG_n.CSV in memoria, con download e
// cancellazione. La pagina viene costruita al volo in una String: coi log
// tipici (pochi KB di HTML)
void webElencoFile() {
  String html;
  html.reserve(2560);  // un blocco unico: meno riallocazioni e frammentazione dello heap
  html += F("<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>WheelStat</title><style>body{font-family:sans-serif;background:#111;"
    "color:#eee;margin:16px}a{color:#4cf}li{margin:8px 0}.x{color:#f55}"
    "td{padding:2px 14px 2px 0}"
    ".b{display:inline-block;padding:10px 16px;background:#4cf;color:#000;"
    "border-radius:8px;text-decoration:none;font-weight:bold}</style></head><body>"
    "<h2>WheelStat</h2><p><a class='b' href='/live'>Telemetria live</a></p>"
    "<h3>Sessioni in memoria</h3><ul>");

  // Scorro la radice della flash e tengo solo i file di log
  int trovati = 0;
  File radice = LittleFS.open("/");
  if (radice) {
    File f = radice.openNextFile();
    while (f) {
      String nome = f.name();  // con o senza "/" a seconda del core
      if (!f.isDirectory() && nome.indexOf("LOG_") >= 0) {
        html += "<li><a href='/scarica?f=";
        html += nome; html += "'>"; html += nome; html += "</a> (";
        html += String(f.size() / 1024.0, 1);
        html += " KB) &nbsp;<a class='x' href='/elimina?f=";
        html += nome;
        html += "' onclick=\"return confirm('Eliminare definitivamente?')\">"
                "elimina</a></li>";
        trovati++;
      }
      f = radice.openNextFile();
    }
    radice.close();
  }
  if (trovati == 0) html += F("<li>Nessuna sessione trovata.</li>");
  html += F("</ul>");

  // Record di sempre: e' la stessa tabella canali del firmware, In fondo i contatori "carriera" e il link di
  // azzeramento, protetto da confirm(): qui si cancella lo storico
  // intero, non un singolo file.
  html += F("<h3>Record di sempre</h3><table>");
  for (int c = 0; c < N_CANALI; c++) {
    html += F("<tr><td>");
    html += NOME_CANALE[c];
    html += F("</td><td><b>");
    html += String(recordStorici.canali[c], decimaliCanale(c));
    html += (c <= C_STOPPIE) ? F("&deg;") : F(" G");
    html += F("</b></td></tr>");
  }
  html += F("</table><p>");
  html += String(recordStorici.sessioni);
  html += F(" sessioni registrate, ");
  html += String(recordStorici.minutiTotali);
  html += F(" minuti totali. &nbsp;<a class='x' href='/azzera_record' "
            "onclick=\"return confirm('Azzerare tutti i record storici?')\">"
            "azzera record</a></p>");

  // Contatore di riempimento: la flash non si estrae, meglio vederlo qui
  html += F("<p>Memoria: ");
  html += String(LittleFS.usedBytes() / 1024);
  html += F(" KB usati su ");
  html += String(LittleFS.totalBytes() / 1024);
  html += F(" KB.</p>");

  if (inRegistrazione)
    html += F("<p>REC in corso: download e cancellazione sono disponibili "
              "a registrazione ferma.</p>");
  html += F("</body></html>");

  server.send(200, "text/html", html);
}

// La pagina live e' statica e sta in flash: send_P la spedisce da li'
void webLive() {
  server.send_P(200, "text/html", PAGINA_LIVE_HTML);
}

// Telemetria istantanea in JSON, interrogata dalla pagina live ogni 300 ms
void webDati() {
  char json[280];
  snprintf(json, sizeof(json),
    "{\"piega\":%.1f,\"pitch\":%.1f,\"glat\":%.2f,\"glon\":%.2f,"
    "\"temp\":%.1f,\"umid\":%.0f,\"rischio\":%.0f,\"rec\":%d,\"min\":%lu,"
    "\"evi\":%u,\"evs\":%u,\"evp\":%u,\"evf\":%u,\"eva\":%u}",
    piegaLive, pitchLive, forzaGLaterale, forzaGLongitudinale,
    temperatura, umidita, indiceRischio, inRegistrazione ? 1 : 0,
    minutiRegistrati,
    conteggioEventi[EV_IMPENNATA], conteggioEventi[EV_STOPPIE],
    conteggioEventi[EV_PIEGA], conteggioEventi[EV_FRENATA],
    conteggioEventi[EV_ACCEL]);
  server.send(200, "application/json", json);
}

// Download di un CSV. Bloccato durante la registrazione: lo streaming di
// un file grosso fermerebbe il loop (e quindi la telemetria) per secondi.
void webScarica() {
  if (inRegistrazione) {
    server.send(503, "text/plain", "Ferma la registrazione prima di scaricare i file.");
    return;
  }

  String nome = server.arg("f");
  if (!nome.startsWith("/")) nome = "/" + nome;
  // Solo i file di log, niente giri strani nel filesystem
  if (!nome.startsWith("/LOG_") || nome.indexOf("..") >= 0 || !LittleFS.exists(nome)) {
    server.send(404, "text/plain", "File non trovato.");
    return;
  }

  File f = LittleFS.open(nome, FILE_READ);
  if (!f) {
    server.send(500, "text/plain", "Errore di lettura dalla memoria.");
    return;
  }
  // Content-Disposition: il browser scarica invece di mostrare il testo
  server.sendHeader("Content-Disposition", "attachment; filename=" + nome.substring(1));
  server.streamFile(f, "text/csv");
  f.close();
}

// Cancellazione di un CSV
void webElimina() {
  if (inRegistrazione) {
    server.send(503, "text/plain", "Ferma la registrazione prima di eliminare i file.");
    return;
  }

  String nome = server.arg("f");
  if (!nome.startsWith("/")) nome = "/" + nome;
  if (!nome.startsWith("/LOG_") || nome.indexOf("..") >= 0 || !LittleFS.exists(nome)) {
    server.send(404, "text/plain", "File non trovato.");
    return;
  }

  bool ok = LittleFS.remove(nome);
  Serial.print(ok ? F("Eliminato ") : F("ERRORE eliminando "));
  Serial.println(nome);

  // Redirect all'elenco: la pagina si ricarica gia' aggiornata
  server.sendHeader("Location", "/");
  server.send(303);
}

// Azzeramento dei record storici. 
void webAzzeraRecord() {
  recordStorici = RecordStorici{};  // tutto a zero (la firma la rimette il salvataggio)
  for (int c = 0; c < N_CANALI; c++) nuovoRecord[c] = false;
  salvaRecordStorici();
  Serial.println(F("Record storici azzerati dal sito web."));

  server.sendHeader("Location", "/");
  server.send(303);
}

// ===========================================================================
// 14. PAGINE OLED
// ===========================================================================

// Ridisegna da zero la pagina corrente, 10 volte al secondo: barra di
// stato comune in alto
void aggiornaDisplay() {
  if (!oledOk) return;  // niente pannello: inutile disegnare e occupare l'I2C

  display.clearDisplay();

  // Barra di stato: titolo a sinistra, REC/IDLE a destra
  display.fillRect(0, 0, 128, 11, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_BLACK);
  display.setCursor(2, 2);
  display.print(F("WHEELSTAT"));
  display.setCursor(94, 2);
  if (inRegistrazione) {
    // L'asterisco lampeggia: si vede che registra con la coda dell'occhio
    if (lampeggioRec) display.print(F("* REC"));
    else display.print(F("  REC"));
  } else {
    display.print(F("IDLE "));
  }
  display.setTextColor(SSD1306_WHITE);

  switch (schermataCorrente) {
    case 0: disegnaPiega();            break;
    case 1: disegnaMeteo();            break;
    case 2: disegnaForzaG();           break;
    case 3: disegnaImpennataStoppie(); break;
    case 4: disegnaRecord();           break;
    case 5: disegnaMemoria();          break;
    case 6: disegnaWiFi();             break;
  }

  display.display();  // spedisce il frame completo al pannello via I2C
}

// --- Elementi comuni a tutte le pagine live -------------------------------
// Il linguaggio grafico e' unico: titolo centrato a y=15, riga di
// separazione a y=24, dati sotto. Le etichette stanno a x=6 e i valori
// incolonnati a x=54, cosi' ogni pagina si legge allo stesso modo.

// Titolo di pagina centrato + riga di separazione.
// La x la sceglie il chiamante: (128 - 6 * caratteri) / 2.
void titoloPagina(int x, const __FlashStringHelper *testo) {
  display.setTextSize(1);
  display.setCursor(x, 15);
  display.print(testo);
  display.drawFastHLine(10, 24, 108, SSD1306_WHITE);
}

// Inizia una riga "etichetta / valore": stampa l'etichetta e lascia il
// cursore sulla colonna dei valori, il chiamante stampa il resto.
void rigaDato(int y, const __FlashStringHelper *etichetta) {
  display.setCursor(6, y);
  display.print(etichetta);
  display.setCursor(54, y);
}

// Numero grande centrato col simbolo dei gradi (pagine 0 e 3).
// Centratura: una cifra sta piu' al centro di due.
void numeroGrande(int valore) {
  display.setTextSize(3);
  if (valore < 10) display.setCursor(45, 30);
  else display.setCursor(35, 30);
  display.print(valore);
  display.setTextSize(1);
  display.setCursor(75, 30);
  display.print(F("o"));  // simbolo gradi artigianale
}

// ---- Pagina 0: angolo di piega -------------------------------------------
void disegnaPiega() {
  // Piu' rischio grip = meno piega concessa prima dell'alert
  float maxPiegaSicura = PIEGA_MAX_TEORICA - (indiceRischio * RIDUZIONE_PIEGA_RISCHIO);

  if (angoloPiega > maxPiegaSicura) {
    // Banner lampeggiante di pericolo al posto del titolo (ogni 250 ms)
    if ((millis() / 250) % 2 == 0) {
      display.fillRoundRect(8, 13, 112, 13, 3, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.setCursor(14, 16);
      display.print(F("! PERICOLO GRIP !"));
      display.setTextColor(SSD1306_WHITE);
    }
  } else {
    titoloPagina(19, F("ANGOLO DI PIEGA"));
  }

  numeroGrande(constrain((int)angoloPiega, 0, 99));

  // Barra orizzontale che si riempie dal centro verso l'esterno
  display.drawRoundRect(4, 56, 120, 6, 2, SSD1306_WHITE);
  display.drawFastVLine(64, 54, 10, SSD1306_WHITE);  // tacca di zero

  int offsetBarra = map((int)angoloPiega, 0, 60, 0, 58);
  offsetBarra = constrain(offsetBarra, 0, 56);
  display.fillRect(64 - offsetBarra, 58, offsetBarra * 2, 2, SSD1306_WHITE);
}

// ---- Pagina 1: meteo e rischio grip ---------------------------------------
void disegnaMeteo() {
  titoloPagina(28, F("METEO E GRIP"));

  rigaDato(30, F("Aria"));
  display.print(temperatura, 1);
  display.print(F(" C"));

  rigaDato(41, F("Umidita"));
  display.print((int)umidita);
  display.print(F(" %"));

  rigaDato(52, F("Rischio"));
  display.print((int)indiceRischio);
  display.print(F(" %"));
}

// ---- Pagina 2: G-meter con radar 2D ---------------------------------------
void disegnaForzaG() {
  titoloPagina(43, F("FORZA G"));

  // Numeri a sinistra (Lat in assoluto, Long con segno)...
  display.setCursor(6, 34);
  display.print(F("Lat :"));
  display.print(fabsf(forzaGLaterale), 1);

  display.setCursor(6, 48);
  display.print(F("Long:"));
  display.print(forzaGLongitudinale, 1);

  // ...e radar a destra: cerchio con mirino, come i G-meter da auto
  int cx = 95;
  int cy = 45;
  int r = 16;

  display.drawCircle(cx, cy, r, SSD1306_WHITE);
  display.drawFastHLine(cx - 18, cy, 37, SSD1306_WHITE);
  display.drawFastVLine(cx, cy - 18, 37, SSD1306_WHITE);

  // Pallino che si sposta col vettore G, confinato dentro al radar.
  // 14 pixel = 1 G circa: su strada e' raro uscire dal cerchio.
  int dotX = cx + (int)(forzaGLaterale * 14);
  int dotY = cy - (int)(forzaGLongitudinale * 14);
  dotX = constrain(dotX, cx - 14, cx + 14);
  dotY = constrain(dotY, cy - 14, cy + 14);

  display.fillCircle(dotX, dotY, 3, SSD1306_WHITE);
}

// ---- Pagina 3: impennata e stoppie ----------------------------------------
void disegnaImpennataStoppie() {
  // Stessa pagina per le due manovre: etichetta e numero grande seguono
  // quella in corso (muso su = impennata, muso giu' = stoppie)
  bool inStoppie = angoloStoppie > angoloImpennata;
  float angoloAttivo = inStoppie ? angoloStoppie : angoloImpennata;

  if (inStoppie) titoloPagina(43, F("STOPPIE"));
  else titoloPagina(37, F("IMPENNATA"));

  int valAngolo = constrain((int)angoloAttivo, 0, 99);
  numeroGrande(valAngolo);

  // Colonna laterale con lo zero al centro: si riempie verso l'alto in
  // impennata e verso il basso in stoppie
  display.drawRoundRect(110, 28, 10, 34, 2, SSD1306_WHITE);
  display.drawFastHLine(108, 45, 14, SSD1306_WHITE);  // tacca di zero

  int altezzaBarra = map(valAngolo, 0, 45, 0, 15);
  altezzaBarra = constrain(altezzaBarra, 0, 15);
  if (inStoppie) display.fillRect(112, 46, 6, altezzaBarra, SSD1306_WHITE);
  else display.fillRect(112, 45 - altezzaBarra, 6, altezzaBarra, SSD1306_WHITE);
}

// ---- Pagina 4: record storici -----------------------------------------------
// I migliori di sempre
void disegnaRecord() {
  titoloPagina(46, F("RECORD"));

  rigaDoppia(27, F("Piega"),
             F("Dx "), recordStorici.canali[C_PIEGA_DX],  false,
             F("Sx "), recordStorici.canali[C_PIEGA_SX],  false, 0);
  rigaDoppia(36, F("Imp/St"),
             F("Im "), recordStorici.canali[C_IMPENNATA], false,
             F("St "), recordStorici.canali[C_STOPPIE],   false, 0);
  rigaDoppia(45, F("G lat"),
             F("Dx "), recordStorici.canali[C_GLAT_DX],   false,
             F("Sx "), recordStorici.canali[C_GLAT_SX],   false, 1);
  rigaDoppia(54, F("G lon"),
             F("Ac "), recordStorici.canali[C_G_ACCEL],   false,
             F("Fr "), recordStorici.canali[C_G_FRENA],   false, 1);
}

// ---- Pagina 5: diagnostica memoria flash ------------------------------------
void disegnaMemoria() {
  titoloPagina(43, F("MEMORIA"));

  // Spazio libero sulla partizione LittleFS, il dato che conta davvero
  rigaDato(30, F("Flash"));
  if (memoriaOk) {
    display.print((LittleFS.totalBytes() - LittleFS.usedBytes()) / 1024);
    display.print(F(" KB lib."));
  } else {
    display.print(F("ERRORE!"));
  }

  if (inRegistrazione) {
    rigaDato(41, F("File"));
    display.print(nomeFileLog + 1);  // +1 salta la "/" iniziale

    rigaDato(52, F("Minuti"));
    display.print(minutiRegistrati);
  } else {
    rigaDato(41, F("Stato"));
    display.print(F("IN PAUSA"));

    rigaDato(52, F("Avvio"));
    display.print(F("tasto [LOG]"));
  }
}

// ---- Pagina 6: WiFi e telemetria dal telefono -------------------------------
void disegnaWiFi() {
  titoloPagina(52, F("WIFI"));

  rigaDato(29, F("Rete"));
  display.print(WIFI_SSID);

  if (wifiAttivo) {
    // WiFi acceso: tutto quello che serve per collegarsi (righe piu'
    // fitte del solito: qui i dati sono quattro)
    rigaDato(38, F("Pass"));
    display.print(WIFI_PASSWORD);

    rigaDato(47, F("Sito"));
    display.print(WiFi.softAPIP());

    rigaDato(56, F("Client"));
    display.print(WiFi.softAPgetStationNum());
    display.print(F("  [OK] off"));
  } else {
    rigaDato(41, F("Stato"));
    display.print(F("SPENTO"));

    rigaDato(52, F("Avvio"));
    display.print(F("tasto [OK]"));
  }
}

// ===========================================================================
// 15. DEBUG SU SERIAL MONITOR
// ===========================================================================

// Telemetria ogni 5 secondi, senza di questa è impossibile fare debug
void stampaSeriale() {
  Serial.println(F("---- TELEMETRIA LIVE ----"));
  Serial.print(F("Piega    : ")); Serial.print(angoloPiega);     Serial.println(F(" deg"));
  Serial.print(F("Impennata: ")); Serial.print(angoloImpennata);
  Serial.print(F(" deg | Stoppie: ")); Serial.print(angoloStoppie); Serial.println(F(" deg"));
  Serial.print(F("G_Lat    : ")); Serial.print(forzaGLaterale);
  Serial.print(F(" | G_Lon: "));  Serial.println(forzaGLongitudinale);
  Serial.print(F("Meteo    : ")); Serial.print(temperatura);
  Serial.print(F(" C / "));       Serial.print(umidita); Serial.println(F(" %"));

  // Contatori eventi, tutti su una riga
  Serial.print(F("Eventi   : "));
  for (int e = 0; e < N_EVENTI; e++) {
    Serial.print(NOME_EVENTO_CSV[e]);
    Serial.print('=');
    Serial.print(conteggioEventi[e]);
    Serial.print(' ');
  }
  Serial.println();

  if (bnoOk) {
    uint8_t calSys, calGyro, calAcc, calMag;
    bno.getCalibration(&calSys, &calGyro, &calAcc, &calMag);
    Serial.print(F("Calibraz.: SYS=")); Serial.print(calSys);
    Serial.print(F(" GYRO="));          Serial.print(calGyro);
    Serial.print(F(" ACC="));           Serial.print(calAcc);
    Serial.print(F(" MAG="));           Serial.print(calMag);
    Serial.println(F("  (0=no, 3=ok)"));
  } else {
    Serial.println(F("Calibraz.: IMU ASSENTE"));
  }
  Serial.println(F("-------------------------"));
}
