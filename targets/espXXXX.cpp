/**
 ******************************************************************************
 * @file           : espXXXX.cpp
 * @brief          : Implements template target defaults.
 * @author         : Kuraga Team
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 Kuraga Tech
 * SPDX-License-Identifier: MIT
 *
 ******************************************************************************
 */

#include "espXXXX.h"
#include "../esptoolqt.h"

#include <QDebug>

using std::vector;

bool EspXXXX::get_chip_description(QString* description, void* esp_tool) {
    return true;
}

bool EspXXXX::get_chip_features(QString* features, void* esp_tool) {

    return true;
}

uint32_t EspXXXX::get_crystal_freq(void* esp_tool) {
	return 0;
}
