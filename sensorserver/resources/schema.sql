CREATE TABLE data(
  id INTEGER PRIMARY KEY,
  timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  sensor_id TEXT NOT NULL,
  temperature REAL NOT NULL,
  humidity REAL NOT NULL
);
