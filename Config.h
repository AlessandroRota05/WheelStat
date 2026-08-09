/*
  WheelStat - Config.h
  ===========================================================================
  Due cose: QUALI componenti sono montati (le macro di scelta driver) e
  COM'E' fatto questo esemplare (piedinatura, indirizzi, orientamento
  dell'IMU, credenziali WiFi).

  Le due sezioni da guardare per prime su una scatola nuova sono la
  piedinatura - che cambia da sola in base a quale ESP32 e' selezionato
  nell'IDE - e l'ORIENTAMENTO DELL'IMU, l'unica cosa qui dentro che non si
  indovina a tavolino e che va provata con la scatola in mano.

  Regola per decidere se una costante va qui: "cambia se monto componenti
  diversi, o gli stessi montati in modo diverso?"
    si' -> qui          (PIN_DHT, I2C_ADDR_OLED, SEGNO_PIEGA)
    no  -> WheelStat.ino sezione 3, che e' la taratura del comportamento
           (SOGLIA_UMIDITA_RISCHIO, MAX_GIRI, INTERVALLO_LOG)

  Chi lo include: WheelStat.ino (e da li' la concatenazione lo rende
  visibile agli altri .ino) e ogni driver in src/driver/, che essendo un
  header non riceve niente dalla concatenazione e senza questo include
  non conoscerebbe ne' le macro ne' i pin.

  Deve essere un .h e non un .ino: l'IDE genera i prototipi delle
  funzioni prima di conoscere i tipi definiti nei .ino, quindi typedef,
  macro ed extern sono garantiti visibili solo da un header incluso
  esplicitamente.

  Il meccanismo per intero: docs/DRIVER.md.
  ===========================================================================
*/

#ifndef CONFIG_H
#define CONFIG_H

// ===========================================================================
// SCELTA DEI DRIVER
// ===========================================================================
// Una macro accesa per categoria. Il driver non scelto viene cancellato
// dal preprocessore e non produce un byte di codice.
//
// I blocchi #if !defined(...) servono a lasciare l'ultima parola alla riga
// di comando: se la macro arriva da fuori (-DMETEO_SHT31, per la matrice
// di compilazione in docs/DRIVER.md) qui non se ne definisce nessuna.
// Senza, un -D esterno accenderebbe DUE driver insieme.

#if !defined(METEO_DHT22)  && !defined(METEO_SHT4X)  && !defined(METEO_SHT31) && \
    !defined(METEO_HTU31D) && !defined(METEO_SI7021) && !defined(METEO_HTU21D) && \
    !defined(METEO_AHT20)  && !defined(METEO_BME280)
  #define METEO_DHT22     // montato
// #define METEO_SHT4X    // SHT40/41/45, il piu' preciso: la scelta se si compra oggi
// #define METEO_SHT31    // generazione precedente dello SHT4x
// #define METEO_HTU31D   // buon compromesso prezzo/precisione
// #define METEO_SI7021   // classico affidabile
// #define METEO_HTU21D   // predecessore dell'HTU31D, da cassetto
// #define METEO_AHT20    // il piu' economico, precisione modesta
// #define METEO_BME280   // ha anche la pressione, che il firmware ignora
#endif

#if !defined(DISPLAY_SSD1306) && !defined(DISPLAY_SSD1309) && \
    !defined(DISPLAY_SH1106)  && !defined(DISPLAY_SH1107)
  #define DISPLAY_SSD1306  // montato
// #define DISPLAY_SSD1309 // controller diverso, stessa libreria dell'SSD1306
// #define DISPLAY_SH1106  // 128x64, layout identico
// #define DISPLAY_SH1107  // ATTENZIONE: quasi sempre 128x128, vedi src/driver/Display.h
#endif

// Una sola implementazione oggi. La macro c'e' lo stesso perche' il BNO055
// e' fuori produzione (docs/HARDWARE.md): e' il gancio a cui appendere il
// driver del successore.
#if !defined(IMU_BNO055)
  #define IMU_BNO055
