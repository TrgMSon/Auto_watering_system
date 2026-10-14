package com.project.backend.common;

import com.fasterxml.jackson.annotation.JsonInclude;

import lombok.AccessLevel;
import lombok.Builder;
import lombok.Data;
import lombok.NoArgsConstructor;
import lombok.experimental.FieldDefaults;

@Data
@Builder
@NoArgsConstructor
@JsonInclude(JsonInclude.Include.NON_NULL)
@FieldDefaults(level = AccessLevel.PRIVATE)
public class ApiResponse<T> {
    boolean success;
    int statusCode;
    String message;
    T data;

    public ApiResponse(boolean success, int statusCode, String message, T data) {
        this.success = success;
        this.statusCode = statusCode;
        this.message = message;
        this.data = data;
    }

    public static <T> ApiResponse<T> success(T data) {
        return success("Success", data);
    }

    public static <T> ApiResponse<T> success(String message, T data) {
        return ApiResponse.<T>builder()
                .success(true)
                .statusCode(200)
                .message(message)
                .data(data)
                .build();
    }

    public static <T> ApiResponse<T> error(String message) {
        return error(400, message);
    }

    public static <T> ApiResponse<T> error(int statusCode, String message) {
        return ApiResponse.<T>builder()
                .success(false)
                .statusCode(statusCode)
                .message(message)
                .build();
    }
}
