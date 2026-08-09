/*
  WheelStat - InterfacciaWeb.ino
  ===========================================================================
  Il sito servito dall'access point "WheelStat" (http://192.168.4.1):
  foglio di stile condiviso, telemetria live con traccia GPS disegnata in
  tempo reale, gestione tracciati (creazione, settori, classifica),
  confronto tra sessioni, download di CSV/GPX ed eliminazione file.

  Fa parte dello stesso sketch di WheelStat.ino e PagineOled.ino: Arduino
  concatena i tre file in un unico programma, le funzioni e le variabili
  globali definite altrove (sensori, stato dei tracciati, record,
  oggetto "server") sono visibili qui esattamente come se fosse tutto
  nello stesso file.
  ===========================================================================
*/

// ===========================================================================
// 13. WIFI E SITO WEB
// ===========================================================================

// Foglio di stile condiviso dalle tre pagine ("/", "/live", "/tracciati"):
// un solo posto per colori, tipografia e componenti (card, bottoni,
// badge, liste), cosi' le tre pagine sembrano lo stesso prodotto invece
// di tre pagine cucite separatamente in momenti diversi. Servito da
// webStyleCss() con un piccolo Cache-Control: sulla stessa visita il
// telefono lo scarica una volta sola.
const char STILE_CSS[] PROGMEM = R"rawliteral(
/* Tema scuro: si legge meglio col telefono sotto il sole. --good/--warn/
   --danger sono le stesse identiche tonalita' usate nel canvas della
   traccia GPS (vedi coloreTraccia() nello script della pagina live):
   qui non posso scrivere var(--good) dentro un canvas 2D, ma i valori
   esadecimali sono stati copiati a mano dagli stessi tre colori, cosi'
   il significato (piega bassa/media/alta) resta coerente ovunque. */
:root{
  --bg:#0d1015; --card:#161b22; --card2:#1e242d; --border:#262e3a;
  --text:#e9edf3; --dim:#8b93a4; --accent:#4cc9ff; --ink:#04222f;
  --danger:#ff5d6c; --good:#4ede93; --warn:#ffd166;
}
*{box-sizing:border-box}
body{
  margin:0;padding:16px 16px 32px;background:var(--bg);color:var(--text);
  font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;
  -webkit-tap-highlight-color:transparent;
}
a{color:var(--accent);text-decoration:none}
a:active{opacity:.7}

/* Intestazione comune: marchio + navigazione a tab, la stessa su tutte
   le pagine (vedi barraNavigazione() nel firmware) */
.topbar{display:flex;align-items:center;justify-content:space-between;
  margin-bottom:16px;flex-wrap:wrap;gap:8px}
.brand{font-size:19px;font-weight:700}
.brand span{color:var(--accent)}
nav.tabs{display:flex;gap:6px}
nav.tabs a{font-size:13px;padding:6px 12px;border-radius:999px;color:var(--dim);
  background:var(--card);border:1px solid var(--border)}
nav.tabs a.on{color:var(--ink);background:var(--accent);border-color:var(--accent);font-weight:600}

h2{font-size:13px;margin:20px 0 8px;color:var(--dim);text-transform:uppercase;
  letter-spacing:.6px;font-weight:600}
h2:first-of-type{margin-top:4px}
p{line-height:1.45}

.card{background:var(--card);border:1px solid var(--border);border-radius:14px;
  padding:14px;margin-bottom:12px}

.grid{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin-bottom:14px}
.stat{background:var(--card);border:1px solid var(--border);border-radius:12px;
  padding:10px 6px;text-align:center}
.stat .e{font-size:10.5px;color:var(--dim);text-transform:uppercase;letter-spacing:.3px}
.stat .v{font-size:20px;font-weight:700;margin-top:2px}

canvas{width:100%;display:block;background:var(--card);border:1px solid var(--border);
  border-radius:14px;margin-bottom:6px}
.caption{font-size:11.5px;color:var(--dim);margin:0 2px 6px;display:flex;
  justify-content:space-between;align-items:center;flex-wrap:wrap;gap:6px}

ul.list{list-style:none;margin:0;padding:0}
ul.list li{background:var(--card);border:1px solid var(--border);border-radius:12px;
  padding:10px 12px;margin-bottom:8px;font-size:14px}

.btn{display:inline-block;padding:8px 15px;border-radius:999px;font-weight:600;
  font-size:13px;background:var(--accent);color:var(--ink);border:none;
  text-decoration:none;cursor:pointer}
.btn.ghost{background:transparent;color:var(--text);border:1px solid var(--border)}
.btn.danger{background:transparent;color:var(--danger);border:1px solid rgba(255,93,108,.35)}
.btn.small{padding:6px 12px;font-size:12px}

.badge{display:inline-block;font-size:11px;padding:2px 9px;border-radius:999px;
  background:var(--card2);color:var(--dim)}
.badge.on{background:rgba(76,201,255,.15);color:var(--accent)}

/* 16px e non meno: sotto questa soglia iOS Safari zooma automaticamente
   la pagina quando l'input riceve il focus, fastidioso su un sito
   pensato per essere usato dal telefono con una mano sola. */
input,button{font-size:16px;padding:9px 12px;border-radius:10px;border:1px solid var(--border);
  background:var(--card2);color:var(--text);font-family:inherit}
input::placeholder{color:var(--dim)}
button{cursor:pointer}

table{width:100%;border-collapse:collapse;font-size:13.5px}
td{padding:6px 4px}
tr+tr{border-top:1px solid var(--border)}

#rec{color:var(--danger);font-weight:700;font-size:13px;display:flex;
  align-items:center;gap:7px}

/* Il pallino della registrazione pulsa, come l'asterisco di "* REC" sul
   display: e' l'unica cosa in pagina che guadagna davvero dal movimento,
   perche' deve dire "sta succedendo adesso" con la coda dell'occhio.
   Il testo accanto NON si muove: i numeri della telemetria devono
   restare fermi e leggibili. */
#rec .dot{width:9px;height:9px;border-radius:50%;background:var(--danger);
  animation:pulsa 1s ease-in-out infinite}
@keyframes pulsa{50%{opacity:.25;transform:scale(.8)}}

/* Riscontro al tocco: sul telefono, magari coi guanti, serve capire che
   il dito ha preso. 90 ms e' sotto la soglia in cui si percepisce
   un'attesa, sopra quella in cui non si vedrebbe niente. */
.btn,nav.tabs a{transition:background .12s,color .12s,transform .09s}
.btn:active,nav.tabs a:active{transform:scale(.96)}

/* Chi ha chiesto meno animazioni al sistema le ottiene: qui non si perde
   nessuna informazione, il pallino resta rosso e fermo. */
@media (prefers-reduced-motion:reduce){
  *{animation:none!important;transition:none!important}
}

/* Stati del valore, con gli stessi tre colori della legenda della
   traccia GPS: verde/giallo/rosso vogliono dire la stessa cosa ovunque */
.v.ok{color:var(--good)} .v.med{color:var(--warn)} .v.hi{color:var(--danger)}
.v.off{color:var(--dim);font-size:15px}
.dim{color:var(--dim)}
)rawliteral";

// Spedisce il CSS condiviso dalla flash. Cache-Control breve: entro la
// stessa visita il telefono non lo riscarica a ogni cambio pagina, ma
// non resta "incollato" se in futuro lo aggiorno ricaricando il firmware.
void webStyleCss() {
  server.sendHeader("Cache-Control", "max-age=3600");
  server.send_P(200, "text/css", STILE_CSS);
}

