package com.project.backend.dto.device;

import com.fasterxml.jackson.annotation.JsonProperty;

import jakarta.validation.constraints.DecimalMax;
import jakarta.validation.constraints.DecimalMin;
import jakarta.validation.constraints.Min;
import jakarta.validation.constraints.NotNull;
import lombok.Data;

@Data
public class DeviceConfigRequest {
    @NotNull
    @JsonProperty("auto_mode")
    private Boolean autoMode;

    @NotNull
    @DecimalMin("0")
    @DecimalMax("100")
    @JsonProperty("soil_moisture_min")
    private Float soilMoistureMin;

    @NotNull
    @DecimalMin("0")
    @DecimalMax("100")
    @JsonProperty("soil_moisture_max")
    private Float soilMoistureMax;

    @NotNull
    @Min(1)
    @JsonProperty("max_watering_duration")
    private Integer maxWateringDuration;

    @NotNull
    @DecimalMin("0")
    @DecimalMax("100")
    @JsonProperty("air_humidity_min")
    private Float airHumidityMin;

    @NotNull
    @DecimalMin("0")
    @DecimalMax("100")
    @JsonProperty("air_humidity_max")
    private Float airHumidityMax;

    @NotNull
    @JsonProperty("air_temperature_min")
    private Float airTemperatureMin;

    @NotNull
    @JsonProperty("air_temperature_max")
    private Float airTemperatureMax;
}
