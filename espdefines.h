#ifndef ESPDEFINES_H
#define ESPDEFINES_H

// Commands supported by ESP8266 ROM bootloader
#define  ESP_FLASH_BEGIN      0x02
#define  ESP_FLASH_DATA       0x03
#define  ESP_FLASH_END        0x04
#define  ESP_MEM_BEGIN        0x05
#define  ESP_MEM_END          0x06
#define  ESP_MEM_DATA         0x07
#define  ESP_SYNC             0x08
#define  ESP_WRITE_REG        0x09
#define  ESP_FLASH_DEFL_BEGIN 0x10
#define  ESP_FLASH_DEFL_DATA  0x11
#define  ESP_FLASH_DEFL_END   0x12
#define  ESP_READ_REG         0x0A

#endif // ESPDEFINES_H
