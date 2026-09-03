#pragma once

#include <Arduino.h>

// Single-page web app, embedded uncompressed in PROGMEM. Kept in one file so
// the UI can never drift out of sync with the firmware. Served at "/".
//
// (The plan mentioned gzipping this blob; it is stored uncompressed here to
// avoid a build-time asset step. It is small enough to serve directly.)

namespace luma {

static const char kIndexHtml[] PROGMEM = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="theme-color" content="#0b0e14">
<title>LumaCanopy</title>
<link rel="manifest" href="/manifest.webmanifest">
<style>
  :root { color-scheme: dark; }
  * { box-sizing: border-box; }
  body { margin:0; font-family: system-ui, -apple-system, Segoe UI, Roboto, sans-serif;
         background:#0b0e14; color:#e8ecf1; -webkit-tap-highlight-color: transparent; }
  .wrap { max-width:520px; margin:0 auto; padding:20px 18px 40px; }
  h1 { font-size:20px; font-weight:600; letter-spacing:.3px; margin:8px 0 2px; }
  .sub { color:#8b96a5; font-size:13px; margin-bottom:18px; }
  .card { background:#141924; border:1px solid #222b3a; border-radius:16px; padding:18px; margin-bottom:14px; }
  .banner { background:#3a1414; border-color:#7a2b2b; color:#ffb4b4; display:none; }
  .banner.on { display:block; }
  .row { display:flex; align-items:center; justify-content:space-between; gap:12px; }
  .badge { font-size:12px; padding:4px 10px; border-radius:999px; background:#1d2636; color:#9fb2cc; border:1px solid #2b3a52; }
  .badge.remote { background:#1d2a1d; color:#a7e0a0; border-color:#2f4a2f; }
  .badge.lock { background:#3a1414; color:#ffb4b4; border-color:#7a2b2b; }
  .big { font-size:44px; font-weight:700; margin:6px 0 0; }
  .unit { font-size:16px; color:#8b96a5; font-weight:500; }
  input[type=range]{ width:100%; height:38px; }
  button { font:inherit; border:none; border-radius:12px; padding:12px 16px; cursor:pointer; }
  .toggle { width:100%; font-size:16px; font-weight:600; }
  .on { background:#1f7a3d; color:#fff; }
  .off { background:#2a2f3a; color:#cfd6e0; }
  .ghost { background:transparent; color:#9fb2cc; border:1px solid #2b3a52; width:100%; }
  .meta { color:#8b96a5; font-size:12px; margin-top:14px; line-height:1.6; }
  a { color:#7fa8ff; }
  .k { color:#8b96a5; }
  label { font-size:13px; color:#9fb2cc; display:block; margin:10px 0 4px; }
  input[type=text],input[type=password]{ width:100%; padding:10px; border-radius:10px;
     border:1px solid #2b3a52; background:#0e1420; color:#e8ecf1; font:inherit; }
  details { margin-top:6px; }
  summary { cursor:pointer; color:#9fb2cc; font-size:13px; }
</style>
</head>
<body>
<div class="wrap">
  <h1>LumaCanopy</h1>
  <div class="sub" id="conn">connecting...</div>

  <div class="card banner" id="lockBanner">Kill switch engaged - output locked off. Nothing can override until it is released.</div>

  <div class="card">
    <div class="row">
      <span class="badge" id="modeBadge">--</span>
      <span class="k" id="amps">-- A</span>
    </div>
    <div class="big"><span id="lvl">0</span><span class="unit">%</span></div>
    <input type="range" id="slider" min="0" max="100" value="0">
  </div>

  <div class="card">
    <button class="toggle off" id="outBtn">Output OFF</button>
    <div style="height:10px"></div>
    <button class="ghost" id="relBtn">Return control to knob</button>
  </div>

  <div class="card">
    <details>
      <summary>Settings</summary>
      <label>Access PIN (required for control)</label>
      <input type="password" id="pin" placeholder="PIN">
      <button class="ghost" id="savePin" style="margin-top:10px">Save PIN on this device</button>
      <hr style="border-color:#222b3a; margin:16px 0">
      <label>Join Wi-Fi network (SSID)</label>
      <input type="text" id="ssid" placeholder="SSID">
      <label>Wi-Fi password</label>
      <input type="password" id="wpass" placeholder="password">
      <button class="ghost" id="saveWifi" style="margin-top:10px">Save Wi-Fi &amp; reboot</button>
    </details>
    <div class="meta" id="meta"></div>
  </div>
</div>

<script>
const $ = id => document.getElementById(id);
let pin = localStorage.getItem('luma_pin') || '';
$('pin').value = pin;
let suppressSlider = false;

function headers(){ return { 'Content-Type':'application/x-www-form-urlencoded', 'X-Pin': pin }; }
async function post(path, body){
  const r = await fetch(path, { method:'POST', headers: headers(), body });
  if (r.status === 401){ alert('Wrong or missing PIN. Set it under Settings.'); }
  return r;
}

function render(s){
  $('conn').textContent = (s.wifiMode==='ap'?'Setup hotspot':'Connected') + ' - ' + s.ip;
  $('lockBanner').classList.toggle('on', !!s.lockout);

  const badge = $('modeBadge');
  badge.className = 'badge';
  if (s.lockout){ badge.textContent = 'LOCKED OFF'; badge.classList.add('lock'); }
  else if (s.mode==='remote'){ badge.textContent = 'Remote'; badge.classList.add('remote'); }
  else { badge.textContent = 'Knob - position ' + (s.knobPosition>=0? (s.knobPosition+1): '?'); }

  $('lvl').textContent = Math.round(s.setpointPct);
  if (!suppressSlider) $('slider').value = Math.round(s.setpointPct);
  $('amps').textContent = s.estimatedAmps.toFixed(1) + ' A';

  const out = $('outBtn');
  out.textContent = s.outputOn ? 'Output ON' : 'Output OFF';
  out.className = 'toggle ' + (s.outputOn ? 'on' : 'off');

  $('meta').innerHTML =
    'driver output: ' + s.outputPercent.toFixed(0) + '% &nbsp;|&nbsp; relay: ' + (s.relayClosed?'closed':'open') +
    '<br>current cap: ' + s.maxLevelPct.toFixed(0) + '% level';
}

// WebSocket for live state
let ws;
function connect(){
  ws = new WebSocket('ws://' + location.host + '/ws');
  ws.onmessage = e => { try { render(JSON.parse(e.data)); } catch(_){} };
  ws.onclose = () => setTimeout(connect, 1500);
}
connect();

// Fallback initial fetch
fetch('/api/state').then(r=>r.json()).then(render).catch(()=>{});

// Slider: commit on release (enters remote mode)
$('slider').addEventListener('input', ()=>{ suppressSlider = true; $('lvl').textContent = $('slider').value; });
$('slider').addEventListener('change', async ()=>{
  await post('/api/level', 'pct=' + $('slider').value);
  setTimeout(()=> suppressSlider = false, 400);
});

$('outBtn').addEventListener('click', async ()=>{
  const on = $('outBtn').classList.contains('on');
  await post('/api/output', 'on=' + (on ? '0' : '1'));
});
$('relBtn').addEventListener('click', ()=> post('/api/release', ''));

$('savePin').addEventListener('click', ()=>{
  pin = $('pin').value.trim();
  localStorage.setItem('luma_pin', pin);
  alert('PIN saved on this device.');
});
$('saveWifi').addEventListener('click', async ()=>{
  const r = await post('/api/wifi', 'ssid=' + encodeURIComponent($('ssid').value) +
                                    '&pass=' + encodeURIComponent($('wpass').value));
  if (r.ok) alert('Saved. Rebooting to join the network...');
});
</script>
</body>
</html>)HTML";

static const char kManifest[] PROGMEM = R"JSON({
  "name": "LumaCanopy",
  "short_name": "LumaCanopy",
  "display": "standalone",
  "background_color": "#0b0e14",
  "theme_color": "#0b0e14",
  "start_url": "/",
  "icons": []
})JSON";

} // namespace luma
