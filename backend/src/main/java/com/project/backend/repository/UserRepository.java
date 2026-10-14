package com.project.backend.repository;

import java.util.Optional;

import org.springframework.data.jpa.repository.JpaRepository;
import org.springframework.data.jpa.repository.Modifying;
import org.springframework.data.jpa.repository.Query;

import com.project.backend.model.User;

import jakarta.transaction.Transactional;

public interface UserRepository extends JpaRepository<User, Integer> {
    Optional<User> findByEmail(String email);

    boolean existsByEmail(String email);

    @Modifying 
    @Transactional 
    @Query(value = """
            UPDATE user SET role = ?1 WHERE id = ?2
            """, nativeQuery = true)
    void assignOwner(String role, int id);
}