// Intestazione comune: marchio + tab di navigazione, con la pagina
// corrente evidenziata. Usata dalle pagine costruite dinamicamente
// (webElencoFile, webTracciati); la pagina live e' un PROGMEM statico e
// ripete lo stesso markup a mano, ma punta allo stesso /style.css: viste
// diverse, stesso vestito.
String barraNavigazione(const char *paginaAttiva) {
  String html = F("<div class='topbar'><div class='brand'>Wheel<span>Stat</span></div>"
    "<nav class='tabs'>");
  html += F("<a href='/'");
  if (strcmp(paginaAttiva, "home") == 0) html += F(" class='on'");
  html += F(">Sessioni</a><a href='/live'");
  if (strcmp(paginaAttiva, "live") == 0) html += F(" class='on'");
  html += F(">Live</a><a href='/tracciati'");
  if (strcmp(paginaAttiva, "tracciati") == 0) html += F(" class='on'");
  html += F(">Tracciati</a><a href='/confronta'");
  if (strcmp(paginaAttiva, "confronta") == 0) html += F(" class='on'");
  html += F(">Confronta</a></nav></div>");
  return html;
}

// Pagina live (HTML+CSS+JS): il telefono e' collegato
// all'access point della moto e NON ha internet, quindi niente librerie
// esterne.
const char PAGINA_LIVE_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>WheelStat - Live</title>
<link rel="stylesheet" href="/style.css">
</head><body>

<div class="topbar"><div class="brand">Wheel<span>Stat</span></div>
<nav class="tabs">
<a href="/">Sessioni</a><a href="/live" class="on">Live</a><a href="/tracciati">Tracciati</a><a href="/confronta">Confronta</a>
</nav></div>

<div class="caption"><span id="rec"></span></div>

<!-- Griglia dei valori istantanei -->
<div class="grid">
<div class="stat"><div class="e">Piega &deg;</div><div class="v" id="vp">--</div></div>
<div class="stat"><div class="e">Imp./Stoppie &deg;</div><div class="v" id="vw">--</div></div>
<div class="stat"><div class="e">Rischio %</div><div class="v" id="vr">--</div></div>
<div class="stat"><div class="e">G laterale</div><div class="v" id="vgl">--</div></div>
<div class="stat"><div class="e">G longitudinale</div><div class="v" id="vgn">--</div></div>
<div class="stat"><div class="e">Aria &deg;C / Umid %</div><div class="v" id="vm">--</div></div>
<div class="stat"><div class="e">Velocita GPS km/h</div><div class="v" id="vgps">--</div></div>
<div class="stat"><div class="e">Satelliti GPS</div><div class="v" id="vsat">--</div></div>
</div>

<!-- Traccia GPS della sessione corrente: forma del percorso colorata
     per intensita' di piega. Si azzera da sola a ogni nuova REC, o a
     mano col pulsante qui sotto (utile per i giri di prova). -->
<div class="caption"><span>Traccia percorsa &mdash;
<span style="color:var(--good)">&lt;20&deg;</span> /
<span style="color:var(--warn)">20-35&deg;</span> /
<span style="color:var(--danger)">&gt;35&deg;</span></span>
<button class="btn ghost small" onclick="resetTraccia()">Pulisci</button></div>
<canvas id="trk" height="220"></canvas>

<!-- Grafici scorrevoli: ~36 secondi di storia (120 punti x 300 ms) -->
<div class="caption"><span>Angoli (&plusmn;60&deg;)</span>
<span><span style="color:var(--accent)">piega</span> /
<span style="color:var(--warn)">impennata(+) stoppie(-)</span></span></div>
<canvas id="ca" height="130"></canvas>
<div class="caption"><span>Forze G (&plusmn;1.5)</span>
<span><span style="color:var(--accent)">laterale</span> /
<span style="color:var(--warn)">longitudinale</span></span></div>
<canvas id="cg" height="130"></canvas>

<div class="card" id="ev">Eventi: --</div>
<div class="card" id="lap">Giri: --</div>
<p><a href="/">&larr; Scarica le sessioni dalla memoria</a></p>

<script>
// Serie storiche dei grafici: N campioni, i piu' vecchi escono a sinistra
var N=120;
var sPiega=[],sPitch=[],sLat=[],sLon=[];

// Disegna due serie (a, b) su un canvas: fondo scala +/-max,
// linea dello zero al centro
function disegna(id,a,b,max){
 var cv=document.getElementById(id);
 cv.width=cv.clientWidth;             // adatta la risoluzione alla larghezza reale
 var c=cv.getContext('2d'),W=cv.width,H=cv.height;
 c.clearRect(0,0,W,H);
 c.strokeStyle='#2a3140';c.beginPath();c.moveTo(0,H/2);c.lineTo(W,H/2);c.stroke();
 function linea(s,col){
  c.strokeStyle=col;c.lineWidth=2;c.beginPath();
  for(var i=0;i<s.length;i++){
   var x=i*W/(N-1);
   var y=H/2-(s[i]/max)*(H/2-4);      // scala il valore sull'altezza
   y=Math.max(2,Math.min(H-2,y));     // niente linee fuori dal riquadro
   if(i==0)c.moveTo(x,y);else c.lineTo(x,y);
  }
  c.stroke();
 }
 linea(a,'#4cc9ff');linea(b,'#ffd166');
}

// Accoda un campione e scarta il piu' vecchio quando la serie e' piena
function punta(s,v){s.push(v);if(s.length>N)s.shift();}

// Scrive un valore e gli assegna la classe di stato (ok/med/hi/off), che
// il CSS traduce in colore. La classe si riscrive solo se e' cambiata:
// toccare className 3 volte al secondo su ogni riquadro farebbe
// ricalcolare lo stile senza motivo.
function stato(id,testo,classe){
 var e=document.getElementById(id);
 e.textContent=testo;
 var c='v'+(classe?' '+classe:'');
 if(e.className!=c) e.className=c;
}

// Formatta una durata in ms come "m:ss.d", stessa idea di stampaTempoGiro()
// nel firmware (senza ore: un giro cosi' lungo sarebbe comunque fuori scala)
function fmtTempo(ms){
 if(!ms)return'--';
 var s=Math.floor(ms/1000),d=Math.floor((ms%1000)/100);
 return Math.floor(s/60)+':'+(s%60<10?'0':'')+(s%60)+'.'+d;
}

// Tempi settore dell'ultimo giro, separati da "/": solo se il tracciato
// attivo ha almeno un checkpoint (d.secN>1), altrimenti il giro e' un
// settore solo e questa riga non aggiungerebbe informazione.
function fmtSettori(d){
 var s=[d.sec0,d.sec1,d.sec2];
 var out=[];
 for(var i=0;i<d.secN;i++) out.push(fmtTempo(s[i]));
 return out.join(' / ');
}

// --- Traccia GPS ------------------------------------------------------------
// Ogni punto valido diventa (x,y) in metri rispetto al primo punto della
// sessione (proiezione locale piatta: sull'area di un circuito la Terra
// e' abbastanza piatta da non aver bisogno di una vera proiezione
// cartografica). Il campo "piega" e' il valore assoluto in quel punto,
// usato solo per il colore del segmento.
var trackPts=[];
var trackRefLat=null, trackRefLon=null;
var prevRec=false;
var lastLapSet=false, lastTrgLat=0, lastTrgLon=0;

function resetTraccia(){
 trackPts=[];
 trackRefLat=null;
 trackRefLon=null;
 disegnaTraccia();
}

// Proietta (lat,lon) in metri rispetto al riferimento della traccia
// (trackRefLat/trackRefLon, il primo punto valido della sessione).
// Un solo posto per la formula di conversione: la usano sia puntaTraccia()
// per i punti del percorso sia disegnaTraccia() per il traguardo.
function proiettaLocale(lat,lon){
 var mPerGradoLat=111320;
 var mPerGradoLon=111320*Math.cos(trackRefLat*Math.PI/180);
 return {x:(lon-trackRefLon)*mPerGradoLon, y:(lat-trackRefLat)*mPerGradoLat};
}

