/*
 * SampleDrum.cpp
 *
 *  Copyright 2024 Phonicbloom Ltd.
 *
 *  Created on: 11 Jul 2021
 *      Author: mario
 *
 *  This file can be used within the terms of GNU GPLv3 license: https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 *  Find more information at:
 *  http://phonicbloom.com/
 *  http://gechologic.com/
 *
 */

#include <SampleDrum.h>
#include <Accelerometer.h>
#include <Interface.h>
#include <InitChannels.h>
//#include <hw/init.h>
#include <hw/signals.h>
#include <hw/gpio.h>
#include <hw/midi.h>
#include <string.h>
#include <math.h>
#include "freertos/portmacro.h"	//for portMAX_DELAY

#define ENABLE_ECHO

//#define VOICES	1
//#define VOICES	2
//#define VOICES	4
//#define VOICES	6
//#define VOICES	8
#define VOICES	9
//#define VOICES	10
//#define VOICES	12

#if VOICES == 2
double s_ptr[VOICES] = {-1,-1}, s_step[VOICES] = {S_STEP_DEFAULT,S_STEP_DEFAULT};
#endif

#if VOICES == 4
double s_ptr[VOICES] = {-1,-1,-1,-1}, s_step[VOICES] = {S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT};
#endif

#if VOICES == 5
double s_ptr[VOICES] = {-1,-1,-1,-1,-1}, s_step[VOICES] = {S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT};
#endif

#if VOICES == 6
int s_ptr[VOICES] = {-1,-1,-1,-1,-1,-1};
float s_step[VOICES] = {S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT};
#endif

#if VOICES == 8
int s_ptr[VOICES] = {-1,-1,-1,-1,-1,-1,-1,-1}, s_note[VOICES] = {0,0,0,0,0,0,0,0};
float s_step[VOICES] = {S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT};
#endif

#if VOICES == 9
int s_ptr[VOICES] = {-1,-1,-1,-1,-1,-1,-1,-1,-1}, s_note[VOICES] = {0,0,0,0,0,0,0,0,0};
float s_step[VOICES] = {S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT};
#endif

#if VOICES == 10
int s_ptr[VOICES] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, s_note[VOICES] = {0,0,0,0,0,0,0,0,0,0};
float s_step[VOICES] = {S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT};
#endif

#if VOICES == 12
int s_ptr[VOICES] = {-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1}, s_note[VOICES] = {0,0,0,0,0,0,0,0,0,0,0,0};
float s_step[VOICES] = {S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT,S_STEP_DEFAULT};
#endif

#define VOICE_OVERRIDE_RAMP_COEFF	0.99f
#define MINIMUM_MIXING_VOLUME		0.015f

float f_ptr, mixing_volumes[VOICES], voice_override_ramp = 0.0f;	// = 0.1f;
uint32_t i_ptr, s_end[VOICES];
int next_voice, sample_nr;

int16_t *MAPPED_SAMPLE_POSITION;
uint32_t MAPPED_SAMPLE_LENGHT;		//in 16-bit samples

#ifdef DOWNSAMPLING_ENABLED
int16_t *DOWNSAMPLED_BUFFER;
uint32_t DOWNSAMPLED_LENGTH;		//in 16-bit samples
int DOWNSAMPLE_FACTOR;
#endif

//#define DEBUG_FIND_NEXT_SLOT

