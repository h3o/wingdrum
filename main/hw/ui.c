/*
 * ui.c
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

#include <ui.h>

#include <hw/codec.h>
#include <hw/gpio.h>
#include <hw/init.h>
#include <hw/signals.h>
#include <hw/midi.h>
#include <string.h>

#include <Interface.h>

//#define CALIBRATE_ACC_ON_DELAY_ADJUST
#define CALIBRATE_ACC_ON_DEMAND

int event_next_channel = 0, event_next_channel0 = 0;

int scale_settings = 0, delay_settings = 0, adjusting_note;
int *selected_scale; //holds the order numbers (pointers to the arrays) of currently selected scale for each patch, wood patches first, then metal ones
int previous_scale, patch_reverb_n;
int clearing_all_scales = 0;
int led_blink_stop = -1, led_blink_timeout = 0;

int patches_found_metal, patches_found_wood;

int volume_buttons_cnt = 0;
int pwr_scale_button_cnt = 0, short_press_button_pwr_scale, long_press_button_pwr_scale, button_pwr_scale = 0;
int pwr_scale_and_plus_button_cnt = 0, long_press_button_pwr_scale_and_plus;
int pwr_scale_and_minus_button_cnt = 0, long_press_button_pwr_scale_and_minus;

#define SHORT_PRESS_SEQ_MAX	10
int short_press_seq_cnt = 0;
int short_press_seq_ptr = 0;
char short_press_seq[SHORT_PRESS_SEQ_MAX];
int short_press_sequence = 0;

int button_volume_plus = 0, button_volume_minus = 0, button_volume_both = 0;
int short_press_volume_minus, short_press_volume_plus, short_press_volume_both,
		long_press_volume_both, long_press_volume_plus, long_press_volume_minus;

int button_wood = 0, button_metal = 0, button_wood_metal_both = 0;
int short_press_wood, short_press_metal, short_press_wood_metal_both,
		long_press_wood_metal_both, long_press_wood, long_press_metal;

int buttons_ignore_timeout = 0;

int btn_event = 0, btn_event_ext = 0;

settings_t global_settings;
persistent_settings_t persistent_settings;

#if defined(CALIBRATE_ACC_ON_DELAY_ADJUST) || defined(CALIBRATE_ACC_ON_DEMAND)
extern float acc_calibrated[], acc_res[];
#endif

uint8_t accelerometer_active = 0;

uint8_t button_SET_state = 0;
uint8_t button_RST_state = 0;
uint8_t button_user_state[4] = { 0, 0, 0, 0 };

int BUTTON_Ux_ON(int x)
{
	if (x == 0)
		return BUTTON_U1_ON;
	if (x == 1)
		return BUTTON_U2_ON;
	if (x == 2)
		return BUTTON_U3_ON;
	return -1;
}

void clear_buttons_counters_and_events()
{
	volume_buttons_cnt = 0;
	button_volume_plus = 0;
	button_volume_minus = 0;
	button_volume_both = 0;

	short_press_volume_minus = 0;
	short_press_volume_plus = 0;
	short_press_volume_both = 0;
	long_press_volume_both = 0;
	long_press_volume_plus = 0;
	long_press_volume_minus = 0;

	button_wood = 0;
	button_metal = 0;
	button_wood_metal_both = 0;
	short_press_wood = 0;
	short_press_metal = 0;
	short_press_wood_metal_both = 0;
	long_press_wood_metal_both = 0;
	long_press_wood = 0;
	long_press_metal = 0;

	button_pwr_scale = 0;
	pwr_scale_button_cnt = 0;
	pwr_scale_and_plus_button_cnt = 0;
	pwr_scale_and_minus_button_cnt = 0;

	short_press_button_pwr_scale = 0;
	long_press_button_pwr_scale = 0;
	long_press_button_pwr_scale_and_plus = 0;
	long_press_button_pwr_scale_and_minus = 0;

	short_press_seq_cnt = 0;
	short_press_seq_ptr = 0;
	short_press_sequence = 0;
}

void process_buttons_controls_drum(void *pvParameters)
{
	printf("process_buttons_controls_drum(): started on core ID=%d\n", xPortGetCoreID());

	while (1)
	{
		Delay(BUTTONS_CNT_DELAY);

		if (touch_pad_enabled)
		{
			//printf("process_buttons_controls_drum(): running on core ID=%d\n", xPortGetCoreID());

			if (NO_BUTTON_ON) //no buttons pressed at the moment
			{
				buttons_ignore_timeout = 0;
			}

			if (buttons_ignore_timeout)
			{
				buttons_ignore_timeout--;
				continue;
			}

			if (BUTTON_U1_ON) //button power/scale
			{
				button_pwr_scale++;
			}
			else if (BUTTON_U3_ON && !BUTTON_U2_ON) //button plus only
			{
				button_volume_plus++;
				volume_buttons_cnt++;

				if ((volume_buttons_cnt > VOLUME_BUTTONS_CNT_THRESHOLD)
						&& (volume_buttons_cnt % VOLUME_BUTTONS_CNT_DIV == 0)
						&& !scale_settings && !delay_settings)
				{
					if (codec_digital_volume > CODEC_DIGITAL_VOLUME_MAX) //actually means lower, as the value meaning is reversed
					{
						codec_digital_volume--; //actually means increment, as the value meaning is reversed
						codec_set_digital_volume();
						persistent_settings.DIGITAL_VOLUME = codec_digital_volume;
						persistent_settings.DIGITAL_VOLUME_updated = 1;
						persistent_settings.update = PERSISTENT_SETTINGS_UPDATE_TIMER;
					}
				}

				if ((volume_buttons_cnt > VOLUME_BUTTONS_CNT_THRESHOLD)
						&& (volume_buttons_cnt % VOLUME_BUTTONS_CNT_DIV == 0)
						&& scale_settings)
				{
					//printf("button_volume_plus(BUTTONS_LONG_PRESS) increment step\n");
					//micro-tune the selected note or octave
					adjust_micro_tuning(scale_settings, 1);
				}

				if ((volume_buttons_cnt > VOLUME_BUTTONS_CNT_THRESHOLD)
						&& (volume_buttons_cnt % VOLUME_BUTTONS_CNT_DIV == 0)
						&& delay_settings)
				{
					//fine adjust echo loop length
					fine_adjust_delay_length(1);
				}

				if (volume_buttons_cnt == PLUS_OR_MINUS_BUTTON_LONG_PRESS)
				{
					printf("button_volume_plus(BUTTONS_LONG_PRESS) detected\n");
					long_press_volume_plus = 1;
					last_button_event++;
				}
			}
			else if (BUTTON_U2_ON && !BUTTON_U3_ON) //button minus only
			{
				button_volume_minus++;
				volume_buttons_cnt++;

				if ((volume_buttons_cnt > VOLUME_BUTTONS_CNT_THRESHOLD)
						&& (volume_buttons_cnt % VOLUME_BUTTONS_CNT_DIV == 0)
						&& !scale_settings && !delay_settings)
				{
					if (codec_digital_volume < CODEC_DIGITAL_VOLUME_MIN) //actually means higher, as the value meaning is reversed
					{
						codec_digital_volume++; //actually means decrement, as the value meaning is reversed
						codec_set_digital_volume();
						persistent_settings.DIGITAL_VOLUME = codec_digital_volume;
						persistent_settings.DIGITAL_VOLUME_updated = 1;
						persistent_settings.update = PERSISTENT_SETTINGS_UPDATE_TIMER;
					}
				}

				if ((volume_buttons_cnt > VOLUME_BUTTONS_CNT_THRESHOLD)
						&& (volume_buttons_cnt % VOLUME_BUTTONS_CNT_DIV == 0)
						&& scale_settings)
				{
					//printf("button_volume_plus(BUTTONS_LONG_PRESS) decrement step\n");
					//micro-tune the selected note or octave
					adjust_micro_tuning(scale_settings, -1);
				}

				if ((volume_buttons_cnt > VOLUME_BUTTONS_CNT_THRESHOLD)
						&& (volume_buttons_cnt % VOLUME_BUTTONS_CNT_DIV == 0)
						&& delay_settings)
				{
					//fine adjust echo loop length
					fine_adjust_delay_length(-1);
				}

				if (volume_buttons_cnt == PLUS_OR_MINUS_BUTTON_LONG_PRESS)
				{
					printf("button_volume_minus(BUTTONS_LONG_PRESS) detected\n");
					long_press_volume_minus = 1;
					last_button_event++;
				}
			}
			else if (BUTTON_U2_ON && BUTTON_U3_ON) //button plus and minus at the same time
			{
				button_volume_both++;
				if (button_volume_both == VOLUME_BUTTONS_BOTH_LONG)
				{
					//special action
					long_press_volume_both = 1;
					last_button_event++;
					//buttons_ignore_timeout = BUTTONS_IGNORE_TIMEOUT_LONG;
					printf("button_volume_both(VOLUME_BUTTONS_BOTH_LONG) detected\n");
				}
			}
			else if (BUTTON_U4_ON && !BUTTON_U5_ON) //button wood only
			{
				button_wood++;

				if (button_wood == WOOD_OR_METAL_BUTTON_LONG_PRESS)
				{
					printf("button_wood(BUTTONS_LONG_PRESS) detected\n");
					long_press_wood = 1;
					last_button_event++;
				}
			}
			else if (!BUTTON_U4_ON && BUTTON_U5_ON) //button metal only
			{
				button_metal++;

				if (button_metal == WOOD_OR_METAL_BUTTON_LONG_PRESS)
				{
					printf("button_metal(BUTTONS_LONG_PRESS) detected\n");
					long_press_metal = 1;
					last_button_event++;
				}
			}
			else if (BUTTON_U4_ON && BUTTON_U5_ON) //button wood and metal at the same time
			{
				button_wood_metal_both++;
				if (button_wood_metal_both == WOOD_OR_METAL_BUTTONS_BOTH_LONG)
				{
					//special action
					long_press_wood_metal_both = 1;
					last_button_event++;
					buttons_ignore_timeout = BUTTONS_IGNORE_TIMEOUT_LONG;
					printf("button_wood_metal_both(VOLUME_BUTTONS_BOTH_LONG) detected\n");
				}
			}
			else //no button pressed at the moment
			{
				volume_buttons_cnt = 0;

				if(clearing_all_scales && clearing_all_scales < 8 && button_volume_both)
				{
					printf("clearing_all_scales = %d, aborting\n", clearing_all_scales);
					clearing_all_scales = 0;
					LEDS_ALL_ON;
					button_volume_both = 0;
					buttons_ignore_timeout = BUTTONS_IGNORE_TIMEOUT_LONG;
				}

				//check if any button was just released recently but not held long enough for another command
				if (button_pwr_scale > BUTTONS_SHORT_PRESS && button_pwr_scale < PWR_SCALE_BUTTON_LONG_PRESS)
				{
					printf("button_pwr_scale(BUTTONS_SHORT_PRESS) detected\n");
					short_press_button_pwr_scale = 1;
					last_button_event++;
				}
				else if (button_volume_plus > BUTTONS_SHORT_PRESS && button_volume_plus < VOLUME_BUTTONS_CNT_THRESHOLD)
				{
					printf("button_volume_plus(BUTTONS_SHORT_PRESS) detected\n");
					short_press_volume_plus = 1;
					last_button_event++;
				}
				else if (button_volume_minus > BUTTONS_SHORT_PRESS && button_volume_minus < VOLUME_BUTTONS_CNT_THRESHOLD)
				{
					printf("button_volume_minus(BUTTONS_SHORT_PRESS) detected\n");
					short_press_volume_minus = 1;
					last_button_event++;
				}
				else if (button_volume_both > BUTTONS_SHORT_PRESS && button_volume_both < VOLUME_BUTTONS_CNT_THRESHOLD)
				{
					printf("button_volume_both(BUTTONS_SHORT_PRESS) detected\n");
					short_press_volume_both = 1;
					last_button_event++;
				}
				else if (button_wood > BUTTONS_SHORT_PRESS && button_wood < WOOD_OR_METAL_BUTTONS_CNT_THRESHOLD)
				{
					printf("button_wood(BUTTONS_SHORT_PRESS) detected\n");
					short_press_wood = 1;
					last_button_event++;
				}
				else if (button_metal > BUTTONS_SHORT_PRESS && button_metal < WOOD_OR_METAL_BUTTONS_CNT_THRESHOLD)
				{
					printf("button_metal(BUTTONS_SHORT_PRESS) detected\n");
					short_press_metal = 1;
					last_button_event++;
				}
				else if (button_wood_metal_both > BUTTONS_SHORT_PRESS && button_wood_metal_both < WOOD_OR_METAL_BUTTONS_CNT_THRESHOLD)
				{
					printf("button_wood_metal_both(BUTTONS_SHORT_PRESS) detected\n");
					short_press_wood_metal_both = 1;
					last_button_event++;
				}

				button_volume_plus = 0;
				button_volume_minus = 0;
				button_volume_both = 0;
				button_metal = 0;
				button_wood = 0;
				button_wood_metal_both = 0;
				button_pwr_scale = 0;
			}

			if (BUTTON_U1_ON && !BUTTON_U2_ON && !BUTTON_U3_ON && /*!BUTTON_U4_ON &&*/ !BUTTON_U5_ON) //button power/scale only (PWR+BTN4 is a HW reset)
			{
				pwr_scale_button_cnt++;
				if (pwr_scale_button_cnt == PWR_SCALE_BUTTON_LONG_PRESS)
				{
					printf("button_pwr_scale(BUTTONS_LONG_PRESS) detected\n");
					long_press_button_pwr_scale = 1;
					last_button_event++;
				}
			}
			else
			{
				pwr_scale_button_cnt = 0;
			}

			if (BUTTON_U1_ON && !BUTTON_U2_ON && BUTTON_U3_ON) //button power/scale and plus
			{
				pwr_scale_and_plus_button_cnt++;
				if (pwr_scale_and_plus_button_cnt == PWR_SCALE_AND_PLUS_MINUS_BUTTON_LONG_PRESS)
				{
					printf("button_pwr_scale_and_plus(BUTTONS_LONG_PRESS) detected\n");
					long_press_button_pwr_scale_and_plus = 1;
					last_button_event++;
					buttons_ignore_timeout = BUTTONS_IGNORE_TIMEOUT_LONG;
				}
			}
			else
			{
				pwr_scale_and_plus_button_cnt = 0;
			}

			if (BUTTON_U1_ON && BUTTON_U2_ON && !BUTTON_U3_ON) //button power/scale and minus
			{
				pwr_scale_and_minus_button_cnt++;
				if (pwr_scale_and_minus_button_cnt == PWR_SCALE_AND_PLUS_MINUS_BUTTON_LONG_PRESS)
				{
					printf("button_pwr_scale_and_minus(BUTTONS_LONG_PRESS) detected\n");
					long_press_button_pwr_scale_and_minus = 1;
					last_button_event++;
					buttons_ignore_timeout = BUTTONS_IGNORE_TIMEOUT_LONG;
				}
			}
			else
			{
				pwr_scale_and_minus_button_cnt = 0;
			}

			#ifdef DISABLE_MCLK_1_SEC
			if (mclk_enabled && ms10_counter==100)
			{
				stop_MCLK();
			}
			#endif

			//ms10_counter++;
			if (persistent_settings.update)
			{
				if (persistent_settings.update==1)
				{
					//good time to save settings
					printf("process_buttons_controls_drum(): store_persistent_settings()\n");
					store_persistent_settings(&persistent_settings);
				}
				persistent_settings.update--;
			}

			/*
			//if (ms10_counter % BUTTONS_SEQUENCE_INTERVAL == 0) //every 1200ms
			if (short_press_seq_cnt)
			{
				short_press_seq_cnt--;
				if (short_press_seq_cnt==1)
				{
					buttons_sequence_check(0);
				}
			}
			*/
		}
	}
}

