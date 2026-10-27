#ifndef _XL9555_H_
#define _XL9555_H_

#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"



#define I2C_MASTER_SCL_IO           GPIO_NUM_42       /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           GPIO_NUM_41       /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM              I2C_NUM_1                   /*!< I2C port number for master dev */
#define I2C_MASTER_FREQ_HZ          100000 /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                           /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       1000

#define XL9555_ADDR         0x20        
#define XL9555_CONFIG_0_REG_ADDR   0x06
#define XL9555_CONFIG_1_REG_ADDR   0x07       

#define XL9555_INPUT_0_REG_ADDR    0x00
#define XL9555_INPUT_1_REG_ADDR    0x01
#define XL9555_OUTPUT_0_REG_ADDR   0x02
#define XL9555_OUTPUT_1_REG_ADDR   0x03
#define XL9555_TOGGER_0_REG_ADDR   0x04
#define XL9555_TOGGER_1_REG_ADDR   0x05

#define XL9555_SET 1
#define XL9555_RESET 0
#define XL9555_INPUT_MODE 1
#define XL9555_OUTPUT_MODE 0

#define CAM_SENSOR_RESET_PORT 0
#define CAM_SENSOR_RESET_PIN 5

#define CAM_SENSOR_PWDN_PORT 0
#define CAM_SENSOR_PWDN_PIN 4

void XL9555_init(void);

void XL9555_gpio_Mode(uint8_t GPIO_PORT, uint8_t GPIO_PIN, uint8_t mode);

/**
 * @brief 设置指定GPIO_PORT的GPIO_PIN的bit�?,注意调用此函数会�?动把gpio_mode设置为输出模�?
 * 
 * @param GPIO_PORT 
 * @param GPIO_PIN 
 * @param bit 
 */
void XL9555_gpio_WriteBit(uint8_t GPIO_PORT, uint8_t GPIO_PIN, uint8_t bit);

void XL9555_gpio_Setbit(uint8_t GPIO_PORT, uint8_t GPIO_PIN);

void XL9555_gpio_Resetbit(uint8_t GPIO_PORT, uint8_t GPIO_PIN);

uint8_t XL9555_gpio_Readbit(uint8_t GPIO_PORT, uint8_t GPIO_PIN);

#endif
