const canvas = document.getElementById('canvas');
const ctx = canvas.getContext('2d');
const timeDisplay = document.getElementById('time-display');
const btnPlay = document.getElementById('btn-play');

// Панель
const panel = document.getElementById('panel');
const panelTitle = document.getElementById('panel-title');
const btnPanelClose = document.getElementById('panel-close');
const btnDeleteNode = document.getElementById('btn-delete-node');

// Модалка
const modal = document.getElementById('modal');
const btnModalClose = document.getElementById('modal-close');
const btnModalCancel = document.getElementById('btn-modal-cancel');
const btnModalCreate = document.getElementById('btn-modal-create');
const modalError = document.getElementById('modal-error');

let nodes = [];
let nodeMap = {};
let selectedNode = null;
let isPlaying = false;
let playInterval = null;
let historyChart = null;

// Карта: pan + zoom
let offsetX = 0;
let offsetY = 0;
let scale = 1;
let isDragging = false;
let dragStartX = 0;
let dragStartY = 0;
let dragStartOffsetX = 0;
let dragStartOffsetY = 0;
let movedDistance = 0;

// ---- Canvas ----
function resizeCanvas() {
    canvas.width = window.innerWidth;
    canvas.height = window.innerHeight;
}
window.addEventListener('resize', resizeCanvas);
resizeCanvas();

// ---- Раскладка ----
function layoutNodes(state) {
    const levels = { 0: [], 1: [], 2: [], 3: [] };
    for (const n of state.nodes) levels[n.type].push(n);

    nodes = [];
    nodeMap = {};

    const levelSpacing = 220;
    const baseY = 200;

    for (let type = 0; type <= 3; type++) {
        const arr = levels[type];
        const count = arr.length;
        const y = baseY + type * levelSpacing;
        const totalWidth = Math.max(count * 180, canvas.width);
        for (let i = 0; i < count; i++) {
            const x = (i + 0.5) * (totalWidth / Math.max(count, 1));
            arr[i].x = x;
            arr[i].y = y;
            nodes.push(arr[i]);
            nodeMap[arr[i].ip] = arr[i];
        }
    }
}

// ---- Цвета ----
function getNodeColor(node) {
    const ratio = node.capacity > 0 ? node.current_load / node.capacity : 0;
    if (ratio < 0.5) return '#4caf50';
    if (ratio < 0.8) return '#ff9800';
    return '#f44336';
}
function getRadius(type) { return [36, 28, 20, 12][type] || 10; }
function getTypeStroke(type) { return ['#ffd700', '#4a90e2', '#50c878', '#888'][type] || '#888'; }
function getTypeName(type) { return ['Core', 'Distribution', 'Access', 'Subscriber'][type] || '?'; }

// ---- Рисование ----
function draw() {
    ctx.setTransform(1, 0, 0, 1, 0, 0);
    ctx.fillStyle = '#1a1a1a';
    ctx.fillRect(0, 0, canvas.width, canvas.height);

    ctx.setTransform(scale, 0, 0, scale, offsetX, offsetY);

    // Сетка (для ощущения карты)
    drawGrid();

    // Рёбра
    for (const node of nodes) {
        if (!node.parent_ip) continue;
        const parent = nodeMap[node.parent_ip];
        if (!parent) continue;

        const ratio = node.capacity > 0 ? node.current_load / node.capacity : 0;
        ctx.lineWidth = 1 + ratio * 5;
        ctx.strokeStyle = ratio > 0.8 ? '#f44336' : (ratio > 0.5 ? '#ff9800' : '#555');

        ctx.beginPath();
        ctx.moveTo(node.x, node.y);
        ctx.lineTo(parent.x, parent.y);
        ctx.stroke();
    }

    // Узлы
    for (const node of nodes) {
        const r = getRadius(node.type);

        // Тень / выделение
        if (selectedNode && selectedNode.ip === node.ip) {
            ctx.beginPath();
            ctx.arc(node.x, node.y, r + 6, 0, 2 * Math.PI);
            ctx.fillStyle = 'rgba(108, 196, 255, 0.15)';
            ctx.fill();
        }

        ctx.beginPath();
        ctx.arc(node.x, node.y, r, 0, 2 * Math.PI);
        ctx.fillStyle = getNodeColor(node);
        ctx.fill();

        ctx.strokeStyle = getTypeStroke(node.type);
        ctx.lineWidth = 2.5;
        ctx.stroke();

        // Подпись
        ctx.fillStyle = '#ddd';
        ctx.font = '11px Arial';
        ctx.textAlign = 'center';
        ctx.fillText(node.ip, node.x, node.y + r + 15);
    }
}

