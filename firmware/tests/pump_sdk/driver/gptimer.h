#pragma once
#include <stdint.h>
using esp_err_t=int;
constexpr int ESP_OK=0,ESP_FAIL=-1,ESP_ERR_INVALID_STATE=0x103,ESP_ERR_INVALID_ARG=0x102;
struct FakeTimer;
using gptimer_handle_t=FakeTimer*;
struct gptimer_alarm_event_data_t {};
struct gptimer_config_t {
 int clk_src=0,direction=0;uint32_t resolution_hz=0;
 struct { unsigned intr_shared:1; } flags={};
};
constexpr int GPTIMER_CLK_SRC_DEFAULT=1,GPTIMER_COUNT_UP=1;
using gptimer_alarm_cb_t=bool(*)(gptimer_handle_t,const gptimer_alarm_event_data_t*,void*);
struct gptimer_event_callbacks_t { gptimer_alarm_cb_t on_alarm=nullptr; };
struct gptimer_alarm_config_t {
 uint64_t alarm_count=0,reload_count=0;
 struct { unsigned auto_reload_on_alarm:1; } flags={};
};
esp_err_t gptimer_new_timer(const gptimer_config_t*,gptimer_handle_t*);
esp_err_t gptimer_register_event_callbacks(gptimer_handle_t,const gptimer_event_callbacks_t*,void*);
esp_err_t gptimer_set_alarm_action(gptimer_handle_t,const gptimer_alarm_config_t*);
esp_err_t gptimer_enable(gptimer_handle_t);
esp_err_t gptimer_start(gptimer_handle_t);
esp_err_t gptimer_stop(gptimer_handle_t);
esp_err_t gptimer_disable(gptimer_handle_t);
esp_err_t gptimer_del_timer(gptimer_handle_t);
