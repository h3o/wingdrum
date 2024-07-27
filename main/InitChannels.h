/*
 * InitChannels.h
 *
 *  Copyright 2024 Phonicbloom Ltd.
 *
 *  Created on: Nov 26, 2016
 *      Author: mario
 *
 *  This file can be used within the terms of GNU GPLv3 license: https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 *  Find more information at:
 *  http://phonicbloom.com/
 *  http://gechologic.com/
 *
 */

#ifndef INIT_CHANNELS_H_
#define INIT_CHANNELS_H_

#include <stdint.h> //for uint16_t type etc.
#include <hw/ui.h>
#include "glo_config.h" //for binaural_profile_t

//#define SWITCH_I2C_SPEED_MODES

//--------------------------------------------------------------

	/*
	//v1: short reverb (higher freqs)
	#define BIT_CRUSHER_REVERB_MIN 			(REVERB_BUFFER_LENGTH/50)		//48 samples @48k
	#define BIT_CRUSHER_REVERB_MAX 			(REVERB_BUFFER_LENGTH/4)		//600 samples @48k
	*/

	/*
	//v2: longer reverb (bass)
	#define BIT_CRUSHER_REVERB_MIN 			(REVERB_BUFFER_LENGTH/5)		//480 samples @48k, 507 @APLL (507.8)
	#define BIT_CRUSHER_REVERB_MAX 			REVERB_BUFFER_LENGTH			//2400 samples @48k, 2540 @APLL
	//#define BIT_CRUSHER_REVERB_DEFAULT 	(BIT_CRUSHER_REVERB_MIN*10)		//240 samples @48k
	//#define BIT_CRUSHER_REVERB_DEFAULT 	(BIT_CRUSHER_REVERB_MIN*20)		//480 samples @48k
	#define BIT_CRUSHER_REVERB_DEFAULT 		(BIT_CRUSHER_REVERB_MIN*2)
	*/

	//v3: wide range
	//#define BIT_CRUSHER_REVERB_MIN			(REVERB_BUFFER_LENGTH/50)		//48 samples @48k, 507 @APLL (507.8)
	//#define BIT_CRUSHER_REVERB_MAX			REVERB_BUFFER_LENGTH			//2400 samples @48k, 2540 @APLL
	//#define BIT_CRUSHER_REVERB_DEFAULT		(BIT_CRUSHER_REVERB_MAX/3)

	//wraps at:
	//BIT_CRUSHER_REVERB=2539,len=2539
	//BIT_CRUSHER_REVERB=50,len=50

extern int bit_crusher_reverb;

//--------------------------------------------------------------

extern int mixed_sample_buffer_ptr_L, mixed_sample_buffer_ptr_R;

//extern int16_t *mixed_sample_buffer;
extern int MIXED_SAMPLE_BUFFER_LENGTH;
extern unsigned int mixed_sample_buffer_adr;

extern uint16_t t_TIMING_BY_SAMPLE_EVERY_250_MS; //optimization of timing counters
extern uint16_t t_TIMING_BY_SAMPLE_EVERY_125_MS; //optimization of timing counters
extern uint16_t t_TIMING_BY_SAMPLE_EVERY_25_MS; //optimization of timing counters

void channel_init(int bg_sample, int song_id, int melody_id, int filters_type, float resonance, int use_mclk, int set_wind_voices, int set_active_filter_pairs, int reverb_ext);
void channel_deinit();

#endif /* INIT_CHANNELS_H_ */
