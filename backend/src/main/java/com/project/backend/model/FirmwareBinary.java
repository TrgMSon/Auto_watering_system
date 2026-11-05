package com.project.backend.model;

import jakarta.persistence.Column;
import jakarta.persistence.Entity;
import jakarta.persistence.Id;
import jakarta.persistence.Lob;
import jakarta.persistence.Table;
import lombok.Data;

@Entity
@Table(name = "firmware_binary")
@Data
public class FirmwareBinary {
    @Id
    @Column(name = "release_id")
    private Long releaseId;

    @Lob
    @Column(name = "firmware_data", nullable = false, columnDefinition = "MEDIUMBLOB")
    private byte[] firmwareData;
}
