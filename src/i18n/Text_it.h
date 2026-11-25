#pragma once

// WheelStat - testi in italiano. Regole e spiegazioni in Text.h.

#define TR_HTML_LANG "it"

// ===========================================================================
// OLED (solo ASCII, attenzione alle larghezze)
// ===========================================================================

// --- Avvio e calibrazione ---
#define TR_O_BOOT_TITLE       "COMPONENTI"
#define TR_O_BOOT_WEATHER     "METEO"
#define TR_O_BOOT_STORAGE     "MEM"
#define TR_O_CAL_TITLE        "CALIBRAZIONE IMU"
#define TR_O_CAL_SKIP         "[OK] salta"
#define TR_O_CAL_DONE         "CALIBRATO"
#define TR_O_CAL_ANY_KEY      "[tasto] per iniziare"

// --- Barra di stato ---

// --- Righe e sigle comuni ---
#define TR_O_ROW_LEAN         "Piega"
#define TR_O_ROW_PITCH        "Imp/St"
#define TR_O_ROW_GLAT         "G lat"
#define TR_O_ROW_GLON         "G lon"
#define TR_O_LBL_RIGHT        "Dx "
#define TR_O_LBL_LEFT         "Sx "
#define TR_O_LBL_WHEELIE      "Im "
#define TR_O_LBL_STOPPIE      "St "
#define TR_O_LBL_ACCEL        "Ac "
#define TR_O_LBL_BRAKE        "Fr "
#define TR_O_SPEED            "Vel"
#define TR_O_STATUS           "Stato"
#define TR_O_START            "Avvio"
#define TR_O_KEY_OK           "tasto [OK]"
#define TR_O_KEY_LOG          "tasto [LOG]"

// --- Riepilogo di fine sessione ---
#define TR_O_SUM_MAX_1        "MASSIMI SESSIONE 1/3"
#define TR_O_SUM_MAX_2        "MASSIMI SESSIONE 2/3"
#define TR_O_SUM_EVENTS       "CONTATORE EVENTI 3/3"
#define TR_O_SUM_CONTINUE     "[tasto] continua"
#define TR_O_SUM_DURATION     "Durata "
#define TR_O_SUM_NEW          "* nuovo!"
#define TR_O_SUM_KEY          " [tasto]"
// 15 caratteri esatti: i conteggi restano incolonnati
#define TR_O_EV_WHEELIES      "Impennate    : "
#define TR_O_EV_STOPPIES      "Stoppie      : "
#define TR_O_EV_LEANS         "Pieghe       : "
#define TR_O_EV_BRAKES        "Fren.brusche : "
#define TR_O_EV_ACCELS        "Acc.brusche  : "

// --- Pagine ---
#define TR_O_LEAN_TITLE       "ANGOLO DI PIEGA"
#define TR_O_GRIP_DANGER      "! PERICOLO GRIP !"

#define TR_O_WEATHER_TITLE    "METEO E GRIP"
#define TR_O_AIR              "Aria"
#define TR_O_HUMIDITY         "Umidita"
#define TR_O_RISK             "Rischio"

#define TR_O_G_TITLE          "FORZA G"

#define TR_O_WHEELIE_TITLE    "IMPENNATA"
#define TR_O_STOPPIE_TITLE    "STOPPIE"

#define TR_O_RECORDS_1        "RECORD 1/2"
#define TR_O_RECORDS_2        "RECORD 2/2"

#define TR_O_STORAGE_TITLE    "MEMORIA"
#define TR_O_FREE             "Libera"
#define TR_O_ERROR            "ERRORE!"
#define TR_O_FILE             "File"
#define TR_O_TIME_LEFT        "Resta"
#define TR_O_QR_1             "Inquadra"
#define TR_O_QR_2             "col"
#define TR_O_QR_3             "telefono"

#define TR_O_WIFI_TITLE       "WIFI"
#define TR_O_NETWORK          "Rete"
#define TR_O_PASSWORD         "Pass"
#define TR_O_SITE             "Sito"
#define TR_O_CLIENTS          "Client"
#define TR_O_OK_OFF           "  [OK] off"
#define TR_O_OFF              "SPENTO"

#define TR_O_GPS_TITLE        "GPS"
#define TR_O_GPS_MISSING      "ASSENTE"
#define TR_O_MODULE           "Modulo"
#define TR_O_GPS_SILENT       "non parla"
#define TR_O_CHECK            "Verifica"
#define TR_O_GPS_CHECK_WIRES  "TX/RX/3.3V"
#define TR_O_NO_FIX           "NESSUN FIX"
#define TR_O_SATS_LOCKED      " agganciati"
#define TR_O_WAIT             "Attesa"
#define TR_O_OPEN_SKY         "cielo libero"

