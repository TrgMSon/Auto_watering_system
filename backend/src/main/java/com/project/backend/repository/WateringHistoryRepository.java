package com.project.backend.repository;

import org.springframework.data.jpa.repository.JpaRepository;

import com.project.backend.model.WateringHistory;

public interface WateringHistoryRepository extends JpaRepository<WateringHistory, Integer> {
    
}
