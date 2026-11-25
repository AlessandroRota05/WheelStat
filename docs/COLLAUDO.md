# Collaudo

La prima prova di una scatola nuova, o di un firmware nuovo, dal banco alla pista. Le prove sono in ordine: ognuna dà per buone quelle prima, quindi se una fallisce conviene fermarsi lì. Tieni aperto il monitor seriale a 115200: ogni cinque secondi stampa quello che vede ogni sensore.

*English version: [en/TESTING.md](en/TESTING.md).*

## Sul banco

### 1. Avvio

- [ ] Il display mostra il logo e poi l'elenco dei cinque componenti, ognuno con la spunta.
- [ ] Il monitor seriale finisce con `=== SYSTEM READY ===`.

Una croce è un controllo vero, non un'ipotesi:

| Croce su | Di solito |
|---|---|
| OLED | non ce ne accorgeresti: il display è spento. Il seriale dice `Boot OLED...... ERROR` |
| METEO | nessun sensore risponde: il DHT non è sul GPIO 4, un sensore I2C non è su SDA/SCL, o manca l'alimentazione. Il monitor seriale dice se ha trovato un BMP280 (senza umidità) |
| IMU | SDA/SCL scambiati, o un'IMU con libreria (BNO085) non attivata in `Config.h` |
| GPS | il TX del modulo non è sul GPIO 16, oppure TX e RX sono invertiti. Con il GPS presente, il seriale stampa famiglia, velocità e report al secondo (per esempio `u-blox M8 at 115200 baud, 10 reports/s`) |
| MEM | partizione della flash: ricarica scegliendo uno schema con SPIFFS/LittleFS |

### 2. Calibrazione

- [ ] Col BNO055 compare la schermata del magnetometro: muovi la scatola disegnando un otto nell'aria finché `MAG` arriva a 3/3. Il primo giorno può volerci un minuto.
- [ ] Con le altre IMU non c'è calibrazione da fare, ma **la scatola deve stare ferma durante la schermata d'avvio**: in quel secondo viene misurato lo zero del giroscopio. Il seriale lo stampa (`Gyro bias`): da fermo deve stare entro qualche grado al secondo.

### 3. Orientamento dell'IMU

- [ ] Le quattro prove della tabella nel [README](../README.md#it-orientamento): piega a destra positiva, impennata col muso in su, pallino in alto spingendo avanti, a sinistra spingendo a sinistra.

**Senza questa prova tutto il resto può sembrare giusto ed essere sbagliato.**

### 4. Pulsanti e pagine

- [ ] SU e GIÙ cambiano pagina, e la pagina nuova entra dal lato giusto.
- [ ] Il tratto sotto il titolo si sposta di una casella a ogni pagina.
- [ ] OK sulla pagina 0, con la scatola ferma e in piano, mostra `CALIBRAZIONE OK` e la piega va a 0.
- [ ] LOG fa comparire in alto a destra un pallino lampeggiante col tempo che scorre; LOG di nuovo ferma la registrazione e mostra le tre schermate di riepilogo.

### 5. Meteo

- [ ] Temperatura e umidità sono quelle della stanza. Alita sul sensore: l'umidità sale nel giro di qualche secondo.

### 6. GPS

Va fatto all'aperto, o almeno accanto a una finestra.

- [ ] Sulla pagina GPS i satelliti agganciati salgono. Il primo fix di un modulo nuovo può richiedere qualche minuto; i successivi meno di un minuto.
- [ ] Con il fix, la velocità da fermo è 0 o quasi, e latitudine e longitudine sono quelle del posto (controlla su una mappa).
- [ ] Nella barra in alto il cerchio del GPS prende il punto al centro quando c'è il fix. La bandiera a scacchi compare solo dopo aver impostato un traguardo: è normale.

### 7. WiFi e sito

- [ ] OK sulla pagina WiFi: compaiono nome della rete, password e `192.168.4.1`.
- [ ] GIÙ porta alla pagina col QR. Inquadrandolo con la fotocamera, il telefono propone di collegarsi alla rete; collegato, propone di "accedere alla rete" e apre il sito.
- [ ] In alternativa, il telefono si collega a mano con quella password e apre `http://192.168.4.1`.
- [ ] Sulla pagina Live c'è `Collegato`, e inclinando la scatola la piega cambia subito.
- [ ] Allontanati finché la rete cade, o spegni il WiFi del telefono: dopo un attimo compare `Connessione persa`.

### 8. Registrazione e sessioni

- [ ] Registra un paio di minuti muovendo la scatola, poi ferma.
- [ ] La pagina Sessioni del sito mostra il file nuovo; Grafici apre il percorso (se c'era il fix) e i grafici; scorrendo un grafico col dito il riquadro sopra mostra i valori.
- [ ] Scarica il CSV e il GPX: il CSV si apre in un foglio di calcolo, il GPX in Google Earth o simili.
- [ ] Durante una registrazione, aprire i grafici, scaricare o salvare le impostazioni risponde che bisogna prima fermarla: è voluto.

### 9. Impostazioni e aggiornamento

- [ ] Cambia i campioni al secondo e salva: la pagina dice che è salvato, e dopo un riavvio il valore resta.
- [ ] Aggiorna il firmware dal sito con `WheelStat.ino.bin` (Arduino IDE: *Sketch → Esporta sketch compilato*). Il display mostra `AGGIORNAMENTO...`, poi la scheda si riavvia; sessioni e impostazioni ci sono ancora.
- [ ] Prova a caricare un file che non è un firmware (per esempio una foto rinominata `.bin`): deve essere rifiutato senza riavviare.

### 10. Spegnimento dalla chiave (solo con `POWER_SENSE_PIN`)

Con un alimentatore da banco a 12 V al posto della moto, collegato come in [HARDWARE.md](HARDWARE.md#alimentazione-dalla-moto-).

- [ ] Il monitor seriale stampa `Power : 12.0 V` circa (la tensione dell'alimentatore). Se è molto diversa, controlla le resistenze del partitore.
- [ ] Avvia una registrazione e spegni l'alimentatore: il display fa in tempo a mostrare `SESSIONE SALVATA`.
- [ ] Riaccendi: nei Grafici di quella sessione **non** c'è l'avviso "interrotta senza riepilogo".

## Sulla moto

### 11. Montaggio

- [ ] La scatola è fissata rigida: se balla, i G e la piega misurano le vibrazioni del supporto.
- [ ] Moto dritta e in piano (sul cavalletto centrale, o tenuta da qualcuno): OK sulla pagina 0 azzera piega e beccheggio. Va rifatto a ogni accensione.

### 12. Giri

- [ ] Fermo sulla linea del traguardo, col fix, crea un tracciato dal sito (o premi OK sulla pagina Giri).
- [ ] Al primo passaggio sopra i 15 km/h il cronometro parte; dal secondo i giri si contano.
- [ ] I tempi tornano con un cronometro a mano, entro qualche decimo.
- [ ] Dal giro dopo il primo completo compare il delta sulla pagina Giri: `-` quando sei più veloce del giro di riferimento (in negativo sul display), `+` quando sei più lento.
- [ ] Con due intertempi aggiunti, nei Grafici della sessione la tabella mostra i settori e il giro ideale.

## Se qualcosa non va

Annota, per chi dovrà capire il problema:

- la prova che non passa e cosa succede invece;
- il testo del monitor seriale dall'avvio (in particolare le righe `Boot ...`);
- una foto del display;
- se riguarda una registrazione, il file `LOG_n.BIN` (lo scarichi dal sito come CSV).
