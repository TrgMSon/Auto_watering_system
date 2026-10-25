package com.project.backend.service;

import java.util.List;
import java.util.Objects;

import org.springframework.messaging.handler.annotation.MessageMapping;
import org.springframework.messaging.simp.SimpMessagingTemplate;
import org.springframework.stereotype.Service;

import com.project.backend.repository.UserDeviceRepository;

@Service
public class WebSocketService {
    private SimpMessagingTemplate messagingTemplate;
    private UserDeviceRepository userDeviceRepository;

    public WebSocketService(SimpMessagingTemplate messagingTemplate) {
        this.messagingTemplate = messagingTemplate;
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
        List<String> userIds = userDeviceRepository.findUserIds(deviceId);
        for (String userId : userIds)
            messagingTemplate.convertAndSendToUser(Objects.requireNonNull(userId), "/queue/pump-status", status);
    }
}
