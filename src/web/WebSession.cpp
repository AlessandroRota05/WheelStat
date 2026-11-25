// WheelStat - the session viewer: map and charts of one recorded session,
// its lap table with the ideal lap, and two laps overlaid.
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
<div class='caption' id='cmpRow' hidden><span id='lCmp'></span><select id='cmp' onchange='pick()'></select></div>
<canvas id='map' height='240'></canvas>
<div class='card' id='read'></div>
<div id='deltaBox' hidden><div class='caption'><span id='lDelta'></span></div>
<canvas class='scrub' id='cDelta' height='110'></canvas></div>
<div class='caption'><span id='lSpeed'></span><span class='legend'></span></div>
<canvas class='scrub' id='cSpeed' height='110'></canvas>
<div class='caption'><span id='lLean'></span><span class='legend'></span></div>
<canvas class='scrub' id='cLean' height='110'></canvas>
<div class='caption'><span id='lPitch'></span><span class='legend'></span></div>
<canvas class='scrub' id='cPitch' height='110'></canvas>
<div class='caption'><span id='lG'></span><span class='legend' id='gLegend'></span></div>
<canvas class='scrub' id='cG' height='110'></canvas>
<div id='lapBox'></div>
)html";

static const char SESSION_SCRIPT[] PROGMEM = R"js(
// S: the decoded session. i0..i1: the samples on screen (everything, or one
// lap). cur: the position under the finger, -1 if none - a sample index, or
// a point of the overlay when two laps are compared. C: the overlay, null
// when there is none.
var S=null,i0=0,i1=0,cur=-1,C=null;
var file=new URLSearchParams(location.search).get('f')||'';
var BLUE='#4cc9ff',YELLOW='#ffd166',PURPLE='#c792ea';

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
function fmtGap(ms){return (ms>0?'+':'')+(ms/1000).toFixed(2);}

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
 var ideal=idealLap();
 if(ideal)facts.push([T.ideal,fmtLap(ideal.ms)]);
 var h='<div class="pairs">';
 for(i=0;i<facts.length;i++)h+='<div>'+facts[i][0]+'<b>'+facts[i][1]+'</b></div>';
 h+='</div>';
 if(!S.foot)h+='<p class="warn">'+T.noFooter+'</p>';
 byId('info').innerHTML=h;

 var options='<option value="0">'+T.all+'</option>';
 for(l in S.range)options+='<option value="'+l+'">'+lapName(l)+'</option>';
 byId('range').innerHTML=options;

 byId('lapBox').innerHTML=lapTable(ideal);
 byId('lRange').textContent=T.range;
 byId('lCmp').textContent=T.compare;
 byId('lSpeed').textContent=T.speed;
 byId('lLean').textContent=T.lean+' (+ '+T.right+')';
 byId('lPitch').textContent=T.pitch;
 byId('lG').textContent=T.g;

 // ?lap=4&vs=2 in the address opens that lap compared with that one: a
 // comparison can be bookmarked and opened again after the next session.
 var q=new URLSearchParams(location.search),la=q.get('lap'),vs=q.get('vs');
 if(la&&S.range[la]){
  byId('range').value=la;
  pick();
  if(vs&&S.range[vs]&&vs!=la)byId('cmp').value=vs;
 }
 pick();
}

function lapName(l){
 var done=S.laps[l-1];
 return T.lap+' '+l+(done?' — '+fmtLap(done.ms):'');
}

// The best time of each sector, wherever it was ridden, and their sum: what
// the session would have given with every sector at its best. Only with
// checkpoints - with one sector it would be the best lap again.
function idealLap(){
 if(S.sectors<2||!S.laps.length)return null;
 var best=[],ms=0;
 for(var c=0;c<S.sectors;c++){
  var b=0;
  for(var l=0;l<S.laps.length;l++){var t=S.laps[l].sec[c];if(t&&(!b||t<b))b=t;}
  if(!b)return null;
  best.push(b);ms+=b;
 }
 return {sec:best,ms:ms};
}

