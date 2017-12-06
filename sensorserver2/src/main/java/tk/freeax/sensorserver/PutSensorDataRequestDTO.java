package tk.freeax.sensorserver;

import lombok.Data;

@Data
public class PutSensorDataRequestDTO {
    private float temperature;
    private float humidity;
}
