#pragma once
#include "driver/gptimer.h"
struct FakeInterrupt;
using intr_handle_t=FakeInterrupt*;
esp_err_t gptimer_get_intr_handle(gptimer_handle_t,intr_handle_t*);
esp_err_t esp_intr_set_in_iram(intr_handle_t,bool);
