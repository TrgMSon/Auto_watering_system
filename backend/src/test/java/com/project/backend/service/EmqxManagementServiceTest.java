package com.project.backend.service;

import static org.junit.jupiter.api.Assertions.assertThrows;
import static org.junit.jupiter.api.Assertions.assertTrue;
import static org.mockito.Mockito.verify;
import static org.springframework.test.web.client.match.MockRestRequestMatchers.content;
import static org.springframework.test.web.client.match.MockRestRequestMatchers.header;
import static org.springframework.test.web.client.match.MockRestRequestMatchers.method;
import static org.springframework.test.web.client.match.MockRestRequestMatchers.requestTo;
import static org.springframework.test.web.client.response.MockRestResponseCreators.withNoContent;
import static org.springframework.test.web.client.response.MockRestResponseCreators.withSuccess;

import org.junit.jupiter.api.AfterEach;
import org.junit.jupiter.api.BeforeEach;
import org.junit.jupiter.api.Test;
import org.springframework.http.HttpMethod;
import org.springframework.web.client.RestClient;
import org.springframework.test.web.client.MockRestServiceServer;

import com.project.backend.exception.DeviceCommandException;

class EmqxManagementServiceTest {
    private MockRestServiceServer mockServer;
    private EmqxManagementService service;

    @BeforeEach
    void setUp() {
        RestClient.Builder builder = RestClient.builder();
        mockServer = MockRestServiceServer.bindTo(builder).build();
        service = new EmqxManagementService(
                builder,
                "https://emqx.example:18084/api/v5",
                "api-key",
                "api-secret",
                "password_based:built_in_database");
    }

    @AfterEach
    void verifyRequests() {
        mockServer.verify();
    }

    @Test
    void createsDeviceAccountInConfiguredBuiltInAuthenticator() {
        mockServer.expect(requestTo(
                        "https://emqx.example:18084/api/v5/authentication/password_based%3Abuilt_in_database/users"))
                .andExpect(method(HttpMethod.POST))
                .andExpect(header("Authorization", org.hamcrest.Matchers.startsWith("Basic ")))
                .andExpect(content().json("""
                        {
                          "user_id": "dev_AABBCCDDEEFF",
                          "password": "device-secret",
                          "is_superuser": false
                        }
                        """))
                .andRespond(withSuccess());

        service.createDeviceUser("dev_AABBCCDDEEFF", "device-secret");
    }

    @Test
    void deletesDeviceAccountFromConfiguredBuiltInAuthenticator() {
        mockServer.expect(requestTo(
                        "https://emqx.example:18084/api/v5/clients?username=dev_AABBCCDDEEFF"))
                .andExpect(method(HttpMethod.GET))
                .andExpect(header("Authorization", org.hamcrest.Matchers.startsWith("Basic ")))
                .andRespond(withSuccess("""
                        {"data":[],"meta":{"count":0}}
                        """, org.springframework.http.MediaType.APPLICATION_JSON));
        mockServer.expect(requestTo(
                        "https://emqx.example:18084/api/v5/authentication/password_based%3Abuilt_in_database/users/dev_AABBCCDDEEFF"))
                .andExpect(method(HttpMethod.DELETE))
                .andExpect(header("Authorization", org.hamcrest.Matchers.startsWith("Basic ")))
                .andRespond(withNoContent());

        service.deleteDeviceUser("dev_AABBCCDDEEFF");
    }

    @Test
    void disconnectsActiveSessionsBeforeDeletingDeviceAccount() {
        mockServer.expect(requestTo(
                        "https://emqx.example:18084/api/v5/clients?username=dev_AABBCCDDEEFF"))
                .andExpect(method(HttpMethod.GET))
                .andRespond(withSuccess("""
                        {"data":[
                          {"clientid":"esp32-device-1","connected":true},
                          {"clientid":"old-session","connected":false}
                        ],"meta":{"count":2}}
                        """, org.springframework.http.MediaType.APPLICATION_JSON));
        mockServer.expect(requestTo(
                        "https://emqx.example:18084/api/v5/clients/esp32-device-1"))
                .andExpect(method(HttpMethod.DELETE))
                .andRespond(withNoContent());
        mockServer.expect(requestTo(
                        "https://emqx.example:18084/api/v5/authentication/password_based%3Abuilt_in_database/users/dev_AABBCCDDEEFF"))
                .andExpect(method(HttpMethod.DELETE))
                .andRespond(withNoContent());

        service.deleteDeviceUser("dev_AABBCCDDEEFF");
    }

    @Test
    void missingManagementApiConfigurationFailsExplicitly() {
        EmqxManagementService unconfigured = new EmqxManagementService(
                RestClient.builder(), "", "", "", "password_based:built_in_database");

        assertThrows(DeviceCommandException.class,
                () -> unconfigured.createDeviceUser("dev_AABBCCDDEEFF", "device-secret"));
    }

    @Test
    void missingEmqxUserIsTreatedAsAlreadyDeleted() {
        mockServer.expect(requestTo(
                        "https://emqx.example:18084/api/v5/clients?username=dev_AABBCCDDEEFF"))
                .andExpect(method(HttpMethod.GET))
                .andRespond(withSuccess("""
                        {"data":[],"meta":{"count":0}}
                        """, org.springframework.http.MediaType.APPLICATION_JSON));
        mockServer.expect(requestTo(
                        "https://emqx.example:18084/api/v5/authentication/password_based%3Abuilt_in_database/users/dev_AABBCCDDEEFF"))
                .andExpect(method(HttpMethod.DELETE))
                .andRespond(org.springframework.test.web.client.response.MockRestResponseCreators.withStatus(
                        org.springframework.http.HttpStatus.NOT_FOUND));

        service.deleteDeviceUser("dev_AABBCCDDEEFF");
    }

    @Test
    void doesNotDeleteBrokerUserWhenSessionLookupFails() {
        mockServer.expect(requestTo(
                        "https://emqx.example:18084/api/v5/clients?username=dev_AABBCCDDEEFF"))
                .andExpect(method(HttpMethod.GET))
                .andRespond(org.springframework.test.web.client.response.MockRestResponseCreators
                        .withStatus(org.springframework.http.HttpStatus.FORBIDDEN));

        DeviceCommandException exception = assertThrows(
                DeviceCommandException.class,
                () -> service.deleteDeviceUser("dev_AABBCCDDEEFF"));

        assertTrue(exception.getMessage().contains("phiên MQTT"));
    }
}
