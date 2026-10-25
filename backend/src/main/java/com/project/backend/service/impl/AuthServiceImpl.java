package com.project.backend.service.impl;

import java.time.LocalDateTime;
import java.util.Objects;

import org.springframework.security.crypto.password.PasswordEncoder;
import org.springframework.stereotype.Service;

import com.project.backend.dto.auth.AuthResponse;
import com.project.backend.dto.auth.LoginRequest;
import com.project.backend.dto.auth.RegisterRequest;
import com.project.backend.dto.user.UserResponse;
import com.project.backend.exception.ConflictException;
import com.project.backend.exception.UnauthorizedException;
import com.project.backend.model.RefreshToken;
import com.project.backend.model.User;
import com.project.backend.model.enums.AccountStatus;
import com.project.backend.model.enums.Role;
import com.project.backend.repository.UserRepository;
import com.project.backend.security.JwtService;
import com.project.backend.service.AuthService;
import com.project.backend.service.RefreshTokenService;

import jakarta.transaction.Transactional;

@Service 
public class AuthServiceImpl implements AuthService {
    private final UserRepository userRepository;
    private final JwtService jwtService;
    private final PasswordEncoder passwordEncoder;
    private final RefreshTokenService refreshTokenService;

    public AuthServiceImpl(UserRepository userRepository, JwtService jwtService, PasswordEncoder passwordEncoder, RefreshTokenService refreshTokenService) {
        this.userRepository = userRepository;
        this.jwtService = jwtService;
        this.passwordEncoder = passwordEncoder;
        this.refreshTokenService = refreshTokenService;
    }
    
    @Override
    public void register(RegisterRequest req) {
        if (userRepository.existsByEmail(req.getEmail())) {
            throw new ConflictException("Email này đã được đăng ký trong hệ thống. Vui lòng đăng nhập hoặc sử dụng email khác.");
        }

        String passwordHash = passwordEncoder.encode(req.getPassword());

        User user = User.builder()
                .email(req.getEmail())
                .role(Role.USER)
                .status(AccountStatus.ACTIVE)
                .passwordHash(passwordHash)
                .createdAt(LocalDateTime.now())
                .build();

        userRepository.save(Objects.requireNonNull(user, "Không tìm thấy người dùng"));
    }

    @Override
    @Transactional
    public AuthResponse login(LoginRequest req) {
        User user = userRepository.findByEmail(req.getEmail())
                .orElseThrow(() -> new UnauthorizedException("Email hoặc mật khẩu không chính xác"));

        boolean isValidPass = passwordEncoder.matches(req.getPassword(), user.getPasswordHash());
        if (!isValidPass) {
            throw new UnauthorizedException("Email hoặc mật khẩu không chính xác");
        }

        if (user.getStatus() == AccountStatus.LOCKED) {
            throw new UnauthorizedException("Tài khoản đã bị khóa hoặc không tồn tại.");
        }

        String accessToken = jwtService.generateToken(user);
        String refreshToken = refreshTokenService.createRefreshToken(user);
        UserResponse userResponse = UserResponse.builder()
                                    .id(user.getId())
                                    .email(user.getEmail())
                                    .phoneNumber(user.getPhoneNumber())
                                    .role(user.getRole().name())
                                    .build();

        return AuthResponse.builder()
                .accessToken(accessToken)
                .refreshToken(refreshToken)
                .role(user.getRole().name())
                .userResponse(userResponse)
                .build();
    }

    @Override
    public AuthResponse refreshToken(String refreshToken) {
        RefreshToken oldToken = refreshTokenService.validateAndGet(refreshToken);
        String newAccessToken = jwtService.generateToken(oldToken.getUser());
        String newRefreshToken = refreshTokenService.rotate(oldToken);

        return AuthResponse.builder()
                .accessToken(newAccessToken)
                .refreshToken(newRefreshToken)
                .build();
    }

    @Override
    public void logout(String refreshToken) {
        RefreshToken token = refreshTokenService.validateAndGet(refreshToken);
        refreshTokenService.revoke(token);
    }
}
