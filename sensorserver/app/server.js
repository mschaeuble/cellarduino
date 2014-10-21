var restify = require('restify');
var dbrepository  = require('./dbrepository');

var server = restify.createServer({
  name: 'SensorServer'
});

server.use(restify.bodyParser());
server.pre(restify.pre.userAgentConnection());

server.get('/sensors/:sensorid/data', getSensorData);
server.get('/sensors/:sensorid/latest', getLatestSensorData);
server.put('/sensors/:sensorid/data', putSensorData);

server.listen(3001, function() {
  console.log('%s listening at %s', server.name, server.url);
});


function getSensorData(req, res, next) {
  console.log('getSensorData [sensorid=%s]', req.params.sensorid);

  dbrepository.getSensorData(req.params.sensorid, function(rows) {
    var output = transformToJson(rows); 
    res.json(200, output);
    next();
  });

}

function getLatestSensorData(req, res, next) {
  console.log('getLatestSensorData [sensorid=%s]', req.params.sensorid);

  dbrepository.getLatestSensorData(req.params.sensorid, function(row) {
    var output = transformToJson([row])
    res.json(200, output);
    next();
  });
}

function transformToJson(rows) {
  var data = [];

  rows.forEach(function(row) {
    data.push({
      "timestamp": row.timestamp,
      "temperature": row.temperature,
      "humidity": row.humidity
    });
  });

  return { "data" : data };
}

function putSensorData(req, res, next) {
  console.log('putSensorData [sensorid=%s]', req.params.sensorid);
 
  dbrepository.saveSensorData(req.params.sensorid, req.body.temperature, req.body.humidity);

  res.send(204);
  next();
}
