# Storico versioni

Trasferito dall'intestazione di WheelStat.ino nel refactor di alleggerimento: erano 112 righe di commento in cima al file principale, e da quando il progetto e' in git il posto giusto per la cronologia e' un documento a parte.

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
```
