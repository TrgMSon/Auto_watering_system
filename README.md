# BTL_IoT

## Device API

Device endpoints require a bearer token. Device IDs must belong to the authenticated user.

| Method | Endpoint | Purpose |
| --- | --- | --- |
| GET | `/api/devices/{id}/config` | Read saved device thresholds and mode |
| PUT | `/api/devices/{id}/config` | Save thresholds and publish them to the device |
| POST | `/api/devices/{id}/control` | Publish a manual pump command |
| POST | `/api/devices/{id}/mode` | Save and publish `auto` or `manual` mode |
| GET | `/api/devices/{id}/schedules` | List watering schedules |
| POST | `/api/devices/{id}/schedules` | Create a schedule |
| PUT | `/api/devices/{id}/schedules/{scheduleId}` | Update a schedule |
| DELETE | `/api/devices/{id}/schedules/{scheduleId}` | Delete a schedule |
| POST | `/api/devices/{id}/register` | Register a device to the authenticated user |

Configuration requests use `auto_mode`, `soil_moisture_min`, `soil_moisture_max`,
`max_watering_duration`, `air_humidity_min`, `air_humidity_max`,
`air_temperature_min`, `air_temperature_max`, and `water_level_min`. Control requests use
`{"action":"ON","duration_seconds":300}` or `{"action":"OFF"}`;
`duration_seconds` is optional and defaults to 300 seconds (or the configured
maximum if lower). Mode requests use `{"mode":"auto"}` or `{"mode":"manual"}`.
Schedule requests use `start_time`, `end_time`, `duration_minutes`,
`days_of_week`, and optionally `is_enabled`.

The MQTT topic contract is:

| Purpose | Topic | QoS |
| --- | --- | --- |
| Sensor telemetry | `iot/garden/{device_id}/telemetry` | 0 |
| Pump command | `iot/garden/{device_id}/cmd` | 1 |
| Device configuration | `iot/garden/{device_id}/config` | 1 |
| Device status / LWT | `iot/garden/{device_id}/status` | 1 |
| Command/config acknowledgment | `iot/garden/{device_id}/ack` | 1 |

Commands contain `command_id`, `device_id`, an ISO-8601 UTC `timestamp`,
`action: "PUMP_CONTROL"` and `params` with `state` and `duration_seconds`.
The API rejects pump-on durations above the device's configured maximum
watering duration (in minutes) and sends zero duration when turning it off.
Command and configuration messages both include a `command_id`. After applying
either message, the device publishes an acknowledgment to its `/ack` topic:

```json
{
  "command_id": "CMD-...",
  "device_id": "A4CF128FC130",
  "status": "SUCCESS",
  "message": "Applied",
  "timestamp": "2026-10-14T08:00:00Z"
}
```

Use `status: "FAILED"` and a diagnostic `message` when execution fails.
Configuration messages wrap thresholds in a `config` object and publish to the
configuration topic. The MQTT `min_water_level_alert` threshold is populated
from the device's saved `water_level_min` setting. Command and config API
responses include `command_id` and `command_status: "PENDING"`; poll
`GET /api/devices/{id}/commands/{commandId}/ack` for `PENDING`, `SUCCEEDED`,
`FAILED`, or `PUBLISH_FAILED`. Acknowledgment statuses are kept in memory for
up to 24 hours and are cleared when the backend restarts.

Configure `MQTT_BROKER_URL`, `MQTT_USERNAME`, `MQTT_PASSWORD` and
`MQTT_CLIENT_ID` in `.env`. Set `MQTT_ENABLED=true` to subscribe to telemetry
and status/LWT topics; telemetry is subscribed at QoS 0 and status at QoS 1.
Sensor telemetry should be published by the device every 10–30 seconds.
Control/configuration publishes wait up to 5 seconds for delivery and retry up
to 3 times. When all attempts fail, the API returns `503 Service Unavailable`
so the mobile app can notify the user rather than showing a successful command.