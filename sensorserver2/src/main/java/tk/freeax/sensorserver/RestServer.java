package tk.freeax.sensorserver;

import com.google.gson.Gson;
import lombok.extern.slf4j.Slf4j;

import java.util.List;
import java.util.Optional;

import static spark.Spark.*;

@Slf4j
class RestServer {
    private final SensorDataRepository sensorDataRepository;
    private final EventRepository eventRepository;

    public RestServer(SensorDataRepository sensorDataRepository, EventRepository eventRepository) {
        this.sensorDataRepository = sensorDataRepository;
        this.eventRepository = eventRepository;

        Gson gson = new Gson();
        path("/sensors/:sensorId", () -> {
            get("/data", (request, response) -> getSensorData(request.params("sensorId"), request.queryParams("startDate")), gson::toJson);
            get("/latest", (request, response) -> getLatestSensorData(request.params("sensorId")), gson::toJson);
            put("/data", (request, response) -> putSensorData(request.params("sensorId"), gson.fromJson(request.body(), PutSensorDataRequestDTO.class)));
        });

        get("/events", (request, response) -> getEvents(request.queryParams("eventTypes")), gson::toJson);
        put("/events", ((request, response) -> putEvent(gson.fromJson(request.body(), PutEventDTO.class))));
    }

    private List<SensorDataDTO> getSensorData(String sensorId, String startDate) {
        log.info("getSensorData [sensorid={}, startDate={}]", sensorId, startDate);

        if (startDate == null) {
            startDate = "2014-01-01";
        }

        return sensorDataRepository.getSensorData(sensorId, startDate);
    }

    private SensorDataDTO getLatestSensorData(String sensorId) {
        log.info("getLatestSensorData [sensorid={}]", sensorId);

        List<SensorDataDTO> sensorData = sensorDataRepository.getLimitedLatestSensorData(sensorId, 1);
        return sensorData.size() == 1 ? sensorData.get(0) : null;
    }

    private String putSensorData(String sensorId, PutSensorDataRequestDTO requestDTO) {
        log.info("putSensorData [sensorid={}]", sensorId);

        float roundedHumidity = round(requestDTO.getHumidity(), 0);
        float roundedTemperature = round(requestDTO.getTemperature(), 1);

        List<SensorDataDTO> sensorData = sensorDataRepository.getLimitedLatestSensorData(sensorId, 2);
        // Optimization: If latest two records in the database are identical with the current measurement, replace latest from db with latest measurement
        if (sensorData.size() == 2 &&
                sensorData.get(0).getTemperature() == sensorData.get(1).getTemperature() && sensorData.get(1).getTemperature() == roundedTemperature &&
                sensorData.get(0).getHumidity() == sensorData.get(1).getHumidity() && sensorData.get(1).getHumidity() == roundedHumidity) {
            sensorDataRepository.deleteSensorData(sensorData.get(0).getId());
        }

        sensorDataRepository.save(sensorId, roundedTemperature, roundedHumidity);
        return "ok";
    }

    private static float round(float value, int precision) {
        int scale = (int) Math.pow(10, precision);
        return (float) Math.round(value * scale) / scale;
    }

    private List<EventDTO> getEvents(String eventTypesQueryParam) {
        log.info("getEvents [eventTypes={}]", eventTypesQueryParam);

        if (eventTypesQueryParam == null) {
            halt(200, "Please provide an 'eventTypes' query parameter");
        }

        String[] eventTypes = eventTypesQueryParam.split(",");
        return eventRepository.selectEventsOfType(eventTypes);
    }

    private String putEvent(PutEventDTO putEventDTO) {
        log.info("putEvent [eventType={}]", putEventDTO.getEventType());

        Optional<EventDTO> latestEvent = eventRepository.getLatestEvent();
        if (latestEvent.isPresent() && latestEvent.get().getEventType().equals(putEventDTO.getEventType())) {
            log.info("Last event in database is already of type '{}', do not persist anything", putEventDTO.getEventType());
        } else {
            eventRepository.save(putEventDTO.getEventType());
        }
        return "ok";
    }

}
