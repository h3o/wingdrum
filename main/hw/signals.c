/*
 * signals.c
 *
 *  Copyright 2024 Phonicbloom Ltd.
 *
 *  Created on: Apr 27, 2016
 *      Author: mario
 *
 *  This file can be used within the terms of GNU GPLv3 license: https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 *  Find more information at:
 *  http://phonicbloom.com/
 *  http://gechologic.com/
 *
 */

#include "signals.h"
#include "init.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

//-------------------------- sample and echo buffers -------------------

//uint32_t ADC_sample;
//uint32_t sample32;

volatile int16_t sample_i16 = 0;
volatile uint16_t sample_u16 = 0, sample_ADC_LPF = 0;

#ifdef DYNAMICS_BY_ADC
uint16_t sample_ADC_dynamic_RB[ADC_DYN_RB];
int sample_ADC_dyn_RB_ptr = 0, sample_ADC_dyn_RB_AVG, skip_ADC_samples = 0;
#endif

float sample_f[2],sample_mix,sample_mix_wt,sample_lpf[4],sample_hpf[4];

//int n_bytes, ADC_result;
uint32_t ADC_sample, ADC_sample0;
//int ADC_sample_valid = 0;
uint32_t sample32;

#ifdef ECHO_BUFFER_STATIC
int16_t echo_buffer[ECHO_BUFFER_LENGTH];	//the buffer is allocated statically
#else
int16_t *echo_buffer = NULL;	//the buffer is allocated dynamically
#endif

int echo_dynamic_loop_current_step = 0;//ECHO_DYNAMIC_LOOP_LENGTH_DEFAULT_STEP;
int echo_buffer_ptr0, echo_buffer_ptr;
int echo_dynamic_loop_length = 0;//ECHO_BUFFER_LENGTH_DEFAULT; //default
int echo_dynamic_loop_length0;//, echo_length_updated = 0;
//int echo_skip_samples = 0, echo_skip_samples_from = 0;
float ECHO_MIXING_GAIN_MUL, ECHO_MIXING_GAIN_DIV, ECHO_MIXING_GAIN_MUL_TARGET;
float echo_mix_f;

const int echo_dynamic_loop_steps[ECHO_DYNAMIC_LOOP_STEPS] = {
	ECHO_BUFFER_LENGTH_DEFAULT,	//default, same as (I2S_AUDIOFREQ * 3 / 2)
	(I2S_AUDIOFREQ),			//one second
	(I2S_AUDIOFREQ / 5 * 4),	//
	(I2S_AUDIOFREQ / 4 * 3),	//
	(I2S_AUDIOFREQ / 3 * 2),	//short delay
	(I2S_AUDIOFREQ / 2),		//short delay
	(I2S_AUDIOFREQ / 3),		//short delay
	(I2S_AUDIOFREQ / 4),		//short delay
	0,							//delay off
	(I2S_AUDIOFREQ / 5),		//short delay
	(I2S_AUDIOFREQ / 6),		//short delay
	(I2S_AUDIOFREQ / 8),		//short delay
	(I2S_AUDIOFREQ / 10),		//short delay
	(I2S_AUDIOFREQ / 16),		//short delay
	(I2S_AUDIOFREQ / 32),		//short delay
	(I2S_AUDIOFREQ / 64),		//short delay
	(I2S_AUDIOFREQ / 128),		//short delay
	0							//delay off
};

//int fixed_arp_level = 0;
#ifndef REVERB_POLYPHONIC
//int16_t reverb_buffer[REVERB_BUFFER_LENGTH];	//the buffer is allocated statically
int16_t *reverb_buffer; //the buffer is allocated dynamically
int reverb_buffer_ptr0, reverb_buffer_ptr;		//pointers for reverb buffer
int reverb_dynamic_loop_length;
float REVERB_MIXING_GAIN_MUL, REVERB_MIXING_GAIN_DIV;
#endif

float reverb_mix_f;

#ifdef REVERB_POLYPHONIC

