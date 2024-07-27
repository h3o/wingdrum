/*
 * ui.h
 *
 *  Copyright 2024 Phonicbloom Ltd.
 *
 *  Created on: Nov 5, 2018
 *      Author: mario
 *
 *  This file can be used within the terms of GNU GPLv3 license: https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 *  Find more information at:
 *  http://phonicbloom.com/
 *  http://gechologic.com/
 *
 */

#ifndef UI_H_
#define UI_H_

#include "glo_config.h"
#include "board.h"

#define EVENT_NEXT_CHANNEL_METAL	101
#define EVENT_NEXT_CHANNEL_WOOD		102
#define EVENT_NEXT_CHANNEL_BOTH		103
#define EVENT_NEXT_CHANNEL_PWR_OFF	109

//extern int select_channel_RDY_idle_blink;
extern int event_next_channel, event_next_channel0;
//extern int event_channel_options;
//extern int settings_menu_active;
//extern int menu_indicator_active;
//extern int context_menu_active;
//extern int last_settings_level;
//extern int default_setting_cycle;
//extern int ui_command;
//extern int ui_ignore_events;
//extern int ui_button3_enabled;
//extern int ui_button4_enabled;
//extern int context_menu_enabled;

extern int scale_settings, delay_settings, adjusting_note;
extern int *selected_scale;
extern int previous_scale, patch_reverb_n;
extern int led_blink_stop, led_blink_timeout;
extern int patches_found_metal, patches_found_wood;

#define LED_BLINK_TIMEOUT_SCALE_CHANGE	4	//100ms resolution => 0.4s
#define LED_ON_TIMEOUT_DELAY_CHANGE		3	//100ms resolution => 0.3s

#define NOTES_PER_SCALE		9
#define SCALES_PER_PATCH	8

#define SCALE_SETTINGS_LEVEL_START	1
#define SCALE_SETTINGS_LEVEL_NOTE	2
#define SCALE_SETTINGS_LEVEL_EXIT	2
#define SCALE_SETTINGS_LEVEL_MAX	3

#define SCALE_PLAY_VELOCITY_DEFAULT		500
#define SCALE_PLAY_VELOCITY_FAST		400

#define SCALE_PLAY_VELOCITY_MICROTUNING	SCALE_PLAY_VELOCITY_DEFAULT
#define SCALE_PLAY_VELOCITY_TRANSPOSE 	SCALE_PLAY_VELOCITY_DEFAULT
#define SCALE_PLAY_VELOCITY_INITIAL		SCALE_PLAY_VELOCITY_FAST

//#define SCALE_PLAY_SPEED_MICROTUNING	150
//#define SCALE_PLAY_SPEED_TRANSPOSE 		300
//#define SCALE_PLAY_SPEED_INITIAL		100

#define SCALE_PLAY_NOTE_ON_DEFAULT	200
#define SCALE_PLAY_NOTE_OFF_DEFAULT	50
#define SCALE_PLAY_NOTE_ON_FAST		100
#define SCALE_PLAY_NOTE_OFF_FAST	25

#define NOTE_CENTER		8

// ----------------------------------------------------------------------------------------------------------------------------------

extern int volume_buttons_cnt;
extern int pwr_scale_button_cnt, short_press_button_pwr_scale, long_press_button_pwr_scale, button_pwr_scale;
extern int pwr_scale_and_plus_button_cnt, long_press_button_pwr_scale_and_plus;
extern int pwr_scale_and_minus_button_cnt, long_press_button_pwr_scale_and_minus;

#define PWR_SCALE_BUTTON_LONG_PRESS 				120		//1200ms is required to detect long press
#define PWR_SCALE_AND_PLUS_MINUS_BUTTON_LONG_PRESS 	120		//1200ms is required to detect long press to activate special action

extern int button_volume_plus, button_volume_minus, button_volume_both;
extern int short_press_volume_minus, short_press_volume_plus, short_press_volume_both, long_press_volume_both, long_press_volume_plus, long_press_volume_minus;
extern int button_wood, button_metal, button_wood_metal_both;
extern int short_press_wood, short_press_metal, short_press_wood_metal_both, long_press_wood_metal_both, long_press_wood, long_press_metal;
extern int buttons_ignore_timeout;

// ----------------------------------------------------------------------------------------------------------------------------------

#define BUTTONS_SHORT_PRESS 				5		//50ms is required to detect button press
#define BUTTONS_CNT_DELAY 					10		//general button scan timing in ms
#define VOLUME_BUTTONS_CNT_DIV 				10		//increase or decrease every 100ms
#define VOLUME_BUTTONS_CONTEXT_MENU_DELAY	50		//additional delay for long press in context menu

