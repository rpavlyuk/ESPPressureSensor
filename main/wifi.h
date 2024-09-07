#ifndef WIFI_H
#define WIFI_H

#include "common.h"
#include "esp_event.h"

void generate_softap_credentials();
static void wifi_provisioning_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void initialize_wifi();
void start_wifi(bool provisioned);


#endif