void process_ui_events(void *pvParameters)
{
	printf("process_ui_events(): started on core ID=%d\n", xPortGetCoreID());

	while (1)
	{
		Delay(UI_EVENTS_LOOP_DELAY);

		//actions for button events

		if (long_press_volume_both)
		{
			long_press_volume_both = 0;
			Delay(50);
			clear_buttons_counters_and_events();

			if(scale_settings==SCALE_SETTINGS_LEVEL_START)
			{
				if(clearing_all_scales<7)
				{
					printf("clearing_all_scales = %d, incrementing\n", clearing_all_scales);
					LED_X_OFF((clearing_all_scales+6)%8);
					clearing_all_scales++;
				}
				else
				{
					LEDS_ALL_OFF;
					Delay(1000);
					for(int i=0;i<4;i++)
					{
						LEDS_ALL_ON;
						Delay(200);
						LEDS_ALL_OFF;
						Delay(200);
					}

					//power_off_animation(1, LED_ERASE_DATA_DELAY);
					delete_all_custom_scales();
				}
			}
			else if(!delay_settings)
			{
				printf("long_press_volume_both while scale_settings==0: calibrating accelerometer\n");

				#ifdef CALIBRATE_ACC_ON_DEMAND
				acc_calibrated[0] = acc_res[0];
				acc_calibrated[1] = acc_res[1];
				printf("calibrated: %f / %f\n", acc_calibrated[0], acc_calibrated[1]);
				#endif

				if(!global_settings.ACCELEROMETER_CALIBRATE_ON_POWERUP)
				{
					persistent_settings_store_calibration();
				}

				for(int i=0; i<3; i++)
				{
					LEDS_ALL_ON;
					Delay(80);
					LEDS_ALL_OFF;
					Delay(80);
				}

				while(BUTTON_U2_ON || BUTTON_U3_ON); //wait until both released
				clear_buttons_counters_and_events();
			}
		}

		if (short_press_volume_both && !scale_settings)
		{
			printf("if (short_press_volume_both && !scale_settings)\n");
			short_press_volume_both = 0;

			#ifdef ANALYSE_THRESHOLDS
			print_thresholds_info(1);
			#endif

			#ifdef CALIBRATE_ACC_ON_DELAY_ADJUST
			acc_calibrated[0] = acc_res[0];
			acc_calibrated[1] = acc_res[1];
			#endif
		}

		if (short_press_volume_both && scale_settings)
		{
			short_press_volume_both = 0;
			clear_micro_tuning(scale_settings);
		}

		if (long_press_button_pwr_scale_and_plus)
		{
			long_press_button_pwr_scale_and_plus = 0;
			buttons_ignore_timeout = BUTTONS_IGNORE_TIMEOUT_LONG;
			paste_scale_and_micro_tuning();
		}

		if (long_press_button_pwr_scale_and_minus)
		{
			long_press_button_pwr_scale_and_minus = 0;
			buttons_ignore_timeout = BUTTONS_IGNORE_TIMEOUT_LONG;
			copy_scale_and_micro_tuning();
		}

		if (long_press_button_pwr_scale)
		{
			event_next_channel = EVENT_NEXT_CHANNEL_PWR_OFF;
			long_press_button_pwr_scale = 0;
		}

		if (short_press_button_pwr_scale)
		{
			short_press_button_pwr_scale = 0;

			if(delay_settings)
			{
				if(echo_dynamic_loop_current_step>=0)
				{
					persistent_settings_store_delay(echo_dynamic_loop_current_step);
				}
				else
				{
					persistent_settings_store_delay(-echo_dynamic_loop_length);
				}

				for(int i=0; i<4; i++)
				{
					LEDS_ALL_ON;
					Delay(60);
					LEDS_ALL_OFF;
					Delay(60);
				}
				delay_settings = 0; //exit the delay menu
			}
			else
			{
				scale_settings++;
				if(scale_settings>=SCALE_SETTINGS_LEVEL_EXIT)
				{
					scale_settings = 0;
					LEDS_ALL_OFF;

					//store the new scale
					store_patch_scales_if_changed(current_patch, patch_notes);
					store_micro_tuning_if_changed(current_patch, micro_tuning);
				}
				printf("scale_settings => %d\n", scale_settings);

				if(scale_settings==SCALE_SETTINGS_LEVEL_START)
				{
					play_current_scale(SCALE_PLAY_VELOCITY_INITIAL,global_settings.SCALE_PLAY_SPEED_INITIAL,SCALE_PLAY_NOTE_OFF_FAST,1,0,0);
				}
			}
		}

		if (short_press_volume_plus)
		{
			short_press_volume_plus = 0;

			if(delay_settings)
			{
				printf("short_press_volume_plus, delay_settings = %d\n", delay_settings);
				fine_adjust_delay_length(1);
			}
			else if(!scale_settings)
			{
				change_scale_up();
			}
			else if(scale_settings==SCALE_SETTINGS_LEVEL_START)
			{
				//transpose the whole scale up a semitone
				for(int i=0;i<9;i++)
				{
					midi_scale_selected[i]++;
				}
				play_current_scale(SCALE_PLAY_VELOCITY_TRANSPOSE,global_settings.SCALE_PLAY_SPEED_TRANSPOSE,SCALE_PLAY_NOTE_OFF_DEFAULT,0,1,1);
			}
			else if(scale_settings==SCALE_SETTINGS_LEVEL_NOTE)
			{
				//transpose the selected note up a semitone
				midi_scale_selected[note_map[adjusting_note]]++;
				new_note(0,adjusting_note,1,SCALE_PLAY_VELOCITY_DEFAULT);
				Delay(SCALE_PLAY_NOTE_ON_FAST);
				new_note(0,adjusting_note,0,0);
			}
		}
		else if (short_press_volume_minus)
		{
			short_press_volume_minus = 0;

			if(delay_settings)
			{
				printf("short_press_volume_minus, delay_settings = %d\n", delay_settings);
				fine_adjust_delay_length(-1);
			}
			else if(!scale_settings)
			{
				change_scale_down();
			}
			else if(scale_settings==SCALE_SETTINGS_LEVEL_START)
			{
				//transpose the whole scale down a semitone
				for(int i=0;i<9;i++)
				{
					midi_scale_selected[i]--;
				}
				play_current_scale(SCALE_PLAY_VELOCITY_TRANSPOSE,global_settings.SCALE_PLAY_SPEED_TRANSPOSE,SCALE_PLAY_NOTE_OFF_DEFAULT,0,1,1);
			}
			else if(scale_settings==SCALE_SETTINGS_LEVEL_NOTE)
			{
				//transpose the selected note down a semitone
				midi_scale_selected[note_map[adjusting_note]]--;
				new_note(0,adjusting_note,1,SCALE_PLAY_VELOCITY_DEFAULT);
				Delay(SCALE_PLAY_NOTE_ON_FAST);
				new_note(0,adjusting_note,0,0);
			}
		}

		if (short_press_wood)
		{
			if(!delay_settings && !scale_settings)
			{
				event_next_channel = EVENT_NEXT_CHANNEL_WOOD;
			}
			else if(delay_settings)
			{
				printf("short_press_wood, delay_settings = %d\n", delay_settings);
				adjust_delay_length(1);
			}
			short_press_wood = 0;
		}

		if (short_press_metal)
		{
			if(!delay_settings && !scale_settings)
			{
				event_next_channel = EVENT_NEXT_CHANNEL_METAL;
			}
			else if(delay_settings)
			{
				printf("short_press_metal, delay_settings = %d\n", delay_settings);
				adjust_delay_length(2);
			}
			short_press_metal = 0;
		}

		if (short_press_wood_metal_both)
		{
			if(delay_settings)
			{
				LEDS_ALL_OFF;
				Delay(LED_ON_TIMEOUT_DELAY_CHANGE*100);
				LEDS_ALL_ON;

				echo_dynamic_loop_current_step = 8;
				echo_dynamic_loop_length = echo_dynamic_loop_steps[echo_dynamic_loop_current_step];
				printf("echo_dynamic_loop_current_step = %d, echo_dynamic_loop_length = %d\n", echo_dynamic_loop_current_step, echo_dynamic_loop_length);
			}
			if(!delay_settings && !scale_settings)
			{
				event_next_channel = EVENT_NEXT_CHANNEL_BOTH;
			}
			short_press_wood_metal_both = 0;
		}

		if(long_press_wood_metal_both)
		{
			long_press_wood_metal_both = 0;

			printf("long_press_wood_metal_both: enter settings\n");

			for(int i=0; i<3; i++)
			{
				LEDS_ALL_OFF;
				Delay(100);
				LEDS_ALL_ON;
				Delay(100);
			}

			while(BUTTON_U4_ON || BUTTON_U5_ON); //wait until both released
			clear_buttons_counters_and_events();

			delay_settings = 1;
			ECHO_MIXING_GAIN_MUL_TARGET = 8;
		}
	}
	//---------------------------------------------------------------------------
}

