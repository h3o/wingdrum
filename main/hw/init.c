/*
 * init.c
 *
 *  Copyright 2024 Phonicbloom Ltd.
 *
 *  Created on: Jan 23, 2018
 *      Author: mario
 *
 *  This file can be used within the terms of GNU GPLv3 license: https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 *  Find more information at:
 *  http://phonicbloom.com/
 *  http://gechologic.com/
 *
 */

#include "init.h"
#include "gpio.h"
#include "codec.h"
#include "signals.h"
#include "midi.h"
#include "glo_config.h"
#include "ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

#include <Accelerometer.h>

#define DEBUG_OUTPUT

i2c_port_t i2c_num;
int i2c_driver_installed = 0;
int i2c_bus_mutex = 0;
int glo_run = 0;
int touch_pad_calibrated = 0;
int touch_pad_enabled = 0;

//int16_t ms10_counter = 0; //sync counter for various processes (buttons, volume ramps, MCLK off)
int auto_power_off = 0, auto_power_off_canceled = 0;
int last_button_event, last_touch_event;

size_t i2s_bytes_rw;

int init_free_mem;

esp_err_t event_handler(void *ctx, system_event_t *event)
{
    return ESP_OK;
}

void init_i2s_and_gpio(int buf_count, int buf_len, int sample_rate)
{
	//printf("Initialize I2S\n");

	i2s_config_t i2s_config = {
        .mode = I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX,                    // enable TX and RX
        .sample_rate = sample_rate,
        .bits_per_sample = 16,                                                  //16-bit per channel
        .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,                           //2-channels
        .communication_format = I2S_COMM_FORMAT_STAND_I2S, //I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB,
#ifdef USE_APLL
		.use_apll = true,
#else
		.use_apll = false,
#endif
		.dma_buf_count = buf_count,
		.dma_buf_len = buf_len,

		.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1                                //Interrupt level 1
    };

	i2s_pin_config_t pin_config = {

		.bck_io_num = 17,			//bit clock
        .ws_io_num = 18,			//byte clock
        .data_in_num = 34,			//MISO
        .data_out_num = 5,			//MOSI
    };

    i2s_driver_install(I2S_NUM, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM, &pin_config);

    current_sampling_rate = sample_rate;
}

void init_deinit_TWDT()
{
	//printf("Initialize TWDT\n");
	//Initialize or reinitialize TWDT
	//CHECK_ERROR_CODE(esp_task_wdt_init(TWDT_TIMEOUT_S, false), ESP_OK);
	CHECK_ERROR_CODE(esp_task_wdt_init(5, false), ESP_OK);

	//Subscribe Idle Tasks to TWDT if they were not subscribed at startup
	#ifndef CONFIG_TASK_WDT_CHECK_IDLE_TASK_CPU0
	esp_task_wdt_add(xTaskGetIdleTaskHandleForCPU(0));
	#endif
	#ifndef CONFIG_TASK_WDT_CHECK_IDLE_TASK_CPU1
	esp_task_wdt_add(xTaskGetIdleTaskHandleForCPU(1));
	#endif

    //unsubscribe idle tasks
    CHECK_ERROR_CODE(esp_task_wdt_delete(xTaskGetIdleTaskHandleForCPU(0)), ESP_OK);     //Unsubscribe Idle Task from TWDT
    CHECK_ERROR_CODE(esp_task_wdt_status(xTaskGetIdleTaskHandleForCPU(0)), ESP_ERR_NOT_FOUND);      //Confirm Idle task has unsubscribed

    CHECK_ERROR_CODE(esp_task_wdt_delete(xTaskGetIdleTaskHandleForCPU(1)), ESP_OK);     //Unsubscribe Idle Task from TWDT
    CHECK_ERROR_CODE(esp_task_wdt_status(xTaskGetIdleTaskHandleForCPU(1)), ESP_ERR_NOT_FOUND);      //Confirm Idle task has unsubscribed

    //Deinit TWDT after all tasks have unsubscribed
    CHECK_ERROR_CODE(esp_task_wdt_deinit(), ESP_OK);
    CHECK_ERROR_CODE(esp_task_wdt_status(NULL), ESP_ERR_INVALID_STATE);     //Confirm TWDT has been deinitialized

    //printf("TWDT Deinitialized\n");
}

float micros()
{
    static struct timeval time_of_day;
    gettimeofday(&time_of_day, NULL);
    return time_of_day.tv_sec + time_of_day.tv_usec / 1000000.0;
}

uint32_t micros_i()
{
    static struct timeval time_of_day;
    gettimeofday(&time_of_day, NULL);
    return 1000000.0 * time_of_day.tv_sec + time_of_day.tv_usec;
}

uint32_t millis()
{
    static struct timeval time_of_day;
    gettimeofday(&time_of_day, NULL);
    return 1000.0 * time_of_day.tv_sec + time_of_day.tv_usec / 1000;
}

void enable_I2S_MCLK_clock() {
	//printf("Codec Init: Enabling MCLK...");

	vTaskDelay(100 / portTICK_RATE_MS);

	REG_WRITE(PIN_CTRL, 0b111111110000);
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);

	//-------------------------------------------------------------------------

	vTaskDelay(100 / portTICK_RATE_MS);
    //printf(" done\n");
}

void disable_I2S_MCLK_clock()
{
	//printf("disable_out_clock(): Disabling MCLK... ");
	PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_GPIO0_0); //back to GPIO function
	//printf(" done\n");
}

void i2c_master_init(int speed)
{
	//printf("I2C Master Init...");

	int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_DISABLE;//GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_DISABLE;//GPIO_PULLUP_ENABLE;

    if(speed)
    {
    	//printf(" FAST MODE (1MHz)");
    	conf.master.clk_speed = I2C_MASTER_FREQ_HZ_FAST;
    }
    else
    {
    	//printf(" NORMAL MODE (400kHz)");
    	conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    }

    i2c_param_config(i2c_master_port, &conf);
    i2c_driver_install(i2c_master_port, conf.mode,
                       I2C_MASTER_RX_BUF_DISABLE,
                       I2C_MASTER_TX_BUF_DISABLE, 0);

    //printf(" driver installed\n");
    i2c_driver_installed = 1;
}

void i2c_master_deinit()
{
	i2c_driver_installed = 0;
	//printf("I2C Master Deinit...");

	int i2c_master_port = I2C_MASTER_NUM;
    i2c_driver_delete(i2c_master_port);

    //printf(" driver uninstalled\n");
}

void i2c_scan_for_devices(int print, uint8_t *addresses, uint8_t *found)
{
	int ret;
	if(found)
	{
		found[0] = 0;
	}
    for(uint16_t address = 0;address<128;address++)
	{
    	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, address << 1 | WRITE_BIT, ACK_CHECK_EN); //set W/R bit to write
        i2c_master_write_byte(cmd, 0x00, ACK_CHECK_EN); //send a zero byte
        i2c_master_stop(cmd);
        ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
        i2c_cmd_link_delete(cmd);

        if(ret == ESP_OK)
		{
			if(print)
			{
				printf("device responds at address %u\n",address);
			}
			if(addresses)
			{
				addresses[found[0]] = address;
				found[0]++;
			}
			vTaskDelay(1 / portTICK_PERIOD_MS);
		}
		else //e.g. ESP_FAIL
		{
		}
	}
	if(print)
	{
		printf("\nScan done.\n");
	}
}

int i2c_codec_two_byte_command(uint8_t b1, uint8_t b2)
{
	//printf("i2c_codec_two_byte_command(reg=%x(%d), val=%x(%d): ", b1, b1, b2, b2);

	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, CODEC_I2C_ADDRESS << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, b1, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, b2, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    int ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    //printf("result=%x(%d)\n", ret, ret);
    return ret;
}

uint8_t i2c_codec_register_read(uint8_t reg)
{
	uint8_t val;
	//printf("i2c_codec_register_read(reg=%x(%d): ", reg, reg);

	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, CODEC_I2C_ADDRESS << 1 | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, CODEC_I2C_ADDRESS << 1 | READ_BIT, ACK_CHECK_EN);
    i2c_master_read_byte(cmd, &val, I2C_MASTER_NACK);
	i2c_master_stop(cmd);
    int ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);

    if(ret!=ESP_OK)
    {
    	printf("ERROR=%x(%d)\n-------------------------------------------------\n", ret, ret);
    	return 0;
    }

    //printf("result=%x(%d)\n", val, val);
    return val;
}

