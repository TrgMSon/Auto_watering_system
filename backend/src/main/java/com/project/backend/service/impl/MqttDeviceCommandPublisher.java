package com.project.backend.service.impl;

import java.time.Instant;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.UUID;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.project.backend.config.MqttGateway;
import com.project.backend.service.MqttCommandAcknowledgmentService;
import org.springframework.beans.factory.ObjectProvider;
import org.springframework.beans.factory.annotation.Autowired;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.messaging.MessagingException;
import org.springframework.stereotype.Component;

import com.project.backend.exception.DeviceCommandException;

@Component
public class MqttDeviceCommandPublisher {
    private static final int MAX_PUBLISH_ATTEMPTS = 3;

    private final String brokerUrl;
    private final String commandTopicTemplate;
    private final String configTopicTemplate;
    private final MqttGateway mqttGateway;
    private final ObjectMapper objectMapper;
    private final MqttCommandAcknowledgmentService acknowledgmentService;

    @Autowired
    public MqttDeviceCommandPublisher(
            ObjectProvider<MqttGateway> mqttGatewayProvider,
            ObjectMapper objectMapper,
            MqttCommandAcknowledgmentService acknowledgmentService,
            @Value("${app.mqtt.broker-url:}") String brokerUrl,
            @Value("${app.mqtt.command.topic:}") String commandTopicTemplate,
            @Value("${app.mqtt.config.topic:}") String configTopicTemplate) {
        this(
                mqttGatewayProvider.getIfAvailable(),
                objectMapper,
                acknowledgmentService,
                brokerUrl,
                commandTopicTemplate,
                configTopicTemplate);
    }

    MqttDeviceCommandPublisher(String brokerUrl, String commandTopicTemplate) {
        this((MqttGateway) null, new ObjectMapper(), null, brokerUrl, commandTopicTemplate,
                "iot/garden/{deviceId}/config");
    }

    MqttDeviceCommandPublisher(
            MqttGateway mqttGateway,
            ObjectMapper objectMapper,
            String brokerUrl,
            String commandTopicTemplate,
            String configTopicTemplate) {
        this(mqttGateway, objectMapper, null, brokerUrl, commandTopicTemplate, configTopicTemplate);
    }

    MqttDeviceCommandPublisher(
            MqttGateway mqttGateway,
            ObjectMapper objectMapper,
            MqttCommandAcknowledgmentService acknowledgmentService,
            String brokerUrl,
            String commandTopicTemplate,
            String configTopicTemplate) {
        this.mqttGateway = mqttGateway;
        this.objectMapper = objectMapper;
        this.acknowledgmentService = acknowledgmentService;
        this.brokerUrl = brokerUrl;
        this.commandTopicTemplate = commandTopicTemplate;
        this.configTopicTemplate = configTopicTemplate;
    }

    public String publishPumpControl(String deviceId, String state) {
        return publishPumpControl(deviceId, state, null);
    }

    public String publishPumpControl(String deviceId, String state, Integer durationSeconds) {
        Map<String, Object> payload = buildPumpControlPayload(deviceId, state, durationSeconds);
        return publish(deviceId, commandTopicTemplate, payload);
    }

    Map<String, Object> buildPumpControlPayload(String deviceId, String state) {
        return buildPumpControlPayload(deviceId, state, null);
    }

    Map<String, Object> buildPumpControlPayload(
            String deviceId, String state, Integer durationSeconds) {
        if (!"ON".equals(state) && !"OFF".equals(state)) {
            throw new IllegalArgumentException("state chỉ nhận giá trị ON hoặc OFF");
        }
        if (durationSeconds != null && durationSeconds < 1) {
            throw new IllegalArgumentException("duration_seconds phải lớn hơn 0");
        }

        Map<String, Object> params = new LinkedHashMap<>();
        params.put("state", state);
        if (durationSeconds != null) {
            params.put("duration_seconds", durationSeconds);
        }

        Map<String, Object> payload = new LinkedHashMap<>();
        payload.put("command_id", "CMD-" + UUID.randomUUID());
        payload.put("device_id", deviceId);
        payload.put("timestamp", Instant.now().toString());
        payload.put("action", "PUMP_CONTROL");
        payload.put("params", params);
        return payload;
    }

    public String publishConfiguration(String deviceId, Map<String, Object> config) {
        Map<String, Object> payload = buildConfigurationPayload(deviceId, config);
        return publish(deviceId, configTopicTemplate, payload);
    }

    Map<String, Object> buildConfigurationPayload(String deviceId, Map<String, Object> config) {
        Map<String, Object> payload = new LinkedHashMap<>();
        payload.put("command_id", "CMD-" + UUID.randomUUID());
        payload.put("device_id", deviceId);
        payload.put("timestamp", Instant.now().toString());
        payload.put("config", new LinkedHashMap<>(config));
        return payload;
    }

    private String publish(String deviceId, String topicTemplate, Map<String, Object> payload) {
        if (brokerUrl == null || brokerUrl.isBlank()) {
            throw new DeviceCommandException("MQTT chưa được cấu hình trên máy chủ");
        }
        if (mqttGateway == null) {
            throw new DeviceCommandException("MQTT chưa được bật hoặc chưa có kết nối gửi bản tin");
        }
        if (deviceId == null || deviceId.isBlank() || deviceId.contains("/")
                || deviceId.contains("+") || deviceId.contains("#")
                || topicTemplate == null || topicTemplate.isBlank()) {
            throw new DeviceCommandException("Cấu hình topic MQTT không hợp lệ");
        }

        String topic = topicTemplate
                .replace("{deviceId}", deviceId)
                .replace("%s", deviceId);
        if (topic.contains("+") || topic.contains("#")) {
            throw new DeviceCommandException("Cấu hình topic MQTT không hợp lệ");
        }

        String commandId = (String) payload.get("command_id");
        if (commandId == null || commandId.isBlank()) {
            commandId = "CMD-" + UUID.randomUUID();
            payload.put("command_id", commandId);
        }

        final String messagePayload;
        try {
            messagePayload = objectMapper.writeValueAsString(payload);
        } catch (JsonProcessingException ex) {
            throw new DeviceCommandException("Không thể tạo bản tin MQTT", ex);
        }

        MessagingException lastFailure = null;
        if (acknowledgmentService != null) {
            acknowledgmentService.register(deviceId, commandId);
        }
        for (int attempt = 1; attempt <= MAX_PUBLISH_ATTEMPTS; attempt++) {
            try {
                mqttGateway.sendToMqtt(messagePayload, topic);
                return commandId;
            } catch (MessagingException ex) {
                lastFailure = ex;
            }
        }
        if (acknowledgmentService != null) {
            acknowledgmentService.markPublishFailed(
                    deviceId, commandId, "Không gửi được bản tin MQTT sau 3 lần thử");
        }
        throw new DeviceCommandException(
                "Không thể gửi bản tin đến thiết bị qua MQTT sau 3 lần thử", lastFailure);
    }
}
