package com.project.backend.controller;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import java.time.LocalDateTime;
import java.util.List;
import java.util.Optional;

import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.extension.ExtendWith;
import org.mockito.Mock;
import org.mockito.junit.jupiter.MockitoExtension;

import com.project.backend.exception.ConflictException;
import com.project.backend.exception.NotFoundException;
import com.project.backend.model.User;
import com.project.backend.model.enums.AccountStatus;
import com.project.backend.model.enums.Role;
import com.project.backend.repository.UserRepository;

@ExtendWith(MockitoExtension.class)
class AdminUserControllerTest {
    @Mock
    private UserRepository userRepository;

    private AdminUserController controller;

    @BeforeEach
    void setUp() {
        controller = new AdminUserController(userRepository);
    }

    @Test
    void listsUsersWithoutExposingPasswordHashes() {
        User user = user(2, "user@example.com", Role.USER, AccountStatus.ACTIVE);
        user.setPasswordHash("must-not-be-returned");
        User admin = user(1, "admin@example.com", Role.ADMIN, AccountStatus.ACTIVE);
        when(userRepository.findAll()).thenReturn(List.of(user, admin));

        var response = controller.getAllUsers().getBody();

        assertEquals(1, response.getData().get(0).id());
        assertEquals("admin@example.com", response.getData().get(0).email());
        assertEquals(2, response.getData().get(1).id());
        assertEquals("ACTIVE", response.getData().get(1).status());
    }

    @Test
    void locksUserAccount() {
        User user = user(2, "user@example.com", Role.USER, AccountStatus.ACTIVE);
        when(userRepository.findById(2)).thenReturn(Optional.of(user));

        controller.lockUser(2);

        assertEquals(AccountStatus.LOCKED, user.getStatus());
        verify(userRepository).save(user);
    }

    @Test
    void unlocksLockedUserAccount() {
        User user = user(2, "user@example.com", Role.USER, AccountStatus.LOCKED);
        when(userRepository.findById(2)).thenReturn(Optional.of(user));

        controller.unlockUser(2);

        assertEquals(AccountStatus.ACTIVE, user.getStatus());
        verify(userRepository).save(user);
    }

    @Test
    void doesNotAllowChangingAdminStatus() {
        User admin = user(1, "admin@example.com", Role.ADMIN, AccountStatus.ACTIVE);
        when(userRepository.findById(1)).thenReturn(Optional.of(admin));

        assertThrows(ConflictException.class, () -> controller.lockUser(1));

        verify(userRepository, never()).save(any(User.class));
    }

    @Test
    void reportsMissingUser() {
        when(userRepository.findById(99)).thenReturn(Optional.empty());

        assertThrows(NotFoundException.class, () -> controller.lockUser(99));
    }

    private static User user(int id, String email, Role role, AccountStatus status) {
        return User.builder()
                .id(id)
                .email(email)
                .phoneNumber("0900000000")
                .role(role)
                .status(status)
                .createdAt(LocalDateTime.of(2026, 10, 28, 8, 30))
                .build();
    }
}
