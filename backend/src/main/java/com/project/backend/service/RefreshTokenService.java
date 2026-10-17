package com.project.backend.service;

import com.project.backend.model.RefreshToken;
import com.project.backend.model.User;

public interface RefreshTokenService {
    String createRefreshToken(User user);
    RefreshToken validateAndGet(String rawToken);
    void revoke(RefreshToken entity);
    String rotate(RefreshToken oldToken);
}