#define TR_O_LAPS_TITLE       "GIRI"
#define TR_O_LAPS_NOT_SET     "da impostare"
#define TR_O_LAPS_NO_FIX      "GPS assente"
#define TR_O_LAP              "Giro"
#define TR_O_LAP_WAITING      "in attesa"
#define TR_O_LAST             "Ultimo"
#define TR_O_BEST             "Record"

// --- Popup ---
#define TR_O_POPUP_ZERO_OK    "CALIBRAZIONE OK"
#define TR_O_POPUP_NO_FIX     "GPS senza fix!"
#define TR_O_POPUP_FINISH_OK  "TRAGUARDO OK"
#define TR_O_POPUP_STORAGE_FULL "MEMORIA PIENA"

// --- Messaggi fissi ---
#define TR_O_UPDATING         "AGGIORNAMENTO..."
#define TR_O_POWER_SAVED      "SESSIONE SALVATA"

// ===========================================================================
// Canali (tabelle del sito)
// ===========================================================================
#define TR_CH_LEAN_RIGHT      "Piega destra"
#define TR_CH_LEAN_LEFT       "Piega sinistra"
#define TR_CH_WHEELIE         "Impennata"
#define TR_CH_STOPPIE         "Stoppie"
#define TR_CH_GLAT_RIGHT      "G laterale destra"
#define TR_CH_GLAT_LEFT       "G laterale sinistra"
#define TR_CH_G_ACCEL         "G in accelerazione"
#define TR_CH_G_BRAKE         "G in frenata"
#define TR_CH_SPEED           "Velocità"

// ===========================================================================
// Sito (UTF-8, niente ' ne' ")
// ===========================================================================

// --- Navigazione e parole comuni ---
#define TR_W_NAV_SESSIONS     "Sessioni"
#define TR_W_NAV_LIVE         "Live"
#define TR_W_NAV_TRACKS       "Tracciati"
#define TR_W_NAV_COMPARE      "Confronta"
#define TR_W_NAV_SETTINGS     "Impostazioni"
#define TR_W_DELETE           "Elimina"
#define TR_W_FREE_SHORT       "libera"
#define TR_W_SESSIONS_WORD    " sessioni"
#define TR_W_LAPS_WORD        " giri"
#define TR_W_SECTORS_WORD     " settori"

// --- Pagina Sessioni ---
#define TR_W_SESSIONS_TITLE        "Sessioni in memoria"
#define TR_W_CONFIRM_DELETE_FILE   "Eliminare definitivamente?"
#define TR_W_NO_SESSIONS           "Nessuna sessione trovata."
#define TR_W_STORAGE               "Memoria"
#define TR_W_TRACKS_AND_RECORDS    "Tracciati e record"
#define TR_W_REC_IN_PROGRESS       " in corso: grafici, download e cancellazione sono disponibili a registrazione ferma."
#define TR_W_CHARTS                "Grafici"
#define TR_W_OLD_FORMAT            "vecchio formato"

// --- Pagina di una sessione (grafici) ---
// Dizionario JavaScript: un oggetto letterale, stringhe tra apici singoli.
#define TR_W_SESSION_JS_DICT \
  "{loading:'Carico la sessione...'," \
  "error:'Impossibile leggere la sessione (se stai registrando, ferma prima la registrazione).'," \
  "noGps:'Nessuna posizione GPS in questo tratto',all:'Tutta la sessione'," \
  "lap:'Giro',laps:'Tempi sul giro',free:'libera',duration:'Durata'," \
  "maxSpeed:'Velocità max',maxLean:'Piega max',right:'dx',left:'sx',best:'Giro migliore'," \
  "ideal:'Giro ideale',compare:'Confronta con',time:'Tempo',gap:'Distacco'," \
  "deltaOf:'Distacco del giro',deltaOn:'sul giro'," \
  "hint:'Tocca o scorri un grafico per leggere i valori in quel punto.'," \
  "range:'Mostra',speed:'Velocità (km/h)',lean:'Piega (°)',leanShort:'piega'," \
  "pitch:'Impennata (+) / stoppie (-) (°)',g:'Forze G',glat:'laterale'," \
  "glon:'longitudinale'," \
  "noFooter:'Registrazione interrotta senza riepilogo (alimentazione persa?): mancano i tempi sul giro.'}"

