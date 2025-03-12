/*  Copyright (C) 2024 Kuraga Tech
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 */

#include "esp32c2.h"
#include "../esptoolqt.h"

#include <QDebug>

using std::vector;

namespace {
    const uint32_t EFUSE_BASE = 0x60008800;
    const uint32_t EFUSE_BLOCK1_ADDR = EFUSE_BASE + 0x044;
    const uint32_t EFUSE_BLOCK2_ADDR = EFUSE_BASE + 0x040;

    uint32_t get_pkg_version(void* _esp_tool) {
        EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
        uint32_t num_word = 1;
        return (esp_tool->read_reg(EFUSE_BLOCK2_ADDR + (4 * num_word)) >> 22) & 0x07;
    }

    uint32_t get_minor_chip_version(void* _esp_tool) {
        EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
        uint32_t num_word = 1;
        return (esp_tool->read_reg(EFUSE_BLOCK2_ADDR + (4 * num_word)) >> 16) & 0xF;
    }

    uint32_t get_major_chip_version(void* _esp_tool) {
        EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
        uint32_t num_word = 1;
        return (esp_tool->read_reg(EFUSE_BLOCK2_ADDR + (4 * num_word)) >> 20) & 0x3;
    }
}

bool Esp32C2::get_chip_description(QString* description, void* esp_tool) {
	QString chip_name;
	switch(get_pkg_version(esp_tool)) {
        case 0: 
		    chip_name = "ESP32-C2";
			break;
		case 1: 
		    chip_name = "ESP32-C2";
			break;
		default:
		    chip_name = "unknown ESP32-C2";
			break;
	}
    
    uint32_t major_rev = get_major_chip_version(esp_tool);
    uint32_t minor_rev = get_minor_chip_version(esp_tool);
	
    *description += QString("%1 (revision v%2.%3)").arg(chip_name, QString::number(major_rev), QString::number(minor_rev));
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

        QString flash_vendor;
        switch(vendor_id) {
            case 1:
                flash_vendor = "XMC";
                break;
            case 2:
                flash_vendor = "GD";
                break;
            case 3:
                flash_vendor = "FM";
                break;
            case 4:
                flash_vendor = "TT";
                break;
            case 5:
                flash_vendor = "ZBIT";
                break;
            default:
                flash_vendor = "";
                break;
        }
        return flash_vendor;
    }
}

bool Esp32C2::get_chip_features(QString* features, void* esp_tool) {
    *features = "WiFi, BLE";

    uint32_t flash_cap = get_flash_cap(esp_tool);
    QString flash;
    switch (flash_cap) {
        case 0:
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
        *features += QString(", %1 (%2)").arg(flash, get_flash_vendor(esp_tool));
    }

    return true;
}

namespace {
    // xtal
    const uint32_t UART_CLKDIV_REG = 0x60000014;
    const uint32_t XTAL_CLK_DIVIDER = 1;
    const uint32_t UART_CLKDIV_MASK = 0xFFFFF;
}

uint32_t Esp32C2::get_crystal_freq(void* _esp_tool) {
    EspToolQt* esp_tool = (EspToolQt*)_esp_tool;

    // """
    // Figure out the crystal frequency from the UART clock divider
    // Returns a normalized value in integer MHz (only values 40 or 26 are supported)
    // """
    // # The logic here is:
    // # - We know that our baud rate and the ESP UART baud rate are roughly the same,
    // #   or we couldn't communicate
    // # - We can read the UART clock divider register to know how the ESP derives this
    // #   from the APB bus frequency
    // # - Multiplying these two together gives us the bus frequency which is either
    // #   the crystal frequency (ESP32) or double the crystal frequency (ESP8266).
    // #   See the self.XTAL_CLK_DIVIDER parameter for this factor.

    uint32_t uart_div = esp_tool->read_reg(UART_CLKDIV_REG) & UART_CLKDIV_MASK;
    double est_xtal = ((double)esp_tool->serial->baudRate() * (double)uart_div) / 1e6 / (double)XTAL_CLK_DIVIDER;
    uint32_t norm_xtal = (est_xtal > 33) ? 40 : 26;
    return norm_xtal;
}

QVector<QString> Esp32C2::CHIP_TARGETS() { return {

    "ESP8684",
    "ESP8684H2",
    "ESP8684H2X",
    "ESP8684H4",
    "ESP8684H4X",
    "ESP8684-MINI-1-H2",
    "ESP8684-MINI-1-H2X",
    "ESP8684-MINI-1-H4",
    "ESP8684-MINI-1-H4X",
    "ESP8684-WROOM-03-H2",
    "ESP8684-WROOM-03-H2X",
    "ESP8684-WROOM-03-H4",
    "ESP8684-WROOM-03-H4X",
    "ESP8684-WROOM-05-H2",
    "ESP8684-WROOM-05-H2X",
    "ESP8684-WROOM-05-H4",
    "ESP8684-WROOM-05-H4X",
    "ESP8684-WROOM-06C-H2",
    "ESP8684-WROOM-06C-H2X",
    "ESP8684-WROOM-06C-H4",
    "ESP8684-WROOM-06C-H4X",
    "ESP8684-WROOM-07-H2",
    "ESP8684-WROOM-07-H2X",
    "ESP8684-WROOM-07-H4",
    "ESP8684-WROOM-07-H4X",
    "ESP8684-WROOM-01C-H2",
    "ESP8684-WROOM-01C-H2X",
    "ESP8684-WROOM-01C-H4",
    "ESP8684-WROOM-01C-H4X",
    "ESP8684-WROOM-02C-H2",
    "ESP8684-WROOM-02C-H2X",
    "ESP8684-WROOM-02C-H4",
    "ESP8684-WROOM-02C-H4X",
    "ESP8684-WROOM-04C-H2",
    "ESP8684-WROOM-04C-H2X",
    "ESP8684-WROOM-04C-H4",
    "ESP8684-WROOM-04C-H4X",
    "ESP8684-WROOM-02UC-H2",
    "ESP8684-WROOM-02UC-H2X",
    "ESP8684-WROOM-02UC-H4",
    "ESP8684-WROOM-02UC-H4X",
    "ESP8684-MINI-1U-H2",
    "ESP8684-MINI-1U-H2X",
    "ESP8684-MINI-1U-H4",
    "ESP8684-MINI-1U-H4X"
};}