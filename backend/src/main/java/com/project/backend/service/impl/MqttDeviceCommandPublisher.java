package com.project.backend.service.impl;

import java.nio.charset.StandardCharsets;
import java.util.UUID;

import org.eclipse.paho.client.mqttv3.MqttClient;
import org.eclipse.paho.client.mqttv3.MqttConnectOptions;
import org.eclipse.paho.client.mqttv3.MqttException;
import org.eclipse.paho.client.mqttv3.MqttMessage;
import org.eclipse.paho.client.mqttv3.persist.MemoryPersistence;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Component;

import com.project.backend.exception.DeviceCommandException;

import jakarta.annotation.PreDestroy;

@Component
public class MqttDeviceCommandPublisher {
    private final String brokerUrl;
    private final String username;
    private final String password;
    private final String topicTemplate;
    private MqttClient client;

    public MqttDeviceCommandPublisher(
            @Value("${app.mqtt.broker-url:}") String brokerUrl,
            @Value("${app.mqtt.username:}") String username,
            @Value("${app.mqtt.password:}") String password,
            @Value("${app.mqtt.control-topic:devices/%s/control}") String topicTemplate) {
        this.brokerUrl = brokerUrl;
        this.username = username;
        this.password = password;
        this.topicTemplate = topicTemplate;
    }

    public synchronized void publish(String deviceId, String payload) {
        if (brokerUrl.isBlank()) {
            throw new DeviceCommandException("MQTT chưa được cấu hình trên máy chủ");
        }
        if (topicTemplate.isBlank() || deviceId.contains("+") || deviceId.contains("#")) {
            throw new DeviceCommandException("Cấu hình topic MQTT không hợp lệ");
        }

        try {
            MqttClient mqttClient = connectedClient();
            String topic = topicTemplate
                    .replace("{deviceId}", deviceId)
                    .replace("%s", deviceId);
            MqttMessage message = new MqttMessage(payload.getBytes(StandardCharsets.UTF_8));
            message.setQos(1);
            mqttClient.publish(topic, message);
        } catch (MqttException ex) {
            throw new DeviceCommandException("Không thể gửi lệnh đến thiết bị qua MQTT", ex);
        }
    }

    private MqttClient connectedClient() throws MqttException {
        if (client == null || !client.isConnected()) {
            if (client == null) {
                client = new MqttClient(
                        brokerUrl,
                        "watering-api-" + UUID.randomUUID(),
                        new MemoryPersistence());
            }

            MqttConnectOptions options = new MqttConnectOptions();
            options.setAutomaticReconnect(true);
            options.setCleanSession(true);
            options.setConnectionTimeout(10);
            if (!username.isBlank()) {
                options.setUserName(username);
                options.setPassword(password.toCharArray());
            }
            client.connect(options);
        }
        return client;
    }

    @PreDestroy
    public synchronized void close() {
        if (client != null) {
            try {
                if (client.isConnected()) {
                    client.disconnect();
                }
                client.close();
            } catch (MqttException ex) {
                throw new DeviceCommandException("Không thể đóng kết nối MQTT", ex);
            } finally {
                client = null;
            }
        }
    }
}
