const API_BASE_URL = 'http://localhost:3000/api';
const UPDATE_INTERVAL = 3000; 
const CHART_DATA_POINTS = 20; // Number of data points to show in charts

let tempChart, humidityChart, airQualityChart;
let updateTimer;

function initCharts() {
    const commonOptions = {
        responsive: true,
        maintainAspectRatio: false,
        plugins: {
            legend: {
                display: false
            }
        },
        scales: {
            x: {
                display: true,
                title: {
                    display: true,
                    text: 'Thời gian'
                }
            },
            y: {
                display: true,
                beginAtZero: false
            }
        }
    };

    const tempCtx = document.getElementById('tempChart').getContext('2d');
    tempChart = new Chart(tempCtx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'Nhiệt độ (°C)',
                data: [],
                borderColor: '#ef4444',
                backgroundColor: 'rgba(239, 68, 68, 0.1)',
                tension: 0.4,
                fill: true
            }]
        },
        options: {
            ...commonOptions,
            scales: {
                ...commonOptions.scales,
                y: {
                    ...commonOptions.scales.y,
                    title: {
                        display: true,
                        text: 'Nhiệt độ (°C)'
                    }
                }
            }
        }
    });

    const humidityCtx = document.getElementById('humidityChart').getContext('2d');
    humidityChart = new Chart(humidityCtx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'Độ ẩm (%)',
                data: [],
                borderColor: '#3b82f6',
                backgroundColor: 'rgba(59, 130, 246, 0.1)',
                tension: 0.4,
                fill: true
            }]
        },
        options: {
            ...commonOptions,
            scales: {
                ...commonOptions.scales,
                y: {
                    ...commonOptions.scales.y,
                    title: {
                        display: true,
                        text: 'Độ ẩm (%)'
                    },
                    min: 0,
                    max: 100
                }
            }
        }
    });

    const airQualityCtx = document.getElementById('airQualityChart').getContext('2d');
    airQualityChart = new Chart(airQualityCtx, {
        type: 'line',
        data: {
            labels: [],
            datasets: [{
                label: 'Chất lượng không khí (V)',
                data: [],
                borderColor: '#10b981',
                backgroundColor: 'rgba(16, 185, 129, 0.1)',
                tension: 0.4,
                fill: true
            }]
        },
        options: {
            ...commonOptions,
            scales: {
                ...commonOptions.scales,
                y: {
                    ...commonOptions.scales.y,
                    title: {
                        display: true,
                        text: 'Voltage (V)'
                    }
                }
            }
        }
    });
}

function updateChart(chart, label, value) {
    if (chart.data.labels.length >= CHART_DATA_POINTS) {
        chart.data.labels.shift();
        chart.data.datasets[0].data.shift();
    }
    
    chart.data.labels.push(label);
    chart.data.datasets[0].data.push(value);
    chart.update('none');
}

async function fetchLatestData() {
    try {
        const response = await fetch(`${API_BASE_URL}/latest`);
        const result = await response.json();
        
        if (result.success && result.data) {
            const data = result.data;
            
            document.getElementById('temp-value').textContent = 
                data.temperature !== null ? data.temperature.toFixed(1) : '--';
            
            document.getElementById('humidity-value').textContent = 
                data.humidity !== null ? data.humidity.toFixed(1) : '--';
            
            document.getElementById('air-quality-value').textContent = 
                data.air_quality !== null ? data.air_quality.toFixed(3) : '--';
            
            const alertElement = document.getElementById('air-alert');
            if (data.air_quality !== null) {
                if (data.air_quality > 1.9) {
                    alertElement.textContent = '⚠️ Chất lượng không khí kém!';
                    alertElement.className = 'alert warning';
                } else {
                    alertElement.textContent = '✅ Chất lượng không khí tốt';
                    alertElement.className = 'alert good';
                }
            } else {
                alertElement.style.display = 'none';
            }
            
            const timestamp = new Date(data.timestamp);
            document.getElementById('last-update').textContent = 
                timestamp.toLocaleString('vi-VN');
            
            const timeLabel = timestamp.toLocaleTimeString('vi-VN');
            if (data.temperature !== null) {
                updateChart(tempChart, timeLabel, data.temperature);
            }
            if (data.humidity !== null) {
                updateChart(humidityChart, timeLabel, data.humidity);
            }
            if (data.air_quality !== null) {
                updateChart(airQualityChart, timeLabel, data.air_quality);
            }
            
            updateStatus(true);
        }
    } catch (error) {
        console.error('Error fetching latest data:', error);
        updateStatus(false);
    }
}

async function fetchStatistics() {
    try {
        const response = await fetch(`${API_BASE_URL}/statistics/24`);
        const result = await response.json();
        
        if (result.success && result.statistics) {
            const stats = result.statistics;
            
            document.getElementById('stat-avg-temp').textContent = 
                stats.avg_temp !== null ? `${stats.avg_temp.toFixed(1)} °C` : '--';
            
            document.getElementById('stat-minmax-temp').textContent = 
                stats.min_temp !== null && stats.max_temp !== null 
                    ? `${stats.min_temp.toFixed(1)} / ${stats.max_temp.toFixed(1)} °C` 
                    : '--';
            
            document.getElementById('stat-avg-humidity').textContent = 
                stats.avg_humidity !== null ? `${stats.avg_humidity.toFixed(1)} %` : '--';
            
            document.getElementById('stat-minmax-humidity').textContent = 
                stats.min_humidity !== null && stats.max_humidity !== null 
                    ? `${stats.min_humidity.toFixed(1)} / ${stats.max_humidity.toFixed(1)} %` 
                    : '--';
        }
    } catch (error) {
        console.error('Error fetching statistics:', error);
    }
}

function updateStatus(connected) {
    const statusElement = document.getElementById('status');
    const statusText = document.getElementById('status-text');
    
    if (connected) {
        statusElement.className = 'status connected';
        statusText.textContent = 'Đang hoạt động';
    } else {
        statusElement.className = 'status disconnected';
        statusText.textContent = 'Mất kết nối';
    }
}

async function loadInitialChartData() {
    try {
        const response = await fetch(`${API_BASE_URL}/recent/${CHART_DATA_POINTS}`);
        const result = await response.json();
        
        if (result.success && result.data && result.data.length > 0) {
            const data = result.data.reverse();
            
            data.forEach(item => {
                const timestamp = new Date(item.timestamp);
                const timeLabel = timestamp.toLocaleTimeString('vi-VN');
                
                tempChart.data.labels.push(timeLabel);
                tempChart.data.datasets[0].data.push(item.temperature);
                
                humidityChart.data.labels.push(timeLabel);
                humidityChart.data.datasets[0].data.push(item.humidity);
                
                airQualityChart.data.labels.push(timeLabel);
                airQualityChart.data.datasets[0].data.push(item.air_quality);
            });
            
            tempChart.update();
            humidityChart.update();
            airQualityChart.update();
        }
    } catch (error) {
        console.error('Error loading initial chart data:', error);
    }
}

function startAutoUpdate() {
    fetchLatestData();
    fetchStatistics();
    
    updateTimer = setInterval(() => {
        fetchLatestData();
    }, UPDATE_INTERVAL);
    
    setInterval(() => {
        fetchStatistics();
    }, 300000);
}

async function init() {    
    initCharts();
    await loadInitialChartData();
    startAutoUpdate();    
}

if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
} else {
    init();
}

window.addEventListener('beforeunload', () => {
    if (updateTimer) {
        clearInterval(updateTimer);
    }
});