// Сетка на фоне
function drawGrid() {
    const step = 80;
    const w = canvas.width / scale;
    const h = canvas.height / scale;
    const startX = -offsetX / scale;
    const startY = -offsetY / scale;

    ctx.strokeStyle = 'rgba(255,255,255,0.04)';
    ctx.lineWidth = 1;

    const gridStartX = Math.floor(startX / step) * step;
    const gridStartY = Math.floor(startY / step) * step;

    for (let x = gridStartX; x < startX + w; x += step) {
        ctx.beginPath();
        ctx.moveTo(x, startY);
        ctx.lineTo(x, startY + h);
        ctx.stroke();
    }
    for (let y = gridStartY; y < startY + h; y += step) {
        ctx.beginPath();
        ctx.moveTo(startX, y);
        ctx.lineTo(startX + w, y);
        ctx.stroke();
    }
}

// ---- API ----
async function fetchState() {
    const r = await fetch('/api/state');
    const s = await r.json();
    updateFromState(s);
}
async function doTick() {
    const r = await fetch('/api/tick', { method: 'POST' });
    updateFromState(await r.json());
}
async function doReset() {
    await fetch('/api/clear-history', { method: 'POST' });
    const r = await fetch('/api/reset', { method: 'POST' });
    selectedNode = null;
    hidePanel();
    updateFromState(await r.json());
}

function updateFromState(state) {
    timeDisplay.textContent = `Time: ${String(state.hour).padStart(2,'0')}:00, day ${state.day}`;
    layoutNodes(state);

    // Обновляем выбранный узел свежими данными
    if (selectedNode) {
        selectedNode = nodeMap[selectedNode.ip] || null;
        if (selectedNode) showPanel(selectedNode);
        else hidePanel();
    }

    draw();
}

// ---- Мышь ----
function screenToWorld(sx, sy) {
    return {
        x: (sx - offsetX) / scale,
        y: (sy - offsetY) / scale,
    };
}

function findNodeAt(wx, wy) {
    for (let i = nodes.length - 1; i >= 0; i--) {
        const n = nodes[i];
        const dx = wx - n.x;
        const dy = wy - n.y;
        const r = getRadius(n.type);
        if (dx*dx + dy*dy < r*r) return n;
    }
    return null;
}

canvas.addEventListener('mousedown', (e) => {
    isDragging = true;
    movedDistance = 0;
    dragStartX = e.clientX;
    dragStartY = e.clientY;
    dragStartOffsetX = offsetX;
    dragStartOffsetY = offsetY;
    canvas.classList.add('dragging');
});

canvas.addEventListener('mousemove', (e) => {
    if (isDragging) {
        const dx = e.clientX - dragStartX;
        const dy = e.clientY - dragStartY;
        movedDistance = Math.sqrt(dx*dx + dy*dy);
        offsetX = dragStartOffsetX + dx;
        offsetY = dragStartOffsetY + dy;
        draw();
    } else {
        // Смена курсора на "pointer", если над узлом
        const rect = canvas.getBoundingClientRect();
        const { x, y } = screenToWorld(e.clientX - rect.left, e.clientY - rect.top);
        const node = findNodeAt(x, y);
        canvas.classList.toggle('pointing', !!node);
    }
});

window.addEventListener('mouseup', (e) => {
    if (isDragging && movedDistance < 5) {
        // Это был клик, а не drag
        const rect = canvas.getBoundingClientRect();
        const { x, y } = screenToWorld(e.clientX - rect.left, e.clientY - rect.top);
        const node = findNodeAt(x, y);
        if (node) {
            selectedNode = node;
            showPanel(node);
            draw();
        } else {
            selectedNode = null;
            hidePanel();
            draw();
        }
    }
    isDragging = false;
    canvas.classList.remove('dragging');
});

// Zoom под курсором
canvas.addEventListener('wheel', (e) => {
    e.preventDefault();
    const rect = canvas.getBoundingClientRect();
    const mx = e.clientX - rect.left;
    const my = e.clientY - rect.top;

    const zoom = e.deltaY < 0 ? 1.12 : 0.89;
    const newScale = Math.max(0.2, Math.min(4, scale * zoom));

    offsetX = mx - (mx - offsetX) * (newScale / scale);
    offsetY = my - (my - offsetY) * (newScale / scale);
    scale = newScale;

    draw();
}, { passive: false });

