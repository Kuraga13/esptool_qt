/**
 ******************************************************************************
 * @file           : esp32p4.cpp
 * @brief          : Implements ESP32-P4 chip support.
 * @author         : Kuraga Team
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 Kuraga Tech
 * SPDX-License-Identifier: MIT
 *
 ******************************************************************************
 */

#include "esp32p4.h"
#include "../esptoolqt.h"

#include <QDebug>

using std::vector;

bool Esp32P4::get_chip_description(QString* description, void* esp_tool) {
	*description = "ESP32-P4";
    return true;
}

bool Esp32P4::get_chip_features(QString* features, void* esp_tool) {
    *features = "High-Performance MCU";
    return true;
}

uint32_t Esp32P4::get_crystal_freq(void* esp_tool) {
	// ESP32P4 XTAL is fixed to 40MHz
	return 40;
}
