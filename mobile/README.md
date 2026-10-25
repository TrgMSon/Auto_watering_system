# Smart Watering Mobile

Flutter app for the IoT watering backend.

## Connect to the backend

The default API address targets the Android emulator host:

```text
http://10.0.2.2:8080/api
```

For a physical device, set the backend host reachable on the same network:

```powershell
flutter run --dart-define=API_BASE_URL=http://192.168.1.20:8080/api
```

Replace the example address with the backend machine's LAN IP. The Android
debug manifest permits cleartext HTTP for local development; production
deployments should use HTTPS.

The app logs in through `/api/auth/login`, keeps access/refresh tokens in
secure storage, refreshes expired access tokens, and revokes the refresh token
on logout. Requests use the backend's `/api` routes and `ApiResponse.data`
envelope.

## Device data and MQTT

The mobile app does not contain MQTT broker credentials and does not connect
directly to the broker. It sends authenticated pump/configuration/schedule
requests to the backend; the backend publishes pump/configuration commands to
MQTT and processes the device acknowledgments.

MQTT telemetry received by the backend is saved to `sensor_log`. The app reads
the authorized device list and sensor readings through the secured device
endpoints. The dashboard polls the latest stored reading every five seconds;
sensor history is requested for the selected date range. An active MQTT
broker and ESP32 publishing `iot/garden/{deviceId}/telemetry` are required for
new readings to appear.

Backend routes used by the app:

| Route | Purpose |
| --- | --- |
| `POST /api/auth/login` | Login |
| `POST /api/auth/refresh-token` | Rotate access/refresh tokens |
| `POST /api/auth/logout` | Revoke refresh token |
| `GET /api/devices` | List devices associated with the user |
| `GET /api/devices/{id}/telemetry/latest` | Latest MQTT telemetry saved by backend |
| `GET /api/devices/{id}/telemetry?from=...&to=...` | Sensor history |
| `POST /api/devices/{id}/control` | Send pump command via backend MQTT |
| `GET /api/devices/{id}/commands/{commandId}/ack` | Wait for MQTT command acknowledgment |
| `GET`, `PUT /api/devices/{id}/config` | Get/update automation thresholds |
| `GET`, `POST /api/devices/{id}/schedules` | Read/create schedules |
| `PUT`, `DELETE /api/devices/{id}/schedules/{scheduleId}` | Update/delete schedules |

The backend currently stores no light-intensity or pump-state fields in sensor
history. The corresponding mobile display fields therefore remain unavailable
from telemetry; pump commands are shown as successful only after a backend
MQTT acknowledgment.

Device management is currently read-only: the backend does not expose
user-facing device registration, renaming, or unlinking routes. Use Dashboard
for pump control; registering a device remains an administrative backend flow.
The user-management tab also remains a UI placeholder because the backend
does not currently expose the user-list/role-management routes it would need.
