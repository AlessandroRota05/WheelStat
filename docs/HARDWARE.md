# WheelStat: compatibilità hardware

Quali componenti si possono sostituire, con cosa, e cosa serve fare. Tutto quello che è elencato qui si cambia **da `Config.h`**, senza toccare la logica del firmware.

> **Stato delle verifiche.** Solo la configurazione montata è provata su hardware reale. I driver alternativi sono scritti e **verificati in compilazione**, non sul campo: quando ne monti uno per la prima volta, aspettati di dover aggiustare l'indirizzo I2C e di dover controllare i versi/le unità. Le righe segnate 🔧 sono quelle mai provate fisicamente.

---

## Configurazione montata

| Categoria | Componente | Bus | Indirizzo / Pin | Macro in `Config.h` |
|---|---|---|---|---|
| MCU | ESP32 DevKit V1 | - | - | - (automatica, vedi sotto) |
| IMU | Bosch BNO055 | I2C | `0x28` | - (non intercambiabile, vedi sotto) |
| Display | OLED SSD1306 128×64 | I2C | `0x3C` | `DISPLAY_SSD1306` |
| Meteo | DHT22 / AM2302 | one-wire | GPIO 4 | `METEO_DHT22` |
| GPS | u-blox NEO-6M / NEO-8M | UART2 | GPIO 16/17 | `GPS_UBLOX` |
| Pulsanti | 4× tattili | GPIO | 13 / 25 / 14 / 27 | - (solo pin) |

---

## Microcontrollore: quale ESP32

Il firmware gira su tutta la famiglia. Cambiano i **numeri GPIO**: sull'S3 e sul C3 i pin 22, 25 e 27 del DevKit V1 non esistono affatto, e il C3 ha due UART invece di tre. `Config.h` ha quindi quattro piedinature, scelte dal preprocessore con le macro `CONFIG_IDF_TARGET_*` del core: basta selezionare la board nell'IDE.

| Chip | Stato | SU/GIÙ/OK/LOG | DHT | SDA/SCL | GPS RX/TX | UART GPS | Flash (v9.16) |
|---|---|---|---|---|---|---|---|
| **ESP32** (DevKit V1, WROOM, WROVER) | montato | 13 / 25 / 14 / 27 | 4 | 21 / 22 | 16 / 17 | 2 | 1086268 B (82 %) |
| **ESP32-S3** | 🔧 | 5 / 6 / 7 / 15 | 4 | 8 / 9 | 18 / 17 | 2 | 1069349 B (81 %) |
| **ESP32-S2** | 🔧 | 5 / 6 / 7 / 15 | 4 | 8 / 9 | 18 / 17 | 1 | 1030660 B (78 %) |
| **ESP32-C3** | 🔧 | 3 / 4 / 5 / 6 | 10 | 8 / 9 | 0 / 1 | 1 | 1187209 B (**90 %**) |

Le piedinature 🔧 compilano e rispettano i vincoli del chip, ma **nessuna è mai stata cablata**: prima di saldare, confrontale con la serigrafia della tua board.

**Il C3 sta al 90% della flash** contro il 78-82% degli altri: è il core RISC-V, ~100 KB in più a parità di sorgente. Ci sta col partizionamento di default su 4 MB, ma il margine per nuove funzioni è un quinto. Se serve spazio, si cambia schema di partizionamento.

**Vincoli da rispettare se cambi i pin:**

- **strapping**, decidono la modalità di boot: un *pulsante* premuto all'accensione impedirebbe l'avvio. Sono `0, 2, 12, 15` (ESP32), `0, 3, 45, 46` (S3), `0, 45, 46` (S2), `2, 8, 9` (C3). L'**I2C ci può stare**: le pull-up tengono le linee alte a riposo, che è lo stato di boot normale, come SDA/SCL sul C3;
- **flash SPI interna**: `6-11` (ESP32), `26-32` (S2), `12-17` (molti moduli C3);
- **USB nativo**: `19/20` (S2, S3), `18/19` (C3).

Due trappole:

- **C3 e S2 hanno due sole UART** e la 0 è del monitor seriale: al GPS resta la 1 (`GPS_UART_NUM`). Scriverci 2 non dà errore di compilazione, dà una seriale muta, cioè un GPS che risulta assente senza motivo apparente.
- **Sull'ESP32 classico i GPIO 34-39 sono solo input e senza pull-up interna:** inutilizzabili per i pulsanti così come sono cablati. È l'errore facile cercando pin liberi su una board da 38 pin.

