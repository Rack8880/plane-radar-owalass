#pragma once

// Settings web site, compiled into the firmware (served by web_portal.cpp).
// Plain HTML/CSS/JS kept small; all data goes through the /api/* JSON calls.

#include <pgmspace.h>

namespace services::web::pages {

static const char kStyleCss[] PROGMEM = R"rawliteral(
:root{--bg:#0f1419;--card:#1a2129;--fg:#e6edf3;--muted:#8b98a5;--accent:#8b5cf6;
--border:#2a3340;--ok:#3fb950;--err:#f85149;--danger:#b3322c}
*{box-sizing:border-box}
body{margin:0;padding:16px;font-family:system-ui,-apple-system,Segoe UI,Roboto,sans-serif;
background:var(--bg);color:var(--fg);line-height:1.4}
.card{max-width:480px;margin:0 auto;background:var(--card);border:1px solid var(--border);
border-radius:12px;padding:20px}
h1{font-size:1.4rem;margin:0 0 16px}
label{display:block;margin:10px 0 4px;font-size:.92rem}
input[type=text],input[type=number],select{width:100%;padding:8px 10px;background:var(--bg);
color:var(--fg);border:1px solid var(--border);border-radius:8px;font-size:1rem}
fieldset{border:1px solid var(--border);border-radius:8px;margin:16px 0;padding:8px 12px 12px}
legend{color:var(--muted);padding:0 6px;font-size:.85rem}
label.chk{display:flex;align-items:center;gap:10px;margin:8px 0}
label.chk input{width:18px;height:18px;flex:none}
.grid2{display:grid;grid-template-columns:1fr 1fr;gap:0 12px}
.grid4{display:grid;grid-template-columns:repeat(4,1fr);gap:0 8px}
.hint{color:var(--muted);font-size:.82rem;margin:4px 0 0}
button,.btn{display:block;width:100%;margin-top:16px;padding:11px;border:0;border-radius:8px;
background:var(--accent);color:#fff;font-size:1rem;font-weight:600;cursor:pointer;
text-align:center;text-decoration:none}
button.small{display:inline-block;width:auto;margin:0 0 0 6px;padding:6px 12px;font-size:.9rem}
button.ghost{background:transparent;border:1px solid var(--border);color:var(--fg)}
button.danger{background:var(--danger)}
button:disabled{opacity:.45;cursor:default}
.menu .btn{margin-top:12px}
.status{min-height:1.4em;margin:12px 0 0}.status.ok{color:var(--ok)}.status.err{color:var(--err)}
.links{margin-top:18px}.links a{color:var(--muted)}
.loc{border:1px solid var(--border);border-radius:8px;padding:10px 12px;margin:10px 0;
display:flex;align-items:center;justify-content:space-between;gap:8px}
.loc.active{border-color:var(--accent)}
.muted{color:var(--muted);font-size:.88rem}
dl{display:grid;grid-template-columns:auto 1fr;gap:4px 14px;margin:0}dt{color:var(--muted)}dd{margin:0}
.swatches{display:grid;grid-template-columns:repeat(3,1fr);gap:8px}
.sw{margin:0;padding:8px 4px;border:2px solid var(--border);border-radius:8px;background:var(--bg);
color:var(--fg);font-size:.85rem;font-weight:500}
.sw.sel{border-color:var(--accent)}
.sw i{display:inline-block;width:12px;height:12px;border-radius:50%;margin:0 1px 4px;
border:1px solid #fff3}
.colors{display:grid;grid-template-columns:repeat(auto-fill,minmax(130px,1fr));gap:4px 10px}
.colors label{font-size:.85rem}
input[type=color]{width:100%;height:42px;padding:2px;border:1px solid var(--border);
border-radius:8px;background:var(--bg)}
#preview{display:block;width:220px;height:220px;max-width:100%;margin:16px auto 4px}
)rawliteral";

static const char kAppJs[] PROGMEM = R"rawliteral(
"use strict";
const $=(id)=>document.getElementById(id);
function setStatus(msg,ok){const s=$("status");s.textContent=msg||"";
s.className="status"+(ok===false?" err":ok===true?" ok":"");}
async function api(path,body){
const opt=body===undefined?{}:{method:"POST",headers:{"Content-Type":"application/json"},
body:JSON.stringify(body)};
const r=await fetch(path,opt);let d={};try{d=await r.json();}catch(e){}
if(!r.ok||d.ok===false)throw new Error(d.error||("HTTP "+r.status));return d;}
)rawliteral";

static const char kIndexHtml[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Plane Radar</title><link rel="stylesheet" href="/style.css"></head>
<body><main class="card"><h1>&#9992; Plane Radar</h1>
<nav class="menu">
<a class="btn" href="/radar">Radar settings</a>
<a class="btn" href="/colours">Colours</a>
<a class="btn" href="/locs">Saved locations</a>
<a class="btn" href="/device">Device</a>
</nav></main></body></html>
)rawliteral";

static const char kRadarHtml[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Radar settings</title><link rel="stylesheet" href="/style.css"></head>
<body><main class="card"><h1>Radar settings</h1>
<form id="f">
<fieldset><legend>Radar centre</legend>
<div class="grid2">
<div><label for="lat">Latitude</label><input id="lat" type="number" step="any" required></div>
<div><label for="lon">Longitude</label><input id="lon" type="number" step="any" required></div>
</div>
<p class="hint">Or pick one under Saved locations.</p>
</fieldset>

<fieldset><legend>Range</legend>
<label for="range">Range shown now (the BOOT button also cycles it)</label>
<select id="range"></select>
<label>Preset distances, km (each bigger than the last)</label>
<div class="grid4">
<input id="p0" type="number" step="0.1" min="0.5" max="500" required>
<input id="p1" type="number" step="0.1" min="0.5" max="500" required>
<input id="p2" type="number" step="0.1" min="0.5" max="500" required>
<input id="p3" type="number" step="0.1" min="0.5" max="500" required>
</div>
<p class="hint">The distance is the label on the third ring.</p>
</fieldset>

<fieldset><legend>Screen</legend>
<label class="chk"><input type="checkbox" id="useMiles"> Distances in miles</label>
<label class="chk"><input type="checkbox" id="showRunways"> Show airport runways</label>
<label class="chk"><input type="checkbox" id="showCount"> Show number of planes</label>
</fieldset>

<fieldset><legend>Plane labels</legend>
<label class="chk"><input type="checkbox" id="showFlight"> Callsign / flight number</label>
<label class="chk"><input type="checkbox" id="showType"> Aircraft type</label>
<label class="chk"><input type="checkbox" id="showAlt"> Altitude</label>
<label class="chk"><input type="checkbox" id="useMeters"> Altitude in metres (not feet)</label>
</fieldset>

<fieldset><legend>Watch filter</legend>
<label for="watch">Only show callsigns starting with</label>
<input id="watch" type="text" maxlength="48" placeholder="e.g. KLM,BAW,EZY" autocapitalize="characters">
<p class="hint">Separate with commas. Leave empty to show every plane.</p>
</fieldset>

<button type="submit">Save</button>
<p id="status" class="status" role="status"></p>
</form>
<nav class="links"><a href="/">&larr; Back to menu</a></nav>
</main>
<script src="/app.js"></script>
<script>
const boxes=["useMiles","showRunways","showCount","showFlight","showType","showAlt","useMeters"];
function fillRange(sel){const mi=$("useMiles").checked;const r=$("range");r.innerHTML="";
for(let i=0;i<4;i++){const km=parseFloat($("p"+i).value)||0;const o=document.createElement("option");
o.value=i;o.textContent=mi?(km/1.609344).toFixed(1)+" mi":km+" km";r.append(o);}
r.value=sel;}
async function load(){try{const d=await api("/api/radar");
$("lat").value=Number(d.lat).toFixed(6);$("lon").value=Number(d.lon).toFixed(6);
boxes.forEach(k=>$(k).checked=!!d[k]);$("watch").value=d.watch||"";
d.presets.forEach((v,i)=>$("p"+i).value=+Number(v).toFixed(1));
fillRange(d.range);}catch(e){setStatus("Could not load settings: "+e.message,false);}}
["p0","p1","p2","p3","useMiles"].forEach(id=>$(id).addEventListener("change",()=>fillRange($("range").value)));
$("f").addEventListener("submit",async(ev)=>{ev.preventDefault();
const body={lat:parseFloat($("lat").value),lon:parseFloat($("lon").value),
range:parseInt($("range").value,10),watch:$("watch").value,
presets:[0,1,2,3].map(i=>parseFloat($("p"+i).value))};
boxes.forEach(k=>body[k]=$(k).checked);
setStatus("Saving...");
try{await api("/api/radar",body);setStatus("Saved. The radar updates within a second.",true);load();}
catch(e){setStatus(e.message,false);}});
load();
</script></body></html>
)rawliteral";

static const char kColoursHtml[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Colours</title><link rel="stylesheet" href="/style.css"></head>
<body><main class="card"><h1>Colours</h1>
<label>Themes</label>
<div id="presets" class="swatches"></div>

<svg id="preview" viewBox="0 0 240 240" font-family="sans-serif" font-weight="bold" aria-label="Preview">
<circle data-fill="bg" cx="120" cy="120" r="120"/>
<g data-stroke="grid" fill="none" stroke-width="2">
<circle cx="120" cy="120" r="26.75"/><circle cx="120" cy="120" r="53.5"/>
<circle cx="120" cy="120" r="80.25"/><circle cx="120" cy="120" r="107"/>
<line x1="120" y1="13" x2="120" y2="227"/><line x1="13" y1="120" x2="227" y2="120"/>
</g>
<g data-stroke="runway" stroke-width="2"><line x1="44" y1="74" x2="74" y2="62"/><line x1="48" y1="84" x2="76" y2="72"/></g>
<text data-fill="airport" x="60" y="100" font-size="12" text-anchor="middle">EHAM</text>
<line data-stroke="heading" x1="168" y1="150" x2="190" y2="128" stroke-width="2"/>
<polygon data-fill="plane" points="168,150 156,154 164,162"/>
<polygon data-fill="plane" points="88,176 98,166 100,178"/>
<circle data-fill="plane" cx="204" cy="36" r="4"/>
<text data-fill="text" x="150" y="148" font-size="12" text-anchor="end">KLM123</text>
<text data-fill="type" x="150" y="161" font-size="12" text-anchor="end">B738</text>
<text data-fill="alt" x="150" y="174" font-size="12" text-anchor="end">35000 ft</text>
<text data-fill="text" x="120" y="14" font-size="15" text-anchor="middle">N</text>
<text data-fill="text" x="238" y="125" font-size="15" text-anchor="end">E</text>
<text data-fill="text" x="221" y="112" font-size="11" text-anchor="end">10km</text>
<text data-fill="text" x="120" y="219" font-size="11" text-anchor="middle">7</text>
<circle data-fill="centre" cx="120" cy="120" r="2.5"/>
</svg>
<p class="hint" style="text-align:center">Preview, using the screen's 256-colour range</p>

<div id="pickers" class="colors"></div>
<label class="chk"><input type="checkbox" id="swap"> Swap red and blue</label>
<p class="hint">Leave this on for this screen. Only change it if red shows up as blue on the radar (or blue as red).</p>

<button id="save">Save</button>
<p id="status" class="status" role="status"></p>
<nav class="links"><a href="/">&larr; Back to menu</a></nav>
</main>
<script src="/app.js"></script>
<script>
let presets=[],slots=[],preset="custom";
// What the glass can actually show. The frame keeps 3 bits for the panel's
// red channel and 2 for blue; with "swap" on (this panel), glass red comes
// from that 2-bit channel and glass blue from the 3-bit one.
function q(hex){const n=parseInt(hex.slice(1),16),sw=$("swap").checked;
const L=(v,bits)=>Math.round((v>>(8-bits))*255/((1<<bits)-1));
const h=v=>v.toString(16).padStart(2,"0");
return "#"+h(L(n>>16&255,sw?2:3))+h(L(n>>8&255,3))+h(L(n&255,sw?3:2));}
const val=k=>$("c_"+k).value;
function paint(){
document.querySelectorAll("[data-fill]").forEach(e=>e.setAttribute("fill",q(val(e.dataset.fill))));
document.querySelectorAll("[data-stroke]").forEach(e=>e.setAttribute("stroke",q(val(e.dataset.stroke))));
document.querySelectorAll(".sw").forEach(b=>b.classList.toggle("sel",b.dataset.id===preset));}
function setAll(c){slots.forEach(s=>{if(c[s.key])$("c_"+s.key).value=c[s.key];});}
function pick(p){preset=p.id;setAll(p.colors);paint();}
function chip(c){const i=document.createElement("i");i.style.background=c;return i;}
async function load(){try{const d=await api("/api/theme");presets=d.presets;slots=d.slots;
const pk=$("pickers");pk.innerHTML="";
for(const s of slots){const l=document.createElement("label");l.textContent=s.label;
const i=document.createElement("input");i.type="color";i.id="c_"+s.key;
i.addEventListener("input",()=>{preset="custom";paint();});l.append(i);pk.append(l);}
const box=$("presets");box.innerHTML="";
for(const p of presets){const b=document.createElement("button");b.type="button";b.className="sw";
b.dataset.id=p.id;const c=p.colors;
b.append(chip(c.bg),chip(c.grid),chip(c.plane),chip(c.text),document.createElement("br"),p.name);
b.onclick=()=>pick(p);box.append(b);}
const c=document.createElement("button");c.type="button";c.className="sw";c.dataset.id="custom";
c.textContent="Custom";c.onclick=()=>{preset="custom";paint();};box.append(c);
setAll(d.colors);$("swap").checked=d.swap;preset=d.preset;paint();}
catch(e){setStatus("Could not load colours: "+e.message,false);}}
$("swap").addEventListener("change",paint);
$("save").addEventListener("click",async()=>{setStatus("Saving...");
const colors={};slots.forEach(s=>colors[s.key]=val(s.key));
try{await api("/api/theme",{preset,colors,swap:$("swap").checked});setStatus("Saved. Look at the radar!",true);}
catch(e){setStatus(e.message,false);}});
load();
</script></body></html>
)rawliteral";

static const char kLocsHtml[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Saved locations</title><link rel="stylesheet" href="/style.css"></head>
<body><main class="card"><h1>Saved locations</h1>
<div id="list"></div>
<form id="add">
<fieldset><legend>Add a location</legend>
<label for="name">Name</label><input id="name" type="text" maxlength="16" required placeholder="e.g. Home">
<div class="grid2">
<div><label for="lat">Latitude</label><input id="lat" type="number" step="any" required></div>
<div><label for="lon">Longitude</label><input id="lon" type="number" step="any" required></div>
</div>
<button type="button" class="ghost" id="here">Fill in where the radar points now</button>
<button type="submit">Save location</button>
</fieldset>
</form>
<p id="status" class="status" role="status"></p>
<nav class="links"><a href="/">&larr; Back to menu</a></nav>
</main>
<script src="/app.js"></script>
<script>
let st={max:3,locations:[]};
function render(){const list=$("list");list.innerHTML="";
const used=st.locations.filter(l=>l.valid);
if(!used.length){const p=document.createElement("p");p.className="muted";
p.textContent="No saved locations yet (up to "+st.max+").";list.append(p);}
for(const l of used){const row=document.createElement("div");row.className="loc"+(l.active?" active":"");
const info=document.createElement("div");const b=document.createElement("b");
b.textContent=(l.name||"(no name)")+(l.active?"  \u25CF in use":"");
const c=document.createElement("div");c.className="muted";
c.textContent=Number(l.lat).toFixed(5)+", "+Number(l.lon).toFixed(5);info.append(b,c);
const acts=document.createElement("div");
const use=document.createElement("button");use.className="small";use.textContent="Use";use.disabled=l.active;
use.onclick=()=>act({action:"use",slot:l.slot},"Radar moved to "+(l.name||"location")+".");
const del=document.createElement("button");del.className="small danger";del.textContent="Delete";
del.onclick=()=>{if(confirm("Delete "+(l.name||"this location")+"?"))act({action:"delete",slot:l.slot},"Deleted.");};
acts.append(use,del);row.append(info,acts);list.append(row);}
$("add").style.display=used.length>=st.max?"none":"";
if(used.length>=st.max){const p=document.createElement("p");p.className="muted";
p.textContent="All "+st.max+" spaces used. Delete one to add another.";list.append(p);}}
async function load(){try{st=await api("/api/locations");render();}
catch(e){setStatus("Could not load: "+e.message,false);}}
async function act(body,msg){setStatus("Working...");
try{await api("/api/locations",body);setStatus(msg,true);await load();return true;}
catch(e){setStatus(e.message,false);return false;}}
$("here").onclick=()=>{$("lat").value=Number(st.lat).toFixed(6);$("lon").value=Number(st.lon).toFixed(6);};
$("add").addEventListener("submit",async(ev)=>{ev.preventDefault();
const free=st.locations.find(l=>!l.valid);if(!free){setStatus("No free space.",false);return;}
if(await act({action:"save",slot:free.slot,name:$("name").value.trim(),
lat:parseFloat($("lat").value),lon:parseFloat($("lon").value)},"Location saved. Press Use to switch to it.")){
$("name").value="";$("lat").value="";$("lon").value="";}});
load();
</script></body></html>
)rawliteral";

static const char kDeviceHtml[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Device</title><link rel="stylesheet" href="/style.css"></head>
<body><main class="card"><h1>Device</h1>
<dl id="info"></dl>
<button id="restart" class="ghost">Restart radar</button>
<button id="wifi" class="danger">Forget Wi-Fi only</button>
<p class="hint">Forgets the Wi-Fi network and restarts into setup mode
(join <b>PlaneRadar-Setup</b> to connect it again). Your settings, colours
and saved locations are kept.</p>
<p class="hint">Holding the BOOT button for 3 seconds does a bigger reset:
Wi-Fi, radar position and distance units.</p>
<p id="status" class="status" role="status"></p>
<nav class="links"><a href="/">&larr; Back to menu</a></nav>
</main>
<script src="/app.js"></script>
<script>
function row(k,v){const dt=document.createElement("dt");dt.textContent=k;
const dd=document.createElement("dd");dd.textContent=v;$("info").append(dt,dd);}
function dur(s){const d=Math.floor(s/86400),h=Math.floor(s%86400/3600),m=Math.floor(s%3600/60);
return (d?d+"d ":"")+h+"h "+m+"m";}
async function load(){try{const d=await api("/api/device");$("info").innerHTML="";
row("Firmware",d.version+" ("+d.built+")");row("Address","http://"+d.mdns);row("IP address",d.ip);
row("Network",d.ssid+" ("+d.rssi+" dBm)");row("Running for",dur(d.uptime));
row("Free memory",Math.round(d.heap/1024)+" KB");}
catch(e){setStatus("Could not load: "+e.message,false);}}
async function post(path,ask,msg){if(!confirm(ask))return;setStatus("Working...");
try{await api(path,{});}catch(e){}setStatus(msg,true);}
$("restart").onclick=()=>post("/api/restart","Restart the radar now?",
"Restarting. Reload this page in about 20 seconds.");
$("wifi").onclick=()=>post("/api/wifi/reset","Forget the Wi-Fi network? The radar restarts into setup mode.",
"Wi-Fi forgotten. Join the PlaneRadar-Setup network to set it up again.");
load();
</script></body></html>
)rawliteral";

}  // namespace services::web::pages