// ---- Панель узла ----
function showPanel(node) {
    panel.classList.remove('hidden');
    panelTitle.textContent = node.ip;

    document.getElementById('p-ip').textContent = node.ip;
    document.getElementById('p-type').textContent = getTypeName(node.type);
    document.getElementById('p-capacity').textContent = node.capacity;
    document.getElementById('p-load').textContent = node.current_load.toFixed(2);

    const ratio = node.capacity > 0 ? node.current_load / node.capacity : 0;
    document.getElementById('p-percent').textContent = (ratio * 100).toFixed(1) + '%';

    const bar = document.getElementById('p-bar');
    bar.style.width = Math.min(ratio * 100, 100) + '%';
    bar.style.background = ratio > 0.8 ? '#f44336' : (ratio > 0.5 ? '#ff9800' : '#4caf50');

    document.getElementById('p-max').textContent = node.max_load.toFixed(2);
    document.getElementById('p-parent').textContent = node.parent_ip || '—';

    // Дети
    const children = nodes.filter(n => n.parent_ip === node.ip).map(n => n.ip);
    document.getElementById('p-children').textContent = children.length ? children.join(', ') : '—';
    loadHistory(node.ip);
    // Кнопка удаления — только если не Core
    if (node.type === 0) {
        btnDeleteNode.disabled = true;
        btnDeleteNode.textContent = 'Ядро удалить нельзя';
    } else {
        btnDeleteNode.disabled = false;
        btnDeleteNode.textContent = 'Удалить узел';
    }
}
function hidePanel() {
    panel.classList.add('hidden');
}

btnPanelClose.onclick = () => {
    selectedNode = null;
    hidePanel();
    draw();
};

btnDeleteNode.onclick = async () => {
    if (!selectedNode || selectedNode.type === 0) return;

    // Считаем всё поддерево
    const descendants = [];
    function collect(ip) {
        descendants.push(ip);
        for (const n of nodes) {
            if (n.parent_ip === ip) collect(n.ip);
        }
    }
    collect(selectedNode.ip);

    const count = descendants.length;
    const msg = count > 1
        ? `Удалить узел ${selectedNode.ip} и его потомков (${count - 1} шт.)?`
        : `Удалить узел ${selectedNode.ip}?`;

    if (!confirm(msg)) return;

    const r = await fetch(`/api/node/${selectedNode.ip}`, { method: 'DELETE' });
    if (!r.ok) {
        alert('Ошибка удаления: ' + (await r.text()));
        return;
    }
    selectedNode = null;
    hidePanel();
    updateFromState(await r.json());
};

// ---- Модалка добавления ----
function showModal() {
    modal.classList.remove('hidden');
    modalError.textContent = '';
    document.getElementById('in-ip').value = '';
    document.getElementById('in-parent').value = '';
}
function hideModal() {
    modal.classList.add('hidden');
}

document.getElementById('btn-add').onclick = showModal;
btnModalClose.onclick = hideModal;
btnModalCancel.onclick = hideModal;

btnModalCreate.onclick = async () => {
    const type = parseInt(document.getElementById('in-type').value);
    const ip = document.getElementById('in-ip').value.trim();
    const capacity = parseInt(document.getElementById('in-capacity').value);
    const parent_ip = document.getElementById('in-parent').value.trim();

    if (!ip || !capacity || capacity <= 0) {
        modalError.textContent = 'Заполните все поля корректно';
        return;
    }

    const r = await fetch('/api/node', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ type, ip, capacity, parent_ip }),
    });

    if (!r.ok) {
        const err = await r.json();
        modalError.textContent = err.detail || 'Ошибка';
        return;
    }

    hideModal();
    updateFromState(await r.json());
};

// ---- Кнопки панели управления ----
document.getElementById('btn-step').onclick = doTick;
document.getElementById('btn-reset').onclick = doReset;

btnPlay.onclick = () => {
    isPlaying = !isPlaying;
    if (isPlaying) {
        btnPlay.textContent = '⏸ Пауза';
        btnPlay.style.background = '#c0392b';
        playInterval = setInterval(doTick, 1000);
    } else {
        btnPlay.textContent = '▶ Пуск';
        btnPlay.style.background = '';
        clearInterval(playInterval);
    }
};

async function loadHistory(ip) {
    const res = await fetch(`/api/history/${ip}`);
        if (!res.ok) {
        console.warn('History fetch failed:', res.status);
        return;
    }
    const data = await res.json();
    if (!data.history || !data.history.length) return;
    const labels = data.history.map(h => `${String(h.sim_hour).padStart(2, '0')}:00`);
    const values = data.history.map(h => h.current_load);
    
    const ctx = document.getElementById('historyChart').getContext('2d');
    
    if (historyChart) historyChart.destroy();
    
    historyChart = new Chart(ctx, {
        type: 'line',
        data: {
            labels: labels,
            datasets: [{
                label: 'Mbps',
                data: values,
                borderColor: '#6cc4ff',
                backgroundColor: 'rgba(108, 196, 255, 0.15)',
                borderWidth: 2,
                fill: true,
                pointRadius: 0,
                tension: 0.3
            }]
        },
        options: {
            responsive: false,
            plugins: { legend: { display: false } },
            scales: {
                x: { ticks: { color: '#888', font: { size: 9 } } },
                y: { ticks: { color: '#888', font: { size: 9 } }, beginAtZero: true }
            }
        }
    });
}
// ---- Старт ----
fetchState();