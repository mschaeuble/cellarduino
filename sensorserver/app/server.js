var restify = require('restify');
var dbrepository  = require('./dbrepository');

var server = restify.createServer({
  name: 'SensorServer'
});

server.use(restify.bodyParser());
server.use(restify.queryParser());
server.pre(restify.pre.userAgentConnection());

server.get('/sensors/:sensorid/data', getSensorData);
server.get('/sensors/:sensorid/latest', getLatestSensorData);
server.put('/sensors/:sensorid/data', putSensorData);

server.get('/events', getEvents);
server.put('/events', putEvent);

server.listen(3001, function() {
  console.log('%s listening at %s', server.name, server.url);
});


function getSensorData(req, res, next) {
  console.log('getSensorData [sensorid=%s]', req.params.sensorid);

  dbrepository.getSensorData(req.params.sensorid, function(rows) {
    var output;
    if (rows.length > 0) {
      output = transformToJson(rows);
    } else {
      output = "{}";
    }
    res.json(200, output);
    next();
  });

}

function getLatestSensorData(req, res, next) {
  console.log('getLatestSensorData [sensorid=%s, format=%s]', req.params.sensorid, req.params.format);

  dbrepository.getLatestSensorData(req.params.sensorid, function(row) {
    if (row == undefined) {
      res.json(200, "{}");
    } else {
      if (req.params.format == "arduino") {
        var output = "T" + [row][0].temperature + ";H" + [row][0].humidity;
        res.writeHead(200, {
          'Content-Length': Buffer.byteLength(output),
          'Content-Type': 'text/plain'
        });
        res.write(output);
      } else {
        var output = transformToJson([row])
        res.json(200, output);
      }
    }
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

  var roundedTemperature = (req.body.temperature).toFixed(1);
  var roundedHumidity = Math.round(req.body.humidity); // round to int

  dbrepository.getLimitedLatestSensorData(req.params.sensorid, 2, function(rows) {
    // Optimization: If latest two records in the database are identical with the current
    // measurement, replace latest from db with latest measurement
    if (rows.length == 2 &&
        rows[0].temperature == rows[1].temperature && rows[1].temperature == roundedTemperature &&
	rows[0].humidity == rows[1].humidity && rows[1].humidity == roundedHumidity) {
       dbrepository.deleteSensorData(rows[0].id);
    }
    
    dbrepository.saveSensorData(req.params.sensorid, roundedTemperature, roundedHumidity);
  });

  res.send(204);
  next();
}

function getEvents(req, res, next) {
  console.log('getEvents [eventTypes=%s]', req.params.eventTypes);
  
  if (!req.params.eventTypes) {
    res.json(200, "Please provide an 'eventTypes' parameter");
    next();
    return;  
  }

  var eventTypes = req.params.eventTypes.split(",");

  dbrepository.getEvents(eventTypes, function(rows) {
    var output;
    if (rows.length > 0) {
      output = transformEventsToJson(rows);
    } else {
      output = "{}";
    }
    res.json(200, output);
    next();
  });
}

function transformEventsToJson(rows) {
  var data = [];

  rows.forEach(function(row) {
    data.push({
      "timestamp": row.timestamp,
      "eventType": row.event_type
    });
  });

  return data;
}

function putEvent(req, res, next) {
  var eventType = req.body.eventType;
  console.log('putEvent [eventType=%s]', eventType);

  if (!eventType) {
    res.json(200, "Please provide an 'eventType'");
    next();
    return;  
  }

  dbrepository.getLatestEvent(function(row) {
    if (row == undefined || row.event_type != eventType) {
      dbrepository.saveEvent(eventType);
    } else {
      console.log("Last event in database is already of same type, do not persist anything");
    }
  });
  
  res.send(204);
  next();
}
