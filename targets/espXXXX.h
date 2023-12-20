/*  Copyright (C) 2024 Kuraga Tech
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 */

#ifndef ESP_XXXX_H
#define ESP_XXXX_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include "esp_base.h"

class EspXXXX : public EspBase
{
    Q_OBJECT
private:
    // This ROM address has a different value on each chip model
    virtual uint32_t CHIP_DETECT_MAGIC_VALUE() = 0; // {return 0xXXXXXXXX;}

public:
    explicit EspXXXX(QObject *parent) : EspBase {parent} {}
    virtual QString CHIP_NAME() = 0; // {return "ESPXXXX";}

    // STUB
    virtual uint32_t stub_entry() = 0; // {return 0xXXXXXXXX;}
    virtual std::vector<uint8_t> stub_text() // {return ba_to_vec(QByteArray::fromBase64("XXXXXXXXXXXXXXXXX"));}
    virtual uint32_t stub_text_start() = 0; // {return 0xXXXXXXXX;}
    virtual std::vector<uint8_t> stub_data() // {return ba_to_vec(QByteArray::fromBase64("XXXXXXXXXXXXXXXXX"));}
    virtual uint32_t stub_data_start() = 0; // {return 0xXXXXXXXX;}

    // SPI
    virtual uint32_t SPI_REG_BASE() = 0; // {return 0xXXXXXXXX;}
    virtual uint32_t SPI_USR_REG()  = 0; // {return SPI_REG_BASE() + 0xXX;}
    virtual uint32_t SPI_USR1_REG() = 0; // {return SPI_REG_BASE() + 0xXX;}
    virtual uint32_t SPI_USR2_REG() = 0; // {return SPI_REG_BASE() + 0xXX;}
    virtual uint32_t SPI_W0_REG()   = 0; // {return SPI_REG_BASE() + 0xXX;}
    virtual bool     SPI_MOSI_DLEN_OFFS() = 0; // {return true;}
    virtual uint32_t SPI_MOSI_DLEN_REG()  = 0; // {return SPI_REG_BASE() + 0xXX;}
    virtual uint32_t SPI_MISO_DLEN_REG()  = 0; // {return SPI_REG_BASE() + 0xXX;}

    // efuse
    virtual uint32_t EFUSE_RD_REG_BASE() = 0; // {return 0xXXXXXXXX;}

    // functions
    virtual bool get_chip_description(QString* description, void* esp_tool);
    virtual bool get_chip_features(QString* features, void* esp_tool);
    virtual uint32_t get_crystal_freq(void* esp_tool);

};

#endif // ESP_XXXX_H
