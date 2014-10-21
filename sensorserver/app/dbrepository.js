var sqlite3 = require('sqlite3').verbose();
var db = new sqlite3.Database('../resources/data.db');

exports.getSensorData = function(sensorId, callback) {
  var selectStmt = db.prepare("SELECT * FROM data WHERE sensor_id = ? ORDER BY timestamp");
  selectStmt.all(sensorId, function(err, rows) {
    console.log("  returning %d row(s)", rows.length);
    callback(rows);
  });  
}

exports.getLatestSensorData = function(sensorId, callback) {
  var selectStmt = db.prepare("SELECT * FROM data WHERE sensor_id = ? ORDER BY timestamp desc LIMIT 1");
  selectStmt.get(sensorId, function(err, row) {
    console.log("  returned: %j", row);
    callback(row);
  });  
}

exports.saveSensorData = function(sensorId, temperature, humidity) {
  var insertStmt = db.prepare("INSERT INTO data(sensor_id, temperature, humidity) VALUES (?,?,?)");
  insertStmt.run(sensorId, temperature, humidity);
  insertStmt.finalize();  

  console.log("  persisted: %s, %s, %s", sensorId, temperature, humidity);
}
