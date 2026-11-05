CREATE TABLE firmware_release (
    id BIGINT NOT NULL AUTO_INCREMENT,
    version VARCHAR(64) NOT NULL,
    notes VARCHAR(500) NULL,
    stored_file_name VARCHAR(80) NOT NULL,
    sha256 CHAR(64) NOT NULL,
    size_bytes BIGINT NOT NULL,
    created_at DATETIME NOT NULL,
    PRIMARY KEY (id),
    UNIQUE KEY uk_firmware_release_version (version),
    UNIQUE KEY uk_firmware_release_file_name (stored_file_name)
);
