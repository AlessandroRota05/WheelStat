// WheelStat - the session viewer: map and charts of one recorded session.
//
// The device only sends the log as it is on flash (/session/data); decoding
// and drawing happen in the phone's browser, which has far more memory and
// speed to spare. The script below therefore reads the binary layout of
// src/core/SessionCodec.h: the two must be changed together.
//
// Body and script contain no translated text: every label comes from the T
// dictionary, so the same script can be tried out in a browser without the
// device.

#include "../Channels.h"
#include "../Tuning.h"
#include "../core/SessionCodec.h"
#include "../i18n/Text.h"
#include "WebInternal.h"

namespace web {

static const char SESSION_BODY[] PROGMEM = R"html(
<h2 id='title'></h2>
<div class='card' id='info'></div>
<div class='caption'><span id='lRange'></span><select id='range' onchange='pick()'></select></div>
<canvas id='map' height='240'></canvas>
<div class='card' id='read'></div>
<div class='caption'><span id='lSpeed'></span></div>
<canvas class='scrub' id='cSpeed' height='110'></canvas>
<div class='caption'><span id='lLean'></span></div>
<canvas class='scrub' id='cLean' height='110'></canvas>
<div class='caption'><span id='lPitch'></span></div>
<canvas class='scrub' id='cPitch' height='110'></canvas>
<div class='caption'><span id='lG'></span><span><span style='color:var(--accent)' id='lGlat'></span>
 / <span style='color:var(--warn)' id='lGlon'></span></span></div>
<canvas class='scrub' id='cG' height='110'></canvas>
<div id='lapBox'></div>
)html";

static const char SESSION_SCRIPT[] PROGMEM = R"js(
// S: the decoded session. i0..i1: the samples on screen (everything, or one
// lap). cur: the sample under the finger, -1 if none.
var S=null,i0=0,i1=0,cur=-1;
var file=new URLSearchParams(location.search).get('f')||'';

function byId(id){return document.getElementById(id);}

// "m:ss.cc" for lap times, "m:ss.s" for a position in time
function fmtLap(ms){
 var s=ms/1000,m=Math.floor(s/60);s-=m*60;
 return m+':'+(s<10?'0':'')+s.toFixed(2);
}
function fmtClock(s){
 var m=Math.floor(s/60);s-=m*60;
 return m+':'+(s<10?'0':'')+s.toFixed(1);
}

// --- Decoding ----------------------------------------------------------------
// Offsets are those of SessionHeader, SessionSample and SessionFooter in
// src/core/SessionCodec.h. The sizes come from the header itself.
function parse(buf){
 var v=new DataView(buf),size=buf.byteLength;
 if(size<12||v.getUint32(0,true)!=K.magic)return null;
 var hs=v.getUint16(4,true),ss=v.getUint16(6,true),fs=v.getUint16(8,true);
 if(size<hs||ss<24)return null;

 var s={rate:v.getUint8(10)||1,sectors:v.getUint8(11),laps:[],best:-1,foot:false};
 s.track=new TextDecoder().decode(new Uint8Array(buf,12,24)).replace(/\0[\s\S]*$/,'');

 // The summary is the last fs bytes - if the recording was closed properly
 var end=size,o=size-fs;
 if(o>=hs&&v.getUint32(o,true)==K.foot){
  end=o;s.foot=true;
  s.duration=v.getUint32(o+8,true);
  var p=o+12+4*K.ch+2*K.ev;            // past the maxima and the event counts
  var count=Math.min(v.getUint8(p),K.laps);
  s.best=v.getInt8(p+1);
  p=o+((p-o+2+3)&~3);                  // lap times start on a 4-byte boundary
  for(var l=0;l<count;l++){
   var lap={ms:v.getUint32(p+4*l,true),sec:[]};
   for(var c=0;c<s.sectors&&c<K.sec;c++)
    lap.sec.push(v.getUint32(p+4*K.laps+4*(l*K.sec+c),true));
   s.laps.push(lap);
  }
 }

 var n=s.n=Math.floor((end-hs)/ss);
 s.lat=new Float64Array(n);s.lon=new Float64Array(n);
 s.lean=new Float32Array(n);s.pitch=new Float32Array(n);
 s.glat=new Float32Array(n);s.glon=new Float32Array(n);
 s.speed=new Float32Array(n);s.lap=new Uint8Array(n);s.fix=new Uint8Array(n);
 for(var i=0;i<n;i++){
  o=hs+i*ss;
  s.lat[i]=v.getInt32(o,true)/1e7;
  s.lon[i]=v.getInt32(o+4,true)/1e7;
  s.lean[i]=v.getInt16(o+8,true)/10;
  s.pitch[i]=v.getInt16(o+10,true)/10;
  s.glat[i]=v.getInt16(o+12,true)/100;
  s.glon[i]=v.getInt16(o+14,true)/100;
  s.speed[i]=v.getUint16(o+16,true)/10;
  s.lap[i]=v.getUint8(o+21);
  s.fix[i]=v.getUint8(o+22)&1;
 }
 return s;
}

