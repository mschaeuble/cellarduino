var app = new Vue({
  el: '#app',
  data: {
    indoorTimestamp: null,
    indoorTemperature: null,
    indoorHumidity: null,

    outdoorTimestamp: null,
    outdoorTemperature: null,
    outdoorHumidity: null,
  },
  created: function () {

    this.$http.get('/sensors/indoor/latest').then(response => {
        var sensorDataDTO = response.body;
        this.indoorTimestamp = sensorDataDTO.timestamp;
        this.indoorTemperature = sensorDataDTO.temperature;
        this.indoorHumidity = sensorDataDTO.humidity;
    }, errorResponse => {
       console.error('Error while fetching latest indoor sensor data');
    });

    this.$http.get('/sensors/outdoor/latest').then(response => {
        var sensorDataDTO = response.body;
        this.outdoorTimestamp = sensorDataDTO.timestamp;
        this.outdoorTemperature = sensorDataDTO.temperature;
        this.outdoorHumidity = sensorDataDTO.humidity;
    }, errorResponse => {
       console.error('Error while fetching latest outdoor sensor data');
    });
  }
})

