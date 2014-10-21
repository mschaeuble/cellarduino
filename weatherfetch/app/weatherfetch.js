var weather = require('openweathermap');
var http = require('http');

weather.defaults({units:'metric', lang:'en', mode:'json'});
var options = {
  host: '127.0.0.1',
  port: 3001,
  path: '/sensors/openweathermap/data',
  method: 'PUT',
  headers: {'Content-Type': 'application/json'}
};

weather.now({q:'Zwillikon,CH'}, function(weather) {
  var currentConditions = {};

  currentConditions.temperature = weather.main.temp;
  currentConditions.humidity = weather.main.humidity;

  var request = http.request(options, function(res) {
    console.log('STATUS: ' + res.statusCode);
    console.log('HEADERS: ' + JSON.stringify(res.headers));
    res.setEncoding('utf8');
    res.on('data', function (chunk) {
      console.log('BODY: ' + chunk);
    });
  });

  request.write(JSON.stringify(currentConditions));
  request.end();
});