#endif

// Le due generazioni u-blox non si coprono a vicenda: l'M10 ha buttato i
// comandi di configurazione vecchi. Sbagliare driver lascia il modulo a
// 1 Hz con GPS_FIX_TIMEOUT_MS tarato sul rate alto, e il fix lampeggia.
#if !defined(GPS_UBLOX) && !defined(GPS_UBLOX_M10) && !defined(GPS_NMEA_GENERICO)
  #define GPS_UBLOX          // montato: NEO-6M/8M, Beitian BN-220/BN-880
// #define GPS_UBLOX_M10     // MAX-M10S, MIA-M10, Beitian serie M10
// #define GPS_NMEA_GENERICO // qualunque modulo non u-blox: resta a 1 Hz
#endif

// ===========================================================================
// CONTRATTI DEI DRIVER
// ===========================================================================
// Le funzioni che ogni src/driver/*.h deve fornire, identiche per tutti i
// driver della sua categoria. Firma sbagliata = non compila, ed e' il
// controllo che si vuole.
//
//   METEO    bool meteoInit()
//            bool meteoLeggi(float &temp, float &umid)
//            const __FlashStringHelper *meteoNome()
//
//   DISPLAY  DisplayDriver display          (l'oggetto, vedi extern sotto)
//            bool displayInit()
//            const __FlashStringHelper *displayNome()
//
//   IMU      bool imuInit()
//            bool imuLeggi(float &piega, float &impennata,
//                          float &accelLong, float &accelLat)
//            uint8_t imuCalibrazione()       0..3, 3 = pronto
//            void imuStampaCalibrazione()    una riga sul monitor seriale
//            const __FlashStringHelper *imuNome()
//
//   GPS      void gpsConfigura()             puo' essere vuota
//            const __FlashStringHelper *gpsNome()
//
// I quattro xNome() servono alla schermata di avvio, che elenca i
// componenti mentre li inizializza: il nome lo dice il driver e non una
// tabella altrove, cosi' non puo' restare indietro. Massimo ~11 caratteri.
//
// TRE COSE NON OVVIE, da rispettare scrivendo un driver nuovo:
//
// 1. meteoLeggi() riceve i parametri GIA' PIENI con gli ultimi valori
//    buoni, e sovrascrive solo cio' che ha letto davvero. Serve perche'
//    un sensore puo' consegnare una grandezza valida e l'altra NaN, e il
//    firmware le ha sempre tenute indipendenti.
//
// 2. imuLeggi() consegna quattro grandezze riferite alla MOTO, non al
//    chip:
//      piega      gradi di inclinazione laterale, positivi a DESTRA
//      impennata  gradi di beccheggio, positivi col MUSO SU
//      accelLong  m/s^2, positiva in ACCELERAZIONE, senza gravita'
//      accelLat   m/s^2, positiva a DESTRA
//    Tradurre dagli assi del sensore a questi e' il lavoro del driver, ed
//    e' la parte che non si indovina a tavolino: sul BNO055 e' costata una
//    versione intera (la v6.3 aveva piega e impennata scambiate). I
//    SEGNO_* piu' sotto invertono un verso, ma scambiare due assi non e'
//    un'inversione di segno e va risolto dentro al driver.
//
// 3. Il DISEGNO non e' nel contratto del display, e la LETTURA non e' in
//    quello del GPS: il primo e' Adafruit_GFX, uguale su ogni pannello
//    monocromatico; la seconda e' NMEA, che parlano tutti i moduli. Solo
//    l'accensione e' specifica del chip - ed e' il motivo per cui i file
//    driver sono corti.
//
//    Un'aggiunta implicita al primo punto: DisplayDriver deve esporre
//    getBuffer(), il puntatore al frame in RAM. Lo usa la transizione fra
//    pagine (transizionePagina in PagineOled.ino) per far scorrere due
//    fotogrammi uno dentro l'altro con delle memcpy invece di ridisegnarli
//    pixel per pixel. Non e' una richiesta esotica: ce l'hanno sia
//    Adafruit_SSD1306 sia Adafruit_GrayOLED, cioe' tutti e quattro i
//    pannelli supportati.

