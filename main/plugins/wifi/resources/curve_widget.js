// curve_widget.js
// Drop-in curve editor widget for brightness/adjustment arrays
// Usage: <script src="curve_widget.js"></script>
// Then: CurveWidget.create({container: ..., onChange: ...})

(function(global) {
    const CurveWidget = {
        create: function(options) {
            // options: {container, onChange}
            const container = options.container || document.body;
            const width = 300, height = 300;
            const barHeight = 48;
            // Create DOM
            const wrapper = document.createElement('div');
            wrapper.style.display = 'flex';
            wrapper.style.flexDirection = 'column';
            wrapper.style.alignItems = 'center';
            wrapper.style.margin = '8px 0';
            const canvas = document.createElement('canvas');
            canvas.width = width; canvas.height = height;
            canvas.style.background = '#181818';
            canvas.style.borderRadius = '8px';
            wrapper.appendChild(canvas);
            container.appendChild(wrapper);

            // Chart.js setup
            const controlPoints = [
                {x: 0, y: 0},
                {x: 255, y: 255}
            ];
            const ctx = canvas.getContext('2d');
            const chart = new Chart(ctx, {
                type: 'scatter',
                data: {
                    datasets: [{
                        type: 'scatter',
                        label: 'Curve',
                        data: controlPoints,
                        borderColor: '#fff',
                        backgroundColor: '#fff',
                        pointBackgroundColor: '#fff',
                        pointRadius: 6,
                        pointHoverRadius: 8,
                        fill: false,
                        showLine: true,
                        tension: 0.4,
                        dragData: true,
                        dragDataRound: 1,
                        parsing: false,
                    }]
                },
                options: {
                    responsive: false,
                    animation: false,
                    interaction: {
                        mode: 'nearest',
                        intersect: true
                    },
                    scales: {
                        x: {
                            type: 'linear',
                            min: 0,
                            max: 256,
                            title: { display: true, text: 'Input (x)' },
                            grid: { color: '#444' },
                            ticks: { stepSize: 32 }
                        },
                        y: {
                            min: 0,
                            max: 256,
                            title: { display: true, text: 'Output (y)' },
                            grid: { color: '#444' },
                            ticks: { stepSize: 32 }
                        }
                    },
                    plugins: {
                        legend: { display: false },
                        dragData: {
                            showTooltip: true,
                            dragDataX: true,
                            dragX: function(e, datasetIndex, index, value) {
                                const pts = chart.data.datasets[0].data;
                                if (index === 0 || index === pts.length - 1) return false;
                                const minX = pts[index - 1].x + 1;
                                const maxX = pts[index + 1].x - 1;
                                if (value.x < minX) value.x = minX;
                                if (value.x > maxX) value.x = maxX;
                                return true;
                            },
                            onDrag: function() { updateCurveYArray(); emitChange(); },
                            onDragEnd: function() { updateCurveYArray(); emitChange(); }
                        }
                    }
                }
            });

            // Add double-click to add a new point
            canvas.addEventListener('dblclick', function(event) {
                const rect = this.getBoundingClientRect();
                const mouseX = event.clientX - rect.left;
                const mouseY = event.clientY - rect.top;
                const xScale = chart.scales.x;
                const yScale = chart.scales.y;
                const x = xScale.getValueForPixel(mouseX);
                const y = yScale.getValueForPixel(mouseY);
                const cx = Math.round(Math.max(0, Math.min(255, x)));
                const cy = Math.round(Math.max(-128, Math.min(127, y)));
                let pts = chart.data.datasets[0].data;
                if (pts.some(pt => pt.x === cx)) return;
                pts.push({x: cx, y: cy});
                pts.sort((a, b) => a.x - b.x);
                chart.update();
                updateCurveYArray();
                emitChange();
            });

            // Ctrl+Click on a point deletes it (except endpoints)
            canvas.addEventListener('click', function(event) {
                if (!event.ctrlKey) return;
                const rect = this.getBoundingClientRect();
                const mouseX = event.clientX - rect.left;
                const mouseY = event.clientY - rect.top;
                const pts = chart.data.datasets[0].data;
                const xScale = chart.scales.x;
                const yScale = chart.scales.y;
                let minDist = 16;
                let closestIdx = -1;
                for (let i = 0; i < pts.length; ++i) {
                    const px = xScale.getPixelForValue(pts[i].x);
                    const py = yScale.getPixelForValue(pts[i].y);
                    const dist = Math.hypot(mouseX - px, mouseY - py);
                    if (dist < minDist) {
                        minDist = dist;
                        closestIdx = i;
                    }
                }
                if (closestIdx > 0 && closestIdx < pts.length - 1) {
                    pts.splice(closestIdx, 1);
                    chart.update();
                    updateCurveYArray();
                    emitChange();
                }
            });

            let curveYArray = new Array(256).fill(0);
            const SAMPLES = 32;
            let sparseY = new Array(SAMPLES).fill(0);

            // Monotonic cubic interpolation using the control points array
            function updateCurveYArray() {
                const pts = chart.data.datasets[0].data.slice().sort((a, b) => a.x - b.x);
                const n = pts.length;
                if (n < 2) return;
                // Compute slopes (delta) and tangents (m)
                const delta = new Array(n - 1);
                const m = new Array(n);
                for (let i = 0; i < n - 1; ++i) {
                    delta[i] = (pts[i + 1].y - pts[i].y) / (pts[i + 1].x - pts[i].x);
                }
                m[0] = delta[0];
                m[n - 1] = delta[n - 2];
                for (let i = 1; i < n - 1; ++i) {
                    if (delta[i - 1] * delta[i] <= 0) {
                        m[i] = 0;
                    } else {
                        const w1 = 2 * (pts[i].x - pts[i - 1].x) + (pts[i + 1].x - pts[i].x);
                        const w2 = (pts[i].x - pts[i - 1].x) + 2 * (pts[i + 1].x - pts[i].x);
                        m[i] = (w1 + w2) > 0 ? (w1 + w2) / (w1 / delta[i - 1] + w2 / delta[i]) : 0;
                    }
                }
                // Interpolate for each x in 0..255
                for (let x = 0; x < 256; ++x) {
                    // Find the segment
                    let i = 0;
                    while (i < n - 2 && x > pts[i + 1].x) ++i;
                    const x0 = pts[i].x, x1 = pts[i + 1].x;
                    const y0 = pts[i].y, y1 = pts[i + 1].y;
                    const m0 = m[i], m1 = m[i + 1];
                    const h = x1 - x0;
                    let t = 0;
                    if (h > 0) t = (x - x0) / h;
                    // Hermite cubic
                    const t2 = t * t, t3 = t2 * t;
                    const h00 = 2 * t3 - 3 * t2 + 1;
                    const h10 = t3 - 2 * t2 + t;
                    const h01 = -2 * t3 + 3 * t2;
                    const h11 = t3 - t2;
                    curveYArray[x] = Math.round(
                        h00 * y0 + h10 * h * m0 + h01 * y1 + h11 * h * m1
                    );
                }
            }



            function emitChange() {
                if (typeof options.onChange === 'function') {
                    options.onChange(curveYArray.slice());
                }
            }

            // Initial render
            updateCurveYArray();
            emitChange();

            // Expose API
            return {
                getAdjustmentArray: function() { return curveYArray.slice(); },
                setPoints: function(arr) {
                    if (Array.isArray(arr) && arr.length >= 2) {
                        chart.data.datasets[0].data = arr.map(pt => ({x: pt.x, y: pt.y}));
                        chart.update();
                        updateCurveYArray();
                        emitChange();
                    }
                },
                chart: chart,
                canvas: canvas,
                wrapper: wrapper
            };
        }
    };
    global.CurveWidget = CurveWidget;
})(window);
