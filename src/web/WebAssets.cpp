// WheelStat - static assets of the website: the shared stylesheet and the
// live telemetry page. Both live in flash and are sent from there.

#include "../i18n/Text.h"
#include "WebInternal.h"

namespace web {

// One stylesheet for every page: colours, type and components (cards,
// buttons, badges, lists) are defined once, so the pages look like one
// product.
static const char STYLE_CSS[] PROGMEM = R"css(
/* Dark theme: easier to read on a phone in the sun. --good/--warn/--danger
   are the very same shades used in the canvas of the GPS trace (see
   traceColor() in the live page script): a 2D canvas cannot read var(), so
   the hex values are copied by hand - keep the two in step. */
:root{
  --bg:#0d1015; --card:#161b22; --card2:#1e242d; --border:#262e3a;
  --text:#e9edf3; --dim:#8b93a4; --accent:#4cc9ff; --ink:#04222f;
  --danger:#ff5d6c; --good:#4ede93; --warn:#ffd166;
}
*{box-sizing:border-box}
/* A phone column even on a desktop: stretched over a wide screen the tiles
   and charts read worse, not better. */
body{
  margin:0 auto;max-width:760px;padding:16px 16px 32px;background:var(--bg);color:var(--text);
  font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;
  -webkit-tap-highlight-color:transparent;
}
html{background:var(--bg)}
a{color:var(--accent);text-decoration:none}
a:active{opacity:.7}

/* Shared header: brand + tab navigation. It stays at the top while the
   page scrolls, on a blurred band so the content passes under it. */
.topbar{display:flex;align-items:center;justify-content:space-between;gap:8px;
  position:sticky;top:0;z-index:5;margin:-16px -16px 14px;padding:12px 16px;
  background:rgba(13,16,21,.88);-webkit-backdrop-filter:blur(10px);backdrop-filter:blur(10px);
  border-bottom:1px solid var(--border)}
.brand{font-size:19px;font-weight:700;white-space:nowrap}
.brand span{color:var(--accent)}
/* One row, always: an icon per tab, and the name next to it. On a narrow
   phone only the open tab keeps its name - five names do not fit on one
   line, and wrapping would push the page down. */
nav.tabs{display:flex;gap:6px}
nav.tabs a{display:flex;align-items:center;gap:6px;font-size:13px;padding:7px 11px;
  border-radius:999px;color:var(--dim);background:var(--card);border:1px solid var(--border)}
nav.tabs svg{width:16px;height:16px;flex:none;fill:none;stroke:currentColor;stroke-width:1.6;
  stroke-linecap:round;stroke-linejoin:round}
nav.tabs a.on{color:var(--ink);background:var(--accent);border-color:var(--accent);font-weight:600}
@media (max-width:620px){nav.tabs a:not(.on) span{display:none}}
/* The narrowest phones: icons only, the open tab still lit */
@media (max-width:430px){
  nav.tabs{gap:4px}
  nav.tabs a{padding:8px 9px}
  nav.tabs a span{display:none}
  .brand{font-size:17px}
}

/* The content comes in with a short fade: enough to see the page changed,
   too short to wait for. */
main{animation:enter .22s ease-out both}
@keyframes enter{from{opacity:0;transform:translateY(5px)}}

/* Waiting for something (a session file can take a few seconds) */
.spin{display:inline-block;width:14px;height:14px;margin-right:8px;vertical-align:-2px;
  border:2px solid var(--border);border-top-color:var(--accent);border-radius:50%;
  animation:spin .8s linear infinite}
@keyframes spin{to{transform:rotate(360deg)}}

/* Visible focus for keyboard users, invisible for touch */
:focus-visible{outline:2px solid var(--accent);outline-offset:2px}

h2{font-size:13px;margin:20px 0 8px;color:var(--dim);text-transform:uppercase;
  letter-spacing:.6px;font-weight:600}
h2:first-of-type{margin-top:4px}
p{line-height:1.45}

.card{background:var(--card);border:1px solid var(--border);border-radius:14px;
  padding:14px;margin-bottom:12px}
.card p:first-child{margin-top:0}
.card p:last-child{margin-bottom:0}

.grid{display:grid;grid-template-columns:repeat(3,1fr);gap:8px;margin-bottom:14px}
.stat{background:var(--card);border:1px solid var(--border);border-radius:12px;
  padding:10px 6px;text-align:center}
.stat .e{font-size:10.5px;color:var(--dim);text-transform:uppercase;letter-spacing:.3px}
.stat .v{font-size:20px;font-weight:700;margin-top:2px;font-variant-numeric:tabular-nums}

/* The two numbers that matter most while riding, larger and on their own
   row. Tabular digits: a value that changes three times a second must not
   make its neighbours jump. */
.hero{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-bottom:8px}
.hero .stat{padding:12px 8px}
.hero .v{font-size:34px;line-height:1.1}
.hero .v small{font-size:15px;font-weight:600;color:var(--dim);margin-left:4px}

/* Gauge under the lean value: grows from the centre towards the side of
   the lean, full scale 60 degrees. */
.lean{position:relative;height:6px;border-radius:3px;background:var(--card2);margin:8px 6px 0}
.lean i{position:absolute;top:0;bottom:0;border-radius:3px;background:var(--accent);
  transition:width .25s ease-out,left .25s ease-out,background-color .25s}
.lean:after{content:'';position:absolute;left:50%;top:-3px;bottom:-3px;width:1px;background:var(--dim)}

/* Connection state of the live page: without it a dropped WiFi would leave
   the last values on screen looking current. */
.link{font-size:12px;display:inline-flex;align-items:center;gap:6px;color:var(--dim)}
.link:before{content:'';width:8px;height:8px;border-radius:50%;background:var(--good)}
.link.lost{color:var(--danger)}
.link.lost:before{background:var(--danger)}

/* Small label/value pairs in a row (laps, events) */
.pairs{display:grid;grid-template-columns:repeat(auto-fit,minmax(96px,1fr));gap:10px 8px}
.pairs div{font-size:11px;color:var(--dim);text-transform:uppercase;letter-spacing:.3px}
.pairs b{display:block;font-size:17px;color:var(--text);text-transform:none;letter-spacing:0;
  margin-top:2px;font-variant-numeric:tabular-nums}
/* Gap next to a sector time: green = best of the session, yellow = slower */
.pairs small{font-size:12px;font-weight:600;margin-left:5px}
.pairs small.up{color:var(--good)} .pairs small.down{color:var(--warn)}

/* Fill level of the storage */
.meter{height:6px;border-radius:3px;background:var(--card2);overflow:hidden;margin-top:6px}
.meter i{display:block;height:100%;background:var(--accent);border-radius:3px;
  animation:grow .6s ease-out both}
@keyframes grow{from{width:0}}
.meter.full i{background:var(--danger)}

canvas{width:100%;display:block;background:var(--card);border:1px solid var(--border);
  border-radius:14px;margin-bottom:6px}
.caption{font-size:11.5px;color:var(--dim);margin:0 2px 6px;display:flex;
  justify-content:space-between;align-items:center;flex-wrap:wrap;gap:6px}

ul.list{list-style:none;margin:0;padding:0}
ul.list li{background:var(--card);border:1px solid var(--border);border-radius:12px;
  padding:10px 12px;margin-bottom:8px;font-size:14px}

/* A session: the card is the link to its charts, the rest is underneath */
ul.list li.session{padding:0;overflow:hidden}
li.session .open{display:flex;justify-content:space-between;align-items:center;gap:10px;
  padding:12px;color:var(--text);transition:background .12s}
li.session a.open:active{background:var(--card2)}
@media (hover:hover){li.session a.open:hover{background:var(--card2)}}
li.session .go{color:var(--accent);font-size:13px;font-weight:600;white-space:nowrap}
li.session .actions{display:flex;gap:18px;padding:8px 12px;border-top:1px solid var(--border);
  font-size:12.5px}
li.session .actions a{color:var(--dim)}
li.session .actions a.danger{color:var(--danger)}

/* A line with something on the left and the actions on the right */
.row{display:flex;justify-content:space-between;align-items:center;gap:10px;flex-wrap:wrap}
.gap{margin-top:8px}

.btn{display:inline-block;padding:8px 15px;border-radius:999px;font-weight:600;
  font-size:13px;background:var(--accent);color:var(--ink);border:none;
  text-decoration:none;cursor:pointer}
.btn.ghost{background:transparent;color:var(--text);border:1px solid var(--border)}
.btn.danger{background:transparent;color:var(--danger);border:1px solid rgba(255,93,108,.35)}
.btn.small{padding:6px 12px;font-size:12px}

.badge{display:inline-block;font-size:11px;padding:2px 9px;border-radius:999px;
  background:var(--card2);color:var(--dim)}
.badge.on{background:rgba(76,201,255,.15);color:var(--accent)}

/* 16px and no less: below that iOS Safari zooms the page when the input
   takes focus, annoying on a site meant for one-handed phone use. */
input,select,button{font-size:16px;padding:9px 12px;border-radius:10px;
  border:1px solid var(--border);background:var(--card2);color:var(--text);
  font-family:inherit}
input::placeholder{color:var(--dim)}
button{cursor:pointer}
form.inline{display:flex;gap:8px;flex-wrap:wrap}
form.inline input{flex:1;min-width:160px}
form.stack{display:flex;flex-direction:column;gap:10px}
/* A setting: what it is on the left, its input on the right */
.fields{display:flex;flex-direction:column;gap:10px}
.hint{font-size:12px;color:var(--dim)}
.field{display:flex;justify-content:space-between;align-items:center;gap:10px;font-size:14px}
.field input{width:130px;flex:none}
input[type=file]{width:100%;font-size:14px}

/* Charts that are read by sliding a finger along them: the page must still
   scroll vertically from there, only the horizontal drag belongs to them. */
canvas.scrub{touch-action:pan-y}

table{width:100%;border-collapse:collapse;font-size:13.5px}
td{padding:6px 4px}
td.num{text-align:right}
td.win{color:var(--good);font-weight:600}
tr+tr{border-top:1px solid var(--border)}
tr.total{font-weight:600}

svg.shape{background:var(--card2);border-radius:8px;display:block}

#rec{color:var(--danger);font-weight:700;font-size:13px;display:flex;
  align-items:center;gap:7px}

