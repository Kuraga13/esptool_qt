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
