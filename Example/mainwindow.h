/**
 ******************************************************************************
 * @file           : mainwindow.h
 * @brief          : Declares the example main window.
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
 * Declares a Qt example window that exposes EspToolQt actions for reset,
 * connection, flash reads, and flash writes from UI button handlers.
 *
 * Features:
 * - Owns an EspToolQt instance for example operations
 * - Provides slots for reset and connection buttons
 * - Provides slots for flash read and write tests
 *
 * Usage Example:
 * ```cpp
 * MainWindow window;
 * window.show();
 * ```
 *
 ******************************************************************************
 */

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "../esptoolqt.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    EspToolQt *esp_tool;

private slots:
    void on_reset_to_boot_clicked();

    void on_reset_from_boot_clicked();

    void on_get_ports_clicked();

    void on_auto_connect_clicked();

    void on_disconnect_clicked();

    void on_read_register_clicked();

    void on_Test_clicked();

    void on_pushButton_clicked();

    void on_Read_All_Button_clicked();

private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
