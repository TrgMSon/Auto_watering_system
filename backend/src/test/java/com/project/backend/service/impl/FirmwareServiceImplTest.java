package com.project.backend.service.impl;

import static org.junit.jupiter.api.Assertions.assertEquals;
import static org.junit.jupiter.api.Assertions.assertFalse;
import static org.junit.jupiter.api.Assertions.assertNull;
import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import java.nio.file.Files;
import java.nio.file.Path;
import java.time.LocalDateTime;
import java.util.HexFormat;
import java.util.Optional;

import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.junit.jupiter.api.io.TempDir;
import org.junit.jupiter.api.extension.ExtendWith;
import org.mockito.Mock;
import org.mockito.junit.jupiter.MockitoExtension;
import org.springframework.dao.DataIntegrityViolationException;
import org.springframework.mock.web.MockMultipartFile;

import com.project.backend.exception.ConflictException;
import com.project.backend.exception.DeviceCommandException;
import com.project.backend.exception.IllegalArgumentException;
import com.project.backend.exception.NotFoundException;
import com.project.backend.model.FirmwareRelease;
import com.project.backend.repository.FirmwareReleaseRepository;

@ExtendWith(MockitoExtension.class)
class FirmwareServiceImplTest {
    @Mock
    private FirmwareReleaseRepository firmwareReleaseRepository;

    @TempDir
    private Path storageDirectory;

    private FirmwareServiceImpl firmwareService;

    @BeforeEach
    void setUp() {
        firmwareService = new FirmwareServiceImpl(firmwareReleaseRepository, storageDirectory.toString(), 1024);
    }

    @SuppressWarnings("null")
    @Test
    void storesEsp32BinaryAndReturnsItsChecksum() throws Exception {
        byte[] bytes = {(byte) 0xE9, 1, 2, 3};
        MockMultipartFile file = firmwareFile("garden.bin", bytes);
        when(firmwareReleaseRepository.save(any(FirmwareRelease.class)))
                .thenAnswer(invocation -> invocation.getArgument(0));

        var response = firmwareService.upload("1.2.0", "Sensor fix", file);

        assertEquals("1.2.0", response.version());
        assertEquals("Sensor fix", response.notes());
        assertEquals(HexFormat.of().formatHex(
                java.security.MessageDigest.getInstance("SHA-256").digest(bytes)), response.sha256());
        assertEquals(bytes.length, response.sizeBytes());
        Path storedFile;
        try (var files = Files.list(storageDirectory)) {
            storedFile = files.findFirst().orElseThrow();
        }
        assertTrue(storedFile.getFileName().toString().endsWith(".bin"));
        assertTrue(java.util.Arrays.equals(bytes, Files.readAllBytes(storedFile)));
    }

    @SuppressWarnings("null")
    @Test
    void rejectsDuplicateVersionBeforeWritingBinary() {
        when(firmwareReleaseRepository.existsByVersion("1.2.0")).thenReturn(true);

        assertThrows(ConflictException.class,
                () -> firmwareService.upload("1.2.0", null, firmwareFile("garden.bin", validFirmware())));

        verify(firmwareReleaseRepository, never()).save(any(FirmwareRelease.class));
    }

    @SuppressWarnings("null")
    @Test
    void rejectsInvalidEsp32ImageAndUnsupportedFileType() {
        assertThrows(IllegalArgumentException.class,
                () -> firmwareService.upload("1.2.0", null, firmwareFile("garden.ino", validFirmware())));
        assertThrows(IllegalArgumentException.class,
                () -> firmwareService.upload("1.2.0", null, firmwareFile("garden.bin", new byte[] {1, 2, 3})));

        verify(firmwareReleaseRepository, never()).save(any(FirmwareRelease.class));
    }