// Positions in metres from the first valid one (flat local projection: over
// the area of a ride the Earth is flat enough), and the samples of each lap.
function prepare(){
 var n=S.n,ref=-1,i;
 S.x=new Float32Array(n);S.y=new Float32Array(n);
 for(i=0;i<n;i++)if(S.fix[i]){ref=i;break;}
 if(ref>=0){
  var kx=111320*Math.cos(S.lat[ref]*Math.PI/180);
  for(i=0;i<n;i++){
   S.x[i]=(S.lon[i]-S.lon[ref])*kx;
   S.y[i]=(S.lat[i]-S.lat[ref])*111320;
  }
 }
 S.range={};
 for(i=0;i<n;i++){
  var l=S.lap[i];
  if(!l)continue;
  if(S.range[l])S.range[l][1]=i+1;else S.range[l]=[i,i+1];
 }
}

// --- Page --------------------------------------------------------------------
function show(){
 byId('title').textContent=file+' · '+(S.track=='free'?T.free:S.track);

 var maxSpeed=0,maxR=0,maxL=0,i,l;
 for(i=0;i<S.n;i++){
  maxSpeed=Math.max(maxSpeed,S.speed[i]);
  maxR=Math.max(maxR,S.lean[i]);
  maxL=Math.max(maxL,-S.lean[i]);
 }
 var facts=[[T.duration,fmtClock(S.foot?S.duration/1000:S.n/S.rate)],
  [T.maxSpeed,maxSpeed.toFixed(0)+' km/h'],
  [T.maxLean,maxR.toFixed(0)+'&deg; '+T.right+' / '+maxL.toFixed(0)+'&deg; '+T.left]];
 if(S.best>=0&&S.laps[S.best])facts.push([T.best,fmtLap(S.laps[S.best].ms)]);
 var h='<div class="pairs">';
 for(i=0;i<facts.length;i++)h+='<div>'+facts[i][0]+'<b>'+facts[i][1]+'</b></div>';
 h+='</div>';
 if(!S.foot)h+='<p class="warn">'+T.noFooter+'</p>';
 byId('info').innerHTML=h;

 var options='<option value="0">'+T.all+'</option>';
 for(l in S.range){
  var done=S.laps[l-1];
  options+='<option value="'+l+'">'+T.lap+' '+l+(done?' — '+fmtLap(done.ms):'')+'</option>';
 }
 byId('range').innerHTML=options;

 if(S.laps.length){
  var t='<h2>'+T.laps+'</h2><div class="card"><table>';
  for(l=0;l<S.laps.length;l++){
   t+='<tr><td>'+(l+1)+'</td><td class="num'+(l==S.best?' win':'')+'">'+fmtLap(S.laps[l].ms)+'</td>';
   if(S.laps[l].sec.length>1)
    for(i=0;i<S.laps[l].sec.length;i++)t+='<td class="num dim">'+fmtLap(S.laps[l].sec[i])+'</td>';
   t+='</tr>';
  }
  byId('lapBox').innerHTML=t+'</table></div>';
 }

 byId('lRange').textContent=T.range;
 byId('lSpeed').textContent=T.speed;
 byId('lLean').textContent=T.lean+' (+ '+T.right+')';
 byId('lPitch').textContent=T.pitch;
 byId('lG').textContent=T.g;
 byId('lGlat').textContent=T.glat;
 byId('lGlon').textContent=T.glon;
 pick();
}

