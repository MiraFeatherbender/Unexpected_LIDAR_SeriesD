// Lightweight SSE manager that manages a single EventSource connection
// and supports subscribe/unsubscribe for named event targets.
(function(global){
  function sseBaseUrl() {
    return `${window.location.origin}/sse`;
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
    let connectRequested = false;
    let statusListeners = [];

    function emitStatus(state) {
      statusListeners.forEach(fn => { try { fn(state); } catch(e){ console.error(e); } });
    }

    function onOpen() {
      backoffMs = 1000;
      console.info('SSE connected');
      emitStatus('open');
    }
    function onError(e) {
      console.warn('SSE error, will reconnect', e);
      emitStatus('error');
      if (connectRequested) tryReconnect();
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

    function doConnect() {
      if (es) {
        try { es.close(); } catch(e){}
        es = null;
      }
      const url = buildUrl(targets);
      emitStatus('connecting');
      try {
        es = new EventSource(url);
        attachListeners();
      } catch(e) {
        console.warn('Failed to create EventSource', e);
        if (connectRequested) tryReconnect();
      }
    }

    function tryReconnect() {
      if (!connectRequested) return;
      if (reconnectTimer) return;
      reconnectTimer = setTimeout(() => {
        reconnectTimer = null;
        backoffMs = Math.min(maxBackoff, backoffMs * 2);
        doConnect();
      }, backoffMs);
    }

    function connect() {
      connectRequested = true;
      doConnect();
    }

    function disconnect() {
      connectRequested = false;
      if (reconnectTimer) {
        clearTimeout(reconnectTimer);
        reconnectTimer = null;
      }
      if (es) {
        try { es.close(); } catch(e){}
        es = null;
      }
      emitStatus('closed');
    }

    function subscribe(eventName, handler) {
      if (!listeners[eventName]) listeners[eventName] = [];
      listeners[eventName].push(handler);
      targets.add(eventName);
      // reconnect to apply new target set if user requested a connection
      if (connectRequested) doConnect();
    }
    function unsubscribe(eventName, handler) {
      const arr = listeners[eventName];
      if (!arr) return;
      listeners[eventName] = arr.filter(fn => fn !== handler);
      if (!listeners[eventName] || listeners[eventName].length === 0) {
        delete listeners[eventName];
        targets.delete(eventName);
        if (connectRequested) doConnect();
      }
    }

    // Allow raw message handlers
    function onmessage(handler) {
      subscribe('message', handler);
    }

    function onStatusChange(handler) {
      statusListeners.push(handler);
    }

    function isConnected() {
      return !!(es && es.readyState === 1);
    }

    function isRequested() {
      return connectRequested;
    }

    return { subscribe, unsubscribe, onmessage, connect, disconnect, isConnected, isRequested, onStatusChange, _raw: () => es };
  })();

  global.SSEManager = SSEManager;
})(window);
