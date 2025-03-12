/*  Copyright (C) 2024 Kuraga Tech
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 */

#include "esp32c3.h"
#include "../esptoolqt.h"

#include <QDebug>

using std::vector;

namespace {
    const uint32_t EFUSE_BASE = 0x60008800;
    const uint32_t EFUSE_BLOCK1_ADDR = EFUSE_BASE + 0x044;

    uint32_t get_pkg_version(void* _esp_tool) {
        EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
        uint32_t num_word = 3;
        return (esp_tool->read_reg(EFUSE_BLOCK1_ADDR + (4 * num_word)) >> 21) & 0x07;
    }

    uint32_t get_minor_chip_version(void* _esp_tool) {
        EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
        uint32_t hi_num_word = 5;
        uint32_t hi = (esp_tool->read_reg(EFUSE_BLOCK1_ADDR + (4 * hi_num_word)) >> 23) & 0x01;
        uint32_t low_num_word = 3;
        uint32_t low = (esp_tool->read_reg(EFUSE_BLOCK1_ADDR + (4 * low_num_word)) >> 18) & 0x07;
        return (hi << 3) + low;
    }

    uint32_t get_major_chip_version(void* _esp_tool) {
        EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
        uint32_t num_word = 5;
        return (esp_tool->read_reg(EFUSE_BLOCK1_ADDR + (4 * num_word)) >> 24) & 0x03;
    }
}

bool Esp32C3::get_chip_description(QString* description, void* esp_tool) {
    uint32_t pkg_version = get_pkg_version(esp_tool);
    uint32_t major_rev = get_major_chip_version(esp_tool);
    uint32_t minor_rev = get_minor_chip_version(esp_tool);

    QString chip_name;
    switch (pkg_version) {
        case 0:
            chip_name = "ESP32-C3 (QFN32)";
            break;
        case 1:
            chip_name = "ESP8685 (QFN28)";
            break;
        case 2:
            chip_name = "ESP32-C3 AZ (QFN32)";
            break;
        case 3:
            chip_name = "ESP8686 (QFN24)";
            break;
        default:
            chip_name = "unknown ESP32-C3";
            break;
    }

    *description = QString("%1 (revision v%2.%3)").arg(chip_name, QString::number(major_rev), QString::number(minor_rev));
    return true;
}

namespace {
    uint32_t get_flash_cap(void* _esp_tool) {
        EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
        uint32_t num_word = 3;
        return (esp_tool->read_reg(EFUSE_BLOCK1_ADDR + (4 * num_word)) >> 27) & 0x07;
    }

    QString get_flash_vendor(void* _esp_tool) {
        EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
        uint32_t num_word = 4;
        uint32_t vendor_id = (esp_tool->read_reg(EFUSE_BLOCK1_ADDR + (4 * num_word)) >> 0) & 0x07;

        QString vendor;
        switch (vendor_id) {
            case 1:
                vendor = "XMC";
                break;
            case 2:
                vendor = "GD";
                break;
            case 3:
                vendor = "FM";
                break;
            case 4:
                vendor = "TT";
                break;
            case 5:
                vendor = "ZBIT";
                break;
            default:
                vendor = "";
                break;
        }
        return vendor;
    }
}

bool Esp32C3::get_chip_features(QString* features, void* esp_tool) {
    *features = "WiFi, BLE";

    uint32_t flash_cap = get_flash_cap(esp_tool);
    QString flash;
    switch (flash_cap) {
        case 0:
            flash = "";
            break;
        case 1:
            flash = "Embedded Flash 4MB";
            break;
        case 2:
            flash = "Embedded Flash 2MB";
            break;
        case 3:
            flash = "Embedded Flash 1MB";
            break;
        case 4:
            flash = "Embedded Flash 8MB";
            break;
        default:
            flash = "Unknown Embedded Flash";
            break;
        }

    if (!flash.isEmpty()) {
        QString flash_vendor = get_flash_vendor(esp_tool);
        *features += QString(", %1 (%2)").arg(flash, flash_vendor);
    }
    return true;
}

uint32_t Esp32C3::get_crystal_freq(void* esp_tool) {
    // ESP32C3 XTAL is fixed to 40MHz
    return 40;
}

QVector<QString> Esp32C3::CHIP_TARGETS() { return {

    "ESP32-C3",
    "ESP32-C3FN4",
    "ESP32-C3FH4",
    "ESP32-C3FH4X",
    "ESP32-C3-MINI-1-N4",
    "ESP32-C3-MINI-1-N4X",
    "ESP32-C3-MINI-1-H4",
    "ESP32-C3-MINI-1U-N4",
    "ESP32-C3-MINI-1U-N4X",
    "ESP32-C3-MINI-1U-H4",
    "ESP32-C3-MINI-1U-H4X",
    "ESP32-C3-WROOM-02-N4",
    "ESP32-C3-WROOM-02-H4",
    "ESP32-C3-WROOM-02U-N4",
    "ESP32-C3-WROOM-02U-H4",
    "ESP8685",
    "ESP8685-WROOM-03-H2",
    "ESP8685-WROOM-05-H2",
    "ESP8685-WROOM-06-H2",
    "ESP8685H4",
    "ESP8685-WROOM-03-H4",
    "ESP8685-WROOM-05-H4",
    "ESP8685-WROOM-06-H4",
    "ESP8685-WROOM-07-H2",
    "ESP8685-WROOM-07-H4",
    "ESP8685-WROOM-01-H2",
    "ESP8685-WROOM-01-H4",
    "ESP8685-WROOM-04-H2",
    "ESP8685-WROOM-04-H4",
    "ESP32-C3-WROOM-02-N8",
    "ESP32-C3-WROOM-02U-N8",
    "ESP32-C3-MINI-1-H4X"

};}