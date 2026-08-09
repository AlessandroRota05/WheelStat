# Come funzionano i driver

Spiegazione di cosa succede davvero quando cambi una riga in `Config.h`. Non serve conoscere niente oltre al C.

---

## L'idea in una frase

**Il preprocessore cancella il driver che non hai scelto prima ancora che il compilatore lo veda.** Non c'è nessuna scelta a runtime, nessun puntatore a funzione, nessuna classe: nel programma finito esiste *una sola* `meteoLeggi()`, e il resto del firmware la chiama senza sapere da quale file venga.

---

## Passo 1: alla fine è tutto un file solo

Questa è la cosa da capire per prima, perché tutto il resto ne discende.

Arduino **non** compila i `.ino` separatamente: li **concatena in un unico** `.cpp` e compila quello. Prima lo sketch principale, poi gli altri in ordine alfabetico:

```
WheelStat.ino          <- sempre per primo
InterfacciaWeb.ino     <- poi gli altri, in ordine alfabetico
PagineOled.ino
```

I **driver non sono in questa lista**: sono header, inclusi in cima a `WheelStat.ino`.

```cpp
#include "Config.h"
#include "src/driver/Display.h"
#include "src/driver/Gps.h"
#include "src/driver/Imu.h"
#include "src/driver/Meteo.h"
```

Il risultato è lo stesso (finiscono nello stesso unico file compilato) ma con due differenze pratiche:

- **possono stare in una sottocartella.** Arduino pretende i `.ino` nella radice dello sketch e ignora le sottocartelle; gli header no. È l'unico motivo per cui i driver sono `.h`;
- **arrivano per primi**, prima di qualunque riga di `WheelStat.ino`. Quindi un driver non può usare niente definito lì, ed è un vincolo che si vuole: un driver che dipende dal resto del firmware non è sostituibile davvero.

Da qui discendono le due conseguenze che tornano di continuo:

- **le funzioni si vedono fra file senza dichiarare niente**: l'IDE genera in cima i prototipi di tutte le funzioni che trova nei `.ino`, quindi `WheelStat.ino` può chiamare `aggiornaDisplay()` anche se è definita in `PagineOled.ino`, che viene dopo;
- **le variabili globali no**: per quelle vale la regola normale del C: vanno dichiarate prima di essere usate. È il motivo di `extern DisplayDriver display;` e `extern HardwareSerial gpsSerial;` in `Config.h` (due oggetti che vivono nei driver ma servono anche fuori) e il motivo per cui `INTERVALLO_METEO` sta in `Config.h` invece che nel driver.

## Passo 2: `Config.h` accende un interruttore

`Config.h` è incluso in cima a `WheelStat.ino`, cioè in cima al file concatenato. Contiene:

```cpp
#if !defined(METEO_DHT22) && !defined(METEO_SHT31)
  #define METEO_DHT22
// #define METEO_SHT31
#endif
```

`#define METEO_DHT22` non è una variabile: è un **nome definito per il preprocessore**, che vale per tutto il resto della compilazione. Non occupa memoria e non esiste a runtime.

Il `#if !defined(...)` attorno serve a un caso solo: se la macro arriva già definita dalla riga di comando (`-DMETEO_SHT31`, per la matrice di compilazione), qui non se ne definisce nessuna e vince quella. Senza, si accenderebbero **due** driver insieme.

## Passo 3: i blocchi non scelti si cancellano da soli

C'è **un file per categoria** (`src/driver/Meteo.h`, `Display.h`, `Gps.h`, `Imu.h`), e dentro un blocco per chip:

```cpp
#include "../../Config.h"   // per sapere quali macro sono accese

#if defined(METEO_DHT22)
  #include "DHT.h"
  DHT sensore(PIN_DHT, DHT22);
  #define METEO_NOME "DHT22"

#elif defined(METEO_SHT4X)
  #include <Adafruit_SHT4x.h>
  Adafruit_SHT4x sensore;
  #define METEO_NOME "SHT4x"
  ...
#endif
```

Il `#include` di `Config.h` in cima non è una formalità: essendo un header e non un `.ino`, il driver non riceve niente "dalla concatenazione": se non include `Config.h` non conosce né le macro né i pin.

Il preprocessore **elimina tutti i rami tranne uno** prima che il compilatore veda qualcosa. Il codice del sensore che non hai scelto non viene compilato, non finisce nel binario, non esiste, e nemmeno la sua libreria viene inclusa.

### Cosa vede il compilatore, concretamente

Con `METEO_DHT22` acceso, dopo il preprocessore il file concatenato contiene:

```cpp
// ... da WheelStat.ino ...
void leggiMeteo() {
  float t = temperatura;
  float u = umidita;
  if (meteoOk && meteoLeggi(t, u)) { temperatura = t; umidita = u; }
  calcolaRischioGrip();
}

// ... da src/driver/Meteo.h: SOLO il ramo DHT22 ...
DHT sensore(PIN_DHT, DHT22);
bool meteoInit()  { sensore.begin(); /* poi prova a leggerlo davvero */ }
bool meteoLeggi(float &temp, float &umid) { ... }
const __FlashStringHelper *meteoNome() { return F("DHT22"); }
```

Una sola `meteoLeggi()`. `leggiMeteo()` la chiama e basta. Scambiando la macro, l'unica cosa che cambia è **quale** ramo sopravvive: `leggiMeteo()` non cambia di un carattere.

### Perché l'oggetto si chiama sempre `sensore`

È un trucco che vale la pena notare, perché è ciò che tiene corto il file. Chiamando l'oggetto con lo stesso nome in ogni ramo, `meteoLeggi()` si scrive **una volta per famiglia di API** invece di una per chip:

- i sensori con `readTemperature()` / `readHumidity()` (DHT22, SHT31, Si7021, HTU21D, BME280) condividono un corpo solo;
- quelli con l'API *unified sensor* di Adafruit (SHT4x, HTU31D, AHT20) ne condividono un altro, selezionato da `METEO_API_EVENTI`.

Otto sensori, due funzioni di lettura.

---

## Il contratto: cos'è e perché sta in `Config.h`

Il **contratto** è l'elenco delle funzioni che ogni driver di una categoria deve fornire, con firma identica. Per il meteo:

```cpp
bool meteoInit();
bool meteoLeggi(float &temp, float &umid);
```

Non è imposto dal linguaggio: è una convenzione. Se scrivi un driver che non la rispetta, non compila, ed è esattamente il controllo che vuoi.

I contratti sono **scritti nei commenti di `Config.h`** e non nei file driver per una ragione pratica: quando ne scrivi uno nuovo, devi leggere una pagina sola. E `Config.h` è anche il posto dove stanno i pezzi del contratto che non sono funzioni, perché devono essere visibili a *tutti* i file:

| Cosa | Perché non può stare nel driver |
|---|---|
| `INTERVALLO_METEO` | Lo usa `loop()`, che sta in `WheelStat.ino`, e i driver non possono definire niente che serva li' |
| `extern DisplayDriver display` | L'oggetto lo usano `WheelStat.ino` e `PagineOled.ino`, entrambi fuori dal driver |
| `COLORE_ON` / `COLORE_OFF` | Usati in ~180 punti di `PagineOled.ino` |
| `typedef ... DisplayDriver` | Serve per poter scrivere l'`extern` qui sopra |

Tutto il resto (l'oggetto `dht`, l'oggetto `bno`, gli `#include` delle librerie) vive **dentro** al driver e non è visibile da nessun'altra parte. È questo che rende il componente sostituibile.

---

## Le quattro categorie di oggi

| Categoria | Driver disponibili | Contratto |
|---|---|---|
| Meteo | `METEO_DHT22` · `SHT4X` · `SHT31` · `HTU31D` · `SI7021` · `HTU21D` · `AHT20` · `BME280` | `meteoInit`, `meteoLeggi`, `meteoNome` |
| Display | `DISPLAY_SSD1306` · `SSD1309` · `SH1106` · `SH1107` | `displayInit`, `displayNome` + oggetto `display` |
| IMU | `IMU_BNO055` | `imuInit`, `imuLeggi`, `imuCalibrazione`, `imuStampaCalibrazione`, `imuNome` |
| GPS | `GPS_UBLOX` · `GPS_UBLOX_M10` · `GPS_NMEA_GENERICO` | `gpsConfigura`, `gpsNome` |

**`xNome()` c'è in tutte e quattro le categorie** e serve alla schermata di avvio, che elenca i componenti mentre li inizializza. Il nome lo dichiara il driver invece di stare in una tabella altrove, così non può restare indietro: cambi macro in `Config.h` e la scritta a schermo cambia da sola. È il caso limite utile del contratto: anche una stringa costante ha senso metterci dentro, se il punto è che non si disallinei.

Due macro possono anche condividere lo **stesso ramo**: `DISPLAY_SSD1309` finisce nello stesso blocco dell'SSD1306, perché quel controller si pilota con la stessa libreria. La macro separata esiste solo per poter scrivere in `Config.h` che pannello c'è davvero, invece di mentire nel commento.

Nota che il contratto contiene sempre **solo ciò che è specifico del chip**. Tutto il resto resta nel codice comune:

- il **disegno** delle 10 pagine non è nel contratto del display: sono primitive Adafruit_GFX, uguali su tutti i pannelli. Solo la sequenza di accensione cambia;
- la **lettura** del GPS non è nel contratto: è NMEA standard, la fa `leggiGPS()`. Solo la configurazione iniziale cambia;
- il **filtro anti-buca** e i canali non sono nel contratto dell'IMU: il driver consegna quattro numeri, cosa farne è affare del firmware.

