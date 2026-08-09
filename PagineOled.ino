/*
  WheelStat - PagineOled.ino
  ===========================================================================
  Tutto cio' che disegna: schermata di avvio, calibrazione IMU, le 10
  pagine live con la transizione fra una e l'altra, il riepilogo di fine
  sessione e gli helper comuni (barraTitolo, rigaDato, rigaDoppia,
  titoloPagina, numeroGrande).

  Arduino concatena i tre .ino in un unico programma, quindi le variabili
  e le funzioni degli altri file (sensori, tracciati, record, l'oggetto
  display) sono visibili qui senza include.
  ===========================================================================
*/

// ===========================================================================
// 11C. CENTRATURA DEL TESTO
// ===========================================================================
// La x da cui far partire una stringa perche' risulti centrata. La calcola
// chi disegna, non il chiamante: cosi' cambiare SCREEN_WIDTH o ritoccare
// un testo non richiede di rifare i conti a mano.
//
// Tre sovrapposizioni esplicite e nessun parametro con valore di default:
// l'IDE Arduino copia i prototipi dalla definizione, e un default
// dichiarato due volte non compila.

// Testo in RAM (per esempio il nome di un tracciato letto da flash)
int centraTesto(const char *testo) {
  int larghezza = strlen(testo) * LARGHEZZA_CARATTERE;
  return (SCREEN_WIDTH - larghezza) / 2;
}

// Testo in flash con F(): la lunghezza va letta con strlen_P, il
// puntatore non punta alla RAM.
int centraTesto(const __FlashStringHelper *testo) {
  int larghezza = strlen_P((PGM_P)testo) * LARGHEZZA_CARATTERE;
  return (SCREEN_WIDTH - larghezza) / 2;
}

// Testo ingrandito: a dimensione N ogni carattere occupa N volte la
// larghezza di base.
int centraTesto(const __FlashStringHelper *testo, uint8_t dimensione) {
  int larghezza = strlen_P((PGM_P)testo) * LARGHEZZA_CARATTERE * dimensione;
  return (SCREEN_WIDTH - larghezza) / 2;
}

// ===========================================================================
// 11D. SCHERMATA DI AVVIO
// ===========================================================================
// Il logo animato, poi una riga per componente che si riempie MENTRE i
// componenti vengono inizializzati davvero.
//
// La checklist mostra il nome del driver compilato, che ogni driver
// dichiara da se' (meteoNome(), displayNome(), ...): lo schermo dice
// sempre che hardware sta girando, anche dopo aver cambiato una macro in
// Config.h. E l'esito e' un controllo vero - vedi il commento su gpsOk in
// WheelStat.ino per il perche' questa non e' una precisazione oziosa.
//
// Tutte queste funzioni escono subito senza pannello: il boot prosegue
// identico, resta il log seriale.

// Un fotogramma costa gia' ~23 ms di trasferimento I2C a 400 kHz;
// PAUSA_FRAME serve a non dipendere da quel valore (su un pannello SPI
// l'animazione partirebbe a razzo).
const unsigned long PAUSA_FRAME       = 5;   // ms
const unsigned long PAUSA_LETTERA     = 40;  // comparsa del nome, per lettera
const unsigned long DURATA_RIGA_CHECK = 120; // quanto resta leggibile "in corso"

// Geometria del logo: cornice arrotondata, nome grande al centro,
// versione sotto.
const int MARGINE_LOGO  = 8;
const int Y_CORNICE     = 12;
const int H_CORNICE     = 40;
const int R_CORNICE     = 6;
const int Y_NOME_LOGO   = 24;
const int Y_VERSIONE    = 42;

// Geometria della checklist
const int Y_PRIMA_RIGA = 15;
const int PASSO_RIGA   = 10;
const int X_ETICHETTA  = 2;
const int X_NOME       = 36;
const int X_ESITO      = 106;

// Un tasto qualsiasi, con lo stesso rilevamento di fronte usato ovunque.
// Il "|=" invece del "||" e' voluto: serve che TUTTI e quattro gli stati
// precedenti vengano aggiornati a ogni giro, altrimenti la valutazione
// pigra ne lascerebbe indietro qualcuno e il tasto successivo verrebbe
// letto come gia' premuto.
bool unTastoQualsiasi() {
  bool tasto = frontePressione(PIN_BTN_SU,  statoPrecSu);
  tasto |= frontePressione(PIN_BTN_GIU, statoPrecGiu);
  tasto |= frontePressione(PIN_BTN_OK,  statoPrecOk);
  tasto |= frontePressione(PIN_BTN_LOG, statoPrecLog);
  return tasto;
}

// Un fotogramma del logo. L'animazione e' la sequenza di chiamate qui
// sotto, non qualcosa scritto qui dentro: saltarla vuol dire disegnare
// direttamente l'ultimo fotogramma.
void disegnaLogo(int larghezza, int lettere, bool conVersione) {
  static const char NOME[] = "WHEELSTAT";

  display.clearDisplay();
  display.drawRoundRect((SCREEN_WIDTH - larghezza) / 2, Y_CORNICE,
                        larghezza, H_CORNICE, R_CORNICE, COLORE_ON);

  display.setTextColor(COLORE_ON);
  display.setTextSize(2);
  display.setCursor(centraTesto(F("WHEELSTAT"), 2), Y_NOME_LOGO);
  for (int i = 0; i < lettere; i++) display.print(NOME[i]);

  display.setTextSize(1);
  if (conVersione) {
    display.setCursor(centraTesto(FIRMWARE_VERSION), Y_VERSIONE);
    display.print(FIRMWARE_VERSION);
  }
  display.display();
}

