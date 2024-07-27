/*
 * midi.c
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

#include "midi.h"
#include "init.h"
#include "gpio.h"
#include "ui.h"

#include <math.h>
#include <string.h>

uint8_t current_MIDI_notes[MIDI_OUT_POLYPHONY] = {0,0,0};
char midi_msg_on[3] = {MIDI_MSG_NOTE_ON,0x29,0x64};
char midi_msg_off[3] = {MIDI_MSG_NOTE_OFF,0x29,0x7f};
char midi_msg_pb[3] = {MIDI_MSG_PITCH_BEND,0x00,0x00};
char midi_msg_cc[3] = {MIDI_MSG_CONT_CTRL,MIDI_CONT_CTRL_N,0x00};

float MIDI_pitch_base[MIDI_pitch_base_MAX] = {69.0f,69.0f/2.0f,69.0f*2.0f,69.0f*4.0f}; //A4 is MIDI note #69
int MIDI_pitch_base_ptr = -1;

int note_status[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};

int MIDI_note(int note, char midi_note, int on_off, int velocity)
{
	if(on_off == 1)
	{
		if(note_status[note] == 0)
		{
			note_status[note] = 1;
			midi_msg_on[1] = midi_note;
			midi_msg_on[2] = velocity;
			/*int length =*/ uart_write_bytes(MIDI_UART, midi_msg_on, 3);
			//printf("UART[%d] sent MIDI note ON message, %d bytes: %02x %02x %02x\n",MIDI_UART, length, midi_msg_on[0], midi_msg_on[1], midi_msg_on[2]);
			return 1;
		}
	}
	else if(on_off == 0)
	{
		if(note_status[note] == 1)
		{
			note_status[note] = 0;
			midi_msg_off[1] = midi_note;
			/*int length =*/ uart_write_bytes(MIDI_UART, midi_msg_off, 3);
			//printf("UART[%d] sent MIDI note OFF message, %d bytes: %02x %02x %02x\n",MIDI_UART, length, midi_msg_off[0], midi_msg_off[1], midi_msg_off[2]);
			return 1;
		}
	}
	return 0;
}

void MIDI_send_PB_CC(int pb_cc, uint8_t value)
{
	if(pb_cc==MIDI_SEND_PB)
	{
		midi_msg_pb[2] = value;
		/*int length =*/ uart_write_bytes(MIDI_UART, midi_msg_pb, 3);
		//printf("UART[%d] sent MIDI PB message, %d bytes: %02x %02x %02x\n", MIDI_UART, length, midi_msg_pb[0], midi_msg_pb[1], midi_msg_pb[2]);
	}
	else if(pb_cc==MIDI_SEND_CC)
	{
		midi_msg_cc[2] = value;
		/*int length =*/ uart_write_bytes(MIDI_UART, midi_msg_cc, 3);
		//printf("UART[%d] sent MIDI CC message, %d bytes: %02x %02x %02x\n", MIDI_UART, length, midi_msg_cc[0], midi_msg_cc[1], midi_msg_cc[2]);
	}
}

double MIDI_note_to_freq(int8_t note)
{
	if (!note)
	{
		return 0;
	}
	return NOTE_FREQ_A4 * pow(HALFTONE_STEP_COEF, note - NOTE_MIDI_A4);// + persistent_settings.TRANSPOSE);
}

float MIDI_note_to_freq_ft(float note)
{
	return NOTE_FREQ_A4 * pow(HALFTONE_STEP_COEF, note - 69.0f/* + (float)persistent_settings.TRANSPOSE*/); //A4 is MIDI note #69
}

float MIDI_note_to_coeff(int8_t note)
{
	if (!note)
	{
		return 0;
	}
	return pow(HALFTONE_STEP_COEF, note - NOTE_MIDI_A4);// + persistent_settings.TRANSPOSE);
}

const int halftone_lookup[8] = {
	0,	//a
	2,	//b
	3,	//c
	5,	//d
	7,	//e
	8,	//f
	10,	//g
	2	//h
};

int parse_note(char *note)
{
	//return strlen(note);

	int ptr=0,halftones,octave=3,octave_shift,MIDI_note=0;
	//char c;

	while(note[ptr]==' ' || note[ptr]=='\t') //skip spaces and tabs
	{
		ptr++;
	}

	if(note[ptr]>='0' && note[ptr]<='9')
	{
		return atoi(note+ptr);
	}

	if(note[ptr] >= 'a' && note[ptr] <= 'h')
	{
		if(note[ptr]=='h') note[ptr] = 'b';

		//get the note
		halftones = halftone_lookup[note[ptr] - 'a'];

		//shift according to the octave
		octave_shift = -4; //ref note is A4 - fourth octave
		if(note[ptr]>='c')
		{
			octave_shift--;
		}

		if(note[ptr+1]=='#')
		{
			halftones++;
			ptr++;
		}
		ptr++;

		//get the octave
		if(note[ptr] >= '0' && note[ptr] <= '7')
		{
			octave = note[ptr] - '0';// + 1;
		}
		else
		{
			printf("parse_note(): error, no octave information in \"%s\"!\n", note);
			while(1) {};
		}

		//the first piano key A0 = MIDI note 21
		MIDI_note = (octave + octave_shift + 4) * 12 + halftones + 21;// + persistent_settings.TRANSPOSE;
	}

	return MIDI_note;
}

void parse_timbre_range_notes(char *notes, int *ranges, int notes_n)
{
	/*
	for(int p=0;p<strlen(notes);p++)
	{
		notes[p] = tolower(notes[p]);
	}
	*/

	for(int n=0;n<notes_n;n++)
	{
		ranges[n] = parse_note(notes);
		notes = strstr(notes, ",");
		notes++;
	}
}

void calculate_timbre_parts_tuning(int *src_ranges, float *dest_tuning, int range_n)
{
	for(int n=0; n<range_n; n++)
	{
		dest_tuning[n] = pow(2, ((float)(NOTE_MIDI_A4 - src_ranges[n])) / 12.0f);
	}
}