// Accoda un punto GPS valido alla traccia, proiettandolo in metri. Tetto
// a 4000 punti (circa un'ora a 1 Hz): oltre, la sessione e' comunque un
// caso limite e si preferisce non rallentare il telefono a ridisegnare.
function puntaTraccia(lat,lon,piega){
 if(trackRefLat===null){trackRefLat=lat;trackRefLon=lon;}
 var p=proiettaLocale(lat,lon);
 if(trackPts.length<4000) trackPts.push({x:p.x,y:p.y,piega:Math.abs(piega)});
}

// Colore del segmento in base alla piega: stessa soglia "piega importante"
// del firmware (SOGLIA_EVENTO[EV_PIEGA] = 35 gradi), piu' una via di
// mezzo gialla per la piega media.
function coloreTraccia(piega){
 if(piega<20)return'#4ede93';
 if(piega<35)return'#ffd166';
 return'#ff5d6c';
}

// Ridisegna la traccia da capo (come i grafici scorrevoli): con qualche
// migliaio di punti al massimo il costo per un canvas e' trascurabile.
function disegnaTraccia(){
 var cv=document.getElementById('trk');
 cv.width=cv.clientWidth;
 var c=cv.getContext('2d'),W=cv.width,H=cv.height;
 c.clearRect(0,0,W,H);

 if(trackPts.length<2){
  c.fillStyle='#8b93a4';c.font='13px -apple-system,sans-serif';
  c.fillText('In attesa di punti GPS...',10,H/2);
  return;
 }

 // Traguardo proiettato con lo stesso riferimento della traccia (se
 // impostato): entra anche lui nel bounding box, cosi' resta visibile
 // anche quando e' fuori dal tratto di percorso registrato finora
 var trg=lastLapSet&&trackRefLat!==null ? proiettaLocale(lastTrgLat,lastTrgLon) : null;

 // Riquadro che contiene tutti i punti (e il traguardo), per adattare
 // la scala mantenendo le proporzioni reali (altrimenti il tracciato
 // risulta deformato rispetto alla forma vera del circuito)
 var minX=Infinity,maxX=-Infinity,minY=Infinity,maxY=-Infinity;
 for(var i=0;i<trackPts.length;i++){
  minX=Math.min(minX,trackPts[i].x);maxX=Math.max(maxX,trackPts[i].x);
  minY=Math.min(minY,trackPts[i].y);maxY=Math.max(maxY,trackPts[i].y);
 }
 if(trg){
  minX=Math.min(minX,trg.x);maxX=Math.max(maxX,trg.x);
  minY=Math.min(minY,trg.y);maxY=Math.max(maxY,trg.y);
 }
 var pad=16;
 var scala=Math.min((W-2*pad)/Math.max(1,maxX-minX),(H-2*pad)/Math.max(1,maxY-minY));
 function px(p){return pad+(p.x-minX)*scala;}
 function py(p){return H-pad-(p.y-minY)*scala;}  // Y invertita: nord in alto

 for(var i=1;i<trackPts.length;i++){
  c.strokeStyle=coloreTraccia(trackPts[i].piega);
  c.lineWidth=3;
  c.beginPath();
  c.moveTo(px(trackPts[i-1]),py(trackPts[i-1]));
  c.lineTo(px(trackPts[i]),py(trackPts[i]));
  c.stroke();
 }

 // Traguardo: quadratino bianco, distinto dal pallino di posizione
 if(trg){
  c.fillStyle='#fff';
  c.fillRect(px(trg)-5,py(trg)-5,10,10);
 }

 // Pallino ciano sull'ultima posizione nota: dove sono adesso
 var ultimo=trackPts[trackPts.length-1];
 c.fillStyle='#4cc9ff';
 c.beginPath();c.arc(px(ultimo),py(ultimo),5,0,Math.PI*2);c.fill();
}

// Interroga /dati, aggiorna i riquadri e ridisegna i grafici. In caso di
// errore (WiFi perso, moto spenta) non fa nulla e riprova al giro dopo.
async function giro(){
 try{
  var d=await (await fetch('/dati')).json();
  document.getElementById('vp').textContent=d.piega.toFixed(1);
  document.getElementById('vw').textContent=d.pitch.toFixed(1);
  document.getElementById('vgl').textContent=d.glat.toFixed(2);
  document.getElementById('vgn').textContent=d.glon.toFixed(2);
  document.getElementById('vm').textContent=d.temp.toFixed(0)+' / '+d.umid.toFixed(0);
  // Velocita' e rischio dicono qualcosa anche col colore, non solo col
  // numero: senza fix la velocita' non e' "zero", e' "non lo so", e un
  // rischio grip all'80% deve saltare all'occhio prima di essere letto.
  stato('vgps', d.gpsFix?d.gpsVel.toFixed(0):'NO FIX', d.gpsFix?'':'off');
  stato('vr', d.rischio.toFixed(0), d.rischio>=60?'hi':(d.rischio>=30?'med':'ok'));

  document.getElementById('vsat').textContent=d.gpsSat;
  document.getElementById('rec').innerHTML=d.rec
   ?('<span class="dot"></span>REC '+d.min+' min'):'';
  document.getElementById('ev').textContent='Eventi - Impennate: '+d.evi
   +' | Stoppie: '+d.evs+' | Pieghe: '+d.evp
   +' | Frenate brusche: '+d.evf+' | Accelerate brusche: '+d.eva;
  document.getElementById('lap').textContent=d.lapSet
   ?((d.trackName?d.trackName+' · ':'')+'Giro: '+fmtTempo(d.lapCur)
     +' | Ultimo: '+fmtTempo(d.lapLast)+' | Record: '+fmtTempo(d.lapBest)
     +' | Giri: '+d.lapNum
     +(d.secN>1?(' | Settori: '+fmtSettori(d)):''))
   :'Traguardo non impostato (pagina GIRI sul display, o /tracciati)';

  // Traccia: si azzera da sola al FRONTE di salita di REC (nuova
  // registrazione appena avviata), cosi' ogni sessione ha la sua traccia
  // pulita senza dover ricordarsi di premere "Pulisci"
  if(d.rec && !prevRec) resetTraccia();
  prevRec=d.rec;
  lastLapSet=d.lapSet; lastTrgLat=d.trgLat; lastTrgLon=d.trgLon;
  if(d.gpsFix) puntaTraccia(d.gpsLat,d.gpsLon,d.piega);
  disegnaTraccia();

  punta(sPiega,d.piega);punta(sPitch,d.pitch);punta(sLat,d.glat);punta(sLon,d.glon);
  disegna('ca',sPiega,sPitch,60);disegna('cg',sLat,sLon,1.5);
 }catch(e){}
}
setInterval(giro,300);  // circa 3 aggiornamenti al secondo
</script></body></html>
)rawliteral";

// Accende l'access point e il server web.
void avviaWiFi() {
  WiFi.mode(WIFI_AP);
  wifiAttivo = WiFi.softAP(WIFI_SSID, WIFI_PASSWORD);
  if (wifiAttivo) {
    server.begin();
    Serial.print(F("WiFi acceso -> http://"));
    Serial.println(WiFi.softAPIP());
  } else {
    Serial.println(F("ERRORE: avvio access point fallito."));
  }
}

// Spegne server e radio WiFi
void fermaWiFi() {
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  wifiAttivo = false;
  Serial.println(F("WiFi spento."));
}