// Lap times with the gap to the best lap, sector times with the best of
// each column in green, and the ideal lap at the bottom.
function lapTable(ideal){
 if(!S.laps.length)return '';
 var best=S.best>=0?S.laps[S.best].ms:0,sectors=S.sectors>1,t,l,c;
 t='<h2>'+T.laps+'</h2><div class="card"><table><tr><td class="dim">#</td>'
  +'<td class="dim num">'+T.time+'</td><td class="dim num">'+T.gap+'</td>';
 if(sectors)for(c=0;c<S.sectors;c++)t+='<td class="dim num">S'+(c+1)+'</td>';
 t+='</tr>';
 for(l=0;l<S.laps.length;l++){
  var lap=S.laps[l];
  t+='<tr><td>'+(l+1)+'</td><td class="num'+(l==S.best?' win':'')+'">'+fmtLap(lap.ms)+'</td>'
   +'<td class="num dim">'+(l==S.best||!best?'':fmtGap(lap.ms-best))+'</td>';
  if(sectors)for(c=0;c<S.sectors;c++){
   var top=ideal&&lap.sec[c]==ideal.sec[c];
   t+='<td class="num'+(top?' win':' dim')+'">'+(lap.sec[c]?fmtLap(lap.sec[c]):'--')+'</td>';
  }
  t+='</tr>';
 }
 if(ideal){
  t+='<tr class="total"><td></td><td class="num">'+fmtLap(ideal.ms)+'</td>'
   +'<td class="num dim">'+T.ideal+'</td>';
  for(c=0;c<S.sectors;c++)t+='<td class="num">'+fmtLap(ideal.sec[c])+'</td>';
  t+='</tr>';
 }
 return t+'</table></div>';
}

function pick(){
 var l=+byId('range').value,cmp=byId('cmp'),row=byId('cmpRow');
 if(l&&S.range[l]){i0=S.range[l][0];i1=S.range[l][1];}
 else{i0=0;i1=S.n;}

 // The second lap can be chosen only once a first one is: the list holds
 // the other laps, and is rebuilt only when the first lap changes.
 if(l&&Object.keys(S.range).length>1){
  if(row.hidden||cmp.dataset.for!=l){
   var o='<option value="0">—</option>';
   for(var k in S.range)if(k!=l)o+='<option value="'+k+'">'+lapName(k)+'</option>';
   cmp.innerHTML=o;cmp.dataset.for=l;
  }
  row.hidden=false;
 }else{row.hidden=true;cmp.value='0';}

 C=(!row.hidden&&+cmp.value)?overlay(l,+cmp.value):null;
 byId('deltaBox').hidden=!C;
 if(C)byId('lDelta').textContent=T.deltaOf+' '+l+' '+T.deltaOn+' '+(+cmp.value)+' (s)';

 var leg=C?'<span style="color:'+BLUE+'">'+T.lap+' '+l+'</span> / <span style="color:'+PURPLE+'">'
   +T.lap+' '+(+cmp.value)+'</span>':'';
 document.querySelectorAll('.legend').forEach(function(e){e.innerHTML=leg;});
 // Overlaid, the G chart shows the lateral G of both laps: four lines
 // would not read.
 if(!C)byId('gLegend').innerHTML='<span style="color:'+BLUE+'">'+T.glat+'</span> / <span style="color:'
   +YELLOW+'">'+T.glon+'</span>';
 byId('lG').textContent=C?T.g+' ('+T.glat+')':T.g;

 cur=-1;
 layer={};
 draw();
}