/* The recording dot pulses, like the "* REC" asterisk on the display: the
   one thing on the page that gains from motion, because it must say "this
   is happening now" out of the corner of the eye. The text next to it does
   NOT move: telemetry numbers must stay still and readable. */
#rec .dot{width:9px;height:9px;border-radius:50%;background:var(--danger);
  animation:pulse 1s ease-in-out infinite}
@keyframes pulse{50%{opacity:.25;transform:scale(.8)}}

/* Touch feedback: on a phone, maybe with gloves on, the finger needs to
   know it landed. 90 ms is below what reads as a wait and above what would
   not be seen at all. */
.btn,nav.tabs a{transition:background .12s,color .12s,transform .09s}
.btn:active,nav.tabs a:active{transform:scale(.96)}

/* Whoever asked the system for less motion gets it: nothing is lost, the
   dot stays red and still. */
@media (prefers-reduced-motion:reduce){
  *{animation:none!important;transition:none!important}
}

/* Value states, in the same three colours as the GPS trace legend: green /
   yellow / red mean the same thing everywhere. */
.v.ok{color:var(--good)} .v.med{color:var(--warn)} .v.hi{color:var(--danger)}
.v.off{color:var(--dim);font-size:15px}
.dim{color:var(--dim)}
.warn{color:var(--warn)}
.danger{color:var(--danger)}
)css";

