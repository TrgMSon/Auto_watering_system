package com.project.backend.service.impl;

import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.extension.ExtendWith;
import org.mockito.Mock;
import org.mockito.junit.jupiter.MockitoExtension;
import org.springframework.security.crypto.password.PasswordEncoder;

import com.project.backend.exception.UnauthorizedException;
import com.project.backend.model.RefreshToken;
import com.project.backend.model.User;
import com.project.backend.model.enums.AccountStatus;
import com.project.backend.repository.UserRepository;
import com.project.backend.security.JwtService;
import com.project.backend.service.RefreshTokenService;

@ExtendWith(MockitoExtension.class)
class AuthServiceImplTest {
    @Mock
    private UserRepository userRepository;
    @Mock
    private JwtService jwtService;
    @Mock
    private PasswordEncoder passwordEncoder;
    @Mock
    private RefreshTokenService refreshTokenService;

    private AuthServiceImpl authService;

    @BeforeEach
    void setUp() {
        authService = new AuthServiceImpl(userRepository, jwtService, passwordEncoder, refreshTokenService);
    }

    @Test
    void doesNotRefreshTokensForLockedUsers() {
        User user = User.builder().status(AccountStatus.LOCKED).build();
        RefreshToken token = RefreshToken.builder().user(user).build();
        when(refreshTokenService.validateAndGet("refresh-token")).thenReturn(token);

        assertThrows(UnauthorizedException.class, () -> authService.refreshToken("refresh-token"));

        verify(jwtService, never()).generateToken(user);
        verify(refreshTokenService, never()).rotate(token);
    }
}