void check_auto_power_off(int extend_timeout)
{
	auto_power_off--;

	//printf("check_auto_power_off(): timer = %d\n", auto_power_off);

	if(auto_power_off_canceled)
	{
		printf("check_auto_power_off(): auto_power_off_canceled\n");
		codec_digital_volume = codec_volume_user;
		//printf("check_auto_power_off(): codec_digital_volume = codec_volume_user = %d\n", codec_digital_volume);
		LEDS_ALL_OFF;
		auto_power_off_canceled = 0;
		printf("check_auto_power_off(): codec_set_digital_volume() => codec_digital_volume %d\n", codec_digital_volume);
		codec_set_digital_volume();
	}
	else if (auto_power_off < AUTO_POWER_OFF_VOLUME_RAMP)
	{
		printf("check_auto_power_off(): auto_power_off < AUTO_POWER_OFF_VOLUME_RAMP (%d < %d)\n", auto_power_off, AUTO_POWER_OFF_VOLUME_RAMP);
		codec_digital_volume = codec_volume_user + (int)((1.0f - (float)(auto_power_off) / (float)AUTO_POWER_OFF_VOLUME_RAMP) * ((float)(CODEC_DIGITAL_VOLUME_MIN) / 1.5f - (float)(codec_volume_user)));
		//printf("check_auto_power_off(): codec_set_digital_volume() => codec_digital_volume %d\n", codec_digital_volume);
		codec_set_digital_volume();
	}

	if (auto_power_off==0)
	{
		printf("check_auto_power_off(): auto_power_off==0\n");
		int codec_volume_user_backup = codec_volume_user;
		codec_set_mute(1);
		if(!power_off_animation(1, LED_PWR_OFF_DELAY))
		{
			printf("check_auto_power_off(): shutting down!\n");
			drum_shutdown();
		}
		else
		{
			printf("check_auto_power_off(): shutdown canceled!\n");
			codec_volume_user = codec_volume_user_backup;
			codec_set_mute(0);
			//printf("check_auto_power_off(): codec_digital_volume = codec_volume_user = %d\n", codec_digital_volume);
			auto_power_off = extend_timeout;
			codec_digital_volume = codec_volume_user;
			//printf("check_auto_power_off(): codec_digital_volume = codec_volume_user = %d\n", codec_digital_volume);
			codec_set_digital_volume();
			LEDS_ALL_OFF;
		}
	}
}

