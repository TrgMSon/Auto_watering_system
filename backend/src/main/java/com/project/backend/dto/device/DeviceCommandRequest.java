package com.project.backend.dto.device;

import com.fasterxml.jackson.annotation.JsonProperty;

import jakarta.validation.constraints.Max;
import jakarta.validation.constraints.Min;
import jakarta.validation.constraints.NotBlank;
import lombok.Data;

@Data
public class DeviceCommandRequest {
    @NotBlank
    private String action;

    @Min(1)
    @Max(86400)
    @JsonProperty("duration_seconds")
    private Integer durationSeconds;
}
