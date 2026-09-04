#pragma once

#include <Arduino.h>

// Single-page web app, embedded uncompressed in PROGMEM. Kept in one file so
// the UI can never drift out of sync with the firmware. Served at "/".
//
// (The plan mentioned gzipping this blob; it is stored uncompressed here to
// avoid a build-time asset step. It is small enough to serve directly.)
//
// The dial mirrors the physical 8-position switch. Two states are always shown
// at once and they are not the same thing: the green fill is the program that
// is actually running, the blue ring is where the physical knob is sitting.
// They diverge whenever a program is selected remotely -- and turning the real
// knob always wins.

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
  [hidden] { display: none !important; }
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
  .unit { font-size:15px; color:#8b96a5; font-weight:500; }
  input[type=range]{ width:100%; height:38px; }
  button { font:inherit; border:none; border-radius:12px; padding:12px 16px; cursor:pointer; }
  .toggle { width:100%; font-size:16px; font-weight:600; }
  .on { background:#1f7a3d; color:#fff; }
  .off { background:#2a2f3a; color:#cfd6e0; }
  .ghost { background:transparent; color:#9fb2cc; border:1px solid #2b3a52; width:100%; }
  .ghost.danger { color:#ffb4b4; border-color:#7a2b2b; }
  .meta { color:#8b96a5; font-size:12px; margin-top:14px; line-height:1.6; }
  a { color:#7fa8ff; }
  .k { color:#8b96a5; }
  label { font-size:13px; color:#9fb2cc; display:block; margin:10px 0 4px; }
  input[type=text],input[type=password],select{ width:100%; padding:10px; border-radius:10px;
     border:1px solid #2b3a52; background:#0e1420; color:#e8ecf1; font:inherit; }
  details { margin-top:6px; }
  summary { cursor:pointer; color:#9fb2cc; font-size:13px; }

  /* --- 8-position dial --- */
  .dial { position:relative; width:100%; max-width:290px; aspect-ratio:1; margin:10px auto 2px; }
  .pos { position:absolute; width:56px; height:56px; margin:-28px 0 0 -28px; padding:0;
         border-radius:50%; background:#1d2636; color:#9fb2cc; border:1px solid #2b3a52;
         display:flex; flex-direction:column; align-items:center; justify-content:center;
         font-size:10px; line-height:1.2; }
  .pos b { font-size:15px; color:#e8ecf1; font-weight:600; }
  .pos.active { background:#1f7a3d; border-color:#3aa862; }
  .pos.active b, .pos.active { color:#fff; }
  .pos.knob { box-shadow:0 0 0 2px #7fa8ff; }
  .pos.koff { opacity:.5; }
  .hub { position:absolute; left:50%; top:50%; transform:translate(-50%,-50%);
         text-align:center; pointer-events:none; }
  .hub .big { font-size:38px; font-weight:700; line-height:1; }
  .hub .hubsub { font-size:11px; color:#8b96a5; margin-top:4px; }
  .legend { display:flex; gap:16px; justify-content:center; font-size:11px; color:#8b96a5; margin-top:8px; }
  .dot { display:inline-block; width:9px; height:9px; border-radius:50%; margin-right:5px; vertical-align:middle; }
  .dot.g { background:#1f7a3d; }
  .dot.b { background:transparent; box-shadow:0 0 0 2px #7fa8ff; }
  .editing .pos { border-style:dashed; }
  .note { font-size:12px; color:#8b96a5; margin-top:8px; line-height:1.5; }
  .warn { color:#e8c07f; }
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

    <div class="dial" id="dial">
      <div class="hub">
        <div class="big"><span id="lvl">0</span><span class="unit">%</span></div>
        <div class="hubsub" id="hubsub"></div>
      </div>
    </div>
    <div class="legend">
      <span><i class="dot g"></i>running</span>
      <span><i class="dot b"></i>physical knob</span>
    </div>

    <label>Free level (overrides the program)</label>
    <input type="range" id="slider" min="0" max="100" value="0">
  </div>

  <div class="card">
    <button class="toggle off" id="outBtn">Output OFF</button>
    <div style="height:10px"></div>
    <button class="ghost" id="relBtn">Return control to knob</button>
    <div style="height:10px"></div>
    <button class="ghost" id="editBtn">Edit programs</button>
  </div>

  <div class="card" id="editor" hidden>
    <div class="row">
      <b>Position <span id="edN">1</span></b>
      <button class="ghost" id="edClose" style="width:auto;padding:6px 12px">close</button>
    </div>

    <label>Program</label>
    <select id="edKind">
      <option value="steady">Steady - hold one level</option>
      <option value="blink">Blink - square pulse</option>
      <option value="breathe">Breathe - smooth fade</option>
      <option value="off">Off - output open at this detent</option>
    </select>

    <div id="fLevel">
      <label>Level <span id="edHighV">50</span>%</label>
      <input type="range" id="edHigh" min="0" max="100" value="50">
    </div>

    <div id="fDyn" hidden>
      <label>Low level <span id="edLowV">0</span>%</label>
      <input type="range" id="edLow" min="0" max="100" value="0">
      <label>On time <span id="edOnV">500</span> ms</label>
      <input type="range" id="edOn" min="120" max="5000" step="10" value="500">
      <label>Off time <span id="edOffV">500</span> ms</label>
      <input type="range" id="edOff" min="120" max="5000" step="10" value="500">
      <div class="note" id="rateNote"></div>
    </div>

    <div style="height:12px"></div>
    <button class="ghost" id="edSave">Save to this position</button>
    <div class="note" id="edNote"></div>
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
      <hr style="border-color:#222b3a; margin:16px 0">
      <button class="ghost danger" id="resetSlots">Reset all 8 programs to defaults</button>
    </details>
    <div class="meta" id="meta"></div>
  </div>
</div>

<script>
const $ = id => document.getElementById(id);
let pin = localStorage.getItem('luma_pin') || '';
$('pin').value = pin;
let suppressSlider = false;
let editMode = false;
let editIndex = -1;
let slots = [];
let maxBlinkHz = 2;

function headers(){ return { 'Content-Type':'application/x-www-form-urlencoded', 'X-Pin': pin }; }
async function post(path, body){
  const r = await fetch(path, { method:'POST', headers: headers(), body });
  if (r.status === 401){ alert('Wrong or missing PIN. Set it under Settings.'); }
  return r;
}

// --- Dial construction: 8 buttons on a circle, position 1 at the top ---
const dial = $('dial');
for (let i = 0; i < 8; i++){
  const b = document.createElement('button');
  b.className = 'pos';
  b.dataset.i = i;
  const a = (-90 + i * 45) * Math.PI / 180;
  b.style.left = (50 + 38 * Math.cos(a)) + '%';
  b.style.top  = (50 + 38 * Math.sin(a)) + '%';
  b.innerHTML = '<b>' + (i+1) + '</b><span class="pl"></span>';
  b.addEventListener('click', () => editMode ? openEditor(i) : post('/api/slot', 'i=' + i));
  dial.appendChild(b);
}

function slotSummary(s){
  if (!s) return '';
  if (s.kind === 'off') return 'off';
  if (s.kind === 'steady') return Math.round(s.highPct) + '%';
  const hz = 1000 / (s.onMs + s.offMs);
  return (s.kind === 'blink' ? 'blink ' : 'fade ') + hz.toFixed(1) + 'Hz';
}

function paintSlots(){
  document.querySelectorAll('.pos').forEach(el => {
    const i = +el.dataset.i;
    const s = slots[i];
    el.querySelector('.pl').textContent = slotSummary(s);
    el.classList.toggle('koff', !!s && s.kind === 'off');
  });
}

async function loadSlots(){
  try {
    const r = await fetch('/api/slots');
    slots = await r.json();
    paintSlots();
  } catch(e){}
}

function render(s){
  $('conn').textContent = (s.wifiMode==='ap'?'Setup hotspot':'Connected') + ' - ' + s.ip;
  $('lockBanner').classList.toggle('on', !!s.lockout);
  if (s.maxBlinkHz) maxBlinkHz = s.maxBlinkHz;

  const badge = $('modeBadge');
  badge.className = 'badge';
  if (s.lockout){ badge.textContent = 'LOCKED OFF'; badge.classList.add('lock'); }
  else if (s.mode==='remote'){ badge.textContent = 'Remote'; badge.classList.add('remote'); }
  else { badge.textContent = 'Knob - position ' + (s.knobPosition>=0? (s.knobPosition+1): '?'); }

  // Live level: show what the effect is commanding right now while the output
  // is on, otherwise the nominal setpoint.
  const live = (s.outputOn && s.relayClosed) ? s.instantPct : s.setpointPct;
  $('lvl').textContent = Math.round(live);
  if (!suppressSlider) $('slider').value = Math.round(s.setpointPct);
  $('amps').textContent = s.estimatedAmps.toFixed(1) + ' A';

  // The running program and the physical knob are shown separately: they only
  // coincide in Master mode.
  document.querySelectorAll('.pos').forEach(el => {
    const i = +el.dataset.i;
    el.classList.toggle('active', i === s.activeSlot && !s.lockout);
    el.classList.toggle('knob', i === s.knobPosition);
  });

  let sub = '';
  if (s.activeSlot >= 0){
    sub = 'position ' + (s.activeSlot+1) + ' - ' + s.effect;
    if (s.mode === 'remote' && s.activeSlot !== s.knobPosition){
      sub += ' (knob at ' + (s.knobPosition>=0 ? s.knobPosition+1 : '?') + ')';
    }
  } else {
    sub = 'free level';
  }
  $('hubsub').textContent = sub;

  // A position programmed Off holds the output open, so the ON button cannot
  // win against it. Say that rather than looking broken.
  const out = $('outBtn');
  const forcedOff = (s.effect === 'off' && s.activeSlot >= 0 && !s.lockout);
  out.disabled = forcedOff || !!s.lockout;
  out.style.opacity = out.disabled ? '.55' : '1';
  if (forcedOff){
    out.textContent = 'Position ' + (s.activeSlot+1) + ' is programmed OFF';
    out.className = 'toggle off';
  } else {
    out.textContent = s.outputOn ? 'Output ON' : 'Output OFF';
    out.className = 'toggle ' + (s.outputOn ? 'on' : 'off');
  }

  $('meta').innerHTML =
    'output: ' + s.outputPercent.toFixed(0) + '% &nbsp;|&nbsp; relay: ' + (s.relayClosed?'closed':'open') +
    '<br>load cap: ' + (s.maxLoadAmps||0).toFixed(1) + ' A at ' + (s.maxDimVolts||0).toFixed(2) + ' V DIM' +
    '<br>effects run on the dimmer only - the relay never cycles for a program';
}

function poll(){
  fetch('/api/state').then(r=>r.json()).then(render).catch(()=>{});
}
loadSlots();
poll();
setInterval(poll, 500);

// --- Editor ---
function openEditor(i){
  editIndex = i;
  const s = slots[i] || { kind:'steady', highPct:50, lowPct:0, onMs:500, offMs:500 };
  $('edN').textContent = (i+1);
  $('edKind').value = s.kind;
  $('edHigh').value = Math.round(s.highPct);
  $('edLow').value = Math.round(s.lowPct);
  $('edOn').value = s.onMs;
  $('edOff').value = s.offMs;
  $('edNote').textContent = '';
  syncEditor();
  $('editor').hidden = false;
  $('editor').scrollIntoView({ behavior:'smooth', block:'nearest' });
}

function syncEditor(){
  const kind = $('edKind').value;
  const dyn = (kind === 'blink' || kind === 'breathe');
  $('fDyn').hidden = !dyn;
  $('fLevel').hidden = (kind === 'off');
  $('edHighV').textContent = $('edHigh').value;
  $('edLowV').textContent = $('edLow').value;
  $('edOnV').textContent = $('edOn').value;
  $('edOffV').textContent = $('edOff').value;

  if (dyn){
    const period = (+$('edOn').value) + (+$('edOff').value);
    const hz = 1000 / period;
    const capped = hz > maxBlinkHz;
    $('rateNote').innerHTML = 'Rate: ' + hz.toFixed(2) + ' Hz.' +
      (capped ? ' <span class="warn">Above the ' + maxBlinkHz + ' Hz cap - the off time will be stretched on save.</span>'
              : '') +
      '<br>The low level is a dip, not darkness: 0 V DIM still leaves roughly 10% driver current.';
  }
}
['edKind','edHigh','edLow','edOn','edOff'].forEach(id =>
  $(id).addEventListener('input', syncEditor));

$('edClose').addEventListener('click', ()=>{ $('editor').hidden = true; });

$('edSave').addEventListener('click', async ()=>{
  if (editIndex < 0) return;
  const body = 'i=' + editIndex +
    '&kind=' + $('edKind').value +
    '&high=' + $('edHigh').value +
    '&low=' + $('edLow').value +
    '&on=' + $('edOn').value +
    '&off=' + $('edOff').value;
  const r = await post('/api/slots', body);
  if (!r.ok) return;
  // The firmware clamps; re-read from its reply so the UI shows what was
  // actually stored rather than what was asked for.
  const stored = await r.json();
  slots[editIndex] = stored;
  paintSlots();
  $('edOn').value = stored.onMs; $('edOff').value = stored.offMs;
  $('edLow').value = Math.round(stored.lowPct);
  $('edHigh').value = Math.round(stored.highPct);
  syncEditor();
  $('edNote').textContent = 'Saved to position ' + (editIndex+1) + '.';
});

$('editBtn').addEventListener('click', ()=>{
  editMode = !editMode;
  $('editBtn').textContent = editMode ? 'Done editing' : 'Edit programs';
  dial.classList.toggle('editing', editMode);
  if (!editMode) $('editor').hidden = true;
});

$('resetSlots').addEventListener('click', async ()=>{
  if (!confirm('Reset all 8 programs to factory defaults?')) return;
  const r = await post('/api/slots/reset', '');
  if (r.ok){ await loadSlots(); $('editor').hidden = true; }
});

// Slider: commit on release (enters remote mode, free level)
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