// ===========================================================================
// PIN - dipendono da QUALE ESP32
// ===========================================================================
// Il firmware gira su tutta la famiglia ESP32, i numeri GPIO no: sull'S3 e
// sul C3 i pin 22, 25 e 27 del DevKit V1 non esistono, e il C3 ha due UART
// invece di tre. Quindi qui sotto c'e' una piedinatura per famiglia, e il
// preprocessore sceglie quella giusta guardando la board selezionata
// nell'IDE. Le macro CONFIG_IDF_TARGET_* le definisce il core Espressif,
// non c'e' niente da impostare a mano.
//
// PER CAMBIARE PIEDINATURA basta modificare i numeri del proprio blocco.
// Per una board non prevista, si copia il blocco piu' vicino.
//
// Vincoli comuni a tutte le varianti, da rispettare scegliendo pin diversi:
//   - mai un PULSANTE su un pin di STRAPPING (decidono la modalita' di boot):
//     verrebbe letto premuto all'accensione e la scheda non partirebbe;
//   - mai niente sui pin della flash SPI interna;
//   - sui moduli con USB nativo (S2/S3/C3) lasciar stare i due pin del
//     connettore.
// L'I2C fa eccezione al primo punto e puo' stare su pin di strapping: le
// resistenze di pull-up dei breakout tengono le due linee ALTE a riposo, che
// e' proprio lo stato di boot normale. E' il caso di SDA/SCL sul C3.

#if defined(CONFIG_IDF_TARGET_ESP32S3)
  // ESP32-S3 (DevKitC-1). Strapping: 0, 3, 45, 46. USB: 19, 20.
  // Su alcuni moduli i GPIO 33-37 servono alla PSRAM octal: qui non si usano.
  const uint8_t PIN_BTN_SU  = 5;
  const uint8_t PIN_BTN_GIU = 6;
  const uint8_t PIN_BTN_OK  = 7;
  const uint8_t PIN_BTN_LOG = 15;
  const uint8_t PIN_DHT     = 4;
  const uint8_t PIN_I2C_SDA = 8;   // default I2C del core su S3
  const uint8_t PIN_I2C_SCL = 9;
  const uint8_t PIN_GPS_RX  = 18;
  const uint8_t PIN_GPS_TX  = 17;
  const uint8_t GPS_UART_NUM = 2;  // l'S3 ha tre UART come l'ESP32 classico

#elif defined(CONFIG_IDF_TARGET_ESP32C3)
  // ESP32-C3. Ha solo i GPIO 0-21, e fra flash (12-17), USB (18, 19) e la
  // UART0 del monitor (20, 21) ne restano pochi: e' il motivo dei numeri
  // sparsi. Strapping: 2, 8, 9.
  const uint8_t PIN_BTN_SU  = 3;
  const uint8_t PIN_BTN_GIU = 4;
  const uint8_t PIN_BTN_OK  = 5;
  const uint8_t PIN_BTN_LOG = 6;
  const uint8_t PIN_DHT     = 10;
  // 8 e 9 sono di strapping, ma per l'I2C va bene (vedi la nota sopra) e
  // sono il default del core su questo chip.
  const uint8_t PIN_I2C_SDA = 8;
  const uint8_t PIN_I2C_SCL = 9;
  const uint8_t PIN_GPS_RX  = 0;
  const uint8_t PIN_GPS_TX  = 1;
  // Il C3 ha SOLO DUE UART, e la 0 e' quella del monitor seriale: al GPS
  // resta la 1. Mettere 2 qui non darebbe un errore di compilazione, darebbe
  // una seriale che non riceve niente.
  const uint8_t GPS_UART_NUM = 1;

