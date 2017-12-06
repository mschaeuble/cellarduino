package tk.freeax.sensorserver;

import lombok.Builder;
import lombok.Data;

@Data
@Builder
public class SensorDataDTO {
    private Integer id;
    private String timestamp;
    private float temperature;
    private float humidity;
}
