/*
 * Reverb.cpp
 *
 *  Copyright 2024 Phonicbloom Ltd.
 *
 *  Created on: 16 Jul 2018
 *      Author: mario
 *
 *  This file can be used within the terms of GNU GPLv3 license: https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 *  Find more information at:
 *  http://phonicbloom.com/
 *  http://gechologic.com/
 *
 */

#include <Reverb.h>
#include <Accelerometer.h>
#include <Interface.h>
#include <InitChannels.h>
#include <hw/signals.h>
#include <hw/gpio.h>
#include <hw/midi.h>
#include <hw/init.h>
#include <string.h>
#include <math.h>

#ifdef BOARD_WHALE
//sensor 3 - echo delay length
#define ACC_SENSOR_DELAY_9					(acc_res[2] > 0.95f)
#define ACC_SENSOR_DELAY_8					(acc_res[2] > 0.85f)
#define ACC_SENSOR_DELAY_7					(acc_res[2] > 0.74f)
#define ACC_SENSOR_DELAY_6					(acc_res[2] > 0.62f)
#define ACC_SENSOR_DELAY_5					(acc_res[2] > 0.49f)
#define ACC_SENSOR_DELAY_4					(acc_res[2] > 0.35f)
#define ACC_SENSOR_DELAY_3					(acc_res[2] > 0.2f)
#define ACC_SENSOR_DELAY_2					(acc_res[2] > 0.0f)
#define ACC_SENSOR_DELAY_ACTIVE				(acc_res[2] > -0.2f)
#else
//sensor 3 - echo delay length
#define ACC_SENSOR_DELAY_9					(acc_res[0] > 0.95f)
#define ACC_SENSOR_DELAY_8					(acc_res[0] > 0.85f)
#define ACC_SENSOR_DELAY_7					(acc_res[0] > 0.74f)
#define ACC_SENSOR_DELAY_6					(acc_res[0] > 0.62f)
#define ACC_SENSOR_DELAY_5					(acc_res[0] > 0.49f)
#define ACC_SENSOR_DELAY_4					(acc_res[0] > 0.35f)
#define ACC_SENSOR_DELAY_3					(acc_res[0] > 0.2f)
#define ACC_SENSOR_DELAY_2					(acc_res[0] > 0.0f)
#define ACC_SENSOR_DELAY_ACTIVE				(acc_res[0] > -0.2f)
#endif

#ifdef BOARD_GECHO
//sensor 1 - echo delay length
#define IRS_SENSOR_DELAY_9					SENSOR_THRESHOLD_RED_9
#define IRS_SENSOR_DELAY_8					SENSOR_THRESHOLD_RED_8
#define IRS_SENSOR_DELAY_7					SENSOR_THRESHOLD_RED_7
#define IRS_SENSOR_DELAY_6					SENSOR_THRESHOLD_RED_6
#define IRS_SENSOR_DELAY_5					SENSOR_THRESHOLD_RED_5
#define IRS_SENSOR_DELAY_4					SENSOR_THRESHOLD_RED_4
#define IRS_SENSOR_DELAY_3					SENSOR_THRESHOLD_RED_3
#define IRS_SENSOR_DELAY_2					SENSOR_THRESHOLD_RED_2
#define IRS_SENSOR_DELAY_ACTIVE				SENSOR_THRESHOLD_RED_1
#endif