// Tre battute: la cornice si apre dal centro, il nome compare una lettera
// per volta, la versione arriva per ultima. Un tasto qualsiasi salta al
// fotogramma finale - al decimo riavvio di fila un secondo e' lungo.
void splashIntro() {
  if (!oledOk) return;

  const int LARGHEZZA = SCREEN_WIDTH - 2 * MARGINE_LOGO;
  const int LETTERE   = 9;  // "WHEELSTAT"
  bool saltato = false;

  // 1. la cornice si apre dal centro. Il passo divide esattamente la
  //    larghezza, cosi' l'ultimo fotogramma e' la cornice piena.
  for (int w = 16; w <= LARGHEZZA && !saltato; w += 12) {
    disegnaLogo(w, 0, false);
    saltato = unTastoQualsiasi();
    delay(PAUSA_FRAME);
  }

  // 2. il nome si scrive
  for (int n = 1; n <= LETTERE && !saltato; n++) {
    disegnaLogo(LARGHEZZA, n, false);
    saltato = unTastoQualsiasi();
    delay(PAUSA_LETTERA);
  }

  // 3. la versione, poi un attimo per guardare il logo intero
  disegnaLogo(LARGHEZZA, LETTERE, true);
  if (!saltato) delay(400);

  // Il tasto usato per saltare non deve poi far scattare anche il menu
  ultimoTempoPulsante = millis();
}

// Cornice della checklist: da qui in poi si scrive solo dentro le righe.
void splashChecklist() {
  if (!oledOk) return;
  display.clearDisplay();
  barraTitolo(F("COMPONENTI"));
  display.display();
}

// Riga "sto provando": etichetta, nome del driver, puntini che avanzano.
// Va chiamata PRIMA dell'inizializzazione vera - se il boot si pianta,
// l'ultima riga coi puntini dice su quale componente.
//
// I puntini durano DURATA_RIGA_CHECK perche' le inizializzazioni sono
// quasi istantanee: senza, la riga passerebbe da vuota a spuntata senza
// che si veda niente.
void splashRigaAttesa(int indice, const __FlashStringHelper *etichetta,
                      const __FlashStringHelper *nome) {
  if (!oledOk) return;
  int y = Y_PRIMA_RIGA + indice * PASSO_RIGA;

  display.setTextSize(1);
  display.setTextColor(COLORE_ON);
  display.setCursor(X_ETICHETTA, y);
  display.print(etichetta);
  display.setCursor(X_NOME, y);
  display.print(nome);

  for (int punti = 1; punti <= 3; punti++) {
    display.fillRect(X_ESITO, y, SCREEN_WIDTH - X_ESITO, 8, COLORE_OFF);
    display.setCursor(X_ESITO, y);
    for (int i = 0; i < punti; i++) display.print('.');
    display.display();
    delay(DURATA_RIGA_CHECK / 3);
  }
}

// Spunta se il componente ha risposto, croce se no. Disegnate a segmenti
// invece di scrivere "OK"/"--": si leggono con la coda dell'occhio e
// occupano meno spazio.
void splashRigaEsito(int indice, bool ok) {
  if (!oledOk) return;
  int y = Y_PRIMA_RIGA + indice * PASSO_RIGA;
  int x = X_ESITO + 6;

  display.fillRect(X_ESITO, y, SCREEN_WIDTH - X_ESITO, 8, COLORE_OFF);
  if (ok) {
    display.drawLine(x,     y + 4, x + 2, y + 6, COLORE_ON);
    display.drawLine(x + 2, y + 6, x + 7, y + 1, COLORE_ON);
  } else {
    display.drawLine(x,     y + 1, x + 6, y + 7, COLORE_ON);
    display.drawLine(x + 6, y + 1, x,     y + 7, COLORE_ON);
  }
  display.display();
}

// ===========================================================================
// 11E. SCHERMATA DI CALIBRAZIONE
// ===========================================================================
// Mostra il movimento da fare con la scatola in mano, un otto in aria,
// finche' il magnetometro non e' pronto. Senza il disegno non c'e' modo di
// indovinarlo.

// Due cerchi tangenti in (CX_OTTO, 35), che e' l'incrocio dell'otto. Tutto
// a sinistra di x=52: la colonna destra e' del valore MAG.
const int CX_OTTO   = 24;
const int CY_ALTO   = 26;
const int CY_BASSO  = 44;
const int R_OTTO    = 9;
const int X_FRECCIA = 40;  // appena fuori dal percorso del pallino
const int R_PALLINO = 3;