#define UI_EVENTS_LOOP_DELAY				15

#define VOLUME_BUTTONS_CNT_THRESHOLD 		50		//wait 500ms till adjusting volume
#define PLUS_OR_MINUS_BUTTON_LONG_PRESS		100		//1sec for long single volume button press event
#define VOLUME_BUTTONS_BOTH_LONG 			100		//1sec for long both volume buttons press event

#define WOOD_OR_METAL_BUTTONS_CNT_THRESHOLD	80		//the 800ms is upper limit to accept as a button short press
#define WOOD_OR_METAL_BUTTON_LONG_PRESS		100		//1sec for long single wood/metal button press event
#define WOOD_OR_METAL_BUTTONS_BOTH_LONG 	100		//1sec for long both wood/metal buttons press event

#define BUTTONS_IGNORE_TIMEOUT_LONG			100		//ignore next events timeout after long press event (useful when releasing slowly and not at the same time)

#define BUTTONS_SEQUENCE_TIMEOUT_DEFAULT	120		//maximum space between short key presses in a special command sequence is 1200ms
#define BUTTONS_SEQUENCE_TIMEOUT_SHORT		50		//in extra channels, short interval is better

extern int BUTTONS_SEQUENCE_TIMEOUT;

#define CONTEXT_MENU_ITEMS			4

#define CONTEXT_MENU_BASS_TREBLE	1
#define CONTEXT_MENU_TEMPO			2
#define CONTEXT_MENU_TUNING			3
#define CONTEXT_MENU_ECHO_ARP		4

#define SEQ_PLUS_MINUS				23
#define SEQ_MINUS_PLUS				32

#define PRESS_ORDER_MAX 				19	//maximum lenght of main-menu buttons sequence to parse
#define SCAN_BUTTONS_LOOP_DELAY 		50	//delay in ms between checking on new button press events
#define SET_RST_BUTTON_THRESHOLD_100ms 	2	//SCAN_BUTTONS_LOOP_DELAY * x ms -> 100ms
#define SET_RST_BUTTON_THRESHOLD_1s 	20	//SCAN_BUTTONS_LOOP_DELAY * x ms -> 1 sec
#define SET_RST_BUTTON_THRESHOLD_3s 	60	//SCAN_BUTTONS_LOOP_DELAY * x ms -> 3 sec

//#define TUNING_DEFAULT 440.0f

#define TUNING_ALL_432	432.0f,432.0f,432.0f,432.0f,432.0f,432.0f
#define TUNING_ALL_440	440.0f,440.0f,440.0f,440.0f,440.0f,440.0f

//extern double global_tuning;
extern uint8_t accelerometer_active;

//#define TUNING_INCREASE_COEFF_5_CENTS 1.0028922878693670715023923823933 //240th root of 2
//#define TUNING_INCREASE_COEFF_10_CENTS 1.0057929410678534309188527497122 //120th root of 2
//#define TUNING_INCREASE_COEFF_20_CENTS 1.0116194403019224846862305670455 //60th root of 2
//#define TUNING_INCREASE_COEFF_25_CENTS 1.0145453349375236414538678576629 //48th root of 2

//#define GLOBAL_TUNING_MIN 391.9954359818f
//#define GLOBAL_TUNING_MAX 493.8833012561f

extern settings_t global_settings;
extern persistent_settings_t persistent_settings;

#define PERSISTENT_SETTINGS_UPDATE_TIMER		200 //wait 2 sec before writing updated settings to Flash
#define PERSISTENT_SETTINGS_UPDATE_TIMER_LONG	500 //wait 5 sec before writing updated settings to Flash

#ifdef __cplusplus
 extern "C" {
#endif

void process_buttons_controls_drum(void *pvParameters);
void process_ui_events(void *pvParameters);

void check_auto_power_off(int extend_timeout);

void play_current_scale(int velocity, int delay_note_on, int delay_note_off, int keep_leds_on, int reverse_leds, int reset_leds);
void change_scale();

void clear_micro_tuning(int scale_settings);
void adjust_micro_tuning(int scale_settings, int direction);

void copy_scale_and_micro_tuning();
void paste_scale_and_micro_tuning();

void change_scale_up();
void change_scale_down();
void adjust_delay_length(int delay_or_reverb);
void fine_adjust_delay_length(int direction);

void check_led_blink_timeout();

#ifdef __cplusplus
}
#endif

#endif /* UI_H_ */