// --- Pagina Live ---
#define TR_W_LIVE_LEAN        "Piega &deg;"
#define TR_W_LIVE_PITCH       "Imp./Stoppie &deg;"
#define TR_W_LIVE_RISK        "Rischio %"
#define TR_W_LIVE_GLAT        "G laterale"
#define TR_W_LIVE_GLON        "G longitudinale"
#define TR_W_LIVE_AIR         "Aria &deg;C / Umid %"
#define TR_W_LIVE_SPEED       "Velocità GPS km/h"
#define TR_W_LIVE_DELTA       "Delta sul giro s"
#define TR_W_LIVE_TRACE       "Traccia percorsa"
#define TR_W_LIVE_CLEAR       "Pulisci"
#define TR_W_LIVE_ANGLES      "Angoli (&plusmn;60&deg;)"
#define TR_W_LIVE_LEGEND_LEAN "piega"
#define TR_W_LIVE_LEGEND_PITCH "impennata(+) stoppie(-)"
#define TR_W_LIVE_GFORCES     "Forze G (&plusmn;1.5)"
#define TR_W_LIVE_LEGEND_LAT  "laterale"
#define TR_W_LIVE_LEGEND_LON  "longitudinale"
#define TR_W_LIVE_EVENTS      "Eventi"
#define TR_W_LIVE_LAPS        "Giri"
#define TR_W_LIVE_BACK        "&larr; Scarica le sessioni dalla memoria"
// Dizionario JavaScript della pagina live: un oggetto letterale, stringhe
// tra apici singoli.
#define TR_W_LIVE_JS_DICT \
  "{wait:'In attesa di punti GPS...',noFix:'NO FIX'," \
  "evW:'Impennate',evS:'Stoppie',evL:'Pieghe',evB:'Frenate brusche'," \
  "evA:'Accelerate brusche',lap:'Giro',last:'Ultimo',best:'Record'," \
  "ideal:'Ideale',top:'migliore',right:'Dx',left:'Sx'," \
  "online:'Collegato',offline:'Connessione persa'," \
  "noFinish:'Traguardo non impostato (pagina GIRI sul display, o scheda Tracciati)'}"

// --- Pagina Tracciati ---
#define TR_W_LEADERBOARD           "Classifica"
#define TR_W_NO_DATA_YET           "Non c’è ancora nessun dato: registra qualche sessione con un traguardo impostato."
#define TR_W_FASTEST_LAP_EVER      "Giro più veloce di sempre: "
#define TR_W_ON_TRACK              " su "
#define TR_W_MOST_VISITED          "Tracciato più frequentato: "
#define TR_W_ACTIVE_TRACK          "Tracciato attivo"
#define TR_W_FREE_MODE             "Modalità libera"
#define TR_W_NO_TRACK_SELECTED     "(nessun tracciato selezionato)"
#define TR_W_ACTIVE                "attivo"
#define TR_W_CHECKPOINT_ONE        " settore intermedio definito"
#define TR_W_CHECKPOINT_MANY       " settori intermedi definiti"
#define TR_W_ADD_CHECKPOINT        "Aggiungi settore qui"
#define TR_W_NEED_FIX_CHECKPOINT   "(serve il fix GPS per aggiungerne altri)"
#define TR_W_CONFIRM_CLEAR_CHECKPOINTS "Rimuovere tutti i settori di questo tracciato?"
#define TR_W_CLEAR_CHECKPOINTS     "Rimuovi settori"
#define TR_W_BACK_TO_FREE          "Torna alla modalità libera"
#define TR_W_RECORDS               "Record"
#define TR_W_MINUTES_TOTAL         " minuti totali"
#define TR_W_CONFIRM_RESET_RECORDS "Azzerare tutti i record storici?"
#define TR_W_RESET                 "Azzera"
#define TR_W_CREATE_TITLE          "Crea un nuovo tracciato qui"
#define TR_W_CREATE_HELP           "Fermati sulla linea di partenza/arrivo col fix GPS già agganciato, poi dai un nome: il traguardo sarà una linea attraverso la pista in questo punto. Il verso di marcia lo impara da solo al primo passaggio; la forma del circuito e il giro di riferimento per il delta si registrano al primo giro completo."
#define TR_W_NO_FIX_CREATE         "GPS senza fix: aspetta l’aggancio prima di creare."
#define TR_W_TRACK_NAME            "Nome del tracciato"
#define TR_W_CREATE_HERE           "Crea qui"
#define TR_W_SAVED_TRACKS          "Tracciati salvati"
#define TR_W_NO_SHAPE              "Forma non ancora registrata (completa un giro)"
#define TR_W_LAP_RECORD            "Record giro: "
#define TR_W_SELECT                "Seleziona"
#define TR_W_CONFIRM_DELETE_TRACK  "Eliminare questo tracciato e i suoi record?"
#define TR_W_NO_TRACKS             "Nessun tracciato salvato."