void find_next_slot(int timbre)
{
	int n0 = new_MIDI_note[note_updated];
	int sorted = 0;

	#ifdef DEBUG_FIND_NEXT_SLOT
	printf("\nnew note = %d, next_voice = %d, voices = (%d,%d,%d,%d,%d,%d,%d,%d,%d)\n", new_MIDI_note[note_updated], next_voice, s_note[0], s_note[1], s_note[2], s_note[3], s_note[4], s_note[5], s_note[6], s_note[7], s_note[8]);
	#endif

	#if 1 //strategy to find if there are more than x of the same notes already used

	//count slots with a voice with the same note only, keep index of the oldest one
	int same_slots = 0, oldest = -1;
	for(int i=0; i<VOICES; i++)
	{
		if(s_note[(next_voice+i)%VOICES] == new_MIDI_note[note_updated])
		{
			//next_voice = (next_voice+i)%VOICES;
			same_slots++;
			if(oldest==-1)
			{
				oldest = (next_voice+i)%VOICES;
			}
		}
	}

	if(same_slots >=5)
	{
		#ifdef DEBUG_FIND_NEXT_SLOT
		printf("found %d slots using the same note %d already, oldest = %d\n", same_slots, new_MIDI_note[note_updated], oldest);
		#endif
		next_voice = oldest;
		sorted = 1;
	}

	if(n0 != new_MIDI_note[note_updated])
	{
		printf("find_next_slot(): [1]note changed %d -> %d\n", n0, new_MIDI_note[note_updated]);
		return;
	}
	#endif

	if(!sorted)
	{
		//look for an empty slot
		for(int i=0; i<VOICES; i++)
		{
			if(s_note[(next_voice+i)%VOICES] == 0)
			{
				next_voice = (next_voice+i)%VOICES;
				#ifdef DEBUG_FIND_NEXT_SLOT
				printf("will use empty slot %d\n", next_voice);
				#endif
				return;
			}
		}
	}

	/*
	#ifdef DEBUG_FIND_NEXT_SLOT
	if(s_note[next_voice] == new_MIDI_note[note_updated]) //if this would replace the same note's voice...
	{
		printf("replacing the same note %d in slot %d\n", s_note[next_voice], next_voice);
	}
	#endif
	*/

	#if 0 //strategy to find oldest same or higher note slot

	if(n0 != new_MIDI_note[note_updated])
	{
		printf("find_next_slot(): [2]note changed %d -> %d\n", n0, new_MIDI_note[note_updated]);
		return;
	}

	if(!sorted)
	{
		//look for the oldest slot with a voice with a higher note only
		for(int i=0; i<VOICES; i++)
		{
			if(s_note[(next_voice+i)%VOICES] > new_MIDI_note[note_updated])
			{
				next_voice = (next_voice+i)%VOICES;
				#ifdef DEBUG_FIND_NEXT_SLOT
				printf("will use slot %d as it contains a higher note (%d > %d)\n", next_voice, s_note[next_voice], new_MIDI_note[note_updated]);
				#endif
				sorted = 1;
				break;
			}
		}
	}

	//look for the oldest slot with a voice with the same note only
	for(int i=0; i<VOICES; i++)
	{
		if(s_note[(next_voice+i)%VOICES] == new_MIDI_note[note_updated])
		{
			next_voice = (next_voice+i)%VOICES;
			#ifdef DEBUG_FIND_NEXT_SLOT
			printf("will use slot %d as it contains the same note (%d)\n", next_voice, s_note[next_voice]);
			#endif
			sorted = 1;
			break;
		}
	}

	#endif

	#if 1 //strategy to replace another voice playing the same note only

	//if(s_note[next_voice] != new_MIDI_note[note_updated]) //if this would replace another note's voice...
	if(!sorted)
	{
		//printf("[1]\n");
		for(int i=0; i<VOICES; i++)
		{
			//if(s_note[(next_voice+i)%VOICES] == new_MIDI_note[note_updated] //find if there is already another voice playing the same note...
			//&& new_notes_timbre[note_updated] == timbre) //...but it must have the same timbre too

			//do not care about timbre part
			if(s_note[(next_voice+i)%VOICES] == new_MIDI_note[note_updated]) //find if there is already another voice playing the same note
			{	//found it, kick out this one
				next_voice = (next_voice+i)%VOICES;
				#ifdef DEBUG_FIND_NEXT_SLOT
				printf("[same note playing] kicking out voice %d\n", next_voice);
				#endif
				sorted = 1;
				break;
			}
		}
	}

	if(n0 != new_MIDI_note[note_updated])
	{
		printf("find_next_slot(): [3]note changed %d -> %d\n", n0, new_MIDI_note[note_updated]);
		return;
	}
	#endif

	#if 1 //strategy to find two slots occupied by the same sample, but playing a different / higher note than the new one

	//if previous attempt was not successfull
	if(!sorted)// && s_note[next_voice] != new_MIDI_note[note_updated]) //if this would replace another note's voice...
	{
		/*
		#ifdef DEBUG_FIND_NEXT_SLOT
		printf("[2]: %d != %d\n", s_note[next_voice], new_MIDI_note[note_updated]);
		#endif
		*/

		/*
		//find the one that has progressed mostly
		float max_val = 0, current_pos;
		//int max_pos = -1;

		for(int i=0; i<VOICES; i++)
		{
			current_pos = ((float)s_ptr[i]) * s_step[i];
			if(current_pos > max_val)
			{
				max_val = current_pos;
				//max_pos = i;
				next_voice = i;
			}
			//next_voice = max_pos;
		}
		//printf("[replacing voice] kicking out sample at slot %d as it has progressed to %f\n", next_voice, ((float)s_ptr[next_voice]) * s_step[next_voice]);
		*/

		//find if there are two slots occupied by the same sample, but playing a different / higher note than the new one
		for(int i=0; i<VOICES-1; i++)
		{
			for(int j=1; j<VOICES; j++)
			{
				//any different note can be kicked out
				if(i!=j && s_note[(next_voice+i)%VOICES]==s_note[(next_voice+j)%VOICES] && s_note[(next_voice+i)%VOICES]!=new_MIDI_note[note_updated])
				//only a higher note can be kicked out (lowest note protected)
				//if(i!=j && s_note[(next_voice+i)%VOICES]==s_note[(next_voice+j)%VOICES] && s_note[(next_voice+i)%VOICES]>new_MIDI_note[note_updated])
				{
					#ifdef DEBUG_FIND_NEXT_SLOT
					printf("slots %d and %d both contain note %d\n", (next_voice+i)%VOICES, (next_voice+i)%VOICES, s_note[(next_voice+j)%VOICES]);
					#endif

					if(s_ptr[(next_voice+i)%VOICES]>s_ptr[(next_voice+j)%VOICES])
					{
						#ifdef DEBUG_FIND_NEXT_SLOT
						printf("slot %d has progressed further than %d\n", (next_voice+i)%VOICES, (next_voice+j)%VOICES);
						#endif
						next_voice = (next_voice+i)%VOICES;
						i=VOICES;
						j=VOICES;
						sorted = 1;
						break;
					}
					else
					{
						#ifdef DEBUG_FIND_NEXT_SLOT
						printf("slot %d has progressed further than %d\n", (next_voice+j)%VOICES, (next_voice+i)%VOICES);
						#endif
						next_voice = (next_voice+j)%VOICES;
						i=VOICES;
						j=VOICES;
						sorted = 1;
						break;
					}
				}
			}
		}
	}

	if(n0 != new_MIDI_note[note_updated])
	{
		printf("find_next_slot(): [4]note changed %d -> %d\n", n0, new_MIDI_note[note_updated]);
		return;
	}
	#endif

	#if 1

	if(!sorted)
	{
		//look for the oldest slot with a voice with higher or the same note only
		for(int i=0; i<VOICES; i++)
		{
			if(s_note[(next_voice+i)%VOICES] >= new_MIDI_note[note_updated])
			{
				next_voice = (next_voice+i)%VOICES;
				#ifdef DEBUG_FIND_NEXT_SLOT
				printf("will use slot %d as it contains the same or higher note (%d >= %d)\n", next_voice, s_note[next_voice], new_MIDI_note[note_updated]);
				#endif
				sorted = 1;
				break;
			}
		}
	}
	#endif

	if(!sorted)
	{
		printf("new voice slot still not foud!\n");
	}

	if(s_ptr[next_voice] >= 0) //if still playing
	{
		//i_ptr = s_ptr[next_voice];// / SAMPLE_PTR_MULTIPLIER;
		//i_ptr *= 2;

		f_ptr = ((float)s_ptr[next_voice]) * s_step[next_voice];
		//i_ptr = (uint32_t)f_ptr + SAMPLE_POSITION[sample];
		//i_ptr *= 2;

		i_ptr = (int)f_ptr;
		f_ptr -= (float)i_ptr;

		//float sample_amplitude = ((float)(((int16_t *)((unsigned int)samples_ptr1 + i_ptr))[0])) * mixing_volumes[next_voice];
		//float sample_amplitude = ((float)(samples_ptr1[i_ptr])) * mixing_volumes[next_voice];

		/*
		i_ptr += SAMPLE_POSITION[sample_nr];
		float sample_amplitude = ((1 - f_ptr) * ((float)(samples_ptr1[i_ptr])) + f_ptr * ((float)(samples_ptr1[i_ptr+1]))) * mixing_volumes[next_voice];
		*/
		float sample_amplitude = ((1 - f_ptr) * ((float)(MAPPED_SAMPLE_POSITION[i_ptr])) + f_ptr * ((float)(MAPPED_SAMPLE_POSITION[i_ptr+1]))) * mixing_volumes[next_voice];
		//printf("sample_amplitude = %f, voice_override_ramp = %f\n", sample_amplitude, voice_override_ramp);

		voice_override_ramp += sample_amplitude;

		//printf("kicking out voice %d (position = %d), amp = %f, [%d -> %d]\n", next_voice, s_ptr[next_voice], sample_amplitude, s_note[next_voice], new_MIDI_note[note_updated]);
	}
	else
	{
		//printf("reused inactive slot at %d\n", next_voice);
		printf("find_next_slot(): unexpected result!\n");
	}
}

