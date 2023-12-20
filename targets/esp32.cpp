/*  Copyright (C) 2024 Kuraga Tech
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 */

#include "esp32.h"
#include "../esptoolqt.h"

#include <QDebug>

using std::vector;


uint32_t Esp32::get_pkg_version_esp32(void* _esp_tool) {
    EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
    uint32_t efuse_3 = esp_tool->readEfuse(3);
    uint32_t pkg_version = (efuse_3 >> 9) & 0x07;
    pkg_version += ((efuse_3 >> 2) & 0x1) << 3;
    return pkg_version;
}

uint32_t Esp32::get_major_chip_version_esp32(void* _esp_tool) {
    EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
    uint32_t rev_bit0 = (esp_tool->readEfuse(3) >> 15) & 0x1;
    uint32_t rev_bit1 = (esp_tool->readEfuse(5) >> 20) & 0x1;
    uint32_t apb_ctl_date = esp_tool->read_reg(APB_CTL_DATE_ADDR);
    uint32_t rev_bit2 = (apb_ctl_date >> APB_CTL_DATE_S) & APB_CTL_DATE_V;
    uint32_t combine_value = (rev_bit2 << 2) | (rev_bit1 << 1) | rev_bit0;

    switch(combine_value) {
        case 0:  return 0;
        case 1:  return 1;
        case 3:  return 2;
        case 7:  return 3;
        default: return 0;
    }
}

uint32_t Esp32::get_minor_chip_version_esp32(void* _esp_tool) {
    EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
    return (esp_tool->readEfuse(5) >> 24) & 0x3;
}

bool Esp32::get_chip_description(QString* description, void* _esp_tool) {
    EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
    QString chip_name;
    uint32_t pkg_version = get_pkg_version_esp32(esp_tool);
    uint32_t major_rev = get_major_chip_version_esp32(esp_tool);
    uint32_t minor_rev = get_minor_chip_version_esp32(esp_tool);
    bool rev3 = (major_rev == 3);
    bool single_core = esp_tool->readEfuse(3) & (1 << 0); // CHIP_VER DIS_APP_CPU

    switch (pkg_version){
        case 0:
            chip_name += single_core ? "ESP32-S0WDQ6" : "ESP32-D0WDQ6";
            break;
        case 1:
            chip_name += single_core ? "ESP32-S0WD" : "ESP32-D0WD";
            break;
        case 2:
            chip_name += "ESP32-D2WD";
            break;
        case 4:
            chip_name += "ESP32-U4WDH";
            break;
        case 5:
            chip_name += rev3 ? "ESP32-PICO-V3" : "ESP32-PICO-D4";
            break;
        case 6:
            chip_name += "ESP32-PICO-V3-02";
            break;
        case 7:
            chip_name += "ESP32-D0WDR2-V3";
            break;
        default:
            chip_name += "unknown ESP32";
            break;
    }

    // deal with ESP32-D0WD-V3, ESP32-D0WDQ6-V3
    if (chip_name.contains("ESP32-D0WD", Qt::CaseInsensitive)) {
        chip_name += "-V3";
    }

    *description += QString("%1 (revision v%2.%3)").arg(chip_name, QString::number(major_rev) , QString::number(minor_rev));
    return true;
}

bool Esp32::get_chip_features(QString* _features, void* _esp_tool) {
    EspToolQt* esp_tool = (EspToolQt*)_esp_tool;
    QString& features = *_features;

    features += "WiFi";
    uint32_t word3 = esp_tool->readEfuse(3);

    // names of variables in this section are lowercase
    // versions of EFUSE names as documented in TRM and
    // ESP-IDF efuse_reg.h

    bool chip_ver_dis_bt = word3 & (1 << 1);
    if (chip_ver_dis_bt == 0) features += ", BT";

    bool chip_ver_dis_app_cpu = word3 & (1 << 0);
    if (chip_ver_dis_app_cpu) {
        features += ", Single Core";
    } else {
        features += ", Dual Core";
    }

    bool chip_cpu_freq_rated = word3 & (1 << 13);
    if (chip_cpu_freq_rated) {
        bool chip_cpu_freq_low = word3 & (1 << 12);
        if (chip_cpu_freq_low) {
            features += ", 160MHz";
        } else {
            features += ", 240MHz";
        }
    }

    uint32_t pkg_version = get_pkg_version_esp32(esp_tool);
    if ((pkg_version == 2) || (pkg_version == 4) || (pkg_version == 5) || (pkg_version == 6)) {
        features += ", Embedded Flash";
    }

    if (pkg_version == 6) {
        features += ", Embedded PSRAM";
    }

    uint32_t word4 = esp_tool->readEfuse(4);
    bool adc_vref = (word4 >> 8) & 0x1F;
    if (adc_vref) {
        features += ", VRef calibration in efuse";
    }

    bool blk3_part_res = word3 >> 14 & 0x1;
    if (blk3_part_res) {
        features += ", BLK3 partially reserved";
    }

    uint32_t word6 = esp_tool->readEfuse(6);
    uint32_t coding_scheme = word6 & 0x3;
    features += ", Coding Scheme ";
    switch (coding_scheme) {
        case 0:  {features += "None"; break;}
        case 1:  {features += "3/4"; break;}
        case 2:  {features += "Repeat (UNSUPPORTED)"; break;}
        case 3:  {features += "Invalid"; break;}
        default: {features += "Unknown"; break;}
    }

    return true;
}

uint32_t Esp32::get_crystal_freq(void* _esp_tool) {
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
