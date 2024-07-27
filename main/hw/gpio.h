/*
 * gpio.h
 *
 *  Copyright 2024 Phonicbloom Ltd.
 *
 *  Created on: May 30, 2020
 *      Author: mario
 *
 *  This file can be used within the terms of GNU GPLv3 license: https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 *  Find more information at:
 *  http://phonicbloom.com/
 *  http://gechologic.com/
 *
 */

#ifndef GPIO_H_
#define GPIO_H_

#include <stdbool.h>
#include "driver/gpio.h"
#include "board.h"

//esp-32

#ifdef BOARD_WINGDRUM

#define BUTTON_U1_PIN		GPIO_NUM_35 //PWR button - scale
#define BUTTON_U25_PIN		GPIO_NUM_36 //BOOT button - metal
#define BUTTON_U34_PIN		GPIO_NUM_39 //RST button - wood
#define BUTTON_U42_PIN		GPIO_NUM_25	//minus
#define BUTTON_U53_PIN		GPIO_NUM_26	//plus

#define BUTTON_U1_ON		(gpio_get_level(BUTTON_U1_PIN)==1)	//BT1 (power / scale) connects to VDD when pressed
#define BUTTON_U5_ON		(gpio_get_level(BUTTON_U25_PIN)==0)	//BT5 (metal) connects to GND when pressed
#define BUTTON_U4_ON		(gpio_get_level(BUTTON_U34_PIN)==1)	//BT4 (wood) connects to VDD when pressed
#define BUTTON_U2_ON		(gpio_get_level(BUTTON_U42_PIN)==0)	//BT2 (minus) connects to GND when pressed
#define BUTTON_U3_ON		(gpio_get_level(BUTTON_U53_PIN)==0)	//BT3 (plus) connects to GND when pressed

#define ANY_BUTTON_ON		(BUTTON_U1_ON || BUTTON_U2_ON || BUTTON_U3_ON || BUTTON_U4_ON || BUTTON_U5_ON)
#define NO_BUTTON_ON		(!ANY_BUTTON_ON)
#define BUTTON_ON(x)		(x==1?BUTTON_U1_ON:(x==2?BUTTON_U2_ON:(x==3?BUTTON_U3_ON:(x==4?BUTTON_U4_ON:(x==5?BUTTON_U5_ON:0)))))

#define LEDS_ALL_OFF		{LED_map[DRUM_LED_QUADRANT_1]=0x55;drum_LED_expanders_update(DRUM_LED_QUADRANT_1);LED_map[DRUM_LED_QUADRANT_2]=0x55;drum_LED_expanders_update(DRUM_LED_QUADRANT_2);}
#define LEDS_ALL_ON			{LED_map[DRUM_LED_QUADRANT_1]=0x00;drum_LED_expanders_update(DRUM_LED_QUADRANT_1);LED_map[DRUM_LED_QUADRANT_2]=0x00;drum_LED_expanders_update(DRUM_LED_QUADRANT_2);}
#define LEDS_ALL_B1			{LED_map[DRUM_LED_QUADRANT_1]=0xaa;drum_LED_expanders_update(DRUM_LED_QUADRANT_1);LED_map[DRUM_LED_QUADRANT_2]=0xaa;drum_LED_expanders_update(DRUM_LED_QUADRANT_2);}
#define LEDS_ALL_B2			{LED_map[DRUM_LED_QUADRANT_1]=0xff;drum_LED_expanders_update(DRUM_LED_QUADRANT_1);LED_map[DRUM_LED_QUADRANT_2]=0xff;drum_LED_expanders_update(DRUM_LED_QUADRANT_2);}

#define PWR_HOLD_SIGNAL	GPIO_NUM_16

#define LED_REGS	0x06
#define DRUM_LED_QUADRANT_1	2	//LED8 to LED11 at PCA9552
#define DRUM_LED_QUADRANT_2	3	//LED12 to LED15 at PCA9552
#define EXPANDERS_TIMING_DELAY 10 //timeout for I2C bus mutex

