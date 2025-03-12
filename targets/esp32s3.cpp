/*  Copyright (C) 2024 Kuraga Tech
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 */

#include "esp32s3.h"
#include "../esptoolqt.h"

#include <QDebug>

using std::vector;


uint32_t Esp32S3::get_pkg_version_esp32s3(void* _esp_tool){
    EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
    uint32_t num_word = 3;
    return (esp_tool->read_reg(EFUSE_BLOCK1_ADDR + (4 * num_word)) >> 21) & 0x07;
}

uint32_t Esp32S3::get_blk_version_major_esp32s3(void* _esp_tool) {
    EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
    uint32_t num_word = 4;
    return (esp_tool->read_reg(EFUSE_BLOCK2_ADDR + (4 * num_word)) >> 0) & 0x03;
}

uint32_t Esp32S3::get_blk_version_minor_esp32s3(void* _esp_tool) {
    EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
    uint32_t num_word = 3;
    return (esp_tool->read_reg(EFUSE_BLOCK1_ADDR + (4 * num_word)) >> 24) & 0x07;
}

// Workaround: The major version field was allocated to other purposes
// when block version is v1.1.
// Luckily only chip v0.0 have this kind of block version and efuse usage.
bool Esp32S3::is_eco0_esp32s3(uint32_t minor_raw, void* esp_tool) {
    return (((minor_raw & 0x7) == 0) && (get_blk_version_major_esp32s3(esp_tool) == 1) && (get_blk_version_minor_esp32s3(esp_tool) == 1));
}

uint32_t Esp32S3::get_raw_major_chip_version_esp32s3(void* _esp_tool) {
    EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
    uint32_t num_word = 5;
    return (esp_tool->read_reg(EFUSE_BLOCK1_ADDR + (4 * num_word)) >> 24) & 0x03;
}

uint32_t Esp32S3::get_raw_minor_chip_version_esp32s3(void* _esp_tool) {
    EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
    uint32_t hi_num_word = 5;
    uint32_t hi = (esp_tool->read_reg(EFUSE_BLOCK1_ADDR + (4 * hi_num_word)) >> 23) & 0x01;
    uint32_t low_num_word = 3;
    uint32_t low = (esp_tool->read_reg(EFUSE_BLOCK1_ADDR + (4 * low_num_word)) >> 18) & 0x07;
    return (hi << 3) + low;
}

uint32_t Esp32S3::get_major_chip_version_esp32s3(void* esp_tool) {
    uint32_t minor_raw = get_raw_minor_chip_version_esp32s3(esp_tool);
    if (is_eco0_esp32s3(minor_raw, esp_tool)) return 0;
    return get_raw_major_chip_version_esp32s3(esp_tool);
}

uint32_t Esp32S3::get_minor_chip_version_esp32s3(void* esp_tool) {
    uint32_t minor_raw = get_raw_minor_chip_version_esp32s3(esp_tool);
    if (is_eco0_esp32s3(minor_raw, esp_tool)) return 0;
    return minor_raw;
}

bool Esp32S3::get_chip_description(QString* description, void* esp_tool) {
    QString chip_name;

    uint32_t major_rev = get_major_chip_version_esp32s3(esp_tool);
    uint32_t minor_rev = get_minor_chip_version_esp32s3(esp_tool);
    uint32_t pkg_version = get_pkg_version_esp32s3(esp_tool);

    switch(pkg_version) {
        case 0:
            chip_name = "ESP32-S3 (QFN56)";
            break;
        case 1:
            chip_name = "ESP32-S3-PICO-1 (LGA56)";
            break;
        default:
            chip_name = "unknown ESP32-S3";
            break;
    }

    *description += QString("%1 (revision v%2.%3)").arg(chip_name, QString::number(major_rev) , QString::number(minor_rev));
    return true;
}

uint32_t Esp32S3::get_flash_cap_esp32s3(void* _esp_tool) {
    EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
    uint32_t num_word = 3;
    return (esp_tool->read_reg(EFUSE_BLOCK1_ADDR + (4 * num_word)) >> 27) & 0x07;
}


bool Esp32S3::get_flash_vendor_esp32s3(QString* flash_vendor, void* _esp_tool) {
    EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
    uint32_t num_word = 4;
    uint32_t vendor_id = (esp_tool->read_reg(EFUSE_BLOCK1_ADDR + (4 * num_word)) >> 0) & 0x07;

    switch(vendor_id) {
        case 1:
            *flash_vendor = "XMC";
            return true;
        case 2:
            *flash_vendor = "GD";
            return true;
        case 3:
            *flash_vendor = "FM";
            return true;
        case 4:
            *flash_vendor = "TT";
            return true;
        case 5:
            *flash_vendor = "BY";
            return true;
        default:
            *flash_vendor = "";
            return false;
    }
}

