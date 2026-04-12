#ifndef CERTIFICATE_MANAGER_H
#define CERTIFICATE_MANAGER_H

#include <stdio.h>
#include <stdbool.h>
#include "esp_err.h"

// NVS namespace to store the CA certificate
#define S_NAMESPACE                 "settings"
// Sync the CA certificate to SPIFFS
#define DO_SYNC_CA_CERT_TO_SPIFFS   false

static const char *CRT_MGR_TAG = "Certificate Manager";

/* Routines */

esp_err_t load_ca_certificate(char **ca_cert, const char *ca_cert_path);
esp_err_t save_ca_certificate(const char *ca_cert, const char *ca_cert_path, bool create_if_not_exist);

const char *esp_basename(const char *path);

#endif