# esptool_qt
ESP flasher on QT C++

## Contributing
Feel free to contribute. Our main idea is build sustainable community around this, so we can convenient programming ESP mcu's on desktop applications with Qt. 

###  Testing status
| Family  |  Tested | Read | Write/Verify/Erase |
| --- | :---: | :---: | :---: |
| Esp8266 |✔️|✔️|✔️|
| Esp32S2 |✔️|✔️|✔️|
| Esp32S3 |✔️|✔️|✔️|
| Esp32C2 |✔️|✔️|✔️|
| Esp32C3 |✔️|✔️|✔️|
| Esp32C6 |✔️|✔️|✔️|
| Esp32H2 |✔️|✔️|✔️|
| Esp32P4 |  |  |  |

If you have tested any of the targets that we did not test, please let us know in the discussion or in the issues if there are any problems.

## Usage Example
Look at example folder.

### Init
```c++
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    esp_tool = new EspToolQt(this);
}

```
### Connect
```c++
void MainWindow::connect()
{

    if(!esp_tool->autoConnect()) {
        qCritical() << "ESP Connection error";
        return;
    };

    bool connected = esp_tool->esp_target_info.connected;
    QString chip_family = esp_tool->esp_target_info.chip_family;
    QString chip_description = esp_tool->esp_target_info.chip_description;
    QString chip_features = esp_tool->esp_target_info.chip_features;
    uint32_t flash_size = esp_tool->esp_target_info.flash_size;
    QString com_port =  esp_tool->esp_target_info.com_port;
}
```
### Read
```c++
void MainWindow::read()
{
    if(!esp_tool->autoConnect()) {
        qCritical() << "ESP Connection error";
        return;
    };
    uint32_t read_task_size = esp_tool->esp_target_info.flash_size;
    std::vector<uint8_t> data = esp_tool->readFlash(0, read_task_size);
    qInfo() << "read data size:" << data.size();
}
```
