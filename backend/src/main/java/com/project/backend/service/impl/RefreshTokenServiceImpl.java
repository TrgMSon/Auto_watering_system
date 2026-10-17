package com.project.backend.service.impl;

import java.time.LocalDateTime;
import java.util.Objects;
import java.util.UUID;

import org.apache.commons.codec.digest.DigestUtils;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.stereotype.Service;

import com.project.backend.exception.UnauthorizedException;
import com.project.backend.model.RefreshToken;
import com.project.backend.model.User;
import com.project.backend.repository.RefreshTokenRepository;
import com.project.backend.service.RefreshTokenService;

@Service
public class RefreshTokenServiceImpl implements RefreshTokenService {
    private final RefreshTokenRepository refreshTokenRepository;

    public RefreshTokenServiceImpl(RefreshTokenRepository refreshTokenRepository) {
        this.refreshTokenRepository = refreshTokenRepository;
    }

    @Value("${app.refresh-token.expiration-days:}")
    private long expirationDays;

    @Override
    public String createRefreshToken(User user) {
        String rawToken = UUID.randomUUID().toString();
        RefreshToken entity = RefreshToken.builder()
                .tokenHash(hash(rawToken))
                .user(user)
                .expiresAt(LocalDateTime.now().plusDays(expirationDays))
                .revoked(false)
                .build();
        refreshTokenRepository.save(Objects.requireNonNull(entity, "Không tìm thấy token"));
        return rawToken;
    }

    private String hash(String raw) {
        return DigestUtils.sha256Hex(raw);
    }

    public RefreshToken validateAndGet(String rawToken) {
        RefreshToken entity = refreshTokenRepository.findByTokenHash(hash(rawToken))
                .orElseThrow(() -> new UnauthorizedException("Refresh token không hợp lệ"));
        if (entity.isRevoked() || entity.getExpiresAt().isBefore(LocalDateTime.now())) {
            throw new UnauthorizedException("Refresh token đã hết hạn hoặc bị thu hồi");
        }
        return entity;
    }

    public void revoke(RefreshToken entity) {
        entity.setRevoked(true);
        entity.setRevokedAt(LocalDateTime.now());
        refreshTokenRepository.save(entity);
    }
    
    //cơ chế rotation refreshtoken 
    @Override
    public String rotate(RefreshToken oldToken) {
    oldToken.setRevoked(true);
    oldToken.setRevokedAt(LocalDateTime.now());
    refreshTokenRepository.save(oldToken);

    // tạo token mới cho cùng user
    return createRefreshToken(oldToken.getUser());
   }
    
}

