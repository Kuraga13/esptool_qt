/*  Copyright (C) 2024 Kuraga Tech
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
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