È il motivo per cui i quattro file driver stanno in poche centinaia di righe in tutto, per 16 componenti.

---

## Aggiungere un driver: la procedura

Esempio: un SHT85 in più fra i sensori meteo.

**1. Aggiungi un ramo** in `src/driver/Meteo.h`, copiando quello del sensore più simile:

```cpp
#elif defined(METEO_SHT85)
  #include <Adafruit_SHT31.h>      // stessa libreria dello SHT31
  Adafruit_SHT31 sensore;
  #define METEO_NOME "SHT85"
```

**2. Se l'accensione è diversa**, aggiungi il suo caso in `meteoInit()`. Se la lettura usa una delle due API già presenti, `meteoLeggi()` non si tocca: è il vantaggio di chiamare l'oggetto `sensore` in ogni ramo.

**3. Aggiungi la macro alla catena** in `Config.h`:

```cpp
#if !defined(METEO_DHT22) && ... && !defined(METEO_SHT85)
  #define METEO_DHT22
// #define METEO_SHT85
#endif
```

e, se serve, il suo indirizzo I2C e la sua cadenza nei blocchi `#if defined(METEO_...)` più sotto.

**4. Verifica tutte le combinazioni**, non solo la tua:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32doit-devkit-v1 --build-property "compiler.cpp.extra_flags=-DMETEO_SHT85" .
```

Se una combinazione non compila, la modularità è solo apparente. La matrice di compilazione è la suite di test di questo progetto: qui i test unitari non esistono, e questo è il criterio oggettivo che li sostituisce.

Perché il `-D` funzioni, la scelta in `Config.h` sta dentro un `#if !defined(...)`: se la macro arriva da fuori, l'header non ne definisce nessuna. Senza quel guard il `-D` accenderebbe **due** driver insieme, e l'errore sarebbe una doppia definizione di `meteoInit()`, molto meno leggibile della causa vera.

### Verificare un driver senza avere il sensore (né la sua libreria)

Il ramo di un sensore che non hai non compila per un motivo banale: la sua libreria non è installata. Invece di installarne una per un chip che non possiedi, basta uno **stub header** con le sole firme usate (`begin`, `readTemperature`, `readHumidity`), tenuto **fuori** dal progetto e passato con `-I`:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32doit-devkit-v1 --build-property "compiler.cpp.extra_flags=-DMETEO_SHT85 -I/percorso/stub" .
```

Verifica tutto ciò che conta davvero: che le macro escludano il driver giusto, che non ci siano doppie definizioni, che il contratto sia rispettato, senza toccare l'ambiente Arduino. Quando il sensore arriva, si installa la libreria vera e si rifà la stessa compilazione senza `-I`.

Attenzione a un'illusione: col driver stub il binario risulta più piccolo, ma **non** perché quel sensore "costi meno". Lo stub è vuoto, quindi quel numero misura solo ciò che è stato tolto.

---

## Errori tipici e cosa vogliono dire

| Errore | Causa quasi certa |
|---|---|
| `redefinition of 'bool meteoInit()'` | Due driver accesi insieme: hai scommentato il nuovo senza commentare il vecchio |
| `undefined reference to 'meteoInit()'` | Nessun driver acceso, o il nome della macro nel `#ifdef` del file non combacia con quello in `Config.h` |
| `'display' was not declared in this scope` | Manca l'`extern` in `Config.h`, oppure stai usando una **variabile** globale definita in un file che viene concatenato dopo |
| `'X' has not been declared` su una struct | Struct usata in una firma ma definita troppo in basso: i prototipi automatici non la vedono. Va prima di ogni funzione, o in `Config.h` |
| `default argument given for parameter 2` | Parametro con valore di default in un `.ino`: il prototipo automatico lo duplica. Usa sovrapposizioni esplicite |
| `#error "Config.h: nessun driver ... selezionato"` | Hai commentato tutte le alternative di una categoria |

---

## Perché non classi C++ ed ereditarietà

Sarebbe la soluzione da manuale: una classe base astratta `SensoreMeteo`, una sottoclasse per chip, un puntatore. È stata scartata di proposito:

- il progetto è costruito su funzioni libere e variabili globali condivise fra `.ino` concatenati: una gerarchia di classi sarebbe un secondo stile di progetto dentro lo stesso firmware;
- il polimorfismo a runtime qui non serve a niente: **il sensore non cambia mentre la moto va**. La scelta è nota a tempo di compilazione, e risolverla lì costa zero byte;
- una classe con metodi virtuali aggiunge la vtable e impedisce al compilatore di ottimizzare le chiamate, per un beneficio nullo con due driver per categoria;
- con le macro, il driver non selezionato **non esiste proprio** nel binario. Con le classi ci sarebbero entrambi.
