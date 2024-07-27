/*
 * gpio.c
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

#include <init.h>

#include <hw/codec.h>
#include <hw/gpio.h>
#include <string.h>

uint8_t LED_map[4] = {0x55,0x55,0x55,0x55};	/* two bits per LED:
												00 = output is set LOW (LED on)
												01 = output is set high-impedance (LED off; default)
												10 = output blinks at PWM0 rate
												11 = output blinks at PWM1 rate */
void Delay(int d)
{
	vTaskDelay(d / portTICK_RATE_MS);
}

void drum_init_power()
{
	gpio_set_direction(PWR_HOLD_SIGNAL, GPIO_MODE_OUTPUT);
	//if(rtc_get_reset_reason(0) == POWERON_RESET)
    //{
    	gpio_set_level(PWR_HOLD_SIGNAL,1); //hold the power on
    	brownout_init();
    //}
    //else
    //{
    //	gpio_set_level(PWR_HOLD_SIGNAL,0); //power off
    //	while(1);
    //}
}

void drum_init_buttons_GPIO()
{
	int result;

	//these are only GPI
	result = gpio_set_direction(BUTTON_U1_PIN, GPIO_MODE_INPUT);
	//printf("GPI%d direction set result = %d\n",BUTTON_U1_PIN,result);
	result = gpio_set_direction(BUTTON_U25_PIN, GPIO_MODE_INPUT);
	//printf("GPI%d direction set result = %d\n",BUTTON_U25_PIN,result);
	result = gpio_set_direction(BUTTON_U34_PIN, GPIO_MODE_INPUT);
	//printf("GPI%d direction set result = %d\n",BUTTON_U34_PIN,result);

	//these are actually GPIO
	result = gpio_set_direction(BUTTON_U42_PIN, GPIO_MODE_INPUT);
	//printf("GPIO%d direction set result = %d\n",BUTTON_U42_PIN,result);
	result = gpio_set_pull_mode(BUTTON_U42_PIN, GPIO_PULLUP_ONLY);
	//printf("GPIO%d pull mode set result = %d\n",BUTTON_U42_PIN,result);
	result = gpio_set_direction(BUTTON_U53_PIN, GPIO_MODE_INPUT);
	//printf("GPIO%d direction set result = %d\n",BUTTON_U53_PIN,result);
	result = gpio_set_pull_mode(BUTTON_U53_PIN, GPIO_PULLUP_ONLY);
	//printf("GPIO%d pull mode set result = %d\n",BUTTON_U53_PIN,result);
}

void drum_shutdown()
{
	printf("drum_shutdown(): Shutting down...\n");
	codec_set_mute(1); //mute the codec
	Delay(1);
	codec_reset();
	Delay(1);
	gpio_set_level(PWR_HOLD_SIGNAL,0); //power off

	while(1);
}

void drum_test_buttons()
{
	int val;
	printf("drum_test_buttons()\n");
	while(1)
	{
		val = gpio_get_level(BUTTON_U1_PIN);
		printf("(+)PWR=%d,",val);
		val = gpio_get_level(BUTTON_U25_PIN);
		printf("(-)METAL=%d,",val);
		val = gpio_get_level(BUTTON_U34_PIN);
		printf("(+)WOOD=%d,",val);
		val = gpio_get_level(BUTTON_U42_PIN);
		printf("(-)MINUS=%d,",val);
		val = gpio_get_level(BUTTON_U53_PIN);
		printf("(-)PLUS=%d\n",val);
		Delay(100);
	}
}

void error_blink(int pattern, int delay)
{
	if(pattern == ERROR_BLINK_PATTERN_1357_2468)
	{
		while(1)
		{
			LEDS_ALL_OFF;
			LED_0_ON;LED_2_ON;LED_4_ON;LED_6_ON;
			vTaskDelay(delay / portTICK_PERIOD_MS);
			LEDS_ALL_OFF;
			LED_1_ON;LED_3_ON;LED_5_ON;LED_7_ON;
			vTaskDelay(delay / portTICK_PERIOD_MS);
		}
	}
	if(pattern == ERROR_BLINK_PATTERN_1256_3478)
	{
		while(1)
		{
			LEDS_ALL_OFF;
			LED_0_ON;LED_1_ON;LED_4_ON;LED_5_ON;
			vTaskDelay(delay / portTICK_PERIOD_MS);
			LEDS_ALL_OFF;
			LED_2_ON;LED_3_ON;LED_6_ON;LED_7_ON;
			vTaskDelay(delay / portTICK_PERIOD_MS);
		}
	}
}

void LED_X_ON(int x)
{
	if(x==0)LED_0_ON
	else if(x==1)LED_1_ON
	else if(x==2)LED_2_ON
	else if(x==3)LED_3_ON
	else if(x==4)LED_4_ON
	else if(x==5)LED_5_ON
	else if(x==6)LED_6_ON
	else if(x==7)LED_7_ON
}

void LED_X_OFF(int x)
{
	if(x==0)LED_0_OFF
	else if(x==1)LED_1_OFF
	else if(x==2)LED_2_OFF
	else if(x==3)LED_3_OFF
	else if(x==4)LED_4_OFF
	else if(x==5)LED_5_OFF
	else if(x==6)LED_6_OFF
	else if(x==7)LED_7_OFF
}

void LED_X_B1(int x)
{
	if(x==0){LED_0_ON_NOUPDATE;LED_0_B1;}
	else if(x==1){LED_1_ON_NOUPDATE;LED_1_B1;}
	else if(x==2){LED_2_ON_NOUPDATE;LED_2_B1;}
	else if(x==3){LED_3_ON_NOUPDATE;LED_3_B1;}
	else if(x==4){LED_4_ON_NOUPDATE;LED_4_B1;}
	else if(x==5){LED_5_ON_NOUPDATE;LED_5_B1;}
	else if(x==6){LED_6_ON_NOUPDATE;LED_6_B1;}
	else if(x==7){LED_7_ON_NOUPDATE;LED_7_B1;}
}

void LED_X_B2(int x)
{
	if(x==0)LED_0_B2
	else if(x==1)LED_1_B2
	else if(x==2)LED_2_B2
	else if(x==3)LED_3_B2
	else if(x==4)LED_4_B2
	else if(x==5)LED_5_B2
	else if(x==6)LED_6_B2
	else if(x==7)LED_7_B2
}

void LED_X_ON_NOUPDATE(int x)
{
	if(x==0)LED_0_ON_NOUPDATE
	else if(x==1)LED_1_ON_NOUPDATE
	else if(x==2)LED_2_ON_NOUPDATE
	else if(x==3)LED_3_ON_NOUPDATE
	else if(x==4)LED_4_ON_NOUPDATE
	else if(x==5)LED_5_ON_NOUPDATE
	else if(x==6)LED_6_ON_NOUPDATE
	else if(x==7)LED_7_ON_NOUPDATE
}
