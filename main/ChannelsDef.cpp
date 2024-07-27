/*
 * ChannelsDef.cpp
 *
 *  Copyright 2024 Phonicbloom Ltd.
 *
 *  Created on: Feb 02, 2019
 *      Author: mario
 *
 *  This file can be used within the terms of GNU GPLv3 license: https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 *  Find more information at:
 *  http://phonicbloom.com/
 *  http://gechologic.com/
 *
 */

#include <ChannelsDef.h>
#include <InitChannels.h>

#include <Interface.h>
#include <hw/codec.h>
#include <hw/midi.h>
#include <hw/signals.h>
#include <hw/gpio.h>

#include <string.h>

#include <dsp/Reverb.h>

void channel_white_noise()
{
	program_settings_reset();

	float r;

	while(!event_next_channel)
	{
		if (!(sampleCounter & 0x00000001)) //right channel
		{
			r = PseudoRNG1a_next_float();
			memcpy(&random_value, &r, sizeof(random_value));

			sample_f[0] = (float)((int16_t)(random_value >> 16)) * OpAmp_ADC12_signal_conversion_factor;
			sample_mix = sample_f[0] * COMPUTED_SAMPLES_MIXING_VOLUME;
			sample_i16 = (int16_t)(sample_mix * SAMPLE_VOLUME);
		}
		else
		{
			sample_f[1] = (float)((int16_t)random_value) * OpAmp_ADC12_signal_conversion_factor;
			sample_mix = sample_f[1] * COMPUTED_SAMPLES_MIXING_VOLUME;
			sample_i16 = (int16_t)(sample_mix * SAMPLE_VOLUME);
		}

		if (sampleCounter & 0x00000001) //left channel
		{
			sample32 += sample_i16 << 16;
			i2s_write(I2S_NUM, (void*)&sample32, 4, &i2s_bytes_rw, portMAX_DELAY);
		}
		else
		{
			sample32 = sample_i16;
		}

		sampleCounter++;

		if (TIMING_BY_SAMPLE_1_SEC) //one full second passed
		{
			seconds++;
			sampleCounter = 0;
		}
	} //end skip channel cnt
}

void channel_reverb()
{
	//no init, happens inside of the function
	#ifdef BOARD_WHALE
	start_MCLK();
	#endif
	decaying_reverb(1);
	#ifdef BOARD_WHALE
	stop_MCLK();
	#endif
}

