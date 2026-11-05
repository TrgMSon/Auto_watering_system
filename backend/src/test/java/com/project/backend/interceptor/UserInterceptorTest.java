package com.project.backend.interceptor;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.jupiter.api.Test;
import org.springframework.messaging.Message;
import org.springframework.messaging.MessageChannel;
import org.springframework.messaging.MessageDeliveryException;
import org.springframework.messaging.simp.stomp.StompCommand;
import org.springframework.messaging.simp.stomp.StompHeaderAccessor;
import org.springframework.messaging.support.MessageBuilder;

import com.project.backend.security.JwtService;

class UserInterceptorTest {
    private final JwtService jwtService = mock(JwtService.class);
    private final UserInterceptor userInterceptor = new UserInterceptor(jwtService);
    private final MessageChannel channel = mock(MessageChannel.class);

    @SuppressWarnings("null")
    @Test
    void setsConnectionPrincipalFromVerifiedJwtInsteadOfClientUserId() {
        when(jwtService.isTokenValid("valid-token")).thenReturn(true);
        when(jwtService.extractUserId("valid-token")).thenReturn(12);

        Message<?> result = userInterceptor.preSend(connectMessage("valid-token", "999"), channel);

        assertEquals("12", StompHeaderAccessor.wrap(result).getUser().getName());
        verify(jwtService).extractUserId("valid-token");
    }

    @Test
    void rejectsConnectionWithoutValidJwt() {
        when(jwtService.isTokenValid("invalid-token")).thenReturn(false);

        assertThrows(
                MessageDeliveryException.class,
                () -> userInterceptor.preSend(connectMessage("invalid-token", "12"), channel));
    }

    private Message<byte[]> connectMessage(String token, String claimedUserId) {
        StompHeaderAccessor accessor = StompHeaderAccessor.create(StompCommand.CONNECT);
        accessor.addNativeHeader("Authorization", "Bearer " + token);
        accessor.addNativeHeader("userId", claimedUserId);
        accessor.setLeaveMutable(true);
        return MessageBuilder.createMessage(new byte[0], accessor.getMessageHeaders());
    }
}