// --- Pagina Confronta ---
#define TR_W_COMPARE_TITLE         "Confronta due sessioni"
#define TR_W_COMPARE_NEED_TWO      "Servono almeno due sessioni registrate per poterle confrontare"
#define TR_W_COMPARE_NONE          " (per ora non ce n’è nessuna)."
#define TR_W_COMPARE_ONE           " (per ora ce n’è solo una)."
#define TR_W_SESSION_A             "Sessione A"
#define TR_W_SESSION_B             "Sessione B"
#define TR_W_COMPARE_BUTTON        "Confronta"
#define TR_W_COMPARE_NO_LAPS       "Una delle due sessioni non ha giri cronometrati da confrontare (serve un traguardo impostato durante la registrazione)."
#define TR_W_DIFFERENT_TRACKS      "Tracciati diversi ("
#define TR_W_DIFFERENT_TRACKS_END  "): il confronto resta possibile ma ha meno senso."
#define TR_W_LAP_TIMES             "Tempi giro"
#define TR_W_LAP                   "Giro"
#define TR_W_BEST                  "Migliore"

// --- Pagina Impostazioni ---
#define TR_W_SET_SAVED             "Impostazioni salvate."
#define TR_W_SET_WIFI_REJECTED     "Nome o password della rete non validi: sono rimasti quelli di prima. Il resto è stato salvato."
#define TR_W_SET_WIFI              "Rete WiFi"
#define TR_W_SET_WIFI_NAME         "Nome"
#define TR_W_SET_WIFI_PASSWORD     "Password"
#define TR_W_SET_WIFI_HELP         "Nome fino a 12 caratteri, password da 8 a 12, senza lettere accentate né spazi nella password: devono stare su una riga del display, dove le puoi rileggere. Valgono dalla prossima accensione del WiFi."
#define TR_W_SET_LOG               "Registrazione"
#define TR_W_SET_LOG_RATE          "Campioni al secondo"
#define TR_W_SET_LOG_HELP_1        "Con questa frequenza lo spazio libero basta per circa "
#define TR_W_SET_LOG_HELP_2        " ore di registrazione."
#define TR_W_SET_EVENTS            "Eventi e allarme"
#define TR_W_SET_EVENTS_HELP       "Soglie oltre le quali una manovra viene contata come evento."
#define TR_W_SET_LEAN_ALERT        "Piega massima"
#define TR_W_SET_LEAN_ALERT_HELP   "Piega oltre la quale il display segnala pericolo su asfalto perfetto; il rischio grip la abbassa."
#define TR_W_SET_SAVE              "Salva"
#define TR_W_SET_RESET             "Ripristina i valori iniziali"
#define TR_W_SET_CONFIRM_RESET     "Riportare soglie e frequenza ai valori iniziali? Nome e password della rete restano."
// Nomi delle soglie degli eventi
#define TR_W_EV_WHEELIES           "Impennata"
#define TR_W_EV_STOPPIES           "Stoppie"
#define TR_W_EV_LEANS              "Piega"
#define TR_W_EV_BRAKES             "Frenata brusca"
#define TR_W_EV_ACCELS             "Accelerata brusca"

// --- Aggiornamento firmware ---
#define TR_W_UPDATE_TITLE          "Aggiornamento firmware"
#define TR_W_UPDATE_CURRENT        "Versione installata:"
#define TR_W_UPDATE_HELP           "Scegli il file WheelStat.ino.bin compilato per questa scheda (in Arduino IDE: Sketch, Esporta sketch compilato). Sessioni, tracciati e impostazioni restano. Non togliere alimentazione durante il caricamento: dura circa un minuto, poi il dispositivo si riavvia."
#define TR_W_UPDATE_CONFIRM        "Caricare questo firmware e riavviare il dispositivo?"
#define TR_W_UPDATE_BUTTON         "Carica e aggiorna"
#define TR_W_UPDATE_DONE           "Firmware aggiornato: il dispositivo si sta riavviando. Riaccendi il WiFi dal display e ricollegati."

// ===========================================================================
// Risposte di errore del sito (testo semplice, UTF-8)
// ===========================================================================
#define TR_E_STOP_RECORDING   "Ferma la registrazione prima di fare questa operazione."
#define TR_E_FILE_NOT_FOUND   "File non trovato."
#define TR_E_READ_ERROR       "Errore di lettura dalla memoria."
#define TR_E_NOT_A_SESSION    "Questo file non è una sessione leggibile da questa versione del firmware."
#define TR_E_NOT_FIRMWARE     "Il file non è un firmware per questa scheda: serve WheelStat.ino.bin compilato per lo stesso tipo di ESP32. Non è stato scritto nulla."
#define TR_E_UPDATE_FAILED    "Aggiornamento non riuscito: il firmware di prima è rimasto al suo posto."
#define TR_E_PAGE_NOT_FOUND   "Pagina inesistente"
