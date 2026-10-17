package com.project.backend.exception;

public class ConflictException extends RuntimeException {
    public ConflictException(String message)
    {
        super(message);
    }
}
