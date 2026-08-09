#pragma once

/*
  WheelStat - src/driver/Imu.h
  ===========================================================================
  Driver dell'IMU. Il BNO055 fa la sensor fusion DENTRO al chip, quindi da
  qui escono angoli gia' pronti in gradi e accelerazione con la gravita'
  gia' tolta: e' il motivo per cui il firmware non contiene nessun filtro
  di fusione (Madgwick, Mahony).

  Contratto: imuInit(), imuLeggi(), imuCalibrazione(),
  imuStampaCalibrazione(), imuNome() - vedi Config.h.

  ATTENZIONE: il BNO055 e' fuori produzione (Bosch lo dichiara "not
  recommended for new designs"). Il successore BNO085 mantiene la fusion
  nel chip ma parla un protocollo diverso e non emette angoli di Eulero:
  vanno ricavati dai quaternioni, e la mappatura degli assi va rifatta sul
  banco. Il senso di questo file e' isolare tutto cio' che sa di BNO055,
  cosi' che scrivere il driver del successore sia un lavoro circoscritto.
  Vedi docs/HARDWARE.md.
  ===========================================================================
*/

#include "../../Config.h"

#ifdef IMU_BNO055

// Il primo parametro (55) e' un id arbitrario della libreria, non
// l'indirizzo: quello e' il secondo.
Adafruit_BNO055 bno = Adafruit_BNO055(55, I2C_ADDR_BNO, &Wire);

// NDOF: fusione completa a 9 assi, assetto assoluto senza deriva.
// La rimappatura hardware fa lavorare il chip come se fosse montato
// piatto con X verso il davanti della moto, e il resto del firmware non
// deve sapere com'e' girata la scatola. E' una feature del BNO055: su un
// altro chip la stessa rotazione andrebbe fatta in software, qui dentro.
bool imuInit() {
  if (!bno.begin(OPERATION_MODE_NDOF)) return false;
  bno.setAxisRemap(REMAP_ASSI);
  bno.setAxisSign(REMAP_SEGNI);
  bno.setExtCrystalUse(true);
  delay(100);
  return true;
}

// Le quattro grandezze del contratto, gia' riferite agli assi della moto.
//
// LA TRADUZIONE DEGLI ASSI E' QUI, ed e' la ragione d'essere del file: il
// BNO055 chiama "roll" (.y) la rotazione LATERALE e "pitch" (.z) quella
// di MARCIA, al contrario dei nomi aeronautici. Quindi .z -> piega,
// .y -> impennata. Non e' deducibile dal datasheet a colpo d'occhio: la
// v6.3 le aveva scambiate e ci si e' accorti solo provando. Chi scrive un
// driver per un altro chip deve rifare quella prova, non copiare queste
// righe.
//
// La libreria non segnala errori di lettura (un chip muto ritorna zeri):
// il controllo vero e' bnoOk, deciso al boot.
bool imuLeggi(float &piega, float &impennata, float &accelLong, float &accelLat) {
  sensors_event_t orientazione, accelLineare;
  bno.getEvent(&orientazione, Adafruit_BNO055::VECTOR_EULER);        // gradi, gia' fusi
  bno.getEvent(&accelLineare, Adafruit_BNO055::VECTOR_LINEARACCEL);  // m/s^2, senza gravita'

  piega     = orientazione.orientation.z;
  impennata = orientazione.orientation.y;
  accelLong = accelLineare.acceleration.x;
  accelLat  = accelLineare.acceleration.y;
  return true;
}

// Dei quattro contatori del BNO055 conta solo il magnetometro: e' l'unico
// lento a salire, e questo chip riparte scalibrato a ogni accensione - da
// li' viene la schermata di attesa all'avvio. Un chip che salva la
// calibrazione (il BNO085 lo fa) puo' ritornare 3 sempre, e quella
// schermata si chiudera' da sola.
uint8_t imuCalibrazione() {
  uint8_t calSys, calGyro, calAcc, calMag;
  bno.getCalibration(&calSys, &calGyro, &calAcc, &calMag);
  return calMag;
}

// Diagnostica seriale: sta nel driver perche' QUALI contatori esistano
// dipende dal chip.
void imuStampaCalibrazione() {
  uint8_t calSys, calGyro, calAcc, calMag;
  bno.getCalibration(&calSys, &calGyro, &calAcc, &calMag);
  Serial.print(F("Calibraz.: SYS=")); Serial.print(calSys);
  Serial.print(F(" GYRO="));          Serial.print(calGyro);
  Serial.print(F(" ACC="));           Serial.print(calAcc);
  Serial.print(F(" MAG="));           Serial.print(calMag);
  Serial.println(F("  (0=no, 3=ok)"));
}

const __FlashStringHelper *imuNome() { return F("BNO055"); }

#endif  // IMU_BNO055
