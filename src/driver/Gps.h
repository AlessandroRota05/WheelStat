#pragma once

/*
  WheelStat - src/driver/Gps.h
  ===========================================================================
  Configurazione del modulo GPS: mandare al ricevitore le impostazioni che
  sa accettare. E' l'unica parte che dipende dal modulo - la LETTURA
  (leggiGPS in WheelStat.ino) e' NMEA standard e vale per tutti.

  Contratto: gpsConfigura(), gpsNome() - vedi Config.h.

  Tre driver perche' u-blox ha cambiato meccanismo per strada:
    GPS_UBLOX          M6/M8/M9  messaggi CFG-MSG e CFG-RATE
    GPS_UBLOX_M10      M10       chiavi CFG-VALSET (i vecchi non ci sono piu')
    GPS_NMEA_GENERICO  altri     niente: restano ai valori di fabbrica
  Un M8 accetta entrambi i meccanismi, un M10 solo il nuovo, un M6 solo il
  vecchio. Scegliere il driver sbagliato lascia il modulo a 1 Hz mentre
  GPS_FIX_TIMEOUT_MS e' tarato sul rate alto: il fix lampeggia e il lap
  timing salta i passaggi. Vedi docs/HARDWARE.md.

  Provato solo il ramo M6/M8.
  ===========================================================================
*/

#include "../../Config.h"

// UART hardware dedicata al modulo GPS (non e' la seriale USB di debug).
// Quale sia lo decide Config.h: il C3 e l'S2 ne hanno due invece di tre, e
// la 0 e' sempre occupata dal monitor seriale. Vive qui perche' e' hardware
// del GPS, ma la usa anche leggiGPS() in WheelStat.ino: da li' la
// dichiarazione extern in Config.h, stessa soluzione dell'oggetto display.
HardwareSerial gpsSerial(GPS_UART_NUM);

// ===========================================================================
// Parte comune ai due driver u-blox
// ===========================================================================
#if defined(GPS_UBLOX) || defined(GPS_UBLOX_M10)

// Frame UBX col checksum Fletcher-8 calcolato invece che copiato da un
// tutorial: cosi' vale per qualunque comando, non solo per quelli usati
// qui.
void inviaComandoUBX(uint8_t classe, uint8_t id, const uint8_t *payload, uint16_t lunghezza) {
  uint8_t intestazione[4] = {
    classe, id, (uint8_t)(lunghezza & 0xFF), (uint8_t)(lunghezza >> 8)
  };

  uint8_t ckA = 0, ckB = 0;
  for (uint8_t i = 0; i < sizeof(intestazione); i++) { ckA += intestazione[i]; ckB += ckA; }
  for (uint16_t i = 0; i < lunghezza; i++)           { ckA += payload[i];      ckB += ckA; }

  gpsSerial.write(0xB5);
  gpsSerial.write(0x62);
  gpsSerial.write(intestazione, sizeof(intestazione));
  gpsSerial.write(payload, lunghezza);
  gpsSerial.write(ckA);
  gpsSerial.write(ckB);
}
#endif

// ===========================================================================
// M6 / M8 / M9 - messaggi di configurazione dedicati
// ===========================================================================
#ifdef GPS_UBLOX

// UBX-CFG-MSG con rate 0 spegne una frase NMEA. idFrase e' l'ID nella
// classe NMEA standard (0xF0): GLL=0x01, GSA=0x02, GSV=0x03, VTG=0x05.
// GGA (0x00) e RMC (0x04) restano accese: sono le uniche due che
// TinyGPS++ usa qui.
void disattivaFraseNMEA(uint8_t idFrase) {
  uint8_t payload[3] = { 0xF0, idFrase, 0x00 };
  inviaComandoUBX(0x06, 0x01, payload, sizeof(payload));
}

// Prima si libera banda spegnendo le frasi inutili, poi si alza il rate:
// al contrario, per qualche ciclo il modulo proverebbe a mandare tutto il
// set di default alla frequenza piu' alta e la seriale a 9600 baud
// saturerebbe (il conto e' in Config.h, su GPS_RATE_HZ).
//
// Non si leggono gli ACK di proposito: se il modulo non c'e', i comandi
// vanno a vuoto senza errori e senza attese, e leggiGPS() valida il fix
// da sola.
void gpsConfigura() {
  disattivaFraseNMEA(0x01); delay(50);  // GLL
  disattivaFraseNMEA(0x02); delay(50);  // GSA
  disattivaFraseNMEA(0x03); delay(50);  // GSV
  disattivaFraseNMEA(0x05); delay(50);  // VTG

  uint16_t misuraMs = 1000 / GPS_RATE_HZ;
  uint8_t payload[6] = {
    (uint8_t)(misuraMs & 0xFF), (uint8_t)(misuraMs >> 8),  // measRate, ms
    1, 0,                                                   // navRate: 1 soluzione per misura
    1, 0                                                    // timeRef: tempo GPS
  };
  inviaComandoUBX(0x06, 0x08, payload, sizeof(payload));  // UBX-CFG-RATE
}

const __FlashStringHelper *gpsNome() { return F("u-blox"); }
#endif

// ===========================================================================
// M10 - chiavi CFG-VALSET
// ===========================================================================
#ifdef GPS_UBLOX_M10