// Le 16 posizioni del pallino, in ordine di percorrenza: cerchio alto in
// senso orario, incrocio, cerchio basso in senso antiorario, e ritorno.
// Scritte a mano invece che con seno e coseno - sono sempre le stesse, e
// cosi' non serve la libreria matematica. I punti a 45 gradi stanno a
// 6 px (9 * 0.707); con meno di 16 posizioni il pallino salta invece di
// scorrere.
const int8_t PERCORSO_OTTO[16][2] = {
  {24, 17}, {30, 20}, {33, 26}, {30, 32},
  {24, 35},
  {18, 38}, {15, 44}, {18, 50},
  {24, 53}, {30, 50}, {33, 44}, {30, 38},
  {24, 35},
  {18, 32}, {15, 26}, {18, 20}
};
const uint8_t PASSI_OTTO = 16;
const unsigned long PASSO_OTTO_MS = 90;  // giro completo in ~1.4 s

// Gambo piu' punta piena. verso = +1 punta in giu', -1 in su; il gambo
// sta sempre dalla parte opposta alla punta, cosi' la freccia arriva nel
// punto che indica invece di attraversarlo.
void frecciaVerticale(int x, int yBase, int verso) {
  display.drawFastVLine(x, verso > 0 ? yBase - 8 : yBase, 9, COLORE_ON);
  display.fillTriangle(x - 3, yBase, x + 3, yBase,
                       x, yBase + 6 * verso, COLORE_ON);
}

void disegnaCalibrazione(uint8_t calMag) {
  display.clearDisplay();
  barraTitolo(F("CALIBRAZIONE IMU"));

  display.drawCircle(CX_OTTO, CY_ALTO,  R_OTTO, COLORE_ON);
  display.drawCircle(CX_OTTO, CY_BASSO, R_OTTO, COLORE_ON);

  // Le frecce stanno sul fianco destro, dove si scende lungo il cerchio di
  // sopra e si risale lungo quello di sotto: puntano una verso l'altra e si
  // incontrano all'incrocio. Agli estremi dell'otto il verso sarebbe
  // ambiguo, qui no.
  frecciaVerticale(X_FRECCIA, CY_ALTO,  +1);
  frecciaVerticale(X_FRECCIA, CY_BASSO, -1);

  const uint8_t passo = (millis() / PASSO_OTTO_MS) % PASSI_OTTO;
  display.fillCircle(PERCORSO_OTTO[passo][0], PERCORSO_OTTO[passo][1],
                     R_PALLINO, COLORE_ON);

  // Grande abbastanza da leggersi mentre si muove la scatola in aria
  display.setTextSize(2);
  display.setCursor(58, 14);
  display.print(F("MAG"));

  display.setTextSize(3);
  display.setCursor(58, 32);
  display.print(calMag);
  display.print(F("/3"));
  display.setTextSize(1);

  display.setCursor(centraTesto(F("[OK] salta")), 56);
  display.print(F("[OK] salta"));

  display.display();
}

void disegnaCalibrazioneOk() {
  display.clearDisplay();
  barraTitolo(F("CALIBRAZIONE IMU"));

  display.setTextSize(2);
  display.setCursor(centraTesto(F("CALIBRATO"), 2), 22);
  display.print(F("CALIBRATO"));
  display.setTextSize(1);

  display.setCursor(centraTesto(F("MAG 3/3")), 42);
  display.print(F("MAG 3/3"));

  display.setCursor(centraTesto(F("[tasto] per iniziare")), 56);
  display.print(F("[tasto] per iniziare"));

  display.display();
}

// ===========================================================================
// 12. RIEPILOGO DI FINE SESSIONE (OLED)
// ===========================================================================

// Attende un tasto qualsiasi; true se premuto, false allo scadere del
// timeout. I primi 500 ms si ignorano, altrimenti il rimbalzo del tasto
// appena rilasciato chiuderebbe subito la pagina.
bool attesaTastoRiepilogo() {
  unsigned long ingresso = millis();
  while (millis() - ingresso < RIEPILOGO_TIMEOUT) {
    // Il telefono deve poter navigare anche durante il riepilogo
    if (wifiAttivo) server.handleClient();

    if (unTastoQualsiasi() && millis() - ingresso > 500) return true;
    delay(PAUSA_LOOP);
  }
  return false;
}

// Riepilogo in tre pagine: massimi (angoli/G-lat), poi G-lon/velocita'
// con durata, poi contatore eventi. Un tasto passa alla pagina
// successiva (l'ultima esce), il timeout chiude tutto in ogni pagina.
// L'attesa bloccante e' accettabile: la registrazione si ferma sempre
// da fermi.
void mostraRiepilogoSessione() {
  mostraPaginaMassimi();
  if (attesaTastoRiepilogo()) {
    mostraPaginaMassimi2();
    if (attesaTastoRiepilogo()) {
      mostraPaginaEventi();
      attesaTastoRiepilogo();
    }
  }

  // Il tasto premuto per uscire non deve anche azionare il menu
  ultimoTempoPulsante = millis();
}

// Barra del titolo invertita (sfondo bianco, testo nero) usata dalle
// pagine di riepilogo
void barraTitolo(const __FlashStringHelper *testo) {
  display.fillRect(0, 0, SCREEN_WIDTH, 11, COLORE_ON);
  display.setTextSize(1);
  display.setTextColor(COLORE_OFF);
  display.setCursor(centraTesto(testo), 2);
  display.print(testo);
  display.setTextColor(COLORE_ON);
}