#if REVERB_BUFFERS_EXT==9
int16_t *reverb_buffer_ext[REVERB_BUFFERS_EXT] = {NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL};	//extra reverb buffers for higher and lower octaves
#elif REVERB_BUFFERS_EXT==6
int16_t *reverb_buffer_ext[REVERB_BUFFERS_EXT] = {NULL,NULL,NULL,NULL,NULL,NULL};	//extra reverb buffers for higher and lower octaves
#elif REVERB_BUFFERS_EXT==5
int16_t *reverb_buffer_ext[REVERB_BUFFERS_EXT] = {NULL,NULL,NULL,NULL,NULL};	//extra reverb buffers for higher and lower octaves
#elif REVERB_BUFFERS_EXT==4
int16_t *reverb_buffer_ext[REVERB_BUFFERS_EXT] = {NULL,NULL,NULL,NULL};	//extra reverb buffers for higher and lower octaves
#elif REVERB_BUFFERS_EXT==3
int16_t *reverb_buffer_ext[REVERB_BUFFERS_EXT] = {NULL,NULL,NULL};	//extra reverb buffers for higher and lower octaves
#endif

#else
int16_t *reverb_buffer_ext[REVERB_BUFFERS_EXT] = {NULL,NULL};	//extra reverb buffers for higher and lower octaves
#endif
int reverb_buffer_ptr0_ext[REVERB_BUFFERS_EXT], reverb_buffer_ptr_ext[REVERB_BUFFERS_EXT];	//pointers for reverb buffer
int reverb_dynamic_loop_length_ext[REVERB_BUFFERS_EXT];
float REVERB_MIXING_GAIN_MUL_EXT, REVERB_MIXING_GAIN_DIV_EXT;

float SAMPLE_VOLUME = SAMPLE_VOLUME_DEFAULT; //9.0f; //12.0f; //0.375f;
float limiter_coeff = DYNAMIC_LIMITER_COEFF_DEFAULT;

#ifdef LPF_ENABLED
float ADC_LPF_ALPHA = 0.2f; //up to 1.0 for max feedback = no filtering
float ADC_HPF_ALPHA = 0.2f;
#endif

//---------------------------------------------------------------------------

double static b_noise = NOISE_SEED;
uint32_t random_value;

void reset_pseudo_random_seed()
{
	b_noise = NOISE_SEED;
}

void set_pseudo_random_seed(double new_value)
{
	//printf("set_pseudo_random_seed(%f)\n",new_value);
	b_noise = new_value;
}

float PseudoRNG1a_next_float() //returns random float between -0.5f and +0.5f
{
/*
	b_noise = b_noise * b_noise;
	int i_noise = b_noise;
	b_noise = b_noise - i_noise;

	float b_noiseout;
	b_noiseout = b_noise - 0.5;

	b_noise = b_noise + 19;

	return b_noiseout;
*/
	b_noise = b_noise + 19;
	b_noise = b_noise * b_noise;
	int i_noise = b_noise;
	b_noise = b_noise - i_noise;
	return b_noise - 0.5;
}

/*
float PseudoRNG1b_next_float()
{
	double b_noiselast = b_noise;
	b_noise = b_noise + 19;
	b_noise = b_noise * b_noise;
	b_noise = (b_noise + b_noiselast) * 0.5;
	b_noise = b_noise - (int)b_noise;

	return b_noise - 0.5;
}

uint32_t PseudoRNG2_next_int32()
{
	//http://musicdsp.org/showone.php?id=59
	//Type : Linear Congruential, 32bit
	//References : Hal Chamberlain, "Musical Applications of Microprocessors" (Posted by Phil Burk)
	//Notes :
	//This can be used to generate random numeric sequences or to synthesise a white noise audio signal.
	//If you only use some of the bits, use the most significant bits by shifting right.
	//Do not just mask off the low bits.

	//Calculate pseudo-random 32 bit number based on linear congruential method.

	//Change this for different random sequences.
	static unsigned long randSeed = 22222;
	randSeed = (randSeed * 196314165) + 907633515;
	return randSeed;
}
*/

void new_random_value()
{
	float r = PseudoRNG1a_next_float();
	memcpy(&random_value, &r, sizeof(random_value));
}

int fill_with_random_value(char *buffer)
{
	float r = PseudoRNG1a_next_float();
	memcpy(buffer, &r, sizeof(r));
	return sizeof(r);
}

