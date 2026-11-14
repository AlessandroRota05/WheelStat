# Come funzionano i driver

Cosa succede quando cambi una riga in `Config.h`, e come aggiungere un componente nuovo. Per la lista di cosa è compatibile vedi [HARDWARE.md](HARDWARE.md); per il resto del codice [ARCHITETTURA.md](ARCHITETTURA.md).

## L'idea

**Il preprocessore cancella il driver che non hai scelto prima che il compilatore lo veda.** Nessuna scelta a runtime, nessuna classe virtuale: nel programma finito esiste una sola `weatherSensor::read()`, e il resto del firmware la chiama senza sapere per quale chip è stata compilata.

Ogni categoria di componente è una coppia di file in `src/drivers/`:

- un **`.h`** che dichiara cosa il driver offre: è il *contratto*, identico per tutti i chip della categoria;
- un **`.cpp`** che lo implementa, con un blocco `#if` per chip.

Chi usa un driver include solo il `.h`. L'oggetto della libreria, i suoi `#include` e ogni dettaglio del chip restano chiusi nel `.cpp`.

## Come si sceglie

`Config.h` accende una macro per categoria:

```cpp
#if !defined(WEATHER_DHT22) && !defined(WEATHER_SHT31)
  #define WEATHER_DHT22
// #define WEATHER_SHT31
#endif
```

Il `#if !defined(...)` attorno serve a un caso solo: se la macro arriva già dalla riga di comando (`-DWEATHER_SHT31`), qui non se ne definisce nessuna e vince quella. Senza, si accenderebbero due driver insieme.

Nel `.cpp` sopravvive un solo ramo:

```cpp
#if defined(WEATHER_DHT22)
  #include <DHT.h>
  static DHT sensor(PIN_DHT, DHT22);
  #define WEATHER_NAME "DHT22"

#elif defined(WEATHER_SHT4X)
  #include <Adafruit_SHT4x.h>
  static Adafruit_SHT4x sensor;
  #define WEATHER_NAME "SHT4x"
  ...
#endif
```

Il codice del sensore non scelto non viene compilato e la sua libreria non viene nemmeno inclusa.

**Perché l'oggetto si chiama sempre `sensor`.** Con lo stesso nome in ogni ramo, `read()` si scrive una volta per famiglia di API invece che una per chip: i sensori con `readTemperature()` / `readHumidity()` condividono un corpo, quelli con l'API *unified sensor* di Adafruit un altro. Otto sensori, due funzioni di lettura.

`Config.h` non include nessuna libreria: essendo incluso ovunque, si tirerebbe dietro quella del display in ogni modulo.

## Le categorie

| Categoria | Macro | File | Contratto |
|---|---|---|---|
| Meteo | `WEATHER_DHT22` · `SHT4X` · `SHT31` · `HTU31D` · `SI7021` · `HTU21D` · `AHT20` · `BME280` | `WeatherSensor` | `weatherSensor::begin`, `read`, `name` |
| Display | `DISPLAY_SSD1306` · `SSD1309` · `SH1106` · `SH1107` | `Display` | `displayBegin`, `displayName`, l'oggetto `display` e il tipo `DisplayDriver` |
| IMU | `IMU_BNO055` | `Imu` | `imu::begin`, `read`, `calibrationLevel`, `printCalibration`, `name` |
| GPS | `GPS_UBLOX` · `GPS_UBLOX_M10` · `GPS_NMEA_GENERIC` | `GpsModule` | `gpsModule::configure`, `name`, l'oggetto `gpsSerial` |

Il contratto contiene **solo ciò che è specifico del chip**. Il disegno delle pagine sono primitive Adafruit_GFX uguali su tutti i pannelli; la lettura del GPS è NMEA standard; il filtro anti-buca non riguarda l'IMU. Tutto questo sta nel codice comune.

Quattro regole che la firma da sola non dice (sono anche negli header):

1. **`name()`** serve alla schermata di avvio. Lo dichiara il driver, così cambiando macro la scritta cambia da sola. Al massimo 11 caratteri.
2. **`weatherSensor::read()` riceve i parametri già pieni** con gli ultimi valori buoni e sovrascrive solo ciò che ha letto davvero: un sensore può dare una grandezza valida e l'altra no.
3. **`imu::read()` consegna quattro grandezze riferite alla moto, non al chip**: piega positiva a destra, beccheggio positivo col muso su, accelerazione positiva in avanti e a destra. Tradurre dagli assi del sensore è il lavoro del driver. I `SIGN_*` invertono un verso; scambiare due assi non è un'inversione di segno e va risolto nel driver.
4. **`DisplayDriver` deve esporre `getBuffer()`**: lo usa la transizione fra pagine.

