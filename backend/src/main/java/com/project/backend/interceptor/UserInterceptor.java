package com.project.backend.interceptor;

import java.security.Principal;
import java.util.Optional;

import org.springframework.messaging.Message;
import org.springframework.messaging.MessageChannel;
import org.springframework.messaging.MessageDeliveryException;
import org.springframework.messaging.simp.stomp.StompCommand;
import org.springframework.messaging.simp.stomp.StompHeaderAccessor;
import org.springframework.messaging.support.ChannelInterceptor;
import org.springframework.messaging.support.MessageHeaderAccessor;
import org.springframework.stereotype.Component;

import com.project.backend.model.UserDevice;
import com.project.backend.repository.UserDeviceRepository;
import com.project.backend.security.JwtService;

@Component
public class UserInterceptor implements ChannelInterceptor {
    private static final String DEVICE_NOTIFICATION_DESTINATION = "/topic/notification/";

    private final JwtService jwtService;
    private final UserDeviceRepository userDeviceRepository;

    public UserInterceptor(JwtService jwtService, UserDeviceRepository userDeviceRepository) {
        this.jwtService = jwtService;
        this.userDeviceRepository = userDeviceRepository;
    }

    @SuppressWarnings("null")
    @Override
    public Message<?> preSend(Message<?> message, MessageChannel channel) {
        StompHeaderAccessor accessor = MessageHeaderAccessor.getAccessor(message, StompHeaderAccessor.class);

        if (StompCommand.CONNECT.equals(accessor.getCommand())) {
            String authorization = accessor.getFirstNativeHeader("Authorization");
            if (authorization == null || !authorization.startsWith("Bearer ")) {
                throw new MessageDeliveryException(message, "WebSocket authentication is required");
            }

            String token = authorization.substring(7);
            if (!jwtService.isTokenValid(token)) {
                throw new MessageDeliveryException(message, "WebSocket authentication is invalid");
            }

            Integer userId = jwtService.extractUserId(token);
            if (userId == null) {
                throw new MessageDeliveryException(message, "WebSocket token has no user identity");
            }
            accessor.setUser(new StompPrincipal(userId.toString()));
        } else if (StompCommand.SUBSCRIBE.equals(accessor.getCommand())) {
            authorizeDeviceNotificationSubscription(message, accessor);
        }
        return message;
    }

    private void authorizeDeviceNotificationSubscription(
            Message<?> message,
            StompHeaderAccessor accessor) {
        String destination = accessor.getDestination();
        if (isPatternDestination(destination)) {
            throw new MessageDeliveryException(message, "Wildcard WebSocket subscriptions are not allowed");
        }
        if (destination == null
                || !(destination.equals("/topic/notification")
                        || destination.startsWith(DEVICE_NOTIFICATION_DESTINATION))) {
            return;
        }

        String deviceId = destination.startsWith(DEVICE_NOTIFICATION_DESTINATION)
                ? destination.substring(DEVICE_NOTIFICATION_DESTINATION.length())
                : "";
        if (deviceId.isBlank() || deviceId.contains("/")) {
            throw new MessageDeliveryException(message, "Invalid device notification destination");
        }

        Principal principal = accessor.getUser();
        if (principal == null) {
            throw new MessageDeliveryException(message, "WebSocket authentication is required");
        }

        int userId;
        try {
            userId = Integer.parseInt(principal.getName());
        } catch (NumberFormatException exception) {
            throw new MessageDeliveryException(message, "WebSocket user identity is invalid");
        }

        Optional<UserDevice> association = userDeviceRepository.findByUser_IdAndDevice_Id(userId, deviceId);
        boolean hasPermission = association
                .map(UserDevice::getPermission)
                .map(permission -> "owner".equalsIgnoreCase(permission) || "viewer".equalsIgnoreCase(permission))
                .orElse(false);
        if (!hasPermission) {
            throw new MessageDeliveryException(message, "User is not allowed to subscribe to this device");
        }
    }

    private boolean isPatternDestination(String destination) {
        return destination != null && (destination.indexOf('*') >= 0
                || destination.indexOf('?') >= 0
                || destination.indexOf('{') >= 0
                || destination.indexOf('}') >= 0);
    }
}
