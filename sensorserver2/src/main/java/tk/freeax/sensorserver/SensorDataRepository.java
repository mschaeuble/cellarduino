package tk.freeax.sensorserver;

import com.querydsl.core.Tuple;
import lombok.extern.slf4j.Slf4j;
import tk.freeax.sensorserver.entities.QData;

import java.io.File;
import java.util.List;
import java.util.stream.Collectors;

@Slf4j
public class SensorDataRepository extends BaseRepository {

    private final QData data = QData.data;

    public SensorDataRepository(String databaseFile) {
        super("jdbc:sqlite:" + new File(databaseFile).getAbsolutePath());
    }

    public List<SensorDataDTO> getSensorData(String sensorId, String startDate) {
        List<Tuple> tuples = select(data.id, data.timestamp, data.humidity, data.temperature).from(data)
                                                                                             .where(data.sensorId.eq(sensorId).and(data.timestamp.goe(startDate)))
                                                                                             .orderBy(data.timestamp.asc())
                                                                                             .fetch();
        return mapToDtos(tuples);
    }

    public List<SensorDataDTO> getLimitedLatestSensorData(String sensorId, long limit) {
        List<Tuple> tuples = select(data.id, data.timestamp, data.humidity, data.temperature).from(data)
                                                                                             .where(data.sensorId.eq(sensorId))
                                                                                             .orderBy(data.timestamp.desc())
                                                                                             .limit(limit)
                                                                                             .fetch();

        return mapToDtos(tuples);
    }

    public void deleteSensorData(Integer id) {
        delete(data).where(data.id.eq(id))
                    .execute();
    }

    public void save(String sensorId, float temperature, float humidity) {
        insert(data).set(data.sensorId, sensorId)
                    .set(data.temperature, temperature)
                    .set(data.humidity, humidity)
                    .execute();
    }

    private List<SensorDataDTO> mapToDtos(List<Tuple> tuples) {
        return tuples.stream()
                     .map(this::mapToDto)
                     .collect(Collectors.toList());
    }

    private SensorDataDTO mapToDto(Tuple tuple) {
        return SensorDataDTO.builder()
                            .id(tuple.get(data.id))
                            .humidity(tuple.get(data.humidity))
                            .temperature(tuple.get(data.temperature))
                            .timestamp(tuple.get(data.timestamp)) // FIXME MSH?
                            .build();
    }

}