//this seems to be NOT a more optimal way of passing the value
void PseudoRNG_next_value(uint32_t *buffer) //load next random value to the variable
{
	b_noise = b_noise * b_noise;
	int i_noise = b_noise;
	b_noise = b_noise - i_noise;
	float b_noiseout = b_noise - 0.5;
	b_noise = b_noise + NOISE_SEED;
	memcpy(buffer, &b_noiseout, sizeof(b_noiseout));
}

//uint8_t test[10000]; //test for how much static memory is left

void init_echo_buffer()
{
	#ifdef ECHO_BUFFER_STATIC
	//printf("init_echo_buffer(): buffer allocated as static array\n");
	#else
	printf("init_echo_buffer(): free heap = %u, allocating = %u\n", xPortGetFreeHeapSize(), ECHO_BUFFER_LENGTH * sizeof(int16_t));
	echo_buffer = (int16_t*)malloc(ECHO_BUFFER_LENGTH * sizeof(int16_t));
	printf("init_echo_buffer(): echo_buffer = %x\n", (unsigned int)echo_buffer);
	if(echo_buffer==NULL)
	{
		printf("init_echo_buffer(): could not allocate buffer of size %d bytes\n", ECHO_BUFFER_LENGTH * sizeof(int16_t));
		while(1);
	}
	#endif
}

void deinit_echo_buffer()
{
	#ifdef ECHO_BUFFER_STATIC
	//printf("deinit_echo_buffer(): buffer allocated as static array\n");
	#else
	if(echo_buffer!=NULL)
	{
		free(echo_buffer);
		echo_buffer = NULL;
		printf("deinit_echo_buffer(): memory freed\n");
	}
	else
	{
		printf("deinit_echo_buffer(): buffer not allocated\n");
	}
	#endif
}

#ifndef REVERB_POLYPHONIC
IRAM_ATTR int16_t add_reverb(int16_t sample)
{
	//wrap the reverb loop
	reverb_buffer_ptr0++;
	if (reverb_buffer_ptr0 >= reverb_dynamic_loop_length)
	{
		reverb_buffer_ptr0 = 0;
	}

	reverb_buffer_ptr = reverb_buffer_ptr0 + 1;
	if (reverb_buffer_ptr >= reverb_dynamic_loop_length)
	{
		reverb_buffer_ptr = 0;
	}

	//add reverb from the loop
	reverb_mix_f = (float)sample + (float)reverb_buffer[reverb_buffer_ptr] * REVERB_MIXING_GAIN_MUL / REVERB_MIXING_GAIN_DIV;

	if (reverb_mix_f > COMPUTED_SAMPLE_MIXING_LIMIT_UPPER) { reverb_mix_f = COMPUTED_SAMPLE_MIXING_LIMIT_UPPER; }
	if (reverb_mix_f < COMPUTED_SAMPLE_MIXING_LIMIT_LOWER) { reverb_mix_f = COMPUTED_SAMPLE_MIXING_LIMIT_LOWER; }

	sample = (int16_t)reverb_mix_f;

	//store result to reverb, the amount defined by a fragment
	//reverb_mix_f = ((float)sample_i16 * REVERB_MIXING_GAIN_MUL / REVERB_MIXING_GAIN_DIV);
	//reverb_mix_f *= REVERB_MIXING_GAIN_MUL / REVERB_MIXING_GAIN_DIV;
	//reverb_buffer[reverb_buffer_ptr0] = (int16_t)reverb_mix_f;

	reverb_buffer[reverb_buffer_ptr0] = sample;

	return sample;
}
#endif

