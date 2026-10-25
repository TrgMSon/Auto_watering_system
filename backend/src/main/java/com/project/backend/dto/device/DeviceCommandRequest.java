package com.project.backend.dto.device;

import com.fasterxml.jackson.annotation.JsonProperty;

import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.Min;
import lombok.Data;

@Data
public class DeviceCommandRequest {
    @NotBlank
    private String action;

    @Min(1)
    @JsonProperty("duration_seconds")
    private Integer durationSeconds;
}
