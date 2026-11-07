#pragma once
#include "sensors.h"
namespace plant_mqtt {
bool begin();
// Owner calls with a value snapshot; one-slot mailbox, no sensor/SDK access.
void publish(const sensors::Reading& reading);
}
