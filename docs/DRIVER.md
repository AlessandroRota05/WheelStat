# Come funzionano i driver

Come il firmware trova i componenti, come sono organizzati i driver e come se ne aggiunge uno. Per la lista di cosa è compatibile vedi [HARDWARE.md](HARDWARE.md); per il resto del codice [ARCHITETTURA.md](ARCHITETTURA.md).

*English version: [en/DRIVERS.md](en/DRIVERS.md).*

## L'idea

**Ogni componente che si può riconoscere, si riconosce all'avvio.** Per ogni categoria c'è un elenco di driver; all'accensione il firmware li prova in ordine e usa il primo che risponde. Il resto del firmware chiama sempre le stesse funzioni (`weatherSensor::read()`, `imu::read()`...) senza sapere quale chip c'è dietro.

```
src/drivers/
  I2cBus.*            le poche transazioni I2C che servono a tutti
  WeatherSensor.*     il contratto del sensore meteo e l'elenco dei candidati
  weather/            un file per famiglia di chip: Sensirion, Aht, Hdc1080,
                      Si70xx, Htu31, Am2320, Bme280, Dht
  Imu.*               il contratto dell'IMU e l'elenco
  imu/                Bno055, Bno08x, Mpu6050, Icm20948, Lsm6ds
  GpsModule.*         velocità, famiglia e frequenza del GPS
  gps/                Ublox, OtherFamilies (CASIC e Airoha)
  Display.*           il pannello OLED
```

Un **file per famiglia**: aggiungere un chip vuol dire aggiungere un file, senza toccare gli altri. La famiglia è l'unità giusta perché i chip di uno stesso produttore spesso condividono protocollo e registri (SHT4x, SHT3x e SHTC3 parlano allo stesso modo; MPU6050, MPU6500 e ICM-20602 hanno gli stessi registri).

## Il contratto

Ogni file di famiglia definisce una sola struttura `Driver` con tre o quattro puntatori a funzione:

```cpp
// src/drivers/weather/WeatherDrivers.h
struct Driver {
  const char *(*detect)();                           // nome se c'è, nullptr se no
  bool (*read)(float &temperature, float &humidity);
  uint32_t intervalMs;                                // ogni quanto leggerlo
};
```

`detect()` cerca il chip, lo prepara e restituisce il nome da mostrare all'avvio. Il file della categoria (`WeatherSensor.cpp`, `Imu.cpp`) tiene l'elenco dei driver e chiama `detect()` su ognuno finché uno risponde. Le funzioni interne di ogni file sono `static`: fuori dal file esiste solo la sua `Driver`.

Le regole che la firma da sola non dice (sono anche negli header):

1. **`detect()` deve riconoscere il chip, non solo un indirizzo che risponde.** Un ACK all'indirizzo `0x40` può venire da quattro chip diversi: serve un registro d'identità, o una risposta con un CRC che torna.
2. **`weatherSensor::read()` riceve i parametri già pieni** con gli ultimi valori buoni e sovrascrive solo ciò che ha letto davvero.
3. **I sensori meteo leggono in due tempi**: ogni chiamata raccoglie la misura avviata dalla chiamata precedente e ne avvia una nuova. Nessun driver aspetta la conversione (fino a 80 ms sull'AHT20) bloccando il ciclo che legge l'IMU.
4. **`imu::read()` consegna gli angoli della moto** (`Kind::Angles`, solo il BNO055) **o i dati grezzi negli assi del chip** (`Kind::Raw`): accelerometro in m/s² con la gravità, giroscopio in rad/s già senza lo zero. Assi e filtro d'assetto li fa `src/Motion`, una volta per tutti.
5. **`name()`** compare nella schermata d'avvio: al massimo 11 caratteri.

## Il riconoscimento

L'ordine conta dove più chip condividono un indirizzo:

| Indirizzo | Chi c'è | Come si distinguono |
|---|---|---|
| `0x40` | HDC1080, HTU31D, Si7021/HTU21D | HDC1080: registro produttore `0x5449`. HTU31D: numero di serie con CRC. Si70xx: registro utente, poi l'ID elettronico (solo il Si7021 ce l'ha) per il nome. Provati in quest'ordine, dal test più sicuro al più debole |
| `0x44` | SHT4x, SHT3x | Si chiede il numero di serie con il comando dell'SHT4x: solo un SHT4x risponde con due parole il cui CRC torna |
| `0x68` / `0x69` | ICM-20948, famiglia MPU | ICM-20948: registro 0 = `0xEA`. MPU: registro `0x75` |
| `0x76` / `0x77` | BME280, BMP280 | Chip-ID `0x60` o `0x58`. Il BMP280 non ha umidità: viene riconosciuto e spiegato sul monitor seriale, non usato |

Il **DHT** è provato per ultimo: non è sul bus, e l'unico modo di sapere se c'è è leggerlo (circa un secondo quando manca).

