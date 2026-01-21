// Lightweight SSE manager that manages a single EventSource connection
// and supports subscribe/unsubscribe for named event targets.
(function(global){
  const DEFAULT_SSE_PORT = 9090;
  function sseBaseUrl() {
    const p = window.location.protocol;
    const h = window.location.hostname;
    return `${p}//${h}:${DEFAULT_SSE_PORT}/sse`;
  }

  function buildUrl(targets) {
    if (!targets || targets.size === 0) return sseBaseUrl();
    const q = Array.from(targets).map(encodeURIComponent).join(',');
    return sseBaseUrl() + '?targets=' + q;
  }

  const SSEManager = (function(){
    let es = null;
    let targets = new Set();
    let listeners = {}; // eventName -> [fn]
    let reconnectTimer = null;
    let backoffMs = 1000;
    let maxBackoff = 30000;

    function onOpen() {
      backoffMs = 1000;
      console.info('SSE connected');
    }
    function onError(e) {
      console.warn('SSE error, will reconnect', e);
      tryReconnect();
    }

    function attachListeners() {
      if (!es) return;
      // remove previous named event listeners? EventSource doesn't give list; we reconnect when changed
      Object.keys(listeners).forEach(evName => {
        if (evName === 'message') return;
        es.addEventListener(evName, function(ev){
          try {
            const obj = JSON.parse(ev.data);
            listeners[evName].forEach(fn => { try { fn(obj); } catch(e){ console.error(e); } });
          } catch(e) {
            // fallback: pass raw data
            listeners[evName].forEach(fn => { try { fn({ msg: ev.data }); } catch(e){ console.error(e); } });
          }
        });
      });
      es.onmessage = function(ev){
        try { const obj = JSON.parse(ev.data); (listeners['message']||[]).forEach(fn=>fn(obj)); }
        catch(e) { (listeners['message']||[]).forEach(fn=>fn({ msg: ev.data })); }
      };
      es.onopen = onOpen;
      es.onerror = onError;
    }

    function connect() {
      if (es) {
        try { es.close(); } catch(e){}
        es = null;
      }
      const url = buildUrl(targets);
      try {
        es = new EventSource(url);
        attachListeners();
      } catch(e) {
        console.warn('Failed to create EventSource', e);
        tryReconnect();
      }
    }

    function tryReconnect() {
      if (reconnectTimer) return;
      reconnectTimer = setTimeout(() => {
        reconnectTimer = null;
        backoffMs = Math.min(maxBackoff, backoffMs * 2);
        connect();
      }, backoffMs);
    }

    function subscribe(eventName, handler) {
      if (!listeners[eventName]) listeners[eventName] = [];
      listeners[eventName].push(handler);
      targets.add(eventName);
      // reconnect to apply new target set
      connect();
    }
    function unsubscribe(eventName, handler) {
      const arr = listeners[eventName];
      if (!arr) return;
      listeners[eventName] = arr.filter(fn => fn !== handler);
      if (!listeners[eventName] || listeners[eventName].length === 0) {
        delete listeners[eventName];
        targets.delete(eventName);
        connect();
      }
    }

    // Allow raw message handlers
    function onmessage(handler) {
      subscribe('message', handler);
    }

    // Start connection immediately
    connect();

    return { subscribe, unsubscribe, onmessage, _raw: () => es };
  })();

  global.SSEManager = SSEManager;
})(window);