// --- Two laps overlaid ---------------------------------------------------------
// Two laps never have the same number of samples, nor the same line: they
// are compared at the same POINT of the track. Each lap is measured along
// its own path and cut into N equal fractions of its length; at each
// fraction both laps are interpolated, so speed, lean and time line up
// where the bike was in the same place. The time gap at each point says
// where one lap gains or loses on the other.
var N=300;
function resample(l){
 var a=S.range[l][0],b=S.range[l][1],n=b-a,i,k;
 if(n<4)return null;
 var dist=new Float32Array(n),total=0;
 for(i=1;i<n;i++){
  var p=a+i,q=p-1;
  if(S.fix[p]&&S.fix[q])total+=Math.hypot(S.x[p]-S.x[q],S.y[p]-S.y[q]);
  dist[i]=total;
 }
 if(total<50)return null;                 // no usable GPS in this lap

 // The samples are 1/rate apart; the lap time from the summary, when there
 // is one, stretches them to the exact length of the lap.
 var done=S.laps[l-1],scale=done?done.ms/1000/((n-1)/S.rate):1;
 var keys=['speed','lean','pitch','glat','x','y'],r={t:new Float32Array(N),idx:new Int32Array(N)};
 keys.forEach(function(key){r[key]=new Float32Array(N);});
 for(k=0,i=0;k<N;k++){
  var d=total*k/(N-1);
  while(i<n-2&&dist[i+1]<d)i++;
  var span=dist[i+1]-dist[i],f=span>0?Math.min(1,Math.max(0,(d-dist[i])/span)):0;
  keys.forEach(function(key){var s=S[key];r[key][k]=s[a+i]+(s[a+i+1]-s[a+i])*f;});
  r.t[k]=(i+f)/S.rate*scale;
  r.idx[k]=a+i;
 }
 return r;
}

function overlay(la,lb){
 var A=resample(la),B=resample(lb);
 if(!A||!B)return null;
 var delta=new Float32Array(N);
 for(var k=0;k<N;k++)delta[k]=A.t[k]-B.t[k];
 return {A:A,B:B,delta:delta};
}

// --- Drawing -----------------------------------------------------------------
function draw(){
 drawMap();
 if(C){
  var c=cur>=0?cur:-1;
  plot('cDelta',[C.delta],['#e9edf3'],true,0,N,c);
  plot('cSpeed',[C.A.speed,C.B.speed],[BLUE,PURPLE],false,0,N,c);
  plot('cLean',[C.A.lean,C.B.lean],[BLUE,PURPLE],true,0,N,c);
  plot('cPitch',[C.A.pitch,C.B.pitch],[BLUE,PURPLE],true,0,N,c);
  plot('cG',[C.A.glat,C.B.glat],[BLUE,PURPLE],true,0,N,c);
 }else{
  plot('cSpeed',[S.speed],[BLUE],false,i0,i1,cur);
  plot('cLean',[S.lean],[BLUE],true,i0,i1,cur);
  plot('cPitch',[S.pitch],[YELLOW],true,i0,i1,cur);
  plot('cG',[S.glat,S.glon],[BLUE,YELLOW],true,i0,i1,cur);
 }
 readout();
}

function readout(){
 var e=byId('read');
 if(cur<0){e.innerHTML='<span class="dim">'+T.hint+'</span>';return;}
 if(C){
  var d=C.delta[cur];
  e.innerHTML=Math.round(cur*100/(N-1))+'% · <span style="color:'+BLUE+'">'+C.A.speed[cur].toFixed(0)
   +'</span> / <span style="color:'+PURPLE+'">'+C.B.speed[cur].toFixed(0)+'</span> km/h · '
   +T.gap+' <b style="color:var('+(d>0?'--danger':'--good')+')">'+(d>0?'+':'')+d.toFixed(2)+' s</b>';
  return;
 }
 var l=S.lean[cur];
 e.innerHTML=fmtClock((cur-i0)/S.rate)+' · <b>'+S.speed[cur].toFixed(0)+' km/h</b> · '
  +T.leanShort+' <b>'+Math.abs(l).toFixed(1)+'&deg;</b> '+(l<0?T.left:T.right)
  +' · G '+S.glat[cur].toFixed(2)+' / '+S.glon[cur].toFixed(2)
  +(S.lap[cur]?' · '+T.lap+' '+S.lap[cur]:'');
}

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

