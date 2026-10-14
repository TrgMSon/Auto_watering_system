package com.project.backend.dto.device;

import com.fasterxml.jackson.annotation.JsonInclude;
import com.fasterxml.jackson.annotation.JsonProperty;

@JsonInclude(JsonInclude.Include.NON_NULL)
public record DeviceConfigResponse(
        @JsonProperty("device_id") String deviceId,
        @JsonProperty("auto_mode") boolean autoMode,
        @JsonProperty("soil_moisture_min") float soilMoistureMin,
        @JsonProperty("soil_moisture_max") float soilMoistureMax,
        @JsonProperty("max_watering_duration") int maxWateringDuration,
        @JsonProperty("air_humidity_min") float airHumidityMin,
        @JsonProperty("air_humidity_max") float airHumidityMax,
        @JsonProperty("air_temperature_min") float airTemperatureMin,
        @JsonProperty("air_temperature_max") float airTemperatureMax,
        @JsonProperty("water_level_min") int waterLevelMin,
        @JsonProperty("command_id") String commandId,
        @JsonProperty("command_status") String commandStatus) {
}