#ifdef REVERB_POLYPHONIC
IRAM_ATTR int16_t add_reverb_ext_all9(int16_t sample)
{
	/*
	//wrap the reverb loop
	reverb_buffer_ptr0++;
	if (reverb_buffer_ptr0 >= reverb_dynamic_loop_length)
	{
		reverb_buffer_ptr0 = 0;
	}

	reverb_buffer_ptr = reverb_buffer_ptr0 + 1;
	if (reverb_buffer_ptr >= reverb_dynamic_loop_length)
	{
		reverb_buffer_ptr = 0;
	}
	*/

	int32_t out_sample = 0;
	float sample_f = ((float)sample) * REVERB_EXT9_SAMPLE_INPUT_COEFF;
	/*

	//add reverb from the loop
	reverb_mix_f = sample_f + (float)reverb_buffer[reverb_buffer_ptr] * REVERB_MIXING_GAIN_MUL / REVERB_MIXING_GAIN_DIV;
	reverb_mix_f *= REVERB_EXT_SAMPLE_OUTPUT_COEFF;

	if (reverb_mix_f > COMPUTED_SAMPLE_MIXING_LIMIT_UPPER) { reverb_mix_f = COMPUTED_SAMPLE_MIXING_LIMIT_UPPER; }
	if (reverb_mix_f < COMPUTED_SAMPLE_MIXING_LIMIT_LOWER) { reverb_mix_f = COMPUTED_SAMPLE_MIXING_LIMIT_LOWER; }

	reverb_buffer[reverb_buffer_ptr0] = (out_sample = (int16_t)reverb_mix_f);
	*/

	//add extended buffers
	for(int r=0;r<REVERB_BUFFERS_EXT;r++)
	{
		if(reverb_dynamic_loop_length_ext[r])
		{
			reverb_buffer_ptr0_ext[r]++;
			if (reverb_buffer_ptr0_ext[r] >= reverb_dynamic_loop_length_ext[r])
			{
				reverb_buffer_ptr0_ext[r] = 0;
			}

			reverb_buffer_ptr_ext[r] = reverb_buffer_ptr0_ext[r] + 1;
			if (reverb_buffer_ptr_ext[r] >= reverb_dynamic_loop_length_ext[r])
			{
				reverb_buffer_ptr_ext[r] = 0;
			}

			//add reverb from the loop
			//reverb_mix_f = 0;///*sample_f +*/ (float)(reverb_buffer_ext[r][reverb_buffer_ptr_ext[r]]) * REVERB_MIXING_GAIN_MUL_EXT / REVERB_MIXING_GAIN_DIV_EXT;
			reverb_mix_f = sample_f + (float)(reverb_buffer_ext[r][reverb_buffer_ptr_ext[r]]) * REVERB_MIXING_GAIN_MUL_EXT / REVERB_MIXING_GAIN_DIV_EXT;
			//reverb_mix_f *= REVERB_EXT9_SAMPLE_OUTPUT_COEFF;

			if (reverb_mix_f > COMPUTED_SAMPLE_MIXING_LIMIT_UPPER) { reverb_mix_f = COMPUTED_SAMPLE_MIXING_LIMIT_UPPER; }
			if (reverb_mix_f < COMPUTED_SAMPLE_MIXING_LIMIT_LOWER) { reverb_mix_f = COMPUTED_SAMPLE_MIXING_LIMIT_LOWER; }

			//out_sample += (reverb_buffer_ext[r][reverb_buffer_ptr0_ext[r]] = (int16_t)reverb_mix_f);
			//printf("reverb_buffer_ptr0_ext[%d]=%d\n", r, reverb_buffer_ptr0_ext[r]);

			/*
			if(reverb_buffer_ptr0_ext[r] >= BIT_CRUSHER_REVERB_MAX_EXT9)
			{
				printf("reverb_buffer_ptr0_ext[%d] >= BIT_CRUSHER_REVERB_MAX_EXT(%d): %d >= %d\n", r, r, reverb_buffer_ptr0_ext[r], BIT_CRUSHER_REVERB_MAX_EXT9);
				printf("reverb_dynamic_loop_length_ext[%d] = %d\n", r, reverb_dynamic_loop_length_ext[r]);
			}
			*/

			//printf("%d-%d\n", r, reverb_buffer_ptr0_ext[r]);

			//reverb_buffer_ext[r][reverb_buffer_ptr0_ext[r]] = (int16_t)reverb_mix_f;
			//out_sample += (int16_t)reverb_mix_f;

			out_sample += (reverb_buffer_ext[r][reverb_buffer_ptr0_ext[r]] = (int16_t)reverb_mix_f);
		}
	}

	if (out_sample > COMPUTED_SAMPLE_MIXING_LIMIT_UPPER_INT) { out_sample = COMPUTED_SAMPLE_MIXING_LIMIT_UPPER_INT; }
	if (out_sample < COMPUTED_SAMPLE_MIXING_LIMIT_LOWER_INT) { out_sample = COMPUTED_SAMPLE_MIXING_LIMIT_LOWER_INT; }

	return (int16_t)out_sample;
}
#else
IRAM_ATTR int16_t add_reverb_ext_all3(int16_t sample)
{
	//wrap the reverb loop
	reverb_buffer_ptr0++;
	if (reverb_buffer_ptr0 >= reverb_dynamic_loop_length)
	{
		reverb_buffer_ptr0 = 0;
	}

	reverb_buffer_ptr = reverb_buffer_ptr0 + 1;
	if (reverb_buffer_ptr >= reverb_dynamic_loop_length)
	{
		reverb_buffer_ptr = 0;
	}

	int32_t out_sample = 0;
	float sample_f = ((float)sample) * REVERB_EXT_SAMPLE_INPUT_COEFF;

	//add reverb from the loop
	reverb_mix_f = sample_f + (float)reverb_buffer[reverb_buffer_ptr] * REVERB_MIXING_GAIN_MUL / REVERB_MIXING_GAIN_DIV;
	reverb_mix_f *= REVERB_EXT_SAMPLE_OUTPUT_COEFF;

	if (reverb_mix_f > COMPUTED_SAMPLE_MIXING_LIMIT_UPPER) { reverb_mix_f = COMPUTED_SAMPLE_MIXING_LIMIT_UPPER; }
	if (reverb_mix_f < COMPUTED_SAMPLE_MIXING_LIMIT_LOWER) { reverb_mix_f = COMPUTED_SAMPLE_MIXING_LIMIT_LOWER; }

	reverb_buffer[reverb_buffer_ptr0] = (out_sample = (int16_t)reverb_mix_f);

	//add 2nd and 3rd buffer
	for(int r=0;r<REVERB_BUFFERS_EXT;r++)
	{
		reverb_buffer_ptr0_ext[r]++;
		if (reverb_buffer_ptr0_ext[r] >= reverb_dynamic_loop_length_ext[r])
		{
			reverb_buffer_ptr0_ext[r] = 0;
		}

		reverb_buffer_ptr_ext[r] = reverb_buffer_ptr0_ext[r] + 1;
		if (reverb_buffer_ptr_ext[r] >= reverb_dynamic_loop_length_ext[r])
		{
			reverb_buffer_ptr_ext[r] = 0;
		}

		//add reverb from the loop
		//reverb_mix_f = 0;///*sample_f +*/ (float)(reverb_buffer_ext[r][reverb_buffer_ptr_ext[r]]) * REVERB_MIXING_GAIN_MUL_EXT / REVERB_MIXING_GAIN_DIV_EXT;
		reverb_mix_f = sample_f + (float)(reverb_buffer_ext[r][reverb_buffer_ptr_ext[r]]) * REVERB_MIXING_GAIN_MUL_EXT / REVERB_MIXING_GAIN_DIV_EXT;
		reverb_mix_f *= REVERB_EXT_SAMPLE_OUTPUT_COEFF;

		if (reverb_mix_f > COMPUTED_SAMPLE_MIXING_LIMIT_UPPER) { reverb_mix_f = COMPUTED_SAMPLE_MIXING_LIMIT_UPPER; }
		if (reverb_mix_f < COMPUTED_SAMPLE_MIXING_LIMIT_LOWER) { reverb_mix_f = COMPUTED_SAMPLE_MIXING_LIMIT_LOWER; }

		//out_sample += (reverb_buffer_ext[r][reverb_buffer_ptr0_ext[r]] = (int16_t)reverb_mix_f);
		//printf("reverb_buffer_ptr0_ext[%d]=%d\n", r, reverb_buffer_ptr0_ext[r]);

		if(reverb_buffer_ptr0_ext[r] >= BIT_CRUSHER_REVERB_MAX_EXT(r))
		{
			printf("reverb_buffer_ptr0_ext[%d] >= BIT_CRUSHER_REVERB_MAX_EXT(%d): %d >= %d\n", r, r, reverb_buffer_ptr0_ext[r], BIT_CRUSHER_REVERB_MAX_EXT(r));
			printf("reverb_dynamic_loop_length_ext[%d] = %d\n", r, reverb_dynamic_loop_length_ext[r]);
		}

		//printf("%d-%d\n", r, reverb_buffer_ptr0_ext[r]);

		//reverb_buffer_ext[r][reverb_buffer_ptr0_ext[r]] = (int16_t)reverb_mix_f;
		//out_sample += (int16_t)reverb_mix_f;

		out_sample += (reverb_buffer_ext[r][reverb_buffer_ptr0_ext[r]] = (int16_t)reverb_mix_f);
	}

	if (out_sample > COMPUTED_SAMPLE_MIXING_LIMIT_UPPER_INT) { out_sample = COMPUTED_SAMPLE_MIXING_LIMIT_UPPER_INT; }
	if (out_sample < COMPUTED_SAMPLE_MIXING_LIMIT_LOWER_INT) { out_sample = COMPUTED_SAMPLE_MIXING_LIMIT_LOWER_INT; }

	return (int16_t)out_sample;
}
#endif

