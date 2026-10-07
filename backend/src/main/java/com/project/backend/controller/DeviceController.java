package com.project.backend.controller;

import java.util.HashMap;
import java.util.Map;

import org.springframework.web.bind.annotation.CrossOrigin;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.PutMapping;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestParam;
import org.springframework.web.bind.annotation.RestController;

@RestController
@RequestMapping("/api/devices/{id}")
@CrossOrigin(origins = "*") // đang test local nên cho truy cập không xác thực
public class DeviceController {
    // 1. Mock API Lấy thông số cấu hình ngưỡng tưới (GET)
    @GetMapping("/config")
    public Map<String, Object> getDeviceConfig(@PathVariable String id) {
        Map<String, Object> config = new HashMap<>();
        config.put("device_id", id);
        config.put("auto_mode", true);
        config.put("soil_moisture_min", 35.0);
        config.put("soil_moisture_max", 70.0);
        config.put("max_watering_duration", 10);
        return config; // Trả về JSON mock ngay lập tức
    }

    // 2. Mock API Cập nhật ngưỡng tưới (PUT) -> Đúng API đã chốt
    @PutMapping("/config")
    public Map<String, Object> updateDeviceConfig(
            @PathVariable String id,
            @RequestBody Map<String, Object> requestBody) {

        Map<String, Object> response = new HashMap<>();
        response.put("status", "SUCCESS");
        response.put("message", "Cấu hình ngưỡng tưới đã được cập nhật thành công!");
        response.put("updated_config", requestBody);
        return response;
    }

    // 3. Mock API Điều khiển bật/tắt bơm thủ công (POST)
    @PostMapping("/control")
    public Map<String, Object> controlPump(
            @PathVariable String id,
            @RequestParam String action) {

        Map<String, Object> response = new HashMap<>();
        response.put("device_id", id);
        response.put("action", action); // ON / OFF
        response.put("status", "COMMAND_SENT_VIA_MQTT");
        return response;
    }

    // 4. Mock API bật/tắt chế độ tưới tự động (POST)
    // mode: auto or manual
    @PostMapping("/mode")
    public Map<String, Object> setModeWatering(
            @PathVariable String id,
            @RequestParam String mode) {

        Map<String, Object> response = new HashMap<>();
        response.put("device_id", id);
        response.put("mode", mode);

        return response;
    }

    // 5. Mock API Thêm lịch tưới tự động mới (POST)
    @PutMapping("/schedules")
    public Map<String, Object> updateWateringSchedule(
            @PathVariable String id,
            @RequestParam String timeStart,
            @RequestParam String timeEnd) {

        Map<String, Object> response = new HashMap<>();
        response.put("device_id", id);
        response.put("time_start", timeStart);
        response.put("time_end", timeEnd);

        return response;
    }

}
