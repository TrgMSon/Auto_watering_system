package com.project.backend.service;

import com.project.backend.dto.auth.AuthResponse;
import com.project.backend.dto.auth.LoginRequest;
import com.project.backend.dto.auth.RegisterRequest;

public interface AuthService {
    public void register(RegisterRequest req);
    public AuthResponse login(LoginRequest req);
    public AuthResponse refreshToken(String refreshToken);
    public void logout(String refreshToken);
}