IRAM_ATTR int16_t add_echo(int16_t sample)
{
	if(echo_dynamic_loop_length)
	{
		//wrap the echo loop
		echo_buffer_ptr0++;
		if (echo_buffer_ptr0 >= echo_dynamic_loop_length)
		{
			echo_buffer_ptr0 = 0;
		}

		echo_buffer_ptr = echo_buffer_ptr0 + 1;
		if (echo_buffer_ptr >= echo_dynamic_loop_length)
		{
			echo_buffer_ptr = 0;
		}

		/*
		if(echo_skip_samples)
		{
			if(echo_skip_samples_from && (echo_skip_samples_from == echo_buffer_ptr))
			{
				echo_skip_samples_from = 0;
			}
			if(echo_skip_samples_from==0)
			{
				echo_buffer[echo_buffer_ptr] = 0;
				echo_skip_samples--;
			}
		}
		*/

		//add echo from the loop
		echo_mix_f = (float)sample + (float)echo_buffer[echo_buffer_ptr] * ECHO_MIXING_GAIN_MUL / ECHO_MIXING_GAIN_DIV;
	}
	else
	{
		echo_mix_f = (float)sample;
	}

	echo_mix_f *= limiter_coeff;

	if(echo_mix_f < -DYNAMIC_LIMITER_THRESHOLD_ECHO && limiter_coeff > 0)
	{
		limiter_coeff *= DYNAMIC_LIMITER_COEFF_MUL;
	}

	sample = (int16_t)echo_mix_f;

	//store result to echo, the amount defined by a fragment
	//echo_mix_f = ((float)sample_i16 * ECHO_MIXING_GAIN_MUL / ECHO_MIXING_GAIN_DIV);
	//echo_mix_f *= ECHO_MIXING_GAIN_MUL / ECHO_MIXING_GAIN_DIV;
	//echo_buffer[echo_buffer_ptr0] = (int16_t)echo_mix_f;

	echo_buffer[echo_buffer_ptr0] = sample;

	return sample;
}
/*
IRAM_ATTR int16_t reversible_echo(int16_t sample, int direction)
{
	if(echo_dynamic_loop_length)
	{
		//wrap the echo loop
		if(direction)
		{
			echo_buffer_ptr0++;
			if (echo_buffer_ptr0 >= echo_dynamic_loop_length)
			{
				echo_buffer_ptr0 = 0;
			}

			echo_buffer_ptr = echo_buffer_ptr0 + 1;
			if (echo_buffer_ptr >= echo_dynamic_loop_length)
			{
				echo_buffer_ptr = 0;
			}
		}
		else
		{
			if (echo_buffer_ptr0 > 0)
			{
				echo_buffer_ptr0--;
			}
			else
			{
				echo_buffer_ptr0 = echo_dynamic_loop_length;
			}

			echo_buffer_ptr = echo_buffer_ptr0 - 1;
			if (echo_buffer_ptr < 0)
			{
				echo_buffer_ptr = echo_dynamic_loop_length;
			}
		}

		//add echo from the loop
		echo_mix_f = (float)sample + (float)echo_buffer[echo_buffer_ptr] * ECHO_MIXING_GAIN_MUL / ECHO_MIXING_GAIN_DIV;
	}
	else
	{
		echo_mix_f = (float)sample;
	}

	echo_mix_f *= limiter_coeff;

	if(echo_mix_f < -DYNAMIC_LIMITER_THRESHOLD_ECHO && limiter_coeff > 0)
	{
		limiter_coeff *= DYNAMIC_LIMITER_COEFF_MUL;
	}

	sample = (int16_t)echo_mix_f;

	echo_buffer[echo_buffer_ptr0] = sample;

	return sample;
}
*/