function pick(){
 var l=+byId('range').value;
 if(l&&S.range[l]){i0=S.range[l][0];i1=S.range[l][1];}
 else{i0=0;i1=S.n;}
 cur=-1;
 layer={};
 draw();
}

function draw(){
 drawMap();
 plot('cSpeed',[S.speed],['#4cc9ff'],false);
 plot('cLean',[S.lean],['#4cc9ff'],true);
 plot('cPitch',[S.pitch],['#ffd166'],true);
 plot('cG',[S.glat,S.glon],['#4cc9ff','#ffd166'],true);
 readout();
}

function readout(){
 var e=byId('read');
 if(cur<0){e.innerHTML='<span class="dim">'+T.hint+'</span>';return;}
 var l=S.lean[cur];
 e.innerHTML=fmtClock((cur-i0)/S.rate)+' · <b>'+S.speed[cur].toFixed(0)+' km/h</b> · '
  +T.leanShort+' <b>'+Math.abs(l).toFixed(1)+'&deg;</b> '+(l<0?T.left:T.right)
  +' · G '+S.glat[cur].toFixed(2)+' / '+S.glon[cur].toFixed(2)
  +(S.lap[cur]?' · '+T.lap+' '+S.lap[cur]:'');
}

// --- Drawing -----------------------------------------------------------------
// A long session is tens of thousands of samples: too many to redraw at
// every move of the finger. Each canvas keeps its curves in an off-screen
// copy, painted once per selection (and again if the width changes); a move
// only puts that copy back and draws the cursor on top.
//
// Both are sized at the screen's pixel density, so lines stay sharp on a
// phone; the drawing code works in CSS pixels all the same. The height
// attribute written in the page is the CSS height, kept in data-h.
var layer={};
function layered(id,paint){
 var cv=byId(id),r=window.devicePixelRatio||1,W=cv.clientWidth,l=layer[id];
 if(!cv.dataset.h)cv.dataset.h=cv.getAttribute('height');
 var H=+cv.dataset.h;
 if(W<2)return null;
 if(!l||l.cssW!=W){
  l=layer[id]=document.createElement('canvas');
  l.cssW=W;l.width=Math.round(W*r);l.height=Math.round(H*r);
  var lc=l.getContext('2d');
  lc.setTransform(r,0,0,r,0,0);
  paint(lc,W,H);
 }
 cv.style.height=H+'px';
 cv.width=l.width;cv.height=l.height;
 var c=cv.getContext('2d');
 c.drawImage(l,0,0);
 c.setTransform(r,0,0,r,0,0);
 return c;
}

// One or two series on a canvas. Each pixel column shows the value of
// largest magnitude that falls in it, so squeezing an hour into a phone
// screen does not flatten the peaks. The scale adapts to the selection.
function plot(id,series,colors,signed){
 var n=i1-i0;
 var c=layered(id,function(c,W,H){
  if(n<2)return;
  var max=0,k,i,x;
  for(k=0;k<series.length;k++)
   for(i=i0;i<i1;i++)max=Math.max(max,Math.abs(series[k][i]));
  max=max||1;
  var base=signed?H/2:H-4,span=signed?H/2-6:H-20;
  c.strokeStyle='#2a3140';c.lineWidth=1;
  c.beginPath();c.moveTo(0,base);c.lineTo(W,base);c.stroke();
  for(k=0;k<series.length;k++){
   var s=series[k];
   c.strokeStyle=colors[k];c.lineWidth=1.5;c.beginPath();
   for(x=0;x<W;x++){
    var a=i0+Math.floor(x*n/W),b=i0+Math.floor((x+1)*n/W),val=s[a];
    for(i=a+1;i<b;i++)if(Math.abs(s[i])>Math.abs(val))val=s[i];
    var y=base-val/max*span;
    if(x)c.lineTo(x,y);else c.moveTo(x,y);
   }
   c.stroke();
  }
  c.fillStyle='#8b93a4';c.font='11px sans-serif';
  c.fillText((signed?'±':'')+max.toFixed(max<10?2:0),4,12);
 });
 if(c&&cur>=i0&&cur<i1){
  var cv=byId(id),cx=(cur-i0+.5)*cv.clientWidth/n;
  c.strokeStyle='#fff';c.lineWidth=1;
  c.beginPath();c.moveTo(cx,0);c.lineTo(cx,+cv.dataset.h);c.stroke();
 }
}

