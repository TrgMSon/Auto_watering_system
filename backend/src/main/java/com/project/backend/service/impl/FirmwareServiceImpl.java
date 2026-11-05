package com.project.backend.service.impl;

import java.io.IOException;
import java.nio.file.AtomicMoveNotSupportedException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardCopyOption;
import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.time.LocalDateTime;
import java.util.HexFormat;
import java.util.List;
import java.util.Locale;
import java.util.UUID;
import java.util.regex.Pattern;

import org.springframework.beans.factory.annotation.Value;
import org.springframework.dao.DataIntegrityViolationException;
import org.springframework.stereotype.Service;
import org.springframework.web.multipart.MultipartFile;

import com.project.backend.dto.firmware.FirmwareCheckResponse;
import com.project.backend.dto.firmware.FirmwareReleaseResponse;
import com.project.backend.exception.ConflictException;
import com.project.backend.exception.DeviceCommandException;
import com.project.backend.exception.IllegalArgumentException;
import com.project.backend.exception.NotFoundException;
import com.project.backend.model.FirmwareRelease;
import com.project.backend.repository.FirmwareReleaseRepository;
import com.project.backend.service.FirmwareService;

@Service
public class FirmwareServiceImpl implements FirmwareService {
    private static final Pattern VERSION_PATTERN = Pattern.compile("[A-Za-z0-9][A-Za-z0-9._+-]{0,63}");
    private static final long DEFAULT_MAX_FILE_SIZE = 4 * 1024 * 1024;

    private final FirmwareReleaseRepository firmwareReleaseRepository;
    private final Path storageDirectory;
    private final long maxFileSize;

    public FirmwareServiceImpl(
            FirmwareReleaseRepository firmwareReleaseRepository,
            @Value("${app.firmware.storage-dir:./data/firmware}") String storageDirectory,
            @Value("${app.firmware.max-file-size:4194304}") long maxFileSize) {
        this.firmwareReleaseRepository = firmwareReleaseRepository;
        this.storageDirectory = Path.of(storageDirectory).toAbsolutePath().normalize();
        this.maxFileSize = maxFileSize > 0 ? maxFileSize : DEFAULT_MAX_FILE_SIZE;
    }

    @Override
    public List<FirmwareReleaseResponse> getReleases() {
        return firmwareReleaseRepository.findAllByOrderByCreatedAtDescIdDesc().stream()
                .map(FirmwareServiceImpl::toResponse)
                .toList();
    }

