#pragma once

// The esp_qrcode API of the ESP32 core, reimplemented in preview.cpp on top
// of qr_matrix.py (segno), so the QR page can be drawn on a PC.

#include <stdint.h>

typedef const uint8_t *esp_qrcode_handle_t;

typedef struct {
  void (*display_func)(esp_qrcode_handle_t qrcode);
  int max_qrcode_version;
  int qrcode_ecc_level;
} esp_qrcode_config_t;

enum { ESP_QRCODE_ECC_LOW, ESP_QRCODE_ECC_MED, ESP_QRCODE_ECC_QUART, ESP_QRCODE_ECC_HIGH };

int  esp_qrcode_generate(esp_qrcode_config_t *cfg, const char *text);
int  esp_qrcode_get_size(esp_qrcode_handle_t qrcode);
bool esp_qrcode_get_module(esp_qrcode_handle_t qrcode, int x, int y);
