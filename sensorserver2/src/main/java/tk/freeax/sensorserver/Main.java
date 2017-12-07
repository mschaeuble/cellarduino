package tk.freeax.sensorserver;

import lombok.extern.slf4j.Slf4j;

import static spark.Spark.staticFiles;

@Slf4j
public class Main {

    public static void main(String[] args) {
        if (args.length <= 0 || args.length > 1) {
            log.info("Usage: java tk.freeax.sensorserver.Main DATABASE_FILE");
            System.exit(0);
        }

        staticFiles.location("/app");

        SensorDataRepository sensorDataRepository = new SensorDataRepository(args[0]);
        EventRepository eventRepository = new EventRepository(args[0]);
        new RestServer(sensorDataRepository, eventRepository);
    }

}
