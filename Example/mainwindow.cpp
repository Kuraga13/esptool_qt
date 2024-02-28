#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QDebug>

#include "../esptoolqt.h"
#include <QFile>
#include <map>

using std::vector;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    esp_tool = new EspToolQt();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_reset_to_boot_clicked()
{
    qInfo() << esp_tool->resetStrategy;
    esp_tool->resetToBoot(esp_tool->resetStrategy);
}


void MainWindow::on_reset_from_boot_clicked()
{
    esp_tool->resetFromBoot();
}

void MainWindow::on_get_ports_clicked()
{
    vector<QString> ports = esp_tool->getPorts();
    qInfo() << ports;
}

void MainWindow::on_auto_connect_clicked()
{
    esp_tool->autoConnect();
}

void MainWindow::on_disconnect_clicked()
{
    esp_tool->disconnect();
}

void MainWindow::on_read_register_clicked()
{

}


void MainWindow::on_Test_clicked()
{
    vector<uint8_t> data = esp_tool->readFlash(0xF000, 0x10);

    qInfo() << Qt::hex << data;
    qInfo() << "Received data size" << Qt::hex << data.size();


//    QString file = "C:/Users/Evgeny/Desktop/esptool/out.bin";
//    qInfo() << "Data saved to: " << file;

//    QFile outputFile(file);
//    outputFile.open(QIODevice::WriteOnly);

//    QString str = "text inside file";

//    if(!outputFile.isOpen())
//    {
//        //alert that file did not open
//    }

//    QDataStream out(&outputFile);

//    out.writeRawData(reinterpret_cast<const char*>(data.data()), data.size());



//    outputFile.close();




}


void MainWindow::on_pushButton_clicked()
{
    vector<uint8_t> data = {0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x57, 0x6F, 0x72, 0x6C, 0x64, 0x00};

//    QString file_path = "C:/Users/Evgeny/Desktop/esptool/blink_500.bin";
//    QFile file(file_path);
//    file.open(QIODevice::ReadOnly);
//    while(!file.atEnd())
//    {
//        // return value from 'file.read' should always be sizeof(char).
//        char file_data;
//        file.read(&file_data,sizeof(char));
//        uint8_t x = reinterpret_cast<uint8_t&>(file_data);
//        data.push_back(x);

//        // do something with 'file_data'.
//    }
//    file.close();

    esp_tool->flashUpload(0xF000, data);
}


void MainWindow::on_Read_All_Button_clicked()
{
    //esp_tool->serial_progress_enabled = true;
    vector<uint8_t> data = esp_tool->readFlash(0, esp_tool->esp_target_info.flash_size);
    qInfo() << data.size();
}