// Nome della grandezza a sinistra e due valori etichettati su colonne
// FISSE (x=42 e x=86), uguali su ogni riga e su ogni pagina che la usa:
// la griglia si impara a leggere una volta sola. Il caso peggiore
// ("Fr 0.9*" = 7 caratteri = 42 px a x=86) finisce esatto al bordo.
// recA/recB accendono l'asterisco "nuovo record".
void rigaDoppia(int y, const __FlashStringHelper *nome,
                const __FlashStringHelper *lblA, float valA, bool recA,
                const __FlashStringHelper *lblB, float valB, bool recB,
                int decimali) {
  display.setCursor(0, y);
  display.print(nome);

  display.setCursor(42, y);
  display.print(lblA);
  display.print(valA, decimali);
  if (recA) display.print('*');

  display.setCursor(86, y);
  display.print(lblB);
  display.print(valB, decimali);
  if (recB) display.print('*');
}

// Piega, impennata/stoppie e G laterale. Angoli a 0 decimali e G a 1:
// sull'OLED piu' cifre non si leggono al volo, la precisione piena resta
// nel CSV. L'asterisco marca chi ha appena battuto il record storico.
void mostraPaginaMassimi() {
  display.clearDisplay();
  barraTitolo(F("MASSIMI SESSIONE 1/3"));

  rigaDoppia(14, F("Piega"),
             F("Dx "), massimiSessione[C_PIEGA_DX],  nuovoRecord[C_PIEGA_DX],
             F("Sx "), massimiSessione[C_PIEGA_SX],  nuovoRecord[C_PIEGA_SX], 0);
  rigaDoppia(26, F("Imp/St"),
             F("Im "), massimiSessione[C_IMPENNATA], nuovoRecord[C_IMPENNATA],
             F("St "), massimiSessione[C_STOPPIE],   nuovoRecord[C_STOPPIE], 0);
  rigaDoppia(38, F("G lat"),
             F("Dx "), massimiSessione[C_GLAT_DX],   nuovoRecord[C_GLAT_DX],
             F("Sx "), massimiSessione[C_GLAT_SX],   nuovoRecord[C_GLAT_SX], 1);

  display.setCursor(0, 56);
  display.print(F("[tasto] continua"));

  display.display();
}

// G longitudinale e velocita'. La durata viene dal cronometro e non dai
// minuti CSV, cosi' conta anche il minuto parziale che il log scarta; la
// legenda "nuovo record" guarda tutti i canali, non solo quelli mostrati
// qui.
void mostraPaginaMassimi2() {
  unsigned long durataSec = (millis() - inizioRegistrazione) / 1000;

  display.clearDisplay();
  barraTitolo(F("MASSIMI SESSIONE 2/3"));

  rigaDoppia(16, F("G lon"),
             F("Ac "), massimiSessione[C_G_ACCEL], nuovoRecord[C_G_ACCEL],
             F("Fr "), massimiSessione[C_G_FRENA], nuovoRecord[C_G_FRENA], 1);

  rigaDato(30, F("Vel"));
  display.print(massimiSessione[C_VELOCITA], 0);
  display.print(F(" km/h"));
  if (nuovoRecord[C_VELOCITA]) display.print('*');

  display.setCursor(0, 56);
  display.print(F("Durata "));
  display.print(durataSec / 60); display.print(F("m"));
  display.print(durataSec % 60); display.print(F("s"));

  bool almenoUnRecord = false;
  for (int c = 0; c < N_CANALI; c++) almenoUnRecord |= nuovoRecord[c];
  display.setCursor(80, 56);
  display.print(almenoUnRecord ? F("* nuovo!") : F(" [tasto]"));

  display.display();
}

// Quante volte ogni soglia evento e' stata superata: un ciclo sulla
// tabella eventi.
void mostraPaginaEventi() {
  display.clearDisplay();
  barraTitolo(F("CONTATORE EVENTI 3/3"));

  for (int e = 0; e < N_EVENTI; e++) {
    display.setCursor(0, 15 + e * 10);
    display.print(NOME_EVENTO_OLED[e]);
    display.print(conteggioEventi[e]);
  }

  display.display();
}

// ===========================================================================
// 14. PAGINE OLED
// ===========================================================================

// Titolo a sinistra, REC/IDLE a destra, e nel mezzo l'avviso GPS solo se
// il traguardo e' impostato: serve a sapere da QUALSIASI pagina se i giri
// stanno contando, senza andare apposta sulla pagina GIRI.
//
// Sta in una funzione sua perche' la ridisegna anche la transizione, sopra
// ogni fotogramma dello scorrimento: la barra e' cornice comune e non deve
// scivolare via insieme a una pagina.
void disegnaBarraStato() {
  display.fillRect(0, 0, SCREEN_WIDTH, 11, COLORE_ON);
  display.setTextSize(1);
  display.setTextColor(COLORE_OFF);
  display.setCursor(2, 2);
  display.print(F("WHEELSTAT"));
  if (traguardoImpostato) {
    display.setCursor(60, 2);
    display.print(gpsFixValido ? F("SAT") : F("SAT!"));
  }
  display.setCursor(94, 2);
  if (inRegistrazione) {
    // L'asterisco lampeggia: si vede che registra con la coda dell'occhio
    if (lampeggioRec) display.print(F("* REC"));
    else display.print(F("  REC"));
  } else {
    display.print(F("IDLE "));
  }
  display.setTextColor(COLORE_ON);
}

