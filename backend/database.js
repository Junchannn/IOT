const sqlite3 = require('sqlite3').verbose();
const path = require('path');

class DatabaseManager {
    constructor(dbPath) {
        this.db = new sqlite3.Database(dbPath, (err) => {
            if (err) {
                console.error('Database connection error:', err);
            } else {
                console.log('Connected to SQLite database');
            }
        });
        this.initDatabase();
    }

    initDatabase() {
        this.db.run(`
            CREATE TABLE IF NOT EXISTS sensor_data (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                temperature REAL,
                humidity REAL,
                air_quality REAL,
                timestamp DATETIME DEFAULT CURRENT_TIMESTAMP
            )
        `, (err) => {
            if (err) {
                console.error('Error creating table:', err);
            } else {
                // Create index only after table is created
                this.db.run(`
                    CREATE INDEX IF NOT EXISTS idx_timestamp 
                    ON sensor_data(timestamp DESC)
                `, (err) => {
                    if (err) {
                        console.error('Error creating index:', err);
                    } else {
                        console.log('Database initialized');
                    }
                });
            }
        });
    }

    insertSensorData(temperature, humidity, airQuality) {
        return new Promise((resolve, reject) => {
            const stmt = this.db.prepare(`
                INSERT INTO sensor_data (temperature, humidity, air_quality)
                VALUES (?, ?, ?)
            `);
            
            stmt.run([temperature, humidity, airQuality], function(err) {
                if (err) {
                    reject(err);
                } else {
                    resolve({ id: this.lastID, changes: this.changes });
                }
            });
            
            stmt.finalize();
        });
    }

    getLatestData() {
        return new Promise((resolve, reject) => {
            this.db.get(`
                SELECT * FROM sensor_data 
                ORDER BY timestamp DESC 
                LIMIT 1
            `, (err, row) => {
                if (err) {
                    reject(err);
                } else {
                    resolve(row);
                }
            });
        });
    }

    getRecentData(limit = 100) {
        return new Promise((resolve, reject) => {
            this.db.all(`
                SELECT * FROM sensor_data 
                ORDER BY timestamp DESC 
                LIMIT ?
            `, [limit], (err, rows) => {
                if (err) {
                    reject(err);
                } else {
                    resolve(rows);
                }
            });
        });
    }

    getDataByTimeRange(startTime, endTime) {
        return new Promise((resolve, reject) => {
            this.db.all(`
                SELECT * FROM sensor_data 
                WHERE timestamp BETWEEN ? AND ?
                ORDER BY timestamp DESC
            `, [startTime, endTime], (err, rows) => {
                if (err) {
                    reject(err);
                } else {
                    resolve(rows);
                }
            });
        });
    }

    getStatistics(hours = 24) {
        return new Promise((resolve, reject) => {
            this.db.get(`
                SELECT 
                    AVG(temperature) as avg_temp,
                    MIN(temperature) as min_temp,
                    MAX(temperature) as max_temp,
                    AVG(humidity) as avg_humidity,
                    MIN(humidity) as min_humidity,
                    MAX(humidity) as max_humidity,
                    AVG(air_quality) as avg_air_quality,
                    MAX(air_quality) as max_air_quality,
                    COUNT(*) as total_records
                FROM sensor_data
                WHERE timestamp >= datetime('now', '-' || ? || ' hours')
            `, [hours], (err, row) => {
                if (err) {
                    reject(err);
                } else {
                    resolve(row);
                }
            });
        });
    }

    deleteOldData(days = 30) {
        return new Promise((resolve, reject) => {
            this.db.run(`
                DELETE FROM sensor_data 
                WHERE timestamp < datetime('now', '-' || ? || ' days')
            `, [days], function(err) {
                if (err) {
                    reject(err);
                } else {
                    console.log(`🗑️  Deleted ${this.changes} old records`);
                    resolve(this.changes);
                }
            });
        });
    }

    close() {
        this.db.close((err) => {
            if (err) {
                console.error('Error closing database:', err);
            } else {
                console.log('Database connection closed');
            }
        });
    }
}

module.exports = DatabaseManager;
