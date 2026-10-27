#include "key.h"

void Key_Init(void)
{
	XL9555_init();
	XL9555_gpio_Mode(1, 5, XL9555_INPUT_MODE);
	XL9555_gpio_Mode(1, 6, XL9555_INPUT_MODE);
}

key_state Key_Get_State(key_num key)
{
	uint16_t key_press_count = 0;
	switch (key)
	{
	case KEY_1:
		while (!XL9555_gpio_Readbit(1, 6))
		{
			key_press_count++;
			vTaskDelay(pdMS_TO_TICKS(5));
			if (key_press_count > 600)
			{
				return KEY_RELEASED;
			}
			if (key_press_count > 4)
			{
				if (XL9555_gpio_Readbit(1, 6))
					return KEY1_PRESSED;
			}
		}
		key_press_count = 0;
		break;
	case KEY_2:
		while (!XL9555_gpio_Readbit(1, 5))
		{
			key_press_count++;
			vTaskDelay(pdMS_TO_TICKS(5));
			if (key_press_count > 600)
			{
				return KEY_RELEASED;
			}
			if (key_press_count > 4)
			{
				if (XL9555_gpio_Readbit(1, 5))
					return KEY2_PRESSED;
			}
		}
		key_press_count = 0;
		break;
	default:
		break;
	}
	return KEY_RELEASED;
}