package com.project.backend.dto.device;

import java.time.LocalTime;

import com.fasterxml.jackson.annotation.JsonProperty;

import jakarta.validation.constraints.Min;
import jakarta.validation.constraints.NotBlank;
import jakarta.validation.constraints.NotNull;
import lombok.Data;

@Data
public class ScheduleRequest {
    @NotNull
    @JsonProperty("start_time")
    private LocalTime startTime;

    @NotNull
    @JsonProperty("end_time")
    private LocalTime endTime;

    @NotNull
    @Min(1)
    @JsonProperty("duration_minutes")
    private Integer durationMinutes;

    @NotBlank
    @JsonProperty("days_of_week")
    private String daysOfWeek;

    @JsonProperty("is_enabled")
    private boolean enabled = true;
}
