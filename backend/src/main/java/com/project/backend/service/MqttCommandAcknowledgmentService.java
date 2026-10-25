package com.project.backend.service;

import java.time.Duration;
import java.time.Instant;
import java.time.LocalDateTime;
import java.util.Comparator;
import java.util.Map;
import java.util.Optional;
import java.util.concurrent.ConcurrentHashMap;

import com.fasterxml.jackson.core.JsonProcessingException;
import com.fasterxml.jackson.databind.JsonNode;
import com.fasterxml.jackson.databind.ObjectMapper;
import com.project.backend.dto.device.CommandAcknowledgmentResponse;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.stereotype.Service;

@Service
public class MqttCommandAcknowledgmentService {
    private static final Logger logger =
            LoggerFactory.getLogger(MqttCommandAcknowledgmentService.class);
    private static final Duration RETENTION = Duration.ofHours(24);
    private static final int MAX_TRACKED_COMMANDS = 10_000;

    private final ObjectMapper objectMapper;
    private final Map<String, TrackedCommand> commands = new ConcurrentHashMap<>();
    private final WateringHistoryService wateringHistoryService;
    private final WebSocketService webSocketService;

    public MqttCommandAcknowledgmentService(ObjectMapper objectMapper, WateringHistoryService wateringHistoryService, WebSocketService webSocketService) {
        this.objectMapper = objectMapper;
        this.wateringHistoryService = wateringHistoryService;
        this.webSocketService = webSocketService;
    }

    public synchronized void register(String deviceId, String commandId) {
        removeExpiredCommands();
        commands.put(key(deviceId, commandId), new TrackedCommand(
                new CommandAcknowledgmentResponse(
                        commandId, deviceId, "PENDING", null, Instant.now().toString()),
                Instant.now()));
        if (commands.size() > MAX_TRACKED_COMMANDS) {
            commands.entrySet().stream()
                    .min(Comparator.comparing(entry -> entry.getValue().registeredAt()))
                    .ifPresent(oldest -> commands.remove(oldest.getKey(), oldest.getValue()));
        }
    }

    public synchronized void markPublishFailed(String deviceId, String commandId, String message) {
        updateStatus(deviceId, commandId, "PUBLISH_FAILED", message);
    }

    public synchronized Optional<CommandAcknowledgmentResponse> find(
            String deviceId, String commandId) {
        removeExpiredCommands();
        TrackedCommand command = commands.get(key(deviceId, commandId));
        return command == null ? Optional.empty() : Optional.of(command.response());
    }

    public synchronized void handleMessage(String topic, String payload) {
        String deviceId = deviceIdFromTopic(topic);
        if (deviceId == null) {
            logger.warn("Ignoring MQTT acknowledgment with unexpected topic: {}", topic);
            return;
        }

        try {
            JsonNode acknowledgment = objectMapper.readTree(payload);
            if (acknowledgment == null || !acknowledgment.isObject()) {
                logger.warn("Ignoring invalid MQTT acknowledgment on topic {}", topic);
                return;
            }
            String payloadDeviceId = textValue(acknowledgment, "device_id");
            String commandId = textValue(acknowledgment, "command_id");
            String status = textValue(acknowledgment, "status");
            String action = textValue(acknowledgment, "action");
            String message = textValue(acknowledgment, "message");
            if (!deviceId.equals(payloadDeviceId) || commandId == null || status == null) {
                logger.warn("Ignoring invalid MQTT acknowledgment on topic {}", topic);
                return;
            }

            if ("SUCCESS".equalsIgnoreCase(status) || "SUCCEEDED".equalsIgnoreCase(status)) {
                updateStatus(deviceId, commandId, "SUCCEEDED", message);
                if (action.equals("OFF")){
                    webSocketService.changePumpStatus(deviceId, "OFF");
                    wateringHistoryService.recordEndWatering(deviceId, LocalDateTime.now());
                } 
                else if (action.equals("ON")) {
                    webSocketService.changePumpStatus(deviceId, "ON");
                    wateringHistoryService.recordStartWatering(deviceId, LocalDateTime.now());
                }
            } else if ("FAILED".equalsIgnoreCase(status) || "ERROR".equalsIgnoreCase(status)) {
                updateStatus(deviceId, commandId, "FAILED", message);
                webSocketService.sendNotification(message, deviceId);
            } else {
                logger.warn("Ignoring MQTT acknowledgment with unsupported status: {}", status);
            }
        } catch (JsonProcessingException ex) {
            logger.warn("Ignoring malformed MQTT acknowledgment on topic {}", topic, ex);
        }
    }

    private void updateStatus(String deviceId, String commandId, String status, String message) {
        String key = key(deviceId, commandId);
        TrackedCommand current = commands.get(key);
        if (current == null) {
            logger.warn("Ignoring acknowledgment for unknown command {} from device {}",
                    commandId, deviceId);
            return;
        }
        if (!"PENDING".equals(current.response().status())) {
            logger.warn("Ignoring duplicate acknowledgment for command {}", commandId);
            return;
        }
        commands.put(key, new TrackedCommand(
                new CommandAcknowledgmentResponse(
                        commandId, deviceId, status, message, Instant.now().toString()),
                current.registeredAt()));
    }

    private void removeExpiredCommands() {
        Instant expiration = Instant.now().minus(RETENTION);
        commands.entrySet().removeIf(entry -> entry.getValue().registeredAt().isBefore(expiration));
    }

    private String deviceIdFromTopic(String topic) {
        if (topic == null || !topic.startsWith("iot/garden/") || !topic.endsWith("/ack")) {
            return null;
        }
        String deviceId = topic.substring("iot/garden/".length(), topic.length() - "/ack".length());
        return deviceId.isBlank() || deviceId.contains("/") ? null : deviceId;
    }

    private String textValue(JsonNode node, String field) {
        JsonNode value = node.path(field);
        return value.isTextual() ? value.asText() : null;
    }

    private String key(String deviceId, String commandId) {
        return deviceId + ":" + commandId;
    }

    private record TrackedCommand(
            CommandAcknowledgmentResponse response,
            Instant registeredAt) {
    }
}
