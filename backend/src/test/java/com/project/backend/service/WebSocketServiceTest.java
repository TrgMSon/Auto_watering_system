package com.project.backend.service;

import static org.mockito.Mockito.verify;

import org.junit.jupiter.api.Test;
import org.springframework.messaging.simp.SimpMessagingTemplate;

import com.project.backend.dto.device.DeviceSharedNotification;
import com.project.backend.repository.UserDeviceRepository;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.mock;

class WebSocketServiceTest {
    @SuppressWarnings("null")
@Test
    void sendsDeviceSharedNotificationToRecipientQueue() {
        SimpMessagingTemplate messagingTemplate = mock(SimpMessagingTemplate.class);
        WebSocketService webSocketService = new WebSocketService(
                messagingTemplate,
                mock(UserDeviceRepository.class));

        webSocketService.notifyDeviceShared(12, "AA:BB:CC:DD:EE:FF", "Vườn nhà");

        verify(messagingTemplate).convertAndSendToUser(
                eq("12"),
                eq("/queue/notifications"),
                argThat(notification -> notification instanceof DeviceSharedNotification event
                        && "device_shared".equals(event.type())
                        && "AA:BB:CC:DD:EE:FF".equals(event.deviceId())
                        && "Vườn nhà".equals(event.deviceName())));
    }
}