// Short Cache-Control: within one visit the phone does not fetch it again at
// every page change, but it does not stay stuck after a firmware update.
void handleStyle() {
  server.sendHeader("Cache-Control", "max-age=3600");
  server.send_P(200, "text/css", STYLE_CSS);
}

// ===========================================================================
// Live page
// ===========================================================================

static const char LIVE_BODY[] PROGMEM =
  "<div class='caption'><span id='link' class='link'></span><span id='rec'></span></div>"

  // Instantaneous values: the two read most often first, larger
  "<div class='hero'>"
  "<div class='stat'><div class='e'>" TR_W_LIVE_LEAN  "</div><div class='v' id='vLean'>--</div>"
  "<div class='lean'><i id='leanBar'></i></div></div>"
  "<div class='stat'><div class='e'>" TR_W_LIVE_SPEED "</div><div class='v' id='vSpeed'>--</div></div>"
  "</div>"
  "<div class='grid'>"
  "<div class='stat'><div class='e'>" TR_W_LIVE_DELTA "</div><div class='v' id='vDelta'>--</div></div>"
  "<div class='stat'><div class='e'>" TR_W_LIVE_PITCH "</div><div class='v' id='vPitch'>--</div></div>"
  "<div class='stat'><div class='e'>" TR_W_LIVE_RISK  "</div><div class='v' id='vRisk'>--</div></div>"
  "<div class='stat'><div class='e'>" TR_W_LIVE_GLAT  "</div><div class='v' id='vGlat'>--</div></div>"
  "<div class='stat'><div class='e'>" TR_W_LIVE_GLON  "</div><div class='v' id='vGlon'>--</div></div>"
  "<div class='stat'><div class='e'>" TR_W_LIVE_AIR   "</div><div class='v' id='vAir'>--</div></div>"
  "</div>"

  // GPS trace of the current session, coloured by lean. It clears itself at
  // every new recording, or by hand with the button (handy for test laps).
  "<div class='caption'><span>" TR_W_LIVE_TRACE " &mdash; "
  "<span style='color:var(--good)'>&lt;20&deg;</span> / "
  "<span style='color:var(--warn)'>20-35&deg;</span> / "
  "<span style='color:var(--danger)'>&gt;35&deg;</span></span>"
  "<button class='btn ghost small' onclick='resetTrace()'>" TR_W_LIVE_CLEAR "</button></div>"
  "<canvas id='trace' height='220'></canvas>"

  // Scrolling charts: ~36 seconds of history (120 points x 300 ms)
  "<div class='caption'><span>" TR_W_LIVE_ANGLES "</span>"
  "<span><span style='color:var(--accent)'>" TR_W_LIVE_LEGEND_LEAN "</span> / "
  "<span style='color:var(--warn)'>" TR_W_LIVE_LEGEND_PITCH "</span></span></div>"
  "<canvas id='chartAngles' height='130'></canvas>"
  "<div class='caption'><span>" TR_W_LIVE_GFORCES "</span>"
  "<span><span style='color:var(--accent)'>" TR_W_LIVE_LEGEND_LAT "</span> / "
  "<span style='color:var(--warn)'>" TR_W_LIVE_LEGEND_LON "</span></span></div>"
  "<canvas id='chartG' height='130'></canvas>"

  "<h2>" TR_W_LIVE_LAPS "</h2><div class='card' id='laps'>--</div>"
  "<h2>" TR_W_LIVE_EVENTS "</h2><div class='card' id='events'>--</div>"
  "<p><a href='/'>" TR_W_LIVE_BACK "</a></p>"

  // T: the translated strings used by the script below
  "<script>var T=" TR_W_LIVE_JS_DICT ";";