void play_current_scale(int velocity, int delay_note_on, int delay_note_off, int keep_leds_on, int reverse_leds, int reset_leds)
{
	//while(BUTTON_U2_ON || BUTTON_U3_ON);
	last_button_event = 0;
	last_touch_event = 0;

	for(int i=0;i<9;i++)
	{
		//if(!BUTTON_U2_ON && !BUTTON_U3_ON)
		if(!last_button_event && !last_touch_event)
		{
			new_note(0,note_map_reverse_center_last[i],1,velocity);
			if(reverse_leds)
			{
				LED_X_OFF(note_map_reverse_center_last[i]);
			}
			else
			{
				LED_X_ON(note_map_reverse_center_last[i]);
			}
		}
		else break;

		//if(!BUTTON_U2_ON && !BUTTON_U3_ON)
		if(!last_button_event && !last_touch_event)
		{
			Delay(delay_note_on);
		}

		new_note(0,note_map_reverse_center_last[i],0,0);
		if(!keep_leds_on)
		{
			if(reverse_leds)
			{
				LED_X_ON(note_map_reverse_center_last[i]);
			}
			else
			{
				LED_X_OFF(note_map_reverse_center_last[i]);
			}
		}

		//if(!BUTTON_U2_ON && !BUTTON_U3_ON)
		if(!last_button_event && !last_touch_event)
		{
			Delay(delay_note_off);
		}
		//if(BUTTON_U2_ON || BUTTON_U3_ON) break;
		if(last_button_event || last_touch_event) break;
	}

	if(reset_leds)
	{
		if(reverse_leds)
		{
			LEDS_ALL_ON;
		}
		else
		{
			LEDS_ALL_OFF;
		}
	}
}

