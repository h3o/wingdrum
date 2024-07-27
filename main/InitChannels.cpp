/*
 * InitChannels.cpp
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

#include <InitChannels.h>
#include <SampleDrum.h>
#include <Accelerometer.h>
#include <Interface.h>
#include <hw/init.h>
#include <hw/gpio.h>
#include <hw/signals.h>
#include <hw/midi.h>
#include <dsp/Reverb.h>
#include "glo_config.h"
#include <string.h>

#define SEND_MIDI_NOTES

int MIXED_SAMPLE_BUFFER_LENGTH;
unsigned int mixed_sample_buffer_adr, mixed_sample_buffer_adr_aligned_64k, mixed_sample_buffer_align_offset;

int bit_crusher_reverb;

uint16_t t_TIMING_BY_SAMPLE_EVERY_250_MS; //optimization of timing counters
uint16_t t_TIMING_BY_SAMPLE_EVERY_125_MS; //optimization of timing counters
uint16_t t_TIMING_BY_SAMPLE_EVERY_25_MS; //optimization of timing counters

spi_flash_mmap_handle_t mmap_handle_samples = 0;//NULL;

void channel_init(int bg_sample, int song_id, int melody_id, int filters_type, float resonance, int use_mclk, int set_wind_voices, int set_active_filter_pairs, int reverb_ext)
{
	//GPIO_LEDs_Buttons_Reset();
	program_settings_reset();

	event_next_channel0 = event_next_channel; //if we need to return back to this patch
	event_next_channel = 0;

	scale_settings = 0;

    //printf("[program_settings_reset]Free heap: %u\n", xPortGetFreeHeapSize());
    //if(!heap_caps_check_integrity_all(true)) { printf("---> HEAP CORRUPT!\n"); }

	#ifdef SWITCH_I2C_SPEED_MODES
	i2c_master_deinit();
    i2c_master_init(1); //fast mode
	#endif

	//----------------------------------------------------------------

	//printf("channel_init(bg_sample = %d)\n",bg_sample);

	if(bg_sample)
	{
	    int samples_map[2*SAMPLES_MAX+2]; //0th element holds number of found samples
		//printf("channel_init(): Free heap before get_samples_map(): %u\n", xPortGetFreeHeapSize());
	    get_samples_map(samples_map);
		//printf("channel_init(): Free heap after get_samples_map(): %u\n", xPortGetFreeHeapSize());

		#ifdef DEBUG_CONFIG_PARSING
	    printf("get_samples_map() returned %d samples, last one = %d\n", samples_map[0], samples_map[1]);
	    if(samples_map[0]>0)
	    {
	    	for(int i=1;i<samples_map[0]+1;i++)
	    	{
	    		printf("%d-%d,",samples_map[2*i],samples_map[2*i+1]);
	    	}
	    	printf("\n");
	    }
		#endif

		if(bg_sample > samples_map[0]) //not enough samples
		{
			printf("channel_init(): requested sample #%d not found in the samples map, there is only %d samples\n", bg_sample, samples_map[0]);
			error_blink(ERROR_BLINK_PATTERN_1256_3478, 200); //blink forever
		}

		mixed_sample_buffer_adr = SAMPLES_BASE + samples_map[2*bg_sample];//SAMPLES_BASE;
		MIXED_SAMPLE_BUFFER_LENGTH = samples_map[2*bg_sample+1] / 2; //samples_map[bg_sample] / 2; //size in 16-bit samples, not bytes!

		if(global_settings.SAMPLE_FORMAT_HEADER_LENGTH)
		{
			mixed_sample_buffer_adr += global_settings.SAMPLE_FORMAT_HEADER_LENGTH;
			MIXED_SAMPLE_BUFFER_LENGTH -= global_settings.SAMPLE_FORMAT_HEADER_LENGTH;
		}

		#ifdef DEBUG_CONFIG_PARSING
		printf("calculated position for sample %d = 0x%x (%d), length = %d (in 16-bit samples)\n", bg_sample, mixed_sample_buffer_adr, mixed_sample_buffer_adr, MIXED_SAMPLE_BUFFER_LENGTH);
		printf("mapping bg sample = %d\n", bg_sample);
		#endif

		//mapping address must be aligned to 64kB blocks (0x10000)
		mixed_sample_buffer_adr_aligned_64k = mixed_sample_buffer_adr & 0xFFFF0000;
		mixed_sample_buffer_align_offset = mixed_sample_buffer_adr - mixed_sample_buffer_adr_aligned_64k;

		#ifdef DEBUG_CONFIG_PARSING
		printf("Mapping address aligned to %x (%d), offset = %x (%d)\n", mixed_sample_buffer_adr_aligned_64k, mixed_sample_buffer_adr_aligned_64k, mixed_sample_buffer_align_offset, mixed_sample_buffer_align_offset);
		printf("Mapping flash memory at %x (+%x length)\n", mixed_sample_buffer_adr_aligned_64k, mixed_sample_buffer_align_offset + MIXED_SAMPLE_BUFFER_LENGTH*2);
		#endif

		//printf("channel_init(): Free heap before spi_flash_mmap(): %u\n", xPortGetFreeHeapSize());
		const void *ptr1;
		/*int esp_result =*/ spi_flash_mmap(mixed_sample_buffer_adr_aligned_64k, (size_t)(mixed_sample_buffer_align_offset+MIXED_SAMPLE_BUFFER_LENGTH*2), SPI_FLASH_MMAP_DATA, &ptr1, &mmap_handle_samples);
		//printf("channel_init(): Free heap after spi_flash_mmap(): %u\n", xPortGetFreeHeapSize());

		#ifdef DEBUG_CONFIG_PARSING
		//printf("spi_flash_mmap() result = %d\n",esp_result);
		printf("spi_flash_mmap() mapped destination = %x\n",(unsigned int)ptr1);
		#endif

		if(ptr1==NULL)
		{
			printf("spi_flash_mmap() returned NULL pointer\n");
			while(1);
		}

		#ifdef DEBUG_CONFIG_PARSING
		printf("mmap_res: handle=%d ptr=%p\n", mmap_handle_samples, ptr1);
		#endif

		//MAPPED_SAMPLE_POSITION = (uint16_t*)mixed_sample_buffer_adr;	//position in 16-bit samples
		MAPPED_SAMPLE_POSITION = (int16_t*)ptr1 + mixed_sample_buffer_align_offset/2;
		MAPPED_SAMPLE_LENGHT = MIXED_SAMPLE_BUFFER_LENGTH;				//length is already in 16-bit samples

		#ifdef DEBUG_CONFIG_PARSING
		printf("channel_init(): MAPPED_SAMPLE_POSITION=%p MAPPED_SAMPLE_LENGHT=%d\n", MAPPED_SAMPLE_POSITION, MAPPED_SAMPLE_LENGHT);
		#endif

		#ifdef DOWNSAMPLING_ENABLED
		printf("channel_init(): Free heap before downsampling: %u\n", xPortGetFreeHeapSize());
		int free_mem = xPortGetFreeHeapSize();

		DOWNSAMPLE_FACTOR = (MAPPED_SAMPLE_LENGHT*2) / free_mem + 1;
		printf("channel_init(): optimal downsample by factor of %d\n", DOWNSAMPLE_FACTOR);

		if(DOWNSAMPLE_FACTOR>=2)
		{
			DOWNSAMPLED_LENGTH = MAPPED_SAMPLE_LENGHT / DOWNSAMPLE_FACTOR;
			int mem_required = DOWNSAMPLED_LENGTH * sizeof(int16_t);
			if(free_mem > mem_required)
			{
				printf("channel_init(): allocating %d bytes for downsampling\n", mem_required);
				DOWNSAMPLED_BUFFER = (int16_t*)malloc(mem_required);
			}
			else
			{
				printf("channel_init(): not enough memory for downsampling: %d required\n", mem_required);
				DOWNSAMPLED_LENGTH = 0;
				DOWNSAMPLED_BUFFER = NULL;
			}
		}
		else
		{
			printf("channel_init(): sample too short, no need for downsampling: %d bytes\n", MAPPED_SAMPLE_LENGHT);
		}

		printf("channel_init(): Free heap after downsampling: %u\n", xPortGetFreeHeapSize());
		#endif
	}

	//----------------------------------------------------------------

	memset(echo_buffer,0,ECHO_BUFFER_LENGTH*sizeof(int16_t));
	echo_buffer_ptr0 = 0;

	bit_crusher_reverb = BIT_CRUSHER_REVERB_DEFAULT;

	#if !defined(REVERB_POLYPHONIC) //&& !defined(REVERB_OCTAVE_UP_DOWN)

    REVERB_MIXING_GAIN_MUL = 9; //amount of signal to feed back to reverb loop, expressed as a fragment
    REVERB_MIXING_GAIN_DIV = 10; //e.g. if MUL=2 and DIV=3, it means 2/3 of signal is mixed in

	//printf("channel_init(): [2] total_chords = %d\n", fil->chord->total_chords);

	reverb_dynamic_loop_length = /* I2S_AUDIOFREQ / */ BIT_CRUSHER_REVERB_DEFAULT; //set default value (can be changed dynamically)
	//reverb_buffer = (int16_t*)malloc(reverb_dynamic_loop_length*sizeof(int16_t)); //allocate memory
    reverb_buffer = (int16_t*)malloc(REVERB_BUFFER_LENGTH*sizeof(int16_t));
    printf("allocated reverb_buffer: length = %d, %d bytes\n", REVERB_BUFFER_LENGTH, REVERB_BUFFER_LENGTH*sizeof(int16_t));

    //printf("channel_init(): [3] total_chords = %d\n", fil->chord->total_chords);

    memset(reverb_buffer,0,REVERB_BUFFER_LENGTH*sizeof(int16_t)); //clear memory
	reverb_buffer_ptr0 = 0; //reset pointer

	//printf("channel_init(): [4] total_chords = %d\n", fil->chord->total_chords);

    printf("channel_init(): Free heap[reverb allocated]: %u\n", xPortGetFreeHeapSize());
	#endif

    if(reverb_ext)
    {
    	for(int r=0;r<REVERB_BUFFERS_EXT;r++)
    	{
    		reverb_dynamic_loop_length_ext[r] = BIT_CRUSHER_REVERB_DEFAULT_EXT(r);
    		reverb_buffer_ext[r] = (int16_t*)malloc(BIT_CRUSHER_REVERB_MAX_EXT(r)*sizeof(int16_t)); //allocate memory
    		//printf("allocated reverb_buffer_ext[%d]: length = %d, %d bytes\n", r, BIT_CRUSHER_REVERB_MAX_EXT(r), BIT_CRUSHER_REVERB_MAX_EXT(r)*sizeof(int16_t));

    		memset(reverb_buffer_ext[r],0,BIT_CRUSHER_REVERB_MAX_EXT(r)*sizeof(int16_t)); //clear memory
    		reverb_buffer_ptr0_ext[r] = 0; //reset pointer

    		//printf("channel_init(): Free heap[extended reverb #%d allocated]: %u\n", r, xPortGetFreeHeapSize());
    	}
    }

    REVERB_MIXING_GAIN_MUL_EXT = REVERB_MIXING_GAIN_MUL_EXT_DEFAULT; //amount of signal to feed back to reverb loop, expressed as a fragment
    REVERB_MIXING_GAIN_DIV_EXT = REVERB_MIXING_GAIN_DIV_EXT_DEFAULT; //e.g. if MUL=2 and DIV=3, it means 2/3 of signal is mixed in

    //----------------------------------------------------------------

	SAMPLE_VOLUME = SAMPLE_VOLUME_DEFAULT;
	limiter_coeff = DYNAMIC_LIMITER_COEFF_DEFAULT;

	//----------------------------------------------------------------

    //ms10_counter = 0;

    seconds = 0;

    //auto_power_off = persistent_settings.AUTO_POWER_OFF*600;//global_settings.AUTO_POWER_OFF_TIMEOUT;
    //printf("channel_init(): setting initial value for [auto_power_off] by global_settings.AUTO_POWER_OFF_TIMEOUT = %d\n", global_settings.AUTO_POWER_OFF_TIMEOUT);

    if(reverb_ext)
    {
    	auto_power_off = global_settings.AUTO_POWER_OFF_TIMEOUT_REVERB;
    }
    else
    {
    	auto_power_off = global_settings.AUTO_POWER_OFF_TIMEOUT_ALL;
    }
    auto_power_off_canceled = 0;

    MIDI_pitch_base_ptr = -1;

	//accelerometer_active = 1;

	//----------------------------------------------------------------

	//LEDs_all_OFF();
	LEDS_ALL_OFF;
}

