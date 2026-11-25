# Storico versioni

La cronologia del progetto sta qui e solo qui: nel codice i commenti spiegano il perché di una scelta, non quando è stata fatta. I nomi di funzioni e file citati nelle voci fino alla v9.16 sono quelli di allora (in italiano, prima della riscrittura v10.0).

```
STORICO VERSIONI
v6.3  rimappatura hardware BNO055 per il montaggio reale
v6.5  statistica stoppie
v6.6  verso impennata/stoppie corretto + zona morta
v6.7  contatore eventi di sessione con soglie
v7.0  WiFi: download CSV e telemetria live con grafici dal telefono
v7.1  password nuova ritoccata interfaccia
v7.2  passati di commenti nuovi per sistemare il casino
v8.0  riscrittura organizzata
v8.1  calibrazione guidata all'avvio
v8.2  schermata di calibrazione ridisegnata
v8.3  interfaccia OLED uniformata: tutte le pagine live hanno titolo
      centrato con riga di separazione alla stessa quota e dati su una
      griglia
v9.0  logging spostato dalla MicroSD alla memoria flash interna
v9.1  riepilogo di fine sessione riorganizzato
v9.2  revisione di robustezza, funzionalita' invariata: valori meteo
      di partenza neutri (niente rischio 70% fittizio prima della
      prima lettura del DHT22 o a sensore guasto)
v9.3  aggiunto modulo GPS (NEO-6M/8M, UART2): nuova pagina OLED con
      stato del fix, velocita', posizione e satelliti; stessi dati
      esposti anche su /dati per il sito. Nessun lap timing ancora:
      e' il primo passo, i prossimi useranno questi stessi dati.
v9.4  lap timing su un traguardo GPS "volante" (un solo punto, vive in
      RAM): nuova pagina GIRI, tempi live su OLED/sito e riepilogo
      giri in coda al CSV di sessione. Ancora nessun salvataggio di
      piu' tracciati: quello e' il prossimo passo.
v9.5  tracciati multipli su flash (/TRACK_n.BIN + record separati per
      tracciato): pagina web /tracciati per creare/selezionare/
      eliminare, storico canali e miglior giro automaticamente
      separati per tracciato, titoli OLED (RECORD/GIRI) che mostrano
      il nome del tracciato attivo.
v9.6  traccia GPS della sessione sulla pagina web live: percorso
      disegnato in tempo reale su canvas (proiezione locale piatta),
      colorato per intensita' di piega, con marcatore del traguardo.
      Si azzera da sola a ogni nuova registrazione.
v9.7  interfaccia web unificata: foglio di stile condiviso (/style.css)
      e navigazione a tab comuni a "/", "/live" e "/tracciati", con
      card/bottoni/badge coerenti al posto di tre CSS inline diversi.
      Nessuna funzionalita' cambiata, solo l'aspetto.
v9.8  la velocita' GPS diventa un canale vero (C_VELOCITA), non piu' un
      dato isolato: filtro anti-buca, massimi di minuto/sessione,
      record storici (globali e per tracciato) e tabella web la
      includono automaticamente, zero codice speciale. CSV per-minuto
      esteso con Lat/Lon/Giro. Formato record cambiato (MAGIC_RECORD
      v2): i file /RECORD.BIN e /TRACK_n_REC.BIN esistenti si azzerano
      al primo avvio con questo firmware, e' l'unica volta.
v9.9  GPS a 5 Hz invece di 1 (comandi UBX: spente le frasi NMEA non
      lette, cosi' la seriale a 9600 baud regge il rate piu' alto senza
      saturarsi), fix piu' reattivo (timeout accorciato di conseguenza).
      Nuova pagina web /confronta: due sessioni a scelta, tempi giro
      affiancati e migliore di ciascuna, con avviso se sono su
      tracciati diversi.
v9.10 rifinitura di interfacce e commenti, nessuna feature nuova:
      /confronta non scansiona piu' la flash due volte (una lettura
      sola, riusata per entrambe le select), gestisce il caso di meno
      di due sessioni disponibili, ed evidenzia il giro piu' veloce
      riga per riga; /tracciati mostra quante sessioni per tracciato;
      indicatore GPS nella barra di stato OLED (solo se il lap timing
      e' attivo); versione firmware su splash OLED e log seriale;
      bottoni web piu' comodi da toccare, niente piu' zoom automatico
      di iOS sui form.
v9.11 le pagine OLED RECORD e MASSIMI SESSIONE, stringendo 5 righe a
      7px per farci stare la velocita', erano tecnicamente senza
      sovrapposizioni ma illeggibili in pratica. Spezzate su piu'
      pagine con spaziatura comoda: RECORD ora 1/2 e 2/2 (10 pagine
      OLED in tutto, non piu' 9), riepilogo di fine sessione ora in
      3 pagine invece di 2. Nessun dato in meno, solo piu' respiro.
v9.12 forma del tracciato salvata (non piu' solo live): si registra da
      sola sul giro piu' veloce di sessione, disegnata come SVG statico
      su /tracciati. Settori: fino a 2 checkpoint intermedi per
      tracciato (3 settori per giro), aggiungibili/rimuovibili da web
      stando fisicamente sul punto; tempi settore su CSV (colonne
      Settore_N, retrocompatibili) e sulla pagina live. Classifica
      generale su /tracciati: giro piu' veloce di sempre su qualunque
      tracciato, tracciato piu' frequentato. Formato tracciato
      cambiato (MAGIC_TRACCIATO v3): i tracciati salvati con firmware
      precedenti vanno ricreati.
v9.13 esportazione GPX di una sessione (pulsante accanto a CSV/Elimina
      sulla pagina Sessioni): stesso percorso apribile in Google Earth
      o altri visualizzatori GPS, colonne Lat/Lon trovate per nome
      cosi' funziona anche sui log di firmware precedenti.
      Riorganizzazione per temi, nessuna funzionalita' cambiata:
      - OLED: pagine in ordine "cosa guardo in sella" (piega, meteo,
        G, impennata, GPS, giri) poi "cosa guardo dopo" (record 1/2,
        2/2) poi "roba di sistema" (memoria, WiFi) - prima erano
        mischiate, con GPS e giri in fondo dopo le pagine di sistema.
      - Web: la pagina Sessioni faceva anche da home per tracciato
        attivo e record dei canali, duplicando contenuti gia' su
        /tracciati. Ora Sessioni gestisce solo i file (CSV, GPX,
        elimina), /tracciati raccoglie tutto cio' che riguarda
        tracciati e prestazioni (classifica, tracciato attivo,
        record canali, settori, elenco tracciati).
v9.14 revisione generale, nessuna funzionalita' cambiata:
      - Sketch diviso in tre file (WheelStat.ino, PagineOled.ino,
        InterfacciaWeb.ino) invece di uno solo da oltre 3700 righe -
        Arduino li concatena da soli, basta metterli nella stessa
        cartella (vedi la nota "STRUTTURA DEL PROGETTO" qui sotto).
        La sola parte web occupava piu' righe dell'intero firmware
        v9.2: separare per argomento la rende molto piu' facile da
        seguire, specie per chi la legge per la prima volta.
      - Corretti due commenti che avevano smesso di essere veri: il
        pin TX del GPS diceva "non usato" ma dalla v9.9 ci passano i
        comandi UBX; MAX_TRACCIATI diceva "poche decine di byte a
        tracciato" ma da quando esistono forma e settori (v9.12) sono
        diventati ~720 - lezione: un commento sbagliato e' peggio di
        nessun commento, va tenuto aggiornato quando cambia il codice
        che descrive.
      - Ripuliti spazi bianchi finali e un paio di commenti in stile
        vecchio (senza spazio dopo //) rimasti dalle primissime
        versioni.
v9.15 la schermata di avvio controlla davvero i componenti: tre righe
      su cinque spuntavano OK senza verificare niente. Il GPS aveva
      l'esito cablato a true, meteoInit() col DHT22 ritornava sempre
      true (quel sensore non ha un indirizzo da interrogare) e
      Adafruit_SSD1306::begin() ritorna false solo se fallisce la
      malloc, senza mai interrogare il pannello. Ora: ascolto NMEA
      sulla UART, lettura forzata del DHT22, ACK I2C sull'OLED. La
      pagina GPS distingue "modulo assente" da "senza fix".
      Interfaccia: cambio pagina con scorrimento nel verso del tasto,
      schermata di calibrazione ridisegnata (la freccia in fondo
      all'otto indicava il verso sbagliato), popup unificati.
      Documentazione: README riscritto, eliminati ROADMAP.md e
      "Prompt sviluppo.md" a piano concluso.
v9.16 il firmware compila per tutta la famiglia ESP32 (classico, S3,
      S2, C3): Config.h ha quattro blocchi di pin scelti dalle macro
      CONFIG_IDF_TARGET_* del core, perche' i GPIO 22, 25 e 27 sull'S3
      e sul C3 non esistono. GPS_UART_NUM perche' C3 e S2 hanno due
      UART e la 0 e' del monitor seriale. Un chip non previsto da'
      #error invece di ripiegare su pin che in parte non esistono.
      Provato su hardware solo il classico; il C3 sta al 90% della
      flash (core RISC-V, ~100 KB in piu').
      L'orientamento dell'IMU diventa una sezione a se' in Config.h:
      macro di montaggio (SOTTOSOPRA / DRITTA / MANUALE per P0-P7) e
      tabella delle quattro prove per raddrizzare i versi. Le due
      costanti REMAP_* si formano da un'unica macro, non possono piu'
      restare disallineate.
      Ripulita la cartella: tolti CLAUDE.md e l'anteprima animazioni in
      browser. Restano il README e i tre documenti in docs/.
v10.0 riscrittura della struttura, comportamento del dispositivo
      invariato salvo i punti segnati con (!).
      - Da tre .ino concatenati a moduli C++ veri: WheelStat.ino contiene
        solo setup()/loop(), il resto sta in src/ (un modulo per
        compito, ognuno con il suo header) con i driver in src/drivers/,
        le pagine OLED in src/ui/, il sito in src/web/. Le ~80 variabili
        globali visibili ovunque sono diventate stato privato del modulo
        che le possiede. Resta uno sketch Arduino IDE.
      - Codice e commenti in inglese; i commenti tengono i "perche'" e
        la cronologia resta solo in questo file.
      - (!) Lingua dell'interfaccia (OLED + sito) scelta in Config.h:
        LANG_IT o LANG_EN. Tutti i testi stanno in src/i18n/. Il log
        seriale e' sempre in inglese.
      - (!) Formato dati nuovo e in inglese, NON compatibile coi file
        esistenti: colonne CSV (Lean_R, Wheelie, Speed_Kmh...), righe di
        riepilogo (TRACK/SUMMARY/MAX/EVENTS/TOTAL/LAPS), firme dei .BIN
        (WSR3, WST4). Record e tracciati salvati ripartono da zero; i
        vecchi LOG_n.CSV restano scaricabili ma non entrano in
        Confronta. Il formato e' descritto in src/SessionFormat.h.
      - (!) Indirizzi del sito in inglese (/tracks, /compare, /data,
        /download...) e campi JSON di /data rinominati; i settori
        diventano array, non piu' tre campi fissi.
      - Le pagine web escono a blocchi mentre vengono generate invece
        di essere costruite in una String da qualche KB: niente piu'
        frammentazione dello heap. GPX compreso, che ora conserva la
        precisione piena di Lat/Lon.
      - Tolte le duplicazioni: azzeramento giri (era copiato in tre
        punti), validazione del nome file (tre handler), lettura CSV
        (un solo lettore a righe), numeri di pagina 0-9 sostituiti da
        un enum, checklist di avvio guidata da una tabella.
      - I nomi dei tracciati vengono ora "escapati" nell'HTML.
      - Senza display il riepilogo di fine sessione non blocca piu' per
        30 secondi.
      Verificato con la matrice di compilazione in docs/DRIVER.md (4
      schede, 2 lingue, tutti i driver). Su hardware resta da provare.
v11.0 funzioni nuove e quattro scelte di progetto cambiate. (!) segna
      cio' che si comporta diversamente da prima.
      - (!) Log fitto: la sessione non e' piu' una riga al minuto coi
        massimi ma 3 campioni al secondo (da 1 a 10, impostabile), in un
        file binario LOG_n.BIN da 24 byte a campione: circa 260 KB l'ora,
        cinque ore sulla partizione dati. Ogni campione e' il picco del
        suo intervallo dopo il filtro anti-buca, quindi il massimo dei
        campioni coincide col massimo di sessione. Il sito lo converte in
        CSV al download (colonne con segno: Lean, Pitch, G_Lat, G_Long).
        Scrittura ogni 5 s a file aperto; a memoria piena la
        registrazione si chiude da sola col suo riepilogo. I LOG_n.CSV
        dei firmware fino alla v9.16 restano scaricabili ed eliminabili.
      - (!) Traguardo e checkpoint sono linee, non cerchi: una linea di
        20 m per lato di traverso al verso di marcia. Il verso non si
        imposta: lo impara dal primo passaggio sopra i 15 km/h e lo
        conferma al secondo; finche' e' provvisorio un passaggio
        contrario lo corregge e fa ripartire il cronometro senza contare
        un giro. Il tempo e' interpolato tra le due posizioni GPS a
        cavallo della linea.
      - (!) I tempi usano l'orologio del ricevitore GPS, non millis():
        servire una pagina web tiene fermo il loop per centinaia di
        millisecondi e prima quel ritardo finiva nel tempo sul giro.
        Buffer della UART GPS portato a 1 KB per non perdere posizioni
        in quei momenti.
      - Delta sul giro migliore: il giro piu' veloce su un tracciato
        viene tenuto punto per punto (160 punti che si diradano da soli
        su giri lunghi) e il distacco in quel punto compare sulla pagina
        GIRI e sul sito. La stessa traccia sostituisce la "forma" del
        tracciato, che prima si fermava a 80 secondi di giro.
      - Pagina Grafici di una sessione sul sito: percorso colorato per
        piega, velocita', piega, impennata e forze G, per l'intera
        sessione o un giro, con cursore che si scorre col dito. Il
        dispositivo manda solo il file: decodifica e disegno li fa il
        telefono.
      - Pagina Impostazioni: nome e password della rete, campioni al
        secondo, soglie degli eventi, allarme piega. Salvate in
        /SETTINGS.BIN.
      - (!) La password del WiFi non e' piu' in Config.h (e quindi nel
        repo): ogni dispositivo se ne genera una alla prima accensione
        del WiFi e la mostra sull'OLED. Quella vecchia resta nella
        cronologia di git: non va piu' usata.
      - Aggiornamento firmware dal sito (OTA), con controllo che il file
        sia un'applicazione per lo stesso chip prima di scrivere.
      - src/core/: la logica che non tocca l'hardware (linee, cronometro,
        traccia e delta, orologio GPS, filtro anti-buca ed eventi,
        formato del log, regole delle impostazioni) e' C++ senza
        Arduino, con 39 test che girano sul PC: pio test -e native.
      - platformio.ini accanto allo sketch Arduino, per le quattro
        schede.
      - (!) Formati su flash cambiati: tracciati (WST5) e sessioni
        ripartono da zero; i record dei canali (WSR3) restano.
      - OLED: tratto sulla linea del titolo che dice in quale delle dieci
        pagine sei; pagina Piega con freccia del lato, barra che cresce
        da quella parte e due tacche sulla soglia dell'allarme (si
        stringono col rischio grip); simbolo dei gradi disegnato invece
        della "o"; barra del rischio sul Meteo; anello di 0,5 G sul
        mirino; sulla pagina Giri il delta in negativo quando sei avanti
        e il numero del giro in corso.
      - Sito: colonna larga al massimo 760 px anche sul computer; sulla
        pagina Live piega (col lato e una barra) e velocita' in grande,
        stato della connessione (prima, a WiFi caduto, restavano a
        schermo gli ultimi valori), giri ed eventi in riquadri invece che
        in una riga di testo, traccia centrata. Grafici nitidi sui
        telefoni (disegnati alla densita' dello schermo). Elenco
        sessioni con tracciato e giri di ognuna, memoria con barra.
      - tools/oled-preview: le schermate OLED disegnate sul PC dal codice
        vero, per le immagini della documentazione. README con schermate
        del display e del sito in tutte e due le lingue e schema dei
        collegamenti.
      Verificato: test sul PC, matrice di compilazione Arduino, build
      PlatformIO dell'ESP32 classico, pagina Grafici provata in un
      browser con una sessione sintetica. Su hardware resta da provare
      tutto, v10.0 compresa.

v11.1 settori, confronto tra giri, alimentazione dalla moto.
      - Giro ideale (somma dei settori migliori della sessione) sulla
        pagina Live e nei Grafici. Live: settori dell'ultimo giro col
        distacco dal migliore della sessione. Grafici: tabella dei giri
        col distacco dal migliore, settore migliore di ogni colonna in
        verde e riga del giro ideale.
      - Due giri sovrapposti nei Grafici: scelto un giro, se ne sceglie
        un secondo. I due sono confrontati nello stesso punto della
        pista (ognuno diviso in 300 frazioni della propria lunghezza),
        con velocita', piega, impennata e G laterale sovrapposti, il
        secondo giro sulla mappa e un grafico del distacco punto per
        punto.
      - Alimentazione dalla moto (opzionale, POWER_SENSE_PIN in
        Config.h): un partitore legge i 12 V sotto chiave; quando restano
        sotto 6 V per 60 ms la registrazione viene chiusa col suo
        riepilogo, mentre un supercondensatore tiene acceso l'ESP32.
        Schema e componenti in HARDWARE.md, mai montati.
      - OLED: barra in alto con icone (GPS col numero di satelliti,
        bandiera del traguardo, WiFi, pallino REC col tempo di
        registrazione; tensione della moto con POWER_SENSE_PIN) al posto
        di "WHEELSTAT SAT REC". Piega con i massimi della sessione ai
        lati; Forza G con valori a due decimali e scia dell'ultimo
        secondo sul mirino; Memoria con barra, spazio libero e ore di
        registrazione rimaste; tempi in m:ss.
      - OLED: pagina 10, QR del WiFi (solo a WiFi acceso): inquadrandolo
        il telefono si collega senza digitare la password. Usa il
        generatore QR del core ESP32 e viene ricalcolato solo quando
        cambiano nome o password.
      - Sito: portale captive (DNS che risponde a tutto e rimando alla
        home), cosi' il telefono appena collegato propone di aprire il
        sito. Barra in alto fissa con icone, su una riga anche sui
        telefoni stretti; contenuto che entra in dissolvenza; sessioni
        come schede che aprono i grafici, con CSV/GPX/Elimina sotto;
        colonna A-B nel Confronta; record dei tracciati a griglia; ore di
        registrazione che si aggiornano mentre cambi i campioni al
        secondo; indicatore di caricamento nei Grafici.
      - docs/COLLAUDO.md: le prove in ordine, dal banco alla pista.
      - Documenti in inglese in docs/en/ (ARCHITECTURE, DRIVERS,
        HARDWARE, TESTING); il CHANGELOG resta in italiano.
      Verificato: compilazione dell'ESP32 classico con e senza
      POWER_SENSE_PIN, test sul PC, pagine Live e Grafici in un browser
      con dati sintetici. Su hardware resta da provare tutto.

v12.0 componenti riconosciuti da soli, molti nuovi.
      - (!) Niente piu' scelta del sensore meteo, dell'IMU e del GPS in
        Config.h: all'avvio il firmware li cerca e usa quello che
        risponde. Le macro WEATHER_*, IMU_BNO055, GPS_* e gli indirizzi
        I2C_ADDR_* sono sparite; restano il controller del display
        (non si puo' chiedere), DISPLAY_SPI, IMU_WITH_BNO055/BNO08X (le
        loro librerie costano flash) e l'orientamento.
      - Driver un file per famiglia di chip (src/drivers/weather/, imu/,
        gps/), con un elenco per categoria provato in ordine. I sensori
        meteo e le IMU senza fusione sono scritti senza libreria, dal
        datasheet: conversioni e checksum in src/core/SensorMath, provati
        sul PC. Le librerie meteo Adafruit non servono piu'.
      - Meteo: DHT22, DHT11, SHT4x, SHT3x, SHTC3, HDC1080, Si7021/HTU21D,
        HTU31D, AHT10/20/21/25, AM2320, BME280 (il BMP280 viene
        riconosciuto e spiegato). Letture in due tempi: nessun sensore
        blocca il ciclo aspettando la conversione.
      - IMU: BNO055 (percorso invariato), BNO085/086 (libreria, da
        attivare), MPU6050/6500/9250, ICM-20602/20689, ICM-20948,
        LSM6DS3/DSL/DSO/DSOX, ISM330DHCX. Per tutte tranne il BNO055,
        filtro di Madgwick in src/core/Ahrs con la correzione per curva e
        frenata dalla velocita' GPS: senza, in curva la piega si perde. Lo
        zero del giroscopio e' misurato all'avvio. Posizione nella
        scatola con IMU_AXES.
      - GPS: velocita' della seriale cercata (9600, 38400, 115200,
        57600), famiglia riconosciuta (u-blox 6/7/M8/M9 coi messaggi
        vecchi, M10 con CFG-VALSET; CASIC e Airoha coi loro comandi),
        seriale portata a 115200 e frequenza a 10 Hz (5 se rifiutata),
        poi frequenza misurata e timeout del fix calcolato da quella.
      - Display: indirizzo 0x3C/0x3D trovato da solo; SSD1306 e SH1106
        anche su SPI (DISPLAY_SPI).
      - microSD (SPI, FAT32): se all'avvio c'e' una scheda le sessioni
        vanno li'. Non sul C3. Dimensioni a 64 bit e mostrate in KB/MB/GB.
      - La schermata d'avvio scrive il nome del componente trovato dopo
        averlo cercato.
      Verificato: 52 test sul PC, compilazione delle combinazioni di
      schede, lingue, display, BNO08x, microSD. Nessuno dei componenti
      nuovi e' mai stato collegato.
```