#elif defined(CONFIG_IDF_TARGET_ESP32S2)
  // ESP32-S2. Strapping: 0, 45, 46. USB: 19, 20. Flash interna: 26-32.
  const uint8_t PIN_BTN_SU  = 5;
  const uint8_t PIN_BTN_GIU = 6;
  const uint8_t PIN_BTN_OK  = 7;
  const uint8_t PIN_BTN_LOG = 15;
  const uint8_t PIN_DHT     = 4;
  const uint8_t PIN_I2C_SDA = 8;
  const uint8_t PIN_I2C_SCL = 9;
  const uint8_t PIN_GPS_RX  = 18;
  const uint8_t PIN_GPS_TX  = 17;
  const uint8_t GPS_UART_NUM = 1;  // anche l'S2 ne ha due

#elif defined(CONFIG_IDF_TARGET_ESP32)
  // ESP32 classico (DevKit V1, 30 o 38 pin). E' la scheda montata e
  // l'unica provata su hardware. Strapping: 0, 2, 12, 15.
  // ATTENZIONE: i GPIO 34-39 sono SOLO INPUT e senza pull-up interna, quindi
  // non vanno bene per i pulsanti come sono cablati qui.
  const uint8_t PIN_BTN_SU  = 13;
  const uint8_t PIN_BTN_GIU = 25;
  const uint8_t PIN_BTN_OK  = 14;
  const uint8_t PIN_BTN_LOG = 27;
  const uint8_t PIN_DHT     = 4;
  const uint8_t PIN_I2C_SDA = 21;
  const uint8_t PIN_I2C_SCL = 22;
  const uint8_t PIN_GPS_RX  = 16;
  const uint8_t PIN_GPS_TX  = 17;
  const uint8_t GPS_UART_NUM = 2;

#else
  // Niente ripiego silenzioso sulla piedinatura del classico: su un chip
  // non previsto (C6, H2, P4...) darebbe pin che in parte non esistono,
  // cioe' un firmware che parte e legge sensori mai collegati. Meglio non
  // compilare e chiedere di aggiungere il blocco.
  #error "Scheda non prevista. Serve un ESP32 classico, S2, S3 o C3 - oppure \
aggiungi il suo blocco di pin qui sopra copiando il piu' simile. Fuori dalla \
famiglia Espressif il vincolo non e' la CPU ma WiFi.h + WebServer.h + LittleFS \
nativi: vedi docs/HARDWARE.md."
#endif

// Pulsanti attivi bassi (premuto = LOW), pull-up interna: niente resistenze
// esterne, un capo al pin e l'altro a GND.
//
// PIN_GPS_RX e' il pin su cui l'ESP32 RICEVE, quindi ci va il TX del modulo
// (e viceversa). PIN_GPS_TX serve a gpsConfigura() per i comandi UBX; con
// GPS_NMEA_GENERICO resta inutilizzato.

// ===========================================================================
// BUS I2C
// ===========================================================================

// ATTENZIONE ALLE COLLISIONI: OLED e BNO055 condividono lo stesso bus
// (PIN_I2C_SDA / PIN_I2C_SCL qui sopra) e ci si aggiunge anche il sensore
// meteo se si sceglie un modello I2C. Prima di scegliere un componente nuovo,
// controllare che il suo indirizzo non sia gia' occupato qui sotto. I
// sensori meteo I2C stanno di solito su 0x44/0x45 (SHT3x) o 0x76/0x77
// (BME280), quindi non collidono - ma va verificato scheda per scheda,
// i breakout economici non sempre rispettano il datasheet.
const uint8_t  I2C_ADDR_OLED = 0x3C;
const uint8_t  I2C_ADDR_BNO  = 0x28;  // 0x29 su alcuni breakout (ponticello ADR)

