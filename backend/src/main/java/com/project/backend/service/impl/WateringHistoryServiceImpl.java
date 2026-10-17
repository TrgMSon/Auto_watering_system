package com.project.backend.service.impl;

import java.time.LocalDateTime;
import java.util.Objects;

import org.springframework.stereotype.Service;

import com.project.backend.exception.NotFoundException;
import com.project.backend.model.Device;
import com.project.backend.model.WateringHistory;
import com.project.backend.repository.DeviceRepository;
import com.project.backend.repository.WateringHistoryRepository;
import com.project.backend.service.WateringHistoryService;

@Service
public class WateringHistoryServiceImpl implements WateringHistoryService {
        private final WateringHistoryRepository wateringHistoryRepository;
        private final DeviceRepository deviceRepository;

        public WateringHistoryServiceImpl(WateringHistoryRepository wateringHistoryRepository,
                        DeviceRepository deviceRepository) {
                this.wateringHistoryRepository = wateringHistoryRepository;
                this.deviceRepository = deviceRepository;
        }

        @Override
        public void recordStartWatering(String deviceId, LocalDateTime ackTime) {
                Device device = deviceRepository
                                .findById(Objects.requireNonNull(deviceId, "Không tìm thấy thiết bị"))
                                .orElseThrow(() -> new NotFoundException("Không tìm thấy thiết bị"));

                WateringHistory record = WateringHistory.builder()
                                .device(device)
                                .startTime(ackTime)
                                .status("running")
                                .build();

                wateringHistoryRepository.save(Objects.requireNonNull(record, "Không tìm thấy bản ghi tưới cây"));
        }

        @Override
        public void recordEndWatering(String deviceId, LocalDateTime ackTime) {
                Device device = deviceRepository
                                .findById(Objects.requireNonNull(deviceId, "Không tìm thấy thiết bị"))
                                .orElseThrow(() -> new NotFoundException("Không tìm thấy thiết bị"));

                WateringHistory recordStartWatering = wateringHistoryRepository.findStartWateringRecord(device.getId());

                wateringHistoryRepository.updateEndWatering(ackTime, "stop", recordStartWatering.getId());
        }
}
