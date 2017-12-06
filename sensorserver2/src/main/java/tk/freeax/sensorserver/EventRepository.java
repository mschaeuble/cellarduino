package tk.freeax.sensorserver;

import com.querydsl.core.Tuple;
import lombok.extern.slf4j.Slf4j;
import tk.freeax.sensorserver.entities.QEvents;

import java.io.File;
import java.util.List;
import java.util.Optional;
import java.util.stream.Collectors;

@Slf4j
public class EventRepository extends BaseRepository {

    private final QEvents events = QEvents.events;

    public EventRepository(String databaseFile) {
        super("jdbc:sqlite:" + new File(databaseFile).getAbsolutePath());
    }

    public List<EventDTO> selectEventsOfType(String[] eventTypes) {
        List<Tuple> tuples = select(events.timestamp, events.eventType).from(events)
                                                                       .where(events.eventType.in(eventTypes))
                                                                       .orderBy(events.timestamp.asc())
                                                                       .fetch();

        return tuples.stream()
                     .map(this::mapToDto)
                     .collect(Collectors.toList());
    }

    public Optional<EventDTO> getLatestEvent() {
        Tuple tuple = select(events.timestamp, events.eventType).from(events)
                                                                .orderBy(events.timestamp.desc())
                                                                .limit(1)
                                                                .fetchOne();

        return Optional.of(tuple)
                       .map(this::mapToDto);
    }

    public void save(String eventType) {
        insert(events).set(events.eventType, eventType)
                      .execute();
    }

    private EventDTO mapToDto(Tuple tuple) {
        return EventDTO.builder()
                       .eventType(tuple.get(events.eventType))
                       .timestamp(tuple.get(events.timestamp)) // FIXME MSH
                       .build();
    }
}