void decaying_reverb(int extended_buffers)
{
	//program_settings_reset();
	channel_init(0, 0, 0, 0, 0, 0, 0, 0, extended_buffers); //init without any features, possibly use extra buffers

	/*
	//echo_dynamic_loop_length = I2S_AUDIOFREQ; //set default value (can be changed dynamically)
	//#define ECHO_LENGTH_DEFAULT ECHO_BUFFER_LENGTH
	DELAY_BY_TEMPO = get_delay_by_BPM(tempo_bpm);
    echo_dynamic_loop_length = DELAY_BY_TEMPO; //ECHO_LENGTH_DEFAULT;
	//echo_buffer = (int16_t*)malloc(echo_dynamic_loop_length*sizeof(int16_t)); //allocate memory
	//memset(echo_buffer,0,echo_dynamic_loop_length*sizeof(int16_t)); //clear memory
	memset(echo_buffer,0,ECHO_BUFFER_LENGTH*sizeof(int16_t)); //clear the entire buffer
	echo_buffer_ptr0 = 0; //reset pointer
	*/
	echo_dynamic_loop_length = ECHO_BUFFER_LENGTH_DEFAULT;
    ECHO_MIXING_GAIN_MUL = 3; //amount of signal to feed back to echo loop, expressed as a fragment
    ECHO_MIXING_GAIN_DIV = 4; //e.g. if MUL=2 and DIV=3, it means 2/3 of signal is mixed in

	#ifdef SWITCH_I2C_SPEED_MODES
	i2c_master_deinit();
    i2c_master_init(1); //fast mode
	#endif

	#if !defined(REVERB_POLYPHONIC) //&& !defined(REVERB_OCTAVE_UP_DOWN)

    //int bit_crusher_div = BIT_CRUSHER_DIV_DEFAULT;
	//int bit_crusher_reverb_d = BIT_CRUSHER_REVERB_DEFAULT;
	int bit_crusher_reverb_d = I2S_AUDIOFREQ / 48;
	reverb_dynamic_loop_length = bit_crusher_reverb_d;

	//#define DECAY_DIR_FROM		-4
	//#define DECAY_DIR_TO		4
	//#define DECAY_DIR_DEFAULT	1
	//int decay_direction = DECAY_DIR_DEFAULT;

	/*
    REVERB_MIXING_GAIN_MUL = 9; //amount of signal to feed back to reverb loop, expressed as a fragment
    REVERB_MIXING_GAIN_DIV = 10; //e.g. if MUL=2 and DIV=3, it means 2/3 of signal is mixed in

    reverb_dynamic_loop_length = BIT_CRUSHER_REVERB_DEFAULT; //set default value (can be changed dynamically)
	//reverb_buffer = (int16_t*)malloc(reverb_dynamic_loop_length*sizeof(int16_t)); //allocate memory
	memset(reverb_buffer,0,REVERB_BUFFER_LENGTH*sizeof(int16_t)); //clear the entire buffer
	reverb_buffer_ptr0 = 0; //reset pointer
	*/

	printf("decaying_reverb(): REVERB_MIXING_GAIN_MUL_EXT=%f, REVERB_MIXING_GAIN_DIV_EXT=%f\n", REVERB_MIXING_GAIN_MUL_EXT, REVERB_MIXING_GAIN_DIV_EXT);
	printf("decaying_reverb(): REVERB_MIXING_GAIN_MUL=%f, REVERB_MIXING_GAIN_DIV=%f\n", REVERB_MIXING_GAIN_MUL, REVERB_MIXING_GAIN_DIV);
	#endif

	#ifdef BOARD_WHALE_V1
    RGB_LED_R_ON;
	#endif

	//int lock_sensors = 0;

	#ifdef LPF_ENABLED
	ADC_LPF_ALPHA = 0.2f;
	#endif

	current_patch = patch_reverb_n;
	patch_notes = notes_reverb[0];

	//load user-defined scales from NVS if present
	load_patch_scales(current_patch, patch_notes);
	load_micro_tuning(current_patch, micro_tuning);

	midi_scale_selected = &patch_notes[selected_scale[current_patch]*NOTES_PER_SCALE];
	micro_tuning_selected = &micro_tuning[selected_scale[current_patch]*NOTES_PER_SCALE];
    printf("decaying_reverb(): micro_tuning_selected = (%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f)\n",
    		micro_tuning_selected[0],
    		micro_tuning_selected[1],
    		micro_tuning_selected[2],
    		micro_tuning_selected[3],
    		micro_tuning_selected[4],
    		micro_tuning_selected[5],
    		micro_tuning_selected[6],
    		micro_tuning_selected[7],
    		micro_tuning_selected[8]);

	touch_pad_enabled = 1;

    codec_set_mute(0); //enable the sound

	while(!event_next_channel)
	{
		sampleCounter++;

		t_TIMING_BY_SAMPLE_EVERY_250_MS = TIMING_EVERY_250_MS;

		#define REVERB_UI_CMD_DECREASE_DECAY	1
		#define REVERB_UI_CMD_INCREASE_DECAY	2

		if (t_TIMING_BY_SAMPLE_EVERY_250_MS == 27) //4Hz
		{
			//ui_command = 0;

			//printf("btn_event_ext = %d\n", btn_event_ext);

			//map UI commands
			#ifdef BOARD_WHALE
			if(short_press_volume_plus) { ui_command = REVERB_UI_CMD_DECREASE_DECAY; short_press_volume_plus = 0; }
			if(short_press_volume_minus) { ui_command = REVERB_UI_CMD_INCREASE_DECAY; short_press_volume_minus = 0; }
			#endif
			#ifdef BOARD_GECHO
			if(btn_event_ext==BUTTON_EVENT_SHORT_PRESS+BUTTON_1) { ui_command = REVERB_UI_CMD_INCREASE_DECAY; btn_event_ext = 0; }
			if(btn_event_ext==BUTTON_EVENT_SHORT_PRESS+BUTTON_2) { ui_command = REVERB_UI_CMD_DECREASE_DECAY; btn_event_ext = 0; }
			#endif
		}

		#if 0
		//if (TIMING_BY_SAMPLE_EVERY_125_MS == 24) //8Hz
		if (t_TIMING_BY_SAMPLE_EVERY_250_MS == 31) //4Hz
		{
			bit_crusher_reverb_d += decay_direction;

			if(bit_crusher_reverb_d>BIT_CRUSHER_REVERB_MAX)
			{
				bit_crusher_reverb_d = BIT_CRUSHER_REVERB_MAX;
			}
			if(bit_crusher_reverb_d<BIT_CRUSHER_REVERB_MIN)
			{
				bit_crusher_reverb_d = BIT_CRUSHER_REVERB_MIN;
			}

			#ifdef BOARD_GECHO
			if(SENSOR_THRESHOLD_WHITE_8)
			{
				reverb_dynamic_loop_length = (1.0f-ir_res[3]) * 0.9f * (float)BIT_CRUSHER_REVERB_MAX;
				if(reverb_dynamic_loop_length<BIT_CRUSHER_REVERB_MIN)
				{
					reverb_dynamic_loop_length = BIT_CRUSHER_REVERB_MIN;
				}
				printf("reverb_dynamic_loop_length = %d\n", reverb_dynamic_loop_length);
			}
			else
			{
				reverb_dynamic_loop_length = bit_crusher_reverb_d;
			}
			#endif

			#ifdef REVERB_OCTAVE_UP_DOWN
			/*
			reverb_dynamic_loop_length_ext[0] = BIT_CRUSHER_REVERB_CALC_EXT(0, reverb_dynamic_loop_length);
			reverb_dynamic_loop_length_ext[1] = BIT_CRUSHER_REVERB_CALC_EXT(1, reverb_dynamic_loop_length);

			while(reverb_dynamic_loop_length_ext[0] > BIT_CRUSHER_REVERB_MAX_EXT(0))
			{
				printf("reverb_dynamic_loop_length_ext[0] > BIT_CRUSHER_REVERB_MAX_EXT(0): %d > %d\n", reverb_dynamic_loop_length_ext[0], BIT_CRUSHER_REVERB_MAX_EXT(0));
				reverb_dynamic_loop_length_ext[0] /= 2;
			}
			while(reverb_dynamic_loop_length_ext[1] > BIT_CRUSHER_REVERB_MAX_EXT(1))
			{
				printf("reverb_dynamic_loop_length_ext[1] > BIT_CRUSHER_REVERB_MAX_EXT(1): %d > %d\n", reverb_dynamic_loop_length_ext[1], BIT_CRUSHER_REVERB_MAX_EXT(1));
				reverb_dynamic_loop_length_ext[1] /= 2;
			}
			printf("reverb_dynamic_loop_length_ext[0-1] = %d,%d, BIT_CRUSHER_REVERB_MAX_EXT(0-1) = %d,%d\n", reverb_dynamic_loop_length_ext[0], reverb_dynamic_loop_length_ext[1], BIT_CRUSHER_REVERB_MAX_EXT(0), BIT_CRUSHER_REVERB_MAX_EXT(1));
			*/
			#endif

			#ifdef REVERB_POLYPHONIC

			if(!midi_override)
			{
				reverb_dynamic_loop_length_ext[0] = reverb_dynamic_loop_length / 2;
				reverb_dynamic_loop_length_ext[1] = reverb_dynamic_loop_length * 2;

				if(reverb_dynamic_loop_length_ext[0]<BIT_CRUSHER_REVERB_MIN_EXT(0))
				{
					reverb_dynamic_loop_length_ext[0] = BIT_CRUSHER_REVERB_MIN_EXT(0);
				}
				if(reverb_dynamic_loop_length_ext[0]>BIT_CRUSHER_REVERB_MAX_EXT(0))
				{
					reverb_dynamic_loop_length_ext[0] = BIT_CRUSHER_REVERB_MAX_EXT(0);
				}

				if(reverb_dynamic_loop_length_ext[1]<BIT_CRUSHER_REVERB_MIN_EXT(1))
				{
					reverb_dynamic_loop_length_ext[1] = BIT_CRUSHER_REVERB_MIN_EXT(1);
				}
				if(reverb_dynamic_loop_length_ext[1]>BIT_CRUSHER_REVERB_MAX_EXT(1))
				{
					reverb_dynamic_loop_length_ext[1] = BIT_CRUSHER_REVERB_MAX_EXT(1);
				}

			}
			#endif
		}
		#endif

		#if 0
		if (t_TIMING_BY_SAMPLE_EVERY_250_MS == 29) //4Hz
		{
			if(event_channel_options) //stops decay
			{
				decay_direction = 0; //DECAY_DIR_DEFAULT;
				event_channel_options = 0;
				//printf("Reverb(): decay_direction = 0\n");
			}
			if(ui_command==REVERB_UI_CMD_DECREASE_DECAY) //as delay length decreases, perceived tone goes up
			{
				decay_direction--;
				if(decay_direction < DECAY_DIR_FROM)
				{
					decay_direction = DECAY_DIR_FROM;
				}
				//#ifdef BOARD_GECHO
				//indicate_context_setting(SETTINGS_INDICATOR_ANIMATE_LEFT_8, 1, 50);
				//#endif
				//printf("Reverb(): decay_direction-- %d\n", decay_direction);
			}
			if(ui_command==REVERB_UI_CMD_INCREASE_DECAY) //as delay length increases, perceived tone goes low
			{
				decay_direction++;
				if(decay_direction > DECAY_DIR_TO)
				{
					decay_direction = DECAY_DIR_TO;
				}
				//#ifdef BOARD_GECHO
				//indicate_context_setting(SETTINGS_INDICATOR_ANIMATE_RIGHT_8, 1, 50);
				//#endif
				//printf("Reverb(): decay_direction++ %d\n", decay_direction);
			}
			#ifdef BOARD_GECHO
			if(!decay_direction) { LED_R8_set_byte(0x18); }
			else if(decay_direction == -1) { LED_R8_set_byte(0x10); }
			else if(decay_direction == -2) { LED_R8_set_byte(0x20); }
			else if(decay_direction == -3) { LED_R8_set_byte(0x40); }
			else if(decay_direction == -4) { LED_R8_set_byte(0x80); }
			else if(decay_direction == 1) { LED_R8_set_byte(0x08); }
			else if(decay_direction == 2) { LED_R8_set_byte(0x04); }
			else if(decay_direction == 3) { LED_R8_set_byte(0x02); }
			else if(decay_direction == 4) { LED_R8_set_byte(0x01); }
			#endif
		}
		#endif

		#ifdef BOARD_GECHO
		if (t_TIMING_BY_SAMPLE_EVERY_250_MS == 12695/2) //after 0.5s
		{
			LED_R8_all_OFF();
		}
		#endif

		//if (t_TIMING_BY_SAMPLE_EVERY_250_MS == 27) //4Hz
		if (TIMING_EVERY_100_MS == 33) //10Hz
		{
			if(acc_res[1]-acc_calibrated[1]+20 > 0)
			{
				ADC_LPF_ALPHA = (acc_res[1]-acc_calibrated[1]+20) / 100;
				if(ADC_LPF_ALPHA > 0.99)
				{
					ADC_LPF_ALPHA = 0.99;
				}
				if(ADC_LPF_ALPHA < 0.01)
				{
					ADC_LPF_ALPHA = 0.01;
				}
				//printf("acc_res[1] = %f, ADC_LPF_ALPHA = %f\n", acc_res[1], ADC_LPF_ALPHA);
			}
			//else if(acc_res[1]>-0.2f)  { ADC_LPF_ALPHA = 0.4f; }
			//else if(acc_res[1]>-0.3f)  { ADC_LPF_ALPHA = 0.3f; }
			//else if(acc_res[1]>-0.45f) { ADC_LPF_ALPHA = 0.2f; }
			//else if(acc_res[1]>-0.65f) { ADC_LPF_ALPHA = 0.1f; }
			//else if(acc_res[1]>-0.85f) { ADC_LPF_ALPHA = 0.05f;}
		}

		/*
		//if (TIMING_BY_SAMPLE_EVERY_125_MS == 26) //8Hz
		if (t_TIMING_BY_SAMPLE_EVERY_250_MS == 26) //4Hz
		{
			printf("bit_crusher_reverb_d=%d,len=%d\n",bit_crusher_reverb_d,reverb_dynamic_loop_length);
		}
		*/

		//i2s_pop_sample(I2S_NUM, (char*)&ADC_sample, portMAX_DELAY);
		i2s_read(I2S_NUM, (void*)&ADC_sample, 4, &i2s_bytes_rw, portMAX_DELAY);

		/*
		if(sampleCounter%bit_crusher_div==0)
		{
			ADC_sample0 = ADC_sample;
		}
		else
		{
			ADC_sample = ADC_sample0;
		}
		*/

		#ifdef LPF_ENABLED
		sample_lpf[0] = sample_lpf[0] + ADC_LPF_ALPHA * ((float)((int16_t)ADC_sample) - sample_lpf[0]);
		sample_mix = sample_lpf[0] * SAMPLE_VOLUME_REVERB; //apply volume
		#else
		sample_mix = (float)((int16_t)ADC_sample);
		sample_mix = sample_mix * SAMPLE_VOLUME_REVERB; //apply volume
		#endif


		//sample32 = (add_echo((int16_t)(sample_mix))) << 16;
		//sample32 = (add_reverb((int16_t)(sample_mix))) << 16;
		if(extended_buffers)
		{
			//sample32 = (add_echo(add_reverb_ext(add_reverb((int16_t)(sample_mix))))) << 16;
			//sample32 = (add_echo(add_reverb((int16_t)(sample_mix)) + add_reverb_ext((int16_t)(sample_mix)))) << 16; //does not sound as expected
			//sample32 = (add_echo(add_reverb_ext_all3((int16_t)(sample_mix)))) << 16;
			#ifdef REVERB_POLYPHONIC
			sample32 = (add_reverb_ext_all9(add_echo((int16_t)sample_mix))) << 16;
			#else
			sample32 = (add_reverb_ext_all3(add_echo((int16_t)sample_mix))) << 16;
			#endif
		}
		#if !defined(REVERB_POLYPHONIC) && !defined(REVERB_OCTAVE_UP_DOWN)
		else
		{
			sample32 = (add_reverb(add_echo((int16_t)(sample_mix)))) << 16;
			//sample32 = (add_echo(add_reverb((int16_t)(sample_mix)))) << 16;
			//sample32 = add_reverb((int16_t)(sample_mix)) << 16;
		}
		#endif

		#ifdef LPF_ENABLED
		sample_lpf[1] = sample_lpf[1] + ADC_LPF_ALPHA * ((float)((int16_t)(ADC_sample>>16)) - sample_lpf[1]);
		sample_mix = sample_lpf[1] * SAMPLE_VOLUME_REVERB; //apply volume
		#else
        //there is only one microphone in wingdrum
		//sample_mix = (float)((int16_t)(ADC_sample>>16));
        //sample_mix = sample_mix * SAMPLE_VOLUME_REVERB; //apply volume
		#endif

        //sample32 += add_echo((int16_t)(sample_mix));
        //sample32 += add_reverb((int16_t)(sample_mix));
		if(extended_buffers)
		{
			//sample32 += add_echo(add_reverb_ext(add_reverb((int16_t)(sample_mix))));
			//sample32 += add_echo(add_reverb((int16_t)(sample_mix)) + add_reverb_ext((int16_t)(sample_mix))); //does not sound as expected
			//sample32 += add_echo(add_reverb_ext_all3((int16_t)(sample_mix)));
			#ifdef REVERB_POLYPHONIC
			sample32 += add_reverb_ext_all9(add_echo((int16_t)sample_mix));
			#else
			sample32 += add_reverb_ext_all3(add_echo((int16_t)sample_mix));
			#endif
		}
		#if !defined(REVERB_POLYPHONIC) && !defined(REVERB_OCTAVE_UP_DOWN)
		else
		{
			sample32 += add_reverb(add_echo((int16_t)(sample_mix)));
			//sample32 += add_echo(add_reverb((int16_t)(sample_mix)));
			//sample32 += add_reverb((int16_t)(sample_mix));
		}
		#endif

		//i2s_push_sample(I2S_NUM, (char *)&sample32, portMAX_DELAY);
		i2s_write(I2S_NUM, (void*)&sample32, 4, &i2s_bytes_rw, portMAX_DELAY);

		#ifdef CLEAR_OLD_ECHO_DATA
		if (t_TIMING_BY_SAMPLE_EVERY_250_MS == 37) //4Hz
		{
			if(echo_dynamic_loop_length0 < echo_dynamic_loop_length)
			{
				//printf("Cleaning echo buffer from echo_dynamic_loop_length0 to echo_dynamic_loop_length\n");
				//echo_length_updated = echo_dynamic_loop_length0;
				//echo_dynamic_loop_length0 = echo_dynamic_loop_length;
			//}
			//if(echo_length_updated)
			//{
				//memset(echo_buffer+echo_dynamic_loop_length0*sizeof(int16_t),0,(echo_dynamic_loop_length-echo_dynamic_loop_length0)*sizeof(int16_t)); //clear memory
				//echo_length_updated = 0;
				echo_skip_samples = echo_dynamic_loop_length - echo_dynamic_loop_length0;
				echo_skip_samples_from = echo_dynamic_loop_length0;
			}
			echo_dynamic_loop_length0 = echo_dynamic_loop_length;
		}
		#endif

		if (TIMING_EVERY_20_MS == 33) //50Hz
		{
			if(limiter_coeff < DYNAMIC_LIMITER_COEFF_DEFAULT)
			{
				limiter_coeff += DYNAMIC_LIMITER_COEFF_DEFAULT / 20; //limiter will fully recover within 0.4 second
			}
		}

		if (TIMING_EVERY_10_MS == 459) //100Hz
		{
			if(note_updated>=0)
			{
				if(auto_power_off < AUTO_POWER_OFF_VOLUME_RAMP)
				{
					auto_power_off_canceled = 1;
				}
				auto_power_off = global_settings.AUTO_POWER_OFF_TIMEOUT_REVERB; //extend the auto power off timeout

				//if(MIDI_last_chord[0])
				//if(new_MIDI_note[note_updated])
				{
					//printf("Reverb(): new_MIDI_note[note_updated] = %d, notes_on = %d\n", new_MIDI_note[note_updated], notes_on);
					note_updated = -1;

					//printf("Reverb(): new_MIDI_note[] = (%d,%d,%d,%d,%d,%d,%d,%d,%d)\n",
					//	new_MIDI_note[0],new_MIDI_note[1],new_MIDI_note[2],new_MIDI_note[3],new_MIDI_note[4],
					//	new_MIDI_note[5],new_MIDI_note[6],new_MIDI_note[7],new_MIDI_note[8]);

					//printf("Reverb(): MIDI_pitch_base[] = (%f,%f,%f,%f)\n",
					//	MIDI_pitch_base[0],MIDI_pitch_base[1],MIDI_pitch_base[2],MIDI_pitch_base[3]);

					#ifndef REVERB_POLYPHONIC
					reverb_dynamic_loop_length = (float)SAMPLE_RATE_DEFAULT / MIDI_note_to_freq_ft(MIDI_pitch_base[0]+MIDI_pitch_bend);
					//printf("Reverb(): [1]reverb_dynamic_loop_length => %d\n", reverb_dynamic_loop_length);

					if(reverb_dynamic_loop_length<BIT_CRUSHER_REVERB_MIN)
					{
						reverb_dynamic_loop_length = BIT_CRUSHER_REVERB_MIN;
						//printf("Reverb(): [2]reverb_dynamic_loop_length<BIT_CRUSHER_REVERB_MAX (%d<%d), => %d\n", reverb_dynamic_loop_length, BIT_CRUSHER_REVERB_MIN, BIT_CRUSHER_REVERB_MIN);
					}
					if(reverb_dynamic_loop_length>BIT_CRUSHER_REVERB_MAX)
					{
						reverb_dynamic_loop_length = BIT_CRUSHER_REVERB_MAX;
						//printf("Reverb(): [3]reverb_dynamic_loop_length>BIT_CRUSHER_REVERB_MAX (%d>%d), => %d\n", reverb_dynamic_loop_length, BIT_CRUSHER_REVERB_MAX, BIT_CRUSHER_REVERB_MAX);
					}
					#endif

					#ifdef REVERB_OCTAVE_UP_DOWN

					reverb_dynamic_loop_length_ext[0] = BIT_CRUSHER_REVERB_CALC_EXT(0, reverb_dynamic_loop_length);
					reverb_dynamic_loop_length_ext[1] = BIT_CRUSHER_REVERB_CALC_EXT(1, reverb_dynamic_loop_length);
					//printf("Reverb(): [4]MIDI note = %d, reverb_dynamic_loop_length = %d, ext = %d, %d\n", new_MIDI_note[note_updated], reverb_dynamic_loop_length, reverb_dynamic_loop_length_ext[0], reverb_dynamic_loop_length_ext[1]);

					while(reverb_dynamic_loop_length_ext[0] > BIT_CRUSHER_REVERB_MAX_EXT(0))
					{
						//printf("reverb_dynamic_loop_length_ext[0] > BIT_CRUSHER_REVERB_MAX_EXT(0): %d > %d\n", reverb_dynamic_loop_length_ext[0], BIT_CRUSHER_REVERB_MAX_EXT(0));
						reverb_dynamic_loop_length_ext[0] /= 2;
					}
					while(reverb_dynamic_loop_length_ext[1] > BIT_CRUSHER_REVERB_MAX_EXT(1))
					{
						//printf("reverb_dynamic_loop_length_ext[1] > BIT_CRUSHER_REVERB_MAX_EXT(1): %d > %d\n", reverb_dynamic_loop_length_ext[1], BIT_CRUSHER_REVERB_MAX_EXT(1));
						reverb_dynamic_loop_length_ext[1] /= 2;
					}
					//printf("reverb_dynamic_loop_length_ext[0-1] = %d,%d, BIT_CRUSHER_REVERB_MAX_EXT(0-1) = %d,%d\n", reverb_dynamic_loop_length_ext[0], reverb_dynamic_loop_length_ext[1], BIT_CRUSHER_REVERB_MAX_EXT(0), BIT_CRUSHER_REVERB_MAX_EXT(1));
					printf("reverb_dynamic_loop_length/_ext[...] / max = %d,%d,%d / %d,%d,%d\n", reverb_dynamic_loop_length_ext[0], reverb_dynamic_loop_length, reverb_dynamic_loop_length_ext[1], BIT_CRUSHER_REVERB_MAX_EXT(0), BIT_CRUSHER_REVERB_MAX, BIT_CRUSHER_REVERB_MAX_EXT(1));

					#endif

					#ifdef REVERB_POLYPHONIC

					/*
					reverb_dynamic_loop_length_ext[0] = (float)SAMPLE_RATE_DEFAULT / MIDI_note_to_freq_ft(MIDI_pitch_base[0]+MIDI_pitch_bend);
					reverb_dynamic_loop_length_ext[1] = (float)SAMPLE_RATE_DEFAULT / MIDI_note_to_freq_ft(MIDI_pitch_base[1]+MIDI_pitch_bend);
					reverb_dynamic_loop_length_ext[2] = (float)SAMPLE_RATE_DEFAULT / MIDI_note_to_freq_ft(MIDI_pitch_base[2]+MIDI_pitch_bend);

					#if REVERB_BUFFERS_EXT==4 || REVERB_BUFFERS_EXT==5
					reverb_dynamic_loop_length_ext[3] = reverb_dynamic_loop_length_ext[0] / 2;
					#endif
					#if REVERB_BUFFERS_EXT==5
					reverb_dynamic_loop_length_ext[4] = reverb_dynamic_loop_length_ext[0] * 2;
					#endif

					#if REVERB_BUFFERS_EXT==6 || REVERB_BUFFERS_EXT==9
					reverb_dynamic_loop_length_ext[3] = reverb_dynamic_loop_length_ext[0] / 2;
					reverb_dynamic_loop_length_ext[4] = reverb_dynamic_loop_length_ext[1] / 2;
					reverb_dynamic_loop_length_ext[5] = reverb_dynamic_loop_length_ext[2] / 2;
					#endif

					#if REVERB_BUFFERS_EXT==9
					reverb_dynamic_loop_length_ext[6] = reverb_dynamic_loop_length_ext[0] * 2;
					reverb_dynamic_loop_length_ext[7] = reverb_dynamic_loop_length_ext[1] * 2;
					reverb_dynamic_loop_length_ext[8] = reverb_dynamic_loop_length_ext[2] * 2;
					#endif
					*/

					if(notes_on==1 && note_updated<0)
					{
						//MIDI_pitch_base[0] = (float)new_MIDI_note[note_updated] - 48.0f;//MIDI_last_chord[0];
						reverb_dynamic_loop_length_ext[0] = (float)SAMPLE_RATE_DEFAULT / MIDI_note_to_freq_ft(MIDI_pitch_base[MIDI_pitch_base_ptr]);
						reverb_dynamic_loop_length_ext[1] = reverb_dynamic_loop_length_ext[0] * 2;
						reverb_dynamic_loop_length_ext[2] = reverb_dynamic_loop_length_ext[0] / 2;
						reverb_dynamic_loop_length_ext[3] = reverb_dynamic_loop_length_ext[0] / 4;

						/*
						printf("notes=%f / buffers=%d,(%d,%d,%d)\n",
							MIDI_pitch_base[MIDI_pitch_base_ptr],
							reverb_dynamic_loop_length_ext[0],
							reverb_dynamic_loop_length_ext[1],
							reverb_dynamic_loop_length_ext[2],
							reverb_dynamic_loop_length_ext[3]);
						*/
					}
					else if(notes_on==2 && note_updated<0)
					{
						reverb_dynamic_loop_length_ext[0] = (float)SAMPLE_RATE_DEFAULT / MIDI_note_to_freq_ft(MIDI_pitch_base[(MIDI_pitch_base_ptr+3)%4]);
						reverb_dynamic_loop_length_ext[1] = (float)SAMPLE_RATE_DEFAULT / MIDI_note_to_freq_ft(MIDI_pitch_base[MIDI_pitch_base_ptr]);
						reverb_dynamic_loop_length_ext[2] = reverb_dynamic_loop_length_ext[0] / 2;
						reverb_dynamic_loop_length_ext[3] = reverb_dynamic_loop_length_ext[1] / 2;

						/*
						printf("notes=%f,%f / buffers=%d,%d,(%d,%d)\n",
							MIDI_pitch_base[(MIDI_pitch_base_ptr+3)%4],
							MIDI_pitch_base[MIDI_pitch_base_ptr],
							reverb_dynamic_loop_length_ext[0],
							reverb_dynamic_loop_length_ext[1],
							reverb_dynamic_loop_length_ext[2],
							reverb_dynamic_loop_length_ext[3]);
						*/
					}
					else if(notes_on==3 && note_updated<0)
					{
						int longest_buffer = 0;
						reverb_dynamic_loop_length_ext[0] = (float)SAMPLE_RATE_DEFAULT / MIDI_note_to_freq_ft(MIDI_pitch_base[(MIDI_pitch_base_ptr+2)%4]);
						reverb_dynamic_loop_length_ext[1] = (float)SAMPLE_RATE_DEFAULT / MIDI_note_to_freq_ft(MIDI_pitch_base[(MIDI_pitch_base_ptr+3)%4]);
						reverb_dynamic_loop_length_ext[2] = (float)SAMPLE_RATE_DEFAULT / MIDI_note_to_freq_ft(MIDI_pitch_base[MIDI_pitch_base_ptr]);
						if(reverb_dynamic_loop_length_ext[0]>longest_buffer) longest_buffer = reverb_dynamic_loop_length_ext[0];
						if(reverb_dynamic_loop_length_ext[1]>longest_buffer) longest_buffer = reverb_dynamic_loop_length_ext[1];
						if(reverb_dynamic_loop_length_ext[2]>longest_buffer) longest_buffer = reverb_dynamic_loop_length_ext[2];
						reverb_dynamic_loop_length_ext[3] = longest_buffer / 2;//reverb_dynamic_loop_length_ext[0] / 2;

						/*
						printf("notes=%f,%f,%f / buffers=%d,%d,%d,(%d)\n",
							MIDI_pitch_base[(MIDI_pitch_base_ptr+2)%4],
							MIDI_pitch_base[(MIDI_pitch_base_ptr+3)%4],
							MIDI_pitch_base[MIDI_pitch_base_ptr],
							reverb_dynamic_loop_length_ext[0],
							reverb_dynamic_loop_length_ext[1],
							reverb_dynamic_loop_length_ext[2],
							reverb_dynamic_loop_length_ext[3]);
						*/
					}
					else if(notes_on==4 && note_updated<0)
					{
						reverb_dynamic_loop_length_ext[0] = (float)SAMPLE_RATE_DEFAULT / MIDI_note_to_freq_ft(MIDI_pitch_base[0]);
						reverb_dynamic_loop_length_ext[1] = (float)SAMPLE_RATE_DEFAULT / MIDI_note_to_freq_ft(MIDI_pitch_base[1]);
						reverb_dynamic_loop_length_ext[2] = (float)SAMPLE_RATE_DEFAULT / MIDI_note_to_freq_ft(MIDI_pitch_base[2]);
						reverb_dynamic_loop_length_ext[3] = (float)SAMPLE_RATE_DEFAULT / MIDI_note_to_freq_ft(MIDI_pitch_base[3]);

						/*
						printf("notes=%f,%f,%f,%f / buffers=%d,%d,%d,%d\n",
							MIDI_pitch_base[0],
							MIDI_pitch_base[1],
							MIDI_pitch_base[2],
							MIDI_pitch_base[3],
							reverb_dynamic_loop_length_ext[0],
							reverb_dynamic_loop_length_ext[1],
							reverb_dynamic_loop_length_ext[2],
							reverb_dynamic_loop_length_ext[3]);
						*/
					}

					for(int r=0;r<REVERB_BUFFERS_EXT;r++)
					{
						//if(reverb_dynamic_loop_length_ext[r]<BIT_CRUSHER_REVERB_MIN_EXT9) { reverb_dynamic_loop_length_ext[r] = BIT_CRUSHER_REVERB_MIN_EXT9; }
						if(reverb_dynamic_loop_length_ext[r]>BIT_CRUSHER_REVERB_MAX_EXT9) { reverb_dynamic_loop_length_ext[r] = BIT_CRUSHER_REVERB_MAX_EXT9; }
						//reverb_buffer_ptr0_ext[r] = 0;
						//memset(reverb_buffer_ext[r], 0, reverb_dynamic_loop_length_ext[r] * sizeof(int16_t));
					}

					/*
					//if(MIDI_last_chord[1])
					{
						//MIDI_to_LED(MIDI_last_chord[1], 1);

						//MIDI_pitch_base[1] = (float)MIDI_last_chord[1];
						reverb_dynamic_loop_length_ext[0] = (float)SAMPLE_RATE_DEFAULT / MIDI_note_to_freq_ft(MIDI_pitch_base[1]+MIDI_pitch_bend);

						if(reverb_dynamic_loop_length_ext[0]<BIT_CRUSHER_REVERB_MIN_EXT(0))
						{
							reverb_dynamic_loop_length_ext[0] = BIT_CRUSHER_REVERB_MIN_EXT(0);
						}
						if(reverb_dynamic_loop_length_ext[0]>BIT_CRUSHER_REVERB_MAX_EXT(0))
						{
							reverb_dynamic_loop_length_ext[0] = BIT_CRUSHER_REVERB_MAX_EXT(0);
						}
					}

					//if(MIDI_last_chord[2])
					{
						//MIDI_to_LED(MIDI_last_chord[2], 1);

						//MIDI_pitch_base[2] = (float)MIDI_last_chord[2];
						reverb_dynamic_loop_length_ext[1] = (float)SAMPLE_RATE_DEFAULT / MIDI_note_to_freq_ft(MIDI_pitch_base[2]+MIDI_pitch_bend);

						if(reverb_dynamic_loop_length_ext[1]<BIT_CRUSHER_REVERB_MIN_EXT(1))
						{
							reverb_dynamic_loop_length_ext[1] = BIT_CRUSHER_REVERB_MIN_EXT(1);
						}
						if(reverb_dynamic_loop_length_ext[1]>BIT_CRUSHER_REVERB_MAX_EXT(1))
						{
							reverb_dynamic_loop_length_ext[1] = BIT_CRUSHER_REVERB_MAX_EXT(1);
						}
					}

					printf("Reverb(): MIDI pitch base = %f-%f-%f, reverb_dynamic_loop_length = %d, ext = %d, %d\n", MIDI_pitch_base[0], MIDI_pitch_base[1], MIDI_pitch_base[2], reverb_dynamic_loop_length, reverb_dynamic_loop_length_ext[0], reverb_dynamic_loop_length_ext[1]);
					*/

					#endif
				}

				//MIDI_notes_updated = 0;
			}
			#if 0
			if(MIDI_controllers_active_CC)
			{
				if(MIDI_controllers_updated==MIDI_WHEEL_CONTROLLER_CC_UPDATED)
				{
					//p->texture = ((float)MIDI_ctrl[MIDI_WHEEL_CONTROLLER_CC]/127.0f) * CLOUDS_MIDI_TEXTURE_MAX;
					//printf("Reverb(): MIDI_WHEEL_CONTROLLER_CC => %d, texture = %f\n", MIDI_ctrl[MIDI_WHEEL_CONTROLLER_CC], p->texture);

					if(MIDI_ctrl[MIDI_WHEEL_CONTROLLER_CC]<8)
					{
						echo_dynamic_loop_length = (DELAY_BY_TEMPO * 3) / 2;
					}
					else if(MIDI_ctrl[MIDI_WHEEL_CONTROLLER_CC]<16)
					{
						echo_dynamic_loop_length = DELAY_BY_TEMPO;
					}
					else if(MIDI_ctrl[MIDI_WHEEL_CONTROLLER_CC]<24)
					{
						echo_dynamic_loop_length = DELAY_BY_TEMPO / 3 * 2;
					}
					else if(MIDI_ctrl[MIDI_WHEEL_CONTROLLER_CC]<32)
					{
						echo_dynamic_loop_length = DELAY_BY_TEMPO / 2;
					}
					else if(MIDI_ctrl[MIDI_WHEEL_CONTROLLER_CC]<40)
					{
						echo_dynamic_loop_length = DELAY_BY_TEMPO / 3;
					}
					else if(MIDI_ctrl[MIDI_WHEEL_CONTROLLER_CC]<48)
					{
						echo_dynamic_loop_length = DELAY_BY_TEMPO / 4;
					}
					else if(MIDI_ctrl[MIDI_WHEEL_CONTROLLER_CC]<56)
					{
						echo_dynamic_loop_length = DELAY_BY_TEMPO / 5;
					}
					else if(MIDI_ctrl[MIDI_WHEEL_CONTROLLER_CC]<64)
					{
						echo_dynamic_loop_length = DELAY_BY_TEMPO / 6;
					}
					else if(MIDI_ctrl[MIDI_WHEEL_CONTROLLER_CC]<72)
					{
						echo_dynamic_loop_length = DELAY_BY_TEMPO / 7;
					}
					else if(MIDI_ctrl[MIDI_WHEEL_CONTROLLER_CC]<80)
					{
						echo_dynamic_loop_length = DELAY_BY_TEMPO / 8;
					}
					else
					{
						echo_dynamic_loop_length = DELAY_BY_TEMPO / (8 + MIDI_ctrl[MIDI_WHEEL_CONTROLLER_CC] - 80);
					}

					if(tempo_bpm < global_settings.TEMPO_BPM_DEFAULT)
					{
						echo_dynamic_loop_length /= 2;
					}

					MIDI_controllers_updated = 0;
				}
			}
			if(MIDI_controllers_active_PB)
			{
				if(MIDI_controllers_updated==MIDI_WHEEL_CONTROLLER_PB_UPDATED)
				{
					MIDI_pitch_bend = ((float)MIDI_ctrl[MIDI_WHEEL_CONTROLLER_PB]/127.0f) * 2.0f - 1.0f;

					reverb_dynamic_loop_length = (float)SAMPLE_RATE_DEFAULT / MIDI_note_to_freq_ft(MIDI_pitch_base[0]+MIDI_pitch_bend);

					if(reverb_dynamic_loop_length<BIT_CRUSHER_REVERB_MIN)
					{
						reverb_dynamic_loop_length = BIT_CRUSHER_REVERB_MIN;
					}
					if(reverb_dynamic_loop_length>BIT_CRUSHER_REVERB_MAX)
					{
						reverb_dynamic_loop_length = BIT_CRUSHER_REVERB_MAX;
					}

					#ifdef REVERB_OCTAVE_UP_DOWN
					reverb_dynamic_loop_length_ext[0] = BIT_CRUSHER_REVERB_CALC_EXT(0, reverb_dynamic_loop_length);
					reverb_dynamic_loop_length_ext[1] = BIT_CRUSHER_REVERB_CALC_EXT(1, reverb_dynamic_loop_length);
					#endif

					#ifdef REVERB_POLYPHONIC

					reverb_dynamic_loop_length_ext[0] = (float)SAMPLE_RATE_DEFAULT / MIDI_note_to_freq_ft(MIDI_pitch_base[1]+MIDI_pitch_bend);

					if(reverb_dynamic_loop_length_ext[0]<BIT_CRUSHER_REVERB_MIN_EXT(0))
					{
						reverb_dynamic_loop_length_ext[0] = BIT_CRUSHER_REVERB_MIN_EXT(0);
					}
					if(reverb_dynamic_loop_length_ext[0]>BIT_CRUSHER_REVERB_MAX_EXT(0))
					{
						reverb_dynamic_loop_length_ext[0] = BIT_CRUSHER_REVERB_MAX_EXT(0);
					}

					reverb_dynamic_loop_length_ext[1] = (float)SAMPLE_RATE_DEFAULT / MIDI_note_to_freq_ft(MIDI_pitch_base[2]+MIDI_pitch_bend);

					if(reverb_dynamic_loop_length_ext[1]<BIT_CRUSHER_REVERB_MIN_EXT(1))
					{
						reverb_dynamic_loop_length_ext[1] = BIT_CRUSHER_REVERB_MIN_EXT(1);
					}
					if(reverb_dynamic_loop_length_ext[1]>BIT_CRUSHER_REVERB_MAX_EXT(1))
					{
						reverb_dynamic_loop_length_ext[1] = BIT_CRUSHER_REVERB_MAX_EXT(1);
					}

					#endif

					//printf("Reverb(): MIDI_pitch_bend => %f, reverb_dynamic_loop_length = %d\n", MIDI_pitch_bend, reverb_dynamic_loop_length);
					MIDI_controllers_updated = 0;
				}
			}
			#endif
		}

		if (TIMING_EVERY_100_MS == 2087) //10Hz
		{
			check_led_blink_timeout();
		}

		if (TIMING_EVERY_500_MS == 7919) //2Hz
		{
			check_auto_power_off(global_settings.AUTO_POWER_OFF_TIMEOUT_REVERB);
		}
	}
}

