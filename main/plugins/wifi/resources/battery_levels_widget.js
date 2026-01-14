// Battery Levels Editor Widget
// Usage: createBatteryLevelsWidget(container, { onSave(blob) })
(function(global) {
  function createBatteryLevelsWidget(container, options = {}) {
    container.innerHTML = '';
    const style = document.createElement('style');
    style.textContent = `
      .battery-panel { background:#181818; border-radius:12px; padding:1.5em; min-width:420px; max-width:900px; margin-bottom:2em; }
      .battery-tabs { display:flex; gap:1em; margin-bottom:1em; align-items:center; justify-content:space-between; }
      .battery-tab-btn { background: #222; color: #eee; border: none; border-radius: 6px 6px 0 0; padding: 0.7em 2em; font-size: 1em; cursor: pointer; margin-bottom: -2px; }
      .battery-tab-btn:focus { outline: 2px solid #888; }
      .battery-save-btn { background: #333; color: #eee; border: none; border-radius: 6px; padding: 0.7em 1.5em; font-size: 1em; cursor: pointer; }
      .battery-save-btn:hover { background: #444; }
      .battery-table { border-collapse: collapse; width: 100%; margin-bottom: 1em; background: #181818; color: #eee; }
      .battery-table th, .battery-table td { border: 1px solid #444; padding: 0.5em; text-align: center; }
      .battery-table th { background: #222; color: #aaa; }
      .battery-input { width: 6em; background: #222; color: #eee; border: 1px solid #444; border-radius: 4px; }
      .battery-input.hsv { width: 3em; min-width: 0; }
    `;
    document.head.appendChild(style);

    const panel = document.createElement('div');
    panel.className = 'battery-panel';
    container.appendChild(panel);

    // Tabs row
    const tabsRow = document.createElement('div');
    tabsRow.className = 'battery-tabs';
    const tabsLeft = document.createElement('div');
    const tabCharging = document.createElement('button');
    tabCharging.className = 'battery-tab-btn';
    tabCharging.textContent = 'Charging';
    tabCharging.onclick = () => showTab('charging');
    const tabDischarging = document.createElement('button');
    tabDischarging.className = 'battery-tab-btn';
    tabDischarging.textContent = 'Discharging';
    tabDischarging.onclick = () => showTab('discharging');
    tabsLeft.appendChild(tabCharging);
    tabsLeft.appendChild(tabDischarging);
    tabsRow.appendChild(tabsLeft);
    panel.appendChild(tabsRow);

    // Editors
    const editorCharging = document.createElement('div');
    editorCharging.id = 'battery-editor-charging';
    editorCharging.style.display = 'block';
    const editorDischarging = document.createElement('div');
    editorDischarging.id = 'battery-editor-discharging';
    editorDischarging.style.display = 'none';
    panel.appendChild(editorCharging);
    panel.appendChild(editorDischarging);

    // Confirm/Cancel row
    const confirmRow = document.createElement('div');
    confirmRow.style.display = 'flex';
    confirmRow.style.flexDirection = 'row';
    confirmRow.style.justifyContent = 'flex-end';
    confirmRow.style.gap = '2em';
    confirmRow.style.marginTop = '2em';
    const confirmBtn = document.createElement('button');
    confirmBtn.className = 'battery-save-btn';
    confirmBtn.textContent = 'Confirm Save';
    confirmBtn.onclick = () => {
      if (batteryData && options.onSave) {
        const blob = new Blob([JSON.stringify(batteryData, null, 2)], {type: 'application/json'});
        options.onSave(blob);
      }
    };
    const cancelBtn = document.createElement('button');
    cancelBtn.className = 'battery-save-btn';
    cancelBtn.style.background = '#444';
    cancelBtn.textContent = 'Cancel';
    cancelBtn.onclick = () => {
      // Optionally clear widget or close modal
      if (options.onCancel) options.onCancel();
    };
    confirmRow.appendChild(confirmBtn);
    confirmRow.appendChild(cancelBtn);
    panel.appendChild(confirmRow);

    let batteryData = null;
    let pluginList = [];
    const fields = ["min_voltage", "plugin", "h", "s", "v", "brightness"];

    function createTable(section, arr) {
      let html = `<div class='section'><table class='battery-table'><tr>`;
      fields.forEach(f => html += `<th>${f}</th>`);
      html += `</tr>`;
      arr.forEach((row, idx) => {
        html += `<tr>`;
        fields.forEach(f => {
          let cls = (f === "h" || f === "s" || f === "v") ? "battery-input hsv" : "battery-input";
          if (f === "plugin" && pluginList.length > 0) {
            html += `<td><select class='${cls}' data-section='${section}' data-idx='${idx}' data-field='${f}'>`;
            pluginList.forEach(opt => {
              const selected = (String(row[f]) === String(opt.id)) ? "selected" : "";
              // Trim text after last underscore
              let displayText = (opt.name || opt.id);
              if (displayText.includes('_')) {
                displayText = displayText.substring(displayText.lastIndexOf('_') + 1);
              }
              html += `<option value='${opt.id}' ${selected}>${displayText}</option>`;
            });
            html += `</select></td>`;
          } else {
            html += `<td><input type='${f=="plugin"?"text":"number"}' class='${cls}' value='${row[f]}' data-section='${section}' data-idx='${idx}' data-field='${f}' /></td>`;
          }
        });
        html += `</tr>`;
      });
      html += `</table></div>`;
      return html;
    }

    function updateField(e) {
      const input = e.target;
      if (!input.classList.contains('battery-input')) return;
      const section = input.dataset.section;
      const idx = parseInt(input.dataset.idx);
      const field = input.dataset.field;
      let val = input.value;
      if (field === "plugin") {
        val = Number(val); // Store plugin as enum id (number)
      } else {
        val = Number(val);
      }
      batteryData[section][idx][field] = val;
    }

    function showTab(tab) {
      editorCharging.style.display = tab === 'charging' ? 'block' : 'none';
      editorDischarging.style.display = tab === 'discharging' ? 'block' : 'none';
      tabCharging.style.background = tab === 'charging' ? '#333' : '#222';
      tabDischarging.style.background = tab === 'discharging' ? '#333' : '#222';
    }

    // Fetch plugin list, then load Battery_Levels.json
    fetch('/api/rgbJSON')
      .then(r => r.json())
      .then(schema => {
        if (schema.plugins && Array.isArray(schema.plugins)) {
          pluginList = schema.plugins;
        }
      })
      .finally(() => {
        fetch('Battery_Levels.json')
          .then(r => r.json())
          .then(json => {
            batteryData = json;
            editorCharging.innerHTML = createTable('charging', batteryData.charging);
            editorDischarging.innerHTML = createTable('discharging', batteryData.discharging);
            showTab('charging');
          })
          .catch(e => {
            editorCharging.innerHTML = '<p style="color:red">Failed to load Battery_Levels.json</p>';
            editorDischarging.innerHTML = '';
          });
      });

    // Listen for changes
    panel.addEventListener('input', updateField);

    // Expose for debugging
    container.batteryLevelsWidget = {
      getData: () => batteryData,
      setData: (data) => {
        batteryData = data;
        editorCharging.innerHTML = createTable('charging', batteryData.charging);
        editorDischarging.innerHTML = createTable('discharging', batteryData.discharging);
      }
    };
  }

  global.createBatteryLevelsWidget = createBatteryLevelsWidget;
})(window);
