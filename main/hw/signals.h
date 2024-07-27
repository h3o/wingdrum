/*
 * signals.h
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

#ifndef SIGNALS_H_
#define SIGNALS_H_

#include <stdbool.h>
#include <stdint.h> //for uint16_t type

#include "codec.h"
#include "board.h"

#include "dsp/reverb.h"

#define ECHO_BUFFER_STATIC //needs to be dynamic to support other engines, but that did not work due to ESP32 malloc limitation (largest free block not large enough)

//===== BEGIN NOISE SEED BLOCK ===========================================

//#define NOISE_SEED 19.0000000000000000000000000000000000000001
#define NOISE_SEED 19.1919191919191919191919191919191919191919
//#define NOISE_SEED 13.1313131313131313131313131313131313131313
//#define NOISE_SEED 19.7228263362716688952313412753412622364256 //k.l.
//#define NOISE_SEED 19.8381511698353141312312412453512642346256 //d.r.
//#define NOISE_SEED 19.6729592785901037505892041201203552160120 //a.f.

//===== END NOISE SEED BLOCK =============================================

extern uint32_t sample32;
//extern int n_bytes, ADC_result;
extern uint32_t ADC_sample, ADC_sample0;
//extern int ADC_sample_valid = 0;

extern volatile int16_t sample_i16;
extern volatile uint16_t sample_u16, sample_ADC_LPF;

#ifdef DYNAMICS_BY_ADC
#define ADC_DYN_RB	1000
extern uint16_t sample_ADC_dynamic_RB[ADC_DYN_RB];
extern int sample_ADC_dyn_RB_ptr, sample_ADC_dyn_RB_AVG, skip_ADC_samples;
#endif

extern float sample_f[2],sample_mix,sample_mix_wt,sample_lpf[4],sample_hpf[4];

#define OpAmp_ADC12_signal_conversion_factor OPAMP_ADC12_CONVERSION_FACTOR_DEFAULT

#ifdef LPF_ENABLED
extern float ADC_LPF_ALPHA, ADC_HPF_ALPHA;
#endif

extern uint32_t random_value;

#ifdef __cplusplus
 extern "C" {
#endif

void reset_pseudo_random_seed();
void set_pseudo_random_seed(double new_value);

float PseudoRNG1a_next_float();
//float PseudoRNG1b_next_float();
//uint32_t PseudoRNG2_next_int32();

void new_random_value();
int fill_with_random_value(char *buffer);
void PseudoRNG_next_value(uint32_t *buffer);

#define ACC_AXIS_Y (acc_res[1] - acc_calibrated[1])
//#define ACC_AXIS_Y (acc_calibrated[1] - acc_res[1])

#define SENSOR_DELAY_9					(ACC_AXIS_Y > 84.0f)
#define SENSOR_DELAY_8					(ACC_AXIS_Y > 76.0f)
#define SENSOR_DELAY_7					(ACC_AXIS_Y > 68.0f)
#define SENSOR_DELAY_6					(ACC_AXIS_Y > 60.0f)
#define SENSOR_DELAY_5					(ACC_AXIS_Y > 42.0f)
#define SENSOR_DELAY_4					(ACC_AXIS_Y > 34.0f)
#define SENSOR_DELAY_3					(ACC_AXIS_Y > 26.0f)
#define SENSOR_DELAY_2					(ACC_AXIS_Y > 18.0f)
#define SENSOR_DELAY_ACTIVE				(ACC_AXIS_Y > 10.0f)

//-------------------------- sample and echo buffers -------------------

#define ECHO_DYNAMIC_LOOP_STEPS 9*2//15
//the length is now controlled dynamically (as defined in signals.c)

#define ECHO_BUFFER_LENGTH (I2S_AUDIOFREQ * 3 / 2)	//76170 samples, 152340 bytes, 1.5 sec
//#define ECHO_BUFFER_LENGTH	90000 //max that fits = 76392

#define ECHO_BUFFER_LENGTH_DEFAULT 				ECHO_BUFFER_LENGTH
//#define ECHO_BUFFER_LENGTH_DEFAULT 				(I2S_AUDIOFREQ * 3 / 2)	//1.5 sec, longest delay currently used (default)
#define ECHO_BUFFER_LENGTH_MIN					20

#define ECHO_DYNAMIC_LOOP_LENGTH_DEFAULT_STEP 	0	//refers to echo_dynamic_loop_steps in signals.c
#define ECHO_DYNAMIC_LOOP_LENGTH_ECHO_OFF	 	(ECHO_DYNAMIC_LOOP_STEPS-1)	//corresponds to what is in echo_dynamic_loop_steps structure as 0

#ifdef ECHO_BUFFER_STATIC
extern int16_t echo_buffer[];	//the buffer is allocated statically
#else
extern int16_t *echo_buffer;	//the buffer is allocated dynamically
#endif

//#define ECHO_DYNAMIC_LOOP_LENGTH_ADJUST_STEP	100
#define ECHO_DYNAMIC_LOOP_LENGTH_ADJUST_FACTOR	100

extern int echo_buffer_ptr0, echo_buffer_ptr;		//pointers for echo buffer
extern int echo_dynamic_loop_length;
extern int echo_dynamic_loop_length0;//, echo_length_updated;
//extern int echo_skip_samples, echo_skip_samples_from;
extern int echo_dynamic_loop_current_step;
extern float ECHO_MIXING_GAIN_MUL, ECHO_MIXING_GAIN_DIV, ECHO_MIXING_GAIN_MUL_TARGET;
extern float echo_mix_f;
extern const uint8_t echo_dynamic_loop_indicator[ECHO_DYNAMIC_LOOP_STEPS];
extern const int echo_dynamic_loop_steps[ECHO_DYNAMIC_LOOP_STEPS];
//extern bool PROG_add_echo;

//#define ARP_VOLUME_MULTIPLIER_DEFAULT	3.333f
//#define ARP_VOLUME_MULTIPLIER_MANUAL	5.0f
//#define FIXED_ARP_LEVEL_STEP 			25
//#define FIXED_ARP_LEVEL_MAX 			100
//extern int fixed_arp_level;

//#define REVERB_BUFFER_LENGTH (I2S_AUDIOFREQ / 10 + 10)	//
//#define REVERB_BUFFER_LENGTH (I2S_AUDIOFREQ / 40)	//
//#define REVERB_BUFFER_LENGTH (I2S_AUDIOFREQ / 24)	//longest reverb currently used (2000 samples @ 48k), 2115.833 @APLL
//#define REVERB_BUFFER_LENGTH (I2S_AUDIOFREQ / 20)	//longest reverb currently used (2400 samples @ 48k), 2539 @APLL
//#define REVERB_BUFFER_LENGTH (I2S_AUDIOFREQ / 10)

#ifndef REVERB_POLYPHONIC
//extern int16_t reverb_buffer[];	//the buffer is allocated statically
extern int16_t *reverb_buffer;	//the buffer is allocated dynamically

extern int reverb_buffer_ptr0, reverb_buffer_ptr;		//pointers for reverb buffer
extern int reverb_dynamic_loop_length;
extern float REVERB_MIXING_GAIN_MUL, REVERB_MIXING_GAIN_DIV;
#endif

extern float reverb_mix_f;

#ifdef REVERB_POLYPHONIC
//#define REVERB_BUFFERS_EXT	9
//#define REVERB_BUFFERS_EXT	6
//#define REVERB_BUFFERS_EXT	5 //already too CPU intensive
#define REVERB_BUFFERS_EXT	4
//#define REVERB_BUFFERS_EXT	3
#else
#define REVERB_BUFFERS_EXT	2
#endif
extern int16_t *reverb_buffer_ext[REVERB_BUFFERS_EXT];	//extra reverb buffers for higher and lower octaves
extern int reverb_buffer_ptr0_ext[REVERB_BUFFERS_EXT], reverb_buffer_ptr_ext[REVERB_BUFFERS_EXT];	//pointers for reverb buffer
extern int reverb_dynamic_loop_length_ext[REVERB_BUFFERS_EXT];
extern float REVERB_MIXING_GAIN_MUL_EXT, REVERB_MIXING_GAIN_DIV_EXT;

//extern uint32_t random_value;
extern float SAMPLE_VOLUME;
#define SAMPLE_VOLUME_DEFAULT 9.0f
//#define SAMPLE_VOLUME 9.0f
#define SAMPLE_VOLUME_REVERB 1.0f; //1.5f

//this controls limits for global volume
//#define SAMPLE_VOLUME_MAX 24.0f
//#define SAMPLE_VOLUME_MIN 1.0f

//#define OPAMP_ADC12_CONVERSION_FACTOR_DEFAULT	0.00150f //was 0.005 for rgb-black-mini
#define OPAMP_ADC12_CONVERSION_FACTOR_DEFAULT	0.00250f //was 0.005 for rgb-black-mini
//#define OPAMP_ADC12_CONVERSION_FACTOR_DEFAULT	0.01000f
#define OPAMP_ADC12_CONVERSION_FACTOR_MIN		0.00010f
#define OPAMP_ADC12_CONVERSION_FACTOR_MAX		0.02000f
#define OPAMP_ADC12_CONVERSION_FACTOR_STEP		0.00010f
//extern float OpAmp_ADC12_signal_conversion_factor;

#define COMPUTED_SAMPLE_MIXING_LIMIT_UPPER 32000.0f //was in mini black
#define COMPUTED_SAMPLE_MIXING_LIMIT_LOWER -32000.0f //was in mini black
#define COMPUTED_SAMPLE_MIXING_LIMIT_UPPER_INT	 32000
#define COMPUTED_SAMPLE_MIXING_LIMIT_LOWER_INT	-32000

//#define COMPUTED_SAMPLE_MIXING_LIMIT_UPPER 16000.0f //32000.0f was in mini black
//#define COMPUTED_SAMPLE_MIXING_LIMIT_LOWER -16000.0f //-32000.0f was in mini black
//#define COMPUTED_SAMPLE_MIXING_LIMIT_UPPER 5000.0f
//#define COMPUTED_SAMPLE_MIXING_LIMIT_LOWER -5000.0f

#define DYNAMIC_LIMITER_THRESHOLD		20000.0f
#define DYNAMIC_LIMITER_STEPDOWN		0.1f
#define DYNAMIC_LIMITER_RECOVER			0.05f

#ifdef BOARD_WHALE

#ifdef BOARD_WHALE_V2
#define DYNAMIC_LIMITER_THRESHOLD_ECHO		16000.0f
#else
#define DYNAMIC_LIMITER_THRESHOLD_ECHO	10000.0f
#endif
#else
#define DYNAMIC_LIMITER_THRESHOLD_ECHO		16000.0f //at 24000.0f it is glitching
#endif
#define DYNAMIC_LIMITER_THRESHOLD_ECHO_DCO	DYNAMIC_LIMITER_THRESHOLD_ECHO

#define DYNAMIC_LIMITER_COEFF_MUL		0.99f
#define DYNAMIC_LIMITER_COEFF_MUL_DCO	0.9f	//need faster limiter for DCO channel
#define DYNAMIC_LIMITER_COEFF_DEFAULT 	1.0f
extern float limiter_coeff;

//#define PREAMP_BOOST 9		//multiplier for signal from ADC1/ADC2 (microphones and piezo pickups input)
#define MAIN_VOLUME 64.0f	//multiplier for samples mixing volume (float to int)

#define UNFILTERED_SIGNAL_VOLUME 64.0f			//for mixing white noise in
#define FLASH_SAMPLES_MIXING_VOLUME 0.8f		//for mixing the samples from flash
#define COMPUTED_SAMPLES_MIXING_VOLUME 2.0f	//used to adjust volume when converting from float to int

//---------------------------------------------------------------------------

void init_echo_buffer();
void deinit_echo_buffer();

#ifndef REVERB_POLYPHONIC
int16_t add_reverb(int16_t sample);
#endif

#ifdef REVERB_POLYPHONIC
int16_t add_reverb_ext_all9(int16_t sample);
#else
int16_t add_reverb_ext_all3(int16_t sample);
#endif

int16_t add_echo(int16_t sample);
//int16_t reversible_echo(int16_t sample, int direction);

#ifdef __cplusplus
}
#endif

#endif /* SIGNALS_H_ */