// Radice del sito: elenco dei LOG_n.CSV in memoria, con download e
// cancellazione. La pagina viene costruita al volo in una String: coi log
// tipici (pochi KB di HTML)
// Pagina "Sessioni": gestione dei file registrati e basta. Tracciato
// attivo e record (canali + giro) si sono spostati su /tracciati insieme
// alla classifica: prima erano sparsi su due pagine con contenuti che si
// sovrapponevano (la stessa card "tracciato attivo" appariva qui e li',
// i record dei canali qui, i record giro li'), ora ogni pagina ha uno
// scopo solo e non ripete quello delle altre.
void webElencoFile() {
  String html;
  html.reserve(3400);  // un blocco unico: meno riallocazioni e frammentazione dello heap
  html += F("<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>WheelStat</title><link rel='stylesheet' href='/style.css'>"
    "</head><body>");
  html += barraNavigazione("home");

  html += F("<h2>Sessioni in memoria</h2><ul class='list'>");

  // Scorro la radice della flash e tengo solo i file di log
  int trovati = 0;
  File radice = LittleFS.open("/");
  if (radice) {
    File f = radice.openNextFile();
    while (f) {
      String nome = f.name();  // con o senza "/" a seconda del core
      if (!f.isDirectory() && nome.indexOf("LOG_") >= 0) {
        html += F("<li><div style='display:flex;justify-content:space-between;"
                  "align-items:center;gap:10px;flex-wrap:wrap'><span>");
        html += nome; html += F(" <span class='dim'>(");
        html += String(f.size() / 1024.0, 1);
        html += F(" KB)</span></span><span>"
                  "<a class='btn ghost small' href='/scarica?f=");
        html += nome;
        html += F("'>CSV</a> <a class='btn ghost small' href='/gpx?f=");
        html += nome;
        html += F("'>GPX</a> <a class='btn danger small' href='/elimina?f=");
        html += nome;
        html += F("' onclick=\"return confirm('Eliminare definitivamente?')\">"
                  "Elimina</a></span></div></li>");
        trovati++;
      }
      f = radice.openNextFile();
    }
    radice.close();
  }
  if (trovati == 0) html += F("<li class='dim'>Nessuna sessione trovata.</li>");
  html += F("</ul>");

  // Contatore di riempimento: la flash non si estrae, meglio vederlo qui
  html += F("<div class='caption'><span class='dim'>Memoria: ");
  html += String(LittleFS.usedBytes() / 1024);
  html += F(" / ");
  html += String(LittleFS.totalBytes() / 1024);
  html += F(" KB</span><a href='/tracciati'>Tracciati e record &rarr;</a></div>");

  if (inRegistrazione)
    html += F("<div class='card'><span id='rec'>&#9679; REC</span> in corso: "
              "download e cancellazione sono disponibili a registrazione ferma.</div>");

  html += F("</body></html>");

  server.send(200, "text/html", html);
}

// La pagina live e' statica e sta in flash: send_P la spedisce da li'
void webLive() {
  server.send_P(200, "text/html", PAGINA_LIVE_HTML);
}

// Telemetria istantanea in JSON, interrogata dalla pagina live ogni 300 ms
void webDati() {
  char json[720];
  // Tempo del giro in corso: 0 se non ne e' ancora partito nessuno
  unsigned long lapCorrenteMs = giroInCorso ? (millis() - inizioGiroCorrente) : 0;

  // Settori dell'ultimo giro completato (0 se nessuno, o se il
  // tracciato non ha checkpoint: in quel caso secN vale 1 e i campi
  // sec1/sec2 restano 0, il JS li ignora in base a secN)
  int numSettoriAttivi = tracciatoCorrente.numCheckpoint + 1;
  unsigned long sec0 = 0, sec1 = 0, sec2 = 0;
  if (numGiri > 0) {
    sec0 = tempiSettorePerGiro[numGiri - 1][0];
    if (numSettoriAttivi > 1) sec1 = tempiSettorePerGiro[numGiri - 1][1];
    if (numSettoriAttivi > 2) sec2 = tempiSettorePerGiro[numGiri - 1][2];
  }

  snprintf(json, sizeof(json),
    "{\"piega\":%.1f,\"pitch\":%.1f,\"glat\":%.2f,\"glon\":%.2f,"
    "\"temp\":%.1f,\"umid\":%.0f,\"rischio\":%.0f,\"rec\":%d,\"min\":%lu,"
    "\"evi\":%u,\"evs\":%u,\"evp\":%u,\"evf\":%u,\"eva\":%u,"
    "\"gpsFix\":%d,\"gpsVel\":%.0f,\"gpsSat\":%u,"
    "\"gpsLat\":%.6f,\"gpsLon\":%.6f,"
    "\"lapSet\":%d,\"lapNum\":%d,\"lapCur\":%lu,\"lapLast\":%lu,\"lapBest\":%lu,"
    "\"trgLat\":%.6f,\"trgLon\":%.6f,"
    "\"secN\":%d,\"sec0\":%lu,\"sec1\":%lu,\"sec2\":%lu,"
    "\"secB0\":%lu,\"secB1\":%lu,\"secB2\":%lu,"
    "\"trackName\":\"%s\"}",
    piegaLive, pitchLive, forzaGLaterale, forzaGLongitudinale,
    temperatura, umidita, indiceRischio, inRegistrazione ? 1 : 0,
    minutiRegistrati,
    conteggioEventi[EV_IMPENNATA], conteggioEventi[EV_STOPPIE],
    conteggioEventi[EV_PIEGA], conteggioEventi[EV_FRENATA],
    conteggioEventi[EV_ACCEL],
    gpsFixValido ? 1 : 0, velocitaGPS, satellitiGPS,
    latitudineGPS, longitudineGPS,
    traguardoImpostato ? 1 : 0, numGiri, lapCorrenteMs,
    numGiri > 0 ? tempiGiro[numGiri - 1] : 0UL,
    giroMigliore >= 0 ? tempiGiro[giroMigliore] : 0UL,
    traguardoLat, traguardoLon,
    numSettoriAttivi, sec0, sec1, sec2,
    tempiSettoreMigliori[0], tempiSettoreMigliori[1], tempiSettoreMigliori[2],
    tracciatoAttivo >= 0 ? tracciatoCorrente.nome : "");
  server.send(200, "application/json", json);
}

// Download di un CSV. Bloccato durante la registrazione: lo streaming di
// un file grosso fermerebbe il loop (e quindi la telemetria) per secondi.
void webScarica() {
  if (inRegistrazione) {
    server.send(503, "text/plain", "Ferma la registrazione prima di scaricare i file.");
    return;
  }

  String nome = server.arg("f");
  if (!nome.startsWith("/")) nome = "/" + nome;
  // Solo i file di log, niente giri strani nel filesystem
  if (!nome.startsWith("/LOG_") || nome.indexOf("..") >= 0 || !LittleFS.exists(nome)) {
    server.send(404, "text/plain", "File non trovato.");
    return;
  }

  File f = LittleFS.open(nome, FILE_READ);
  if (!f) {
    server.send(500, "text/plain", "Errore di lettura dalla memoria.");
    return;
  }
  // Content-Disposition: il browser scarica invece di mostrare il testo
  server.sendHeader("Content-Disposition", "attachment; filename=" + nome.substring(1));
  server.streamFile(f, "text/csv");
  f.close();
}

