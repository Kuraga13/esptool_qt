/*  Copyright (C) 2024 Kuraga Tech
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 */

#include "esp32s2.h"
#include "../esptoolqt.h"

#include <QDebug>

using std::vector;

namespace {
    const uint32_t EFUSE_BASE = 0x3F41A000;
    const uint32_t EFUSE_BLOCK1_ADDR = EFUSE_BASE + 0x044;
    const uint32_t EFUSE_BLOCK2_ADDR = EFUSE_BASE + 0x05C;

    uint32_t get_pkg_version(void* _esp_tool) {
        EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
        uint32_t num_word = 4;
        return (esp_tool->read_reg(EFUSE_BLOCK1_ADDR + (4 * num_word)) >> 0) & 0x0F;
    }

    uint32_t get_minor_chip_version(void* _esp_tool) {
        EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
        uint32_t hi_num_word = 3;
        uint32_t hi = (esp_tool->read_reg(EFUSE_BLOCK1_ADDR + (4 * hi_num_word)) >> 20) & 0x01;
        uint32_t low_num_word = 4;
        uint32_t low = (esp_tool->read_reg(EFUSE_BLOCK1_ADDR + (4 * low_num_word)) >> 4) & 0x07;
        return (hi << 3) + low;
    }

    uint32_t get_major_chip_version(void* _esp_tool) {
        EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
        uint32_t num_word = 3;
        return (esp_tool->read_reg(EFUSE_BLOCK1_ADDR + (4 * num_word)) >> 18) & 0x03;
    }

    uint32_t get_flash_version(void* _esp_tool) {
        EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
        uint32_t num_word = 3;
        return (esp_tool->read_reg(EFUSE_BLOCK1_ADDR + (4 * num_word)) >> 21) & 0x0F;
    }

    uint32_t get_flash_cap(void* esp_tool) {
        return get_flash_version(esp_tool);
    }

    uint32_t get_psram_version(void* _esp_tool) {
        EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
        uint32_t num_word = 3;
        return (esp_tool->read_reg(EFUSE_BLOCK1_ADDR + (4 * num_word)) >> 28) & 0x0F;
    }

    uint32_t get_psram_cap(void* esp_tool) {
        return get_psram_version(esp_tool);
    }

    uint32_t get_block2_version(void* _esp_tool) {
        // BLK_VERSION_MINOR
        EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
        uint32_t num_word = 4;
        return (esp_tool->read_reg(EFUSE_BLOCK2_ADDR + (4 * num_word)) >> 4) & 0x07;
    }

}

bool Esp32S2::get_chip_description(QString* description, void* esp_tool) {
    uint32_t flash_cap = get_flash_cap(esp_tool);
    uint32_t psram_cap = get_psram_cap(esp_tool);
    uint32_t major_rev = get_major_chip_version(esp_tool);
    uint32_t minor_rev = get_minor_chip_version(esp_tool);

    QString chip_name;
    switch (flash_cap + psram_cap * 100) {
        case 0:
            chip_name = "ESP32-S2";
            break;
        case 1:
            chip_name = "ESP32-S2FH2";
            break;
        case 2:
            chip_name = "ESP32-S2FH4";
            break;
        case 102:
            chip_name = "ESP32-S2FNR2";
            break;
        case 100:
            chip_name = "ESP32-S2R2";
            break;
        default:
            chip_name = "unknown ESP32-S2";
            break;
    }

    *description = QString("%1 (revision v%2.%3)").arg(chip_name, QString::number(major_rev), QString::number(minor_rev));
    return true;
}

bool Esp32S2::get_chip_features(QString* features, void* esp_tool) {
    *features += "WiFi";

    QString flash_version;
    switch (get_flash_cap(esp_tool)) {
        case 0:
            flash_version = "No Embedded Flash";
            break;
        case 1:
            flash_version = "Embedded Flash 2MB";
            break;
        case 2:
            flash_version = "Embedded Flash 4MB";
            break;
        default:
            flash_version = "Unknown Embedded Flash";
            break;
    }
    *features += QString(", %1").arg(flash_version);

    QString psram_version;
    switch (get_psram_cap(esp_tool)) {
        case 0:
            psram_version = "No Embedded PSRAM";
            break;
        case 1:
            psram_version = "Embedded PSRAM 2MB";
            break;
        case 2:
            psram_version = "Embedded PSRAM 4MB";
            break;
        default:
            psram_version = "Unknown Embedded PSRAM";
            break;
    }
    *features += QString(", %1").arg(psram_version);

    QString block2_version;
    switch (get_block2_version(esp_tool)) {
        case 0:
            block2_version = "No calibration in BLK2 of efuse";
            break;
        case 1:
            block2_version = "ADC and temperature sensor calibration in BLK2 of efuse V1";
            break;
        case 2:
            block2_version = "ADC and temperature sensor calibration in BLK2 of efuse V2";
            break;
        default:
            block2_version = "Unknown Calibration in BLK2";
            break;
    }
    *features += QString(", %1").arg(block2_version);
    return true;
}

uint32_t Esp32S2::get_crystal_freq(void* esp_tool) {
    // ESP32-S2 XTAL is fixed to 40MHz
    return 40;
}

uint32_t Esp32S2::get_uart_no(void* _esp_tool) {
    EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
    // Read the UARTDEV_BUF_NO register to get the number of the currently used console
    return esp_tool->read_reg(UARTDEV_BUF_NO) & 0xFF;
}

uint32_t Esp32S2::ESP_RAM_BLOCK(void* esp_tool) {
    uint32_t uart_no = get_uart_no(esp_tool);
    bool uses_otg = (uart_no == UARTDEV_BUF_NO_USB_OTG);
    if (uses_otg )qInfo() << "[OK] USB OTG is used";
    return uses_otg ? USB_RAM_BLOCK : 1800;
}

