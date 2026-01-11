
// rgb_ui.js: UI logic for color picker, brightness slider, animation dropdown, and apply button

window.addEventListener('DOMContentLoaded', function() {
    // Main color picker
    var colorPicker = new iro.ColorPicker('#picker-container', {
        width: 300,
        color: { h: 180, s: 100, v: 100 },
        layoutDirection: 'horizontal',
        layout: [
            { component: iro.ui.Box, options: {} },
            { component: iro.ui.Slider, options: { sliderType: 'hue', sliderDirection: 'vertical'} }
        ]
    });

    // Custom brightness slider picker (value only)
    var bPicker = new iro.ColorPicker('#b-slider-container', {
        width: 300,
        color: { h: 180, s: 100, v: 100 },
        layout: [
            { component: iro.ui.Slider, options: { sliderType: 'value' } }
        ]
    });

    // Sync hue and saturation from main picker to brightness picker
    function syncHS() {
        var hsv = colorPicker.color.hsv;
        bPicker.color.hsv = { h: hsv.h, s: hsv.s, v: bPicker.color.hsv.v };
    }
    colorPicker.on('color:change', syncHS);
    syncHS();

    // Display HSV and 8-bit values for main picker
    function updateHSV() {
        var hsv = colorPicker.color.hsv;
        var h8 = Math.round(hsv.h * 255 / 360);
        var s8 = Math.round(hsv.s * 255 / 100);
        var v8 = Math.round(hsv.v * 255 / 100);
        document.getElementById('hsv-values').innerHTML =
            `<b>H:</b> ${Math.round(hsv.h)} &nbsp; <b>S:</b> ${Math.round(hsv.s)} &nbsp; <b>V:</b> ${Math.round(hsv.v)}<br>` +
            `<span style='color:#555'>8-bit: <b>H:</b> ${h8} &nbsp; <b>S:</b> ${s8} &nbsp; <b>V:</b> ${v8}</span>`;
        document.getElementById('color-preview').style.background = colorPicker.color.hexString;
    }
    colorPicker.on('color:change', updateHSV);
    updateHSV();

    // Display 8-bit B value for brightness picker
    function updateB() {
        var b = bPicker.color.hsv.v;
        var b8 = Math.round(b * 255 / 100);
        document.getElementById('b-value').innerHTML = `<b>B:</b> ${b8} (8-bit)`;
    }
    bPicker.on('color:change', updateB);
    updateB();

    // Populate animation dropdown from /api/rgbJSON
    fetch('/api/rgbJSON')
        .then(response => {
            if (!response.ok) throw new Error('Failed to fetch RGB JSON');
            return response.json();
        })
        .then(schema => {
            const animationDropdown = document.getElementById('animation-dropdown');
            animationDropdown.innerHTML = '';
            if (schema.plugins && Array.isArray(schema.plugins)) {
                schema.plugins.forEach(plugin => {
                    const opt = document.createElement('option');
                    opt.value = plugin.id;
                    opt.textContent = plugin.name || plugin.id;
                    animationDropdown.appendChild(opt);
                });
            }
        })
        .catch(err => {
            console.error('Error fetching RGB JSON:', err);
            // fallback: show a default option
            const animationDropdown = document.getElementById('animation-dropdown');
            animationDropdown.innerHTML = '<option value="none">None</option>';
        });

    // Handle Apply button
    document.getElementById('apply-btn').onclick = function() {
        var hsv = colorPicker.color.hsv;
        var h8 = Math.round(hsv.h * 255 / 360);
        var s8 = Math.round(hsv.s * 255 / 100);
        var v8 = Math.round(hsv.v * 255 / 100);
        var b8 = Math.round(bPicker.color.hsv.v * 255 / 100);
        var plugin = Number(document.getElementById('animation-dropdown').value);
        var payload = { plugin, h: h8, s: s8, v: v8, b: b8 };
        console.log('Sending payload:', payload);
        fetch('/api/rgb', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify(payload)
        })
        .then(resp => {
            if (!resp.ok) throw new Error('Failed to apply RGB settings');
            return resp.json();
        })
        .then(result => {
            // Optionally: show success message
            console.log('RGB settings applied:', result);
        })
        .catch(err => {
            alert('Failed to apply RGB settings.');
            console.error(err);
        });
    };
});
