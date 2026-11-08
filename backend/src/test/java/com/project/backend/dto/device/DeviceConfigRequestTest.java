package com.project.backend.dto.device;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertTrue;

import java.util.Set;

import jakarta.validation.ConstraintViolation;
import jakarta.validation.Validation;
import jakarta.validation.Validator;

import org.junit.jupiter.api.Test;

class DeviceConfigRequestTest {
    private final Validator validator = Validation.buildDefaultValidatorFactory().getValidator();

    @Test
    void acceptsFirmwareSupportedWaterLevelAndDurationBoundaries() {
        DeviceConfigRequest request = validRequest();
        request.setWaterLevelMin(20);
        request.setMaxWateringDuration(1);
        assertTrue(validator.validate(request).isEmpty());

        request.setWaterLevelMin(100);
        request.setMaxWateringDuration(10);
        assertTrue(validator.validate(request).isEmpty());
    }

    @Test
    void rejectsWaterLevelAndDurationOutsideFirmwareRanges() {
        DeviceConfigRequest request = validRequest();
        request.setWaterLevelMin(19);
        request.setMaxWateringDuration(11);

        Set<ConstraintViolation<DeviceConfigRequest>> violations = validator.validate(request);

        assertEquals(2, violations.size());
    }

    private DeviceConfigRequest validRequest() {
        DeviceConfigRequest request = new DeviceConfigRequest();
        request.setAutoMode(true);
        request.setSoilMoistureMin(30f);
        request.setSoilMoistureMax(70f);
        request.setMaxWateringDuration(5);
        request.setAirHumidityMin(40f);
        request.setAirHumidityMax(80f);
        request.setAirTemperatureMin(10f);
        request.setAirTemperatureMax(35f);
        request.setWaterLevelMin(50);
        return request;
    }
}