void channel_deinit()
{
	//printf("heap_trace_dump(void)\n");
	//heap_trace_dump();

	//printf("[codec_reset]Free heap: %u\n", xPortGetFreeHeapSize());
	//if(!heap_caps_check_integrity_all(true)) { printf("---> HEAP CORRUPT!\n"); }

    codec_set_mute(1); //mute the codec

    touch_pad_enabled = 0;
	scale_settings = 0;

    Delay(10); //wait till IR sensors / LEDs indication stopped

	//release some allocated memory

	//if(mixed_sample_buffer!=NULL)
    if(mmap_handle_samples)
	{
		//printf("channel_deinit(): Free heap before spi_flash_munmap(): %u\n", xPortGetFreeHeapSize());
		if(mmap_handle_samples)//!=NULL)
		{
			printf("spi_flash_munmap(mmap_handle_samples) ... ");
			spi_flash_munmap(mmap_handle_samples);
			mmap_handle_samples = 0;//NULL;
			printf("unmapped!\n");
		}
		//printf("channel_deinit(): Free heap after spi_flash_munmap(): %u\n", xPortGetFreeHeapSize());

		//mixed_sample_buffer = NULL;

		//printf("[spi_flash_munmap]Free heap: %u\n", xPortGetFreeHeapSize());
		//if(!heap_caps_check_integrity_all(true)) { printf("---> HEAP CORRUPT!\n"); }
	}

	#ifdef DOWNSAMPLING_ENABLED
    if(DOWNSAMPLED_LENGTH)
	{
		free(DOWNSAMPLED_BUFFER);
		DOWNSAMPLED_BUFFER = NULL;
		DOWNSAMPLED_LENGTH = 0;
	}
	#endif

	#if !defined(REVERB_POLYPHONIC) && !defined(REVERB_OCTAVE_UP_DOWN)
	if(reverb_buffer!=NULL)
	{
		printf("free(reverb_buffer)\n");
		free(reverb_buffer);
		reverb_buffer=NULL;
	}
	#endif

	for(int r=0;r<REVERB_BUFFERS_EXT;r++)
	{
		if(reverb_buffer_ext[r])
		{
			free(reverb_buffer_ext[r]);
			reverb_buffer_ext[r] = NULL;
			//printf("free(reverb_buffer_ext[%d])\n", r);
		}
	}

    LEDS_ALL_OFF;

	//printf("done\n");
    //Delay(500);
}
