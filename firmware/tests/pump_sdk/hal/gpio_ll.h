#pragma once
#include "soc/gpio_struct.h"
void fakePinWrite(int,int);
inline void gpio_ll_set_level(gpio_dev_t*,unsigned pin,unsigned level){fakePinWrite(int(pin),int(level));}