Per una board non prevista si copia il blocco più vicino in `Config.h`.

---

## Mappa del bus I2C: controllare prima di comprare

OLED, IMU ed eventuale sensore meteo I2C condividono **lo stesso bus** (SDA 21 / SCL 22 sull'ESP32 classico, 8 / 9 sulle altre varianti). Due componenti sullo stesso indirizzo non funzionano, e il sintomo è ambiguo: di solito ne sparisce uno solo, apparentemente a caso.

| Indirizzo | Occupato da | Note |
|---|---|---|
| `0x28` | BNO055 | `0x29` su alcuni breakout (ponticello ADR) |
| `0x3C` | SSD1306 / SH1106 / SH1107 / SSD1327 | `0x3D` su alcuni moduli |
| `0x38` | *libero* | AHT20 |
| `0x40` / `0x41` | *libero* | HTU21D, HTU31D, Si7021 |
| `0x44` / `0x45` | *libero* | SHT31, SHT4x |
| `0x4A` / `0x4B` | *libero* | BNO085 / BNO086, **non** collide col BNO055 |
| `0x76` / `0x77` | *libero* | BME280 / BMP280 |

Nessuna delle alternative elencate in questo documento collide con l'esistente. **Va comunque verificato scheda per scheda**: i breakout economici non sempre rispettano il datasheet, e capita che un modulo venduto come `0x76` arrivi cablato a `0x77`. Uno sketch I2C scanner da 20 righe risolve il dubbio in un minuto.

---

## Sensore meteo

**Sette driver, tutti scritti.** Si passa dall'uno all'altro con una riga in `Config.h` più l'installazione della libreria.

| Componente | Stato | Bus | Indirizzo | Macro | Libreria | Cosa cambia |
|---|---|---|---|---|---|---|
| **DHT22 / AM2302** | montato | one-wire | GPIO 4 | `METEO_DHT22` | DHT sensor library | - |
| **Sensirion SHT4x** (SHT40/41/45) | 🔧 **consigliato** | I2C | `0x44` fisso | `METEO_SHT4X` | Adafruit SHT4x | La generazione attuale Sensirion: ±1.8 %RH, ±0.2 °C. **Se compri oggi, è questo.** Riscaldatore tenuto spento dal driver: scalderebbe il sensore falsando proprio la temperatura da cui dipende il rischio grip |
| **Sensirion SHT31-D** | 🔧 pronto | I2C | `0x44` | `METEO_SHT31` | Adafruit SHT31 | Generazione precedente: ±2 %RH, ±0.3 °C |
| **HTU31D** | 🔧 pronto | I2C | `0x40` | `METEO_HTU31D` | Adafruit HTU31D | Paragonabile allo SHT31 a costo minore |
| **Si7021** | 🔧 pronto | I2C | `0x40` fisso | `METEO_SI7021` | Adafruit Si7021 | Classico affidabile, gruppo di testa nei confronti indipendenti |
| **HTU21D-F** | 🔧 pronto | I2C | `0x40` fisso | `METEO_HTU21D` | Adafruit HTU21DF | Predecessore dell'HTU31D. Utile come ricambio |
| **AHT20** | 🔧 pronto | I2C | `0x38` fisso | `METEO_AHT20` | Adafruit AHTX0 | Il più economico. Nei confronti indipendenti lo scarto dal riferimento **varia con la temperatura** e tende a leggere alto: accettabile per un indice indicativo come il rischio grip, non per misure fini |
| **Bosch BME280** | 🔧 pronto | I2C | `0x76` | `METEO_BME280` | Adafruit BME280 | Ha anche la pressione, che il firmware ignora: rappresentarla richiederebbe una voce in `IndiceCanale` (la tabella dei canali in `WheelStat.ino`), non un driver. Il suo `meteoInit()` legge il chip-ID e **spiega sul monitor seriale** se hai ricevuto un BMP280 |

Tre di questi (SHT4x, HTU31D, AHT20) usano l'API *unified sensor* di Adafruit: una chiamata che riempie due strutture evento invece di due funzioni che ritornano `float`. Per il contratto non cambia niente, ma attenzione all'ordine dei parametri di `getEvent()`: **prima l'umidità**.

Si7021, HTU21D e HTU31D stanno tutti su `0x40`: non collidono con niente di montato, ma non possono convivere fra loro. Non è un problema: se ne monta uno solo.

> ⚠️ **Trappola BME280 / BMP280.** Sono due sensori diversi con lo stesso identico aspetto e **lo stesso indirizzo I2C** (`0x76`/`0x77`): il BMP280 misura solo temperatura e pressione, **niente umidità**. È una delle sostituzioni sbagliate più comuni fra i moduli venduti online, dove capita di ordinare un BME280 e ricevere un BMP280. Per questo progetto non è un dettaglio: **l'umidità è metà della formula del rischio grip** (`calcolaRischioGrip`), quindi un BMP280 non degrada la funzione, la disattiva. Si distinguono solo leggendo il registro chip-ID `0xD0`, non dall'indirizzo.

Il contratto da implementare (`meteoInit` / `meteoLeggi`) è documentato in `Config.h`. Un ramo nuovo sono ~10 righe: il modello è il ramo `METEO_SHT31` in `src/driver/Meteo.h`.

**Requisito non negoziabile della categoria:** il sensore deve dare **temperatura *e* umidità**. Un sensore di sola temperatura non è un'alternativa parziale, è incompatibile: senza umidità il rischio grip non si calcola.

**Attenzione al `INTERVALLO_METEO`**: sta in `Config.h`, in un blocco per driver. Non è una preferenza, è il limite fisico del sensore: il DHT22 campiona internamente ogni 2 s e interrogarlo più spesso restituisce la lettura precedente, non una nuova.

---

## Display OLED

| Componente | Stato | Risoluzione | Indirizzo | Macro | Cosa cambia |
|---|---|---|---|---|---|
| **SSD1306 128×64** | montato | 128×64 | `0x3C` | `DISPLAY_SSD1306` | - |
| **SH1106 128×64** | 🔧 pronto | 128×64 | `0x3C` | `DISPLAY_SH1106` | **Niente**: stessa risoluzione, layout identico al pixel |
| **SH1107 128×64** | 🔧 pronto | 128×64 | `0x3C` | `DISPLAY_SH1107` | Stessa libreria dell'SH1106 (`Adafruit_SH110X` espone due classi). ⚠️ La maggior parte dei moduli SH1107 è **128×128**: vedi riga sotto |
| **SSD1309 128×64** | 🔧 pronto | 128×64 | `0x3C` | `DISPLAY_SSD1309` | Controller diverso ma compatibile nei comandi: usa **lo stesso driver dell'SSD1306**, la macro separata serve solo a poter scrivere in `Config.h` che pannello c'è davvero |
| **SSD1306 128×32**, **SH1107 128×128**, **SSD1327 128×128** | ❌ non supportati | ≠128×64 | `0x3C` | - | Il layout è parametrico **solo in orizzontale**: le y (titolo 15, riga 24, dati 30/41/52, piede 56) sono una griglia disegnata a mano, non frazioni di 64 da riscalare. Su 32 px **non è un problema di aritmetica ma di quali dati sacrificare**: tre righe dati non ci stanno, ed è una riprogettazione da fare col pannello davanti; su 128 px il contenuto starebbe schiacciato nella metà superiore con mezzo schermo nero |

**Regola pratica della categoria:** qualunque pannello **monocromatico 128×64 con una libreria basata su Adafruit_GFX** entra con poche righe di driver e zero modifiche al layout. Tutto il resto è un altro lavoro.

Esiste anche **U8g2**, che da sola copre una cinquantina di controller diversi, ma ha una sua API, quindi adottarla vorrebbe dire riscrivere tutte le ~180 chiamate di disegno di `PagineOled.ino`. Non ne vale la pena finché i pannelli che interessano sono coperti da Adafruit_GFX.

> **Se compri un OLED "SSD1306" e l'immagine appare spostata di due pixel in orizzontale, o con una colonna di rumore sul bordo, non è rotto: è un SH1106.** È un caso frequentissimo: quel controller viene rivenduto sotto il nome più noto. Si risolve accendendo `DISPLAY_SH1106` in `Config.h`.

Il disegno vero e proprio (le 10 pagine, il riepilogo di fine sessione) non fa parte del contratto e non cambia mai: sono primitive **Adafruit_GFX**, comuni a tutti i pannelli monocromatici. L'unica cosa specifica del chip è la sequenza di accensione, ed è il motivo per cui i file driver sono corti.

---

## GPS

| Componente | Stato | Macro | Cosa cambia |
|---|---|---|---|
| **u-blox NEO-6M / NEO-8M** | montato | `GPS_UBLOX` | - |
| **Beitian BN-220 / BN-880** | 🔧 **consigliato** | `GPS_UBLOX` | Sono moduli **u-blox M8**: i comandi UBX del driver attuale valgono così come sono. Il vantaggio vero è l'**antenna ceramica integrata**, molto meglio del NEO-6M nudo per un uso in moto. Il BN-880 aggiunge una bussola che il firmware non usa |
| **u-blox NEO-M9N** | 🔧 compatibile | `GPS_UBLOX` | Accetta sia i comandi legacy sia quelli nuovi |
| **u-blox M10** (MAX-M10S, MIA-M10, Beitian serie M10) | 🔧 pronto | `GPS_UBLOX_M10` | Aggancia il fix in 15-20 s invece di 45-60 s e vede più costellazioni. **Serve il driver dedicato**, non `GPS_UBLOX`: vedi sotto |
| **Qualunque modulo NMEA** (MT3339, Quectel L80, SIM28, ATGM336H, cloni) | 🔧 pronto | `GPS_NMEA_GENERICO` | Resta a 1 Hz invece di 5 Hz |

> ⚠️ **`GPS_UBLOX` e `GPS_UBLOX_M10` non si coprono a vicenda.** Dalla generazione M10 u-blox ha rimosso il meccanismo di configurazione legacy: `UBX-CFG-MSG` e `UBX-CFG-RATE` non sono più supportati, sostituiti dalle chiavi `UBX-CFG-VALSET`. Un M8 accetta entrambi, un M10 solo il nuovo, un M6 solo il vecchio.
>
> Scegliere il driver sbagliato non rompe niente in modo vistoso: il modulo resta a 1 Hz di fabbrica mentre `GPS_FIX_TIMEOUT_MS` è tarato sul rate alto, `gpsFixValido` lampeggia più volte al secondo, e a rimetterci sono `rilevaGiro()` e il canale velocità. Il sintomo (lap timing che salta passaggi, velocità a zero a intermittenza) non assomiglia alla causa.
>
> Per questo `Gps_UbloxM10.ino` **legge gli ACK**, a differenza di `Gps_Ublox.ino`. Se una chiave viene rifiutata, il monitor seriale lo dice al boot nominando l'impostazione. È anche ciò che ha reso quel driver scrivibile senza avere il modulo in mano: le chiavi numeriche vengono dal manuale d'interfaccia e non sono verificate sul campo, ma un errore si presenta come un NAK stampato, non come un GPS che si comporta male in pista.

Passare al driver generico **non** toglie funzionalità: la lettura è NMEA standard, che parlano tutti. Si perde risoluzione, e conviene sapere dove:

- **Lap timing**: a 100 km/h, un punto al secondo significa 28 metri fra un campione e l'altro, contro 5.6 metri a 5 Hz. Con `RAGGIO_TRAGUARDO_M` = 20 m, il momento in cui si rileva il passaggio sul traguardo diventa meno preciso e i tempi sul giro più rumorosi.
- **Traccia e forma del tracciato**: cinque volte meno punti, quindi disegno più spigoloso.

`GPS_FIX_TIMEOUT_MS` è già allargato a 3000 ms su quel ramo. Non è cosmetico: col timeout da 700 ms tarato sul 5 Hz, un modulo a 1 Hz avrebbe il fix "scaduto" per i 300 ms prima di ogni aggiornamento, e `gpsFixValido` lampeggerebbe fra vero e falso più volte al secondo, con conseguenze reali su `rilevaGiro()` e sul canale velocità.

---

## Pulsanti

Quattro GPIO con pull-up interna, un capo al pin e l'altro a GND: nessuna resistenza esterna. Si spostano su qualunque pin libero cambiando `PIN_BTN_*` in `Config.h`.

Cambiarne il **numero** è un'altra cosa: non è un problema hardware ma di interfaccia (con tre tasti servirebbe ripensare come si naviga fra 10 pagine e si accede alle azioni di `gestisciPulsanti()`). Fuori dallo scopo della modularità.

---

## ⚠️ IMU: il BNO055 è fuori produzione

Bosch lo dichiara **"not recommended for new designs"**. Resterà reperibile a lungo sui breakout già prodotti, ma non è un componente su cui costruire. Il successore è la famiglia **BNO085 / BNO086**, pin-for-pin compatibile e con la sensor fusion ancora dentro al chip.

| | BNO055 (montato) | BNO085 / BNO086 |
|---|---|---|
| Stato | fine vita | attuale |
| Sensor fusion | nel chip, 100 Hz | nel chip, 400 Hz |
| Indirizzo I2C | `0x28` / `0x29` | `0x4A` / `0x4B` (nessuna collisione) |
| Libreria | `Adafruit BNO055` | `Adafruit BNO08x` |
| Protocollo | registri I2C classici | SH-2 / SHTP, **completamente diverso** |
| Calibrazione | da rifare a ogni accensione | salvata automaticamente |

**Il seam c'è, il driver no.** L'IMU sta dietro un contratto come gli altri componenti (`src/driver/Imu.h`), quindi scrivere `Imu_BNO085.h` è un lavoro circoscritto a un file: sarebbero ~120 righe, e la libreria Adafruit ricava già gli angoli dai quaternioni.

Manca perché **tre cose non si deducono senza il sensore in mano**:

1. **Quale angolo del quaternione è la piega e quale l'impennata**, dato il montaggio fisico. Sul BNO055 questa mappatura è costata una versione intera: la v6.3 li aveva scambiati, e si è scoperto solo provando.
2. **I segni.** Ci sono i `SEGNO_*` in `Config.h` per invertirli, ma vanno determinati sul banco.
3. **La rimappatura degli assi.** Sul BNO055 la fa il chip (`REMAP_CONFIG_P5`), il BNO085 non ha quella feature: la stessa rotazione va rifatta in software, e **scambiare due assi non è un'inversione di segno**.

Un driver scritto indovinando queste tre cose compilerebbe e sembrerebbe finito. Su un dispositivo che mostra "PERICOLO GRIP" in base all'angolo di piega, un asse scambiato non è un difetto cosmetico.

Da sapere in anticipo: `attesaCalibrazioneIMU()` perderebbe gran parte del suo senso, perché il BNO085 salva la calibrazione da solo invece di ripartire scalibrato a ogni accensione.

---

## Componenti esclusi

| Componente | Motivo dell'esclusione |
|---|---|
| **IMU senza fusion** (MPU6050, ICM-20948) | Richiedono un filtro Madgwick o Mahony scritto nel firmware: è lavoro reale, non un'interfaccia. Per il BNO085, che la fusion ce l'ha, vedi la sezione qui sopra. |
| **Storage (LittleFS)** | La microSD richiede il level shifter 5V→3.3V già scartato in passato; una flash SPI esterna è un porting diverso di LittleFS. |
| **Microcontrollore fuori dalla famiglia Espressif** | Il vincolo non è la CPU ma il **WiFi**: `WiFi.h` + `WebServer.h` + LittleFS nativi esistono solo lì. STM32 richiede hardware WiFi esterno, Arduino classico non ha la RAM per il web server, Arduino WiFi userebbe WiFiNINA e imporrebbe di riscrivere l'interfaccia web quasi da zero. Per gli ESP32 diversi dal classico vedi la sezione qui sotto: quelli sono supportati. |
| **ESP8266** | Ha il WiFi, ma con API diverse (`ESP8266WiFi.h`, `ESP8266WebServer.h`) e una sola UART completa, che serve già al monitor seriale. Non è uno scambio di configurazione ma un porting. |

---

## Come si cambia un componente

1. In `Config.h`, sezione **SCELTA DEI DRIVER**: commenta la macro attuale, scommenta quella nuova.
2. Installa la libreria del driver nuovo (Arduino IDE → Gestore librerie).
3. Se il componente è I2C, controlla l'indirizzo nella mappa qui sopra e correggilo in `Config.h` se il tuo modulo usa quello alternativo.
4. Compila. Se non compila, non è un problema tuo: è un bug del driver, segnalalo.
5. Prova sull'hardware: è l'unico passo che la compilazione non sostituisce.

## Come si aggiunge un driver nuovo

1. Aggiungi un ramo `#elif defined(...)` nel `src/driver/*.h` della categoria, copiando quello del componente più simile: oggetto, include della libreria, nome.
2. Aggiungi la macro alla catena `#if !defined(...)` in `Config.h` e, se serve, il suo indirizzo I2C e la sua temporizzazione.
3. Se l'accensione o la lettura differiscono, aggiungi il caso in `xInit()` / `xLeggi()`. Spesso non serve: i sensori meteo che condividono l'API condividono anche la funzione di lettura.
4. Verifica **tutte** le combinazioni con la matrice di compilazione (vedi [DRIVER.md](DRIVER.md)): se non hai ancora il componente, si compila contro uno stub header, senza installare niente.

La procedura dettagliata, con un esempio completo, è in [DRIVER.md](DRIVER.md).

**Regola per capire dove va una costante:** *"cambia se monto componenti diversi, o gli stessi montati in modo diverso?"* → sì: `Config.h`; no: sezione 3 di `WheelStat.ino`.