void change_scale()
{
	if(led_blink_stop>=0)
	{
		LED_X_ON_NOUPDATE(led_blink_stop); //because the current state is B2, need to clear both control bits first
		LED_X_OFF(led_blink_stop);
	}
	led_blink_stop = (selected_scale[current_patch] + 6) % 8; //need to shift 2 LEDs back as they are numbered from 7th one
	LED_X_B2(led_blink_stop);
	//printf("LED_X_B2(%d)\n", led_blink_stop);
	led_blink_timeout = LED_BLINK_TIMEOUT_SCALE_CHANGE;
	printf("selected_scale[current_patch] => %d\n", selected_scale[current_patch]);
	midi_scale_selected = &patch_notes[selected_scale[current_patch]*NOTES_PER_SCALE];
	micro_tuning_selected = &micro_tuning[selected_scale[current_patch]*NOTES_PER_SCALE];

	if(midi_scale_selected[0]==0) //no notes defined yet
	{
		printf("no notes yet, copying over scale #%d => #%d\n", previous_scale, selected_scale[current_patch]);
		memcpy(midi_scale_selected, &patch_notes[previous_scale*NOTES_PER_SCALE], NOTES_PER_SCALE*sizeof(int));
		memcpy(micro_tuning_selected, &micro_tuning[previous_scale*NOTES_PER_SCALE], NOTES_PER_SCALE*sizeof(float));
	}
}

