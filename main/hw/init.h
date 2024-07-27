/*
 * init.h
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

#ifndef INIT_H_
#define INIT_H_

//#define ANALYSE_THRESHOLDS

//#define DYNAMICS_BY_ADC
#define NEW_NOTE_MIN_VELOCITY			64

//#include <stdint.h>
#include <string.h>

#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
//#include "esp32/include/esp_event.h"
#include "esp_event_legacy.h"
#include "esp_attr.h"
#include "esp_sleep.h"
//#include "esp_event/include/esp_event_loop.h"
#include "esp_task_wdt.h"
#include "bootloader_random.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/i2s.h"
#include "driver/mcpwm.h"
#include "driver/ledc.h"
#include "driver/i2c.h"
#include "driver/adc.h"
#include "esp_spi_flash.h"
#include "driver/uart.h"
#include "esp_heap_caps_init.h"
#include "driver/dac.h"
#include "esp32/rom/rtc.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/io_mux_reg.h"
#include "driver/rtc_cntl.h"
//#include "esp32/include/esp_brownout.h"
#include "esp_intr_alloc.h"
#include "driver/touch_sensor.h"
//#include "driver/touch_sensor_common.h"
#include "freertos/FreeRTOSConfig.h"

#include "board.h"

#define I2S_NUM         I2S_NUM_0

extern size_t i2s_bytes_rw;

//#define MCLK_GPIO 1
#define USE_APLL
#define CODEC_RST_PIN GPIO_NUM_23 //32 //works: 0,2,4,18,19,21,22, doesnt: 3 (RX), 15

extern i2c_port_t i2c_num;

#define I2C_MASTER_SCL_IO          GPIO_NUM_22	//gpio number for I2C master clock
#define I2C_MASTER_SDA_IO          GPIO_NUM_21	//gpio number for I2C master data
#define I2C_MASTER_NUM             I2C_NUM_1	//I2C port number for master dev
#define I2C_MASTER_TX_BUF_DISABLE  0			//I2C master do not need buffer
#define I2C_MASTER_RX_BUF_DISABLE  0			//I2C master do not need buffer
#define I2C_MASTER_FREQ_HZ         400000		//I2C master clock frequency

//overdrive I2C: 700k works for codec too, 750k not...
//#define I2C_MASTER_FREQ_HZ_FAST         650000	//I2C master clock frequency
#define I2C_MASTER_FREQ_HZ_FAST         1000000		//I2C master clock frequency

#define CODEC_I2C_ADDRESS                  0x18		//slave address for TLV320AIC3104

#define ESP_SLAVE_ADDR                     0x28             //ESP32 slave address, you can set any 7bit value
#define WRITE_BIT                          I2C_MASTER_WRITE //I2C master write
#define READ_BIT                           I2C_MASTER_READ  //I2C master read
#define ACK_CHECK_EN                       0x1              //I2C master will check ack from slave
#define ACK_CHECK_DIS                      0x0              //I2C master will not check ack from slave
#define ACK_VAL                            0x0              //I2C ack value
#define NACK_VAL                           0x1              //I2C nack value

#define MIDI_UART UART_NUM_2

#define TWDT_TIMEOUT_S						5
#define TASK_RESET_PERIOD_S					4

//extern int16_t ms10_counter;
extern int auto_power_off, auto_power_off_canceled;
extern int last_button_event, last_touch_event;

//#define AUTO_POWER_OFF_TIMEOUT		120*5	//5 minutes (500ms intervals)
#define AUTO_POWER_OFF_VOLUME_RAMP	60		//at 500ms check interval it takes 30 seconds to lower down the volume

#define TASK_PRIORITY_ACCELEROMETER	10//8
#define TASK_PRIORITY_TOUCH_PADS	10//9
#define TASK_PRIORITY_BUTTONS		10
#define TASK_PRIORITY_UI_EVENTS		10//11

extern int touch_pad_enabled;

extern int i2c_driver_installed;
extern int i2c_bus_mutex;
extern uint32_t sample32;
extern int glo_run;
extern int touch_pad_calibrated;
extern int init_free_mem;

extern int TOUCHPAD_LED_THRESHOLD_A[];
extern int SPLIT_TIMBRE_NOTE_THRESHOLD1_A[], SPLIT_TIMBRE_NOTE_THRESHOLD2_A[];
extern int TOUCHPAD_MAX_LEVEL_A[];

extern int new_MIDI_note[9];
extern int new_notes_timbre[9];
extern int new_touch_event[9];
extern int note_updated, note_updated_pad, notes_on;
extern int note_map[9];
extern int note_map_reverse_center_last[9];

extern int current_patch, *patch_notes;
extern int *midi_scale_selected;
extern float *tuning_coeffs;
extern float *micro_tuning, *micro_tuning_selected;

extern int **notes_reverb;

extern int *midi_scale_clipboard;
extern float *micro_tuning_clipboard;

/*
 * Macro to check the outputs of TWDT functions and trigger an abort if an
 * incorrect code is returned.
 */