// Series over [a,b) on a canvas. Each pixel column shows the value of
// largest magnitude that falls in it, so squeezing an hour into a phone
// screen does not flatten the peaks. The scale adapts to what is shown.
function plot(id,series,colors,signed,a,b,at){
 var n=b-a;
 var c=layered(id,function(c,W,H){
  if(n<2)return;
  var max=0,k,i,x;
  for(k=0;k<series.length;k++)
   for(i=a;i<b;i++)max=Math.max(max,Math.abs(series[k][i]));
  max=max||1;
  var base=signed?H/2:H-4,span=signed?H/2-6:H-20;
  c.strokeStyle='#2a3140';c.lineWidth=1;
  c.beginPath();c.moveTo(0,base);c.lineTo(W,base);c.stroke();
  for(k=0;k<series.length;k++){
   var s=series[k];
   c.strokeStyle=colors[k];c.lineWidth=1.5;c.beginPath();
   for(x=0;x<W;x++){
    var p=a+Math.floor(x*n/W),q=a+Math.floor((x+1)*n/W),val=s[p];
    for(i=p+1;i<q;i++)if(Math.abs(s[i])>Math.abs(val))val=s[i];
    var y=base-val/max*span;
    if(x)c.lineTo(x,y);else c.moveTo(x,y);
   }
   c.stroke();
  }
  c.fillStyle='#8b93a4';c.font='11px sans-serif';
  c.fillText((signed?'±':'')+max.toFixed(max<10?2:0),4,12);
 });
 if(c&&at>=a&&at<b){
  var cv=byId(id),cx=(at-a+.5)*cv.clientWidth/n;
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

// M: how metres become pixels on the map, kept for the cursor dots
var M=null;
function px(x){return M.ox+x*M.sc;}
function py(y){return M.oy-y*M.sc;}   // Y flipped: north up

function drawMap(){
 var c=layered('map',function(c,W,H){
  M=null;
  var minX=1e9,maxX=-1e9,minY=1e9,maxY=-1e9,any=0,i;
  for(i=i0;i<i1;i++)if(S.fix[i]){
   any++;
   minX=Math.min(minX,S.x[i]);maxX=Math.max(maxX,S.x[i]);
   minY=Math.min(minY,S.y[i]);maxY=Math.max(maxY,S.y[i]);
  }
  if(C)for(i=0;i<N;i++){
   minX=Math.min(minX,C.B.x[i]);maxX=Math.max(maxX,C.B.x[i]);
   minY=Math.min(minY,C.B.y[i]);maxY=Math.max(maxY,C.B.y[i]);
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

  // The lap compared against, thin and underneath
  c.lineCap='round';
  if(C){
   c.strokeStyle=PURPLE;c.lineWidth=1.5;c.globalAlpha=.8;c.beginPath();
   for(i=0;i<N;i++){if(i)c.lineTo(px(C.B.x[i]),py(C.B.y[i]));else c.moveTo(px(C.B.x[i]),py(C.B.y[i]));}
   c.stroke();c.globalAlpha=1;
  }

  var prev=-1;
  c.lineWidth=3;
  for(i=i0;i<i1;i++){
   if(!S.fix[i]){prev=-1;continue;}    // no line across a GPS dropout
   if(prev>=0){
    c.strokeStyle=leanColor(Math.abs(S.lean[i]));
    c.beginPath();c.moveTo(px(S.x[prev]),py(S.y[prev]));c.lineTo(px(S.x[i]),py(S.y[i]));c.stroke();
   }
   prev=i;
  }
 });
 if(!c||!M||cur<0)return;
 function dot(x,y,col){c.fillStyle=col;c.beginPath();c.arc(px(x),py(y),5,0,7);c.fill();}
 if(C){dot(C.B.x[cur],C.B.y[cur],PURPLE);dot(C.A.x[cur],C.A.y[cur],'#fff');}
 else if(cur>=i0&&cur<i1&&S.fix[cur])dot(S.x[cur],S.y[cur],'#fff');
}

// --- Reading the charts ------------------------------------------------------
function scrub(ev){
 if(!S)return;
 var r=ev.currentTarget.getBoundingClientRect(),a=C?0:i0,n=C?N:i1-i0;
 if(n<1)return;
 cur=a+Math.max(0,Math.min(n-1,Math.floor((ev.clientX-r.left)/r.width*n)));
 draw();
}
['cDelta','cSpeed','cLean','cPitch','cG'].forEach(function(id){
 var cv=byId(id);
 cv.addEventListener('pointerdown',scrub);
 // A mouse scrubs only while pressed; a finger is always "pressed"
 cv.addEventListener('pointermove',function(ev){if(ev.pointerType!='mouse'||ev.buttons)scrub(ev);});
});
window.addEventListener('resize',function(){if(S)draw();});

byId('info').innerHTML='<span class="spin"></span>'+T.loading;
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