// Genera un file GPX (GPS Exchange Format, XML standard) dalla posizione
// registrata in una sessione: apribile in Google Earth o qualunque
// software che legge tracce GPS, a differenza del CSV che resta un
// formato "nostro". Le colonne Lat/Lon si cercano per NOME
// nell'intestazione, non per posizione fissa: cosi' funziona anche sui
// log scritti da versioni di firmware precedenti a quando sono state
// aggiunte (schema diverso, stesso principio di leggiInfoSessione() che
// legge il resto del riepilogo senza assumere un formato fisso).
//
// Niente orario nei punti (<time>): il dispositivo non ha un orologio
// sincronizzato a un riferimento reale (nessun RTC, nessun NTP: l'AP e'
// offline), quindi inventare un timestamp sarebbe piu' fuorviante che
// utile. Il file resta comunque valido: per vedere la forma del
// percorso su una mappa non serve altro.
void webGpx() {
  if (inRegistrazione) {
    server.send(503, "text/plain", "Ferma la registrazione prima di esportare un GPX.");
    return;
  }

  String nome = server.arg("f");
  if (!nome.startsWith("/")) nome = "/" + nome;
  if (!nome.startsWith("/LOG_") || nome.indexOf("..") >= 0 || !LittleFS.exists(nome)) {
    server.send(404, "text/plain", "File non trovato.");
    return;
  }

  File f = LittleFS.open(nome, FILE_READ);
  if (!f) {
    server.send(500, "text/plain", "Errore di lettura dalla memoria.");
    return;
  }

  // Intestazione (prima riga): trovo a che colonna stanno Lat e Lon
  String intestazione = f.readStringUntil('\n');
  intestazione.trim();
  int idxLat = -1, idxLon = -1;
  {
    int colonna = 0, pos = 0;
    while (pos <= (int)intestazione.length()) {
      int virgola = intestazione.indexOf(',', pos);
      String nomeColonna = (virgola < 0) ? intestazione.substring(pos)
                                          : intestazione.substring(pos, virgola);
      if (nomeColonna == "Lat") idxLat = colonna;
      else if (nomeColonna == "Lon") idxLon = colonna;
      colonna++;
      if (virgola < 0) break;
      pos = virgola + 1;
    }
  }

  if (idxLat < 0 || idxLon < 0) {
    f.close();
    server.send(400, "text/plain", "Questa sessione non ha dati di posizione: e' stata "
                "registrata con un firmware precedente al modulo GPS.");
    return;
  }

  String gpx;
  gpx.reserve(4096);
  gpx += F("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           "<gpx version=\"1.1\" creator=\"WheelStat\" "
           "xmlns=\"http://www.topografix.com/GPX/1/1\">\n<trk><name>");
  gpx += nome.substring(1);
  gpx += F("</name><trkseg>\n");

  // Righe dati al minuto: si fermano alla prima riga vuota (dove inizia
  // il riepilogo di fine sessione, che non ha colonne Lat/Lon fisse)
  while (f.available()) {
    String riga = f.readStringUntil('\n');
    riga.trim();
    if (riga.length() == 0) break;

    int colonna = 0, pos = 0;
    String campoLat, campoLon;
    while (pos <= (int)riga.length()) {
      int virgola = riga.indexOf(',', pos);
      String campo = (virgola < 0) ? riga.substring(pos) : riga.substring(pos, virgola);
      if (colonna == idxLat) campoLat = campo;
      else if (colonna == idxLon) campoLon = campo;
      colonna++;
      if (virgola < 0) break;
      pos = virgola + 1;
    }
    if (campoLat.length() == 0 || campoLon.length() == 0) continue;

    float lat = campoLat.toFloat();
    float lon = campoLon.toFloat();
    if (lat == 0.0f && lon == 0.0f) continue;  // nessun fix in quel minuto

    gpx += F("<trkpt lat=\"");
    gpx += String(lat, 6);
    gpx += F("\" lon=\"");
    gpx += String(lon, 6);
    gpx += F("\"></trkpt>\n");
  }
  f.close();

  gpx += F("</trkseg></trk></gpx>\n");

  String nomeScaricato = nome.substring(1);
  int puntoEstensione = nomeScaricato.lastIndexOf('.');
  if (puntoEstensione > 0) nomeScaricato = nomeScaricato.substring(0, puntoEstensione);
  nomeScaricato += F(".gpx");

  server.sendHeader("Content-Disposition", "attachment; filename=" + nomeScaricato);
  server.send(200, "application/gpx+xml", gpx);
}

// Cancellazione di un CSV
void webElimina() {
  if (inRegistrazione) {
    server.send(503, "text/plain", "Ferma la registrazione prima di eliminare i file.");
    return;
  }

  String nome = server.arg("f");
  if (!nome.startsWith("/")) nome = "/" + nome;
  if (!nome.startsWith("/LOG_") || nome.indexOf("..") >= 0 || !LittleFS.exists(nome)) {
    server.send(404, "text/plain", "File non trovato.");
    return;
  }

  bool ok = LittleFS.remove(nome);
  Serial.print(ok ? F("Eliminato ") : F("ERRORE eliminando "));
  Serial.println(nome);

  // Redirect all'elenco: la pagina si ricarica gia' aggiornata
  server.sendHeader("Location", "/");
  server.send(303);
}

// Azzeramento dei record storici. Con un tracciato attivo azzera SOLO lo
// storico di quel tracciato (fileRecordAttivo() decide quale file), non
// tutti i tracciati salvati.
void webAzzeraRecord() {
  recordStorici = RecordStorici{};  // tutto a zero (la firma la rimette il salvataggio)
  for (int c = 0; c < N_CANALI; c++) nuovoRecord[c] = false;
  salvaRecordStorici();
  Serial.println(F("Record storici azzerati dal sito web."));

  server.sendHeader("Location", "/");
  server.send(303);
}