    @Override
    public FirmwareReleaseResponse upload(String version, String notes, MultipartFile file) {
        String normalizedVersion = version == null ? "" : version.trim();
        if (!VERSION_PATTERN.matcher(normalizedVersion).matches()) {
            throw new IllegalArgumentException("Phiên bản firmware không hợp lệ");
        }
        if (firmwareReleaseRepository.existsByVersion(normalizedVersion)) {
            throw new ConflictException("Phiên bản firmware đã tồn tại");
        }
        if (notes != null && notes.length() > 500) {
            throw new IllegalArgumentException("Ghi chú không được vượt quá 500 ký tự");
        }
        if (file == null || file.isEmpty()) {
            throw new IllegalArgumentException("Vui lòng chọn file firmware .bin");
        }
        if (file.getSize() > maxFileSize) {
            throw new IllegalArgumentException("File firmware vượt quá giới hạn 4 MB");
        }
        String originalFilename = file.getOriginalFilename();
        if (originalFilename == null || !originalFilename.toLowerCase(Locale.ROOT).endsWith(".bin")) {
            throw new IllegalArgumentException("Chỉ chấp nhận file firmware có đuôi .bin");
        }

        byte[] firmware;
        try {
            firmware = file.getBytes();
        } catch (IOException exception) {
            throw new DeviceCommandException("Không thể đọc file firmware đã tải lên", exception);
        }
        if (firmware.length == 0 || firmware[0] != (byte) 0xE9) {
            throw new IllegalArgumentException("File không có header firmware ESP32 hợp lệ");
        }

        String storedFileName = UUID.randomUUID() + ".bin";
        Path storedFile = storageDirectory.resolve(storedFileName);
        Path temporaryFile = storageDirectory.resolve(storedFileName + ".tmp");
        FirmwareRelease release = new FirmwareRelease();
        release.setVersion(normalizedVersion);
        release.setNotes(notes == null || notes.isBlank() ? null : notes.trim());
        release.setStoredFileName(storedFileName);
        release.setSha256(sha256(firmware));
        release.setSizeBytes(firmware.length);
        release.setCreatedAt(LocalDateTime.now());

        try {
            Files.createDirectories(storageDirectory);
            Files.write(temporaryFile, firmware);
            try {
                Files.move(temporaryFile, storedFile, StandardCopyOption.ATOMIC_MOVE);
            } catch (AtomicMoveNotSupportedException exception) {
                try {
                    Files.move(temporaryFile, storedFile);
                } catch (IOException fallbackException) {
                    fallbackException.addSuppressed(exception);
                    throw fallbackException;
                }
            }
            try {
                return toResponse(firmwareReleaseRepository.save(release));
            } catch (DataIntegrityViolationException exception) {
                deleteAfterFailure(storedFile, exception);
                throw new ConflictException("Phiên bản firmware đã tồn tại");
            } catch (RuntimeException exception) {
                deleteAfterFailure(storedFile, exception);
                throw exception;
            }
        } catch (IOException exception) {
            deleteAfterFailure(temporaryFile, exception);
            deleteAfterFailure(storedFile, exception);
            throw new DeviceCommandException("Không thể lưu file firmware trên máy chủ", exception);
        }
    }

    @Override
    public FirmwareCheckResponse checkForUpdate(String currentVersion) {
        String normalizedCurrentVersion = currentVersion == null ? null : currentVersion.trim();
        FirmwareRelease latestRelease = firmwareReleaseRepository.findFirstByOrderByCreatedAtDescIdDesc()
                .orElse(null);
        if (latestRelease == null) {
            return new FirmwareCheckResponse(normalizedCurrentVersion, null, false, null, null, null);
        }
        boolean updateAvailable = !latestRelease.getVersion().equals(normalizedCurrentVersion);
        return new FirmwareCheckResponse(
                normalizedCurrentVersion,
                latestRelease.getVersion(),
                updateAvailable,
                latestRelease.getSha256(),
                latestRelease.getSizeBytes(),
                updateAvailable ? "/api/firmware/" + latestRelease.getVersion() + "/download" : null);
    }

    @Override
    public Path getFirmwareFile(String version) {
        FirmwareRelease release = firmwareReleaseRepository.findByVersion(version)
                .orElseThrow(() -> new NotFoundException("Không tìm thấy phiên bản firmware"));
        Path firmwareFile = storageDirectory.resolve(release.getStoredFileName()).normalize();
        if (!firmwareFile.startsWith(storageDirectory) || !Files.isRegularFile(firmwareFile)) {
            throw new DeviceCommandException("File firmware không tồn tại trên máy chủ");
        }
        return firmwareFile;
    }

    private static FirmwareReleaseResponse toResponse(FirmwareRelease release) {
        return new FirmwareReleaseResponse(
                release.getVersion(),
                release.getNotes(),
                release.getSha256(),
                release.getSizeBytes(),
                release.getCreatedAt());
    }

    private static String sha256(byte[] firmware) {
        try {
            return HexFormat.of().formatHex(MessageDigest.getInstance("SHA-256").digest(firmware));
        } catch (NoSuchAlgorithmException exception) {
            throw new IllegalStateException("SHA-256 is required by the Java runtime", exception);
        }
    }

    private static void deleteAfterFailure(Path path, Exception originalException) {
        try {
            Files.deleteIfExists(path);
        } catch (IOException cleanupException) {
            originalException.addSuppressed(cleanupException);
        }
    }
}
