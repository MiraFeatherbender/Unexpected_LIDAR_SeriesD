// rgbAnimationsEditor.js
// One-file, one-instance RGB Animations Editor widget

(function(global) {
  // --- CSS ---
  const style = document.createElement('style');
  style.textContent = `
    .rgb-editor-container { max-width: 260px; margin: 40px auto; background: #23272a; border-radius: 8px; box-shadow: 0 2px 8px #000a; padding: 10px 12px; color: #e0e0e0; font-family: 'Segoe UI', Arial, sans-serif; }
    .rgb-editor-container label { display: block; margin: 2px 0 2px 0; color: #b0b8c1; }
    .rgb-editor-container input[type="number"], .rgb-editor-container input[type="text"] { background: #181a1b; color: #e0e0e0; border: 1px solid #444; border-radius: 4px; padding: 6px 10px; font-size: 1em; margin-bottom: 8px; width: 100%; box-sizing: border-box; }
    .rgb-editor-container select { background: #181a1b; color: #e0e0e0; border: 1px solid #444; border-radius: 4px; padding: 5px 6px; font-size: 1em; margin-bottom: 8px; max-width: 220px; min-width: 60px; width: auto; box-sizing: border-box; display: inline-block; }
    .rgb-editor-container .anim-block { background: #23272a; border: 1px solid #333; border-radius: 6px; margin-bottom: 8px; padding: 4px 16px 18px 16px; box-shadow: inset 0 2px 8px #181a1b, inset 0 1.5px 0 #444; }
    .rgb-editor-container .btn { background: #7ecfff; color: #181a1b; border: none; border-radius: 4px; padding: 8px 17px; font-size: 1em; cursor: pointer; margin-top: 12px; margin-right: 8px; transition: background 0.2s; }
    .rgb-editor-container .btn:hover { background: #5bb0e6; }
    .rgb-editor-container .tab-bar { display:flex; gap:8px; margin:8px 0 4px 0; justify-content:center; }
    .rgb-editor-container .tab-btn.active { background:#36393f !important; border-bottom:2px solid #7289da !important; color:#7289da !important; font-weight:bold; }
    .rgb-editor-container .tab-btn { background:#2c2f33; color:#8a929a; border:1px solid #444; border-radius:4px 4px 0 0; padding:6px 18px; font-size:14px; cursor:pointer; outline:none; margin-bottom:-1px; }
  `;
  document.head.appendChild(style);

  // --- Widget Constructor ---
  function createRgbAnimationsEditor(container, options = {}) {
    // State
    let animations = options.animations || [];
    let selectedIdx = 0;
    let selectedTab = 'contrast';
    const BRIGHTNESS_STRATEGIES = ["value_noise", "index"];
    // Use global lists if available, fallback to defaults
    const PALETTES = window.Palette_PNG_List || ["HSV_PALETTE_WATER", "HSV_PALETTE_AURORA", "HSV_PALETTE_FIRE"];
    const CONTRAST_NOISE = window.Contrast_PNG_List || ["openSimplex2", "perlin"];
    const BRIGHTNESS_NOISE = window.Brightness_PNG_List || ["openSimplex2", "perlin"];

    function renderAnimationDropdown() {
      // Use idNameList if provided, otherwise fallback to ID only
      const idNameList = options.idNameList || [];
      return `<label for="animIdSelect" style="display:block; text-align:center; margin-bottom:4px;">Select Animation</label>
        <select id="animIdSelect" style="width:220px; margin-bottom:12px;" onchange="window.rgbEditorApi.selectAnimation(this.value)">
          ${animations.map((a, i) => {
            let label = `ID ${a.id}`;
            const found = idNameList.find(entry => entry.id === a.id);
            if (found) label = `${a.id}: ${found.name}`;
            return `<option value="${i}"${i === selectedIdx ? ' selected' : ''}>${label}</option>`;
          }).join('')}
        </select>`;
    }

    // --- Helpers ---
    function renderDropdown(options, selected, onchange, style = '') {
      return `<select onchange="${onchange}" style="${style}">
        ${options.map(opt => `<option value="${opt}"${opt === selected ? ' selected' : ''}>${opt}</option>`).join('')}
      </select>`;
    }
    function renderTabBar(tabs, selectedTab, onClick) {
      return `<div class="tab-bar">
        ${tabs.map(tab => `<button class="tab-btn${selectedTab === tab ? ' active' : ''}" onclick="${onClick}('${tab}')">${tab.charAt(0).toUpperCase() + tab.slice(1)}</button>`).join('')}
      </div>`;
    }
    function renderWalkSpecGrid(walkSpec, tab) {
      return `<label>Walk Spec</label>
        <div style="display: grid; grid-template-columns: 30px 1fr 1fr; grid-template-rows: 24px 1fr 1fr; gap: 2px; align-items: center; margin-top: 4px; margin-bottom: 8px;">
          <div></div>
          <div style="text-align:center; font-size: 0.95em; color: #b0b8c1;">min</div>
          <div style="text-align:center; font-size: 0.95em; color: #b0b8c1;">max</div>
          <div style="font-size: 0.95em; color: #b0b8c1;">dx</div>
          <input type="number" style="width:75%;" value="${walkSpec.min_dx}" onchange="window.rgbEditorApi.updateMimicWalk('${tab}', 'min_dx', this.valueAsNumber)">
          <input type="number" style="width:75%;" value="${walkSpec.max_dx}" onchange="window.rgbEditorApi.updateMimicWalk('${tab}', 'max_dx', this.valueAsNumber)">
          <div style="font-size: 0.95em; color: #b0b8c1;">dy</div>
          <input type="number" style="width:75%;" value="${walkSpec.min_dy}" onchange="window.rgbEditorApi.updateMimicWalk('${tab}', 'min_dy', this.valueAsNumber)">
          <input type="number" style="width:75%;" value="${walkSpec.max_dy}" onchange="window.rgbEditorApi.updateMimicWalk('${tab}', 'max_dy', this.valueAsNumber)">
        </div>`;
    }

    // --- Render ---
    function render() {
      container.innerHTML = '';
      const panel = document.createElement('div');
      panel.className = 'rgb-editor-container';
      // Animation select dropdown
      panel.innerHTML = renderAnimationDropdown();
      // Action buttons
      panel.innerHTML += `
        <div style="text-align:center; margin-bottom:12px;">
          <button class="btn" id="addAnimBtn">Add</button>
          <button class="btn" onclick="window.rgbEditorApi.removeCurrentAnimation()" style="margin-right:4px;">Remove</button>
          <button class="btn" onclick="window.rgbEditorApi.saveJson()">Save</button>
        </div>
        <div id="addAnimModal" style="display:none; position:fixed; top:0; left:0; width:100vw; height:100vh; background:rgba(0,0,0,0.4); z-index:1000; align-items:center; justify-content:center;">
          <div style="background:#23272a; color:#e0e0e0; border-radius:10px; box-shadow:0 2px 16px #000a; padding:32px 28px; min-width:320px; max-width:90vw; margin:auto; position:relative; top:15vh;">
            <h2 style="margin-top:0; color:#7ecfff;">Add Animation</h2>
            <label for="modalAnimIdSelect">Select Animation</label>
            <select id="modalAnimIdSelect" style="width:100%; margin-bottom:18px;"></select>
            <div style="text-align:right;">
              <button class="btn" id="confirmAddAnimBtn">Confirm</button>
              <button class="btn" id="cancelAddAnimBtn" style="background:#444; color:#eee;">Cancel</button>
            </div>
          </div>
        </div>
      `;
      // Tab bar
      const tabBar = renderTabBar(['contrast', 'brightness'], selectedTab, 'window.rgbEditorApi.setTab');
      // Mimic fields
      let mimicNoise, mimicWalk, noiseList;
      if (selectedTab === 'contrast') {
        mimicNoise = animations[selectedIdx].contrast_noise_field;
        mimicWalk = animations[selectedIdx].contrast_walk_spec;
        noiseList = CONTRAST_NOISE;
      } else {
        mimicNoise = animations[selectedIdx].brightness_noise_field;
        mimicWalk = animations[selectedIdx].brightness_walk_spec;
        noiseList = BRIGHTNESS_NOISE;
      }
      const mimicFields = `
        <div class="anim-block" style="background:none; box-shadow:none; border:none; padding:0; margin-bottom:0;">
          <label style="margin-top:0;">Noise</label>
          <div style="text-align:center;">
            ${renderDropdown(noiseList, mimicNoise, `window.rgbEditorApi.updateNoiseField('${selectedTab}', this.value)`, "width:220px !important; display:block; margin:auto;")}
          </div>
          ${renderWalkSpecGrid(mimicWalk, selectedTab)}
        </div>
      `;
      panel.innerHTML += `
        <label>Palette</label>
        <div style="text-align:center;">
          ${renderDropdown(PALETTES, animations[selectedIdx].palette, `window.rgbEditorApi.updateField(${selectedIdx}, 'palette', this.value)`, "width:220px !important; display:block; margin:auto;")}
        </div>
        ${tabBar}
        <div style="height:8px;"></div>
        ${mimicFields}
        ${selectedTab === 'brightness' ? `
          <label>Brightness Strategy</label>
          <div style="text-align:center;">
            ${renderDropdown(BRIGHTNESS_STRATEGIES, animations[selectedIdx].brightness_strategy, `window.rgbEditorApi.updateField(${selectedIdx}, 'brightness_strategy', this.value)`, "width:220px !important; display:block; margin:auto;")}
          </div>
        ` : ''}
      `;
      container.appendChild(panel);

      // Modal logic for Add Animation
      const addAnimBtn = panel.querySelector('#addAnimBtn');
      const addAnimModal = panel.querySelector('#addAnimModal');
      const modalAnimIdSelect = panel.querySelector('#modalAnimIdSelect');
      const confirmAddAnimBtn = panel.querySelector('#confirmAddAnimBtn');
      const cancelAddAnimBtn = panel.querySelector('#cancelAddAnimBtn');
      if (addAnimBtn && addAnimModal && modalAnimIdSelect && confirmAddAnimBtn && cancelAddAnimBtn) {
        addAnimBtn.onclick = function() {
          // Populate modalAnimIdSelect with unused IDs from idNameList
          const used = new Set(animations.map(a => a.id));
          const idNameList = options.idNameList || [];
          // Filter to palette-based entries only (exclude plugin-exclusive)
          const paletteIdNameList = (options.paletteIdNameList || idNameList).filter(entry => entry.isPaletteBased);
          let opts = '';
          paletteIdNameList.forEach(entry => {
            if (!used.has(entry.id)) {
              opts += `<option value=\"${entry.id}\">${entry.id}: ${entry.name}</option>`;
            }
          });
          modalAnimIdSelect.innerHTML = opts;
          addAnimModal.style.display = 'flex';
        };
        cancelAddAnimBtn.onclick = function() {
          addAnimModal.style.display = 'none';
        };
        confirmAddAnimBtn.onclick = function() {
          const newId = parseInt(modalAnimIdSelect.value);
          if (isNaN(newId)) {
            alert('No available IDs');
            return;
          }
          // Add new animation with selected ID
          animations.push({
            id: newId,
            palette: PALETTES[0],
            brightness_noise_field: NOISE_TYPES[0],
            contrast_walk_spec: { min_dx: 0, max_dx: 0, min_dy: 0, max_dy: 0 },
            brightness_walk_spec: { min_dx: 0, max_dx: 0, min_dy: 0, max_dy: 0 },
            brightness_strategy: BRIGHTNESS_STRATEGIES[0]
          });
          selectedIdx = animations.length - 1;
          addAnimModal.style.display = 'none';
          render();
        };
      }
    }

    // --- API for event handlers ---
    window.rgbEditorApi = {
            addAnimation: function() {
              // Add a new animation with default fields
              animations.push({
                id: animations.length ? Math.max(...animations.map(a => a.id)) + 1 : 1,
                palette: PALETTES[0],
                contrast_noise_field: NOISE_TYPES[0],
                brightness_noise_field: NOISE_TYPES[1],
                contrast_walk_spec: { min_dx: 0, max_dx: 1, min_dy: 0, max_dy: 1 },
                brightness_walk_spec: { min_dx: 0, max_dx: 6, min_dy: 0, max_dy: 3 },
                brightness_strategy: BRIGHTNESS_STRATEGIES[0]
              });
              selectedIdx = animations.length - 1;
              render();
            },
            removeCurrentAnimation: function() {
              if (animations.length > 1) {
                animations.splice(selectedIdx, 1);
                selectedIdx = Math.max(0, selectedIdx - 1);
                render();
              }
            },
            saveJson: function() {
              // Download JSON as a file
              const jsonStr = JSON.stringify({ animations }, null, 2);
              const blob = new Blob([jsonStr], { type: 'application/json' });
              const a = document.createElement('a');
              a.href = URL.createObjectURL(blob);
              a.download = 'rgb_animations.json';
              document.body.appendChild(a);
              a.click();
              setTimeout(() => { document.body.removeChild(a); URL.revokeObjectURL(a.href); }, 500);
            },
      setTab: function(tab) { selectedTab = tab; render(); },
      updateField: function(idx, field, value) { animations[idx][field] = value; render(); },
      updateMimicWalk: function(tab, subfield, value) {
        if (tab === 'contrast') {
          animations[selectedIdx].contrast_walk_spec[subfield] = value;
        } else {
          animations[selectedIdx].brightness_walk_spec[subfield] = value;
        }
        render();
      },
      updateNoiseField: function(tab, value) {
        if (tab === 'contrast') {
          animations[selectedIdx].contrast_noise_field = value;
        } else {
          animations[selectedIdx].brightness_noise_field = value;
        }
        render();
      },
      selectAnimation: function(idx) {
        selectedIdx = parseInt(idx, 10);
        render();
      }
    };

    // Initial render
    render();
  }

  // Expose globally
  global.createRgbAnimationsEditor = createRgbAnimationsEditor;

})(window);
