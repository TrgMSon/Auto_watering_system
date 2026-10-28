package com.project.backend.model.enums;

import jakarta.persistence.AttributeConverter;
import jakarta.persistence.Converter;

@Converter(autoApply = true)
public class RoleConverter implements AttributeConverter<Role, String> {

    @Override
    public String convertToDatabaseColumn(Role role) {
        if (role == null) return null;
        return role.name().toLowerCase();
    }

    @Override
    public Role convertToEntityAttribute(String dbData) {
        if (dbData == null || dbData.isEmpty()) return null;
        return switch (dbData.trim().toUpperCase()) {
            case "USER", "OWNER", "VIEWER" -> Role.USER;
            case "ADMIN" -> Role.ADMIN;
            default -> throw new IllegalArgumentException("Vai trò tài khoản không hợp lệ: " + dbData);
        };
    }
}