static const char LIVE_SCRIPT[] PROGMEM = R"js(
// Chart history: N samples, the oldest leave on the left
var N=120;
var sLean=[],sPitch=[],sLat=[],sLon=[];

function byId(id){return document.getElementById(id);}

// Sizes a canvas to its real width at the screen's pixel density (sharp on
// a phone, where one CSS pixel is 2-3 device pixels) and returns a context
// that still draws in CSS pixels. The drawing height is the height attribute
// as first written in the page, kept aside before it gets multiplied.
function fit(cv){
 var r=window.devicePixelRatio||1,W=cv.clientWidth;
 if(!cv.dataset.h)cv.dataset.h=cv.getAttribute('height');
 var H=+cv.dataset.h;
 cv.style.height=H+'px';
 cv.width=Math.round(W*r);cv.height=Math.round(H*r);
 var c=cv.getContext('2d');
 c.setTransform(r,0,0,r,0,0);
 return {c:c,W:W,H:H};
}

// Draws two series (a, b) on a canvas: full scale +/-max, zero line in the
// middle, a faint line at half scale on each side with its value.
function drawChart(id,a,b,max){
 var f=fit(byId(id)),c=f.c,W=f.W,H=f.H;
 c.lineWidth=1;
 [-.5,.5].forEach(function(k){
  var y=H/2-k*(H/2-4);
  c.strokeStyle='#1f2632';c.beginPath();c.moveTo(0,y);c.lineTo(W,y);c.stroke();
  c.fillStyle='#5d6576';c.font='10px sans-serif';
  c.fillText((k>0?'+':'')+(k*max),4,y-3);
 });
 c.strokeStyle='#2a3140';c.beginPath();c.moveTo(0,H/2);c.lineTo(W,H/2);c.stroke();
 function line(s,col){
  c.strokeStyle=col;c.lineWidth=2;c.beginPath();
  for(var i=0;i<s.length;i++){
   var x=i*W/(N-1);
   var y=H/2-(s[i]/max)*(H/2-4);
   y=Math.max(2,Math.min(H-2,y));     // no lines outside the box
   if(i==0)c.moveTo(x,y);else c.lineTo(x,y);
  }
  c.stroke();
 }
 line(a,'#4cc9ff');line(b,'#ffd166');
}

