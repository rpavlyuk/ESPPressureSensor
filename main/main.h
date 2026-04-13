#ifndef MAIN_H
#define MAIN_H

#include "freertos/FreeRTOS.h"   // must be first
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"      // if you use queues

#include "common.h"

extern EventGroupHandle_t g_sys_events;


#endif