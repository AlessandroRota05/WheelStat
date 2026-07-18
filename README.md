# WheelStat

![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)
![Platform](https://img.shields.io/badge/platform-ESP32-orange.svg)

>  **Seleziona la lingua / Select your language:** [🇮🇹 Italiano](#italiano) · [🇬🇧 English](#english)

---

<a id="italiano"></a>
## 🇮🇹 Italiano

WheelStat è un sistema di telemetria open source basato su **ESP32**, progettato per moto. Grazie all'integrazione di un sensore IMU (**Bosch BNO055**), il sistema monitora in tempo reale l'angolo di piega, l'angolo di impennata e le forze G dinamiche, calcolando contemporaneamente un indice di rischio di perdita di grip basato sui parametri ambientali rilevati da un sensore **DHT22**.

I dati vengono visualizzati live su una pagina web o un modulo display OLED 0.96" con 4 pulsanti integrati tramite un'interfaccia a 7 schermate e salvati automaticamente nella memoria flash interna dell'ESP32 in formato CSV: a fine giro si scaricano via WiFi direttamente dal telefono per l'analisi post-sessione.


### 📑 Indice

- [Architettura Hardware](#it-architettura)
- [Bill of Materials](#it-bom)
- [Schemi e Immagini del Progetto](#it-schemi)
- [Come Funziona il Rischio Grip](#it-come-funziona)
- [Librerie Richieste](#it-librerie)
- [Struttura dei Dati di Log (CSV)](#it-csv)
- [Record Storici](#it-record)
- [WiFi e Interfaccia Web](#it-wifi)
- [Note di Sicurezza](#it-sicurezza)
- [Licenza](#it-licenza)
- [Autore](#it-autore)

#### Interfaccia Grafica

| Pagina | Contenuto |
|---|---|
| **Page 0 — Piega** | Visualizzazione dell'angolo di inclinazione laterale con barra grafica dinamica e alert di pericolo |
| **Page 1 — Meteo** | Monitoraggio della temperatura, umidità dell'aria e percentuale di rischio grip |
| **Page 2 — Forza G** | Radar 2D grafico con tracciamento vettoriale delle accelerazioni |
| **Page 3 — Impennata/Stoppie** | Monitoraggio dell'angolo di beccheggio con indicatore verticale grafico |
| **Page 4 — Record** | Massimi storici di tutte le sessioni: piega, impennata, stoppie e forze G |
| **Page 5 — Memoria** | Spazio libero sulla flash interna, file corrente e minuti registrati |
| **Page 6 — WiFi** | Accensione dell'access point, con SSID, password e indirizzo del sito |

<a id="it-architettura"></a>
### Architettura Hardware

Il centro del progetto è un microcontrollore **ESP32 DevKit V1** (30 o 38 pin).

#### Schema di Cablaggio (Pinout)

Tutti i moduli comunicano con l'ESP32 tramite i bus standard I2C e SPI o pin digitali dedicati:

| Componente | Bus / Segnale | Pin ESP32 | Note |
|---|---|---|---|
| OLED SSD1306 | I2C (SDA) | GPIO 21 | Condiviso con BNO055 |
| OLED SSD1306 | I2C (SCL) | GPIO 22 | Condiviso con BNO055 |
| Bosch BNO055 | I2C (SDA) | GPIO 21 | Indirizzo I2C standard: 0x28 |
| Bosch BNO055 | I2C (SCL) | GPIO 22 | Indirizzo I2C standard: 0x28 |
| DHT22 | GPIO Digitale | GPIO 4 | Linea dati singola |
| Pulsante SU | GPIO Digitale | GPIO 13 | Configurato come INPUT_PULLUP |
| Pulsante GIÙ | GPIO Digitale | GPIO 25 | Configurato come INPUT_PULLUP |
| Pulsante OK | GPIO Digitale | GPIO 14 | Configurato come INPUT_PULLUP |
| Pulsante LOG | GPIO Digitale | GPIO 27 | Configurato come INPUT_PULLUP |

<a id="it-bom"></a>
### Bill of Materials

| Componente | Quantità | Note |
|---|---|---|
| ESP32 DevKit V1 (30/38 pin) | 1 | Microcontrollore principale |
| Bosch BNO055 (breakout) | 1 | IMU a 9 assi, sensor fusion hardware |
| DHT22  | 1 | Sensore temperatura/umidità |
| Display OLED SSD1306 (I2C, 128x64) | 1 | Interfaccia grafica |
| Pulsanti tattili | 4 | SU / GIÙ / OK / LOG (integrati nel modulo display)|
| Cavi jumper / dupont | q.b. | Collegamenti |
| Alimentazione | 1 | alimentazione usb-c |

<a id="it-schemi"></a>
### Schemi e Immagini del Progetto

#### Schema Elettrico / Circuitale

![Schema Circuitale](Media/schemacircuitale.png)

#### Schema Topografico

![Schema Topografico](Media/schemaTopografico.png)


#### Interfaccia OLED

![Interfaccia OLED](Media/interfacciaOled.png)
![Interfaccia OLED](Media/interfacciaOled1.png)
![Interfaccia OLED](Media/interfacciaOled2.png)
![Interfaccia OLED](Media/InterfacciaOled3.png)
![Interfaccia OLED](Media/InterfacciaOled4.png)

<a id="it-come-funziona"></a>
### Come Funziona il Rischio Grip

L'indice di rischio grip è una stima, non una misura diretta di aderenza. Il firmware incrocia la temperatura e l'umidità rilevate dal DHT22 per valutare condizioni potenzialmente sfavorevoli (es. asfalto freddo o umido) e restituisce una percentuale di rischio indicativa, mostrata a display con eventuali allarmi visivi. Non sostituisce la valutazione diretta del pilota sulle reali condizioni dell'asfalto.

<a id="it-librerie"></a>
### 📦 Librerie Richieste

Per compilare correttamente il firmware su Arduino IDE assicurati di aver installato le seguenti librerie:

- `Adafruit BNO055` (di Adafruit)
- `Adafruit SSD1306` (di Adafruit)
- `Adafruit GFX Library` (di Adafruit)
- `DHT sensor library` (di Adafruit)
- `Adafruit Unified Sensor` (richiesta come dipendenza comune)

<a id="it-csv"></a>
### Struttura dei Dati di Log (CSV)

I file di log vengono salvati in modo sequenziale nella memoria flash interna (LittleFS) con la nomenclatura `LOG_1.CSV`, `LOG_2.CSV`, ecc. Ogni minuto viene scritta una riga con i valori massimi del minuto (angoli, forze G, meteo e rischio grip); alla chiusura della sessione viene accodato un riepilogo con i massimi complessivi e il conteggio degli eventi oltre soglia (impennate, stoppie, pieghe, frenate e accelerate brusche). I file si scaricano e si cancellano dall'interfaccia web.

<a id="it-record"></a>
### Record Storici

A fine registrazione i massimi della sessione aggiornano i record "di sempre", salvati in un file sulla flash che sopravvive allo spegnimento. Si consultano sulla pagina OLED **Record** e sul sito web, da cui si possono anche azzerare (con conferma). Nel riepilogo di fine sessione un asterisco segnala i valori che hanno appena battuto un record.

<a id="it-wifi"></a>
### WiFi e Interfaccia Web

Dalla pagina **WiFi** (tasto OK) l'ESP32 accende un access point dedicato: collegando il telefono alla rete `WheelStat` (credenziali mostrate a display) e aprendo `http://192.168.4.1` si accede alla telemetria live con grafici, al download e alla cancellazione dei CSV e ai record storici. Non serve internet: è tutto servito dall'ESP32.

<a id="it-sicurezza"></a>
### ⚠️ Note di Sicurezza

WheelStat è pensato come strumento di analisi post-sessione e non come ausilio alla guida in tempo reale. Non consultare il display mentre si è in movimento: tenere sempre lo sguardo sulla strada/pista ha priorità assoluta.
Il display deve essere consultato post sessione registrandolo con una telecamera esterna per avere un feedback preciso nell'istante desiderato.
Il rischio grip calcolato è indicativo e non sostituisce l'esperienza del pilota né la valutazione diretta delle condizioni dell'asfalto e degli pneumatici. Verifica sempre che il montaggio dell'hardware sul veicolo sia solido e non interferisca con comandi o visuale.

<a id="it-licenza"></a>
### 📄 Licenza

Questo progetto è distribuito sotto licenza **Apache License 2.0**. Consulta il file [LICENSE](LICENSE)

<a id="it-autore"></a>
### Autore

Progettato e sviluppato da **Alessandro Rota** con il supporto indispensabile di (tanta) caffeina.


---

<a id="english"></a>
## 🇬🇧 English

WheelStat is an open source telemetry system based on **ESP32**, designed for motorcycles. Thanks to the integration of an IMU sensor (**Bosch BNO055**), the system monitors in real time the lean angle, wheelie angle and dynamic G-forces, while simultaneously calculating a grip-loss risk index based on the environmental parameters detected by a **DHT22** sensor.

Data is displayed live on a 0.96" OLED display module with 4 integrated buttons through a 7-screen interface, and is automatically saved to the ESP32's internal flash memory in CSV format: at the end of the ride, files can be downloaded via WiFi directly from your phone for post-session analysis.

### 📑 Table of Contents

- [Hardware Architecture](#en-architecture)
- [Bill of Materials](#en-bom)
- [Project Diagrams and Images](#en-diagrams)
- [How Grip Risk Works](#en-how-it-works)
- [Required Libraries](#en-libraries)
- [Log Data Structure (CSV)](#en-csv)
- [All-Time Records](#en-record)
- [WiFi and Web Interface](#en-wifi)
- [Safety Notes](#en-safety)
- [License](#en-license)
- [Author](#en-autore)

#### Graphic Interface

| Page | Content |
|---|---|
| **Page 0 — Lean** | Display of the lateral lean angle with a dynamic graphic bar and danger alert |
| **Page 1 — Weather** | Monitoring of temperature, air humidity and grip risk percentage |
| **Page 2 — G-Force** | 2D graphic radar with vector tracking of accelerations |
| **Page 3 — Wheelie/Stoppie** | Monitoring of the pitch angle with a vertical graphic indicator |
| **Page 4 — Records** | All-time maximums across every session: lean, wheelie, stoppie and G-forces |
| **Page 5 — Memory** | Free space on the internal flash, current file name and minutes logged |
| **Page 6 — WiFi** | Access point toggle, with SSID, password and website address |

<a id="en-architecture"></a>
### Hardware Architecture

The core of the project is an **ESP32 DevKit V1** microcontroller (30 or 38 pin).

#### Wiring Diagram (Pinout)

All modules communicate with the ESP32 via the standard I2C and SPI buses, or dedicated digital pins:

| Component | Bus / Signal | ESP32 Pin | Notes |
|---|---|---|---|
| OLED SSD1306 | I2C (SDA) | GPIO 21 | Shared with BNO055 |
| OLED SSD1306 | I2C (SCL) | GPIO 22 | Shared with BNO055 |
| Bosch BNO055 | I2C (SDA) | GPIO 21 | Standard I2C address: 0x28 |
| Bosch BNO055 | I2C (SCL) | GPIO 22 | Standard I2C address: 0x28 |
| DHT22 | Digital GPIO | GPIO 4 | Single data line |
| UP Button | Digital GPIO | GPIO 13 | Configured as INPUT_PULLUP |
| DOWN Button | Digital GPIO | GPIO 25 | Configured as INPUT_PULLUP |
| OK Button | Digital GPIO | GPIO 14 | Configured as INPUT_PULLUP |
| LOG Button | Digital GPIO | GPIO 27 | Configured as INPUT_PULLUP |

<a id="en-bom"></a>
### Bill of Materials

| Component | Quantity | Notes |
|---|---|---|
| ESP32 DevKit V1 (30/38 pin) | 1 | Main microcontroller |
| Bosch BNO055 (breakout) | 1 | 9-axis IMU, hardware sensor fusion |
| DHT22 | 1 | Temperature/humidity sensor |
| OLED SSD1306 Display (I2C, 128x64) | 1 | Graphic interface |
| Tactile buttons | 4 | UP / DOWN / OK / LOG (integrated in the display module) |
| Jumper / dupont wires | as needed | Connections |
| Power supply | 1 | USB-C power supply |

<a id="en-diagrams"></a>
### Project Diagrams and Images

#### Circuit Diagram

![Circuit Diagram](Media/schemacircuitale.png)

#### Topographic Diagram

![Topographic Diagram](Media/schemaTopografico.png)


#### OLED Interface

![OLED Interface](Media/interfacciaOled.png)
![OLED Interface](Media/interfacciaOled1.png)
![OLED Interface](Media/interfacciaOled2.png)
![OLED Interface](Media/InterfacciaOled3.png)
![OLED Interface](Media/InterfacciaOled4.png)

<a id="en-how-it-works"></a>
### How Grip Risk Works

The grip risk index is an estimate, not a direct measurement of traction. The firmware cross-references the temperature and humidity detected by the DHT22 to assess potentially unfavorable conditions (e.g. cold or damp asphalt) and returns an indicative risk percentage, shown on the display along with any visual alerts. It does not replace the rider's direct assessment of the actual road surface conditions.

<a id="en-libraries"></a>
### 📦 Required Libraries

To correctly compile the firmware in Arduino IDE make sure you have installed the following libraries:

- `Adafruit BNO055` (by Adafruit)
- `Adafruit SSD1306` (by Adafruit)
- `Adafruit GFX Library` (by Adafruit)
- `DHT sensor library` (by Adafruit)
- `Adafruit Unified Sensor` (required as a common dependency)

<a id="en-csv"></a>
### Log Data Structure (CSV)

Log files are saved sequentially in the internal flash memory (LittleFS) with the naming `LOG_1.CSV`, `LOG_2.CSV`, etc. Every minute a row is written with the minute's maximum values (angles, G-forces, weather and grip risk); when the session is stopped, a summary is appended with the overall maximums and the count of over-threshold events (wheelies, stoppies, deep leans, hard braking and hard acceleration). Files can be downloaded and deleted through the web interface.

<a id="en-record"></a>
### All-Time Records

At the end of each recording, the session maximums update the all-time records, stored in a file on the flash that survives power-off. They can be viewed on the **Records** OLED page and on the website, where they can also be reset (with confirmation). In the end-of-session summary, an asterisk marks the values that have just beaten a record.

<a id="en-wifi"></a>
### WiFi and Web Interface

From the **WiFi** page (OK button) the ESP32 starts a dedicated access point: connect your phone to the `WheelStat` network (credentials shown on the display) and open `http://192.168.4.1` to access live telemetry with charts, CSV download and deletion, and the all-time records. No internet needed: everything is served by the ESP32.

<a id="en-safety"></a>
### ⚠️ Safety Notes

WheelStat is designed as a post-session analysis tool, not as a real-time riding aid. Do not look at the display while riding: keeping your eyes on the road/track always has absolute priority.
The display should be reviewed after the session by recording it with an external camera, in order to get precise feedback at the desired moment.
The calculated grip risk is indicative and does not replace the rider's experience or their direct assessment of asphalt and tire conditions. Always make sure the hardware mounting on the vehicle is solid and does not interfere with controls or visibility.

<a id="en-license"></a>
### 📄 License

This project is distributed under the **Apache License 2.0**. See the [LICENSE](LICENSE) file.

<a id="en-autore"></a>
### Author

Designed and developed by **Alessandro Rota** with the indispensable support of (lots of) caffeine.