#define CHECK_ERROR_CODE(returned, expected) ({                        \
            if(returned != expected){                                  \
                printf("TWDT ERROR\n");                                \
                abort();                                               \
            }                                                          \
})

//-------------------------------------------------------

#define MIDI_OUT_SIGNAL		GPIO_NUM_19
#define MIDI_IN_SIGNAL		UART_PIN_NO_CHANGE //GPIO_NUM_18

//------------------------------------------

#define ACC_ORIENTATION_XYZ		0
#define ACC_ORIENTATION_XZY		1
#define ACC_ORIENTATION_ZYX		2
#define ACC_ORIENTATION_YXZ		3
#define ACC_ORIENTATION_ZXY		4
#define ACC_ORIENTATION_YZX		5
#define ACC_ORIENTATION_MODES	6

#define ACC_ORIENTATION_DEFAULT	ACC_ORIENTATION_XYZ
extern const uint8_t acc_orientation_indication[ACC_ORIENTATION_MODES];

#define ACC_INVERT_PPP			0	//+x+y+z
#define ACC_INVERT_PPM			1	//-x+y+z
#define ACC_INVERT_PMP			2	//+x-y+z
#define ACC_INVERT_PMM			3	//-x-y+z
#define ACC_INVERT_MPP			4	//+x+y-z
#define ACC_INVERT_MPM			5	//-x+y-z
#define ACC_INVERT_MMP			6	//+x-y-z
#define ACC_INVERT_MMM			7	//-x-y-z
#define ACC_INVERT_MODES		8

#define ACC_INVERT_DEFAULT		ACC_INVERT_PPP
extern const uint8_t acc_invert_indication[ACC_INVERT_MODES];

//------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

void init_i2s_and_gpio(int buf_count, int buf_len, int sample_rate);
void init_deinit_TWDT();

float micros();
uint32_t micros_i();
uint32_t millis();

void enable_I2S_MCLK_clock();
void disable_I2S_MCLK_clock();

void i2c_master_init(int speed);
void i2c_master_deinit();
void i2c_scan_for_devices(int print, uint8_t *addresses, uint8_t *found);
int i2c_codec_two_byte_command(uint8_t b1, uint8_t b2);
uint8_t i2c_codec_register_read(uint8_t reg);
int i2c_bus_write(int addr_rw, unsigned char *data, int len);
int i2c_bus_read(int addr_rw, unsigned char *buf, int buf_len);

uint16_t hardware_check();

void generate_random_seed();

unsigned char byte_bit_reverse(unsigned char b);

void drum_init_MIDI(int uart_num, int midi_out_enabled);
void drum_deinit_MIDI();
void drum_LED_expander_test();
void drum_LED_expanders_update(int quadrant);
void drum_LED_expanders_set_blink_rates();

void power_on_animation();
void power_on_animation_reversed(int start_from);
int power_off_animation(int auto_shutdown, int delay);

void drum_restart();
void brownout_init();

void free_memory_info();

void new_note(int pad, int note, int status, int level0);
void touch_pad_process(void *pvParameters);
void touch_pad_test();

uint16_t accelerometer_read_Z();
void print_thresholds_info(int update);

#ifdef __cplusplus
}
#endif

#endif