Due macro possono condividere lo stesso ramo: `DISPLAY_SSD1309` usa il blocco dell'SSD1306, perché quel controller si pilota con la stessa libreria. La macro separata esiste solo per scrivere in `Config.h` che pannello c'è davvero.

Restano in `Config.h`, e non nel driver, le scelte di chi monta la scatola: le macro, pin e indirizzi I2C, `WEATHER_INTERVAL_MS` (limite fisico del sensore), `GPS_RATE_HZ` e `GPS_FIX_TIMEOUT_MS`, `IMU_MOUNT_*` e `SIGN_*`.

## Aggiungere un driver

Esempio: un SHT85 fra i sensori meteo.

**1. Un ramo** in `src/drivers/WeatherSensor.cpp`, copiando quello del sensore più simile:

```cpp
#elif defined(WEATHER_SHT85)
  #include <Adafruit_SHT31.h>      // stessa libreria dello SHT31
  static Adafruit_SHT31 sensor;
  #define WEATHER_NAME "SHT85"
```

**2. Se l'accensione è diversa**, il suo caso in `begin()`. Se la lettura usa una delle due API già presenti, `read()` non si tocca.

**3. La macro** in `Config.h`, in due punti: la catena di scelta e il controllo in fondo al file (*SANITY CHECKS*), che dà un errore leggibile se le commenti tutte. Se serve, anche il suo indirizzo I2C.

**4. La libreria** in `lib_deps` di `platformio.ini`.

**5. La verifica**, di tutte le combinazioni e non solo della tua:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32doit-devkit-v1 --build-property "compiler.cpp.extra_flags=-DWEATHER_SHT85" .
```

### La matrice di compilazione

Un driver si può verificare senza il componente solo compilandolo: se una combinazione non compila, la modularità è solo apparente. Ogni versione viene compilata in venti combinazioni:

| Cosa varia | Valori |
|---|---|
| Scheda | `esp32doit-devkit-v1` · `esp32s3` · `esp32s2` · `esp32c3` |
| Lingua | `LANG_IT` · `LANG_EN` |
| Meteo | le otto macro `WEATHER_*` |
| Display | le quattro macro `DISPLAY_*` |
| GPS | le tre macro `GPS_*` |
| Montaggio IMU | `IMU_MOUNT_UPSIDE_DOWN` · `IMU_MOUNT_UPRIGHT` · `IMU_MOUNT_MANUAL` |

Compilare nelle due lingue controlla anche che i due file di testi definiscano le stesse chiavi.

### Senza il sensore e senza la sua libreria

Il ramo di un sensore che non hai non compila per un motivo banale: la libreria non è installata. Invece di installarla basta uno **stub header** con le sole firme usate, tenuto fuori dal progetto e passato con `-I`:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32doit-devkit-v1 --build-property "compiler.cpp.extra_flags=-DWEATHER_SHT85 -I/percorso/stub" .
```

Verifica ciò che conta: che le macro escludano il driver giusto, che non ci siano doppie definizioni, che il contratto sia rispettato. Il binario risulta più piccolo, ma solo perché lo stub è vuoto.

## Errori tipici

| Errore | Causa quasi certa |
|---|---|
| `redefinition of 'sensor'` o `'WEATHER_NAME' redefined` | Due driver accesi insieme |
| `undefined reference to 'weatherSensor::begin()'` | Nessun ramo compilato: il nome della macro nel `.cpp` non combacia con quello in `Config.h` |
| `'sensor' was not declared in this scope` | Come sopra |
| `'X' was not declared in this scope` in un modulo | Manca l'`#include` dell'header che lo dichiara |
| `#error "Config.h: no ... driver selected"` | Hai commentato tutte le alternative di una categoria |
| `#error "Config.h: select only one language"` | `LANG_IT` e `LANG_EN` accesi insieme |
| `'TR_...' was not declared` | Una chiave di testo esiste in un file lingua ma non nell'altro |

## Perché non classi ed ereditarietà

Una classe base astratta con una sottoclasse per chip sarebbe la soluzione da manuale. È stata scartata di proposito: il sensore non cambia mentre la moto va, quindi la scelta è nota a tempo di compilazione e risolverla lì costa zero byte. Con le classi finirebbero nel binario tutti i driver con le loro librerie, più una vtable, per un beneficio nullo.
