/*
 * midi.h
 *
 *  Copyright 2024 Phonicbloom Ltd.
 *
 *  Created on: Mar 25, 2019
 *      Author: mario
 *
 *  This file can be used within the terms of GNU GPLv3 license: https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 *  Find more information at:
 *  http://phonicbloom.com/
 *  http://gechologic.com/
 *
 */

#ifndef MIDI_H_
#define MIDI_H_

#include "init.h"

#define MIDI_CHORD_NOTES 3
#define MIDI_OUT_POLYPHONY 3

//https://ccrma.stanford.edu/~craig/articles/linuxmidi/misc/essenmidi.html
//Command	Meaning					# parameters	param 1			param 2
//0x80		Note-off				2				key				velocity
//0x90		Note-on					2				key				veolcity
//0xA0		Aftertouch				2				key				touch
//0xB0		Continuous controller	2				controller #	controller value
//0xC0		Patch change			2				instrument #
//0xD0		Channel Pressure		1				pressure
//0xE0		Pitch bend				2				lsb (7 bits)	msb (7 bits)
//0xF0		(non-musical commands)

#define MIDI_MSG_NOTE_OFF			0x80
#define MIDI_MSG_NOTE_ON			0x90

#define MIDI_MSG_CONT_CTRL			0xb0
#define MIDI_CONT_CTRL_N			0x01	//modulation wheel

#define MIDI_MSG_PITCH_BEND			0xe0

// ----------------------------------------------------------------------------------------------------------------------------------

#define MIDI_RX_CHANNEL_DEFAULT					1
#define MIDI_TX_CHANNEL_DEFAULT					1
#define MIDI_CONT_CTRL_N_DEFAULT				1

#define NOTE_MIDI_A2	45
#define NOTE_MIDI_A3	57
#define NOTE_MIDI_A4	69

#define NOTE_FREQ_A4 440.0f //440Hz of A4 note as default for calculation

#define HALFTONE_STEP_COEF 1.0594630943592952645618252949463f //(pow((double)2,(double)1/(double)12))
#define TEN_CENTS_STEP_COEF 1.0057929410678534309188527497122f //(pow((double)2,(double)1/(double)120))

// ----------------------------------------------------------------------------------------------------------------------------------

#define MIDI_pitch_base_MAX 4
extern float MIDI_pitch_base[];
extern int MIDI_pitch_base_ptr;

// ----------------------------------------------------------------------------------------------------------------------------------

#ifdef __cplusplus
extern "C" {
#endif

int MIDI_note(int note, char midi_note, int on_off, int velocity);

#define MIDI_SEND_PB	0
#define MIDI_SEND_CC	1

void MIDI_send_PB_CC(int pb_cc, uint8_t value);

double MIDI_note_to_freq(int8_t note);
float MIDI_note_to_freq_ft(float note);
float MIDI_note_to_coeff(int8_t note);
int parse_note(char *note);

void parse_timbre_range_notes(char *notes, int *ranges, int notes_n);
void calculate_timbre_parts_tuning(int *src_ranges, float *dest_tuning, int range_n);

#ifdef __cplusplus
}
#endif

#endif