// Elenco e gestione dei tracciati: crearne uno nuovo sulla posizione GPS
// attuale, selezionarne uno esistente, tornare alla modalita' libera o
// eliminarne uno. La creazione va fatta fisicamente fermi sulla linea di
// partenza/arrivo, col telefono in mano: il traguardo e' la posizione
// GPS del dispositivo nell'istante in cui arriva la richiesta, non
// quella (eventuale, e diversa) del telefono.
void webTracciati() {
  String html;
  html.reserve(4200);
  html += F("<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>WheelStat - Tracciati</title><link rel='stylesheet' href='/style.css'>"
    "</head><body>");
  html += barraNavigazione("tracciati");

  // Elenco sessioni: una scansione sola, riusata sia per la classifica
  // (tracciato piu' frequentato) sia per il conteggio sessioni di ogni
  // tracciato piu' sotto. Costo accettabile: avviene solo all'apertura
  // di questa pagina, non in loop.
  VoceSessione sessioni[MAX_SESSIONI_ELENCO];
  int numSessioni = elencaSessioni(sessioni, MAX_SESSIONI_ELENCO);

  // Classifica generale: giro piu' veloce di sempre su QUALUNQUE
  // tracciato, e il tracciato piu' frequentato. Prima passata sui
  // tracciati solo per raccogliere questi due numeri; la lista completa
  // (piu' sotto) fa una seconda passata, rileggendo gli stessi file: piu'
  // semplice che tenere in RAM tutti i Tracciato caricati (~700 byte
  // l'uno, fino a 30 sarebbero troppi) per uno storico che si consulta
  // di rado.
  unsigned long recordAssoluto = 0;
  String nomeRecordAssoluto;
  int idPiuFrequentato = -1;
  int sessioniPiuFrequentato = 0;

  for (int id = 1; id <= MAX_TRACCIATI; id++) {
    Tracciato t;
    if (!caricaTracciato(id, t)) continue;

    if (t.migliorGiroMs > 0 && (recordAssoluto == 0 || t.migliorGiroMs < recordAssoluto)) {
      recordAssoluto = t.migliorGiroMs;
      nomeRecordAssoluto = t.nome;
    }

    int sessioniQui = 0;
    for (int i = 0; i < numSessioni; i++)
      if (sessioni[i].tracciato == t.nome) sessioniQui++;
    if (sessioniQui > sessioniPiuFrequentato) {
      sessioniPiuFrequentato = sessioniQui;
      idPiuFrequentato = id;
    }
  }

  html += F("<h2>Classifica</h2><div class='card'>");
  if (recordAssoluto == 0 && idPiuFrequentato < 0) {
    html += F("Non c'e' ancora nessun dato: registra qualche sessione con "
              "un traguardo impostato.");
  } else {
    if (recordAssoluto > 0) {
      html += F("Giro piu' veloce di sempre: <b>");
      html += formattaTempoGiro(recordAssoluto);
      html += F("</b> su ");
      html += nomeRecordAssoluto;
    }
    if (idPiuFrequentato > 0) {
      Tracciato tPop;
      caricaTracciato(idPiuFrequentato, tPop);
      if (recordAssoluto > 0) html += F("<br>");
      html += F("Tracciato piu' frequentato: <b>");
      html += tPop.nome;
      html += F("</b> (");
      html += String(sessioniPiuFrequentato);
      html += F(" sessioni)");
    }
  }
  html += F("</div>");

  html += F("<h2>Tracciato attivo</h2><div class='card'>");
  if (tracciatoAttivo < 0) {
    html += F("Modalita' libera <span class='dim'>(nessun tracciato selezionato)</span>");
  } else {
    html += F("<b>");
    html += tracciatoCorrente.nome;
    html += F("</b> <span class='badge on'>attivo</span>");

    // Gestione settori: solo per il tracciato attivo, perche' aggiungere
    // un checkpoint richiede di essere fisicamente li' col fix GPS,
    // stesso principio della creazione del tracciato o del traguardo.
    html += F("<div style='margin-top:8px'>");
    html += String(tracciatoCorrente.numCheckpoint);
    if (tracciatoCorrente.numCheckpoint == 1) html += F(" settore intermedio definito");
    else html += F(" settori intermedi definiti");
    if (tracciatoCorrente.numCheckpoint < MAX_SETTORI) {
      if (gpsFixValido) {
        html += F(" &nbsp;<a class='btn ghost small' href='/tracciato_aggiungi_settore'>"
                  "Aggiungi settore qui</a>");
      } else {
        html += F(" <span class='dim'>(serve il fix GPS per aggiungerne altri)</span>");
      }
    }
    if (tracciatoCorrente.numCheckpoint > 0) {
      html += F(" &nbsp;<a class='btn danger small' href='/tracciato_rimuovi_settori' "
                "onclick=\"return confirm('Rimuovere tutti i settori di questo "
                "tracciato?')\">Rimuovi settori</a>");
    }
    html += F("</div>");

    html += F("<div style='margin-top:8px'><a class='btn ghost small' "
              "href='/tracciato_libera'>Torna alla modalita' libera</a></div>");
  }
  html += F("</div>");

  // Record dei canali (piega, G, velocita'...): sul file del tracciato
  // attivo, o quello globale in modalita' libera (vedi
  // fileRecordAttivo()). Vive qui, non sulla pagina Sessioni, perche'
  // e' un dato legato al CONTESTO tracciato/modalita', proprio come la
  // classifica e i record giro qui sopra e sotto - un solo posto per
  // "quanto sono andato bene", invece di due pagine che si sovrappongono.
  html += F("<h2>Record");
  if (tracciatoAttivo >= 0) {
    html += F(" &middot; ");
    html += tracciatoCorrente.nome;
  }
  html += F("</h2><div class='card'><table>");
  for (int c = 0; c < N_CANALI; c++) {
    html += F("<tr><td>");
    html += NOME_CANALE[c];
    html += F("</td><td style='text-align:right'><b>");
    html += String(recordStorici.canali[c], decimaliCanale(c));
    html += unitaCanale(c);
    html += F("</b></td></tr>");
  }
  html += F("</table></div>");

  html += F("<div class='caption'><span>");
  html += String(recordStorici.sessioni);
  html += F(" sessioni &middot; ");
  html += String(recordStorici.minutiTotali);
  html += F(" minuti totali</span><a class='btn small danger' href='/azzera_record' "
            "onclick=\"return confirm('Azzerare tutti i record storici?')\">"
            "Azzera</a></div>");

  html += F("<h2>Crea un nuovo tracciato qui</h2><div class='card'>"
    "<p style='margin-top:0'>Fermati sulla linea di partenza/arrivo col fix GPS "
    "gia' agganciato, poi dai un nome: il traguardo sara' la posizione GPS "
    "in questo istante. La forma del circuito si registra da sola al primo "
    "giro veloce.</p>");
  if (!gpsFixValido) {
    html += F("<p style='color:var(--danger);margin-bottom:0'>"
              "GPS senza fix: aspetta l'aggancio prima di creare.</p>");
  } else {
    html += F("<form action='/tracciato_crea' style='display:flex;gap:8px;flex-wrap:wrap'>"
      "<input name='nome' maxlength='23' placeholder='Nome del tracciato' "
      "required style='flex:1;min-width:160px'>"
      "<button type='submit' class='btn'>Crea qui</button></form>");
  }
  html += F("</div>");

  // Scorro gli id possibili e tengo solo quelli con un file valido: piu'
  // semplice che tenere un indice separato, e con MAX_TRACCIATI piccolo
  // il costo di provare ad aprire ogni file e' trascurabile.
  html += F("<h2>Tracciati salvati</h2><ul class='list'>");
  int trovati = 0;
  for (int id = 1; id <= MAX_TRACCIATI; id++) {
    Tracciato t;
    if (!caricaTracciato(id, t)) continue;
    trovati++;

    int sessioniQui = 0;
    for (int i = 0; i < numSessioni; i++)
      if (sessioni[i].tracciato == t.nome) sessioniQui++;

    html += F("<li>");

    String svg = svgFormaTracciato(t);
    if (svg.length() > 0) html += svg;
    else html += F("<div class='dim' style='font-size:12px;padding:4px 0'>"
                   "Forma non ancora registrata (fai un giro veloce)</div>");

    html += F("<div style='display:flex;justify-content:space-between;"
              "align-items:center;gap:10px;flex-wrap:wrap;margin-top:6px'><span><b>");
    html += t.nome;
    html += F("</b>");
    if (id == tracciatoAttivo) html += F(" <span class='badge on'>attivo</span>");
    html += F("<br><span class='dim'>Record giro: ");
    if (t.migliorGiroMs > 0) html += formattaTempoGiro(t.migliorGiroMs);
    else html += F("--");
    html += F(" &middot; ");
    html += String(sessioniQui);
    html += F(" sessioni &middot; ");
    html += String(t.numCheckpoint + 1);
    html += F(" settori</span></span><span>"
              "<a class='btn ghost small' href='/tracciato_seleziona?id=");
    html += String(id);
    html += F("'>Seleziona</a> "
              "<a class='btn danger small' href='/tracciato_elimina?id=");
    html += String(id);
    html += F("' onclick=\"return confirm('Eliminare questo tracciato e i suoi "
              "record?')\">Elimina</a></span></div></li>");
  }
  if (trovati == 0) html += F("<li class='dim'>Nessun tracciato salvato.</li>");
  html += F("</ul></body></html>");

  server.send(200, "text/html", html);
}

// Crea un nuovo tracciato sulla posizione GPS attuale. Nome dal form; se
// manca o il fix non c'e', non si crea nulla (redirect comunque, cosi'
// l'utente torna alla pagina e vede lo stato aggiornato). Bloccato
// durante la registrazione, come le altre azioni che toccano lo storico.
void webTracciatoCrea() {
  if (inRegistrazione) {
    server.send(503, "text/plain", "Ferma la registrazione prima di creare un tracciato.");
    return;
  }

  String nome = server.arg("nome");
  if (nome.length() == 0 || !gpsFixValido) {
    server.sendHeader("Location", "/tracciati");
    server.send(303);
    return;
  }

  int id = primoIdTracciatoLibero();
  if (id < 0) {
    Serial.println(F("ERRORE: nessuno slot libero per un nuovo tracciato."));
    server.sendHeader("Location", "/tracciati");
    server.send(303);
    return;
  }

  Tracciato t = {};
  nome.toCharArray(t.nome, sizeof(t.nome));
  sanitizzaNome(t.nome);
  t.traguardoLat  = latitudineGPS;
  t.traguardoLon  = longitudineGPS;
  t.migliorGiroMs = 0;
  salvaTracciato(id, t);

  Serial.print(F("Tracciato creato: "));
  Serial.println(t.nome);

  selezionaTracciato(id);  // lo attivo subito: e' il flusso piu' comodo

  server.sendHeader("Location", "/tracciati");
  server.send(303);
}