void clear_micro_tuning(int scale_settings)
{
	printf("clear_micro_tuning(scale_settings=%d)\n", scale_settings);
	if(scale_settings==SCALE_SETTINGS_LEVEL_START) //all notes at once
	{
		//memset(micro_tuning_selected, 0, NOTES_PER_SCALE * sizeof(float));
	    for(int i=0;i<NOTES_PER_SCALE;i++)
	    {
	    	micro_tuning_selected[i] = 1.0f;
	    }

	    play_current_scale(SCALE_PLAY_VELOCITY_MICROTUNING,global_settings.SCALE_PLAY_SPEED_MICROTUNING,SCALE_PLAY_NOTE_OFF_DEFAULT,0,1,1);
	}
	else if(scale_settings==SCALE_SETTINGS_LEVEL_NOTE)
	{
		micro_tuning_selected[note_map[adjusting_note]] = 1.0f;
		printf("micro_tuning_selected[note_map[%d]] => %.24f\n", adjusting_note, micro_tuning_selected[note_map[adjusting_note]]);

		new_note(0,adjusting_note,1,SCALE_PLAY_VELOCITY_DEFAULT);
		Delay(SCALE_PLAY_NOTE_ON_FAST);
		new_note(0,adjusting_note,0,0);
	}
}

void adjust_micro_tuning(int scale_settings, int direction)
{
	printf("adjust_micro_tuning(scale_settings=%d, direction=%d)\n", scale_settings, direction);

	if(scale_settings==SCALE_SETTINGS_LEVEL_START) //all notes at once
	{
		//shift the whole scale up by a micro-tuning step
		for(int i=0;i<9;i++)
		{
			if(direction>0)
			{
				micro_tuning_selected[note_map[i]] *= global_settings.MICRO_TUNING_STEP;
			}
			else
			{
				micro_tuning_selected[note_map[i]] /= global_settings.MICRO_TUNING_STEP;
			}
			printf("micro_tuning_selected[note_map[%d]] => %.24f\n", i, micro_tuning_selected[note_map[i]]);
		}

		play_current_scale(SCALE_PLAY_VELOCITY_MICROTUNING,global_settings.SCALE_PLAY_SPEED_MICROTUNING,SCALE_PLAY_NOTE_OFF_DEFAULT,0,1,1);
	}
	else if(scale_settings==SCALE_SETTINGS_LEVEL_NOTE)
	{
		if(direction>0)
		{
			micro_tuning_selected[note_map[adjusting_note]] *= global_settings.MICRO_TUNING_STEP;
		}
		else
		{
			micro_tuning_selected[note_map[adjusting_note]] /= global_settings.MICRO_TUNING_STEP;
		}

		printf("micro_tuning_selected[note_map[%d]] => %.24f\n", adjusting_note, micro_tuning_selected[note_map[adjusting_note]]);

		new_note(0,adjusting_note,1,SCALE_PLAY_VELOCITY_DEFAULT);
		Delay(SCALE_PLAY_NOTE_ON_FAST);
		new_note(0,adjusting_note,0,0);
	}
}