// Indirizzi dei sensori meteo I2C. Ci sono solo quelli che la libreria
// chiede: SHT4x, Si7021 e HTU21D hanno l'indirizzo fisso nel chip e non
// lo prendono nemmeno come parametro.
#ifdef METEO_SHT31
const uint8_t  I2C_ADDR_SHT31  = 0x44;  // 0x45 col pin ADDR a VDD
#endif
#ifdef METEO_HTU31D
const uint8_t  I2C_ADDR_HTU31D = 0x40;  // 0x41 col pin ADDR a VDD
#endif
#ifdef METEO_AHT20
const uint8_t  I2C_ADDR_AHT20  = 0x38;  // fisso su tutti i moduli
#endif
#ifdef METEO_BME280
const uint8_t  I2C_ADDR_BME280 = 0x76;  // 0x77 col pin SDO a VDD
#endif

const uint32_t I2C_CLOCK_HZ  = 400000;  // alzato per diminuire imput lag

// ===========================================================================
// TEMPORIZZAZIONI DEI SENSORI
// ===========================================================================
// Non sono scelte di prodotto ma limiti fisici del componente montato:
// per questo stanno qui e non fra le temporizzazioni della sezione 3 di
// WheelStat.ino.
//
// PERCHE' NON DENTRO AL FILE DEL DRIVER, dove concettualmente starebbero
// meglio: loop() usa INTERVALLO_METEO per decidere quando chiamare
// leggiMeteo(), e loop() sta in WheelStat.ino. Arduino concatena il file
// principale PER PRIMO e gli altri .ino dopo, in ordine alfabetico: le
// funzioni sono salve (l'IDE genera i prototipi automatici) ma le
// variabili globali no, quindi una costante definita in src/driver/Meteo.h
// non sarebbe ancora visibile in loop(). Config.h invece e' incluso dal
// preprocessore prima di tutto, quindi funziona sempre.

#if defined(METEO_DHT22)
  // Il DHT22 campiona internamente ogni 2 s: interrogarlo piu' spesso
  // restituisce la stessa lettura di prima, non una nuova. E' l'unico
  // sensore della lista con questo limite, ed e' anche il motivo per cui
  // la costante e' finita qui invece che fra le temporizzazioni di
  // prodotto in WheelStat.ino.
  const unsigned long INTERVALLO_METEO = 2000;

#elif defined(METEO_SHT4X)  || defined(METEO_SHT31)  || defined(METEO_HTU31D) || \
      defined(METEO_SI7021) || defined(METEO_HTU21D) || defined(METEO_AHT20)  || \
      defined(METEO_BME280)
  // Tutti i sensori I2C della lista reggono 1 Hz abbondantemente (i piu'
  // lenti, AHT20 e BME280, impiegano meno di 100 ms per conversione).
  // Un secondo e' comunque piu' che sufficiente: temperatura e umidita'
  // dell'aria non cambiano in decimi di secondo, e il rischio grip che
  // ne deriva e' un indice lento per natura. Andare piu' veloci
  // occuperebbe il bus I2C condiviso con OLED e IMU senza guadagnare
  // niente.
  const unsigned long INTERVALLO_METEO = 1000;

#else
  #error "Config.h: nessun driver meteo selezionato (vedi la sezione SCELTA DEI DRIVER)"
#endif

// ===========================================================================
// DISPLAY
// ===========================================================================

// Dimensioni del pannello in pixel. Oggi il layout delle pagine e'
// disegnato a mano su questi 128x64 (coordinate fisse in PagineOled.ino):
// cambiarli qui NON riadatta le pagine da solo. Servono per la
// costruzione dell'oggetto display e come riferimento unico per i
// calcoli che sono gia' stati parametrizzati.
//
// Nota: SSD1306 e SH1106 sono ENTRAMBI 128x64, quindi passare dall'uno
// all'altro non tocca questi valori ne' una riga di layout. Servirebbero
// solo per un pannello di risoluzione diversa (128x32), che e' un lavoro
// molto piu' grosso e di natura diversa: vedi docs/HARDWARE.md, sezione
// "Display OLED".
const uint8_t SCREEN_WIDTH  = 128;
const uint8_t SCREEN_HEIGHT = 64;

