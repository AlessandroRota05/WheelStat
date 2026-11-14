# Com'è fatto il firmware

Per chi vuole leggere o modificare il codice. Per usare il dispositivo basta il [README](../README.md).

## La cartella

```
WheelStat/
  WheelStat.ino        solo setup() e loop(): chiamano src/App
  Config.h             quello che cambia con l'hardware: lingua, driver, pin, orientamento IMU
  platformio.ini       lo stesso sketch per PlatformIO, e l'ambiente dei test
  src/
    App                avvio, ciclo principale, pulsanti
    Tuning.h           soglie, tempi e limiti che non dipendono dall'hardware
    Channels           la tabella delle grandezze misurate e degli eventi
    Settings           le impostazioni cambiate dal sito
    Motion, Weather, Gps, Stats          sensori e filtro
    LapTimer, Tracks, Records            giri, tracciati, record
    Recorder, Sessions, SessionFormat    scrittura e lettura delle sessioni
    Storage, Buttons, Util, Version
    core/              logica senza Arduino, testata sul PC
    drivers/           un file per categoria di componente
    ui/                pagine OLED
    web/               sito
    i18n/              testi in italiano e in inglese
  test/                test di src/core
  tools/oled-preview/  le schermate OLED disegnate sul PC, per la documentazione
  Media/               immagini della documentazione: oled/ e web/ (it, en), hardware/
  docs/
```

Arduino compila da solo, ricorsivamente, tutto ciò che trova in `src/`. La cartella dello sketch deve chiamarsi `WheelStat`.

## Le regole del codice

- **Un modulo è un namespace**, con lo stato in variabili `static` del suo `.cpp`. Di ognuno esiste un'istanza sola, quindi una classe non aggiungerebbe niente: lo stato resta irraggiungibile da fuori lo stesso, senza oggetti da passare in giro. Chi usa un modulo include solo il suo `.h`.
- **Le dipendenze vanno in un verso solo.** `Tracks` conosce `LapTimer` e `Records`, non il contrario: il cronometro riferisce un evento e non sa che esistono i tracciati.
- **Config.h o Tuning.h?** *"Cambia se monto componenti diversi, o gli stessi montati in modo diverso?"* Sì: `Config.h`. No: `src/Tuning.h`.
- **Codice e commenti in inglese.** I commenti dicono il perché di una scelta, non ripetono il codice. La cronologia sta solo in [CHANGELOG.md](CHANGELOG.md).
- **Niente `String` nei percorsi caldi.** Le pagine web escono a blocchi da un buffer fisso (`web::PageWriter`) invece di essere costruite in RAM.

## Il ciclo principale

`app::loop()` è uno scheduler cooperativo: ogni lavoro ha il suo timer e nessuno blocca gli altri.

| Ordine | Lavoro | Ogni quanto |
|---|---|---|
| 1 | `weather::update()` | `WEATHER_INTERVAL_MS` (dipende dal sensore) |
| 2 | `motion::update()` → `stats::sample()` | ogni giro: non si perdono i picchi |
| 3 | `gps::update()` | ogni giro: l'NMEA va svuotato subito |
| 4 | `laps::update()` → `tracks::onLapEvent()` | ogni giro, sulle posizioni appena lette |
| 5 | pulsanti, `web::handleClient()` | ogni giro |
| 6 | `oled::render()` | 100 ms |
| 7 | telemetria sul monitor seriale | 5 s |
| 8 | `recorder::update()` | ogni giro: prende i campioni dovuti |

Tre cose bloccano il ciclo, e sono note: servire una pagina web (centinaia di ms), l'animazione di cambio pagina (~200 ms) e le schermate di riepilogo. Per questo i tempi sul giro non usano `millis()` ma l'orologio del ricevitore GPS (`core::GpsClock`), e il file di sessione colloca ogni campione al suo posto nel tempo contandoli dall'inizio.

## src/core: la logica senza hardware

Tutto ciò che si può decidere senza toccare un sensore sta in `src/core/`, in C++ senza `Arduino.h`. Il firmware lo compila così com'è; i test lo compilano per il PC.

| File | Cosa contiene | Chi lo usa |
|---|---|---|
| `Geo.h` | Proiezione piana locale, prodotti vettoriali | Gate, Trace, sito |
| `Gate` | Traguardo e checkpoint come linee; apprendimento del verso | LapLogic |
| `LapLogic` | Giri, settori, giro migliore | `src/LapTimer` |
| `Trace` | Traccia di un giro che si dirada da sola; delta sul riferimento | LapLogic, `src/Tracks` |
| `GpsClock` | Orologio monotono agganciato all'ora del GPS | `src/Gps` |
| `PeakFilter` | Filtro anti-buca, massimi, eventi | `src/Stats` |
| `SessionCodec` | Formato binario delle sessioni, riga CSV | `src/Recorder`, `src/Sessions`, sito |
| `SettingsRules` | Valori iniziali e limiti delle impostazioni | `src/Settings` |
| `GripRisk.h` | Formula del rischio grip | `src/Weather`, CSV |

I moduli corrispondenti in `src/` sono involucri sottili: leggono i sensori e chiamano il core.

### I test

```bash
pio test -e native
```

Serve `g++` nel PATH. Sono in `test/test_core/` e simulano una guida dando al cronometro le posizioni che manderebbe un GPS: un circuito circolare a velocità costante, di cui si conosce il tempo esatto. Ciò che i test non possono coprire è tutto il resto: sensori, display, flash, WiFi. Per quello resta solo la prova sul dispositivo.

