#pragma once

#include "irrigation.h"

namespace plant_ota {
bool begin();
void ownerService(irrigation::System& system);
void confirmBoot();
}