// Same lean bands as the live trace
function leanColor(lean){
 if(lean<20)return'#4ede93';
 if(lean<35)return'#ffd166';
 return'#ff5d6c';
}

// M: how metres become pixels on the map, kept for the cursor dot
var M=null;
function mx(i){return M.ox+S.x[i]*M.sc;}
function my(i){return M.oy-S.y[i]*M.sc;}   // Y flipped: north up

function drawMap(){
 var c=layered('map',function(c,W,H){
  M=null;
  var minX=1e9,maxX=-1e9,minY=1e9,maxY=-1e9,any=0,i;
  for(i=i0;i<i1;i++)if(S.fix[i]){
   any++;
   minX=Math.min(minX,S.x[i]);maxX=Math.max(maxX,S.x[i]);
   minY=Math.min(minY,S.y[i]);maxY=Math.max(maxY,S.y[i]);
  }
  if(any<2){
   c.fillStyle='#8b93a4';c.font='13px sans-serif';
   c.fillText(T.noGps,10,H/2);
   return;
  }
  // One scale for both axes (or the path would look distorted), centred
  var pad=14;
  var sc=Math.min((W-2*pad)/Math.max(1,maxX-minX),(H-2*pad)/Math.max(1,maxY-minY));
  M={sc:sc,ox:(W-(maxX-minX)*sc)/2-minX*sc,oy:(H+(maxY-minY)*sc)/2+minY*sc};

  var prev=-1;
  c.lineWidth=3;c.lineCap='round';
  for(i=i0;i<i1;i++){
   if(!S.fix[i]){prev=-1;continue;}    // no line across a GPS dropout
   if(prev>=0){
    c.strokeStyle=leanColor(Math.abs(S.lean[i]));
    c.beginPath();c.moveTo(mx(prev),my(prev));c.lineTo(mx(i),my(i));c.stroke();
   }
   prev=i;
  }
 });
 if(c&&M&&cur>=i0&&cur<i1&&S.fix[cur]){
  c.fillStyle='#fff';
  c.beginPath();c.arc(mx(cur),my(cur),5,0,7);c.fill();
 }
}

// --- Reading the charts ------------------------------------------------------
function scrub(ev){
 var r=ev.currentTarget.getBoundingClientRect(),n=i1-i0;
 if(!S||n<1)return;
 cur=i0+Math.max(0,Math.min(n-1,Math.floor((ev.clientX-r.left)/r.width*n)));
 draw();
}
['cSpeed','cLean','cPitch','cG'].forEach(function(id){
 var cv=byId(id);
 cv.addEventListener('pointerdown',scrub);
 // A mouse scrubs only while pressed; a finger is always "pressed"
 cv.addEventListener('pointermove',function(ev){if(ev.pointerType!='mouse'||ev.buttons)scrub(ev);});
});
window.addEventListener('resize',function(){if(S)draw();});

byId('info').textContent=T.loading;
fetch('/session/data?f='+encodeURIComponent(file))
 .then(function(r){if(!r.ok)throw new Error(r.status);return r.arrayBuffer();})
 .then(function(b){
  S=parse(b);
  if(!S)throw new Error('format');
  prepare();show();
 })
 .catch(function(e){console.error(e);byId('info').textContent=T.error;});
)js";

void handleSessionView() {
  PageWriter page(server, CONTENT_HTML);
  beginPage(page, "WheelStat - " TR_W_NAV_SESSIONS, Tab::Sessions);
  page.print(FPSTR(SESSION_BODY));

  // K: the constants of the log layout the script cannot know by itself.
  page.printf("<script>var K={magic:%lu,foot:%lu,ch:%d,ev:%d,laps:%d,sec:%d};",
              (unsigned long)core::SESSION_MAGIC, (unsigned long)core::SESSION_FOOTER_MAGIC,
              (int)CHANNEL_COUNT, (int)EVENT_COUNT, MAX_LAPS, MAX_SECTORS);
  page.print(F("var T=" TR_W_SESSION_JS_DICT ";"));
  page.print(FPSTR(SESSION_SCRIPT));
  page.print(F("</script>"));
  endPage(page);
}

}  // namespace web