function push(s,v){s.push(v);if(s.length>N)s.shift();}

// Writes a value and gives it its state class (ok/med/hi/off), which the
// CSS turns into a colour. The class is rewritten only when it changes.
function setState(id,text,cls){
 var e=byId(id);
 e.textContent=text;
 var c='v'+(cls?' '+cls:'');
 if(e.className!=c) e.className=c;
}

// A duration in ms as "m:ss.d", like formatLapTime() in the firmware
function fmtTime(ms){
 if(!ms)return'--';
 var s=Math.floor(ms/1000),d=Math.floor((ms%1000)/100);
 return Math.floor(s/60)+':'+(s%60<10?'0':'')+(s%60)+'.'+d;
}

// The ideal lap: the best time of each sector in this session, added up.
// Only once every sector has one, and only with checkpoints (a single
// sector would just repeat the best lap).
function idealLap(best){
 if(best.length<2)return 0;
 var sum=0;
 for(var i=0;i<best.length;i++){if(!best[i])return 0;sum+=best[i];}
 return sum;
}

// The sectors of the last lap, each with how far it was from the best of
// the session: green when it IS the best, yellow with the gap otherwise.
function sectorPairs(d){
 if(d.sec.length<2||!d.lapNum)return '';
 var l=[];
 for(var i=0;i<d.sec.length;i++){
  var gap=d.sec[i]-d.secBest[i],tag;
  if(!d.sec[i])tag='';
  else if(gap<=0)tag='<small class="up">'+T.top+'</small>';
  else tag='<small class="down">+'+(gap/1000).toFixed(2)+'</small>';
  l.push(['S'+(i+1),fmtTime(d.sec[i])+tag]);
 }
 return '<div class="gap"></div>'+pairs(l);
}

// --- GPS trace ---------------------------------------------------------------
// Every valid point becomes (x,y) in metres from the first point of the
// session (flat local projection: over the area of a circuit the Earth is
// flat enough). "lean" is the absolute value there, used for the colour.
var tracePts=[];
var refLat=null, refLon=null;
var prevRec=false;
var finishSet=false, finishLat=0, finishLon=0;

