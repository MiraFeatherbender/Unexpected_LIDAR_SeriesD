// HSV Curve Editor Module
// Usage: HSVEditor.init('#container')
// Assumes Chart.js and iro.js are loaded globally

const HSVEditor = (function() {
  let firePreset = [
    { h: 0, s: 100, v: 30 },
    { h: 0, s: 100, v: 55 },
    { h: 0, s: 100, v: 70 },
    { h: 0, s: 100, v: 80 },
    { h: 0, s: 100, v: 75 },
    { h: 0, s: 100, v: 40 }
  ];
  let palettePresets = {};
  let paletteNames = [];
  let chart, iroPicker;

  function getChannelMinMax(channel) {
    let arr = firePreset.map(stop => channel === 'h' ? stop.h : channel === 's' ? stop.s : stop.v);
    return {min: Math.min(...arr), max: Math.max(...arr)};
  }
  function getChannelMaxScale(channel) {
    return channel === 'h' ? 360 : 100;
  }

  function drawInterpolatedBar(canvas) {
    const ctx = canvas.getContext('2d');
    const stops = firePreset.map(stop => ({ h: stop.h, s: stop.s, v: stop.v }));
    const nStops = stops.length;
    ctx.clearRect(0, 0, canvas.width, canvas.height);
    for (let i = 0; i < canvas.width; i+=(canvas.width/256)) {
      let t = i / (canvas.width - 1) * (nStops - 1);
      let idx = Math.floor(t);
      let frac = t - idx;
      let a = stops[idx];
      let b = stops[Math.min(idx + 1, nStops - 1)];
      let dh = ((b.h - a.h + 540) % 360) - 180;
      let h = (a.h + frac * dh + 360) % 360;
      let s = a.s + frac * (b.s - a.s);
      let v = a.v + frac * (b.v - a.v);
      let rgb = hsvToRgb(h, s, v);
      ctx.fillStyle = `rgb(${rgb.r},${rgb.g},${rgb.b})`;
      ctx.fillRect(i, 0, canvas.width/256, canvas.height);
    }
  }

  function hsvToRgb(h, s, v) {
    s /= 100; v /= 100;
    let c = v * s;
    let x = c * (1 - Math.abs(((h / 60) % 2) - 1));
    let m = v - c;
    let r1, g1, b1;
    if (h < 60)      { r1 = c; g1 = x; b1 = 0; }
    else if (h < 120){ r1 = x; g1 = c; b1 = 0; }
    else if (h < 180){ r1 = 0; g1 = c; b1 = x; }
    else if (h < 240){ r1 = 0; g1 = x; b1 = c; }
    else if (h < 300){ r1 = x; g1 = 0; b1 = c; }
    else             { r1 = c; g1 = 0; b1 = x; }
    return {
      r: Math.round((r1 + m) * 255),
      g: Math.round((g1 + m) * 255),
      b: Math.round((b1 + m) * 255)
    };
  }

  function getHSVChartDatasets(active) {
    return [
      {
        label: 'Hue',
        data: firePreset.map(stop => stop.h),
        borderColor: '#e74c3c',
        backgroundColor: '#e74c3c',
        pointRadius: 8,
        pointHoverRadius: 12,
        fill: false,
        tension: 0.2,
        yAxisID: 'y-h',
        borderWidth: 3,
        opacity: (active === 'h') ? 1.0 : 0.3,
        pointBackgroundColor: (active === 'h') ? '#e74c3c' : 'rgba(231,76,60,0.3)',
        pointBorderColor: (active === 'h') ? '#e74c3c' : 'rgba(231,76,60,0.3)',
        borderDash: (active === 'h') ? [] : [4,4],
        order: 1
      },
      {
        label: 'Saturation',
        data: firePreset.map(stop => stop.s),
        borderColor: '#3498db',
        backgroundColor: '#3498db',
        pointRadius: 8,
        pointHoverRadius: 12,
        fill: false,
        tension: 0.2,
        yAxisID: 'y-sv',
        borderWidth: 3,
        opacity: (active === 's') ? 1.0 : 0.3,
        pointBackgroundColor: (active === 's') ? '#3498db' : 'rgba(52,152,219,0.3)',
        pointBorderColor: (active === 's') ? '#3498db' : 'rgba(52,152,219,0.3)',
        borderDash: (active === 's') ? [] : [4,4],
        order: 2
      },
      {
        label: 'Value',
        data: firePreset.map(stop => stop.v),
        borderColor: '#2ecc40',
        backgroundColor: '#2ecc40',
        pointRadius: 8,
        pointHoverRadius: 12,
        fill: false,
        tension: 0.2,
        yAxisID: 'y-sv',
        borderWidth: 3,
        opacity: (active === 'v') ? 1.0 : 0.3,
        pointBackgroundColor: (active === 'v') ? '#2ecc40' : 'rgba(46,204,64,0.3)',
        pointBorderColor: (active === 'v') ? '#2ecc40' : 'rgba(46,204,64,0.3)',
        borderDash: (active === 'v') ? [] : [4,4],
        order: 3
      }
    ];
  }

  function setActiveChannel(ch) {
    activeChannel = ch;
    channelButtons.h.style.opacity = (ch === 'h') ? '1.0' : '0.5';
    channelButtons.s.style.opacity = (ch === 's') ? '1.0' : '0.5';
    channelButtons.v.style.opacity = (ch === 'v') ? '1.0' : '0.5';
    const newDatasets = getHSVChartDatasets(ch);
    chart.data.datasets.forEach((ds, i) => {
      ds.data = newDatasets[i].data;
      ds.label = newDatasets[i].label;
      ds.borderColor = newDatasets[i].borderColor;
      ds.backgroundColor = newDatasets[i].backgroundColor;
      ds.pointRadius = newDatasets[i].pointRadius;
      ds.pointHoverRadius = newDatasets[i].pointHoverRadius;
      ds.fill = newDatasets[i].fill;
      ds.tension = newDatasets[i].tension;
      ds.yAxisID = newDatasets[i].yAxisID;
      ds.borderWidth = newDatasets[i].borderWidth;
      ds.opacity = newDatasets[i].opacity;
      ds.pointBackgroundColor = newDatasets[i].pointBackgroundColor;
      ds.pointBorderColor = newDatasets[i].pointBorderColor;
      ds.borderDash = newDatasets[i].borderDash;
      ds.order = newDatasets[i].order;
    });
    chart.update({animation: false});
  }

  let activeChannel = 'h';
  let channelButtons;

  function updateHSV(hsvValuesDiv, interpolatedBar) {
    var selectedColor = iroPicker.color;
    var hsv = selectedColor.hsv;
    var h8 = Math.round(hsv.h * 255 / 360);
    var s8 = Math.round(hsv.s * 255 / 100);
    var v8 = Math.round(hsv.v * 255 / 100);
    // hsvValuesDiv.innerHTML =
    //   `<span style='color:#555'>8-bit: <b>H:</b> ${h8} <b>S:</b> ${s8} <b>V:</b> ${v8}</span>`;
    drawInterpolatedBar(interpolatedBar);
    if (typeof setActiveChannel === 'function' && typeof activeChannel !== 'undefined') {
      setActiveChannel(activeChannel);
    }
    if (typeof HSVEditor.onLUTChange === 'function') {
      HSVEditor.onLUTChange();
    }
  }

  function getLUT(n = 256) {
    // Returns an array of n RGB colors from the current interpolated bar
    let lut = [];
    for (let i = 0; i < n; i++) {
      let t = i / (n - 1) * (firePreset.length - 1);
      let idx = Math.floor(t);
      let frac = t - idx;
      let a = firePreset[idx];
      let b = firePreset[Math.min(idx + 1, firePreset.length - 1)];
      let dh = ((b.h - a.h + 540) % 360) - 180;
      let h = (a.h + frac * dh + 360) % 360;
      let s = a.s + frac * (b.s - a.s);
      let v = a.v + frac * (b.v - a.v);
      lut.push(hsvToRgb(h, s, v));
    }
    return lut;
  }

  return {
    init: function(containerSelector) {
      const container = document.querySelector(containerSelector);
      if (!container) throw new Error('HSVEditor: container not found');
      container.innerHTML = `
        <div style="width: 780px; height: 400px; background: #181818; border-radius: 8px; border: 1px solid #181818; margin-bottom: 1em; display: flex; flex-direction: column; align-items: center; justify-content: flex-start;">
          <div style="display: flex; flex-direction: row; align-items: center; gap: 8px;">
            <select id="palette-dropdown" style="font-size:1em; padding:4px 12px; border-radius:4px;"></select>
            <button id="save-preset-btn" style="font-size:1.1em; padding:4px 16px; background:#2a2a2a; color:#fff; border:none; border-radius:4px; cursor:pointer;">Save Color Palette</button>
          </div>
          <div style="width: 100%; display: flex; flex-direction: row; align-items: center; justify-content: center; margin-top: 16px; margin-bottom: 8px;">
            <canvas id="interpolated-bar" width="512" height="32" style="display:block; margin:1em 0; border-radius:6px; border:1px solid #ccc;"></canvas>
            <div id="hsv-channel-select" style="display: flex; flex-direction: row; align-items: center; margin-left: 24px;">
              <button type="button" id="btn-hue" style="margin-right:8px; font-weight:bold; background:#e74c3c; color:#fff; border:none; border-radius:4px; padding:6px 18px;">H</button>
              <button type="button" id="btn-sat" style="margin-right:8px; font-weight:bold; background:#3498db; color:#fff; border:none; border-radius:4px; padding:6px 18px; opacity:0.5;">S</button>
              <button type="button" id="btn-val" style="font-weight:bold; background:#2ecc40; color:#fff; border:none; border-radius:4px; padding:6px 18px; opacity:0.5;">V</button>
            </div>
          </div>
          <div style="width: 100%; height: 300px; display: flex; align-items: center; justify-content: flex-start;">
            <canvas id="hsv-curve-canvas" width="640" height="300" style="background: #222; margin-left: 16px;"></canvas>
            <div style="display: flex; flex-direction: row; align-items: center; margin-left: 8px; height: 240px; justify-content: center;"></div>
            <div id="irojs-interface" style="margin-left: 8px; display: flex; flex-direction: column; align-items: flex-start; justify-content: center; height: 240px; width: 256px; gap: 5px;"></div>
          </div>
        </div>
      `;

      fetch('color_palettes.json')
        .then(resp => resp.json())
        .then(data => {
          // Support both old and new formats
          palettePresets = data.presets ? data.presets : data;
          paletteNames = Object.keys(palettePresets);
          const dropdown = container.querySelector('#palette-dropdown');
          dropdown.innerHTML = '';
          paletteNames.forEach(name => {
            const opt = document.createElement('option');
            opt.value = name;
            opt.textContent = name;
            dropdown.appendChild(opt);
          });
          // Set initial preset to first
          if (paletteNames.length > 0) {
            setFirePresetFromName(paletteNames[0]);
            dropdown.value = paletteNames[0];
            // Ensure display updates to match loaded preset
            updateHSV(hsvValuesDiv, interpolatedBar);
            setActiveChannel(activeChannel);
          }
          dropdown.addEventListener('change', e => {
            setFirePresetFromName(e.target.value);
            updateHSV(hsvValuesDiv, interpolatedBar);
            setActiveChannel(activeChannel);
            updateAllIroBases();
            updateIroHueSliderBounds();
            updateIroSatSliderBounds();
            updateIroValSliderBounds();
          });
          // Save button logic: update in-memory JSON for selected preset
          const saveBtn = container.querySelector('#save-preset-btn');

          // Create save dialog elements
          const saveDialog = document.createElement('div');
          saveDialog.style.position = 'absolute';
          saveDialog.style.background = '#222';
          saveDialog.style.border = '1px solid #444';
          saveDialog.style.borderRadius = '8px';
          saveDialog.style.padding = '16px';
          saveDialog.style.zIndex = '1000';
          saveDialog.style.display = 'none';

          const saveDropdown = document.createElement('select');
          paletteNames.forEach(name => {
            const opt = document.createElement('option');
            opt.value = name;
            opt.textContent = name;
            saveDropdown.appendChild(opt);
          });
          const newOpt = document.createElement('option');
          newOpt.value = '__new__';
          newOpt.textContent = 'New...';
          saveDropdown.appendChild(newOpt);

          const newNameInput = document.createElement('input');
          newNameInput.type = 'text';
          newNameInput.placeholder = 'New preset name';
          newNameInput.style.display = 'none';
          newNameInput.style.marginLeft = '8px';

          const saveConfirmBtn = document.createElement('button');
          saveConfirmBtn.textContent = 'Confirm';
          saveConfirmBtn.style.marginLeft = '8px';
          saveConfirmBtn.style.padding = '4px 16px';
          saveConfirmBtn.style.background = '#2a2a2a';
          saveConfirmBtn.style.color = '#fff';
          saveConfirmBtn.style.border = 'none';
          saveConfirmBtn.style.borderRadius = '4px';
          saveConfirmBtn.style.cursor = 'pointer';
          saveConfirmBtn.style.fontSize = '1em';

          const saveCancelBtn = document.createElement('button');
          saveCancelBtn.textContent = 'Cancel';
          saveCancelBtn.style.marginLeft = '8px';
          saveCancelBtn.style.padding = '4px 16px';
          saveCancelBtn.style.background = '#444';
          saveCancelBtn.style.color = '#fff';
          saveCancelBtn.style.border = 'none';
          saveCancelBtn.style.borderRadius = '4px';
          saveCancelBtn.style.cursor = 'pointer';
          saveCancelBtn.style.fontSize = '1em';

          saveDialog.appendChild(document.createTextNode('Save as: '));
          saveDialog.appendChild(saveDropdown);
          saveDialog.appendChild(newNameInput);
          saveDialog.appendChild(saveConfirmBtn);
          saveDialog.appendChild(saveCancelBtn);
          container.appendChild(saveDialog);

          // Show/hide new name input
          saveDropdown.addEventListener('change', () => {
            newNameInput.style.display = (saveDropdown.value === '__new__') ? 'inline-block' : 'none';
          });

          // Save button click handler
          saveBtn.addEventListener('click', () => {
            saveDialog.style.display = 'block';
            saveDialog.style.top = (saveBtn.offsetTop + saveBtn.offsetHeight + 8) + 'px';
            saveDialog.style.left = saveBtn.offsetLeft + 'px';
          });

          // Cancel button handler
          saveCancelBtn.addEventListener('click', () => {
            saveDialog.style.display = 'none';
            newNameInput.value = '';
            saveDropdown.value = paletteNames[0];
            newNameInput.style.display = 'none';
          });

          // Confirm button handler
          saveConfirmBtn.addEventListener('click', () => {
            let name;
            if (saveDropdown.value === '__new__') {
              name = newNameInput.value.trim();
              if (!name) return;
              if (palettePresets[name]) {
                alert('Preset name already exists.');
                return;
              }
              paletteNames.push(name);
              const opt = document.createElement('option');
              opt.value = name;
              opt.textContent = name;
              saveDropdown.insertBefore(opt, newOpt);
              container.querySelector('#palette-dropdown').appendChild(opt.cloneNode(true));
            } else {
              name = saveDropdown.value;
            }
            palettePresets[name] = {
              hue: firePreset.map(stop => stop.h),
              sat: firePreset.map(stop => stop.s),
              val: firePreset.map(stop => stop.v)
            };
            uploadPalettesJSON(palettePresets);
            savePaletteRowPNG(name, container.querySelector('#interpolated-bar'));
            saveDialog.style.display = 'none';
            newNameInput.value = '';
            saveDropdown.value = paletteNames[0];
            newNameInput.style.display = 'none';
          });
        })
        .catch(err => {
          console.error('Failed to load color_palettes.json:', err);
        });

      let suppressIroChange = false;

      function setFirePresetFromName(name) {
        const preset = palettePresets[name];
        if (!preset) return;
        firePreset.length = 0;
        for (let i = 0; i < preset.hue.length; ++i) {
          firePreset.push({
            h: preset.hue[i],
            s: preset.sat[i],
            v: preset.val[i]
          });
        }
        // Set iro.js color picker to average of preset, suppressing color:change
        if (iroPicker && preset.hue && preset.sat && preset.val) {
          const avg = arr => arr.reduce((a, b) => a + b, 0) / arr.length;
          suppressIroChange = true;
          iroPicker.color.hsv = {
            h: avg(preset.hue),
            s: avg(preset.sat),
            v: avg(preset.val)
          };
          setTimeout(() => { suppressIroChange = false; }, 0); // allow event loop to finish
        }
      }
      const hsvValuesDiv = container.querySelector('#hsv-values');
      const interpolatedBar = container.querySelector('#interpolated-bar');
      const irojsInterface = container.querySelector('#irojs-interface');
      const curveCanvas = container.querySelector('#hsv-curve-canvas');
      channelButtons = {
        h: container.querySelector('#btn-hue'),
        s: container.querySelector('#btn-sat'),
        v: container.querySelector('#btn-val')
      };
      channelButtons.h.addEventListener('click', () => setActiveChannel('h'));
      channelButtons.s.addEventListener('click', () => setActiveChannel('s'));
      channelButtons.v.addEventListener('click', () => setActiveChannel('v'));

      // Setup iro.js color picker
      iroPicker = new iro.ColorPicker(irojsInterface, {
        width: 300,
        color: { h: 0, s: 100, v: 50 },
        layoutDirection: 'horizontal',
        layout: [
          { component: iro.ui.Slider, options: { sliderType: 'hue'} },
          { component: iro.ui.Slider, options: { sliderType: 'saturation'} },
          { component: iro.ui.Slider, options: { sliderType: 'value'} }
        ]
      });

      // Dynamic min/max and base logic for iro.js sliders
      let iroHueMin = 0, iroHueMax = 0, iroHueBase = [];
      let iroSatMin = 0, iroSatMax = 0, iroSatBase = [];
      let iroValMin = 0, iroValMax = 0, iroValBase = [];
      function updateIroHueSliderBounds() {
        const {min, max} = getChannelMinMax('h');
        const maxScale = getChannelMaxScale('h');
        iroHueMin = -min;
        iroHueMax = maxScale - max;
        iroHueBase = firePreset.map(stop => stop.h); // Always use current values
      }
      function updateIroSatSliderBounds() {
        const {min, max} = getChannelMinMax('s');
        const maxScale = getChannelMaxScale('s');
        iroSatMin = -min;
        iroSatMax = maxScale - max;
        iroSatBase = firePreset.map(stop => stop.s); // Always use current values
      }
      function updateIroValSliderBounds() {
        const {min, max} = getChannelMinMax('v');
        const maxScale = getChannelMaxScale('v');
        iroValMin = -min;
        iroValMax = maxScale - max;
        iroValBase = firePreset.map(stop => stop.v); // Always use current values
      }
      // After any manual stop edit, update base arrays
      function updateAllIroBases() {
        iroHueBase = firePreset.map(stop => stop.h);
        iroSatBase = firePreset.map(stop => stop.s);
        iroValBase = firePreset.map(stop => stop.v);
      }
      iroPicker.on('color:change', function(color) {
        if (suppressIroChange) return;
        const h = Math.round(color.hsv.h);
        const s = Math.round(color.hsv.s);
        const v = Math.round(color.hsv.v);
        const avgBaseH = iroHueBase.length ? (iroHueBase.reduce((a, b) => a + b, 0) / iroHueBase.length) : 0;
        const avgBaseS = iroSatBase.length ? (iroSatBase.reduce((a, b) => a + b, 0) / iroSatBase.length) : 0;
        const avgBaseV = iroValBase.length ? (iroValBase.reduce((a, b) => a + b, 0) / iroValBase.length) : 0;
        let offsetH = h - avgBaseH;
        offsetH = Math.max(iroHueMin, Math.min(iroHueMax, offsetH));
        let offsetS = s - avgBaseS;
        offsetS = Math.max(iroSatMin, Math.min(iroSatMax, offsetS));
        let offsetV = v - avgBaseV;
        offsetV = Math.max(iroValMin, Math.min(iroValMax, offsetV));
        const maxScaleH = getChannelMaxScale('h');
        const maxScaleS = getChannelMaxScale('s');
        const maxScaleV = getChannelMaxScale('v');
        firePreset.forEach((stop, i) => {
          let newH = iroHueBase[i] + offsetH;
          let newS = iroSatBase[i] + offsetS;
          let newV = iroValBase[i] + offsetV;
          stop.h = Math.max(0, Math.min(maxScaleH, newH));
          stop.s = Math.max(0, Math.min(maxScaleS, newS));
          stop.v = Math.max(0, Math.min(maxScaleV, newV));
        });
        updateHSV(hsvValuesDiv, interpolatedBar);
      });
      updateIroHueSliderBounds();
      updateIroSatSliderBounds();
      updateIroValSliderBounds();
      iroPicker.on('colors:change', function() {
        updateIroHueSliderBounds();
        updateIroSatSliderBounds();
        updateIroValSliderBounds();
      });

      // Chart.js setup
      const setOpacityPlugin = {
        id: 'setOpacity',
        beforeDatasetsDraw(chart) {
          chart.data.datasets.forEach((ds, i) => {
            const meta = chart.getDatasetMeta(i);
            meta.dataset.options.borderColor = ds.borderColor;
            meta.dataset.options.backgroundColor = ds.backgroundColor;
            meta.dataset.options.borderDash = ds.borderDash;
            meta.data.forEach(point => {
              point.options.backgroundColor = ds.pointBackgroundColor;
              point.options.borderColor = ds.pointBorderColor;
            });
            meta.dataset.options.borderWidth = ds.borderWidth;
            meta.dataset.options.opacity = ds.opacity;
          });
        }
      };
      chart = new Chart(curveCanvas.getContext('2d'), {
        type: 'line',
        data: {
          labels: firePreset.map((_, i) => i+1),
          datasets: getHSVChartDatasets('h')
        },
        options: {
          responsive: false,
          plugins: {
            legend: { display: true, labels: { color: '#f8f8f8' } },
            dragData: {
              round: 1,
              showTooltip: true,
              onDragStart: function(e, datasetIndex, index, value) {
                if (['h','s','v'][datasetIndex] !== activeChannel) return false;
                document.body.style.cursor = 'grabbing';
              },
              onDrag: function(e, datasetIndex, index, value) {},
              onDragEnd: function(e, datasetIndex, index, value) {
                document.body.style.cursor = 'default';
                const ch = ['h','s','v'][datasetIndex];
                if (ch !== activeChannel) return false;
                if (ch === 'h') firePreset[index].h = value;
                else if (ch === 's') firePreset[index].s = value;
                else firePreset[index].v = value;
                updateHSV(hsvValuesDiv, interpolatedBar);
                setActiveChannel(activeChannel);
                updateAllIroBases();
                updateIroHueSliderBounds();
                updateIroSatSliderBounds();
                updateIroValSliderBounds();
              }
            }
          },
          scales: {
            'y-h': {
              position: 'left',
              min: 0,
              max: 360,
              title: { display: true, text: 'Hue', color: '#f8f8f8' },
              grid: { display: false },
              ticks: { color: '#f8f8f8' },
              display: true
            },
            'y-sv': {
              position: 'right',
              min: 0,
              max: 100,
              title: { display: true, text: 'Sat/Val (%)', color: '#f8f8f8' },
              grid: { display: false },
              ticks: { color: '#f8f8f8' },
              display: true
            },
            x: {
              title: { display: true, text: 'Stop', color: '#f8f8f8' },
              ticks: { color: '#f8f8f8' }
            }
          }
        },
        plugins: [setOpacityPlugin]
      });
      updateHSV(hsvValuesDiv, interpolatedBar);
      drawInterpolatedBar(interpolatedBar);
    },
    getLUT: getLUT,
    onLUTChange: null
  };
})();

