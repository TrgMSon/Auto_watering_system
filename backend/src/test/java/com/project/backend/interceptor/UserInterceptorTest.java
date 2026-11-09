package com.project.backend.interceptor;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import java.util.Optional;

import org.junit.jupiter.api.Test;
import org.springframework.messaging.Message;
import org.springframework.messaging.MessageChannel;
import org.springframework.messaging.MessageDeliveryException;
import org.springframework.messaging.simp.stomp.StompCommand;
import org.springframework.messaging.simp.stomp.StompHeaderAccessor;
import org.springframework.messaging.support.MessageBuilder;

import com.project.backend.model.UserDevice;
import com.project.backend.repository.UserDeviceRepository;
import com.project.backend.security.JwtService;

class UserInterceptorTest {
    private final JwtService jwtService = mock(JwtService.class);
    private final UserDeviceRepository userDeviceRepository = mock(UserDeviceRepository.class);
    private final UserInterceptor userInterceptor = new UserInterceptor(jwtService, userDeviceRepository);
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

    @Test
    void allowsOwnerAndUnlockedViewerToSubscribeToDeviceNotifications() {
        assertNotificationSubscriptionAllowed("owner");
        assertNotificationSubscriptionAllowed("viewer");
    }

    @Test
    void rejectsLockedViewerSubscription() {
        when(userDeviceRepository.findByUser_IdAndDevice_Id(12, "AA:BB:CC:DD:EE:FF"))
                .thenReturn(Optional.of(userDeviceWithPermission("viewer_locked")));

        assertThrows(
                MessageDeliveryException.class,
                () -> userInterceptor.preSend(notificationSubscriptionMessage("AA:BB:CC:DD:EE:FF"), channel));
    }

    @Test
    void rejectsSubscriptionWhenUserHasNoDeviceAssociation() {
        when(userDeviceRepository.findByUser_IdAndDevice_Id(12, "AA:BB:CC:DD:EE:FF"))
                .thenReturn(Optional.empty());

        assertThrows(
                MessageDeliveryException.class,
                () -> userInterceptor.preSend(notificationSubscriptionMessage("AA:BB:CC:DD:EE:FF"), channel));
    }

    @Test
    void rejectsMalformedDeviceNotificationDestination() {
        assertThrows(
                MessageDeliveryException.class,
                () -> userInterceptor.preSend(notificationSubscriptionMessage("AA:BB/other"), channel));
    }

    @Test
    void rejectsWildcardSubscriptionThatCouldBypassDeviceAuthorization() {
        assertThrows(
                MessageDeliveryException.class,
                () -> userInterceptor.preSend(subscriptionMessage("/topic/**"), channel));
    }

    private void assertNotificationSubscriptionAllowed(String permission) {
        when(userDeviceRepository.findByUser_IdAndDevice_Id(12, "AA:BB:CC:DD:EE:FF"))
                .thenReturn(Optional.of(userDeviceWithPermission(permission)));

        Message<?> result = userInterceptor.preSend(notificationSubscriptionMessage("AA:BB:CC:DD:EE:FF"), channel);

        assertEquals("/topic/notification/AA:BB:CC:DD:EE:FF", StompHeaderAccessor.wrap(result).getDestination());
    }

    private UserDevice userDeviceWithPermission(String permission) {
        UserDevice userDevice = new UserDevice();
        userDevice.setPermission(permission);
        return userDevice;
    }

    private Message<byte[]> connectMessage(String token, String claimedUserId) {
        StompHeaderAccessor accessor = StompHeaderAccessor.create(StompCommand.CONNECT);
        accessor.addNativeHeader("Authorization", "Bearer " + token);
        accessor.addNativeHeader("userId", claimedUserId);
        accessor.setLeaveMutable(true);
        return MessageBuilder.createMessage(new byte[0], accessor.getMessageHeaders());
    }

    private Message<byte[]> notificationSubscriptionMessage(String deviceId) {
        return subscriptionMessage("/topic/notification/" + deviceId);
    }

    private Message<byte[]> subscriptionMessage(String destination) {
        StompHeaderAccessor accessor = StompHeaderAccessor.create(StompCommand.SUBSCRIBE);
        accessor.setDestination(destination);
        accessor.setUser(new StompPrincipal("12"));
        accessor.setLeaveMutable(true);
        return MessageBuilder.createMessage(new byte[0], accessor.getMessageHeaders());
    }
}
