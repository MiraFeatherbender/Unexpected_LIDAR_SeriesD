(function(){
  const PANEL_ID = 'line-sensor-grid';
  const MAX_ROWS = 32; // safety limit for UI

  function renderBytes(bytes, bitOrder='msb') {
    const container = document.getElementById(PANEL_ID);
    if (!container) return;
    // Clear
    container.innerHTML = '';
    if (!bytes || bytes.length === 0) return;
    const rows = SSEHelpers.bytesToBitRows(bytes, bitOrder);
    // Clamp rows
    const rowCount = Math.min(rows.length, MAX_ROWS);
    for (let i = 0; i < rowCount; ++i) {
      const row = rows[i];
      const rowEl = document.createElement('div');
      rowEl.className = 'bit-row';
      // cells
      for (let b = 0; b < 8; ++b) {
        const c = document.createElement('div');
        c.className = 'bit-cell' + (row[b] ? ' on' : '');
        const dot = document.createElement('div');
        dot.className = 'dot';
        dot.textContent = row[b] ? '1' : '';
        c.appendChild(dot);
        rowEl.appendChild(c);
      }
      container.appendChild(rowEl);
    }
  }

  let lastBytes = null;
  let currentBitOrder = 'msb';

  function onEvent(obj) {
    try {
      if (!obj) return;
      if (obj.schema === 'line_sensor.v1' && obj.data_b64) {
        const bytes = SSEHelpers.b64ToUint8Array(obj.data_b64);
        if (bytes) {
          lastBytes = bytes;
          renderBytes(bytes, currentBitOrder);
        }
      } else if (obj.data) {
        // fallback: parse hex string like "A7 3B DF"
        const hexStr = (typeof obj.data === 'string') ? obj.data.replace(/\s+/g,'') : '';
        if (hexStr.length >= 2) {
          const ba = new Uint8Array(Math.floor(hexStr.length/2));
          for (let i=0;i<ba.length;i++) ba[i]=parseInt(hexStr.substr(i*2,2),16);
          lastBytes = ba;
          renderBytes(ba, currentBitOrder);
        }
      }
    } catch (e) {
      console.error('line_sensor panel error', e);
    }
  }

  function init() {
    // restore saved bit order preference
    try {
      const saved = localStorage.getItem('lineSensorBitOrder');
      if (saved === 'msb' || saved === 'lsb') currentBitOrder = saved;
    } catch (e) { /* ignore */ }

    const select = document.getElementById('line-sensor-bitorder');
    if (select) {
      select.value = currentBitOrder;
      select.addEventListener('change', function(ev) {
        currentBitOrder = select.value === 'lsb' ? 'lsb' : 'msb';
        try { localStorage.setItem('lineSensorBitOrder', currentBitOrder); } catch(e){}
        if (lastBytes) renderBytes(lastBytes, currentBitOrder);
      });
    }

    if (typeof SSEManager !== 'undefined') {
      SSEManager.subscribe('line_sensor', onEvent);
    }
    // show default empty grid
    const container = document.getElementById(PANEL_ID);
    if (container) container.textContent = 'Waiting for data...';
  }

  document.addEventListener('DOMContentLoaded', init);
})();