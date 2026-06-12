/**
 ******************************************************************************
 * @file           : esp32.h
 * @brief          : Declares ESP32 target support.
 * @author         : Kuraga Team
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 Kuraga Tech
 * SPDX-License-Identifier: MIT
 *
 ******************************************************************************
 * @details
 *
 * Defines target metadata, flasher stubs, register addresses, and chip
 * information callbacks for the ESP32 chip family.
 *
 * Features:
 * - ESP32 ROM detection and supported module names
 * - Embedded RAM stub text and data payloads
 * - SPI flash, efuse, revision, and crystal hooks
 *
 * Usage Example:
 * ```cpp
 * Esp32 target(nullptr);
 * QString name = target.CHIP_NAME();
 * ```
 *
 ******************************************************************************
 */

#ifndef ESP32_H
#define ESP32_H

#include <QObject>
#include <QString>
#include "esp_base.h"

class Esp32 : public EspBase
{
    Q_OBJECT
private:
    uint32_t get_pkg_version_esp32(void* esp_tool);
    uint32_t get_major_chip_version_esp32(void* esp_tool);
    uint32_t get_minor_chip_version_esp32(void* esp_tool);

    uint32_t CHIP_DETECT_MAGIC_VALUE() {return 0x00F01D83;}

public:
    explicit Esp32(QObject *parent) : EspBase {parent} {}
    QString CHIP_NAME() {return "ESP32";}
    QVector<QString> CHIP_TARGETS();

    // STUB
    virtual uint32_t stub_entry() {return 1074521688;}
    virtual std::vector<uint8_t> stub_text() {return ba_to_vec(QByteArray::fromBase64("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"));}
    virtual uint32_t stub_text_start() {return 1074520064;}
    virtual std::vector<uint8_t> stub_data() {return ba_to_vec(QByteArray::fromBase64("DMD8P1XoC0Dr6AtAd+0LQIvpC0AO6QtAi+kLQOTpC0Dr6gtAYesLQAbrC0AB6AtAl+oLQODqC0AC6gtAgusLQCzqC0CC6wtA4ugLQETpC0CL6QtA5OkLQPToC0BP7AtAO+0LQCLnC0Bb7QtAIucLQCLnC0Ai5wtAIucLQCLnC0Ai5wtAIucLQCLnC0Dj6wtAIucLQGrsC0A77QtA"));}
    virtual uint32_t stub_data_start() {return 1073605548;}

    // SPI
    virtual uint32_t SPI_REG_BASE() {return 0x3FF42000;}
    virtual uint32_t SPI_USR_REG()  {return SPI_REG_BASE() + 0x1C;}
    virtual uint32_t SPI_USR1_REG() {return SPI_REG_BASE() + 0x20;}
    virtual uint32_t SPI_USR2_REG() {return SPI_REG_BASE() + 0x24;}
    virtual uint32_t SPI_W0_REG()   {return SPI_REG_BASE() + 0x80;}
    virtual bool     SPI_MOSI_DLEN_OFFS() {return true;}
    virtual uint32_t SPI_MOSI_DLEN_REG() {return SPI_REG_BASE() + 0x28;}
    virtual uint32_t SPI_MISO_DLEN_REG() {return SPI_REG_BASE() + 0x2C;}

    // efuse
    virtual uint32_t EFUSE_RD_REG_BASE() {return 0x3FF5A000;}
    const   uint32_t DR_REG_SYSCON_BASE = 0x3FF66000;
    const   uint32_t APB_CTL_DATE_ADDR = DR_REG_SYSCON_BASE + 0x7C;
    const   uint32_t APB_CTL_DATE_V = 0x1;
    const   uint32_t APB_CTL_DATE_S = 31;

    // xtal
    const uint32_t UART_CLKDIV_REG = 0x3FF40014;
    const uint32_t XTAL_CLK_DIVIDER = 1;
    const uint32_t UART_CLKDIV_MASK = 0xFFFFF;

    // functions
    virtual bool get_chip_description(QString* description, void* esp_tool);
    virtual bool get_chip_features(QString* features, void* esp_tool);
    virtual uint32_t get_crystal_freq(void* esp_tool);

};

#endif // ESP32_H
