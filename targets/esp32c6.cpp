/*  Copyright (C) 2024 Kuraga Tech
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 */

#include "esp32c6.h"
#include "../esptoolqt.h"

#include <QDebug>

using std::vector;

namespace {
    const uint32_t EFUSE_BASE = 0x600B0800;
    const uint32_t EFUSE_BLOCK1_ADDR = EFUSE_BASE + 0x044;

    uint32_t get_pkg_version(void* _esp_tool) {
        EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
        uint32_t num_word = 3;
        return (esp_tool->read_reg(EFUSE_BLOCK1_ADDR + (4 * num_word)) >> 24) & 0x07;
    }

    uint32_t get_minor_chip_version(void* _esp_tool) {
        EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
        uint32_t num_word = 3;
        return (esp_tool->read_reg(EFUSE_BLOCK1_ADDR + (4 * num_word)) >> 18) & 0x0F;
    }

    uint32_t get_major_chip_version(void* _esp_tool) {
        EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
        uint32_t num_word = 3;
        return (esp_tool->read_reg(EFUSE_BLOCK1_ADDR + (4 * num_word)) >> 22) & 0x03;
    }
}

bool Esp32C6::get_chip_description(QString* description, void* esp_tool) {
    uint32_t pkg_version = get_pkg_version(esp_tool);
    QString chip_name;

    switch (pkg_version) {
        case 0:
            chip_name = "ESP32-C6 (QFN40)";
            break;
        case 1:
            chip_name = "ESP32-C6FH4 (QFN32)";
            break;
        default:
            chip_name = "unknown ESP32-C6";
            break;
    }

    uint32_t major_rev = get_major_chip_version(esp_tool);
    uint32_t minor_rev = get_minor_chip_version(esp_tool);
    *description = QString("%1 (revision v%2.%3)").arg(chip_name, QString::number(major_rev), QString::number(minor_rev));
    return true;
}

bool Esp32C6::get_chip_features(QString* features, void* esp_tool) {
    *features = "WiFi 6, BT 5, IEEE802.15.4";
    return true;
}

uint32_t Esp32C6::get_crystal_freq(void* esp_tool) {
    // ESP32C6 XTAL is fixed to 40MHz
    return 40;
}