function resetTrace(){
 tracePts=[];
 refLat=null;
 refLon=null;
 drawTrace();
}

// One place for the conversion: used for the path points and for the
// finish line.
function project(lat,lon){
 var mPerDegLat=111320;
 var mPerDegLon=111320*Math.cos(refLat*Math.PI/180);
 return {x:(lon-refLon)*mPerDegLon, y:(lat-refLat)*mPerDegLat};
}

// Capped at 4000 points: beyond that redrawing would slow the phone down.
function addTracePoint(lat,lon,lean){
 if(refLat===null){refLat=lat;refLon=lon;}
 var p=project(lat,lon);
 if(tracePts.length<4000) tracePts.push({x:p.x,y:p.y,lean:Math.abs(lean)});
}

// Same "big lean" threshold as the firmware's lean event (35 degrees), plus
// a yellow middle band.
function traceColor(lean){
 if(lean<20)return'#4ede93';
 if(lean<35)return'#ffd166';
 return'#ff5d6c';
}

function drawTrace(){
 var f=fit(byId('trace')),c=f.c,W=f.W,H=f.H;

 if(tracePts.length<2){
  c.fillStyle='#8b93a4';c.font='13px -apple-system,sans-serif';
  c.fillText(T.wait,10,H/2);
  return;
 }

 // The finish line joins the bounding box, so it stays visible even when
 // it is outside the stretch ridden so far
 var fin=finishSet&&refLat!==null ? project(finishLat,finishLon) : null;

 // Box containing every point, to fit the scale while keeping the real
 // proportions (otherwise the circuit would look distorted)
 var minX=Infinity,maxX=-Infinity,minY=Infinity,maxY=-Infinity;
 for(var i=0;i<tracePts.length;i++){
  minX=Math.min(minX,tracePts[i].x);maxX=Math.max(maxX,tracePts[i].x);
  minY=Math.min(minY,tracePts[i].y);maxY=Math.max(maxY,tracePts[i].y);
 }
 if(fin){
  minX=Math.min(minX,fin.x);maxX=Math.max(maxX,fin.x);
  minY=Math.min(minY,fin.y);maxY=Math.max(maxY,fin.y);
 }
 // One scale for both axes, and the drawing centred in the box
 var pad=16;
 var scale=Math.min((W-2*pad)/Math.max(1,maxX-minX),(H-2*pad)/Math.max(1,maxY-minY));
 var ox=(W-(maxX-minX)*scale)/2, oy=(H-(maxY-minY)*scale)/2;
 function px(p){return ox+(p.x-minX)*scale;}
 function py(p){return H-oy-(p.y-minY)*scale;}  // Y flipped: north up

 for(var i=1;i<tracePts.length;i++){
  c.strokeStyle=traceColor(tracePts[i].lean);
  c.lineWidth=3;c.lineCap='round';
  c.beginPath();
  c.moveTo(px(tracePts[i-1]),py(tracePts[i-1]));
  c.lineTo(px(tracePts[i]),py(tracePts[i]));
  c.stroke();
 }

 // Finish line: white square, distinct from the position dot
 if(fin){
  c.fillStyle='#fff';
  c.fillRect(px(fin)-5,py(fin)-5,10,10);
 }

 // Cyan dot on the last known position: where I am now
 var last=tracePts[tracePts.length-1];
 c.fillStyle='#4cc9ff';
 c.beginPath();c.arc(px(last),py(last),5,0,Math.PI*2);c.fill();
}

// Label/value pairs for the laps and events cards
function pairs(list){
 var h='<div class="pairs">';
 for(var i=0;i<list.length;i++)h+='<div>'+list[i][0]+'<b>'+list[i][1]+'</b></div>';
 return h+'</div>';
}

// Two missed answers in a row (~0.6 s) and the page says so: the values on
// screen are then the last ones received, not the current ones.
var misses=0;
function setLink(ok,sats){
 misses=ok?0:misses+1;
 var e=byId('link'),lost=misses>=2;
 if(ok)e.textContent=T.online+' · '+sats+' sat';
 else if(lost)e.textContent=T.offline;
 e.className=lost?'link lost':'link';
}