uint32_t Esp32S3::get_psram_cap_esp32s3(void* _esp_tool) {
    EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
    uint32_t num_word = 4;
    return (esp_tool->read_reg(EFUSE_BLOCK1_ADDR + (4 * num_word)) >> 3) & 0x03;
}

bool Esp32S3::get_psram_vendor_esp32s3(QString* psram_vendor, void* _esp_tool) {
    EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
    uint32_t num_word = 4;
    uint32_t vendor_id = (esp_tool->read_reg(EFUSE_BLOCK1_ADDR + (4 * num_word)) >> 7) & 0x03;

    switch(vendor_id) {
        case 1:
            *psram_vendor = "AP_3v3";
            return true;
        case 2:
            *psram_vendor = "AP_1v8";
            return true;
        default:
            *psram_vendor = "";
            return false;
    }
}

bool Esp32S3::get_chip_features(QString* features, void* esp_tool) {
    *features += "WiFi, BLE";

    QString flash;
    switch(get_flash_cap_esp32s3(esp_tool)) {
        case 0:
            flash = "";
            break;
        case 1:
            flash = "Embedded Flash 8MB";
            break;
        case 2:
            flash = "Embedded Flash 4MB";
            break;
        default:
            flash = "Unknown Embedded Flash";
            break;
    }

    if (!flash.isEmpty()) {
        *features += QString(", %1").arg(flash);
        QString flash_vendor;
        get_flash_vendor_esp32s3(&flash_vendor, esp_tool);
        if (!flash_vendor.isEmpty()) {
            *features += QString(" (%1)").arg(flash_vendor);
        }
    }

    QString psram_cap;
    switch(get_psram_cap_esp32s3(esp_tool)) {
        case 0:
            psram_cap = "";
            break;
        case 1:
            psram_cap = "Embedded PSRAM 8MB";
            break;
        case 2:
            psram_cap = "Embedded PSRAM 2MB";
            break;
        default:
            psram_cap = "Unknown Embedded PSRAM";
            break;
    }

    if (!psram_cap.isEmpty()) {
        *features += QString(", %1").arg(psram_cap);

        QString psram_vendor;
        get_psram_vendor_esp32s3(&psram_vendor, esp_tool);
        if (!psram_vendor.isEmpty()) {
            *features += QString(" (%1)").arg(psram_vendor);
        }
    }

    if (!features->isEmpty()) {
        return true;
    } else {
        return false;
    }
}

uint32_t Esp32S3::get_uart_no(void* _esp_tool) {
    EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
    // Read the UARTDEV_BUF_NO register to get the number of the currently used console
    return esp_tool->read_reg(UARTDEV_BUF_NO) & 0xFF;
}

uint32_t Esp32S3::ESP_RAM_BLOCK(void* esp_tool) {
    uint32_t uart_no = get_uart_no(esp_tool);
    bool uses_otg = (uart_no == UARTDEV_BUF_NO_USB_OTG);
    if (uses_otg )qInfo() << "[OK] USB OTG is used";
    return uses_otg ? USB_RAM_BLOCK : 1800;
}

QVector<QString> Esp32S3::CHIP_TARGETS() { return {

    "ESP32-S3",
    "ESP32-S3R2",
    "ESP32-S3R8",
    "ESP32-S3-PICO-1-N8R2",
    "ESP32-S3R8V",
    "ESP32-S3FN8",
    "ESP32-S3-WROOM-1-N4",
    "ESP32-S3-WROOM-1-N8",
    "ESP32-S3-WROOM-1-N16",
    "ESP32-S3-WROOM-1-N4R8",
    "ESP32-S3-WROOM-1-N8R8",
    "ESP32-S3-WROOM-1-N16R8",
    "ESP32-S3-WROOM-1-N16R16VA",
    "ESP32-S3-WROOM-1-N4R2",
    "ESP32-S3-WROOM-1-N8R2",
    "ESP32-S3-WROOM-1-N16R2",
    "ESP32-S3-WROOM-1-H4",
    "ESP32-S3-MINI-1-N8",
    "ESP32-S3-MINI-1-N4R2",
    "ESP32-S3-WROOM-2-N16R8V",
    "ESP32-S3-WROOM-2-N32R8V",
    "ESP32-S3-WROOM-2-N32R16V",
    "ESP32-S3FH4R2",
    "ESP32-S3-WROOM-1U-N4",
    "ESP32-S3-WROOM-1U-N8",
    "ESP32-S3-WROOM-1U-N16",
    "ESP32-S3-WROOM-1U-N4R8",
    "ESP32-S3-WROOM-1U-N8R8",
    "ESP32-S3-WROOM-1U-N16R8",
    "ESP32-S3-WROOM-1U-N4R2",
    "ESP32-S3-WROOM-1U-N8R2",
    "ESP32-S3-WROOM-1U-N16R2",
    "ESP32-S3-MINI-1U-N8",
    "ESP32-S3-MINI-1U-N4R2",
    "ESP32-S3-PICO-1-N8R8"
    
};}
