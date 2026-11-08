#pragma once
// Exact field names/order from DeviceServiceImpl.configPayload and the publisher
// envelope. Fixed UUID/time replace the publisher's generated fixture values.
constexpr char kBackendConfig[] =
    "{\"command_id\":\"CMD-12345678-1234-1234-1234-123456789abc\","
    "\"device_id\":\"02:03:04:05:06:07\",\"timestamp\":\"2026-11-08T08:00:00Z\","
    "\"config\":{\"auto_mode\":true,\"soil_moisture_min\":30.0,\"soil_moisture_max\":70.0,"
    "\"air_humidity_min\":50.0,\"air_humidity_max\":80.0,"
    "\"air_temperature_min\":20.0,\"air_temperature_max\":35.0,\"min_water_level_alert\":20}}";