// Larghezza di un carattere del font integrato di Adafruit_GFX a
// dimensione 1: il glifo e' 5x7 pixel piu' una colonna di spaziatura.
// Serve a centrare il testo senza calcolarlo a mano (vedi centraTesto()
// in PagineOled.ino). E' una proprieta' del FONT, non del pannello: cambia
// solo se un giorno si passa a un font personalizzato.
const uint8_t LARGHEZZA_CARATTERE = 6;

// L'include e il tipo concreto del pannello: e' l'unico punto del
// firmware dove compare il nome del chip. Il typedef sta qui e non nel
// file del driver perche' l'oggetto "display" viene usato da tre file
// diversi, e per dichiararlo extern serve conoscerne il tipo.
#if defined(DISPLAY_SSD1306) || defined(DISPLAY_SSD1309)
  #include <Adafruit_SSD1306.h>
  typedef Adafruit_SSD1306 DisplayDriver;
  const uint16_t COLORE_ON  = SSD1306_WHITE;
  const uint16_t COLORE_OFF = SSD1306_BLACK;
#elif defined(DISPLAY_SH1106) || defined(DISPLAY_SH1107)
  // Una libreria sola per due controller, ma due classi distinte:
  // cambiano gli indirizzi interni di memoria del pannello.
  #include <Adafruit_SH110X.h>
  #if defined(DISPLAY_SH1106)
    typedef Adafruit_SH1106G DisplayDriver;
  #else
    typedef Adafruit_SH1107 DisplayDriver;
  #endif
  const uint16_t COLORE_ON  = SH110X_WHITE;
  const uint16_t COLORE_OFF = SH110X_BLACK;
#else
  #error "Config.h: nessun driver display selezionato (vedi la sezione SCELTA DEI DRIVER)"
#endif

// L'oggetto e' DEFINITO nel file del driver selezionato e solo
// DICHIARATO qui. Non e' pedanteria: Arduino concatena il file
// principale per primo e gli altri .ino dopo, in ordine alfabetico.
// Le funzioni sono salve (l'IDE genera i prototipi automatici) ma le
// variabili globali no, quindi senza questo extern le 47 chiamate
// display.* che stanno in WheelStat.ino non compilerebbero: userebbero
// un oggetto definito piu' avanti nel testo. Config.h e' incluso prima
// di tutto, quindi la dichiarazione arriva sempre in tempo.
extern DisplayDriver display;

// Stessa storia per la UART del GPS: e' hardware del modulo, quindi vive
// nel suo driver, ma la usa anche leggiGPS() in WheelStat.ino.
extern HardwareSerial gpsSerial;

