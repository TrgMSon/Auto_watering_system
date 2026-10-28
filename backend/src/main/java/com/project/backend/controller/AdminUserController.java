package com.project.backend.controller;

import java.util.List;

import org.springframework.http.ResponseEntity;
import org.springframework.security.access.prepost.PreAuthorize;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.PostMapping;
import org.springframework.web.bind.annotation.GetMapping;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RestController;

import com.project.backend.common.ApiResponse;
import com.project.backend.dto.user.AdminUserResponse;
import com.project.backend.exception.ConflictException;
import com.project.backend.exception.NotFoundException;
import com.project.backend.model.User;
import com.project.backend.model.enums.AccountStatus;
import com.project.backend.model.enums.Role;
import com.project.backend.repository.UserRepository;

@RestController
@RequestMapping("/api/admin/users")
public class AdminUserController {
    private final UserRepository userRepository;

    public AdminUserController(UserRepository userRepository) {
        this.userRepository = userRepository;
    }

    @GetMapping
    @PreAuthorize("hasRole('ADMIN')")
    public ResponseEntity<ApiResponse<List<AdminUserResponse>>> getAllUsers() {
        List<AdminUserResponse> users = userRepository.findAll()
                .stream()
                .sorted((first, second) -> Integer.compare(first.getId(), second.getId()))
                .map(AdminUserController::toResponse)
                .toList();
        return ResponseEntity.ok(ApiResponse.success(users));
    }

    @PostMapping("/{userId}/lock")
    @PreAuthorize("hasRole('ADMIN')")
    public ResponseEntity<ApiResponse<Void>> lockUser(@PathVariable int userId) {
        return updateUserStatus(userId, AccountStatus.LOCKED, "Đã khóa tài khoản người dùng");
    }

    @PostMapping("/{userId}/unlock")
    @PreAuthorize("hasRole('ADMIN')")
    public ResponseEntity<ApiResponse<Void>> unlockUser(@PathVariable int userId) {
        return updateUserStatus(userId, AccountStatus.ACTIVE, "Đã mở khóa tài khoản người dùng");
    }

    private ResponseEntity<ApiResponse<Void>> updateUserStatus(
            int userId, AccountStatus status, String message) {
        User user = userRepository.findById(userId)
                .orElseThrow(() -> new NotFoundException("Không tìm thấy người dùng"));
        if (user.getRole() != Role.USER) {
            throw new ConflictException("Chỉ có thể khóa hoặc mở khóa tài khoản USER");
        }

        user.setStatus(status);
        userRepository.save(user);
        return ResponseEntity.ok(ApiResponse.success(message, null));
    }

    private static AdminUserResponse toResponse(User user) {
        return new AdminUserResponse(
                user.getId(),
                user.getEmail(),
                user.getPhoneNumber(),
                user.getRole().name(),
                user.getStatus().name(),
                user.getCreatedAt());
    }
}