void pass_through()
{
	#define MIN_V 32000		//initial minimum value, should be higher than any possible value
	#define MAX_V -32000	//initial maximum value, should be lower than any possible value

	int16_t min_lr, min_rr, max_lr, max_rr; //raw
	float min_lf, min_rf, max_lf, max_rf; //converted
	int32_t min_l, min_r, max_l, max_r; //final

	/*
	//test for powered mics - voltage taken from CV out
	gecho_deinit_MIDI();
	cv_out_init();

	//DAC output is 8-bit. Maximum (255) corresponds to VDD.

	//dac_output_voltage(DAC_CHANNEL_1, 150); //cca 1.76V
	//dac_output_voltage(DAC_CHANNEL_2, 150); //cca 1.76V
	dac_output_voltage(DAC_CHANNEL_1, 200); //cca 2.35V
	dac_output_voltage(DAC_CHANNEL_2, 200); //cca 2.35V
	*/

	program_settings_reset();

	#ifdef BOARD_WHALE_V1
	RGB_LED_R_ON;
	#endif

	uint8_t /*l,r,*/clipping_l=0,clipping_r=0,clipping_restore=0;

	LEDS_ALL_OFF;

	#define MAX_UV_RANGE	32000.0f

	while(!event_next_channel)
    {
		if (!(sampleCounter & 0x00000001)) //right channel
		{
			//r = PseudoRNG1a_next_float();
			//memcpy(&random_value, &r, sizeof(random_value));

			sample_f[0] = (float)((int16_t)(ADC_sample >> 16)) * OpAmp_ADC12_signal_conversion_factor;

			if((int16_t)(ADC_sample >> 16)<min_rr) { min_rr = (int16_t)(ADC_sample >> 16); }
			if((int16_t)(ADC_sample >> 16)>max_rr) { max_rr = (int16_t)(ADC_sample >> 16); }

			if(sample_f[0]<min_rf) { min_rf = sample_f[0]; }
			if(sample_f[0]>max_rf) { max_rf = sample_f[0]; }
		}

		if (sampleCounter & 0x00000001) //left channel
		{
			sample_f[1] = (float)((int16_t)ADC_sample) * OpAmp_ADC12_signal_conversion_factor;

			if((int16_t)ADC_sample<min_lr) { min_lr = (int16_t)ADC_sample; }
			if((int16_t)ADC_sample>max_lr) { max_lr = (int16_t)ADC_sample; }

			if(sample_f[1]<min_lf) { min_lf = sample_f[1]; }
			if(sample_f[1]>max_lf) { max_lf = sample_f[1]; }

			sample_mix = sample_f[1] * MAIN_VOLUME;

			sample_i16 = (int16_t)(sample_mix * SAMPLE_VOLUME);

			if(sample_i16<min_l) { min_l = sample_i16; }
			if(sample_i16>max_l) { max_l = sample_i16; }
		}
		else
		{
			sample_mix = sample_f[0] * MAIN_VOLUME;

			sample_i16 = (int16_t)(sample_mix * SAMPLE_VOLUME);

			if(sample_i16<min_r) { min_r = sample_i16; }
			if(sample_i16>max_r) { max_r = sample_i16; }
		}

		if (sampleCounter & 0x00000001) //left channel
		{
			sample32 += sample_i16 << 16;
		}
		else
		{
			sample32 = sample_i16;
		}

		if (sampleCounter & 0x00000001) //left channel
		{
			/*n_bytes = */ //i2s_push_sample(I2S_NUM, (char *)&sample32, portMAX_DELAY);
			i2s_write(I2S_NUM, (void*)&sample32, 4, &i2s_bytes_rw, portMAX_DELAY);

			/*ADC_result = */ //i2s_pop_sample(I2S_NUM, (char*)&ADC_sample, portMAX_DELAY);
			i2s_read(I2S_NUM, (void*)&ADC_sample, 4, &i2s_bytes_rw, portMAX_DELAY);
		}

		sampleCounter++;

		#ifdef PASS_THROUGH_LEVELS_DEBUG
		if (TIMING_BY_SAMPLE_1_SEC) //one full second passed
    	{
    		seconds++;
    		printf("LEFT: %d - %d (raw), %f - %f / %d - %d (out), RIGHT: %d - %d (raw), %f - %f / %d - %d (out)\n", min_lr, max_lr, min_lf, max_lf, min_l, max_l, min_rr, max_rr, min_rf, max_rf, min_r, max_r);
		#else
    		if (TIMING_EVERY_50_MS==0)
    		{
				#ifdef BOARD_GECHO
    			clipping_restore++;
    			if(clipping_restore==20) //every 1 second
    			{
    				clipping_restore = 0;
    				if(clipping_r>1) clipping_r--;
    				if(clipping_l>1) clipping_l--;
    				LED_B5_set_byte(0x1f >> (5-clipping_r));
    				LED_O4_set_byte(0x0f << (4-clipping_l));
    			}

    			l = 9 - ((float)max_l / MAX_UV_RANGE)*9.0f;
    			r = 9 - ((float)max_r / MAX_UV_RANGE)*9.0f;
    			LED_R8_set_byte( 0xff << l);
    			LED_W8_set_byte( 0xff >> r);

    			if(max_r > MAX_UV_RANGE || min_r < -MAX_UV_RANGE)
    			{
    				if(clipping_r<5)
    				{
    					clipping_r++;
    					LED_B5_set_byte(0x1f >> (5-clipping_r));
    				}
    			}
    			if(max_l > MAX_UV_RANGE || min_l < -MAX_UV_RANGE)
    			{
    				if(clipping_l<4)
    				{
    					clipping_l++;
    					LED_O4_set_byte(0x0f << (4-clipping_l));
    				}
    			}
				#endif
			#endif
    		sampleCounter = 0;

    		min_l=MIN_V;
    		min_r=MIN_V;
    		max_l=MAX_V;
    		max_r=MAX_V;
    		min_lf=MIN_V;
    		min_rf=MIN_V;
    		max_lf=MAX_V;
    		max_rf=MAX_V;
    		min_lr=MIN_V;
    		min_rr=MIN_V;
    		max_lr=MAX_V;
    		max_rr=MAX_V;
    	}

    } //end skip channel cnt

	//return input select back to previous user-defined value
	codec_select_input(ADC_input_select);
}
