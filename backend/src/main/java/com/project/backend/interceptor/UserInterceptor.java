package com.project.backend.interceptor;

import org.springframework.messaging.Message;
import org.springframework.messaging.MessageChannel;
import org.springframework.messaging.MessageDeliveryException;
import org.springframework.messaging.simp.stomp.StompCommand;
import org.springframework.messaging.simp.stomp.StompHeaderAccessor;
import org.springframework.messaging.support.ChannelInterceptor;
import org.springframework.messaging.support.MessageHeaderAccessor;
import org.springframework.stereotype.Component;

import com.project.backend.security.JwtService;

@Component
public class UserInterceptor implements ChannelInterceptor {
    private final JwtService jwtService;

    public UserInterceptor(JwtService jwtService) {
        this.jwtService = jwtService;
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
        }
        return message;
    }
}