// ###########################################################################
// #                                                                         #
// #   ORIENTAMENTO DELL'IMU  -  LA PRIMA COSA DA SISTEMARE SU UNA           #
// #   SCATOLA NUOVA                                                         #
// #                                                                         #
// ###########################################################################
//
// Qui si dice com'e' girata la scatola, non che IMU c'e' dentro (il BNO055
// resta fisso, vedi docs/HARDWARE.md). Va provato con la scatola in mano:
// a tavolino non si indovina, e se e' sbagliato non compare nessun errore,
// arrivano solo numeri credibili e falsi.
//
// Due passaggi, in quest'ordine: prima si mettono a posto gli assi, poi si
// raddrizzano i versi.
//
//   PASSO 1 - POSIZIONE   scegli una riga di IMU_MONTAGGIO qui sotto
//   PASSO 2 - VERSI       accendi, fai le quattro prove qui sotto e inverti
//                         i SEGNO_* che escono al rovescio
//
// LE QUATTRO PROVE (pagina 0 per la piega, 2 per le G, 3 per l'impennata):
//
//   muovi la scatola cosi'          deve succedere questo        se no inverti
//   ------------------------------  ---------------------------  ------------
//   inclinala a DESTRA              sale "Piega Dx"              SEGNO_PIEGA
//   alza il DAVANTI (muso su)       sale "Impennata", non        SEGNO_IMPENNATA
//                                   "Stoppie"
//   spingila in AVANTI              Long positivo, pallino       SEGNO_G_LONG
//                                   del radar in ALTO
//   spingila a SINISTRA             pallino del radar a          SEGNO_G_LAT
//                                   SINISTRA
//
// Attenzione a distinguere due casi. Un verso rovesciato si sistema col
// SEGNO_* corrispondente. Due grandezze SCAMBIATE invece (la piega si muove
// quando alzi il muso) vogliono dire montaggio sbagliato: si torna al
// passo 1 e si prova un'altra posizione.

// Come e' orientata la basetta rispetto alla moto. Scommenta UNA riga sola.
// Il riferimento e' la faccia dei componenti, quella dove sta il chip: "in
// su" vuol dire rivolta verso il cielo.
#if !defined(IMU_MONTAGGIO_DRITTA) && !defined(IMU_MONTAGGIO_SOTTOSOPRA) && \
    !defined(IMU_MONTAGGIO_MANUALE)
  #define IMU_MONTAGGIO_SOTTOSOPRA  // montato: basetta capovolta
// #define IMU_MONTAGGIO_DRITTA     // componenti in su, basetta in piano
// #define IMU_MONTAGGIO_MANUALE    // nessuna delle due: vedi sotto
#endif

#ifdef IMU_BNO055
// La rimappatura degli assi la fa il chip, non il firmware: il BNO055
// presenta i dati come se fosse montato dritto. Sta dentro questo #ifdef,
// insieme all'include della libreria, perche' e' una sua caratteristica -
// il BNO085 non ce l'ha e li' la stessa rotazione andrebbe fatta in
// software dentro al driver.
//
// Le posizioni predefinite sono otto, da P0 a P7, ognuna con la sua coppia
// config/segni. Le due qui sotto coprono i montaggi normali; per qualcosa
// di diverso (basetta di taglio, o ruotata di 90 gradi sul piano) si passa
// a IMU_MONTAGGIO_MANUALE e si provano le altre.
#include <Adafruit_BNO055.h>

#if defined(IMU_MONTAGGIO_SOTTOSOPRA)
  #define REMAP_P P5
#elif defined(IMU_MONTAGGIO_DRITTA)
  #define REMAP_P P1   // P1 e' l'orientamento di fabbrica del chip
#elif defined(IMU_MONTAGGIO_MANUALE)
  #define REMAP_P P1   // <-- cambia qui: P0 ... P7
#endif

// Le due macro si incollano a REMAP_CONFIG_/REMAP_SIGN_ per formare il nome
// della costante della libreria: cosi' la posizione si cambia in un posto
// solo invece che in due, senza rischio di lasciarne una disallineata.
#define INCOLLA(a, b) a##b
#define NOME_REMAP(prefisso, p) INCOLLA(prefisso, p)

const Adafruit_BNO055::adafruit_bno055_axis_remap_config_t REMAP_ASSI =
    Adafruit_BNO055::NOME_REMAP(REMAP_CONFIG_, REMAP_P);
const Adafruit_BNO055::adafruit_bno055_axis_remap_sign_t REMAP_SEGNI =
    Adafruit_BNO055::NOME_REMAP(REMAP_SIGN_, REMAP_P);

