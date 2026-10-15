package com.project.backend.service.impl;

import java.util.Objects;

import org.springframework.stereotype.Service;

import com.project.backend.dto.wateringHistory.RecordHistoryRequest;
import com.project.backend.exception.NotFoundException;
import com.project.backend.model.Device;
import com.project.backend.model.User;
import com.project.backend.model.WateringHistory;
import com.project.backend.repository.DeviceRepository;
import com.project.backend.repository.UserRepository;
import com.project.backend.repository.WateringHistoryRepository;
import com.project.backend.service.WateringHistoryService;

@Service
public class WateringHistoryServiceImpl implements WateringHistoryService {
        private final WateringHistoryRepository wateringHistoryRepository;
        private final UserRepository userRepository;
        private final DeviceRepository deviceRepository;

        public WateringHistoryServiceImpl(WateringHistoryRepository wateringHistoryRepository,
                        UserRepository userRepository, DeviceRepository deviceRepository) {
                this.wateringHistoryRepository = wateringHistoryRepository;
                this.userRepository = userRepository;
                this.deviceRepository = deviceRepository;
        }

        @Override
        public void recordWateringHistory(RecordHistoryRequest request) {
                Device device = deviceRepository
                                .findById(Objects.requireNonNull(request.getDeviceId(), "Không tìm thấy thiết bị"))
                                .orElseThrow(() -> new NotFoundException("Không tìm thấy thiết bị"));
                User user = userRepository.findById(request.getUserId())
                                .orElseThrow(() -> new NotFoundException("Không tìm thấy người dùng"));

                WateringHistory record = WateringHistory.builder()
                                .device(device)
                                .user(user)
                                .startTime(request.getStartTime())
                                .endTime(request.getEndTime())
                                .status(request.getStatus())
                                .build();

                wateringHistoryRepository.save(Objects.requireNonNull(record, "Không tìm thấy bản ghi tưới cây"));
        }
}
