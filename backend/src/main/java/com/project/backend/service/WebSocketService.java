package com.project.backend.service;

import java.util.List;
import java.util.Objects;

import org.springframework.messaging.handler.annotation.MessageMapping;
import org.springframework.messaging.simp.SimpMessagingTemplate;
import org.springframework.stereotype.Service;

import com.project.backend.dto.device.DeviceSharedNotification;
import com.project.backend.dto.device.PumpStatusNotification;
import com.project.backend.repository.UserDeviceRepository;

@Service
public class WebSocketService {
    private final SimpMessagingTemplate messagingTemplate;
    private final UserDeviceRepository userDeviceRepository;

    public WebSocketService(
            SimpMessagingTemplate messagingTemplate,
            UserDeviceRepository userDeviceRepository) {
        this.messagingTemplate = messagingTemplate;
        this.userDeviceRepository = userDeviceRepository;
    }

    @SuppressWarnings("null")
    public void notifyDeviceShared(int userId, String deviceId, String deviceName) {
        String displayName = deviceName == null || deviceName.isBlank() ? deviceId : deviceName;
        messagingTemplate.convertAndSendToUser(
                Integer.toString(userId),
                "/queue/notifications",
                new DeviceSharedNotification(
                        "device_shared",
                        deviceId,
                        deviceName,
                        "Thiết bị " + displayName + " đã được chia sẻ với bạn"));
    }

    @MessageMapping("/notification")
    public void sendNotification(String message, String deviceId) {
        if (message.isBlank() || deviceId.isBlank())
            return;
        messagingTemplate.convertAndSend("/topic/notification/" + deviceId, message);
    }

    @MessageMapping("/pumpStatus")
    public void changePumpStatus(String deviceId, String status) {
        if (deviceId.isBlank() || status.isBlank())
            return;
        List<String> userIds = userDeviceRepository.findAuthorizedUserIds(deviceId);
        for (String userId : userIds)
            messagingTemplate.convertAndSendToUser(
                    Objects.requireNonNull(userId),
                    "/queue/pump-status",
                    new PumpStatusNotification(deviceId, status));
    }
}