// Compone la pagina NEL BUFFER senza spedirla. Serve separata perche' la
// usano in due: il ciclo normale, che subito dopo chiama display(), e la
// transizione, che si tiene il fotogramma da parte per farlo scorrere -
// e il disegno dev'essere lo stesso, altrimenti la pagina salterebbe alla
// fine dello scorrimento.
void disegnaSchermata() {
  display.clearDisplay();
  disegnaBarraStato();

  switch (schermataCorrente) {
    case 0: disegnaPiega();            break;
    case 1: disegnaMeteo();            break;
    case 2: disegnaForzaG();           break;
    case 3: disegnaImpennataStoppie(); break;
    case 4: disegnaGPS();              break;
    case 5: disegnaGiri();             break;
    case 6: disegnaRecord();           break;
    case 7: disegnaRecord2();          break;
    case 8: disegnaMemoria();          break;
    case 9: disegnaWiFi();             break;
  }
}

// Ridisegna da zero la pagina corrente, 10 volte al secondo.
void aggiornaDisplay() {
  if (!oledOk) return;  // niente pannello: inutile disegnare e occupare l'I2C

  disegnaSchermata();
  display.display();  // spedisce il frame completo al pannello via I2C
}

// --- Transizione fra le pagine --------------------------------------------
// La pagina nuova entra facendo scorrere via la vecchia. Con dieci pagine
// in fila e uno schermo che cambia in un fotogramma solo non si capisce se
// si sta andando avanti o indietro; il verso dello scorrimento lo dice.
//
// Lo scorrimento e' orizzontale perche' nel buffer un byte tiene otto
// pixel in colonna (x, y..y+7): spostare l'immagine in x costa una memcpy
// per fascia, spostarla in y vorrebbe dire ricombinare i bit di due byte
// per ogni pixel.
//
// I due fotogrammi vanno tenuti da parte tutti e due perche' la
// composizione scrive nel buffer del pannello, che e' anche la sorgente da
// cui copia. Costano 2 KB di RAM sui 320 disponibili.
const uint8_t  FASCE_BUFFER = SCREEN_HEIGHT / 8;
const uint16_t BYTE_BUFFER  = SCREEN_WIDTH * FASCE_BUFFER;

uint8_t fotoUscita[BYTE_BUFFER];
uint8_t fotoEntrata[BYTE_BUFFER];

// Ogni fotogramma costa il trasferimento I2C del frame intero (~23 ms):
// 16 px sono sette fotogrammi, ~200 ms in tutto. Piu' fine e' piu' morbido
// ma si sente come ritardo fra la pressione e la pagina.
const int PASSO_TRANSIZIONE = 16;

// verso = +1 (tasto GIU'): la pagina nuova entra da destra. -1: da
// sinistra. Va chiamata DOPO aver cambiato schermataCorrente.
void transizionePagina(int verso) {
  if (!oledOk) return;

  // La pagina che si lascia e' gia' a schermo
  memcpy(fotoUscita, display.getBuffer(), BYTE_BUFFER);

  // Quella che arriva si disegna una volta sola: i valori restano fermi
  // durante lo scorrimento, un numero che cambia mentre la pagina scivola
  // si legge peggio.
  disegnaSchermata();
  memcpy(fotoEntrata, display.getBuffer(), BYTE_BUFFER);

  uint8_t *frame = display.getBuffer();
  for (int scorso = PASSO_TRANSIZIONE; scorso < SCREEN_WIDTH; scorso += PASSO_TRANSIZIONE) {
    int restano = SCREEN_WIDTH - scorso;  // colonne ancora visibili della vecchia

    for (uint8_t fascia = 0; fascia < FASCE_BUFFER; fascia++) {
      uint16_t inizio = fascia * SCREEN_WIDTH;
      uint8_t *riga = frame + inizio;

      if (verso > 0) {
        memcpy(riga,           fotoUscita + inizio + scorso, restano);
        memcpy(riga + restano, fotoEntrata + inizio,         scorso);
      } else {
        // Entra per prima la CODA della pagina nuova
        memcpy(riga,          fotoEntrata + inizio + restano, scorso);
        memcpy(riga + scorso, fotoUscita + inizio,            restano);
      }
    }

    disegnaBarraStato();  // ferma sopra il fotogramma composto
    display.display();
    delay(PAUSA_FRAME);
  }

  // Non e' una ripetizione: se PASSO_TRANSIZIONE non divide la larghezza,
  // il ciclo finisce con qualche colonna della vecchia ancora a schermo.
  memcpy(frame, fotoEntrata, BYTE_BUFFER);
  display.display();
}

// --- Elementi comuni a tutte le pagine live -------------------------------
// Il linguaggio grafico e' unico: titolo centrato a y=15, riga di
// separazione a y=24, dati sotto. Le etichette stanno a x=6 e i valori
// incolonnati a x=54, cosi' ogni pagina si legge allo stesso modo.

