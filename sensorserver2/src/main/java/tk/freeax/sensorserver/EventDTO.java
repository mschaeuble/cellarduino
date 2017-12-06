package tk.freeax.sensorserver;

import lombok.Builder;
import lombok.Data;

@Data
@Builder
class EventDTO {
    private String timestamp;
    private String eventType;
}