// Ogni impostazione ha una chiave a 32 bit che ne codifica anche la
// dimensione: 0x20... = 1 byte (rate delle frasi NMEA), 0x30... = 2 byte
// (periodo di misura).
//
// CFG-RATE-MEAS e' documentata ovunque. Le quattro chiavi NMEA vengono
// dal manuale d'interfaccia M10 e NON sono verificate sul campo: se una
// fosse sbagliata il modulo risponde NAK e inviaChiave() lo stampa col
// nome dell'impostazione. E' il motivo per cui questo driver legge gli
// ACK e quello M8 no - senza, un errore si presenterebbe come un GPS che
// resta a 1 Hz senza dirlo a nessuno.
const uint32_t CFG_RATE_MEAS       = 0x30210001;
const uint32_t CFG_MSGOUT_NMEA_GLL = 0x209100CA;
const uint32_t CFG_MSGOUT_NMEA_GSA = 0x209100C0;
const uint32_t CFG_MSGOUT_NMEA_GSV = 0x209100C5;
const uint32_t CFG_MSGOUT_NMEA_VTG = 0x209100B1;

// Cerca UBX-ACK-ACK (classe 0x05, id 0x01) o UBX-ACK-NAK (0x05, 0x00)
// nel flusso in arrivo, che nel frattempo contiene anche frasi NMEA.
// Non si controlla il checksum ne' a quale comando si riferisce: si manda
// una chiave alla volta, quindi la prima risposta e' per forza la sua.
// Timeout scaduto = modulo assente, o modulo che non parla UBX.
bool attendiAck(unsigned long timeoutMs) {
  const unsigned long inizio = millis();
  uint8_t atteso = 0;  // quanti byte dell'intestazione ho gia' riconosciuto

  while (millis() - inizio < timeoutMs) {
    if (!gpsSerial.available()) continue;
    uint8_t b = gpsSerial.read();
    switch (atteso) {
      case 0: atteso = (b == 0xB5) ? 1 : 0; break;
      case 1: atteso = (b == 0x62) ? 2 : 0; break;
      case 2: atteso = (b == 0x05) ? 3 : 0; break;
      case 3: return (b == 0x01);
    }
  }
  return false;
}

// Payload di UBX-CFG-VALSET: versione, strati, 2 riservati, chiave (4
// byte little-endian), valore. Strato 0x01 = solo RAM, cosi' il modulo
// resta riutilizzabile altrove senza sorprese dopo lo spegnimento.
bool inviaChiave(uint32_t chiave, uint16_t valore, uint8_t byteValore,
                 const __FlashStringHelper *nome) {
  uint8_t payload[10];
  uint8_t n = 0;
  payload[n++] = 0x00;
  payload[n++] = 0x01;
  payload[n++] = 0x00;
  payload[n++] = 0x00;
  for (uint8_t i = 0; i < 4; i++) payload[n++] = (uint8_t)(chiave >> (8 * i));
  payload[n++] = (uint8_t)(valore & 0xFF);
  if (byteValore == 2) payload[n++] = (uint8_t)(valore >> 8);

  inviaComandoUBX(0x06, 0x8A, payload, n);
  if (attendiAck(1000)) return true;

  Serial.print(F("  GPS M10: rifiutata o senza risposta -> "));
  Serial.println(nome);
  return false;
}

void gpsConfigura() {
  bool ok = true;
  ok &= inviaChiave(CFG_MSGOUT_NMEA_GLL, 0, 1, F("spegni GLL"));
  ok &= inviaChiave(CFG_MSGOUT_NMEA_GSA, 0, 1, F("spegni GSA"));
  ok &= inviaChiave(CFG_MSGOUT_NMEA_GSV, 0, 1, F("spegni GSV"));
  ok &= inviaChiave(CFG_MSGOUT_NMEA_VTG, 0, 1, F("spegni VTG"));
  ok &= inviaChiave(CFG_RATE_MEAS, 1000 / GPS_RATE_HZ, 2, F("frequenza"));

  if (!ok) {
    Serial.println(F("  GPS M10: configurazione INCOMPLETA, il modulo resta a 1 Hz"));
    Serial.println(F("  e il fix sara' intermittente. Se non e' un M10, usa"));
    Serial.println(F("  GPS_UBLOX (M6/M8/M9) o GPS_NMEA_GENERICO."));
  }
}

const __FlashStringHelper *gpsNome() { return F("u-blox M10"); }
#endif

// ===========================================================================
// Moduli non u-blox
// ===========================================================================
#ifdef GPS_NMEA_GENERICO

// Vuota, ed e' giusto cosi': ogni famiglia non-u-blox ha comandi
// proprietari spesso non documentati, quindi si lascia il modulo com'e'.
// Resta a 1 Hz invece di 5: su strada si nota poco, sul lap timing si',
// perche' a 100 km/h un punto al secondo sono 28 metri fra un campione e
// l'altro contro 5.6. GPS_FIX_TIMEOUT_MS in Config.h e' gia' allargato
// di conseguenza. Esiste come funzione, e non come #ifdef dentro setup(),
// proprio perche' il motivo stia scritto dove uno lo cerca.
void gpsConfigura() {
}

const __FlashStringHelper *gpsNome() { return F("NMEA"); }
#endif
