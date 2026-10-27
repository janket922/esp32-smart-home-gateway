#include "XL9555.h"

i2c_master_bus_handle_t XL9555_bus_handle = NULL;
i2c_master_dev_handle_t XL9555_dev_handle = NULL;
static const char *TAG = "example";
static bool xl9555_init_flag = false;
/*
 * 初始化XL9555
 */
void XL9555_init(void)
{
	if (xl9555_init_flag == false)
	{
	i2c_master_bus_handle_t *bus_handle = &XL9555_bus_handle;
	i2c_master_dev_handle_t *dev_handle = &XL9555_dev_handle;
	i2c_master_bus_config_t bus_config = {
		.i2c_port = I2C_MASTER_NUM,
		.sda_io_num = I2C_MASTER_SDA_IO,
		.scl_io_num = I2C_MASTER_SCL_IO,
		.clk_source = I2C_CLK_SRC_DEFAULT,
		.glitch_ignore_cnt = 7,
		.flags.enable_internal_pullup = true,
	};
	ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, bus_handle));

	i2c_device_config_t dev_config = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = XL9555_ADDR,
		.scl_speed_hz = I2C_MASTER_FREQ_HZ,
	};
	ESP_ERROR_CHECK(i2c_master_bus_add_device(*bus_handle, &dev_config, dev_handle));
	xl9555_init_flag = true;
	}
	else
	{
		return;
	}
}

void XL9555_gpio_Mode(uint8_t GPIO_PORT, uint8_t GPIO_PIN, uint8_t mode)
{
	if (GPIO_PORT > 1 || GPIO_PIN > 7 || mode > 1)
	{
		ESP_LOGE(TAG, "XL9555_GPIO_PORT or XL9555_GPIO_PIN or bit error");
		return;
	}
	uint8_t reg_value = 0;
	uint8_t config_reg_addr;
	uint8_t temp_data[2];
	// 1.设置指定位的控制寄存器为OUTPUT模式
	// 1.1 读取当前控制寄存器状态
	switch (GPIO_PORT)
	{
	case 0:
		config_reg_addr = XL9555_CONFIG_0_REG_ADDR;
		break;
	case 1:
		config_reg_addr = XL9555_CONFIG_1_REG_ADDR;
		break;
	default:
		ESP_LOGE(TAG, "XL9555_GPIO_PORT error");
		return;
	}
	ESP_ERROR_CHECK(i2c_master_transmit_receive(XL9555_dev_handle, &config_reg_addr, 1, &reg_value, 1, I2C_MASTER_TIMEOUT_MS));

	// 1.2 设置指定PIN模式
	if (mode == XL9555_OUTPUT_MODE)
	{
		reg_value &= ~(1 << GPIO_PIN);
	}
	else if (mode == XL9555_INPUT_MODE)
	{
		reg_value |= (1 << GPIO_PIN);
	}
	// 1.3 写入控制寄存器
	temp_data[0] = config_reg_addr;
	temp_data[1] = reg_value;
	ESP_ERROR_CHECK(i2c_master_transmit(XL9555_dev_handle, temp_data, 2, I2C_MASTER_TIMEOUT_MS));
}

/*
 * 设置XL9555的指定GPIO的输出值
 * GPIO_PORT: 0-1
 * GPIO_PIN: 0-7
 * bit: 0-1
 */
void XL9555_gpio_WriteBit(uint8_t GPIO_PORT, uint8_t GPIO_PIN, uint8_t bit)
{
	if (GPIO_PORT > 1 || GPIO_PIN > 7 || bit > 1)
	{
		ESP_LOGE(TAG, "XL9555_GPIO_PORT or XL9555_GPIO_PIN or bit error");
		return;
	}
	uint8_t reg_value = 0;
	uint8_t output_reg_addr;
	uint8_t temp_data[2];
	// 1.设置指定位的控制寄存器为OUTPUT模式
	switch (GPIO_PORT)
	{
	case 0:
		output_reg_addr = XL9555_OUTPUT_0_REG_ADDR;
		break;
	case 1:
		output_reg_addr = XL9555_OUTPUT_1_REG_ADDR;
		break;
	default:
		ESP_LOGE(TAG, "XL9555_GPIO_PORT error");
		return;
	}
	XL9555_gpio_Mode(GPIO_PORT, GPIO_PIN, XL9555_OUTPUT_MODE);
	// 2.设置指定PIN的输出值
	reg_value = 0;
	// 2.1 读取当前输出寄存器状态
	ESP_ERROR_CHECK(i2c_master_transmit_receive(XL9555_dev_handle, &output_reg_addr, 1, &reg_value, 1, I2C_MASTER_TIMEOUT_MS));
	// 2.2 设置指定PIN的输出值
	if (bit)
	{
		reg_value |= (1 << GPIO_PIN);
	}
	else
	{
		reg_value &= ~(1 << GPIO_PIN);
	}
	// 2.3 写入输出寄存器
	temp_data[0] = output_reg_addr;
	temp_data[1] = reg_value;
	ESP_ERROR_CHECK(i2c_master_transmit(XL9555_dev_handle, temp_data, 2, I2C_MASTER_TIMEOUT_MS));
}

void XL9555_gpio_Setbit(uint8_t GPIO_PORT, uint8_t GPIO_PIN)
{
	XL9555_gpio_WriteBit(GPIO_PORT, GPIO_PIN, 1);
}

void XL9555_gpio_Resetbit(uint8_t GPIO_PORT, uint8_t GPIO_PIN)
{
	XL9555_gpio_WriteBit(GPIO_PORT, GPIO_PIN, 0);
}

uint8_t XL9555_gpio_Readbit(uint8_t GPIO_PORT, uint8_t GPIO_PIN)
{
	uint8_t reg_value = 0;
	uint8_t input_reg_addr;
	if (GPIO_PORT > 1 || GPIO_PIN > 7)
	{
		ESP_LOGE(TAG, "XL9555_GPIO_PORT or XL9555_GPIO_PIN error");
		return 9;
	}
	switch (GPIO_PORT)
	{
	case 0:
		input_reg_addr = XL9555_INPUT_0_REG_ADDR;
		break;
	case 1:
		input_reg_addr = XL9555_INPUT_1_REG_ADDR;
		break;
	default:
		ESP_LOGE(TAG, "XL9555_GPIO_PORT error");
		return 9;
	}
	// 读取当前输入寄存器状态
	ESP_ERROR_CHECK(i2c_master_transmit_receive(XL9555_dev_handle, &input_reg_addr, 1, &reg_value, 1, I2C_MASTER_TIMEOUT_MS));

	// 返回指定PIN的输入值
	return (reg_value >> GPIO_PIN) & 0x01;
}