// Helpers for SSE payload decoding and bit conversions
(function(global){
  function b64ToUint8Array(b64) {
    try {
      const bin = atob(b64);
      const len = bin.length;
      const out = new Uint8Array(len);
      for (let i = 0; i < len; ++i) out[i] = bin.charCodeAt(i);
      return out;
    } catch (e) {
      console.error('b64ToUint8Array failed', e);
      return null;
    }
  }

  function bytesToBitRows(bytes, bitOrder='msb') {
    if (!bytes) return [];
    const rows = [];
    for (let i = 0; i < bytes.length; ++i) {
      const b = bytes[i];
      const row = new Array(8);
      for (let bit = 0; bit < 8; ++bit) {
        const idx = (bitOrder === 'msb') ? (7 - bit) : bit;
        row[bit] = (b >> idx) & 1;
      }
      rows.push(row);
    }
    return rows;
  }

  function bitRowsToString(rows, sep=' | ') {
    return rows.map(r => r.map(x => x ? '1' : '0').join('')).join(sep);
  }

  global.SSEHelpers = {
    b64ToUint8Array,
    bytesToBitRows,
    bitRowsToString
  };
})(window);