extern uint8_t LED_map[4];

#define LED_0_ON	{LED_map[DRUM_LED_QUADRANT_1]&=0xfc;drum_LED_expanders_update(DRUM_LED_QUADRANT_1);}
#define LED_0_OFF	{LED_map[DRUM_LED_QUADRANT_1]|=0x01;drum_LED_expanders_update(DRUM_LED_QUADRANT_1);}
#define LED_1_ON	{LED_map[DRUM_LED_QUADRANT_1]&=0xf3;drum_LED_expanders_update(DRUM_LED_QUADRANT_1);}
#define LED_1_OFF	{LED_map[DRUM_LED_QUADRANT_1]|=0x04;drum_LED_expanders_update(DRUM_LED_QUADRANT_1);}
#define LED_2_ON	{LED_map[DRUM_LED_QUADRANT_1]&=0xcf;drum_LED_expanders_update(DRUM_LED_QUADRANT_1);}
#define LED_2_OFF	{LED_map[DRUM_LED_QUADRANT_1]|=0x10;drum_LED_expanders_update(DRUM_LED_QUADRANT_1);}
#define LED_3_ON	{LED_map[DRUM_LED_QUADRANT_1]&=0x3f;drum_LED_expanders_update(DRUM_LED_QUADRANT_1);}
#define LED_3_OFF	{LED_map[DRUM_LED_QUADRANT_1]|=0x40;drum_LED_expanders_update(DRUM_LED_QUADRANT_1);}

#define LED_4_ON	{LED_map[DRUM_LED_QUADRANT_2]&=0xfc;drum_LED_expanders_update(DRUM_LED_QUADRANT_2);}
#define LED_4_OFF	{LED_map[DRUM_LED_QUADRANT_2]|=0x01;drum_LED_expanders_update(DRUM_LED_QUADRANT_2);}
#define LED_5_ON	{LED_map[DRUM_LED_QUADRANT_2]&=0xf3;drum_LED_expanders_update(DRUM_LED_QUADRANT_2);}
#define LED_5_OFF	{LED_map[DRUM_LED_QUADRANT_2]|=0x04;drum_LED_expanders_update(DRUM_LED_QUADRANT_2);}
#define LED_6_ON	{LED_map[DRUM_LED_QUADRANT_2]&=0xcf;drum_LED_expanders_update(DRUM_LED_QUADRANT_2);}
#define LED_6_OFF	{LED_map[DRUM_LED_QUADRANT_2]|=0x10;drum_LED_expanders_update(DRUM_LED_QUADRANT_2);}
#define LED_7_ON	{LED_map[DRUM_LED_QUADRANT_2]&=0x3f;drum_LED_expanders_update(DRUM_LED_QUADRANT_2);}
#define LED_7_OFF	{LED_map[DRUM_LED_QUADRANT_2]|=0x40;drum_LED_expanders_update(DRUM_LED_QUADRANT_2);}

//set LED on without updating the registers (to be used before setting to B1, or from B2 to OFF)
#define LED_0_ON_NOUPDATE	{LED_map[DRUM_LED_QUADRANT_1]&=0xfc;}
#define LED_1_ON_NOUPDATE	{LED_map[DRUM_LED_QUADRANT_1]&=0xf3;}
#define LED_2_ON_NOUPDATE	{LED_map[DRUM_LED_QUADRANT_1]&=0xcf;}
#define LED_3_ON_NOUPDATE	{LED_map[DRUM_LED_QUADRANT_1]&=0x3f;}
#define LED_4_ON_NOUPDATE	{LED_map[DRUM_LED_QUADRANT_2]&=0xfc;}
#define LED_5_ON_NOUPDATE	{LED_map[DRUM_LED_QUADRANT_2]&=0xf3;}
#define LED_6_ON_NOUPDATE	{LED_map[DRUM_LED_QUADRANT_2]&=0xcf;}
#define LED_7_ON_NOUPDATE	{LED_map[DRUM_LED_QUADRANT_2]&=0x3f;}

