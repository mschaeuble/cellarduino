var http = require('http');

var options = {
  host: '127.0.0.1',
  port: 3001,
  path: '/sensors/swissmetnet/data',
  method: 'PUT',
  headers: {'Content-Type': 'application/json'}
};


http.get("http://data.netcetera.com/smn/smn/SMA", function(res) {
  
    var body = '';

    res.on('data', function(chunk) {
      body += chunk;
    });

    res.on('end', function() {
      var data = JSON.parse(body);
      var temperature = parseFloat(data.temperature);
      var humidity = parseFloat(data.humidity);
      sendWeatherDataToServer(temperature, humidity);
    });


  }).on('error', function(e) {
    console.log("Got error: " + e.message);
});

function sendWeatherDataToServer(temperature, humidity) {
  var currentConditions = {};
  currentConditions.temperature = temperature;
  currentConditions.humidity = humidity;

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
}
