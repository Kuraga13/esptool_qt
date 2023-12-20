/*  Copyright (C) 2024 Kuraga Tech
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 */

#include "esptoolqt.h"

#include <QThread>

#if defined(Q_OS_WIN32)
#  include <qt_windows.h>
#endif

void EspToolQt::resetToBoot(ResetStrategy strategy)
{
    switch (strategy)
    {
    case (ResetStrategy::classic_reset) :
        // Classic reset sequence, sets DTR and RTS lines sequentially.
        // https://github.com/espressif/esptool/blob/master/esptool/reset.py
        EscapeCommFunction(serial->handle(),CLRDTR);
        EscapeCommFunction(serial->handle(),SETRTS);
        QThread::msleep(500);
        EscapeCommFunction(serial->handle(),SETDTR);
        EscapeCommFunction(serial->handle(),CLRRTS);
        QThread::msleep(500);
        EscapeCommFunction(serial->handle(),CLRDTR);
        EscapeCommFunction(serial->handle(),CLRRTS);
        break;

    case (ResetStrategy::usb_jtag_serial_reset) :
        // Custom reset sequence, which is required when the device
        // is connecting via its USB-JTAG-Serial peripheral.
        // https://github.com/espressif/esptool/blob/master/esptool/reset.py
        EscapeCommFunction(serial->handle(),CLRDTR);
        EscapeCommFunction(serial->handle(),CLRRTS);
        QThread::msleep(100);
        EscapeCommFunction(serial->handle(),SETDTR);
        EscapeCommFunction(serial->handle(),CLRRTS);
        QThread::msleep(100);
        EscapeCommFunction(serial->handle(),SETRTS);
        EscapeCommFunction(serial->handle(),CLRDTR);
        EscapeCommFunction(serial->handle(),SETRTS);
        QThread::msleep(100);
        EscapeCommFunction(serial->handle(),CLRDTR);
        EscapeCommFunction(serial->handle(),CLRRTS);
        break;
    }
}

void EspToolQt::resetFromBoot()
{
    // Reset sequence for hard resetting the chip.
    // Can be used to reset out of the bootloader or to restart a running app.
    // https://github.com/espressif/esptool/blob/master/esptool/reset.py
    EscapeCommFunction(serial->handle(),SETRTS);
    QThread::msleep(200);
    EscapeCommFunction(serial->handle(),CLRRTS);
    QThread::msleep(200);
}