//before setting to B1 (blink mode PWM0), need to set ON
#define LED_0_B1	{LED_map[DRUM_LED_QUADRANT_1]|=0x02;drum_LED_expanders_update(DRUM_LED_QUADRANT_1);}
#define LED_0_B2	{LED_map[DRUM_LED_QUADRANT_1]|=0x03;drum_LED_expanders_update(DRUM_LED_QUADRANT_1);}
#define LED_1_B1	{LED_map[DRUM_LED_QUADRANT_1]|=0x08;drum_LED_expanders_update(DRUM_LED_QUADRANT_1);}
#define LED_1_B2	{LED_map[DRUM_LED_QUADRANT_1]|=0x0c;drum_LED_expanders_update(DRUM_LED_QUADRANT_1);}
#define LED_2_B1	{LED_map[DRUM_LED_QUADRANT_1]|=0x20;drum_LED_expanders_update(DRUM_LED_QUADRANT_1);}
#define LED_2_B2	{LED_map[DRUM_LED_QUADRANT_1]|=0x30;drum_LED_expanders_update(DRUM_LED_QUADRANT_1);}
#define LED_3_B1	{LED_map[DRUM_LED_QUADRANT_1]|=0x80;drum_LED_expanders_update(DRUM_LED_QUADRANT_1);}
#define LED_3_B2	{LED_map[DRUM_LED_QUADRANT_1]|=0xc0;drum_LED_expanders_update(DRUM_LED_QUADRANT_1);}

#define LED_4_B1	{LED_map[DRUM_LED_QUADRANT_2]|=0x02;drum_LED_expanders_update(DRUM_LED_QUADRANT_2);}
#define LED_4_B2	{LED_map[DRUM_LED_QUADRANT_2]|=0x03;drum_LED_expanders_update(DRUM_LED_QUADRANT_2);}
#define LED_5_B1	{LED_map[DRUM_LED_QUADRANT_2]|=0x08;drum_LED_expanders_update(DRUM_LED_QUADRANT_2);}
#define LED_5_B2	{LED_map[DRUM_LED_QUADRANT_2]|=0x0c;drum_LED_expanders_update(DRUM_LED_QUADRANT_2);}
#define LED_6_B1	{LED_map[DRUM_LED_QUADRANT_2]|=0x20;drum_LED_expanders_update(DRUM_LED_QUADRANT_2);}
#define LED_6_B2	{LED_map[DRUM_LED_QUADRANT_2]|=0x30;drum_LED_expanders_update(DRUM_LED_QUADRANT_2);}
#define LED_7_B1	{LED_map[DRUM_LED_QUADRANT_2]|=0x80;drum_LED_expanders_update(DRUM_LED_QUADRANT_2);}
#define LED_7_B2	{LED_map[DRUM_LED_QUADRANT_2]|=0xc0;drum_LED_expanders_update(DRUM_LED_QUADRANT_2);}

#define LED_TEST_DELAY 		300

#define LED_PWR_ON_DELAY 		100
#define LED_PWR_OFF_DELAY 		250
#define LED_ERASE_DATA_DELAY 	150

#define ERROR_BLINK_PATTERN_1357_2468	1
#define ERROR_BLINK_PATTERN_1256_3478	2

#endif

//#define MIDI_SIGNAL_HW_TEST

#ifdef __cplusplus
 extern "C" {
#endif

/* Exported functions ------------------------------------------------------- */

void Delay(int d);

void drum_init_buttons_GPIO();

void drum_init_power();
void drum_shutdown();

void drum_test_buttons();

void error_blink(int pattern, int delay);

void LED_X_ON(int x);
void LED_X_OFF(int x);
void LED_X_B1(int x);
void LED_X_B2(int x);
void LED_X_ON_NOUPDATE(int x);

#ifdef __cplusplus
}
#endif

#endif /* GPIO_H_ */
