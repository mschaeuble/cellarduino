CREATE TABLE data(
  id INTEGER PRIMARY KEY,
  timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL,
  sensor_id TEXT NOT NULL,
  temperature REAL NOT NULL,
  humidity REAL NOT NULL
);

CREATE TABLE events(
  id INTEGER PRIMARY KEY,
  timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP NOT NULL,
  event_type TEXT NOT NULL
);