#define SAMPLE_TIMBRES_MAX	4

//int timbre_map_records;
int *timbre_map = NULL;
int timbre_map_size = 0;

//int timbre_parts[] = {0,639620-88,639620,346744-88,986364,363876-88,1350240,679916-88};
//int timbre_parts[] = {0,679916,0,363876,0,639620,0,346744};
int timbre_parts[SAMPLE_TIMBRES_MAX*2];
int sample_timbres;
int timbre_ranges[SAMPLE_TIMBRES_MAX];
//char *timbre_range_notes = "f2,f3,g#3,c4";
//float timbre_parts_tuning[] = {2.0f,1.0f,0.840896415253,0.667419927085};
//float timbre_parts_tuning[] = {5.039684199579492659,2.5198420997897463,2.11892618871859,1.681792830507429}; //relative to A4
float timbre_parts_tuning[SAMPLE_TIMBRES_MAX];


void load_patch_sample_timbres()
{
	char prefix[10];
	sprintf(prefix,"seg_%d:",sample_nr);
	printf("get_line_in_block(): looking up line in block [timbre_segments] with prefix \"%s\"\n", prefix);
	char *timbre_segments = get_line_in_block("[timbre_segments]", prefix);
	if(timbre_segments==NULL)
	{
		printf("get_line_in_block() returned NULL\n");
		sample_timbres = 0;
	}
	else
	{
		printf("get_line_in_block() returned timbre_segments: \"%s\"\n", timbre_segments);

		sample_timbres = SAMPLE_TIMBRES_MAX;

		char *line_ptr = timbre_segments;
		int part = 0;

		line_ptr = strstr(line_ptr,":[") + 2;
		while(1)
		{
			timbre_parts[2*part+1] = atoi(line_ptr);
			printf("found sample timbre #%d: %d\n", part, timbre_parts[2*part+1]);
			part++;
			while(line_ptr[0]!=',' && line_ptr[0]!=']')line_ptr++;
			if(line_ptr[0]==']' || part==SAMPLE_TIMBRES_MAX)
			{
				break;
			}
			line_ptr++;
		}
		sample_timbres = part;
		printf("found %d sample_timbres\n", sample_timbres);

		int sum_length = 0;
		for(int t=0;t<sample_timbres;t++)
		{
			timbre_parts[t*2] = sum_length;
			sum_length += timbre_parts[t*2+1];
			timbre_parts[t*2+1] -= global_settings.SAMPLE_FORMAT_HEADER_LENGTH * 2;

			//suspicious inconsistent bug fix attempt (seems to help but not sure why)
			timbre_parts[t*2] += global_settings.SAMPLE_FORMAT_HEADER_LENGTH / 2;
			timbre_parts[t*2+1] -= global_settings.SAMPLE_FORMAT_HEADER_LENGTH / 2;
		}
		printf("timbre_parts[] = %d,%d,%d,%d,%d,%d,%d,%d\n", timbre_parts[0], timbre_parts[1], timbre_parts[2], timbre_parts[3], timbre_parts[4], timbre_parts[5], timbre_parts[6], timbre_parts[7]);

		line_ptr = strstr(line_ptr,"[") + 1;
		char *timbre_range_notes = line_ptr;
		line_ptr = strstr(line_ptr,"]");
		line_ptr[0] = 0;

		printf("sample_drum(): parse_timbre_range_notes(%s)\n", timbre_range_notes);
		parse_timbre_range_notes(timbre_range_notes, timbre_ranges, sample_timbres);
		printf("parse_timbre_range_notes() %s => %d,%d,%d,%d\n", timbre_range_notes, timbre_ranges[0], timbre_ranges[1], timbre_ranges[2], timbre_ranges[3]);

		calculate_timbre_parts_tuning(timbre_ranges, timbre_parts_tuning, sample_timbres);
		printf("calculate_timbre_parts_tuning() => %f,%f,%f,%f\n", timbre_parts_tuning[0], timbre_parts_tuning[1], timbre_parts_tuning[2], timbre_parts_tuning[3]);

		float timbre_parts_adjustment[SAMPLE_TIMBRES_MAX];

		line_ptr++;
		line_ptr = strstr(line_ptr,"[") + 1;
		for(int t=0;t<sample_timbres;t++)
		{
			timbre_parts_adjustment[t] = atof(line_ptr);
			printf("found sample timbre adjustment #%d: %f\n", t, timbre_parts_adjustment[t]);
			timbre_parts_tuning[t] *= timbre_parts_adjustment[t];

			while(line_ptr[0]!=',' && line_ptr[0]!=']')line_ptr++;
			if(line_ptr[0]==']')
			{
				break;
			}
			line_ptr++;
		}
		printf("adjusted timbre_parts_tuning[] = %f,%f,%f,%f\n", timbre_parts_tuning[0], timbre_parts_tuning[1], timbre_parts_tuning[2], timbre_parts_tuning[3]);

		free(timbre_segments);
	}

	if(sample_timbres)
	{
		sprintf(prefix,"map_%d:",sample_nr);
		printf("get_line_in_block(): looking up line in block [timbre_segments] with prefix \"%s\"\n", prefix);
		char *timbre_map_str = get_line_in_block("[timbre_segments]", prefix);
		if(timbre_map_str==NULL)
		{
			printf("get_line_in_block() returned NULL\n");
			//timbre_map_records = 0;

			printf("sample_drum(): segments defined but no mapping in [timbre_segments]\n");
			error_blink(ERROR_BLINK_PATTERN_1256_3478, 200); //blink forever
		}
		else
		{
			int timbre_map_length = 1;

			for(int i=0;i<strlen(timbre_map_str);i++)
			{
				if(timbre_map_str[i]==',' || timbre_map_str[i]=='/') timbre_map_length++;
			}
			printf("timbre_map_str seems to contain %d items\n", timbre_map_length);
			timbre_map = (int*)malloc(timbre_map_length * sizeof(int));
			timbre_map_size = timbre_map_length / sample_timbres;
			printf("timbre_map_size = %d\n", timbre_map_size);

			char *ptr = strstr(timbre_map_str,"[") + 1;
			int p=0;
			printf("found sample timbre map item: ");
			while(1)
			{
				timbre_map[p] = parse_note(ptr);
				printf("#%d: %d, ", p, timbre_map[p]);

				while(ptr[0]!=',' && ptr[0]!='/' && ptr[0]!=' ' && ptr[0]!=']')ptr++;
				if(ptr[0]==']')
				{
					break;
				}
				while(ptr[0]==',' || ptr[0]=='/' || ptr[0]==' ' || ptr[0]==']')ptr++;
				p++;
			}
			printf("\n");

			free(timbre_map_str);
		}
	}
}