void copy_scale_and_micro_tuning()
{
	memcpy(midi_scale_clipboard, midi_scale_selected, NOTES_PER_SCALE * sizeof(int));
	memcpy(micro_tuning_clipboard, micro_tuning_selected, NOTES_PER_SCALE * sizeof(float));

	for(int i=0; i<4; i++)
	{
		LEDS_ALL_ON;
		Delay(100);
		LEDS_ALL_OFF;
		Delay(100);
	}
}

void paste_scale_and_micro_tuning()
{
	if(midi_scale_clipboard[0])
	{
		memcpy(midi_scale_selected, midi_scale_clipboard, NOTES_PER_SCALE * sizeof(int));
		memcpy(micro_tuning_selected, micro_tuning_clipboard, NOTES_PER_SCALE * sizeof(float));

		for(int i=0; i<2; i++)
		{
			LEDS_ALL_ON;
			Delay(200);
			LEDS_ALL_OFF;
			Delay(200);
		}
	}
}

void change_scale_up()
{
	previous_scale = selected_scale[current_patch];
	selected_scale[current_patch]++;
	if(selected_scale[current_patch]>=SCALES_PER_PATCH)
	{
		selected_scale[current_patch] = 0;
	}
	change_scale();
}

void change_scale_down()
{
	previous_scale = selected_scale[current_patch];
	selected_scale[current_patch]--;
	if(selected_scale[current_patch]<0)
	{
		selected_scale[current_patch] = SCALES_PER_PATCH - 1;
	}
	change_scale();
}