#if 0
void decaying_reverb_with_wavetable()
{
	channel_init(0, 0, 0, 0, 0, 0, 0, 0, 0); //init without any features

    ECHO_MIXING_GAIN_MUL = 1; //amount of signal to feed back to echo loop, expressed as a fragment
    ECHO_MIXING_GAIN_DIV = 4; //e.g. if MUL=2 and DIV=3, it means 2/3 of signal is mixed in

	//int bit_crusher_reverb_d = BIT_CRUSHER_REVERB_DEFAULT;
	int bit_crusher_reverb_d = I2S_AUDIOFREQ / 200 + 1;
	//int bit_crusher_reverb_d = I2S_AUDIOFREQ / 150 + 1;
	//int bit_crusher_reverb_d = I2S_AUDIOFREQ / 80 + 2;
	//int bit_crusher_reverb_d = I2S_AUDIOFREQ / 40 + 1;
	//int bit_crusher_reverb_d = I2S_AUDIOFREQ / 20 + 2;
	//int bit_crusher_reverb_d = 8000;

    //int bit_crusher_reverb_d = 222;
    //int bit_crusher_reverb_d = 400;
    reverb_dynamic_loop_length = bit_crusher_reverb_d;

	#define DECAY_DIR_FROM		-4
	#define DECAY_DIR_TO		4
	#define DECAY_DIR_DEFAULT	1
	int decay_direction = DECAY_DIR_DEFAULT;

	//int lock_sensors = 0;

	#ifdef LPF_ENABLED
	ADC_LPF_ALPHA = 0.2f;
	#endif

//============================================================== TONGUE DRUM ======================

#define SINETABLE_SIZE		10000 //32779 //50777 //32000
//#define W_STEP_DEFAULT		40 //200
#define W_STEP_DEFAULT		20 //200

#define SINETABLE_AMP		32767
int16_t *sinetable = (int16_t*)malloc(SINETABLE_SIZE*sizeof(int16_t));

#define VOICES	1
//#define VOICES	2
//#define VOICES	4

#if VOICES == 1
double w_ptr[VOICES] = {0}, w_step[VOICES] = {W_STEP_DEFAULT}, env_amp[VOICES] = {0};
int e_ptr[VOICES] = {-1};
#endif

#if VOICES == 2
double w_ptr[VOICES] = {0,0}, w_step[VOICES] = {W_STEP_DEFAULT,W_STEP_DEFAULT}, env_amp[VOICES] = {0,0};
int e_ptr[VOICES] = {-1,-1};
#endif

#if VOICES == 4
double w_ptr[VOICES] = {0,0,0,0}, w_step[VOICES] = {W_STEP_DEFAULT,W_STEP_DEFAULT,W_STEP_DEFAULT,W_STEP_DEFAULT}, env_amp[VOICES] = {0,0,0,0};
int e_ptr[VOICES] = {-1,-1,-1,-1};
#endif

double v, mixing_volume = 0.1f;
int i_ptr, next_voice = 0, vo;
int16_t d;

printf("----REVERB-------[init] Free heap before sinetable gen: %u\n", xPortGetFreeHeapSize());

printf("generating sinetable...\n");
for(int i=0;i<SINETABLE_SIZE;i++)
{
	v = sin(2*M_PI*(double)i/double(SINETABLE_SIZE));
	d = v * SINETABLE_AMP;
	sinetable[i] = d;
}

#define ENVTABLE_AMP		65000//4000
#define ENVTABLE_OFFSET		0//1500
#define ENVTABLE_SIZE		1000
#define ENV_DECAY_FACTOR	0.25 //0.3 //0.5f
#define ENV_DECAY_STEP_DIV	40

uint16_t *envtable = (uint16_t*)malloc(ENVTABLE_SIZE*sizeof(uint16_t));

printf("----REVERB-------[init] Free heap before envtable gen: %u\n", xPortGetFreeHeapSize());

printf("generating envtable...\n");

#define LINEAR_BIT	0 //500

#if LINEAR_BIT > 0
for(int i=0;i<LINEAR_BIT;i++)
{
	v = (double)i/(double)LINEAR_BIT;
	d = v * ENVTABLE_AMP + ENVTABLE_OFFSET;
	envtable[i] = d;
}
#endif
for(int i=LINEAR_BIT;i<ENVTABLE_SIZE;i++)
{
	v = pow(1.0f-ENV_DECAY_FACTOR,1.0f+(double)i/ENV_DECAY_STEP_DIV);
	d = v * ENVTABLE_AMP + ENVTABLE_OFFSET;
	envtable[i] = d;
}

//============================================================== TONGUE DRUM ======================

printf("----REVERB-------[loop] Free heap before loop start: %u\n", xPortGetFreeHeapSize());

	while(!event_next_channel)
	{
		sampleCounter++;

		#if 0
		//============================================================== TONGUE DRUM ======================
		//if (TIMING_EVERY_25_MS == 31) //40Hz
		if (TIMING_EVERY_5_MS == 31) //40Hz
		{
			if(note_updated>=0)
			{
				w_step[next_voice] = W_STEP_DEFAULT + note_updated * W_STEP_DEFAULT;
				w_ptr[next_voice] = 0;
				e_ptr[next_voice] = 0;
				env_amp[next_voice] = 1.0f;

				note_updated = -1;
				next_voice++;
				if(next_voice==VOICES)
				{
					next_voice = 0;
				}
			}
		}

		sample_mix_wt = 0;

		for(vo=0;vo<VOICES;vo++)
		{
			if(e_ptr[vo]>=0)
			{
				i_ptr = w_ptr[vo];
				sample_mix_wt += sinetable[i_ptr] * mixing_volume * env_amp[vo];

				w_ptr[vo] += w_step[vo];
				if(w_ptr[vo] > SINETABLE_SIZE)
				{
					//w_ptr[vo] = 0;
					w_ptr[vo] -= SINETABLE_SIZE;
				}
			}
		}

		//sample32 = ((int16_t)(sample_mix_wt)) << 16;
        //sample32 += (int16_t)(sample_mix_wt);

        //if(TIMING_EVERY_10_MS == 37)
        if(TIMING_EVERY_2_MS == 37)
        {
    		for(vo=0;vo<VOICES;vo++)
    		{
    			if(e_ptr[vo]>=0)
    			{
    				e_ptr[vo]++;

    				env_amp[vo] = envtable[e_ptr[vo]] / (double)(ENVTABLE_AMP+ENVTABLE_OFFSET);

    				if(e_ptr[vo]==ENVTABLE_SIZE-1)
    				{
    					e_ptr[vo] = -1;
    					env_amp[vo] = 0;
    				}
    			}
        	}
        }
        //============================================================== TONGUE DRUM ======================
		#endif

        t_TIMING_BY_SAMPLE_EVERY_250_MS = TIMING_EVERY_250_MS;

		#define REVERB_UI_CMD_DECREASE_DECAY	1
		#define REVERB_UI_CMD_INCREASE_DECAY	2

		if (t_TIMING_BY_SAMPLE_EVERY_250_MS == 27) //4Hz
		{
			//ui_command = 0;

			//printf("btn_event_ext = %d\n", btn_event_ext);

			//map UI commands
			#ifdef BOARD_WHALE
			if(short_press_volume_plus) { ui_command = REVERB_UI_CMD_DECREASE_DECAY; short_press_volume_plus = 0; }
			if(short_press_volume_minus) { ui_command = REVERB_UI_CMD_INCREASE_DECAY; short_press_volume_minus = 0; }
			#endif
			#ifdef BOARD_GECHO
			if(btn_event_ext==BUTTON_EVENT_SHORT_PRESS+BUTTON_1) { ui_command = REVERB_UI_CMD_INCREASE_DECAY; btn_event_ext = 0; }
			if(btn_event_ext==BUTTON_EVENT_SHORT_PRESS+BUTTON_2) { ui_command = REVERB_UI_CMD_DECREASE_DECAY; btn_event_ext = 0; }
			#endif
		}

		//if (TIMING_BY_SAMPLE_EVERY_125_MS == 24) //8Hz
		if (t_TIMING_BY_SAMPLE_EVERY_250_MS == 31) //4Hz
		{
			bit_crusher_reverb_d += decay_direction;

			if(bit_crusher_reverb_d>BIT_CRUSHER_REVERB_MAX)
			{
				bit_crusher_reverb_d = BIT_CRUSHER_REVERB_MAX;
			}
			if(bit_crusher_reverb_d<BIT_CRUSHER_REVERB_MIN)
			{
				bit_crusher_reverb_d = BIT_CRUSHER_REVERB_MIN;
			}
		}

		#if 0
		if (t_TIMING_BY_SAMPLE_EVERY_250_MS == 29) //4Hz
		{
			if(event_channel_options) //stops decay
			{
				decay_direction = 0; //DECAY_DIR_DEFAULT;
				event_channel_options = 0;
				//printf("Reverb(): decay_direction = 0\n");
			}
			if(ui_command==REVERB_UI_CMD_DECREASE_DECAY) //as delay length decreases, perceived tone goes up
			{
				decay_direction--;
				if(decay_direction < DECAY_DIR_FROM)
				{
					decay_direction = DECAY_DIR_FROM;
				}
			}
			if(ui_command==REVERB_UI_CMD_INCREASE_DECAY) //as delay length increases, perceived tone goes low
			{
				decay_direction++;
				if(decay_direction > DECAY_DIR_TO)
				{
					decay_direction = DECAY_DIR_TO;
				}
			}
		}
		#endif

		//i2s_pop_sample(I2S_NUM, (char*)&ADC_sample, portMAX_DELAY);
		i2s_read(I2S_NUM, (void*)&ADC_sample, 4, &i2s_bytes_rw, portMAX_DELAY);

		#ifdef LPF_ENABLED
		//LPF enabled
		sample_lpf[0] = sample_lpf[0] + ADC_LPF_ALPHA * ((float)((int16_t)ADC_sample) - sample_lpf[0]);
		sample_mix = sample_lpf[0] * SAMPLE_VOLUME_REVERB; //apply volume
		#else
		sample_mix = (int16_t)ADC_sample;
		sample_mix = sample_mix * SAMPLE_VOLUME_REVERB; //apply volume
		#endif

		//sample_mix += sample_mix_wt; //add wavetable sounds
		//sample_mix = sample_mix_wt; //only wavetable sounds

		//sample32 = (add_echo((int16_t)(sample_mix))) << 16;
		//sample32 = (add_reverb((int16_t)(sample_mix))) << 16;
		sample32 = (add_echo(add_reverb((int16_t)(sample_mix)))) << 16;
		//sample32 = ((int16_t)(sample_mix)) << 16;

		#ifdef LPF_ENABLED
		//LPF enabled
		sample_lpf[1] = sample_lpf[1] + ADC_LPF_ALPHA * ((float)((int16_t)(ADC_sample>>16)) - sample_lpf[1]);
		sample_mix = sample_lpf[1] * SAMPLE_VOLUME_REVERB; //apply volume
		#else
        //there is only one microphone in wingdrum
        //sample_mix = (int16_t)(ADC_sample>>16);
        //sample_mix = sample_mix * SAMPLE_VOLUME_REVERB; //apply volume
		#endif

        //sample32 += add_echo((int16_t)(sample_mix));
        //sample32 += add_reverb((int16_t)(sample_mix));
        sample32 += add_echo(add_reverb((int16_t)(sample_mix)));
        //sample32 += (int16_t)(sample_mix);

		i2s_write(I2S_NUM, (void*)&sample32, 4, &i2s_bytes_rw, portMAX_DELAY);

		if (TIMING_EVERY_20_MS == 33) //50Hz
		{
			if(limiter_coeff < DYNAMIC_LIMITER_COEFF_DEFAULT)
			{
				limiter_coeff += DYNAMIC_LIMITER_COEFF_DEFAULT / 20; //limiter will fully recover within 0.4 second
			}
		}

		if (TIMING_EVERY_100_MS == 2087) //10Hz
		{
			check_led_blink_timeout();
		}

		if (TIMING_EVERY_500_MS == 7919) //2Hz
		{
			check_auto_power_off();
		}
	}
}
#endif