IRAM_ATTR void sample_drum(int sample, float tuning, int *notes, int patch_number)
{
	printf("sample_drum(sample=%d, tuning=%f, *notes=%p, patch_number=%d)\n", sample, tuning, notes, patch_number);

	//clean up re-used variables

	//printf("sample_drum(): voice_override_ramp[1] = %f\n", voice_override_ramp);
	voice_override_ramp = 0.0f;
	//printf("sample_drum(): voice_override_ramp[2] = %f\n", voice_override_ramp);

	next_voice = 0;
	sample_nr = sample;

	timbre_map = NULL;
	timbre_map_size = 0;
	for(int i=0;i<VOICES;i++)
	{
		s_ptr[i] = -1;
		s_end[i] = 0;
		s_note[i] = 0;
		s_step[i] = S_STEP_DEFAULT;
		mixing_volumes[i] = 0;
	}

	//printf("----SAMPLE DRUM-------[start] Free heap before channel_init(): %u\n", xPortGetFreeHeapSize());

    //program_settings_reset();
	channel_init(sample, 0, 0, 0, 0, 0, 0, 0, 0); //init without any features except for loading the sample

	#ifdef LPF_ENABLED
    ADC_LPF_ALPHA = 0.2f;
	#endif

	//int vo;
	//float f_s1, f_s2, intpart;
	//int16_t *mem_ptr;

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

	//echo_dynamic_loop_length = ECHO_BUFFER_LENGTH_DEFAULT;

    ECHO_MIXING_GAIN_MUL = 0;//3;//1;//9; //amount of signal to feed back to echo loop, expressed as a fragment
    ECHO_MIXING_GAIN_DIV = 10;//4;//8;//10; //e.g. if MUL=2 and DIV=3, it means 2/3 of signal is mixed in

	#define ECHO_MIXING_GAIN_MUL_STEP	0.01
    ECHO_MIXING_GAIN_MUL_TARGET = ECHO_MIXING_GAIN_MUL;

	//int bit_crusher_reverb_d = I2S_AUDIOFREQ / 40 + 1;
    //reverb_dynamic_loop_length = bit_crusher_reverb_d;

    //printf("----SAMPLE DRUM-------[loop] Free heap before channel loop: %u\n", xPortGetFreeHeapSize());

    sample_ADC_LPF = 0;
	#define sample_ADC_LPF_ALPHA	0.95

	#ifdef DYNAMICS_BY_ADC
	memset(sample_ADC_dynamic_RB, 0, sizeof(sample_ADC_dynamic_RB));
	sample_ADC_dyn_RB_ptr = 0;
	#endif

	int pb_cc[4];

	current_patch = patch_number;
	patch_notes = notes;

	//decode patch timbres and mapping information
	load_patch_sample_timbres();

	//load user-defined scales from NVS if present
	load_patch_scales(current_patch, patch_notes);
	load_micro_tuning(current_patch, micro_tuning);

	midi_scale_selected = &patch_notes[selected_scale[current_patch]*NOTES_PER_SCALE];
	micro_tuning_selected = &micro_tuning[selected_scale[current_patch]*NOTES_PER_SCALE];
    printf("sample_drum(): micro_tuning_selected = (%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f)\n",
    		micro_tuning_selected[0],
    		micro_tuning_selected[1],
    		micro_tuning_selected[2],
    		micro_tuning_selected[3],
    		micro_tuning_selected[4],
    		micro_tuning_selected[5],
    		micro_tuning_selected[6],
    		micro_tuning_selected[7],
    		micro_tuning_selected[8]);

    center_y = 0;
    center_y += (int)acc_res[1];
    center_y += (int)acc_res[1];
    center_y += (int)acc_res[1];
    center_y /= 3;

    ignore_acc_octave_switch = IGNORE_ACC_OCTAVE_SW_RESET;

    touch_pad_enabled = 1;

    codec_silence(100);
    codec_set_mute(0); //enable the sound

    while(!event_next_channel)
	{
		sampleCounter++;

		//t_TIMING_BY_SAMPLE_EVERY_250_MS = TIMING_EVERY_250_MS;
		//t_TIMING_BY_SAMPLE_EVERY_25_MS = TIMING_EVERY_25_MS;

		//if (TIMING_EVERY_25_MS == 31) //40Hz
		//if (TIMING_EVERY_5_MS == 31) //200Hz
		//if (TIMING_EVERY_4_MS == 31) //250Hz
		//if (TIMING_EVERY_2_MS == 31) //250Hz
		if (TIMING_EVERY_1_MS == 31) //250Hz
		{
			if(note_updated>=0)
			{
				if(auto_power_off < AUTO_POWER_OFF_VOLUME_RAMP)
				{
					auto_power_off_canceled = 1;
				}
				auto_power_off = global_settings.AUTO_POWER_OFF_TIMEOUT_ALL; //extend the auto power off timeout

				int timbre_part = -1;

				if(sample_timbres)
				{
					int use_timbre_part;
					if(new_touch_event[note_updated]>SPLIT_TIMBRE_NOTE_THRESHOLD2_A[note_updated_pad] && timbre_map_size>=3)
					{
						//printf("will use timbre part #2\n");
						use_timbre_part = 2;
					}
					else if(new_touch_event[note_updated]>SPLIT_TIMBRE_NOTE_THRESHOLD1_A[note_updated_pad] && timbre_map_size>=2)
					{
						//printf("will use timbre part #1\n");
						use_timbre_part = 1;
					}
					else
					{
						//printf("will use timbre part #0\n");
						use_timbre_part = 0;
					}

					//printf("looking up nearest applicable timbre part for note %d\n", s_note[next_voice]);

					int diff, min_diff = 999; //for lookup of the nearest note

					/*
					for(int t=0; t<sample_timbres; t++)
					{
						diff = timbre_ranges[t] - s_note[next_voice];
						if(diff<0) { diff *= -1; }
						if(diff<min_diff)
						{
							min_diff = diff;
							timbre_part = t;
						}
					}
					*/

				    for(int t=0; t<sample_timbres; t++)
					{
						int map_value = timbre_map[use_timbre_part*sample_timbres+t];
						if(map_value)
						{
							diff = map_value - s_note[next_voice];
							if(diff<0) { diff *= -1; }
							if(diff<min_diff)
							{
								min_diff = diff;
								timbre_part = t;
							}
						}
					}
				}

				new_notes_timbre[note_updated] = timbre_part;

				//if(s_ptr[next_voice] >= 0) //if still playing
				//{
					find_next_slot(timbre_part);
					//printf("%d\n", next_voice);
				//}

				//s_step[next_voice] = S_STEP_DEFAULT/4 + (double)note_updated * S_STEP_DEFAULT/4;
				//s_step[next_voice] = S_STEP_DEFAULT/8 + (double)new_MIDI_note[note_updated] * S_STEP_DEFAULT/8;
				//s_step[next_voice] = MIDI_note_to_freq(new_MIDI_note[note_updated]) / 440.0f;

				//s_step[next_voice] = 1000 * MIDI_note_to_coeff(new_MIDI_note[note_updated]);
				//s_step[next_voice] /= 1000;

				s_step[next_voice] = MIDI_note_to_coeff(new_MIDI_note[note_updated]) * tuning * micro_tuning_selected[note_updated];
				s_note[next_voice] = new_MIDI_note[note_updated];
				//printf("sample_drum(): s_step[%d]=%.20lf\n", next_voice, s_step[next_voice]);

				if(sample_timbres)
				{
					//printf("thresholds for pad[%d]: %d,%d,%d\n", note_updated_pad, TOUCHPAD_LED_THRESHOLD_A[note_updated_pad], SPLIT_TIMBRE_NOTE_THRESHOLD1_A[note_updated_pad], SPLIT_TIMBRE_NOTE_THRESHOLD2_A[note_updated_pad]);
					//printf("nearest applicable timbre for note %d and velocity level %d = %d (part #%d)\n", s_note[next_voice], new_touch_event[note_updated], timbre_ranges[timbre_part], timbre_part);

					s_step[next_voice] *= timbre_parts_tuning[timbre_part];

					s_ptr[next_voice] = timbre_parts[timbre_part*2] / 2; //position
					s_end[next_voice] = s_ptr[next_voice] + timbre_parts[timbre_part*2+1] / 2; //length
					//printf("s_ptr[%d] = %d, s_end[%d] = %d\n", next_voice, s_ptr[next_voice], next_voice, s_end[next_voice]);
					s_ptr[next_voice] /= s_step[next_voice];
				}
				else
				{
					s_ptr[next_voice] = 0;//SAMPLE_POSITION[sample_nr];
					s_end[next_voice] = MAPPED_SAMPLE_LENGHT;
				}

				mixing_volumes[next_voice] = (float)new_touch_event[note_updated] / 5000.0f;
				//printf("mixing_volumes[next_voice]=%f\n", mixing_volumes[next_voice]);
				if(mixing_volumes[next_voice]<MINIMUM_MIXING_VOLUME)
				{
					mixing_volumes[next_voice] = MINIMUM_MIXING_VOLUME;
				}

				//printf("sample_drum(): level = %d, mixing_volumes[%d]=%f\n", new_touch_event[note_updated], next_voice, mixing_volumes[next_voice]);
				note_updated = -1;

				next_voice++;

				if(next_voice==VOICES)
				{
					next_voice = 0;
				}
			}
		}

		sample_mix = 0;

		for(int vo=0;vo<VOICES;vo++)
		{
			if(s_ptr[vo] >= 0)
			{
				f_ptr = ((float)s_ptr[vo]) * s_step[vo];
				i_ptr = (int)f_ptr;
				f_ptr -= (float)i_ptr;
				//f_ptr = modf(f_ptr, &intpart);

				//if(i_ptr > SAMPLE_LENGHT[sample_nr]) //SAMPLE_END[sample_nr])
				if(i_ptr > s_end[vo])//MAPPED_SAMPLE_LENGHT)
				{
					s_ptr[vo] = -1;
					s_note[vo] = 0;
				}
				else
				{
					/*
					i_ptr += SAMPLE_POSITION[sample_nr];
					*/

					//i_ptr *= 2;

					//sample_mix += ((float)(((int16_t *)((unsigned int)samples_ptr1 + i_ptr))[0])) * mixing_volumes[vo]/2;
					//sample_mix += ((float)(((int16_t *)((unsigned int)samples_ptr1 + i_ptr))[1])) * mixing_volumes[vo]/2;

					//sample_mix += ((float)(samples_ptr1[i_ptr])) * mixing_volumes[vo];///2;
					//sample_mix += ((float)(((int16_t *)(samples_ptr1[i_ptr+1])) * mixing_volumes[vo]/2;

					/*
					mem_ptr = ((int16_t *)((unsigned int)samples_ptr1 + i_ptr));

					f_s1 = (float)(mem_ptr[0]);
					//f_s2 = (float)(mem_ptr[1]);
					//sample_mix += ((1 - f_ptr) * f_s1 + f_ptr * f_s2) * mixing_volumes[vo];
					sample_mix += f_s1 * mixing_volumes[vo];

					//sample_mix += ((float)(mem_ptr[0])) * mixing_volumes[vo];
					*/

					//sample_mix += ((1 - f_ptr) * ((float)(((int16_t *)((unsigned int)samples_ptr1 + i_ptr))[0])) + f_ptr * ((float)(((int16_t *)((unsigned int)samples_ptr1 + i_ptr))[1]))) * mixing_volumes[vo];
					/*
					sample_mix += ((1 - f_ptr) * ((float)(samples_ptr1[i_ptr])) + f_ptr * ((float)(samples_ptr1[i_ptr+1]))) * mixing_volumes[vo];
					*/
					sample_mix += ((1 - f_ptr) * ((float)(MAPPED_SAMPLE_POSITION[i_ptr])) + f_ptr * ((float)(MAPPED_SAMPLE_POSITION[i_ptr+1]))) * mixing_volumes[vo];

					//sample_mix += (((float)(samples_ptr1[i_ptr])) + ((float)(samples_ptr1[i_ptr+1]))) * mixing_volumes[vo] / 2;
					//sample_mix += ((float)(samples_ptr1[i_ptr])) * mixing_volumes[vo];

					s_ptr[vo]++;// += s_step[vo];
				}
			}
		}

		#if 0 //VOICE_OVERRIDE_RAMP_LINEAR
		if(voice_override_ramp > 1)
		{
			sample_mix += voice_override_ramp;
			voice_override_ramp -= 1.0f;
			//printf("%f,", voice_override_ramp);
		}

		if(voice_override_ramp < -1)
		{
			sample_mix += voice_override_ramp;
			voice_override_ramp += 1.0f;
			//printf("%f,", voice_override_ramp);
		}
		#endif

		#if 1 //VOICE_OVERRIDE_RAMP_EXP
		//if(voice_override_ramp > 0.01f || voice_override_ramp < -0.01f)
		if(voice_override_ramp > 1.0f || voice_override_ramp < -1.0f)
		{
			sample_mix += voice_override_ramp;
			voice_override_ramp *= VOICE_OVERRIDE_RAMP_COEFF;//0.75f;//0.99f;
			//printf("%f,", voice_override_ramp);
		}
		#endif

		//sample32 = ((int16_t)(sample_mix)) << 16;
        //sample32 += (int16_t)(sample_mix);

		#ifndef ENABLE_ECHO
		sample32 = ((uint16_t)(sample_mix)) << 16;
        sample32 += (uint16_t)(sample_mix);
		#else
		sample32 = (add_echo((int16_t)(sample_mix))) << 16;
		//sample32 = (add_reverb((int16_t)(sample_mix))) << 16;
		//sample32 = (add_echo(add_reverb((int16_t)(sample_mix)))) << 16;

        sample32 += add_echo((int16_t)(sample_mix));
        //sample32 += add_reverb((int16_t)(sample_mix));
        //sample32 += add_echo(add_reverb((int16_t)(sample_mix)));
		#endif

        /*
		i2s_read(I2S_NUM, (void*)&ADC_sample, 4, &i2s_bytes_rw, portMAX_DELAY);

		sample_i16 = abs((int16_t)ADC_sample);

		sample_ADC_dynamic_RB[sample_ADC_dyn_RB_ptr] = sample_i16;
		sample_ADC_dyn_RB_ptr++;
		if(sample_ADC_dyn_RB_ptr==ADC_DYN_RB)
		{
			sample_ADC_dyn_RB_ptr = 0;
		}
		*/

		//sample_ADC_LPF = sample_ADC_LPF + int16_t(sample_ADC_LPF_ALPHA * ((float)(sample_i16 - sample_ADC_LPF)));

		/*
		if(sampleCounter%50==0 && sample_i16>500)
		{
			printf("sample_i16 = %d, sample_ADC_LPF = %d\n", sample_i16, sample_ADC_LPF);
		}
		*/

		/*
		//sample_mix = (int16_t)ADC_sample;
		//sample_mix = sample_mix * SAMPLE_VOLUME_REVERB; //apply volume
		//LPF enabled
		sample_lpf[0] = sample_lpf[0] + ADC_LPF_ALPHA * ((float)((int16_t)ADC_sample) - sample_lpf[0]);
		sample_mix = sample_lpf[0] * SAMPLE_VOLUME_REVERB; //apply volume


		//sample32 = (add_echo((int16_t)(sample_mix))) << 16;
		//sample32 = (add_reverb((int16_t)(sample_mix))) << 16;
		sample32 = (add_echo(add_reverb((int16_t)(sample_mix)))) << 16;

        //sample_mix = (int16_t)(ADC_sample>>16);
        //sample_mix = sample_mix * SAMPLE_VOLUME_REVERB; //apply volume
		//LPF enabled
		sample_lpf[1] = sample_lpf[1] + ADC_LPF_ALPHA * ((float)((int16_t)(ADC_sample>>16)) - sample_lpf[1]);
		sample_mix = sample_lpf[1] * SAMPLE_VOLUME_REVERB; //apply volume

        //sample32 += add_echo((int16_t)(sample_mix));
        //sample32 += add_reverb((int16_t)(sample_mix));
        sample32 += add_echo(add_reverb((int16_t)(sample_mix)));
        */

		i2s_write(I2S_NUM, &sample32, 4, &i2s_bytes_rw, portMAX_DELAY);

		#ifdef DYNAMICS_BY_ADC

		if(skip_ADC_samples)
		{
			skip_ADC_samples--;
		}
		else
		{
			i2s_read(I2S_NUM, (void*)&ADC_sample, 4, &i2s_bytes_rw, portMAX_DELAY);
		}
		#endif

		/*
		if (TIMING_EVERY_20_MS == 33) //50Hz
		{
			if(limiter_coeff < DYNAMIC_LIMITER_COEFF_DEFAULT)
			{
				limiter_coeff += DYNAMIC_LIMITER_COEFF_DEFAULT / 20; //limiter will fully recover within 0.4 second
			}
		}
		*/

		#if 1
		//if (TIMING_EVERY_250_MS == 33) //4Hz
		//if (TIMING_EVERY_125_MS == 33) //8Hz
		if (TIMING_EVERY_100_MS == 33) //10Hz
		//if (TIMING_EVERY_50_MS == 33) //20Hz
		//if (TIMING_EVERY_25_MS == 33) //40Hz
		{
			#if 0//def ACC_CALIBRATION_ENABLED
			if(acc_calibrating>1)
			{
				acc_calibrating--;
			}
			else if(acc_calibrating==1)
			{
				acc_calibrating = 0;

				acc_calibrated[0] = acc_res[0];
				acc_calibrated[1] = acc_res[1];

				printf("Accelerometer calibrated: %f / %f\n", acc_calibrated[0], acc_calibrated[1]);

				pb_cc[1] = 64 + (int)acc_res[0]; //pb
				//if(pb_cc[1]>127) { pb_cc[1] = 127; }
				//if(pb_cc[1]<0) { pb_cc[1] = 0; }
				pb_cc[0] = pb_cc[1];

				pb_cc[3] = 64 + (int)acc_res[1];  //cc
				//if(pb_cc[3]>127) { pb_cc[3] = 127; }
				//if(pb_cc[3]<0) { pb_cc[3] = 0; }
				pb_cc[2] = pb_cc[3];

			}
			else
			{
			#endif

				pb_cc[1] = (int)(acc_res[0] - acc_calibrated[0]);
				//pb_cc[1] = (int)acc_res[0]; //pb

				if(pb_cc[1]<-10)
				{
					pb_cc[1] += 10;
				}
				else if(pb_cc[1]>10)
				{
					pb_cc[1] -= 10;
				}
				else
				{
					pb_cc[1] = 0;
				}

				pb_cc[1] += 64;
				if(pb_cc[1]>127) { pb_cc[1] = 127; }
				if(pb_cc[1]<0) { pb_cc[1] = 0; }

				if(pb_cc[1]!=pb_cc[0]) //pb updated
				{
					//printf("Accelerometer: PB updated: %d -> %d\n", pb_cc[0], pb_cc[1]);
					pb_cc[0] = pb_cc[1];

					MIDI_send_PB_CC(MIDI_SEND_PB, pb_cc[0]);
				}

			#if 0//def ACC_CALIBRATION_ENABLED
			}
			#endif
		}

		#if 1
		//if (TIMING_EVERY_100_MS == 37) //10Hz
		if (TIMING_EVERY_250_MS == 37) //4Hz
		{
			//pb_cc[3] = (int)(acc_res[1] - acc_calibrated[1]);

			//pb_cc[3] = 64 - (int)acc_res[1]; //cc with reversed axis, 64 in the middle
			//pb_cc[3] = - (int)acc_res[1]; //cc with reversed axis, 0 in the middle
			pb_cc[3] = (int)(acc_res[1] - acc_calibrated[1]);

			pb_cc[3] -= 15; //margin for not precisely leveled drum

			if(pb_cc[3]>127) { pb_cc[3] = 127; }
			if(pb_cc[3]<0) { pb_cc[3] = 0; }

			if(pb_cc[3]!=pb_cc[2]) //cc updated
			{
				//printf("Accelerometer: CC updated: %d -> %d\n", pb_cc[3], pb_cc[2]);
				pb_cc[2] = pb_cc[3];

				MIDI_send_PB_CC(MIDI_SEND_CC, pb_cc[2]);
			}
		}

		#if 0
		//if (TIMING_EVERY_100_MS == 2113) //10Hz
		if (TIMING_EVERY_50_MS == 2113) //20Hz = same as process_accelerometer loop delay
		//if (TIMING_EVERY_25_MS == 337) //40Hz = won't work, too fast
		{
			//printf("%f-%f-%f, center_y = %d\n", acc_res[0], acc_res[1], acc_res[2], center_y);

			#ifdef OCTAVE_SWITCH_X
			if(acc_res[0] < THRESHOLD_SEGMENT_X_LEFT)
			{
				segment_x = -1;
			}
			else if(acc_res[0] > THRESHOLD_SEGMENT_X_RIGHT)
			{
				segment_x = 1;
			}
			else
			{
				segment_x = 0;
			}
			if(segment_x != segment_x0)
			{
				printf("x-axis segment changed: %d\n", segment_x);
				segment_x0 = segment_x;

				if(segment_x == 1)
				{
					change_scale_up();
				}
				if(segment_x == -1)
				{
					change_scale_down();
				}
			}
			#endif

			#ifdef OCTAVE_SWITCH_Y

			if(ignore_acc_octave_switch)
			{
				ignore_acc_octave_switch--;
			}
			else
			{
				if(acc_res[1] - center_y < THRESHOLD_SEGMENT_Y_DOWN)
				{
					segment_y = -1;
					printf("acc_res[1] - center_y (%f) < THRESHOLD_SEGMENT_Y_UP (%d)\n", acc_res[1] - center_y, THRESHOLD_SEGMENT_Y_DOWN);
				}
				else if(acc_res[1] - center_y > THRESHOLD_SEGMENT_Y_UP)
				{
					segment_y = 1;
					printf("acc_res[1] - center_y (%f) > THRESHOLD_SEGMENT_Y_UP (%d)\n", acc_res[1] - center_y, THRESHOLD_SEGMENT_Y_UP);
				}
				else
				{
					segment_y = 0;
				}

				if(segment_y != segment_y0)
				{
					printf("y-axis segment changed: %d-%d-%d\n", segment_y1, segment_y0, segment_y);

					/*
					if(segment_y == 0 && segment_y1 == 0)
					{
						printf("y-axis segment changed there and back\n");

						ignore_acc_octave_switch = IGNORE_ACC_OCTAVE_SW_RESET;

						if(segment_y0 == 1)
						{
							change_scale_up();
						}
						if(segment_y0 == -1)
						{
							change_scale_down();
						}
					}
					*/

					if(segment_y0 == 0)
					{
						printf("y-axis segment changed\n");

						ignore_acc_octave_switch = IGNORE_ACC_OCTAVE_SW_RESET;

						if(segment_y == 1)
						{
							change_scale_up();
						}
						if(segment_y == -1)
						{
							change_scale_down();
						}
					}

					segment_y1 = segment_y0;
					segment_y0 = segment_y;
				}
			}

			center_y = (center_y + acc_res[1]) / 2;
			#endif
		}
		#endif

		#ifdef DELAY_LENGTH_BY_ACCELEROMETER
		if (TIMING_EVERY_250_MS == 4999) //4Hz
		{
			if(SENSOR_DELAY_ACTIVE && !scale_settings)
			{
				if(SENSOR_DELAY_9)
				{
					//echo_dynamic_loop_length = ECHO_BUFFER_LENGTH_DEFAULT / 64;
					echo_dynamic_loop_length = ECHO_BUFFER_LENGTH_DEFAULT;
				}
				else if(SENSOR_DELAY_8)
				{
					//echo_dynamic_loop_length = ECHO_BUFFER_LENGTH_DEFAULT / 32;
					echo_dynamic_loop_length = ECHO_BUFFER_LENGTH_DEFAULT / 3 * 2;
				}
				else if(SENSOR_DELAY_7)
				{
					//echo_dynamic_loop_length = ECHO_BUFFER_LENGTH_DEFAULT / 16;
					echo_dynamic_loop_length = ECHO_BUFFER_LENGTH_DEFAULT / 2;
				}
				else if(SENSOR_DELAY_6)
				{
					//echo_dynamic_loop_length = ECHO_BUFFER_LENGTH_DEFAULT / 8;
					echo_dynamic_loop_length = ECHO_BUFFER_LENGTH_DEFAULT / 3;
				}
				else if(SENSOR_DELAY_5)
				{
					echo_dynamic_loop_length = ECHO_BUFFER_LENGTH_DEFAULT / 4;
				}
				else if(SENSOR_DELAY_4)
				{
					//echo_dynamic_loop_length = ECHO_BUFFER_LENGTH_DEFAULT / 3;
					echo_dynamic_loop_length = ECHO_BUFFER_LENGTH_DEFAULT / 8;
				}
				else if(SENSOR_DELAY_3)
				{
					//echo_dynamic_loop_length = ECHO_BUFFER_LENGTH_DEFAULT / 2;
					echo_dynamic_loop_length = ECHO_BUFFER_LENGTH_DEFAULT / 16;
				}
				else if(SENSOR_DELAY_2)
				{
					//echo_dynamic_loop_length = ECHO_BUFFER_LENGTH_DEFAULT / 3 * 2;
					echo_dynamic_loop_length = ECHO_BUFFER_LENGTH_DEFAULT / 32;
				}
				else
				{
					//echo_dynamic_loop_length = ECHO_BUFFER_LENGTH_DEFAULT;
					echo_dynamic_loop_length = 0;//ECHO_BUFFER_LENGTH_DEFAULT / 64;
				}
			}
			else
			{
				//echo_dynamic_loop_length = ECHO_BUFFER_LENGTH_DEFAULT;
				echo_dynamic_loop_length = 0;//ECHO_BUFFER_LENGTH_DEFAULT / 64;
			}
			printf("acc_res[1] = %f, echo_dynamic_loop_length = %d\n", acc_res[1], echo_dynamic_loop_length);
		}
		#endif

		if (TIMING_EVERY_250_MS == 4999) //4Hz
		{
			if(SENSOR_DELAY_ACTIVE && !scale_settings && !delay_settings)
			{
				if(SENSOR_DELAY_9)
				{
				    ECHO_MIXING_GAIN_MUL_TARGET = 8;
				    //ECHO_MIXING_GAIN_DIV = 10;
				}
				else if(SENSOR_DELAY_8)
				{
				    ECHO_MIXING_GAIN_MUL_TARGET = 6;
				    //ECHO_MIXING_GAIN_DIV = 10;
				}
				else if(SENSOR_DELAY_7)
				{
				    ECHO_MIXING_GAIN_MUL_TARGET = 5;
				    //ECHO_MIXING_GAIN_DIV = 10;
				}
				else if(SENSOR_DELAY_6)
				{
				    ECHO_MIXING_GAIN_MUL_TARGET = 4;
				    //ECHO_MIXING_GAIN_DIV = 10;
				}
				else if(SENSOR_DELAY_5)
				{
				    ECHO_MIXING_GAIN_MUL_TARGET = 3;
				    //ECHO_MIXING_GAIN_DIV = 10;
				}
				else if(SENSOR_DELAY_4)
				{
				    ECHO_MIXING_GAIN_MUL_TARGET = 2.5;
				    //ECHO_MIXING_GAIN_DIV = 10;
				}
				else if(SENSOR_DELAY_3)
				{
				    ECHO_MIXING_GAIN_MUL_TARGET = 2;
				    //ECHO_MIXING_GAIN_DIV = 10;
				}
				else if(SENSOR_DELAY_2)
				{
				    ECHO_MIXING_GAIN_MUL_TARGET = 1.5;
				    //ECHO_MIXING_GAIN_DIV = 10;
				}
				else
				{
				    ECHO_MIXING_GAIN_MUL_TARGET = 1;
				    //ECHO_MIXING_GAIN_DIV = 10;
				}
			}
			else if(!delay_settings)
			{
			    ECHO_MIXING_GAIN_MUL_TARGET = 0;
			}
			//printf("acc_res[1] = %f, ECHO_MIXING_GAIN_MUL = %f\n", acc_res[1], ECHO_MIXING_GAIN_MUL);
		}

		if (TIMING_EVERY_1_MS == 19)
		{
		 	if(ECHO_MIXING_GAIN_MUL < ECHO_MIXING_GAIN_MUL_TARGET)
		 	{
		 		ECHO_MIXING_GAIN_MUL += ECHO_MIXING_GAIN_MUL_STEP;
		 		if(ECHO_MIXING_GAIN_MUL > ECHO_MIXING_GAIN_MUL_TARGET)
		 		{
		 			ECHO_MIXING_GAIN_MUL = ECHO_MIXING_GAIN_MUL_TARGET;
		 		}
		 		//printf("+M=%f, T=%f\n", ECHO_MIXING_GAIN_MUL, ECHO_MIXING_GAIN_MUL_TARGET);
		 	}
		 	if(ECHO_MIXING_GAIN_MUL > ECHO_MIXING_GAIN_MUL_TARGET)
		 	{
		 		ECHO_MIXING_GAIN_MUL -= ECHO_MIXING_GAIN_MUL_STEP;
		 		if(ECHO_MIXING_GAIN_MUL < ECHO_MIXING_GAIN_MUL_TARGET)
		 		{
		 			ECHO_MIXING_GAIN_MUL = ECHO_MIXING_GAIN_MUL_TARGET;
		 		}
		 		//printf("-M=%f, T=%f\n", ECHO_MIXING_GAIN_MUL, ECHO_MIXING_GAIN_MUL_TARGET);
		 	}
		}

		if (TIMING_EVERY_100_MS == 2087) //10Hz
		{
			check_led_blink_timeout();
		}

		if (TIMING_EVERY_500_MS == 7919) //2Hz
		{
			check_auto_power_off(global_settings.AUTO_POWER_OFF_TIMEOUT_ALL);
		}


		#endif

#endif

	}

    codec_silence(100);

    if(timbre_map!=NULL)
    {
    	free(timbre_map);
    	timbre_map = NULL;
    }
}
