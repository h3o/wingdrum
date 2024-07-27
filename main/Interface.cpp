/*
 * Interface.cpp
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

#include <Interface.h>
#include <hw/codec.h>
#include <hw/gpio.h>
#include <hw/signals.h>
#include <hw/ui.h>
#include <InitChannels.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include <hw/init.h>

unsigned long seconds;

//int progressive_rhythm_factor;
//float noise_volume, noise_volume_max, noise_boost_by_sensor, mixed_sample_volume;
//int special_effect, selected_song, selected_melody;

int SHIFT_CHORD_INTERVAL;

//int arpeggiator_loop;

int use_alt_settings = 0;

void program_settings_reset()
{
	//run_program = 1; //enable the main program loop
	//noise_volume_max = 1.0f;
	//noise_boost_by_sensor = 1.0f;
	//special_effect = 0;
	//mixed_sample_volume = 1.0f;

    SHIFT_CHORD_INTERVAL = 2; //default every 2 seconds

    ECHO_MIXING_GAIN_MUL = 2; //amount of signal to feed back to echo loop, expressed as a fragment
    ECHO_MIXING_GAIN_DIV = 3; //e.g. if MUL=2 and DIV=3, it means 2/3 of signal is mixed in

    //ECHO_MIXING_GAIN_MUL = 4; //amount of signal to feed back to echo loop, expressed as a fragment
    //ECHO_MIXING_GAIN_DIV = 5; //e.g. if MUL=2 and DIV=3, it means 2/3 of signal is mixed in

    //echo_length_updated = 0;
    echo_dynamic_loop_length0 = echo_dynamic_loop_length;
    //echo_skip_samples = 0;
    //echo_skip_samples_from = 0;

	seconds = 0;
    sampleCounter = 0;
    //arpeggiator_loop = 0;
	//progressive_rhythm_factor = 1;
    //noise_volume = noise_volume_max;

	#ifdef BOARD_WHALE
    //reset counters and events to default values
    short_press_volume_minus = 0;
    short_press_volume_plus = 0;
    short_press_volume_both = 0;
    long_press_volume_plus = 0;
    long_press_volume_minus = 0;
	long_press_volume_both = 0;

	short_press_button_play = 0;
    play_button_cnt = 0;
    short_press_sequence = 0;
	#endif

    sample_lpf[0] = 0;
    sample_lpf[1] = 0;
}
