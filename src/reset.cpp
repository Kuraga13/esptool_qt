/**
 ******************************************************************************
 * @file           : src/reset.cpp
 * @brief          : Controls ESP boot reset sequences.
 * @author         : Kuraga Team
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 Kuraga Tech
 * SPDX-License-Identifier: MIT
 *
 ******************************************************************************
 */

#include "../esptoolqt.h"

#include <QThread>

#if defined(Q_OS_WIN32)
#  include <qt_windows.h>
#endif

namespace {

class EspLineControl {
public:
    EspLineControl(QSerialPort *serial, bool swapDtrRts)
        : serial_(serial), swapDtrRts_(swapDtrRts)
    {}

    void setDtr(bool state)
    {
        if (swapDtrRts_) {
            setPhysicalRts(state);
        } else {
            setPhysicalDtr(state);
        }
    }

    void setRts(bool state)
    {
        if (swapDtrRts_) {
            setPhysicalDtr(state);
        } else {
            setPhysicalRts(state);
        }
    }

private:
    void setPhysicalDtr(bool state)
    {
        dtrState_ = state;
        EscapeCommFunction(serial_->handle(), state ? SETDTR : CLRDTR);
    }

    void setPhysicalRts(bool state)
    {
        EscapeCommFunction(serial_->handle(), state ? SETRTS : CLRRTS);
        // Windows usbser.sys workaround: repeat current DTR after RTS so the
        // control-line-state request is propagated with both line states.
        setPhysicalDtr(dtrState_);
    }

    QSerialPort *serial_ = nullptr;
    bool swapDtrRts_ = false;
    bool dtrState_ = false;
};

} // namespace

void EspToolQt::resetToBoot(ResetStrategy strategy)
{
    EspLineControl lines(serial, swapDtrRts);

    switch (strategy)
    {
    case (ResetStrategy::classic_reset) :
        // Classic reset sequence, sets DTR and RTS lines sequentially.
        // https://github.com/espressif/esptool/blob/master/esptool/reset.py
        lines.setDtr(false); // IO0 high
        lines.setRts(true);  // EN low, chip in reset
        QThread::msleep(500);
        lines.setDtr(true);  // IO0 low
        lines.setRts(false); // EN high, chip out of reset
        QThread::msleep(500);
        lines.setDtr(false); // IO0 high, done
        lines.setRts(false);
        break;

    case (ResetStrategy::usb_jtag_serial_reset) :
        // Custom reset sequence, which is required when the device
        // is connecting via its USB-JTAG-Serial peripheral.
        // https://github.com/espressif/esptool/blob/master/esptool/reset.py
        lines.setDtr(false);
        lines.setRts(false);
        QThread::msleep(100);
        lines.setDtr(true);
        lines.setRts(false);
        QThread::msleep(100);
        lines.setRts(true);
        lines.setDtr(false);
        lines.setRts(true);
        QThread::msleep(100);
        lines.setDtr(false);
        lines.setRts(false);
        QThread::msleep(200);
        break;
    }
}

void EspToolQt::resetFromBoot()
{
    // Reset sequence for hard resetting the chip.
    // Can be used to reset out of the bootloader or to restart a running app.
    // https://github.com/espressif/esptool/blob/master/esptool/reset.py
    EspLineControl lines(serial, swapDtrRts);
    lines.setDtr(false);
    lines.setRts(true);
    QThread::msleep(200);
    lines.setRts(false);
    QThread::msleep(200);
}