// Servivano solo per le due righe qui sopra: non restano in giro con nomi
// cosi' generici per tutto il resto della compilazione.
#undef INCOLLA
#undef NOME_REMAP
#undef REMAP_P
#endif

// PASSO 2. Valgono per QUALUNQUE driver IMU: agiscono sulle quattro
// grandezze del contratto, non sugli assi del chip, quindi restano validi
// anche cambiando sensore. Vedi la tabella delle prove qui sopra.
const float SEGNO_PIEGA     =  1.0f;
const float SEGNO_IMPENNATA = -1.0f;
const float SEGNO_G_LONG    =  1.0f;
const float SEGNO_G_LAT     = -1.0f;

// ===========================================================================
// MODULO GPS
// ===========================================================================

const uint32_t GPS_BAUD = 9600;  // default di fabbrica NEO-6M/8M

#if defined(GPS_UBLOX) || defined(GPS_UBLOX_M10)

// A 9600 baud il modulo, con le frasi NMEA di default (GGA+GLL+GSA+GSV+
// RMC+VTG), spedisce circa 500 byte a ogni ciclo: a 1 Hz sono 500 B/s,
// comodi nei ~960 B/s di banda disponibili a 9600 baud. Salendo a 5 Hz
// servirebbero 2500 B/s con le frasi di default: si satura la seriale e
// i dati arrivano troncati. Per questo gpsConfigura() spegne le frasi
// che non uso (GLL/GSA/GSV/VTG) prima di alzare il rate: restano solo
// GGA e RMC (~140 byte/ciclo), che a 5 Hz fanno ~700 B/s, dentro budget
// con margine. 10 Hz servirebbe un baud rate piu' alto (fuori scopo per
// ora: cambiarlo su un modulo economico non e' sempre affidabile senza
// poterne verificare l'ACK).
//
// Sta dentro il ramo u-blox perche' e' una frequenza IMPOSTATA, non
// osservata: solo un modulo che accetta i comandi UBX puo' garantirla.
// Vale per entrambi i driver u-blox: l'M10 reggerebbe di piu' del
// vecchio M8, ma il limite qui non e' il chip, e' la seriale a 9600
// baud - il conto sulla banda qui sopra non cambia.
const uint8_t GPS_RATE_HZ = 5;

// Con GPS a 5 Hz un fix arriva ogni 200 ms: 700 ms (3.5 cicli mancati)
// e' un margine ragionevole prima di considerare il fix scaduto, molto
// piu' reattivo del vecchio timeout tarato per l'1 Hz di fabbrica.
// Se si cambia GPS_RATE_HZ va rivisto anche questo.
const unsigned long GPS_FIX_TIMEOUT_MS = 700;

#elif defined(GPS_NMEA_GENERICO)

// Nessun GPS_RATE_HZ qui, ed e' voluto: senza i comandi UBX la frequenza
// non si imposta, si subisce. Praticamente tutti i moduli escono di
// fabbrica a 1 Hz, cioe' un fix ogni 1000 ms.
//
// Il timeout va quindi allargato di conseguenza: con 700 ms tarati sul
// 5 Hz, un modulo a 1 Hz avrebbe il fix "scaduto" per i 300 ms prima di
// ogni aggiornamento, e il firmware lo vedrebbe lampeggiare fra valido
// e non valido piu' volte al secondo - con conseguenze vere, perche'
// rilevaGiro() e il canale velocita' guardano gpsFixValido. 3000 ms
// sono 3 cicli mancati, lo stesso margine relativo del ramo u-blox.
const unsigned long GPS_FIX_TIMEOUT_MS = 3000;

#else
  #error "Config.h: nessun driver GPS selezionato (vedi la sezione SCELTA DEI DRIVER)"
#endif

// ===========================================================================
// ACCESS POINT
// ===========================================================================

// Access point per il telefono
const char WIFI_SSID[]     = "WheelStat";
const char WIFI_PASSWORD[] = "30elode!";

#endif  // CONFIG_H
