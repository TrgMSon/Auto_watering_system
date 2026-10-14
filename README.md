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
`air_temperature_min`, and `air_temperature_max`. Control requests use
`{"action":"ON"}` or `{"action":"OFF"}`; mode requests use `{"mode":"auto"}` or
`{"mode":"manual"}`. Schedule requests use `start_time`, `end_time`,
`duration_minutes`, `days_of_week`, and optionally `is_enabled`.

The backend publishes device commands to MQTT topic
`devices/{deviceId}/control` by default. Configure `MQTT_BROKER_URL`,
`MQTT_USERNAME`, `MQTT_PASSWORD`, and optionally `MQTT_CONTROL_TOPIC` in `.env`.
The topic template uses `%s` for the device ID (a literal `{deviceId}` from an
environment variable is also accepted). Commands are JSON messages published
with QoS 1. If MQTT is not configured or publishing fails, the API returns
`503 Service Unavailable` instead of reporting a command as sent.