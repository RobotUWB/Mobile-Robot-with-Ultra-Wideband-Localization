#ifndef WEBPAGE_H
#define WEBPAGE_H

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1, maximum-scale=1, user-scalable=no">
  <title>Robot Control</title>
  <style>
    body { 
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; 
      background-color: #f4f4f9; color: #333; 
      text-align: center; margin: 0; padding: 0; 
      height: 100vh; display: flex; flex-direction: column; 
      justify-content: center; align-items: center; user-select: none; 
    }
    h2 { margin: 10px 0; font-weight: 600; color: #444; }
    p { font-size: 12px; color: #666; margin-bottom: 20px; }
    .control-panel { 
      background: white; padding: 25px; border-radius: 20px; 
      box-shadow: 0 4px 15px rgba(0,0,0,0.05); border: 1px solid #eee;
    }
    .btn { 
      width: 60px; height: 60px; background-color: #e0e0e0; 
      border: none; border-radius: 12px; color: #555; 
      font-size: 24px; margin: 4px; cursor: pointer; 
      display: inline-flex; justify-content: center; align-items: center;
      transition: transform 0.1s; -webkit-tap-highlight-color: transparent;
    }
    .btn:active, .btn.active { background-color: #bbb; transform: scale(0.95); }
    .btn-spin { background-color: #d4e6f1; color: #2980b9; }
    .btn-spin:active, .btn-spin.active { background-color: #a9cce3; }
    .btn-move { background-color: #d5f5e3; color: #27ae60; }
    .btn-move:active, .btn-move.active { background-color: #abebc6; }
    .btn-stop { background-color: #fadbd8; color: #c0392b; font-weight: bold; font-size: 14px; }
    .btn-stop:active, .btn-stop.active { background-color: #f5b7b1; }
    .row { display: flex; justify-content: center; }
    .console-box {
      margin-top: 20px; width: 90%; max-width: 320px; height: 100px;
      background-color: #fff; border: 1px solid #ddd; border-radius: 8px;
      padding: 10px; text-align: left; font-family: monospace; 
      font-size: 11px; color: #555; overflow-y: auto;
    }
    .log-line { border-bottom: 1px solid #f9f9f9; padding: 2px 0; }
  </style>
</head>
<body>
  <div class="control-panel">
    <h2>Robot Controller</h2>
    <p>Status: Ready | Clean Code</p>
    <div class="row">
      <button id="btn-q" class="btn btn-spin" ontouchstart="move('Q')" onmousedown="move('Q')">&#8634;</button>
      <button id="btn-w" class="btn btn-move" ontouchstart="move('F')" onmousedown="move('F')">&#9650;</button>
      <button id="btn-e" class="btn btn-spin" ontouchstart="move('E')" onmousedown="move('E')">&#8635;</button>
    </div>
    <div class="row">
      <button id="btn-a" class="btn" ontouchstart="move('L')" onmousedown="move('L')">&#9664;</button>
      <button id="btn-s" class="btn btn-stop" ontouchstart="move('S')" onmousedown="move('S')">STOP</button>
      <button id="btn-d" class="btn" ontouchstart="move('R')" onmousedown="move('R')">&#9654;</button>
    </div>
    <div class="row">
      <button id="btn-b" class="btn btn-move" ontouchstart="move('B')" onmousedown="move('B')">&#9660;</button>
    </div>
  </div>
  <div id="console" class="console-box"><div class="log-line">> System initialized...</div></div>
  <script>
    function move(dir) {
      if(navigator.vibrate) navigator.vibrate(15);
      var x = new XMLHttpRequest();
      x.open("GET", "/action?go=" + dir, true);
      x.onload = function() { if (x.status === 200) logToScreen(x.responseText); };
      x.send();
    }
    function logToScreen(msg) {
      var consoleBox = document.getElementById("console");
      var time = new Date().toLocaleTimeString('en-GB', { hour12: false });
      consoleBox.innerHTML = '<div class="log-line">[' + time + '] ' + msg + '</div>' + consoleBox.innerHTML;
    }
    document.addEventListener('keydown', function(e) {
      if (e.repeat) return;
      var k = e.key.toLowerCase(), c = "", id = "";
      if (k === 'w') { c='F'; id='btn-w'; } else if (k === 'a') { c='L'; id='btn-a'; }
      else if (k === 's') { c='B'; id='btn-b'; } else if (k === 'd') { c='R'; id='btn-d'; }
      else if (k === 'q') { c='Q'; id='btn-q'; } else if (k === 'e') { c='E'; id='btn-e'; }
      else if (k === ' ') { c='S'; id='btn-s'; }
      if (c) { move(c); document.getElementById(id)?.classList.add('active'); }
    });
    document.addEventListener('keyup', function(e) {
      if(['q','w','e','a','s','d',' '].includes(e.key.toLowerCase())) 
        document.querySelectorAll('.btn').forEach(b => b.classList.remove('active'));
    });
    document.addEventListener('contextmenu', e => e.preventDefault());
  </script>
</body>
</html>
)rawliteral";

#endif