int i2c_bus_write(int addr_rw, unsigned char *data, int len)
{
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, addr_rw, ACK_CHECK_EN);
	i2c_master_write(cmd, data, len, ACK_CHECK_EN);

	i2c_master_stop(cmd);
	esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);

	if(ret == ESP_OK)
	{
		//printf("[i2c_bus_write]: I2C command %x accepted at address %x \n", data[0], addr_rw);
	}
	else //e.g. ESP_FAIL
	{
		printf("[i2c_bus_write]: I2C command %x NOT accepted at address %x \n", data[0], addr_rw);
	}
	return ret;
}

int i2c_bus_read(int addr_rw, unsigned char *buf, int buf_len)
{
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
	i2c_master_start(cmd);
	i2c_master_write_byte(cmd, addr_rw, ACK_CHECK_EN);

	//i2c_master_read(cmd, buf, buf_len, I2C_MASTER_ACK);
	//i2c_master_read(cmd, buf, buf_len, I2C_MASTER_NACK);
	i2c_master_read(cmd, buf, buf_len, I2C_MASTER_LAST_NACK);
    //I2C_MASTER_ACK = 0x0,        /*!< I2C ack for each byte read */
    //I2C_MASTER_NACK = 0x1,       /*!< I2C nack for each byte read */
    //I2C_MASTER_LAST_NACK = 0x2,   /*!< I2C nack for the last byte*/

	i2c_master_stop(cmd);
	esp_err_t ret = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
	i2c_cmd_link_delete(cmd);

	if(ret == ESP_OK)
	{
		//printf("[i2c_bus_read]: I2C data %x received from address %x \n", buf[0], addr_rw);
	}
	else //e.g. ESP_FAIL
	{
		printf("[i2c_bus_read]: I2C data NOT received from address %x \n", addr_rw);
	}
	return ret;
}

const char* FW_VERSION = "[Gv5/1.0.001]";

uint16_t hardware_check()
{
	uint8_t adr[20];
	uint8_t found;
	i2c_scan_for_devices(0, adr, &found);
	printf("hardware_check(): found %d i2c devices: ", found);

	#ifdef BOARD_WINGDRUM
	#define I2C_DEVICES_EXPECTED 4
	uint8_t expected[I2C_DEVICES_EXPECTED] = {0x00,0x12,0x18,0x60};
	#endif

	uint8_t as_expected = 0;
	uint8_t i2c_errors = 0;

	for(int i=0;i<found;i++)
	{
		printf("%02x ", adr[i]);
		if(adr[i]==expected[i])
		{
			as_expected++;
		}
		else
		{
			i2c_errors += 1<<i;
		}
	}
	printf("\n");

	printf("as_expected = %d\n", as_expected);

	if(found!=I2C_DEVICES_EXPECTED || as_expected!=I2C_DEVICES_EXPECTED)
	{
		return i2c_errors + (found << 8);
	}
	else
	{
		return 0;
	}
}

void generate_random_seed()
{
	//randomize the pseudo RNG seed
	bootloader_random_enable();
	set_pseudo_random_seed((double)esp_random() / (double)UINT32_MAX);
	bootloader_random_disable();
}

void drum_LED_expander_test()
{
	printf("drum_LED_expander_test()\n");

	while(1)
	{
		LED_0_ON; Delay(LED_TEST_DELAY);LED_0_OFF;
		LED_1_ON; Delay(LED_TEST_DELAY);LED_1_OFF;
		LED_2_ON; Delay(LED_TEST_DELAY);LED_2_OFF;
		LED_3_ON; Delay(LED_TEST_DELAY);LED_3_OFF;
		LED_4_ON; Delay(LED_TEST_DELAY);LED_4_OFF;
		LED_5_ON; Delay(LED_TEST_DELAY);LED_5_OFF;
		LED_6_ON; Delay(LED_TEST_DELAY);LED_6_OFF;
		LED_7_ON; Delay(LED_TEST_DELAY);LED_7_OFF;
	}
}

void power_on_animation()
{
	//printf("power_on_animation()\n");

	LED_6_ON;Delay(LED_PWR_ON_DELAY);
	LED_7_ON;Delay(LED_PWR_ON_DELAY);
	LED_0_ON;Delay(LED_PWR_ON_DELAY);
	LED_1_ON;Delay(LED_PWR_ON_DELAY);
	LED_2_ON;Delay(LED_PWR_ON_DELAY);
	LED_3_ON;Delay(LED_PWR_ON_DELAY);
	LED_4_ON;Delay(LED_PWR_ON_DELAY);
	LED_5_ON;
}

void power_on_animation_reversed(int start_from)
{
	printf("power_on_animation_reversed()\n");

	LED_5_ON;if(start_from<1)Delay(LED_PWR_ON_DELAY);
	LED_4_ON;if(start_from<2)Delay(LED_PWR_ON_DELAY);
	LED_3_ON;if(start_from<3)Delay(LED_PWR_ON_DELAY);
	LED_2_ON;if(start_from<4)Delay(LED_PWR_ON_DELAY);
	LED_1_ON;if(start_from<5)Delay(LED_PWR_ON_DELAY);
	LED_0_ON;if(start_from<6)Delay(LED_PWR_ON_DELAY);
	LED_7_ON;if(start_from<7)Delay(LED_PWR_ON_DELAY);
	LED_6_ON;
}

int power_off_animation(int auto_shutdown, int delay)
{
	printf("power_off_animation()\n");

	LEDS_ALL_ON;

	Delay(delay);if((!BUTTON_U1_ON && !auto_shutdown) || (auto_shutdown && note_updated>=0)) return 8;LED_6_OFF;
	//printf("note_updated=%d\n", note_updated);
	Delay(delay);if((!BUTTON_U1_ON && !auto_shutdown) || (auto_shutdown && note_updated>=0)) return 7;LED_7_OFF;
	Delay(delay);if((!BUTTON_U1_ON && !auto_shutdown) || (auto_shutdown && note_updated>=0)) return 6;LED_0_OFF;
	Delay(delay);if((!BUTTON_U1_ON && !auto_shutdown) || (auto_shutdown && note_updated>=0)) return 5;LED_1_OFF;
	Delay(delay);if((!BUTTON_U1_ON && !auto_shutdown) || (auto_shutdown && note_updated>=0)) return 4;LED_2_OFF;
	Delay(delay);if((!BUTTON_U1_ON && !auto_shutdown) || (auto_shutdown && note_updated>=0)) return 3;LED_3_OFF;
	Delay(delay);if((!BUTTON_U1_ON && !auto_shutdown) || (auto_shutdown && note_updated>=0)) return 2;LED_4_OFF;
	Delay(delay);if((!BUTTON_U1_ON && !auto_shutdown) || (auto_shutdown && note_updated>=0)) return 1;LED_5_OFF;

	return 0;
}


#define EXP_I2C_ADDRESS 0x60 //PCA9552PW

void LED_exp_2_byte_cmd(uint8_t b1, uint8_t b2)
{
	i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);

    //send I2C address with write bit
    i2c_master_write_byte(cmd, EXP_I2C_ADDRESS << 1 | WRITE_BIT, ACK_CHECK_EN);

    i2c_master_write_byte(cmd, b1, ACK_CHECK_EN);
    i2c_master_write_byte(cmd, b2, ACK_CHECK_EN);

    i2c_master_stop(cmd);

    /*int ret =*/ i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
}

void drum_LED_expanders_update(int quadrant)
{
	//printf("drum_LED_expanders_update(): quadrant = %d\n", quadrant);

	//uint8_t tmp;
	//uint8_t reg;

	while(i2c_bus_mutex)
	{
		//printf("drum_LED_expanders_update(): waiting for I2C bus\n");
		Delay(EXPANDERS_TIMING_DELAY);
	}

	i2c_bus_mutex = 1;

	LED_exp_2_byte_cmd(LED_REGS+quadrant,LED_map[quadrant]);

	i2c_bus_mutex = 0;
}

