package com.project.backend.service;

import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import java.util.List;

import org.junit.jupiter.api.Test;
import org.springframework.messaging.simp.SimpMessagingTemplate;

import com.project.backend.dto.device.DeviceSharedNotification;
import com.project.backend.dto.device.PumpStatusNotification;
import com.project.backend.repository.UserDeviceRepository;

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

    @SuppressWarnings("null")
    @Test
    void sendsPumpStatusWithDeviceIdToAuthorizedUsers() {
        SimpMessagingTemplate messagingTemplate = mock(SimpMessagingTemplate.class);
        UserDeviceRepository userDeviceRepository = mock(UserDeviceRepository.class);
        when(userDeviceRepository.findAuthorizedUserIds("AA:BB:CC:DD:EE:FF"))
                .thenReturn(List.of("12"));
        WebSocketService webSocketService = new WebSocketService(messagingTemplate, userDeviceRepository);

        webSocketService.changePumpStatus("AA:BB:CC:DD:EE:FF", "ON");

        verify(messagingTemplate).convertAndSendToUser(
                eq("12"),
                eq("/queue/pump-status"),
                argThat(notification -> notification instanceof PumpStatusNotification status
                        && "AA:BB:CC:DD:EE:FF".equals(status.deviceId())
                        && "ON".equals(status.status())));
    }
}
