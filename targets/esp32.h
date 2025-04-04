/*  Copyright (C) 2024 Kuraga Tech
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
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
    virtual uint32_t stub_entry() {return 1074521560;}
    virtual std::vector<uint8_t> stub_text() {return ba_to_vec(QByteArray::fromBase64("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"));}
    virtual uint32_t stub_text_start() {return 1074520064;}
    virtual std::vector<uint8_t> stub_data() {return ba_to_vec(QByteArray::fromBase64("DMD8P9jnC0Br6AtAA+0LQPLoC0CL6AtA8ugLQFHpC0Ae6gtAkOoLQDnqC0CB5wtAtukLQBDqC0B06QtAtOoLQJ7pC0C06gtAWegLQLboC0Dy6AtAUekLQHHoC0Bk6wtAxewLQKTmC0Dn7AtApOYLQKTmC0Ck5gtApOYLQKTmC0Ck5gtApOYLQKTmC0AL6wtApOYLQOXrC0DF7AtA"));}
    virtual uint32_t stub_data_start() {return 1073605544;}

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