Per il **GPS** il riconoscimento è diverso perché non c'è un indirizzo: si prova a leggere NMEA alle velocità più comuni, poi si chiede la versione col protocollo u-blox, e i comandi delle altre famiglie si mandano tutti (ognuna ignora quelli che non conosce). Infine si **misura** la frequenza ottenuta: è l'unico numero che conta, qualunque cosa il modulo abbia fatto coi comandi.

## Perché senza librerie

I sensori meteo e le IMU senza fusione sono scritti **senza libreria**: qualche decina di righe ciascuno, dal datasheet. Tre motivi:

- **Flash**: per riconoscerli bisogna averli tutti nel firmware. Una libreria per chip (con le sue dipendenze) costerebbe decine di KB; il C3 è già al 95%.
- **Controllo**: le letture in due tempi, che non bloccano il ciclo, non le offre nessuna delle librerie di questi sensori.
- **Verifica**: la parte che può sbagliare in silenzio, cioè conversioni, checksum e assetto, sta in `src/core/` (`SensorMath`, `Ahrs`, `GpsCommands`) ed è provata sul PC contro gli esempi dei datasheet.

Restano su libreria i chip dove scriverlo a mano non conviene: **BNO055** e **BNO085** (protocolli complessi), **DHT** (temporizzazioni al microsecondo), i **display** (Adafruit GFX disegna le pagine) e il parser **NMEA** (TinyGPS++).

## Le scelte rimaste in `Config.h`

Solo ciò che non si può chiedere al componente:

| Macro | Perché non si riconosce |
|---|---|
| `DISPLAY_SSD1306` · `SH1106` · `SSD1309` · `SH1107` | Controller diversi rispondono allo stesso indirizzo e non hanno un registro d'identità leggibile |
| `DISPLAY_SPI` | Su SPI non c'è acknowledge: un pannello assente non si nota |
| `IMU_WITH_BNO055` · `IMU_WITH_BNO08X` | Si riconoscono, ma le loro librerie costano flash: si includono solo se servono |
| `SD_CARD` / `NO_SD_CARD` | Come sopra: il supporto FAT costa ~50 KB |
| `IMU_MOUNT_*`, `IMU_AXES`, `SIGN_*` | Come è montata la scatola: nessun sensore lo sa |
| `POWER_SENSE_PIN` | Un partitore non ha niente da rispondere |

## Aggiungere un sensore meteo

Esempio: un Sensirion SHT85, che parla come l'SHT3x ma risponde solo a `0x44`.

**1. Un file** `src/drivers/weather/Sht85.cpp` (o un caso in più in `Sensirion.cpp`, se il protocollo è lo stesso), copiando la famiglia più simile:

```cpp
namespace weatherSensor {
static const char *detect() { /* identità del chip */ }
static bool read(float &temperature, float &humidity) { /* due tempi */ }
const Driver SHT85 = { detect, read, 1000 };
}
```

**2. La conversione** in `src/core/SensorMath`, con un test in `test/test_core/` che usi l'esempio del datasheet.

**3. Una riga** in `WeatherDrivers.h` (`extern const Driver SHT85;`) e una nell'elenco di `WeatherSensor.cpp`, al posto giusto rispetto ai chip con lo stesso indirizzo.

**4. La verifica**: `pio test -e native` e la compilazione su tutte le schede.

Per un'IMU è lo stesso in `src/drivers/imu/`: `detect()` imposta il chip e misura lo zero del giroscopio con `measureGyroBias()`, `read()` consegna `Kind::Raw`.

### La matrice di compilazione

Un driver si può verificare senza il componente solo compilandolo. Ogni versione viene compilata nelle combinazioni che cambiano il codice compilato:

| Cosa varia | Valori |
|---|---|
| Scheda | `esp32doit-devkit-v1` · `esp32s3` · `esp32s2` · `esp32c3` |
| Lingua | `LANG_IT` · `LANG_EN` |
| Display | `DISPLAY_SH1106`, `DISPLAY_SPI` e la loro combinazione |
| IMU con libreria | `IMU_WITH_BNO08X` |
| microSD | `NO_SD_CARD` |
| Altro | `POWER_SENSE_PIN`, `IMU_MOUNT_UPRIGHT` |

I sensori meteo e le IMU senza libreria non hanno combinazioni: sono sempre tutti compilati.

## Errori tipici

| Errore | Causa quasi certa |
|---|---|
| `undefined reference to 'weatherSensor::XYZ'` | Il driver è nell'elenco ma il suo file non definisce la `Driver` con quel nome |
| `'X' was not declared in this scope` in un modulo | Manca l'`#include` dell'header che lo dichiara |
| `#error "Config.h: no display controller selected"` | Hai commentato tutte le macro `DISPLAY_*` |
| `#error "Config.h: the C3 has no free pins..."` | microSD o display SPI attivati su un C3 |
| `#error "Config.h: select only one language"` | `LANG_IT` e `LANG_EN` accesi insieme |
| `'TR_...' was not declared` | Una chiave di testo esiste in un file lingua ma non nell'altro |
| Componente presente ma riga con la croce all'avvio | Indirizzo occupato da un altro chip, o cablaggio: il monitor seriale dice cosa è stato trovato |