const int MARGINE_RIGA = 10;

void titoloPagina(const __FlashStringHelper *testo) {
  display.setTextSize(1);
  display.setCursor(centraTesto(testo), 15);
  display.print(testo);
  display.drawFastHLine(MARGINE_RIGA, 24, SCREEN_WIDTH - 2 * MARGINE_RIGA, COLORE_ON);
}

// Col tracciato attivo mostra il suo nome (troncato a 14 caratteri),
// altrimenti il testo di default. La usano RECORD e GIRI, che senza
// tracciato mostrano lo storico globale della modalita' libera.
void titoloPaginaTracciato(const __FlashStringHelper *testoDefault) {
  display.setTextSize(1);
  if (tracciatoAttivo < 0) {
    display.setCursor(centraTesto(testoDefault), 15);
    display.print(testoDefault);
  } else {
    char titolo[15];
    strncpy(titolo, tracciatoCorrente.nome, 14);
    titolo[14] = '\0';
    display.setCursor(centraTesto(titolo), 15);
    display.print(titolo);
  }
  display.drawFastHLine(MARGINE_RIGA, 24, SCREEN_WIDTH - 2 * MARGINE_RIGA, COLORE_ON);
}

// Inizia una riga "etichetta / valore": stampa l'etichetta e lascia il
// cursore sulla colonna dei valori, il chiamante stampa il resto.
void rigaDato(int y, const __FlashStringHelper *etichetta) {
  display.setCursor(6, y);
  display.print(etichetta);
  display.setCursor(54, y);
}

// Numero grande centrato col simbolo dei gradi (pagine 0 e 3).
// Centratura: una cifra sta piu' al centro di due.
void numeroGrande(int valore) {
  display.setTextSize(3);
  if (valore < 10) display.setCursor(45, 30);
  else display.setCursor(35, 30);
  display.print(valore);
  display.setTextSize(1);
  display.setCursor(75, 30);
  display.print(F("o"));  // simbolo gradi artigianale
}

// ---- Pagina 0: angolo di piega -------------------------------------------
void disegnaPiega() {
  // Piu' rischio grip = meno piega concessa prima dell'alert
  float maxPiegaSicura = PIEGA_MAX_TEORICA - (indiceRischio * RIDUZIONE_PIEGA_RISCHIO);

  if (angoloPiega > maxPiegaSicura) {
    // Banner lampeggiante di pericolo al posto del titolo (ogni 250 ms)
    if ((millis() / 250) % 2 == 0) {
      const int MARGINE_BANNER = 8;
      display.fillRoundRect(MARGINE_BANNER, 13, SCREEN_WIDTH - 2 * MARGINE_BANNER, 13, 3, COLORE_ON);
      display.setTextColor(COLORE_OFF);
      display.setCursor(centraTesto(F("! PERICOLO GRIP !")), 16);
      display.print(F("! PERICOLO GRIP !"));
      display.setTextColor(COLORE_ON);
    }
  } else {
    titoloPagina(F("ANGOLO DI PIEGA"));
  }

  numeroGrande(constrain((int)angoloPiega, 0, 99));

  // Barra che si riempie dal centro verso l'esterno. SEMI_BARRA e' quanto
  // puo' allungarsi da un lato della tacca di zero restando nella cornice:
  // meta' schermo meno margine e bordo.
  const int MARGINE_BARRA = 4;
  const int SEMI_BARRA    = SCREEN_WIDTH / 2 - MARGINE_BARRA - 2;

  display.drawRoundRect(MARGINE_BARRA, 56, SCREEN_WIDTH - 2 * MARGINE_BARRA, 6, 2, COLORE_ON);
  display.drawFastVLine(SCREEN_WIDTH / 2, 54, 10, COLORE_ON);  // tacca di zero

  int offsetBarra = map((int)angoloPiega, 0, 60, 0, SEMI_BARRA);
  offsetBarra = constrain(offsetBarra, 0, SEMI_BARRA - 2);
  display.fillRect(SCREEN_WIDTH / 2 - offsetBarra, 58, offsetBarra * 2, 2, COLORE_ON);
}

// ---- Pagina 1: meteo e rischio grip ---------------------------------------
void disegnaMeteo() {
  titoloPagina(F("METEO E GRIP"));

  rigaDato(30, F("Aria"));
  display.print(temperatura, 1);
  display.print(F(" C"));

  rigaDato(41, F("Umidita"));
  display.print((int)umidita);
  display.print(F(" %"));

  rigaDato(52, F("Rischio"));
  display.print((int)indiceRischio);
  display.print(F(" %"));
}