I driver non hanno test: si verificano compilando tutte le combinazioni ([DRIVER.md](DRIVER.md#la-matrice-di-compilazione)).

### Le schermate sul PC

`tools/oled-preview/` compila il codice vero di `src/ui/` insieme alla vera Adafruit GFX, con al posto degli altri moduli delle controfigure che descrivono una guida inventata. Ogni schermata viene salvata e trasformata nelle immagini di `Media/oled/`, in tutte e due le lingue:

```bash
powershell -File tools/oled-preview/build.ps1
```

Servono `g++`, Python con Pillow e la libreria GFX dove la installa l'Arduino IDE. Dopo una modifica a una pagina basta rilanciarlo: le immagini del README restano quelle vere. Se una pagina comincia a usare una funzione di un modulo che le controfigure non hanno ancora, il collegamento fallisce e la si aggiunge in `preview.cpp`.

## I file sulla flash

Partizione LittleFS da 1,4 MB (schema di default da 4 MB, che ha anche le due aree applicazione usate dall'aggiornamento via WiFi).

| File | Formato | Contenuto |
|---|---|---|
| `LOG_n.BIN` | `core/SessionCodec.h` | Una sessione |
| `TRACK_n.BIN` | `tracks::Track`, ~2 KB | Nome, traguardo, checkpoint, giro di riferimento |
| `TRACK_n_REC.BIN` | `records::AllTime` | Record dei canali su quel tracciato |
| `RECORD.BIN` | `records::AllTime` | Record dei canali in modalità libera |
| `SETTINGS.BIN` | `core::Settings` | Impostazioni |

I file piccoli sono struct scritte così come stanno in RAM, con un **numero magico** nel primo campo che fa anche da versione (`storage::loadStruct`). Chi cambia una struct cambia il suo magico: i file vecchi smettono di essere validi e si riparte dai valori iniziali, invece di leggere dati sfasati.

### Una sessione

```
SessionHeader   56 byte   firma, dimensioni, frequenza, nome del tracciato, soglie
SessionSample   24 byte   × N, alla frequenza di log
SessionFooter  860 byte   massimi, eventi, giri e settori
```

- Un campione è il **picco** di piega, beccheggio e G nel suo intervallo dopo il filtro anti-buca, col segno: il massimo dei campioni coincide col massimo di sessione.
- I campioni aspettano in RAM e vanno sulla flash ogni 5 secondi, a file aperto.
- Un file **senza footer** è una registrazione che ha perso l'alimentazione: intestazione e campioni sono buoni, manca solo il riepilogo.
- Il CSV non esiste sulla flash: lo produce il sito al download.
- La pagina Grafici decodifica gli stessi byte in JavaScript (`src/web/WebSession.cpp`): cambiando una struct va aggiornata anche quella.

## Il sito

`WebServer` della libreria ESP32 su un access point, senza internet: nessuna libreria o font esterni.

| File | Contenuto |
|---|---|
| `Web.cpp` | Server, rotte, access point, intestazione comune |
| `WebAssets.cpp` | Foglio di stile, pagina Live |
| `WebPages.cpp` | Sessioni, Tracciati, Confronta |
| `WebSession.cpp` | Pagina Grafici |
| `WebSettings.cpp` | Impostazioni, aggiornamento firmware |
| `WebActions.cpp` | JSON live, download, azioni |

Le azioni che cambiano le impostazioni e il caricamento del firmware sono `POST`. Durante una registrazione tutto ciò che legge o scrive file è rifiutato (`rejectWhileRecording`).

**Provare una pagina senza il dispositivo.** Corpo e script della pagina Grafici non contengono testi tradotti (arrivano dal dizionario `T`), quindi si possono estrarre dal sorgente e aprire in un browser con un file di sessione finto. È così che è stata verificata.

## I testi

Tutte le scritte stanno in `src/i18n/Text_it.h` e `Text_en.h` come macro, così si incollano in letterali più grandi a tempo di compilazione. Se ne compila una sola: la seconda lingua non occupa flash.

| Prefisso | Dove finisce | Vincoli |
|---|---|---|
| `TR_O_` | OLED | Solo ASCII, 21 caratteri per riga, 8 per l'etichetta di una riga dati |
| `TR_W_` | Sito | UTF-8, niente `'` né `"` (finiscono dentro attributi HTML e JavaScript) |
| `TR_E_` | Risposte di errore del sito | Testo semplice |
| `TR_CH_` | Nomi dei canali | - |

Una chiave nuova va aggiunta a entrambi i file: compilare nelle due lingue è il controllo.

## Aggiungere cose

- **Un canale** (una grandezza misurata): una voce in `Channel`, una riga nella tabella di `Channels.cpp`, una riga in `stats::sample()`. Massimi, record e riepilogo lo includono da soli. Cambia `records::AllTime`: va cambiato il suo magico.
- **Un'impostazione**: un campo in `core::Settings` con valore iniziale e limiti in `SettingsRules.cpp`, un campo nel form di `WebSettings.cpp`, e il magico di `Settings.cpp`.
- **Una lingua**: copia `Text_en.h`, aggiungi `LANG_xx` in `Config.h` e il ramo in `Text.h`.
- **Un driver**: [DRIVER.md](DRIVER.md).