// To use:
// HSVEditor.init('#your-container');
// let lut = HSVEditor.getLUT();

// Upload updated palettes JSON to ESP32 server
function uploadPalettesJSON(palettePresets) {
    const jsonStr = JSON.stringify(palettePresets, null, 2);
    const blob = new Blob([jsonStr], { type: "application/json" });
    const fileName = "color_palettes.json";

    var xhttp = new XMLHttpRequest();
    xhttp.open('POST', '/upload/' + encodeURIComponent(fileName), true);
    xhttp.send(blob);
    xhttp.onload = function() {
        if (xhttp.status === 200) {
            console.log('Palette JSON uploaded to ESP32!');
        } else {
            console.error('Failed to upload palette JSON.');
        }
    };
}

// Patch: Add logic to save 1px row PNG of interpolated bar on palette save
// This should be called in the save confirm handler inside HSVEditor.init
// Example usage: savePaletteRowPNG(name, container.querySelector('#interpolated-bar'));
function savePaletteRowPNG(name, interpolatedBar) {
    if (interpolatedBar) {
        // Create a 1px-high canvas
        const rowCanvas = document.createElement('canvas');
        rowCanvas.width = interpolatedBar.width;
        rowCanvas.height = 1;
        const srcCtx = interpolatedBar.getContext('2d');
        const rowCtx = rowCanvas.getContext('2d');
        // Get the first row of pixels
        const rowData = srcCtx.getImageData(0, 0, interpolatedBar.width, 1);
        rowCtx.putImageData(rowData, 0, 0);

        // Export as PNG and upload
        rowCanvas.toBlob(function(blob) {
            // Use plain path, do not encode
            const fileName = `images/${name}_Palette.png`;
            var xhttp = new XMLHttpRequest();
            xhttp.open('POST', '/upload/' + fileName, true);
            xhttp.send(blob);
            xhttp.onload = function() {
                if (xhttp.status === 200) {
                    console.log('Palette PNG uploaded to ESP32!');
                } else {
                    console.error('Failed to upload palette PNG.');
                }
            };
        }, 'image/png');
    }
}

// --- PATCH: Add PNG export to save confirm logic ---
// Find the saveConfirmBtn event handler inside HSVEditor.init and add:
// savePaletteRowPNG(name, container.querySelector('#interpolated-bar'));
