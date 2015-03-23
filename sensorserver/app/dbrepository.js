var sqlite3 = require('sqlite3').verbose();
var db = new sqlite3.Database('../resources/data.db');

exports.getSensorData = function(sensorId, callback) {
  var selectStmt = db.prepare("SELECT * FROM data WHERE sensor_id = ? ORDER BY timestamp");
  selectStmt.all(sensorId, function(err, rows) {
    console.log("  returning %d row(s)", rows.length);
    callback(rows);
  });  
};

exports.getLatestSensorData = function(sensorId, callback) {
  var selectStmt = db.prepare("SELECT * FROM data WHERE sensor_id = ? ORDER BY timestamp desc LIMIT 1");
  selectStmt.get(sensorId, function(err, row) {
    console.log("  returned: %j", row);
    callback(row);
  });  
};

exports.getLimitedLatestSensorData = function(sensorId, limit, callback) {
  var selectStmt = db.prepare("SELECT * FROM data WHERE sensor_id = ? ORDER BY timestamp desc LIMIT ?");
  selectStmt.all(sensorId, limit, function(err, rows) {
    console.log("  returning: %d", rows.length);
    callback(rows);
  });  
};

exports.saveSensorData = function(sensorId, temperature, humidity) {
  var insertStmt = db.prepare("INSERT INTO data(sensor_id, temperature, humidity) VALUES (?,?,?)");
  insertStmt.run(sensorId, temperature, humidity);
  insertStmt.finalize();  

  console.log("  persisted: %s, %s, %s", sensorId, temperature, humidity);
};

exports.getEvents = function(eventTypes, callback) {
  var params = [];
  for (var i = 1; i <= eventTypes.length; i++) {
    params.push('$'+i);
  }

  var selectStmt = db.prepare("SELECT * FROM events WHERE event_type in (" + params.join(",") + ") ORDER BY timestamp");
  selectStmt.all(eventTypes, function(err, rows) {
    console.log("  returning %d row(s)", rows.length);
    callback(rows);
  });
};

exports.getLatestEvent = function(callback) {
  var selectStmt = db.prepare("SELECT * FROM events ORDER BY timestamp desc LIMIT 1");
  selectStmt.get(function(err, row) {
    console.log("  latest event: %s", JSON.stringify(row));
    callback(row);
  });  
};


exports.saveEvent = function(eventType) {
  var insertStmt = db.prepare("INSERT INTO events(event_type) VALUES (?)");
  insertStmt.run(eventType);
  insertStmt.finalize();  

  console.log("  persisted: %s", eventType);
};

exports.deleteSensorData = function(recordId) {
  console.log("  deleting record with id %s", recordId);
  var deleteStmt = db.prepare("DELETE FROM data WHERE id = ?");
  deleteStmt.run(recordId);
  deleteStmt.finalize();
};