// Seleziona un tracciato esistente come attivo
void webTracciatoSeleziona() {
  if (inRegistrazione) {
    server.send(503, "text/plain", "Ferma la registrazione prima di cambiare tracciato.");
    return;
  }
  selezionaTracciato(server.arg("id").toInt());
  server.sendHeader("Location", "/tracciati");
  server.send(303);
}

// Torna alla modalita' libera (nessun tracciato attivo)
void webTracciatoLibera() {
  if (inRegistrazione) {
    server.send(503, "text/plain", "Ferma la registrazione prima di cambiare tracciato.");
    return;
  }
  selezionaTracciato(-1);
  server.sendHeader("Location", "/tracciati");
  server.send(303);
}

// Elimina un tracciato e il suo file di record. Se era quello attivo, si
// torna alla modalita' libera: non avrebbe senso restare "attivi" su un
// tracciato che non esiste piu'.
void webTracciatoElimina() {
  if (inRegistrazione) {
    server.send(503, "text/plain", "Ferma la registrazione prima di eliminare un tracciato.");
    return;
  }

  int id = server.arg("id").toInt();
  if (id > 0) {
    LittleFS.remove(percorsoTracciato(id));
    LittleFS.remove(percorsoRecordTracciato(id));
    if (id == tracciatoAttivo) selezionaTracciato(-1);
    Serial.print(F("Tracciato eliminato: "));
    Serial.println(id);
  }

  server.sendHeader("Location", "/tracciati");
  server.send(303);
}

// Aggiunge un checkpoint sulla posizione GPS attuale al tracciato attivo
// (fino a MAX_SETTORI): stesso principio della creazione del tracciato,
// va fatto fisicamente fermi sul punto col fix gia' agganciato. Bloccato
// durante la registrazione, come le altre azioni che toccano un file di
// tracciato.
void webTracciatoAggiungiSettore() {
  if (inRegistrazione) {
    server.send(503, "text/plain", "Ferma la registrazione prima di modificare i settori.");
    return;
  }
  if (tracciatoAttivo >= 0 && gpsFixValido && tracciatoCorrente.numCheckpoint < MAX_SETTORI) {
    tracciatoCorrente.checkpointLat[tracciatoCorrente.numCheckpoint] = latitudineGPS;
    tracciatoCorrente.checkpointLon[tracciatoCorrente.numCheckpoint] = longitudineGPS;
    tracciatoCorrente.numCheckpoint++;
    salvaTracciato(tracciatoAttivo, tracciatoCorrente);
    Serial.print(F("Settore aggiunto, checkpoint totali: "));
    Serial.println(tracciatoCorrente.numCheckpoint);
  }
  server.sendHeader("Location", "/tracciati");
  server.send(303);
}

// Rimuove tutti i checkpoint del tracciato attivo: torna a un giro con
// un solo settore (il lap timing "classico"), esattamente come un
// tracciato appena creato.
void webTracciatoRimuoviSettori() {
  if (inRegistrazione) {
    server.send(503, "text/plain", "Ferma la registrazione prima di modificare i settori.");
    return;
  }
  if (tracciatoAttivo >= 0) {
    tracciatoCorrente.numCheckpoint = 0;
    salvaTracciato(tracciatoAttivo, tracciatoCorrente);
    Serial.println(F("Settori rimossi dal tracciato attivo."));
  }
  server.sendHeader("Location", "/tracciati");
  server.send(303);
}

// Genera un piccolo SVG con la forma salvata del tracciato (proiezione
// locale piatta rispetto al primo punto, stesso principio della traccia
// GPS live sulla pagina /live, vedi proiettaLocale() nello script). Qui
// pero' i punti non cambiano da una richiesta all'altra: e' un tag
// <svg> generato lato firmware, niente bisogno di canvas o JavaScript.
// Stringa vuota se il tracciato non ha ancora una forma salvata.
String svgFormaTracciato(const Tracciato &t) {
  if (t.numPuntiForma < 2) return String();

  double refLat = t.formaLat[0];
  double refLon = t.formaLon[0];
  double mPerGradoLat = 111320.0;
  double mPerGradoLon = 111320.0 * cos(refLat * PI / 180.0);

  float xs[MAX_PUNTI_FORMA], ys[MAX_PUNTI_FORMA];
  float minX = 1e9f, maxX = -1e9f, minY = 1e9f, maxY = -1e9f;
  for (int i = 0; i < t.numPuntiForma; i++) {
    xs[i] = (float)((t.formaLon[i] - refLon) * mPerGradoLon);
    ys[i] = (float)((t.formaLat[i] - refLat) * mPerGradoLat);
    if (xs[i] < minX) minX = xs[i];
    if (xs[i] > maxX) maxX = xs[i];
    if (ys[i] < minY) minY = ys[i];
    if (ys[i] > maxY) maxY = ys[i];
  }

  // Il traguardo entra nel bounding box, cosi' resta visibile anche se
  // il primo punto della forma non e' esattamente li'
  float trgX = (float)((t.traguardoLon - refLon) * mPerGradoLon);
  float trgY = (float)((t.traguardoLat - refLat) * mPerGradoLat);
  if (trgX < minX) minX = trgX;
  if (trgX > maxX) maxX = trgX;
  if (trgY < minY) minY = trgY;
  if (trgY > maxY) maxY = trgY;

  float scala = 100.0f / fmaxf(1.0f, fmaxf(maxX - minX, maxY - minY));

  String path = "M";
  for (int i = 0; i < t.numPuntiForma; i++) {
    float px = (xs[i] - minX) * scala + 4;
    float py = 108.0f - ((ys[i] - minY) * scala + 4);  // Y invertita: nord in alto
    if (i > 0) path += " L";
    path += String(px, 1) + "," + String(py, 1);
  }
  float tpx = (trgX - minX) * scala + 4;
  float tpy = 108.0f - ((trgY - minY) * scala + 4);

  String svg = F("<svg viewBox='0 0 108 112' width='100%' height='90' "
                 "style='background:var(--card2);border-radius:8px;display:block'>");
  svg += "<path d='" + path + "' fill='none' stroke='var(--accent)' "
         "stroke-width='2' stroke-linejoin='round' stroke-linecap='round'/>";
  svg += "<circle cx='" + String(tpx, 1) + "' cy='" + String(tpy, 1) + "' r='3' fill='#fff'/>";
  svg += F("</svg>");
  return svg;
}


// file di log, SENZA caricare l'intero file in RAM: si legge riga per
// riga (il file puo' avere centinaia di righe di dati al minuto prima
// del riepilogo che serve qui) e si tengono solo le righe TRACCIATO/GIRI
// della coda. trovata=false se il file non ha un riepilogo valido (es.
// sessione fermata a meta', o file che non e' un log WheelStat).
InfoSessione leggiInfoSessione(String percorso) {
  InfoSessione info;
  info.trovata  = false;
  info.numGiri  = 0;
  info.tracciato = F("libera");

  if (!percorso.startsWith("/")) percorso = "/" + percorso;

  File f = LittleFS.open(percorso, FILE_READ);
  if (!f) return info;

  bool inBloccoGiri = false;
  while (f.available()) {
    String riga = f.readStringUntil('\n');
    riga.trim();
    if (riga.length() == 0) { inBloccoGiri = false; continue; }

    if (riga.startsWith("TRACCIATO,")) {
      info.tracciato = riga.substring(10);
      info.trovata = true;
    } else if (riga.startsWith("GIRI,")) {
      inBloccoGiri = true;
    } else if (inBloccoGiri && riga.startsWith("Giro_") && info.numGiri < MAX_GIRI) {
      int virgola = riga.indexOf(',');
      if (virgola > 0) {
        String valore = riga.substring(virgola + 1);
        if (valore.endsWith("*")) valore.remove(valore.length() - 1);  // asterisco "record"
        info.tempiGiro[info.numGiri] = (unsigned long)(valore.toFloat() * 1000.0f + 0.5f);
        info.numGiri++;
      }
    }
  }
  f.close();
  return info;
}