// Polls /data, updates the tiles and redraws the charts. On error (WiFi
// lost, bike off) it marks the link as lost and tries again at the next tick.
async function tick(){
 try{
  var d=await (await fetch('/data')).json();
  setLink(true,d.gpsSat);

  // Lean: the size and, beside it, the side; the gauge grows that way
  var a=Math.abs(d.lean),w=Math.min(a,60)/60*50;
  byId('vLean').innerHTML=a.toFixed(1)+'<small>'+(a<1?'':(d.lean>0?T.right:T.left))+'</small>';
  var bar=byId('leanBar');
  bar.style.width=w+'%';bar.style.left=(d.lean>0?50:50-w)+'%';
  bar.style.background=traceColor(a);

  byId('vPitch').textContent=d.pitch.toFixed(1);
  byId('vGlat').textContent=d.glat.toFixed(2);
  byId('vGlon').textContent=d.glon.toFixed(2);
  byId('vAir').textContent=d.temp.toFixed(0)+' / '+d.hum.toFixed(0);
  // Speed and risk say something with colour too: without a fix the speed
  // is not "zero", it is "unknown", and an 80% grip risk must catch the
  // eye before it is read.
  setState('vSpeed', d.gpsFix?d.gpsSpeed.toFixed(0):T.noFix, d.gpsFix?'':'off');
  setState('vRisk', d.risk.toFixed(0), d.risk>=60?'hi':(d.risk>=30?'med':'ok'));

  // Delta against the reference lap: green ahead, red behind
  setState('vDelta', d.delta===null?'--':((d.delta>0?'+':'')+(d.delta/1000).toFixed(2)),
           d.delta===null?'off':(d.delta>0?'hi':'ok'));

  byId('rec').innerHTML=d.rec?('<span class="dot"></span>REC '+d.min+' min'):'';
  byId('events').innerHTML=pairs([[T.evW,d.evw],[T.evS,d.evs],[T.evL,d.evl],
                                  [T.evB,d.evb],[T.evA,d.eva]]);
  if(d.lapSet){
   var l=[[T.lap+(d.lapCur?' '+(d.lapNum+1):''),fmtTime(d.lapCur)],[T.last,fmtTime(d.lapLast)],
          [T.best,fmtTime(d.lapBest)]];
   var ideal=idealLap(d.secBest);
   if(ideal)l.push([T.ideal,fmtTime(ideal)]);
   var name=document.createElement('div');  // the name is typed by the user: as text
   name.className='dim';name.style.marginBottom='8px';name.textContent=d.trackName||'';
   byId('laps').innerHTML=pairs(l)+sectorPairs(d);
   if(d.trackName)byId('laps').prepend(name);
  }else byId('laps').textContent=T.noFinish;

  // The trace clears itself on the RISING edge of REC, so every session has
  // its own clean trace
  if(d.rec && !prevRec) resetTrace();
  prevRec=d.rec;
  finishSet=d.lapSet; finishLat=d.finLat; finishLon=d.finLon;
  if(d.gpsFix) addTracePoint(d.gpsLat,d.gpsLon,d.lean);
  drawTrace();

  push(sLean,d.lean);push(sPitch,d.pitch);push(sLat,d.glat);push(sLon,d.glon);
  drawChart('chartAngles',sLean,sPitch,60);drawChart('chartG',sLat,sLon,1.5);
 }catch(e){setLink(false);}
}
setInterval(tick,300);  // about 3 updates per second
)js";

void handleLive() {
  PageWriter page(server, CONTENT_HTML);
  beginPage(page, "WheelStat - " TR_W_NAV_LIVE, Tab::Live);
  page.print(FPSTR(LIVE_BODY));
  page.print(FPSTR(LIVE_SCRIPT));
  page.print(F("</script>"));
  endPage(page);
}

}  // namespace web
