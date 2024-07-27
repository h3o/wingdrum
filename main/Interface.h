/*
 * Interface.h
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

#ifndef INTERFACE_H_
#define INTERFACE_H_

#ifdef __cplusplus

extern unsigned long seconds;

//extern int progressive_rhythm_factor;
//#define PROGRESSIVE_RHYTHM_BOOST_RATIO 2
//#define PROGRESSIVE_RHYTHM_RAMP_COEF 0.999f

//extern float noise_volume, noise_volume_max, noise_boost_by_sensor, mixed_sample_volume;
//extern int special_effect, selected_song, selected_melody;

//extern int arpeggiator_loop;

extern int use_alt_settings;

//#ifdef __cplusplus
 extern "C" {
//#endif

void program_settings_reset();

//#ifdef __cplusplus
}
//#endif

#else

//this needs to be accessible from ui.c with c-linkage

#endif

#endif /* INTERFACE_H_ */
