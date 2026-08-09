#pragma once

/*
  WheelStat - src/driver/Meteo.h
  ===========================================================================
  Tutti i driver del sensore meteo, uno per blocco #if. Ne viene compilato
  uno solo, quello scelto in Config.h; gli altri il preprocessore li
  cancella. Il meccanismo e' spiegato in docs/DRIVER.md.

  Contratto: meteoInit(), meteoLeggi(), meteoNome() - vedi Config.h.

  I sensori si dividono in due famiglie per come si leggono, e questo
  spiega la struttura del file:
    A) due funzioni che ritornano float e NaN se falliscono
       (DHT22, SHT31, Si7021, HTU21D, BME280)
    B) API "unified sensor" di Adafruit: una chiamata riempie due
       strutture evento (SHT4x, HTU31D, AHT20)
  Chiamando l'oggetto "sensore" in ogni blocco, meteoLeggi() e'
  scritta una volta per famiglia invece che una per chip.

  meteoInit() ritorna false quando il sensore non c'e', per tutti e otto:
  quelli I2C perche' non rispondono all'indirizzo, il DHT22 perche' viene
  letto davvero e consegna NaN.

  Solo il DHT22 e' provato su hardware. Gli altri compilano e rispettano
  il contratto: da verificare al primo montaggio (indirizzo I2C in testa).
  ===========================================================================
*/

#include "../../Config.h"

// ---------------------------------------------------------------------------
// Oggetto del sensore + nome mostrato all'avvio. Un blocco per chip: qui
// sta tutto cio' che cambia da un sensore all'altro.
// ---------------------------------------------------------------------------

#if defined(METEO_DHT22)
  #include "DHT.h"
  DHT sensore(PIN_DHT, DHT22);
  #define METEO_NOME "DHT22"

#elif defined(METEO_SHT4X)
  #include <Adafruit_SHT4x.h>
  Adafruit_SHT4x sensore;
  #define METEO_NOME "SHT4x"
  #define METEO_API_EVENTI

#elif defined(METEO_SHT31)
  #include <Adafruit_SHT31.h>
  Adafruit_SHT31 sensore;
  #define METEO_NOME "SHT31"

#elif defined(METEO_HTU31D)
  #include <Adafruit_HTU31D.h>
  Adafruit_HTU31D sensore;
  #define METEO_NOME "HTU31D"
  #define METEO_API_EVENTI

#elif defined(METEO_SI7021)
  #include <Adafruit_Si7021.h>
  Adafruit_Si7021 sensore;
  #define METEO_NOME "Si7021"

#elif defined(METEO_HTU21D)
  #include <Adafruit_HTU21DF.h>
  Adafruit_HTU21DF sensore;
  #define METEO_NOME "HTU21D"

#elif defined(METEO_AHT20)
  #include <Adafruit_AHTX0.h>
  Adafruit_AHTX0 sensore;
  #define METEO_NOME "AHT20"
  #define METEO_API_EVENTI

#elif defined(METEO_BME280)
  #include <Adafruit_BME280.h>
  Adafruit_BME280 sensore;
  #define METEO_NOME "BME280"
#endif

// ---------------------------------------------------------------------------

// Accensione. L'unica parte davvero diversa fra un chip e l'altro.
bool meteoInit() {
#if defined(METEO_DHT22)
  // Il DHT22 non ha un registro identita' da leggere: begin() configura il
  // pin e non parla col sensore, quindi non sa dire cosa c'e' attaccato.
  // L'unico modo di scoprirlo e' provare a leggerlo, perche' un sensore
  // assente consegna NaN.
  //
  // Ci vogliono piu' tentativi: il chip ha bisogno di circa 2 s
  // dall'accensione prima di dare un dato buono, e capita che la prima
  // lettura esca NaN anche col sensore al suo posto.
  //
  // Il secondo parametro e' il "force" della libreria. Senza, le letture
  // ravvicinate tornano il valore in cache (il chip campiona ogni 2 s) e i
  // tentativi dopo il primo sarebbero copie inutili del primo.
  //
  // Se il sensore c'e' esce al primo giro; se non c'e' costa circa 1 s.
  const uint8_t  TENTATIVI      = 4;
  const uint16_t PAUSA_TENTATIVO = 350;  // ms

  sensore.begin();
  for (uint8_t tentativo = 0; tentativo < TENTATIVI; tentativo++) {
    if (tentativo > 0) delay(PAUSA_TENTATIVO);
    if (!isnan(sensore.readTemperature(false, true))) return true;
  }
  return false;

#elif defined(METEO_SHT4X)
  if (!sensore.begin()) return false;
  sensore.setPrecision(SHT4X_HIGH_PRECISION);
  // Riscaldatore SPENTO: serve a togliere la condensa, ma scalderebbe il
  // chip di alcuni gradi - e la temperatura dell'aria e' meta' della
  // formula del rischio grip.
  sensore.setHeater(SHT4X_NO_HEATER);
  return true;

#elif defined(METEO_SHT31)
  return sensore.begin(I2C_ADDR_SHT31);

#elif defined(METEO_HTU31D)
  return sensore.begin(I2C_ADDR_HTU31D);

#elif defined(METEO_SI7021) || defined(METEO_HTU21D)
  return sensore.begin();

#elif defined(METEO_AHT20)
  return sensore.begin(&Wire, 0, I2C_ADDR_AHT20);

#elif defined(METEO_BME280)
  if (sensore.begin(I2C_ADDR_BME280, &Wire)) return true;
  // Unico driver che spiega il proprio fallimento, per un motivo
  // preciso: il BMP280 ha lo stesso aspetto e lo stesso indirizzo del
  // BME280 ma non misura l'umidita', e senza umidita' il rischio grip
  // non esiste. Distinguerli si puo' solo dal chip-ID.
  Serial.print(F("  BME280: chip-ID 0x"));
  Serial.print(sensore.sensorID(), HEX);
  Serial.println(sensore.sensorID() == 0x58
      ? F(" = e' un BMP280! Non misura l'umidita': niente rischio grip.")
      : F(" = non risponde, o non e' un BME280. Prova l'indirizzo 0x77."));
  return false;
#endif
}

// Una lettura. I parametri arrivano pieni con gli ultimi valori buoni: si
// sovrascrive solo cio' che si e' letto davvero, cosi' una lettura sporca
// di umidita' non porta via anche una temperatura valida.
bool meteoLeggi(float &temp, float &umid) {
#ifdef METEO_API_EVENTI
  // Famiglia B. Attenzione all'ordine: prima l'umidita'.
  sensors_event_t eventoUmidita, eventoTemperatura;
  if (!sensore.getEvent(&eventoUmidita, &eventoTemperatura)) return false;
  temp = eventoTemperatura.temperature;
  umid = eventoUmidita.relative_humidity;
  return true;
#else
  // Famiglia A. Le due grandezze si validano separatamente: un chip puo'
  // consegnarne una buona e l'altra NaN.
  float t = sensore.readTemperature();
  float u = sensore.readHumidity();
  bool valido = false;
  if (!isnan(t)) { temp = t; valido = true; }
  if (!isnan(u)) { umid = u; valido = true; }
  return valido;
#endif
}

const __FlashStringHelper *meteoNome() { return F(METEO_NOME); }
