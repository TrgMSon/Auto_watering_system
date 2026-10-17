package com.project.backend.repository;

import java.util.List;
import java.util.Optional;

import org.springframework.data.jpa.repository.JpaRepository;

import com.project.backend.model.Schedule;

public interface ScheduleRepository extends JpaRepository<Schedule, Integer> {
    List<Schedule> findAllByDevice_IdOrderByStartTimeAsc(String deviceId);

    Optional<Schedule> findByIdAndDevice_Id(int id, String deviceId);
}
