package com.project.backend.config;

import java.nio.charset.StandardCharsets;

import javax.net.ssl.SSLSocketFactory;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.boot.autoconfigure.condition.ConditionalOnProperty;
import org.springframework.context.annotation.Bean;
import org.springframework.context.annotation.Configuration;
import org.springframework.context.event.EventListener;
import org.springframework.integration.annotation.ServiceActivator;
import org.springframework.integration.channel.DirectChannel;
import org.springframework.integration.core.MessageProducer;
import org.springframework.integration.mqtt.core.DefaultMqttPahoClientFactory;
import org.springframework.integration.mqtt.core.MqttPahoClientFactory;
import org.springframework.integration.mqtt.event.MqttConnectionFailedEvent;
import org.springframework.integration.mqtt.inbound.MqttPahoMessageDrivenChannelAdapter;
import org.springframework.integration.mqtt.outbound.MqttPahoMessageHandler;
import org.springframework.integration.mqtt.support.DefaultPahoMessageConverter;
import org.springframework.messaging.MessageChannel;
import org.springframework.messaging.MessageHandler;

import com.project.backend.service.MqttCommandAcknowledgmentService;
import com.project.backend.service.impl.SensorDataServiceImpl;

import org.eclipse.paho.client.mqttv3.MqttConnectOptions;

@Configuration
@ConditionalOnProperty(name = "app.mqtt.enabled", havingValue = "true")
public class MqttConfig {
    private static final Logger logger = LoggerFactory.getLogger(MqttConfig.class);
    private SensorDataServiceImpl sensorDataServiceImpl;

    public MqttConfig(SensorDataServiceImpl sensorDataServiceImpl) {
        this.sensorDataServiceImpl = sensorDataServiceImpl;
    }

    @Value("${app.mqtt.broker-url:}")
    private String brokerUrl;

    @Value("${app.mqtt.username:}")
    private String username;

    @Value("${app.mqtt.password:}")
    private String password;

    @Value("${app.mqtt.client.id:}")
    private String clientId;

    @EventListener
    public void onMqttConnectionFailed(MqttConnectionFailedEvent event) {
        logger.error("MQTT client connection failed: {}", event.getSource(), event.getCause());
    }

    @Bean
    public MqttPahoClientFactory mqttClientFactory() {
        DefaultMqttPahoClientFactory factory = new DefaultMqttPahoClientFactory();
        MqttConnectOptions options = new MqttConnectOptions();
        options.setServerURIs(new String[] { brokerUrl });
        options.setCleanSession(true);
        options.setKeepAliveInterval(60);
        options.setConnectionTimeout(5);
        options.setAutomaticReconnect(true);
        if (!username.isBlank()) {
            options.setUserName(username);
            options.setPassword(password.toCharArray());
        }
        if (brokerUrl.startsWith("ssl://")) {
            options.setSocketFactory(SSLSocketFactory.getDefault());
        }
        factory.setConnectionOptions(options);
        return factory;
    }

    @Bean
    public MessageChannel mqttInputChannel() {
        return new DirectChannel();
    }

    @Bean
    public MessageProducer inbound() {
        MqttPahoMessageDrivenChannelAdapter adapter =
                new MqttPahoMessageDrivenChannelAdapter(
                        clientId + "_inbound",
                        mqttClientFactory(),
                        "iot/garden/+/telemetry",
                        "iot/garden/+/status",
                        "iot/garden/+/ack");
        adapter.setCompletionTimeout(5000);
        adapter.setConverter(new DefaultPahoMessageConverter());
        adapter.setQos(0, 1, 1);
        adapter.setOutputChannel(mqttInputChannel());
        return adapter;
    }

    @Bean
    @ServiceActivator(inputChannel = "mqttInputChannel")
    public MessageHandler inboundHandler(
            MqttCommandAcknowledgmentService acknowledgmentService) {
        return message -> {
            Object topic = message.getHeaders().get("mqtt_receivedTopic");
            String topicName = topic == null ? "" : topic.toString();
            Object messagePayload = message.getPayload();
            String payload = messagePayload instanceof byte[] bytes
                    ? new String(bytes, StandardCharsets.UTF_8)
                    : messagePayload.toString();
            if (topicName.endsWith("/ack")) {
                acknowledgmentService.handleMessage(topicName, payload);
            } 
            else if (topicName.endsWith("/telemetry")) {
                sensorDataServiceImpl.recordSensorLog(topicName, payload);
            }
            else {
                logger.info("MQTT message received; topic={}, payload={}", topicName, payload);
            }
        };
    }

    @Bean
    public MessageChannel mqttOutboundChannel() {
        return new DirectChannel();
    }

    @Bean
    @ServiceActivator(inputChannel = "mqttOutboundChannel")
    public MessageHandler mqttOutbound() {
        MqttPahoMessageHandler messageHandler =
                new MqttPahoMessageHandler(clientId + "_outbound", mqttClientFactory());
        messageHandler.setAsync(false);
        messageHandler.setDefaultQos(1);
        messageHandler.setCompletionTimeout(5000);
        return messageHandler;
    }
}
