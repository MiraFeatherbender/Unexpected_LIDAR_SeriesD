// Console widget: subscribes to 'console' SSE events and renders them
(function(){
  const OUTPUT_ID = 'console-output';
  const MAX_LINES = 2000;
  let paused = false;
  let autoscroll = true;
  let buffer = [];
  let lines = [];

  function formatTime(iso) {
    try { const d = new Date(iso); return d.toLocaleTimeString(); } catch(e) { return new Date().toLocaleTimeString(); }
  }

  function addConsoleLine(obj) {
    const out = document.getElementById(OUTPUT_ID);
    if (!out) return;
    const el = document.createElement('div');
    el.className = 'console-line';
    const ts = document.createElement('span');
    ts.className = 'ts';
    ts.textContent = formatTime(obj.time || new Date().toISOString());
    const msg = document.createElement('span');
    msg.className = 'msg';
    msg.textContent = obj.msg || (typeof obj === 'string' ? obj : JSON.stringify(obj));
    el.appendChild(ts);
    el.appendChild(msg);
    if (obj.level === 'error') el.classList.add('error');
    else if (obj.level === 'warn') el.classList.add('warn');

    lines.push({time: ts.textContent, level: obj.level, msg: msg.textContent});
    if (lines.length > MAX_LINES) lines.shift();

    if (paused) {
      buffer.push(el);
      if (buffer.length > 500) buffer.shift();
      return;
    }
    out.appendChild(el);
    if (autoscroll) out.scrollTop = out.scrollHeight;
  }

  function clearConsole() {
    const out = document.getElementById(OUTPUT_ID);
    if (out) out.innerHTML = '';
    buffer = [];
    lines = [];
  }

  function togglePause() {
    paused = !paused;
    const btn = document.getElementById('console-pause-btn');
    if (btn) btn.textContent = paused ? 'Resume' : 'Pause';
    if (!paused && buffer.length > 0) {
      const out = document.getElementById(OUTPUT_ID);
      buffer.forEach(e => out.appendChild(e));
      buffer = [];
      if (autoscroll) out.scrollTop = out.scrollHeight;
    }
  }

  function toggleAutoscroll() {
    autoscroll = !autoscroll;
    const cb = document.getElementById('console-autoscroll-cb');
    if (cb) cb.checked = autoscroll;
  }

  function downloadLog() {
    const txt = lines.map(l => `${l.time} ${l.level || 'info'}: ${l.msg}`).join('\n');
    const blob = new Blob([txt], {type: 'text/plain'});
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = 'console_log.txt';
    document.body.appendChild(a);
    a.click();
    a.remove();
    URL.revokeObjectURL(url);
  }

  function init() {
    const clearBtn = document.getElementById('console-clear-btn');
    const pauseBtn = document.getElementById('console-pause-btn');
    const autoCb = document.getElementById('console-autoscroll-cb');
    const dlBtn = document.getElementById('console-download-btn');
    if (clearBtn) clearBtn.addEventListener('click', clearConsole);
    if (pauseBtn) pauseBtn.addEventListener('click', togglePause);
    if (autoCb) autoCb.addEventListener('change', function(){ autoscroll = autoCb.checked; });
    if (dlBtn) dlBtn.addEventListener('click', downloadLog);

    // Subscribe to 'console'
    if (typeof SSEManager !== 'undefined') {
      SSEManager.subscribe('console', function(obj) {
        addConsoleLine(obj);
      });
    }

    // Example: show that widget was initialized
    addConsoleLine({time: new Date().toISOString(), level: 'info', msg: 'Console widget initialized.'});
  }

  document.addEventListener('DOMContentLoaded', init);
})();