// ---- Pagina 2: G-meter con radar 2D ---------------------------------------
void disegnaForzaG() {
  titoloPagina(F("FORZA G"));

  // Numeri a sinistra (Lat in assoluto, Long con segno)...
  display.setCursor(6, 34);
  display.print(F("Lat :"));
  display.print(fabsf(forzaGLaterale), 1);

  display.setCursor(6, 48);
  display.print(F("Long:"));
  display.print(forzaGLongitudinale, 1);

  // ...e radar a destra: cerchio con mirino, come i G-meter da auto
  int cx = 95;
  int cy = 45;
  int r = 16;

  display.drawCircle(cx, cy, r, COLORE_ON);
  display.drawFastHLine(cx - 18, cy, 37, COLORE_ON);
  display.drawFastVLine(cx, cy - 18, 37, COLORE_ON);

  // Pallino che si sposta col vettore G, confinato dentro al radar.
  // 14 pixel = 1 G circa: su strada e' raro uscire dal cerchio.
  int dotX = cx + (int)(forzaGLaterale * 14);
  int dotY = cy - (int)(forzaGLongitudinale * 14);
  dotX = constrain(dotX, cx - 14, cx + 14);
  dotY = constrain(dotY, cy - 14, cy + 14);

  display.fillCircle(dotX, dotY, 3, COLORE_ON);
}

// ---- Pagina 3: impennata e stoppie ----------------------------------------
void disegnaImpennataStoppie() {
  // Stessa pagina per le due manovre: etichetta e numero grande seguono
  // quella in corso (muso su = impennata, muso giu' = stoppie)
  bool inStoppie = angoloStoppie > angoloImpennata;
  float angoloAttivo = inStoppie ? angoloStoppie : angoloImpennata;

  if (inStoppie) titoloPagina(F("STOPPIE"));
  else titoloPagina(F("IMPENNATA"));

  int valAngolo = constrain((int)angoloAttivo, 0, 99);
  numeroGrande(valAngolo);

  // Colonna con lo zero al centro: si riempie verso l'alto in impennata e
  // verso il basso in stoppie. Le x si contano dal bordo DESTRO, cosi' su
  // un pannello piu' largo resta appoggiata al bordo invece di finire in
  // mezzo.
  const int LARGHEZZA_COLONNA = 10;
  const int MARGINE_COLONNA   = 8;
  const int xColonna = SCREEN_WIDTH - MARGINE_COLONNA - LARGHEZZA_COLONNA;

  display.drawRoundRect(xColonna, 28, LARGHEZZA_COLONNA, 34, 2, COLORE_ON);
  display.drawFastHLine(xColonna - 2, 45, LARGHEZZA_COLONNA + 4, COLORE_ON);  // tacca di zero

  int altezzaBarra = map(valAngolo, 0, 45, 0, 15);
  altezzaBarra = constrain(altezzaBarra, 0, 15);
  const int xRiempimento = xColonna + 2;
  if (inStoppie) display.fillRect(xRiempimento, 46, 6, altezzaBarra, COLORE_ON);
  else display.fillRect(xRiempimento, 45 - altezzaBarra, 6, altezzaBarra, COLORE_ON);
}

// ---- Pagina 6: record storici (1/2) ------------------------------------
// Angoli e G laterali. Spezzata su due pagine da quando la velocita' e'
// un canale come gli altri: cinque righe ci starebbero a 7 px di passo,
// ma sarebbero illeggibili. Meglio una pagina in piu'.
void disegnaRecord() {
  titoloPaginaTracciato(F("RECORD 1/2"));

  rigaDoppia(28, F("Piega"),
             F("Dx "), recordStorici.canali[C_PIEGA_DX],  false,
             F("Sx "), recordStorici.canali[C_PIEGA_SX],  false, 0);
  rigaDoppia(39, F("Imp/St"),
             F("Im "), recordStorici.canali[C_IMPENNATA], false,
             F("St "), recordStorici.canali[C_STOPPIE],   false, 0);
  rigaDoppia(50, F("G lat"),
             F("Dx "), recordStorici.canali[C_GLAT_DX],   false,
             F("Sx "), recordStorici.canali[C_GLAT_SX],   false, 1);
}

// ---- Pagina 7: record storici (2/2) ------------------------------------
// G longitudinale e velocita', le due rimaste fuori dalla prima pagina.
void disegnaRecord2() {
  titoloPaginaTracciato(F("RECORD 2/2"));

  rigaDoppia(34, F("G lon"),
             F("Ac "), recordStorici.canali[C_G_ACCEL], false,
             F("Fr "), recordStorici.canali[C_G_FRENA], false, 1);

  rigaDato(50, F("Vel"));
  display.print(recordStorici.canali[C_VELOCITA], 0);
  display.print(F(" km/h"));
}

// ---- Pagina 8: diagnostica memoria flash ------------------------------------
void disegnaMemoria() {
  titoloPagina(F("MEMORIA"));

  // Spazio libero sulla partizione LittleFS, il dato che conta davvero
  rigaDato(30, F("Flash"));
  if (memoriaOk) {
    display.print((LittleFS.totalBytes() - LittleFS.usedBytes()) / 1024);
    display.print(F(" KB lib."));
  } else {
    display.print(F("ERRORE!"));
  }

  if (inRegistrazione) {
    rigaDato(41, F("File"));
    display.print(nomeFileLog + 1);  // +1 salta la "/" iniziale

    rigaDato(52, F("Minuti"));
    display.print(minutiRegistrati);
  } else {
    rigaDato(41, F("Stato"));
    display.print(F("IN PAUSA"));

    rigaDato(52, F("Avvio"));
    display.print(F("tasto [LOG]"));
  }
}