void adjust_delay_length(int delay_or_reverb)
{
	echo_dynamic_loop_current_step++;

	if (echo_dynamic_loop_current_step == ECHO_DYNAMIC_LOOP_STEPS)
	{
		echo_dynamic_loop_current_step = 0;
	}

	if(delay_or_reverb==1 && echo_dynamic_loop_current_step > 8)
	{
		echo_dynamic_loop_current_step = 0;
	}
	if(delay_or_reverb==2 && echo_dynamic_loop_current_step <= 8)
	{
		echo_dynamic_loop_current_step = 9;
	}

	echo_dynamic_loop_length = echo_dynamic_loop_steps[echo_dynamic_loop_current_step];

	if(led_blink_stop>-1)
	{
		LED_X_ON(led_blink_stop);
	}
	if(echo_dynamic_loop_current_step!=8 && echo_dynamic_loop_current_step!=17)
	{
		if(echo_dynamic_loop_current_step<8)
		{
			led_blink_stop = (echo_dynamic_loop_current_step + 6) % 8;
		}
		else
		{
			led_blink_stop = (echo_dynamic_loop_current_step + 5) % 8;
		}
		LED_X_OFF(led_blink_stop);
		led_blink_timeout = LED_ON_TIMEOUT_DELAY_CHANGE;
	}
	else
	{
		LEDS_ALL_OFF;
		Delay(LED_ON_TIMEOUT_DELAY_CHANGE*100);
		LEDS_ALL_ON;
	}

	echo_dynamic_loop_length = echo_dynamic_loop_steps[echo_dynamic_loop_current_step];
	printf("echo_dynamic_loop_current_step = %d, echo_dynamic_loop_length = %d, led_blink_stop = %d\n", echo_dynamic_loop_current_step, echo_dynamic_loop_length, led_blink_stop);
}

void fine_adjust_delay_length(int direction)
{
	echo_dynamic_loop_length += direction * (echo_dynamic_loop_length/ECHO_DYNAMIC_LOOP_LENGTH_ADJUST_FACTOR+1);
	echo_dynamic_loop_current_step = -1;
	if(echo_dynamic_loop_length>ECHO_BUFFER_LENGTH) echo_dynamic_loop_length = ECHO_BUFFER_LENGTH;
	if(echo_dynamic_loop_length<ECHO_BUFFER_LENGTH_MIN) echo_dynamic_loop_length = ECHO_BUFFER_LENGTH_MIN;

	printf("adjust_echo_loop_length(%d): echo_dynamic_loop_length = %d\n", direction, echo_dynamic_loop_length);
}

void check_led_blink_timeout()
{
	//printf("led_blink_timeout=%d\n", led_blink_timeout);
	if(led_blink_timeout)
	{
		led_blink_timeout--;
		if(led_blink_timeout==0)
		{
			if(delay_settings) //while settings active, LEDs logic is reversed
			{
				LED_X_ON(led_blink_stop);
			}
			else
			{
				//printf("LED_X_OFF(%d)\n", led_blink_stop);
				LED_X_ON_NOUPDATE(led_blink_stop); //because the current state is B2, need to clear both control bits first
				LED_X_OFF(led_blink_stop);
			}
			//LEDS_ALL_OFF;
			led_blink_stop = -1;
		}
	}
}
