package com.project.backend.dto.device;

import java.time.LocalTime;

import com.fasterxml.jackson.annotation.JsonProperty;

public record ScheduleResponse(
        int id,
        @JsonProperty("device_id") String deviceId,
        @JsonProperty("start_time") LocalTime startTime,
        @JsonProperty("end_time") LocalTime endTime,
        @JsonProperty("duration_minutes") int durationMinutes,
        @JsonProperty("days_of_week") String daysOfWeek,
        @JsonProperty("is_enabled") boolean enabled) {
}
