$(function () {

        $.getJSON('/sensors/indoor/latest', function (sensorDataDTO) {
            $("#indoorTimestamp").text(moment(sensorDataDTO.timestamp + "+0000").format('DD.MM.YYYY hh:mm:ss'));
            $("#indoorTemperature").text(sensorDataDTO.temperature);
            $("#indoorHumidity").text(sensorDataDTO.humidity);
        });

        $.getJSON('/sensors/outdoor/latest', function (sensorDataDTO) {
            $("#outdoorTimestamp").text(moment(sensorDataDTO.timestamp + "+0000").format('DD.MM.YYYY hh:mm:ss'));
            $("#outdoorTemperature").text(sensorDataDTO.temperature);
            $("#outdoorHumidity").text(sensorDataDTO.humidity);
        });


        var chartOptions = {
            yaxes: [{
                minTickSize: 0.5
            }, {
                position: "right",
                min: 0,
                max: 100,
                tickSize: 10
            }],
            xaxis: {
                mode: "time",
                timezone: "browser"
            },
            grid: {
                markings: [{
                    y2axis: {
                        from: 65,
                        to: 65
                    },
                    color: "#FFE2CF"
                }, {
                    y2axis: {
                        from: 70,
                        to: 70
                    },
                    color: "#FF8585"
                }],
                backgroundColor: {
                    colors: ["#FFF", "#F5F5F5"]
                }
            },
            legend: {
                show: true,
                position: "nw"
            }
        };

        var data = [{
            label: "Indoor: Temperature",
            color: 2,
            yaxis: 1
        }, {
            label: "Indoor: Humidity",
            color: 3,
            yaxis: 2
        }, {
            label: "Outdoor: Temperature",
            color: 4,
            yaxis: 1
        }, {
            label: "Outdoor: Humidity",
            color: 5,
            yaxis: 2
        }];

        var chart = $.plot($("#chart"), data, chartOptions);

        loadData();

        function loadData() {
            $.getJSON('/events?eventTypes=FLAPS_OPEN,FLAPS_CLOSE', function (eventDtos) {
                var markings = createMarkings(eventDtos);
                Array.prototype.unshift.apply(chart.getOptions().grid.markings, markings); // unshift = "add as first element" in order to not hide the critical humidity border lines
            });

            var startDateParameter = determineStartDateParameter();

            var indoorDataUrl = "/sensors/indoor/data";
            var outdoorDataUrl = "/sensors/outdoor/data";
            if (startDateParameter) {
                indoorDataUrl += "?startDate=" + startDateParameter;
                outdoorDataUrl += "?startDate=" + startDateParameter;
            }

            $.getJSON(indoorDataUrl, function (sensorDataDTO) {
                data[0].data = sensorDataDTO.map(temperatureMapper);
                data[1].data = sensorDataDTO.map(humidityMapper);

                chart.setData(data);
                chart.setupGrid();
                chart.draw();
            });

            $.getJSON(outdoorDataUrl, function (sensorDataDTO) {
                data[2].data = sensorDataDTO.map(temperatureMapper);
                data[3].data = sensorDataDTO.map(humidityMapper);

                chart.setData(data);
                chart.setupGrid();
                chart.draw();
            });

        }

        $("#scope").change(function () {
            switch ($('option:selected', this).val()) {
                case "twentyfourhours":
                    chart.getOptions().xaxes[0].min = moment().subtract(24, 'hours').valueOf();
                    chart.getOptions().xaxes[0].max = moment().valueOf();
                    break;
                case "lastweek":
                    chart.getOptions().xaxes[0].min = moment().subtract(7, 'days').valueOf();
                    chart.getOptions().xaxes[0].max = moment().valueOf();
                    break;
                case "whole":
                    chart.getOptions().xaxes[0].min = null;
                    chart.getOptions().xaxes[0].max = null;
                    break;
            }
            loadData();
        });

        function determineStartDateParameter() {
            var daysToSubtract = null;
            switch ($('option:selected').val()) {
                case "twentyfourhours":
                    daysToSubtract = 1;
                    break;
                case "lastweek":
                    daysToSubtract = 7;
                    break;
                case "whole":
                    return null;
            }

            var startDate = new Date();
            startDate.setDate(startDate.getDate() - daysToSubtract);
            return startDate.toISOString().substring(0, 10); // format: 2014-12-01
        }

        function createMarkings(events) {
            var markings = [];

            var fromDate = null;
            for (var i = 0; i < events.length; i++) {
                var currentEvent = events[i];

                if (fromDate == null) {
                    // we are looking for start event "FLAPS_CLOSE"
                    if (currentEvent.eventType == "FLAPS_CLOSE") {
                        fromDate = currentEvent.timestamp;
                    }
                } else {
                    // we are looking for a closing event "FLAPS_OPEN"
                    if (currentEvent.eventType == "FLAPS_OPEN") {
                        var toDate = currentEvent.timestamp;
                        markings.push(createMarking(fromDate, toDate));
                        fromDate = null;
                    }
                }
            }

            if (fromDate != null) {
                // flap is currently still open
                markings.push(createMarking(fromDate, new Date()));
            }

            return markings;
        }

        function createMarking(fromDateString, toDateString) {
            var fromDate = new Date(fromDateString + " UTC");
            var toDate = new Date(toDateString + " UTC");

            return {
                xaxis: {
                    from: fromDate,
                    to: toDate
                },
                color: "#FFD9D9"
            };
        }

        var temperatureMapper = function (climate) {
            return [moment(climate.timestamp + "+0000").valueOf(), climate.temperature];
        };

        var humidityMapper = function (climate) {
            return [moment(climate.timestamp + "+0000").valueOf(), climate.humidity];
        };
    }
);
