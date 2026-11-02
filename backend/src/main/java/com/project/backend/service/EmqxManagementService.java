package com.project.backend.service;

import com.fasterxml.jackson.databind.JsonNode;
import java.util.Map;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.http.HttpStatus;
import org.springframework.http.MediaType;
import org.springframework.stereotype.Service;
import org.springframework.web.client.RestClient;
import org.springframework.web.client.RestClientException;
import org.springframework.web.client.RestClientResponseException;

import com.project.backend.exception.DeviceCommandException;

@Service
public class EmqxManagementService {
    private static final Logger logger = LoggerFactory.getLogger(EmqxManagementService.class);
    private static final String AUTHENTICATOR_USERS_PATH =
            "/authentication/{authenticatorId}/users";

    private final RestClient restClient;
    private final String baseUrl;
    private final String apiKey;
    private final String apiSecret;
    private final String authenticatorId;

    public EmqxManagementService(
            RestClient.Builder restClientBuilder,
            @Value("${app.emqx.management.base-url:}") String baseUrl,
            @Value("${app.emqx.management.api-key:}") String apiKey,
            @Value("${app.emqx.management.api-secret:}") String apiSecret,
            @Value("${app.emqx.management.authenticator-id:password_based:built_in_database}") String authenticatorId) {
        this.baseUrl = baseUrl;
        this.apiKey = apiKey;
        this.apiSecret = apiSecret;
        this.authenticatorId = authenticatorId;
        this.restClient = restClientBuilder.clone().baseUrl(baseUrl).build();
    }

    public void createDeviceUser(String username, String password) {
        requireConfiguration();
        try {
            restClient.post()
                    .uri(AUTHENTICATOR_USERS_PATH, authenticatorId)
                    .headers(headers -> headers.setBasicAuth(apiKey, apiSecret))
                    .contentType(MediaType.APPLICATION_JSON)
                    .body(Map.of(
                            "user_id", username,
                            "password", password,
                            "is_superuser", false))
                    .retrieve()
                    .toBodilessEntity();
        } catch (RestClientResponseException exception) {
            logger.error("EMQX rejected device-user creation with HTTP {}", exception.getStatusCode().value());
            throw new DeviceCommandException(
                    "Không thể tạo MQTT user trên EMQX; thiết bị chưa được tạo", exception);
        } catch (RestClientException exception) {
            logger.error("EMQX device-user creation request failed", exception);
            throw new DeviceCommandException(
                    "Không thể kết nối EMQX để tạo MQTT user; thiết bị chưa được tạo", exception);
        }
    }

    public void deleteDeviceUser(String username) {
        if (username == null || username.isBlank()) {
            return;
        }
        requireConfiguration();
        try {
            disconnectDeviceClients(username);
            restClient.delete()
                    .uri(AUTHENTICATOR_USERS_PATH + "/{username}", authenticatorId, username)
                    .headers(headers -> headers.setBasicAuth(apiKey, apiSecret))
                    .retrieve()
                    .toBodilessEntity();
        } catch (RestClientResponseException exception) {
            if (exception.getStatusCode() == HttpStatus.NOT_FOUND) {
                return;
            }
            logger.error("EMQX rejected device-user deletion with HTTP {}", exception.getStatusCode().value());
            throw new DeviceCommandException(
                    "Không thể xóa MQTT user trên EMQX; thiết bị chưa được ngừng hoạt động", exception);
        } catch (RestClientException exception) {
            logger.error("EMQX device-user deletion request failed", exception);
            throw new DeviceCommandException(
                    "Không thể kết nối EMQX để xóa MQTT user; thiết bị chưa được ngừng hoạt động", exception);
        }
    }

    private void disconnectDeviceClients(String username) {
        JsonNode response;
        try {
            response = restClient.get()
                    .uri(uriBuilder -> uriBuilder
                            .path("/clients")
                            .queryParam("username", username)
                            .build())
                    .headers(headers -> headers.setBasicAuth(apiKey, apiSecret))
                    .retrieve()
                    .body(JsonNode.class);
        } catch (RestClientResponseException exception) {
            logger.error("EMQX rejected device-client lookup with HTTP {}", exception.getStatusCode().value());
            throw new DeviceCommandException(
                    "Không thể kiểm tra phiên MQTT trước khi ngừng thiết bị", exception);
        } catch (RestClientException exception) {
            logger.error("EMQX device-client lookup failed", exception);
            throw new DeviceCommandException(
                    "Không thể kiểm tra phiên MQTT trước khi ngừng thiết bị", exception);
        }

        JsonNode sessions = response == null ? null : response.path("data");
        if (sessions == null || !sessions.isArray()) {
            logger.error("EMQX returned an invalid client-list response");
            throw new DeviceCommandException("EMQX trả về danh sách phiên MQTT không hợp lệ");
        }

        for (JsonNode session : sessions) {
            if (!session.path("connected").asBoolean(false)) {
                continue;
            }
            String clientId = session.path("clientid").asText();
            if (clientId.isBlank()) {
                logger.error("EMQX returned a connected client session without a client ID");
                throw new DeviceCommandException("EMQX trả về phiên MQTT thiếu client ID");
            }
            disconnectClient(clientId);
        }
    }

    private void disconnectClient(String clientId) {
        try {
            restClient.delete()
                    .uri("/clients/{clientId}", clientId)
                    .headers(headers -> headers.setBasicAuth(apiKey, apiSecret))
                    .retrieve()
                    .toBodilessEntity();
        } catch (RestClientResponseException exception) {
            if (exception.getStatusCode() == HttpStatus.NOT_FOUND) {
                return;
            }
            logger.error("EMQX rejected MQTT session disconnection with HTTP {}",
                    exception.getStatusCode().value());
            throw new DeviceCommandException(
                    "Không thể ngắt phiên MQTT của thiết bị trước khi thu hồi quyền", exception);
        } catch (RestClientException exception) {
            logger.error("EMQX MQTT session disconnection request failed", exception);
            throw new DeviceCommandException(
                    "Không thể ngắt phiên MQTT của thiết bị trước khi thu hồi quyền", exception);
        }
    }

    private void requireConfiguration() {
        if (baseUrl.isBlank() || apiKey.isBlank() || apiSecret.isBlank() || authenticatorId.isBlank()) {
            logger.error("EMQX Management API is not configured");
            throw new DeviceCommandException("EMQX Management API chưa được cấu hình");
        }
    }
}