void drum_LED_expanders_set_blink_rates()
{
	//printf("drum_LED_expanders_set_blink_rates()");

	while(i2c_bus_mutex)
	{
		//printf("drum_LED_expanders_set_blink_rates(): waiting for I2C bus\n");
		Delay(EXPANDERS_TIMING_DELAY);
	}

	i2c_bus_mutex = 1;

	//LED_exp_2_byte_cmd(0x02,0x2b);	//Set prescaler PSC0 to achieve a period of 1 second: (PSC0 + 1) / 44 => (43 + 1) / 44
	LED_exp_2_byte_cmd(0x02,0x08);	//Set prescaler PSC0 to achieve a period of ~0.2 seconds: (PSC0 + 1) / 44 => (8 + 1) / 44
	//LED_exp_2_byte_cmd(0x02,0x0a);	//Set prescaler PSC0 to achieve a period of 0.25 seconds
	LED_exp_2_byte_cmd(0x03,0x80);	//Set PWM0 duty cycle to 50%: (256 � PWM0) / 256 => (256 � 128) / 256 = 0.5
	//LED_exp_2_byte_cmd(0x04,0x15);	//Set prescaler PCS1 to achieve a period of 0.5 seconds: (PSC0 + 1) / 44 => (21 + 1) / 44
	//LED_exp_2_byte_cmd(0x04,0x0a);	//Set prescaler PCS1 to achieve a period of 0.25 seconds: (PSC0 + 1) / 44 => (10 + 1) / 44
	//LED_exp_2_byte_cmd(0x04,0x05);	//Set prescaler PCS1 to achieve a period of ~0.136 seconds: (PSC0 + 1) / 44 => (5 + 1) / 44
	LED_exp_2_byte_cmd(0x04,0x03);	//Set prescaler PCS1 to achieve a period of ~0.1 seconds: (PSC0 + 1) / 44 => (3 + 1) / 44
	//LED_exp_2_byte_cmd(0x04,0x01);	//Set prescaler PCS1 to achieve a period of 1/22 seconds: (PSC0 + 1) / 44 => (1 + 1) / 44
	//LED_exp_2_byte_cmd(0x05,0xc0);	//Set PWM1 output duty cycle to 25%: (256 � PWM1) / 256 => (256 � 192) / 256 = 0.25
	//LED_exp_2_byte_cmd(0x05,0xe6);	//Set PWM1 output duty cycle to 10%: (256 � PWM1) / 256 => (256 � 230) / 256 = ~0.1
	LED_exp_2_byte_cmd(0x05,0x80);	//Set PWM1 output duty cycle to 50%

	i2c_bus_mutex = 0;
}

//#define MIDI_SIGNAL_HW_TEST
#define MIDI_OUT_ENABLED

QueueHandle_t uart_queue;