// Elenca tutte le sessioni disponibili (nome file, tracciato, numero di
// giri) in UNA sola scansione della flash. La pagina /confronta ha
// bisogno dello stesso elenco due volte (le select "A" e "B"): prima
// facevo una scansione per ciascuna, rileggendo il riepilogo di ogni
// file due volte per lo stesso identico risultato. Ora si scansiona una
// volta sola e si riusa l'array per entrambe le select.
int elencaSessioni(VoceSessione *elenco, int capienza) {
  int trovate = 0;
  File radice = LittleFS.open("/");
  if (!radice) return trovate;

  File f = radice.openNextFile();
  while (f && trovate < capienza) {
    String nome = f.name();
    if (!f.isDirectory() && nome.indexOf("LOG_") >= 0) {
      InfoSessione info = leggiInfoSessione(nome);
      elenco[trovate].nome      = nome.startsWith("/") ? nome.substring(1) : nome;
      elenco[trovate].tracciato = info.tracciato;
      elenco[trovate].numGiri   = info.numGiri;
      trovate++;
    }
    f = radice.openNextFile();
  }
  radice.close();
  return trovate;
}

// Aggiunge le <option> di una select a partire da un elenco gia' letto
// (vedi elencaSessioni): nessuna nuova lettura da flash qui, e' solo
// costruzione di HTML.
void aggiungiOpzioniSessioni(String &html, VoceSessione *elenco, int numSessioni,
                              const String &selezionata) {
  for (int i = 0; i < numSessioni; i++) {
    html += F("<option value='");
    html += elenco[i].nome;
    html += F("'");
    if (elenco[i].nome == selezionata) html += F(" selected");
    html += F(">");
    html += elenco[i].nome;
    html += F(" - ");
    html += elenco[i].tracciato;
    html += F(" (");
    html += String(elenco[i].numGiri);
    html += F(" giri)</option>");
  }
}

// Confronta i tempi giro di due sessioni, tipicamente sullo stesso
// tracciato: aiuta a rispondere a "sto migliorando?" senza dover aprire
// due CSV in un foglio di calcolo. Le sessioni si scelgono da due select
// con lo stesso elenco; se sono su tracciati diversi il confronto resta
// possibile ma viene segnalato. Il giro piu' veloce tra i due, riga per
// riga, si colora per saltare subito all'occhio chi ha vinto quel giro.
void webConfronta() {
  String selA = server.hasArg("a") ? server.arg("a") : "";
  String selB = server.hasArg("b") ? server.arg("b") : "";

  String html;
  html.reserve(3600);
  html += F("<!DOCTYPE html><html><head><meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>WheelStat - Confronta</title><link rel='stylesheet' href='/style.css'>"
    "</head><body>");
  html += barraNavigazione("confronta");

  VoceSessione elenco[MAX_SESSIONI_ELENCO];
  int numSessioni = elencaSessioni(elenco, MAX_SESSIONI_ELENCO);

  // Stato vuoto: senza almeno due sessioni il modulo di confronto non ha
  // nulla da mostrare, meglio dirlo chiaro che disegnare due select
  // vuote e un bottone che non porta a niente.
  if (numSessioni < 2) {
    html += F("<h2>Confronta due sessioni</h2><div class='card'>Servono almeno due "
              "sessioni registrate per poterle confrontare");
    html += (numSessioni == 0) ? F(" (per ora non ce n'e' nessuna).")
                                : F(" (per ora ce n'e' solo una).");
    html += F("</div></body></html>");
    server.send(200, "text/html", html);
    return;
  }

  html += F("<h2>Confronta due sessioni</h2><div class='card'>"
    "<form action='/confronta' style='display:flex;flex-direction:column;gap:10px'>"
    "<label class='dim'>Sessione A</label><select name='a'>");
  aggiungiOpzioniSessioni(html, elenco, numSessioni, selA);
  html += F("</select><label class='dim'>Sessione B</label><select name='b'>");
  aggiungiOpzioniSessioni(html, elenco, numSessioni, selB);
  html += F("</select><button type='submit' class='btn'>Confronta</button></form></div>");

  if (selA.length() > 0 && selB.length() > 0 && selA != selB) {
    InfoSessione a = leggiInfoSessione(selA);
    InfoSessione b = leggiInfoSessione(selB);

    if (!a.trovata || !b.trovata || a.numGiri == 0 || b.numGiri == 0) {
      html += F("<div class='card'>Una delle due sessioni non ha giri cronometrati da "
                "confrontare (serve un traguardo impostato durante la registrazione).</div>");
    } else {
      if (a.tracciato != b.tracciato) {
        html += F("<div class='card' style='color:var(--warn)'>Tracciati diversi (");
        html += a.tracciato; html += F(" / "); html += b.tracciato;
        html += F("): il confronto resta possibile ma ha meno senso.</div>");
      }

      unsigned long migliorA = 0, migliorB = 0;
      for (int i = 0; i < a.numGiri; i++)
        if (migliorA == 0 || a.tempiGiro[i] < migliorA) migliorA = a.tempiGiro[i];
      for (int i = 0; i < b.numGiri; i++)
        if (migliorB == 0 || b.tempiGiro[i] < migliorB) migliorB = b.tempiGiro[i];

      int maxGiri = (a.numGiri > b.numGiri) ? a.numGiri : b.numGiri;

      html += F("<h2>Tempi giro</h2><div class='card'><table>"
                "<tr><td class='dim'>Giro</td>"
                "<td class='dim' style='text-align:right'>A</td>"
                "<td class='dim' style='text-align:right'>B</td></tr>");
      for (int i = 0; i < maxGiri; i++) {
        bool haA = i < a.numGiri, haB = i < b.numGiri;
        // Il giro piu' veloce tra i due si colora di verde: un confronto
        // riga per riga si legge molto piu' in fretta a colpo d'occhio
        // che confrontando due colonne di numeri a mente
        bool aVince = haA && haB && a.tempiGiro[i] < b.tempiGiro[i];
        bool bVince = haA && haB && b.tempiGiro[i] < a.tempiGiro[i];

        html += F("<tr><td>"); html += String(i + 1); html += F("</td>");
        html += F("<td style='text-align:right");
        if (aVince) html += F(";color:var(--good);font-weight:600");
        html += F("'>");
        html += haA ? formattaTempoGiro(a.tempiGiro[i]) : String(F("--"));
        html += F("</td><td style='text-align:right");
        if (bVince) html += F(";color:var(--good);font-weight:600");
        html += F("'>");
        html += haB ? formattaTempoGiro(b.tempiGiro[i]) : String(F("--"));
        html += F("</td></tr>");
      }

      html += F("<tr style='font-weight:600'><td>Migliore</td>"
                "<td style='text-align:right");
      if (migliorA < migliorB) html += F(";color:var(--good)");
      html += F("'>");
      html += formattaTempoGiro(migliorA);
      html += F("</td><td style='text-align:right");
      if (migliorB < migliorA) html += F(";color:var(--good)");
      html += F("'>");
      html += formattaTempoGiro(migliorB);
      html += F("</td></tr></table></div>");
    }
  }

  html += F("</body></html>");
  server.send(200, "text/html", html);
}