// ---- Pagina 9: WiFi e telemetria dal telefono -------------------------------
void disegnaWiFi() {
  titoloPagina(F("WIFI"));

  rigaDato(29, F("Rete"));
  display.print(WIFI_SSID);

  if (wifiAttivo) {
    // WiFi acceso: tutto quello che serve per collegarsi (righe piu'
    // fitte del solito: qui i dati sono quattro)
    rigaDato(38, F("Pass"));
    display.print(WIFI_PASSWORD);

    rigaDato(47, F("Sito"));
    display.print(WiFi.softAPIP());

    rigaDato(56, F("Client"));
    display.print(WiFi.softAPgetStationNum());
    display.print(F("  [OK] off"));
  } else {
    rigaDato(41, F("Stato"));
    display.print(F("SPENTO"));

    rigaDato(52, F("Avvio"));
    display.print(F("tasto [OK]"));
  }
}

// ---- Pagina 4: GPS -----------------------------------------------------------
// Il sensore grezzo; il lap timing sta nella pagina GIRI, che usa questi
// stessi dati. Tre stati distinti, e la distinzione e' il punto: modulo
// assente, modulo senza fix, fix valido. Senza fix non si mostrano lat/lon
// congelate all'ultimo valore buono - sarebbe un dato silenziosamente
// sbagliato.
void disegnaGPS() {
  titoloPagina(F("GPS"));

  // Modulo che non ha mai parlato: e' cablaggio, non satelliti, e dirlo
  // cambia cosa si va a controllare.
  if (!gpsOk) {
    rigaDato(30, F("Stato"));
    display.print(F("ASSENTE"));

    rigaDato(41, F("Modulo"));
    display.print(F("non parla"));

    rigaDato(52, F("Verifica"));
    display.print(F("TX/RX/3.3V"));
    return;
  }

  if (!gpsFixValido) {
    rigaDato(30, F("Stato"));
    display.print(F("NESSUN FIX"));

    rigaDato(41, F("Sat"));
    display.print(satellitiGPS);
    display.print(F(" agganciati"));

    rigaDato(52, F("Attesa"));
    display.print(F("cielo libero"));
    return;
  }

  // Righe piu' fitte del solito per far stare lat/lon a piena precisione.
  // "Vel" mostra live/record insieme: il confronto immediato serve piu'
  // del record isolato sulla pagina RECORD.
  rigaDato(29, F("Vel"));
  display.print(velocitaGPS, 0);
  display.print('/');
  display.print(recordStorici.canali[C_VELOCITA], 0);
  display.print(F(" km/h"));

  rigaDato(38, F("Lat"));
  display.print(latitudineGPS, 5);

  rigaDato(47, F("Lon"));
  display.print(longitudineGPS, 5);

  rigaDato(56, F("Sat"));
  display.print(satellitiGPS);
}

// "m:ss.d". Niente ore: un giro cosi' lungo e' fuori scala per qualunque
// pista. La usano sia l'OLED sia la pagina web, cosi' il formato e'
// definito in un posto solo.
String formattaTempoGiro(unsigned long ms) {
  unsigned long totSec = ms / 1000;
  int decimi = (ms % 1000) / 100;
  String s = String(totSec / 60) + ':';
  if (totSec % 60 < 10) s += '0';
  s += String(totSec % 60) + '.' + String(decimi);
  return s;
}

// Stampa sull'OLED il risultato di formattaTempoGiro()
void stampaTempoGiro(unsigned long ms) {
  display.print(formattaTempoGiro(ms));
}

// ---- Pagina 5: lap timing --------------------------------------------------
// [OK] imposta il traguardo sulla posizione GPS attuale: in modalita'
// libera resta in RAM, con un tracciato attivo finisce anche sul suo file
// (vedi impostaTraguardo).
void disegnaGiri() {
  titoloPaginaTracciato(F("GIRI"));

  if (!traguardoImpostato) {
    rigaDato(41, F("Stato"));
    display.print(F("da impostare"));

    rigaDato(52, F("Avvio"));
    display.print(F("tasto [OK]"));
    return;
  }

  // "Dist" punta al prossimo obiettivo vero: un checkpoint intermedio se
  // il tracciato ne ha, altrimenti il traguardo.
  rigaDato(29, F("Dist"));
  if (gpsFixValido) {
    double obLat, obLon; bool obETraguardo;
    prossimoObiettivo(obLat, obLon, obETraguardo);
    display.print(distanzaDa(obLat, obLon), 0);
    display.print(F(" m"));
  } else {
    display.print(F("GPS assente"));
  }

  rigaDato(38, F("Giro"));
  if (giroInCorso) {
    unsigned long secGiro = (millis() - inizioGiroCorrente) / 1000;
    display.print(secGiro / 60); display.print(F("m"));
    display.print(secGiro % 60); display.print(F("s"));
  } else {
    display.print(F("in attesa"));
  }

  rigaDato(47, F("Ultimo"));
  if (numGiri > 0) stampaTempoGiro(tempiGiro[numGiri - 1]);
  else display.print(F("--"));

  rigaDato(56, F("Record"));
  if (giroMigliore >= 0) stampaTempoGiro(tempiGiro[giroMigliore]);
  else display.print(F("--"));
}