void drum_init_MIDI(int uart_num, int midi_out_enabled)
{
	#ifdef MIDI_SIGNAL_HW_TEST

	//this config is for Glo only!
	PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[GPIO_NUM_16], PIN_FUNC_GPIO); //normally U2RXD
	int result = gpio_set_direction(GPIO_NUM_16, GPIO_MODE_INPUT); //MIDI in (test)
	printf("GPIO16 direction set result = %d\n",result);
	result = gpio_set_pull_mode(GPIO_NUM_16, GPIO_FLOATING);
	printf("GPIO16 pull mode set result = %d\n",result);

	int val,val0=0;
	while(1)
	{
		val = gpio_get_level(GPIO_NUM_16); //MIDI in (test)
		printf("GPIO16=%d...\r",val);
		//Delay(1);
		if(val0!=val)
		{
			val0=val;
			printf("\n");
		}
	}

	/*
	//test midi signal reception at alternative pin #18 (instead of default U2RXD #16)
	int result = gpio_set_direction(GPIO_NUM_18, GPIO_MODE_INPUT); //analog buttons
	printf("GPIO18 direction set result = %d\n",result);
	//result = gpio_set_pull_mode(GPIO_NUM_18, GPIO_FLOATING);
	//printf("GPIO18 pull mode set result = %d\n",result);

	int val,val0=0;
	while(1)
	{
		val = gpio_get_level(GPIO_NUM_18); //MIDI in alt signal (test)
		printf("GPIO18=%d...\r",val);
		//Delay(1);
		if(val0!=val)
		{
			val0=val;
			printf("\n");
		}
	}
	*/
	#else

	//https://docs.espressif.com/projects/esp-idf/en/latest/api-reference/peripherals/uart.html

	uart_config_t uart_config = {
		.baud_rate = 31250,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,//UART_HW_FLOWCTRL_CTS_RTS,
		.rx_flow_ctrl_thresh = 122,
	};
	// Configure UART parameters
	ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));

	ESP_ERROR_CHECK(uart_set_pin(MIDI_UART, MIDI_OUT_SIGNAL, MIDI_IN_SIGNAL, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
	//ESP_ERROR_CHECK(uart_set_line_inverse(MIDI_UART, UART_INVERSE_TXD));

	// Setup UART buffered IO with event queue
	const int uart_buffer_size = (1024 * 2);

	// Install UART driver using an event queue here
	//ESP_ERROR_CHECK(uart_driver_install(MIDI_UART, uart_buffer_size, uart_buffer_size, 10, &uart_queue, 0));
	// Install UART driver without using an event queue
	ESP_ERROR_CHECK(uart_driver_install(MIDI_UART, uart_buffer_size, uart_buffer_size, 10, NULL, 0));

	#endif
}

/*
void drum_test_MIDI_output()
{
	const char note[16][3] = {
		{0x90,0x29,0x64},
		{0x80,0x29,0x7f},
		{0x90,0x2b,0x64},
		{0x80,0x2b,0x7f},
		{0x90,0x2d,0x64},
		{0x80,0x2d,0x7f},
		{0x90,0x2f,0x64},
		{0x80,0x2f,0x7f},
		{0x90,0x30,0x64},
		{0x80,0x30,0x7f},
		{0x90,0x32,0x64},
		{0x80,0x32,0x7f},
		{0x90,0x34,0x64},
		{0x80,0x34,0x7f},
		{0x90,0x35,0x64},
		{0x80,0x35,0x7f}
	};

	gecho_deinit_MIDI(); //in case driver already running with a different configuration
	gecho_init_MIDI(MIDI_UART, 1); //midi out enabled

	while(1)
	{
		for(int i=0;i<16;i++)
		{
			int length = uart_write_bytes(MIDI_UART, note[i], 3);
			printf("UART[%d] sent %d bytes: %02x %02x %02x\n", MIDI_UART, length, note[i][0], note[i][1], note[i][2]);
			Delay(500);
		}
	}
}
*/

void drum_restart()
{
	codec_set_mute(1); //mute the codec
	Delay(20);
	codec_reset();
	Delay(10);
	esp_restart();
	while(1);
}

void low_voltage_poweroff(void *params)
{
	printf("low_voltage_poweroff()\n");
	drum_shutdown();
}

void brownout_init()
{
	#define BROWNOUT_DET_LVL 0

	REG_WRITE(RTC_CNTL_BROWN_OUT_REG,
            RTC_CNTL_BROWN_OUT_ENA // Enable BOD
            | RTC_CNTL_BROWN_OUT_PD_RF_ENA // Automatically power down RF
            //Reset timeout must be set to >1 even if BOR feature is not used
            | (2 << RTC_CNTL_BROWN_OUT_RST_WAIT_S)
            | (BROWNOUT_DET_LVL << RTC_CNTL_DBROWN_OUT_THRES_S));

    rtc_isr_register(low_voltage_poweroff, NULL, RTC_CNTL_BROWN_OUT_INT_ENA_M);
    //printf("Initialized BOD\n");

    REG_SET_BIT(RTC_CNTL_INT_ENA_REG, RTC_CNTL_BROWN_OUT_INT_ENA_M);
}

void free_memory_info()
{
	int current_free_mem = xPortGetFreeHeapSize();
	printf("free_memory_info(): initial free memory = %u, currently free: %u, loss = %d\n", init_free_mem, current_free_mem, init_free_mem - current_free_mem);
}

//https://github.com/espressif/esp-idf/blob/master/examples/peripherals/touch_pad_read/main/esp32/tp_read_main.c

#define TOUCH_PAD_NO_CHANGE   (-1)
#define TOUCH_THRESH_NO_USE   (0)
#define TOUCH_FILTER_MODE_EN  (0) //(1)
#define TOUCHPAD_FILTER_TOUCH_PERIOD (10)

int TOUCHPAD_LED_THRESHOLD_A[10];
int SPLIT_TIMBRE_NOTE_THRESHOLD1_A[10], SPLIT_TIMBRE_NOTE_THRESHOLD2_A[10];
int TOUCHPAD_MAX_LEVEL_A[10] = {0,0,0,0,0,0,0,0,0,0};

int new_MIDI_note[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
int new_notes_timbre[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
int new_touch_event[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
int note_updated = -1, note_updated_pad, notes_on = 0;
int note_map[9] = {3,4,5,6,7,8,1,2,0};
int note_map_reverse[9] = {8,6,7,0,1,2,3,4,5};
int note_map_reverse_center_last[9] = {6,7,0,1,2,3,4,5,8};

int current_patch, *patch_notes;
int *midi_scale_selected;
float *tuning_coeffs;
float *micro_tuning, *micro_tuning_selected;

int **notes_reverb;

int *midi_scale_clipboard;
float *micro_tuning_clipboard;

void new_note(int pad, int note, int status, int level0)
{
	note = note_map[note];

	int level = level0;

	if(level < NEW_NOTE_MIN_VELOCITY)
	{
		level = NEW_NOTE_MIN_VELOCITY;
	}

	if(status && level0>TOUCHPAD_MAX_LEVEL_A[pad])
	{
		//printf("updating max[%d]: %d -> %d\n", pad, TOUCHPAD_MAX_LEVEL_A[pad], level0);
		TOUCHPAD_MAX_LEVEL_A[pad] = level0;
		SPLIT_TIMBRE_NOTE_THRESHOLD1_A[pad] = (float)TOUCHPAD_MAX_LEVEL_A[pad] * global_settings.SPLIT_TIMBRE_NOTE_THRESHOLD1_COEFF;
		SPLIT_TIMBRE_NOTE_THRESHOLD2_A[pad] = (float)TOUCHPAD_MAX_LEVEL_A[pad] * global_settings.SPLIT_TIMBRE_NOTE_THRESHOLD2_COEFF;
	}

	//new_MIDI_note[note] = midi_scale_default[note];// + 12; //replace this with a transpose or fine tuning parameter
	new_MIDI_note[note] = midi_scale_selected[note];

	int velocity = level / 4;
	if(velocity > 127)
	{
		velocity = 127;
	}

	int changed = MIDI_note(note, new_MIDI_note[note], status, velocity);

	if(changed)
	{
		if(status)
		{
			//printf("new_note(pad:%d, note:%d->%d, status:%d, level:%d -> %d, velocity:%d)\n", pad, note, new_MIDI_note[note], status, level0, level, velocity);

			#ifdef DYNAMICS_BY_ADC
			if(!notes_on)
			{
				//uint16_t acc_z = accelerometer_read_Z();
				//printf("new_note(): acc_z = %d\n", acc_z);

				#define ADC_DYN_RB_PTR_FRAGMENT	200
				#define ADC_DYN_RB_IGNORE_START	100

				sample_ADC_dyn_RB_AVG = 0;
				skip_ADC_samples += ADC_DYN_RB_PTR_FRAGMENT;

				for(sample_ADC_dyn_RB_ptr=0;sample_ADC_dyn_RB_ptr<ADC_DYN_RB_PTR_FRAGMENT;sample_ADC_dyn_RB_ptr++)
				{
					i2s_read(I2S_NUM, (void*)&ADC_sample, 4, &i2s_bytes_rw, portMAX_DELAY);
					//sample_i16 = abs((int16_t)ADC_sample);
					//sample_ADC_dynamic_RB[sample_ADC_dyn_RB_ptr] = sample_i16;
					if(sample_ADC_dyn_RB_ptr>ADC_DYN_RB_IGNORE_START)
					{
						sample_ADC_dyn_RB_AVG += abs((int16_t)ADC_sample);
					}
				}
				sample_ADC_dyn_RB_AVG /= (ADC_DYN_RB_PTR_FRAGMENT - ADC_DYN_RB_IGNORE_START);

				/*
				for(sample_ADC_dyn_RB_ptr=0;sample_ADC_dyn_RB_ptr<ADC_DYN_RB_PTR_FRAGMENT;sample_ADC_dyn_RB_ptr++)
				{
					printf("%d\t", sample_ADC_dynamic_RB[sample_ADC_dyn_RB_ptr]);
				}
				printf("\n");
				*/

				printf("sample_ADC_dyn_RB_AVG=%d\n", sample_ADC_dyn_RB_AVG);
			}
			#endif

			notes_on++;

			new_touch_event[note] = level;
			note_updated = note;
			note_updated_pad = pad;
			//printf("new_touch_event[%d] = 1)\n", note);
			//printf("new_note(%d,%d): sample_ADC_LPF = %d\n", note, status, sample_ADC_LPF);

			MIDI_pitch_base_ptr++;
			if(MIDI_pitch_base_ptr==MIDI_pitch_base_MAX)
			{
				MIDI_pitch_base_ptr = 0;
			}

			MIDI_pitch_base[MIDI_pitch_base_ptr] = (float)new_MIDI_note[note_updated] - 48.0f;

			//printf("new_note(): new_touch_event[%d] = 1, note_updated = %d, notes_on = %d, MIDI_pitch_base_ptr = %d\n", note, note_updated, notes_on, MIDI_pitch_base_ptr);

			/*
			for(int i=0; i<ADC_DYN_RB; i++)
			{
				if(sample_ADC_dynamic_RB[i]>1000)
				{
					printf("%d ", sample_ADC_dynamic_RB[i]);
				}
			}
			printf("\n");
			*/
		}
		else if(notes_on>0)
		{
			notes_on--;
		}
	}
}

void adjust_note(int note)
{
	last_touch_event++;

	if(delay_settings)
	{
		printf("adjust_note(%d): delay_settings = %d\n", note, delay_settings);
	}
	else if(scale_settings)
	{
		scale_settings = SCALE_SETTINGS_LEVEL_NOTE;
		adjusting_note = note;

		if(note==NOTE_CENTER)
		{
			LEDS_ALL_ON;
			LED_0_B1;
			LED_1_B1;
			LED_2_B1;
			LED_3_B1;
			LED_4_B1;
			LED_5_B1;
			LED_6_B1;
			LED_7_B1;
		}
		else
		{
			LEDS_ALL_OFF;
			LED_X_B1(note);
		}
		new_note(0, note,1,SCALE_PLAY_VELOCITY_DEFAULT);
		Delay(SCALE_PLAY_NOTE_ON_FAST);
		new_note(0, note,0,0);
	}
}

void touch_pad_process(void *pvParameters)
{
	uint16_t touch_value;//, touch_filter_value;
	int pad_states[9] = {0,0,0,0,0,0,0,0,0};

	// Initialize touch pad peripheral.
	// The default fsm mode is software trigger mode.
	touch_pad_init();

	// Set reference voltage for charging/discharging
	// In this case, the high reference voltage will be 2.7V - 1V = 1.7V
	// The low reference voltage will be 0.5
	// The larger the range, the larger the pulse count value.
	touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
	//touch_pad_set_voltage(TOUCH_HVOLT_2V4, TOUCH_LVOLT_0V8, TOUCH_HVOLT_ATTEN_1V);
	//touch_pad_set_voltage(TOUCH_HVOLT_2V4, TOUCH_LVOLT_0V8, TOUCH_HVOLT_ATTEN_1V5);
	//touch_pad_set_voltage(TOUCH_HVOLT_2V4, TOUCH_LVOLT_0V8, TOUCH_HVOLT_ATTEN_0V5);

	touch_pad_config(TOUCH_PAD_NUM0, TOUCH_THRESH_NO_USE);
	touch_pad_config(TOUCH_PAD_NUM2, TOUCH_THRESH_NO_USE);
	touch_pad_config(TOUCH_PAD_NUM3, TOUCH_THRESH_NO_USE);
	touch_pad_config(TOUCH_PAD_NUM4, TOUCH_THRESH_NO_USE);
	touch_pad_config(TOUCH_PAD_NUM5, TOUCH_THRESH_NO_USE);
	touch_pad_config(TOUCH_PAD_NUM6, TOUCH_THRESH_NO_USE);
	touch_pad_config(TOUCH_PAD_NUM7, TOUCH_THRESH_NO_USE);
	touch_pad_config(TOUCH_PAD_NUM8, TOUCH_THRESH_NO_USE);
	touch_pad_config(TOUCH_PAD_NUM9, TOUCH_THRESH_NO_USE);

	for(int i=0;i<10;i++)
	{
		if(i==1) continue;
		TOUCHPAD_MAX_LEVEL_A[i] = global_settings.TOUCH_PAD_DEFAULT_MAX;
	}

	#define TOUCH_PAD_CALIBRATION_WAIT	30
	#define TOUCH_PAD_CALIBRATION_LOOPS	20

	int touch_pad_calibration_cnt = TOUCH_PAD_CALIBRATION_WAIT + TOUCH_PAD_CALIBRATION_LOOPS;
	uint32_t touch_pad_calibration_val[10] = {0,0,0,0,0,0,0,0,0,0};

	while(1)
	{
		ets_delay_us(100);

		if(touch_pad_calibration_cnt)
		{
			if(touch_pad_calibration_cnt<=TOUCH_PAD_CALIBRATION_LOOPS)
			{
				touch_pad_read(TOUCH_PAD_NUM0, &touch_value); touch_pad_calibration_val[0] += touch_value;
				//touch_pad_read(TOUCH_PAD_NUM1, &touch_value); touch_pad_calibration_val[1] += touch_value;
				touch_pad_read(TOUCH_PAD_NUM2, &touch_value); touch_pad_calibration_val[2] += touch_value;
				touch_pad_read(TOUCH_PAD_NUM3, &touch_value); touch_pad_calibration_val[3] += touch_value;
				touch_pad_read(TOUCH_PAD_NUM4, &touch_value); touch_pad_calibration_val[4] += touch_value;
				touch_pad_read(TOUCH_PAD_NUM5, &touch_value); touch_pad_calibration_val[5] += touch_value;
				touch_pad_read(TOUCH_PAD_NUM6, &touch_value); touch_pad_calibration_val[6] += touch_value;
				touch_pad_read(TOUCH_PAD_NUM7, &touch_value); touch_pad_calibration_val[7] += touch_value;
				touch_pad_read(TOUCH_PAD_NUM8, &touch_value); touch_pad_calibration_val[8] += touch_value;
				touch_pad_read(TOUCH_PAD_NUM9, &touch_value); touch_pad_calibration_val[9] += touch_value;
			}
			else
			{
				touch_pad_read(TOUCH_PAD_NUM0, &touch_value);
				//touch_pad_read(TOUCH_PAD_NUM1, &touch_value);
				touch_pad_read(TOUCH_PAD_NUM2, &touch_value);
				touch_pad_read(TOUCH_PAD_NUM3, &touch_value);
				touch_pad_read(TOUCH_PAD_NUM4, &touch_value);
				touch_pad_read(TOUCH_PAD_NUM5, &touch_value);
				touch_pad_read(TOUCH_PAD_NUM6, &touch_value);
				touch_pad_read(TOUCH_PAD_NUM7, &touch_value);
				touch_pad_read(TOUCH_PAD_NUM8, &touch_value);
				touch_pad_read(TOUCH_PAD_NUM9, &touch_value);
			}

			touch_pad_calibration_cnt--;

			if(touch_pad_calibration_cnt==0)
			{
				/*
				touch_pad_calibration_val /= 9; //center pad was left out
				TOUCHPAD_LED_THRESHOLD = (touch_pad_calibration_val / TOUCH_PAD_CALIBRATION_LOOPS);
				printf("touch_pad_process(): touch_pad_calibration_val = %d, rel = %d\n", touch_pad_calibration_val, TOUCHPAD_LED_THRESHOLD);

				SPLIT_TIMBRE_NOTE_THRESHOLD1 = TOUCHPAD_LED_THRESHOLD;
				SPLIT_TIMBRE_NOTE_THRESHOLD2 = TOUCHPAD_LED_THRESHOLD;

				TOUCHPAD_LED_THRESHOLD *= 950;
				TOUCHPAD_LED_THRESHOLD /= TOUCH_PAD_THRESHOLD_REF;
				TOUCHPAD_THRESHOLD_CENTER_COEFF = (float)750 / (float)950;
				TOUCHPAD_LED_THRESHOLD_CENTER = (int)((float)TOUCHPAD_LED_THRESHOLD * TOUCHPAD_THRESHOLD_CENTER_COEFF);
				printf("touch_pad_process(): TOUCHPAD_LED_THRESHOLD = %d, TOUCHPAD_LED_THRESHOLD_CENTER = %d\n", TOUCHPAD_LED_THRESHOLD, TOUCHPAD_LED_THRESHOLD_CENTER);

				SPLIT_TIMBRE_NOTE_THRESHOLD1 *= 250;
				SPLIT_TIMBRE_NOTE_THRESHOLD1 /= TOUCH_PAD_THRESHOLD_REF;
				SPLIT_TIMBRE_NOTE_THRESHOLD2 *= 500;
				SPLIT_TIMBRE_NOTE_THRESHOLD2 /= TOUCH_PAD_THRESHOLD_REF;
				printf("touch_pad_process(): SPLIT_TIMBRE_NOTE_THRESHOLD1 = %d, SPLIT_TIMBRE_NOTE_THRESHOLD2 = %d\n", SPLIT_TIMBRE_NOTE_THRESHOLD1, SPLIT_TIMBRE_NOTE_THRESHOLD2);
				*/

				//TOUCHPAD_THRESHOLD_CENTER_COEFF = 0.6f;

				for(int i=0;i<10;i++)
				{
					if(i==1) continue;

					touch_pad_calibration_val[i] /= TOUCH_PAD_CALIBRATION_LOOPS;
					TOUCHPAD_LED_THRESHOLD_A[i] = (touch_pad_calibration_val[i]*global_settings.TOUCHPAD_LED_THRESHOLD_A_MUL) / global_settings.TOUCHPAD_LED_THRESHOLD_A_DIV;
					/*
					if(i==8) //center pad is a lot more reactive
					{
						TOUCHPAD_LED_THRESHOLD_A[i] = (float)TOUCHPAD_LED_THRESHOLD_A[i] * TOUCHPAD_THRESHOLD_CENTER_COEFF;
					}
					*/
					//printf("touch_pad_process(): normalized touch_pad_calibration_val[%d] = %d, THR=%d\n", i, touch_pad_calibration_val[i], TOUCHPAD_LED_THRESHOLD_A[i]);

					/*
					if(i==8) //center pad is a lot more reactive
					{
						//SPLIT_TIMBRE_NOTE_THRESHOLD1_A[i] = touch_pad_calibration_val[i] / 3;
						//SPLIT_TIMBRE_NOTE_THRESHOLD2_A[i] = touch_pad_calibration_val[i] / 2;
						SPLIT_TIMBRE_NOTE_THRESHOLD1_A[i] = touch_pad_calibration_val[i] * 0.3f;
						SPLIT_TIMBRE_NOTE_THRESHOLD2_A[i] = touch_pad_calibration_val[i] * 0.5f;
					}
					else
					{
					* /
						//SPLIT_TIMBRE_NOTE_THRESHOLD1_A[i] = touch_pad_calibration_val[i] / 4;
						//SPLIT_TIMBRE_NOTE_THRESHOLD2_A[i] = touch_pad_calibration_val[i] / 3;
						SPLIT_TIMBRE_NOTE_THRESHOLD1_A[i] = (float)touch_pad_calibration_val[i] * 0.18;//0.2f;
						SPLIT_TIMBRE_NOTE_THRESHOLD2_A[i] = (float)touch_pad_calibration_val[i] * 0.33;//0.4f;
					/ *}*/

					SPLIT_TIMBRE_NOTE_THRESHOLD1_A[i] = (float)TOUCHPAD_MAX_LEVEL_A[i] * global_settings.SPLIT_TIMBRE_NOTE_THRESHOLD1_COEFF;
					SPLIT_TIMBRE_NOTE_THRESHOLD2_A[i] = (float)TOUCHPAD_MAX_LEVEL_A[i] * global_settings.SPLIT_TIMBRE_NOTE_THRESHOLD2_COEFF;

					printf("touch_pad_process(): normalized calibration_val[%d] = %d, THR=%d, SPL1=%d, SPL2=%d\n", i, touch_pad_calibration_val[i], TOUCHPAD_LED_THRESHOLD_A[i], SPLIT_TIMBRE_NOTE_THRESHOLD1_A[i], SPLIT_TIMBRE_NOTE_THRESHOLD2_A[i]);
				}

				touch_pad_calibrated = 1;
				LEDS_ALL_OFF;
			}

			continue;
		}

		if(!touch_pad_enabled)
		{
			continue;
		}

		if(scale_settings)
		{
			touch_pad_read(TOUCH_PAD_NUM0, &touch_value);
			if(touch_value<TOUCHPAD_LED_THRESHOLD_A[0]) { if(!pad_states[2]) { pad_states[2]=1; adjust_note(2); last_touch_event++; }} else if(pad_states[2]) { pad_states[2]=0; }

			touch_pad_read(TOUCH_PAD_NUM2, &touch_value);
			if(touch_value<TOUCHPAD_LED_THRESHOLD_A[2]) { if(!pad_states[1]) { pad_states[1]=1; adjust_note(1); last_touch_event++; }} else if(pad_states[1]) { pad_states[1]=0; }

			touch_pad_read(TOUCH_PAD_NUM3, &touch_value);
			if(touch_value<TOUCHPAD_LED_THRESHOLD_A[3]) { if(!pad_states[0]) { pad_states[0]=1; adjust_note(0); last_touch_event++; }} else if(pad_states[0]) { pad_states[0]=0; }

			touch_pad_read(TOUCH_PAD_NUM4, &touch_value);
			if(touch_value<TOUCHPAD_LED_THRESHOLD_A[4]) { if(!pad_states[6]) { pad_states[6]=1; adjust_note(6); last_touch_event++; }} else if(pad_states[6]) { pad_states[6]=0; }

			touch_pad_read(TOUCH_PAD_NUM5, &touch_value);
			if(touch_value<TOUCHPAD_LED_THRESHOLD_A[5]) { if(!pad_states[7]) { pad_states[7]=1; adjust_note(7); last_touch_event++; }} else if(pad_states[7]) { pad_states[7]=0; }

			touch_pad_read(TOUCH_PAD_NUM6, &touch_value);
			if(touch_value<TOUCHPAD_LED_THRESHOLD_A[6]) { if(!pad_states[3]) { pad_states[3]=1; adjust_note(3); last_touch_event++; }} else if(pad_states[3]) { pad_states[3]=0; }

			touch_pad_read(TOUCH_PAD_NUM7, &touch_value);
			if(touch_value<TOUCHPAD_LED_THRESHOLD_A[7]) { if(!pad_states[4]) { pad_states[4]=1; adjust_note(4); last_touch_event++; }} else if(pad_states[4]) { pad_states[4]=0; }

			touch_pad_read(TOUCH_PAD_NUM8, &touch_value);
			if(touch_value<TOUCHPAD_LED_THRESHOLD_A[8]) { if(!pad_states[8]) { pad_states[8]=1; adjust_note(8); last_touch_event++; }} else if(pad_states[8]) { pad_states[8]=0; }

			touch_pad_read(TOUCH_PAD_NUM9, &touch_value);
			if(touch_value<TOUCHPAD_LED_THRESHOLD_A[9]) { if(!pad_states[5]) { pad_states[5]=1; adjust_note(5); last_touch_event++; }} else if(pad_states[5]) { pad_states[5]=0; }
		}
		else if(delay_settings)
		{
			touch_pad_read(TOUCH_PAD_NUM0, &touch_value);
			if(touch_value<TOUCHPAD_LED_THRESHOLD_A[0]) { if(!pad_states[2]) { pad_states[2]=1; new_note(0,2,1,TOUCHPAD_LED_THRESHOLD_A[0]-touch_value); LED_2_OFF; last_touch_event++; }} else if(pad_states[2]) { pad_states[2]=0; LED_2_ON; new_note(0,2,0,0); }

			touch_pad_read(TOUCH_PAD_NUM2, &touch_value);
			if(touch_value<TOUCHPAD_LED_THRESHOLD_A[2]) { if(!pad_states[1]) { pad_states[1]=1; new_note(2,1,1,TOUCHPAD_LED_THRESHOLD_A[2]-touch_value); LED_1_OFF; last_touch_event++; }} else if(pad_states[1]) { pad_states[1]=0; LED_1_ON; new_note(2,1,0,0); }

			touch_pad_read(TOUCH_PAD_NUM3, &touch_value);
			if(touch_value<TOUCHPAD_LED_THRESHOLD_A[3]) { if(!pad_states[0]) { pad_states[0]=1; new_note(3,0,1,TOUCHPAD_LED_THRESHOLD_A[3]-touch_value); LED_0_OFF; last_touch_event++; }} else if(pad_states[0]) { pad_states[0]=0; LED_0_ON; new_note(3,0,0,0); }

			touch_pad_read(TOUCH_PAD_NUM4, &touch_value);
			if(touch_value<TOUCHPAD_LED_THRESHOLD_A[4]) { if(!pad_states[6]) { pad_states[6]=1; new_note(4,6,1,TOUCHPAD_LED_THRESHOLD_A[4]-touch_value); LED_6_OFF; last_touch_event++; }} else if(pad_states[6]) { pad_states[6]=0; LED_6_ON; new_note(4,6,0,0); }

			touch_pad_read(TOUCH_PAD_NUM5, &touch_value);
			if(touch_value<TOUCHPAD_LED_THRESHOLD_A[5]) { if(!pad_states[7]) { pad_states[7]=1; new_note(5,7,1,TOUCHPAD_LED_THRESHOLD_A[5]-touch_value); LED_7_OFF; last_touch_event++; }} else if(pad_states[7]) { pad_states[7]=0; LED_7_ON; new_note(5,7,0,0); }

			touch_pad_read(TOUCH_PAD_NUM6, &touch_value);
			if(touch_value<TOUCHPAD_LED_THRESHOLD_A[6]) { if(!pad_states[3]) { pad_states[3]=1; new_note(6,3,1,TOUCHPAD_LED_THRESHOLD_A[6]-touch_value); LED_3_OFF; last_touch_event++; }} else if(pad_states[3]) { pad_states[3]=0; LED_3_ON; new_note(6,3,0,0); }

			touch_pad_read(TOUCH_PAD_NUM7, &touch_value);
			if(touch_value<TOUCHPAD_LED_THRESHOLD_A[7]) { if(!pad_states[4]) { pad_states[4]=1; new_note(7,4,1,TOUCHPAD_LED_THRESHOLD_A[7]-touch_value); LED_4_OFF; last_touch_event++; }} else if(pad_states[4]) { pad_states[4]=0; LED_4_ON; new_note(7,4,0,0); }

			touch_pad_read(TOUCH_PAD_NUM8, &touch_value);
			if(touch_value<TOUCHPAD_LED_THRESHOLD_A[8]) { if(!pad_states[8]) { pad_states[8]=1; new_note(8,8,1,TOUCHPAD_LED_THRESHOLD_A[8]-touch_value); last_touch_event++; }} else if(pad_states[8]) { pad_states[8]=0; new_note(8,8,0,0); }

			touch_pad_read(TOUCH_PAD_NUM9, &touch_value);
			if(touch_value<TOUCHPAD_LED_THRESHOLD_A[9]) { if(!pad_states[5]) { pad_states[5]=1; new_note(9,5,1,TOUCHPAD_LED_THRESHOLD_A[9]-touch_value); LED_5_OFF; last_touch_event++; }} else if(pad_states[5]) { pad_states[5]=0; LED_5_ON; new_note(9,5,0,0); }
		}
		else
		{
			touch_pad_read(TOUCH_PAD_NUM0, &touch_value);
			if(touch_value<TOUCHPAD_LED_THRESHOLD_A[0]) { if(!pad_states[2]) { pad_states[2]=1; new_note(0,2,1,TOUCHPAD_LED_THRESHOLD_A[0]-touch_value); LED_2_ON; last_touch_event++; }} else if(pad_states[2]) { pad_states[2]=0; LED_2_OFF; new_note(0,2,0,0); }

			touch_pad_read(TOUCH_PAD_NUM2, &touch_value);
			if(touch_value<TOUCHPAD_LED_THRESHOLD_A[2]) { if(!pad_states[1]) { pad_states[1]=1; new_note(2,1,1,TOUCHPAD_LED_THRESHOLD_A[2]-touch_value); LED_1_ON; last_touch_event++; }} else if(pad_states[1]) { pad_states[1]=0; LED_1_OFF; new_note(2,1,0,0); }

			touch_pad_read(TOUCH_PAD_NUM3, &touch_value);
			if(touch_value<TOUCHPAD_LED_THRESHOLD_A[3]) { if(!pad_states[0]) { pad_states[0]=1; new_note(3,0,1,TOUCHPAD_LED_THRESHOLD_A[3]-touch_value); LED_0_ON; last_touch_event++; }} else if(pad_states[0]) { pad_states[0]=0; LED_0_OFF; new_note(3,0,0,0); }

			touch_pad_read(TOUCH_PAD_NUM4, &touch_value);
			if(touch_value<TOUCHPAD_LED_THRESHOLD_A[4]) { if(!pad_states[6]) { pad_states[6]=1; new_note(4,6,1,TOUCHPAD_LED_THRESHOLD_A[4]-touch_value); LED_6_ON; last_touch_event++; }} else if(pad_states[6]) { pad_states[6]=0; LED_6_OFF; new_note(4,6,0,0); }

			touch_pad_read(TOUCH_PAD_NUM5, &touch_value);
			if(touch_value<TOUCHPAD_LED_THRESHOLD_A[5]) { if(!pad_states[7]) { pad_states[7]=1; new_note(5,7,1,TOUCHPAD_LED_THRESHOLD_A[5]-touch_value); LED_7_ON; last_touch_event++; }} else if(pad_states[7]) { pad_states[7]=0; LED_7_OFF; new_note(5,7,0,0); }

			touch_pad_read(TOUCH_PAD_NUM6, &touch_value);
			if(touch_value<TOUCHPAD_LED_THRESHOLD_A[6]) { if(!pad_states[3]) { pad_states[3]=1; new_note(6,3,1,TOUCHPAD_LED_THRESHOLD_A[6]-touch_value); LED_3_ON; last_touch_event++; }} else if(pad_states[3]) { pad_states[3]=0; LED_3_OFF; new_note(6,3,0,0); }

			touch_pad_read(TOUCH_PAD_NUM7, &touch_value);
			if(touch_value<TOUCHPAD_LED_THRESHOLD_A[7]) { if(!pad_states[4]) { pad_states[4]=1; new_note(7,4,1,TOUCHPAD_LED_THRESHOLD_A[7]-touch_value); LED_4_ON; last_touch_event++; }} else if(pad_states[4]) { pad_states[4]=0; LED_4_OFF; new_note(7,4,0,0); }

			touch_pad_read(TOUCH_PAD_NUM8, &touch_value);
			if(touch_value<TOUCHPAD_LED_THRESHOLD_A[8]) { if(!pad_states[8]) { pad_states[8]=1; new_note(8,8,1,TOUCHPAD_LED_THRESHOLD_A[8]-touch_value); last_touch_event++; }} else if(pad_states[8]) { pad_states[8]=0; new_note(8,8,0,0); }

			touch_pad_read(TOUCH_PAD_NUM9, &touch_value);
			if(touch_value<TOUCHPAD_LED_THRESHOLD_A[9]) { if(!pad_states[5]) { pad_states[5]=1; new_note(9,5,1,TOUCHPAD_LED_THRESHOLD_A[9]-touch_value); LED_5_ON; last_touch_event++; }} else if(pad_states[5]) { pad_states[5]=0; LED_5_OFF; new_note(9,5,0,0); }
		}
	}
}

void touch_pad_test()
{
	esp_err_t res;
	uint16_t touch_value;//, touch_filter_value;
	//int pad_states[9] = {0,0,0,0,0,0,0,0,0};

	// Initialize touch pad peripheral.
	// The default fsm mode is software trigger mode.
	res = touch_pad_init();
	printf("touch_pad_test(): touch_pad_init returned code %d\n", res);

	// Set reference voltage for charging/discharging
	// In this case, the high reference voltage will be 2.7V - 1V = 1.7V
	// The low reference voltage will be 0.5
	// The larger the range, the larger the pulse count value.
	res = touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V);
	//res = touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V5, TOUCH_HVOLT_ATTEN_1V5);
	//res = touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V8, TOUCH_HVOLT_ATTEN_1V5);
	//res = touch_pad_set_voltage(TOUCH_HVOLT_2V7, TOUCH_LVOLT_0V8, TOUCH_HVOLT_ATTEN_0V5);
	printf("touch_pad_test(): touch_pad_set_voltage returned code %d\n", res);

	res = touch_pad_config(TOUCH_PAD_NUM0, TOUCH_THRESH_NO_USE);
	printf("touch_pad_test(): touch_pad_config returned code %d\n", res);
	res = touch_pad_config(TOUCH_PAD_NUM2, TOUCH_THRESH_NO_USE);
	printf("touch_pad_test(): touch_pad_config returned code %d\n", res);
	res = touch_pad_config(TOUCH_PAD_NUM3, TOUCH_THRESH_NO_USE);
	printf("touch_pad_test(): touch_pad_config returned code %d\n", res);
	res = touch_pad_config(TOUCH_PAD_NUM4, TOUCH_THRESH_NO_USE);
	printf("touch_pad_test(): touch_pad_config returned code %d\n", res);
	res = touch_pad_config(TOUCH_PAD_NUM5, TOUCH_THRESH_NO_USE);
	printf("touch_pad_test(): touch_pad_config returned code %d\n", res);
	res = touch_pad_config(TOUCH_PAD_NUM6, TOUCH_THRESH_NO_USE);
	printf("touch_pad_test(): touch_pad_config returned code %d\n", res);
	res = touch_pad_config(TOUCH_PAD_NUM7, TOUCH_THRESH_NO_USE);
	printf("touch_pad_test(): touch_pad_config returned code %d\n", res);
	res = touch_pad_config(TOUCH_PAD_NUM8, TOUCH_THRESH_NO_USE);
	printf("touch_pad_test(): touch_pad_config returned code %d\n", res);
	res = touch_pad_config(TOUCH_PAD_NUM9, TOUCH_THRESH_NO_USE);
	printf("touch_pad_test(): touch_pad_config returned code %d\n", res);

	/*
	for (int i = 0;i< TOUCH_PAD_MAX;i++)
	{
		touch_pad_config(i, TOUCH_THRESH_NO_USE);
	}
	*/

	//res = touch_pad_filter_start(TOUCHPAD_FILTER_TOUCH_PERIOD);
	//printf("touch_pad_test(): touch_pad_filter_start returned code %d\n", res);

	#define DEBUG_RAW_TOUCH_VALUES

	while(1)
	{
		ets_delay_us(100);

		touch_pad_read(TOUCH_PAD_NUM0, &touch_value);
		//touch_pad_read_filtered(TOUCH_PAD_NUM0, &touch_filter_value);
		#ifdef DEBUG_RAW_TOUCH_VALUES
		//printf("%d:[%4d,%4d]\t", TOUCH_PAD_NUM0, touch_value, touch_filter_value);
		printf("%d:[%4d,%4d]\t", TOUCH_PAD_NUM0, touch_value, touch_value);
		#endif

		//if(touch_value<TOUCHPAD_LED_THRESHOLD) { if(!pad_states[2]) { pad_states[2]=1; new_note(2,1,TOUCHPAD_LED_THRESHOLD-touch_value); LED_2_ON; last_touch_event++; }} else if(pad_states[2]) { pad_states[2]=0; LED_2_OFF; new_note(2,0,0); }

		touch_pad_read(TOUCH_PAD_NUM2, &touch_value);
		//touch_pad_read_filtered(TOUCH_PAD_NUM2, &touch_filter_value);
		#ifdef DEBUG_RAW_TOUCH_VALUES
		//printf("%d:[%4d,%4d]\t", TOUCH_PAD_NUM2, touch_value, touch_filter_value);
		printf("%d:[%4d,%4d]\t", TOUCH_PAD_NUM2, touch_value, touch_value);
		#endif

		//if(touch_value<TOUCHPAD_LED_THRESHOLD) { if(!pad_states[1]) { pad_states[1]=1; new_note(1,1,TOUCHPAD_LED_THRESHOLD-touch_value); LED_1_ON; last_touch_event++; }} else if(pad_states[1]) { pad_states[1]=0; LED_1_OFF; new_note(1,0,0); }

		touch_pad_read(TOUCH_PAD_NUM3, &touch_value);
		//touch_pad_read_filtered(TOUCH_PAD_NUM3, &touch_filter_value);
		#ifdef DEBUG_RAW_TOUCH_VALUES
		//printf("%d:[%4d,%4d]\t", TOUCH_PAD_NUM3, touch_value, touch_filter_value);
		printf("%d:[%4d,%4d]\t", TOUCH_PAD_NUM3, touch_value, touch_value);
		#endif

		//if(touch_value<TOUCHPAD_LED_THRESHOLD) { if(!pad_states[0]) { pad_states[0]=1; new_note(0,1,TOUCHPAD_LED_THRESHOLD-touch_value); LED_0_ON; last_touch_event++; }} else if(pad_states[0]) { pad_states[0]=0; LED_0_OFF; new_note(0,0,0); }

		touch_pad_read(TOUCH_PAD_NUM4, &touch_value);
		//touch_pad_read_filtered(TOUCH_PAD_NUM4, &touch_filter_value);
		#ifdef DEBUG_RAW_TOUCH_VALUES
		//printf("%d:[%4d,%4d]\t", TOUCH_PAD_NUM4, touch_value, touch_filter_value);
		printf("%d:[%4d,%4d]\t", TOUCH_PAD_NUM4, touch_value, touch_value);
		#endif

		//if(touch_value<TOUCHPAD_LED_THRESHOLD) { if(!pad_states[6]) { pad_states[6]=1; new_note(6,1,TOUCHPAD_LED_THRESHOLD-touch_value); LED_6_ON; last_touch_event++; }} else if(pad_states[6]) { pad_states[6]=0; LED_6_OFF; new_note(6,0,0); }

		touch_pad_read(TOUCH_PAD_NUM5, &touch_value);
		//touch_pad_read_filtered(TOUCH_PAD_NUM5, &touch_filter_value);
		#ifdef DEBUG_RAW_TOUCH_VALUES
		//printf("%d:[%4d,%4d]\t", TOUCH_PAD_NUM5, touch_value, touch_filter_value);
		printf("%d:[%4d,%4d]\t", TOUCH_PAD_NUM5, touch_value, touch_value);
		#endif

		//if(touch_value<TOUCHPAD_LED_THRESHOLD) { if(!pad_states[7]) { pad_states[7]=1; new_note(7,1,TOUCHPAD_LED_THRESHOLD-touch_value); LED_7_ON; last_touch_event++; }} else if(pad_states[7]) { pad_states[7]=0; LED_7_OFF; new_note(7,0,0); }

		touch_pad_read(TOUCH_PAD_NUM6, &touch_value);
		//touch_pad_read_filtered(TOUCH_PAD_NUM6, &touch_filter_value);
		#ifdef DEBUG_RAW_TOUCH_VALUES
		//printf("%d:[%4d,%4d]\t", TOUCH_PAD_NUM6, touch_value, touch_filter_value);
		printf("%d:[%4d,%4d]\t", TOUCH_PAD_NUM6, touch_value, touch_value);
		#endif

		//if(touch_value<TOUCHPAD_LED_THRESHOLD) { if(!pad_states[3]) { pad_states[3]=1; new_note(3,1,TOUCHPAD_LED_THRESHOLD-touch_value); LED_3_ON; last_touch_event++; }} else if(pad_states[3]) { pad_states[3]=0; LED_3_OFF; new_note(3,0,0); }

		touch_pad_read(TOUCH_PAD_NUM7, &touch_value);
		//touch_pad_read_filtered(TOUCH_PAD_NUM7, &touch_filter_value);
		#ifdef DEBUG_RAW_TOUCH_VALUES
		//printf("%d:[%4d,%4d]\t", TOUCH_PAD_NUM7, touch_value, touch_filter_value);
		printf("%d:[%4d,%4d]\t", TOUCH_PAD_NUM7, touch_value, touch_value);
		#endif

		//if(touch_value<TOUCHPAD_LED_THRESHOLD) { if(!pad_states[4]) { pad_states[4]=1; new_note(4,1,TOUCHPAD_LED_THRESHOLD-touch_value); LED_4_ON; last_touch_event++; }} else if(pad_states[4]) { pad_states[4]=0; LED_4_OFF; new_note(4,0,0); }

		touch_pad_read(TOUCH_PAD_NUM8, &touch_value);
		//touch_pad_read_filtered(TOUCH_PAD_NUM8, &touch_filter_value);
		#ifdef DEBUG_RAW_TOUCH_VALUES
		//printf("%d:[%4d,%4d]\t", TOUCH_PAD_NUM8, touch_value, touch_filter_value);
		printf("%d:[%4d,%4d]\t", TOUCH_PAD_NUM8, touch_value, touch_value);
		#endif

		//if(touch_value<TOUCHPAD_LED_THRESHOLD_CENTER) { if(!pad_states[8]) { pad_states[8]=1; new_note(8,1,TOUCHPAD_LED_THRESHOLD_CENTER-touch_value); last_touch_event++; }} else if(pad_states[8]) { pad_states[8]=0; new_note(8,0,0); }

		touch_pad_read(TOUCH_PAD_NUM9, &touch_value);
		//touch_pad_read_filtered(TOUCH_PAD_NUM9, &touch_filter_value);
		#ifdef DEBUG_RAW_TOUCH_VALUES
		//printf("%d:[%4d,%4d]\n", TOUCH_PAD_NUM9, touch_value, touch_filter_value);
		printf("%d:[%4d,%4d]\n", TOUCH_PAD_NUM9, touch_value, touch_value);
		#endif

		//if(touch_value<TOUCHPAD_LED_THRESHOLD) { if(!pad_states[5]) { pad_states[5]=1; new_note(5,1,TOUCHPAD_LED_THRESHOLD-touch_value); LED_5_ON; last_touch_event++; }} else if(pad_states[5]) { pad_states[5]=0; LED_5_OFF; new_note(5,0,0); }

		if(touch_value>1150) LEDS_ALL_OFF;
		if(touch_value<1150) LED_0_ON;
		if(touch_value<1000) LED_1_ON;
		if(touch_value<850) LED_2_ON;
		if(touch_value<700) LED_3_ON;
		if(touch_value<550) LED_4_ON;
		if(touch_value<400) LED_5_ON;
		if(touch_value<250) LED_6_ON;
		if(touch_value<100) LED_7_ON;

		/*
		for (int i = 0; i < TOUCH_PAD_MAX; i++)
		{
			 touch_pad_read_filtered(i, &touch_filter_value);
			 touch_pad_read(i, &touch_value);
			 printf("T%d:[%4d,%4d] ", i, touch_value, touch_filter_value);
		}
		printf("\n");
		*/

		//Delay(1);
	}
}

uint16_t accelerometer_read_Z()
{
	int16_t acc_val;
	while(i2c_bus_mutex)
	{
		printf("accelerometer_read_Z(): waiting for I2C\n");
		Delay(1);
	}
	i2c_bus_mutex = 1;

	//acc_val = WMA6981_read_register(0x05);
	//printf("reg[5]=%d	", acc_val);
	//acc_val = WMA6981_read_register(0x06);
	//printf("reg[6]=%d\n", acc_val);

	#define ACC_REG_Z	0x05

    //uint8_t val1,val2;
    //esp_err_t result;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    /*result =*/ i2c_master_write_byte(cmd, WMA6981_I2C_ADDRESS << 1 | WRITE_BIT, ACK_CHECK_EN);
    //printf("WMA6981_read_register(): result = %d\n", result);
    /*result =*/ i2c_master_write_byte(cmd, ACC_REG_Z, ACK_CHECK_EN);
    //printf("WMA6981_read_register(): result = %d\n", result);

    i2c_master_start(cmd);
    /*result =*/ i2c_master_write_byte(cmd, WMA6981_I2C_ADDRESS << 1 | READ_BIT, ACK_CHECK_EN);
    //printf("WMA6981_read_register(): result = %d\n", result);
    /*result =*/ //i2c_master_read_byte(cmd, &val, I2C_MASTER_NACK);
    //printf("WMA6981_read_register(): result = %d\n", result);
    i2c_master_read(cmd, (unsigned char*)&acc_val, 2, I2C_MASTER_LAST_NACK);

    i2c_master_stop(cmd);
    /*result =*/ i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    //printf("WMA6981_read_register(): result = %d\n", result);

    i2c_cmd_link_delete(cmd);

	i2c_bus_mutex = 0;

	printf("accelerometer_read_Z(): regs 5,6: %d\n", acc_val);

	return acc_val;
}

#ifdef ANALYSE_THRESHOLDS
void print_thresholds_info(int update)
{
	char *old_info;
	int info_length = nvs_get_text("pad_stats", &old_info);
	printf("print_thresholds_info(): nvs_get_text() returned lenght = %d\n", info_length);

	if(info_length)
	{
		printf("PREVIOUS STATE:\n");
		printf("%s", old_info);
		free(old_info);
	}

	char info[500];
	info[0]=0;

	for(int i=0;i<10;i++)
	{
		if(i!=1)
		{
			sprintf(info+strlen(info), "pad[%d]: max = %d, thr = %d,%d,%d\n", i, TOUCHPAD_MAX_LEVEL_A[i], TOUCHPAD_LED_THRESHOLD_A[i], SPLIT_TIMBRE_NOTE_THRESHOLD1_A[i], SPLIT_TIMBRE_NOTE_THRESHOLD2_A[i]);
		}
	}
	printf("CURRENT STATE:\n");
	printf("%s", info);

	if(update)
	{
		nvs_set_text("pad_stats", info);

		for(int i=0; i<5; i++)
		{
			LEDS_ALL_ON;
			Delay(80);
			LEDS_ALL_OFF;
			Delay(80);
		}
	}
}
#endif
