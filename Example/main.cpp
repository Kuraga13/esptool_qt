/**
 ******************************************************************************
 * @file           : main.cpp
 * @brief          : Starts the ESP tool example app.
 * @author         : Kuraga Team
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2026 Kuraga Tech
 * SPDX-License-Identifier: MIT
 *
 ******************************************************************************
 */

#include "mainwindow.h"

#include <QApplication>
#include <QDebug>
#include <QThread>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