    @SuppressWarnings("null")
    @Test
    void rejectsInvalidVersionAndFilesAboveConfiguredLimit() {
        assertThrows(IllegalArgumentException.class,
                () -> firmwareService.upload("../latest", null, firmwareFile("garden.bin", validFirmware())));
        assertThrows(IllegalArgumentException.class,
                () -> firmwareService.upload(
                        "1.2.0", null, firmwareFile("garden.bin", new byte[1025])));

        verify(firmwareReleaseRepository, never()).save(any(FirmwareRelease.class));
    }

    @Test
    void returnsUpdateMetadataWhenDeviceVersionDiffersFromLatestRelease() {
        FirmwareRelease latest = release("2.0.0", "new.bin", "f".repeat(64), 100, 2);
        when(firmwareReleaseRepository.findFirstByOrderByCreatedAtDescIdDesc()).thenReturn(Optional.of(latest));

        var response = firmwareService.checkForUpdate("1.0.0");

        assertTrue(response.updateAvailable());
        assertEquals("2.0.0", response.latestVersion());
        assertEquals("/api/firmware/2.0.0/download", response.downloadUrl());
        assertEquals(latest.getSha256(), response.sha256());
    }

    @Test
    void reportsNoUpdateWhenDeviceAlreadyHasLatestVersion() {
        when(firmwareReleaseRepository.findFirstByOrderByCreatedAtDescIdDesc())
                .thenReturn(Optional.of(release("2.0.0", "new.bin", "f".repeat(64), 100, 2)));

        var response = firmwareService.checkForUpdate("2.0.0");

        assertFalse(response.updateAvailable());
        assertEquals("2.0.0", response.latestVersion());
        assertNull(response.downloadUrl());
    }

    @Test
    void reportsNoUpdateWhenThereAreNoFirmwareReleases() {
        when(firmwareReleaseRepository.findFirstByOrderByCreatedAtDescIdDesc()).thenReturn(Optional.empty());

        var response = firmwareService.checkForUpdate("1.0.0");

        assertFalse(response.updateAvailable());
        assertNull(response.latestVersion());
    }

    @Test
    void downloadsOnlyKnownFirmwareFilesAndReportsMissingStoredFiles() throws Exception {
        FirmwareRelease release = release("1.0.0", "stored.bin", "f".repeat(64), 4, 1);
        when(firmwareReleaseRepository.findByVersion("1.0.0")).thenReturn(Optional.of(release));
        Path file = storageDirectory.resolve("stored.bin");
        Files.write(file, validFirmware());

        assertEquals(file, firmwareService.getFirmwareFile("1.0.0"));

        Files.delete(file);
        assertThrows(DeviceCommandException.class, () -> firmwareService.getFirmwareFile("1.0.0"));
        when(firmwareReleaseRepository.findByVersion("missing")).thenReturn(Optional.empty());
        assertThrows(NotFoundException.class, () -> firmwareService.getFirmwareFile("missing"));
    }

    @SuppressWarnings("null")
    @Test
    void removesStoredFileWhenVersionConstraintFailsDuringSave() throws Exception {
        when(firmwareReleaseRepository.save(any(FirmwareRelease.class)))
                .thenThrow(new DataIntegrityViolationException("duplicate version"));

        assertThrows(ConflictException.class,
                () -> firmwareService.upload("1.2.0", null, firmwareFile("garden.bin", validFirmware())));

        try (var files = Files.list(storageDirectory)) {
            assertTrue(files.findAny().isEmpty());
        }
    }

    private static MockMultipartFile firmwareFile(String name, byte[] bytes) {
        return new MockMultipartFile("file", name, "application/octet-stream", bytes);
    }

    private static byte[] validFirmware() {
        return new byte[] {(byte) 0xE9, 1, 2, 3};
    }

    private static FirmwareRelease release(String version, String filename, String checksum, long size, int day) {
        FirmwareRelease release = new FirmwareRelease();
        release.setVersion(version);
        release.setStoredFileName(filename);
        release.setSha256(checksum);
        release.setSizeBytes(size);
        release.setCreatedAt(LocalDateTime.of(2026, 11, day, 9, 0));
        return release;
    }
}
