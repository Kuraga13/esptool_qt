/*  Copyright (C) 2024 Kuraga Tech
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 */

#include "esp8266.h"
#include "../esptoolqt.h"

#include <QDebug>

using std::vector;

//def get_efuses(self):
//# Return the 128 bits of ESP8266 efuse as a single Python integer
//    result = self.read_reg(0x3FF0005C) << 96
//    result |= self.read_reg(0x3FF00058) << 64
//    result |= self.read_reg(0x3FF00054) << 32
//    result |= self.read_reg(0x3FF00050)
//    return result

uint64_t efuses_0_63_f(void* _esp_tool) {
    EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
    uint64_t result = 0;
    result |= (uint64_t)esp_tool->read_reg(0x3FF00054) << 32;
    result |= (uint64_t)esp_tool->read_reg(0x3FF00050);
    return result;
}

uint64_t efuses_64_127_f(void* _esp_tool) {
    EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
    uint64_t result = 0;
    result |= (uint64_t)esp_tool->read_reg(0x3FF0005C) << (96-64);
    result |= (uint64_t)esp_tool->read_reg(0x3FF00058) << (64-64);
    return result;
}

uint32_t _get_flash_size(uint64_t efuses_0_63, uint64_t efuses_64_127) {
    // original 128 bit version
    // r0_4 = (efuses & (1 << 4)) != 0
    // r3_25 = (efuses & (1 << 121)) != 0
    // r3_26 = (efuses & (1 << 122)) != 0
    // r3_27 = (efuses & (1 << 123)) != 0

    bool r0_4 =  ((uint64_t)efuses_0_63   & ((uint64_t)1 << 4)) != 0;
    bool r3_25 = ((uint64_t)efuses_64_127 & ((uint64_t)1 << (121-64))) != 0;
    bool r3_26 = ((uint64_t)efuses_64_127 & ((uint64_t)1 << (122-64))) != 0;
    bool r3_27 = ((uint64_t)efuses_64_127 & ((uint64_t)1 << (123-64))) != 0;

    if (r0_4 && !r3_25) {
        if (!r3_27 && !r3_26) return 1;
        else if (!r3_27 && r3_26) return 2;
    }
    if (!r0_4 && r3_25) {
        if (!r3_27 && !r3_26) return 2;
        else if (!r3_27 && r3_26) return 4;
    }
    return 0;
}

bool Esp8266::get_chip_description(QString* description, void* esp_tool) {
    uint64_t efuses_0_63 = efuses_0_63_f(esp_tool);
    uint64_t efuses_64_127 = efuses_64_127_f(esp_tool);

    // One or the other efuse bit is set for ESP8285
    bool is_8285 = (efuses_0_63 & (1 << 4)) | (efuses_64_127 & (1 << (80-64)));

    if (!is_8285) {
        *description = QString("ESP8266EX");
        return true;
    }

    // deal with esp8255
    uint32_t flash_size = _get_flash_size(efuses_0_63, efuses_64_127);
    bool max_temp = efuses_0_63 & (1 << 5); // This efuse bit identifies the max flash temperature
    switch (flash_size) {
        case 1: {
            *description = QString(max_temp ? "ESP8285H08" : "ESP8285N08");
            break;
        }
        case 2: {
            *description = QString(max_temp ? "ESP8285H16" : "ESP8285N16");
            break;
        }
        default: {
            *description = QString("ESP8285");
            break;
        }
    }

    return true;
}

bool Esp8266::get_chip_features(QString* features, void* esp_tool) {
    *features += "WiFi";
    QString description;
    get_chip_description(&description, esp_tool);
    if (description.contains("ESP8285", Qt::CaseInsensitive)) {
        *features += ", Embedded Flash";
    }
    return true;
}

uint32_t Esp8266::get_crystal_freq(void* _esp_tool) {
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

QVector<QString> Esp8266::CHIP_TARGETS() { return {

    "ESP8266",
    "ESP8266EX",
    "ESP8285N08",
    "ESP8285H16",
    "ESP-WROOM-02D-N2",
    "ESP-WROOM-02D-N4",
    "ESP-WROOM-02D-H2",
    "ESP-WROOM-02U-N2",
    "ESP-WROOM-02U-N4",
    "ESP-WROOM-02U-H2",
    "ESP-WROOM-02-N2",
    "ESP-WROOM-S2-N2"

};}
