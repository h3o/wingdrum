/*
 * main.cpp
 *
 *  Copyright 2024 Phonicbloom Ltd.
 *
 *  Created on: Jan 23, 2018
 *      Author: mario
 *
 *  This file can be used within the terms of GNU GPLv3 license: https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 *  Find more information at:
 *  http://phonicbloom.com/
 *  http://gechologic.com/
 *
 */

//#define I2C_SCANNER
#define I2C_HW_CHECK

//#define ENABLE_MCLK				//disable when debugging
#ifdef ENABLE_MCLK
#define ENABLE_MCLK_ON_START
#define DISABLE_MCLK_1_SEC
#endif

#define INIT_CODEC_AT_START

#include "board.h"
#include "main.h"

extern "C" void app_main(void)
{
	//printf("Running on core ID=%d\n", xPortGetCoreID());
    //printf("spi_flash_cache_enabled() returns %u\n", spi_flash_cache_enabled());

#ifdef BOARD_WINGDRUM
    //esp_brownout_init();
	codec_reset();

    //hold the power on
	drum_init_power();
#endif

    i2c_master_init(0);
    gpio_set_level(CODEC_RST_PIN, 1);  //release the codec reset
    uint16_t hw_check_result = hardware_check();
	codec_reset();

#ifdef I2C_SCANNER
    //while(1)
    //{
    	codec_reset();
    	Delay(100);
    	gpio_set_level(CODEC_RST_PIN, 1);  //release the codec reset

    	//look for i2c devices
    	printf("looking for i2c devices...\n");
    	//i2c_init(26, 27); //pin numbers: SDA,SCL
    	i2c_scan_for_devices(1,NULL,NULL);
    	printf("finished...\n");

    	codec_reset();

    //	Delay(1000);
    //}
#endif

	//printf("init_echo_buffer();\n");
	init_echo_buffer();

	//continuous accelerometer read mode
	init_accelerometer();

	//drum_test_buttons();
	//drum_LED_expander_test();

	printf("main(): starting task [process_buttons_controls_drum]\n");
	xTaskCreatePinnedToCore((TaskFunction_t)&process_buttons_controls_drum, "process_buttons_controls_task", 4096, NULL, TASK_PRIORITY_BUTTONS, NULL, 1);

	printf("main(): starting task [process_ui_events]\n");
	xTaskCreatePinnedToCore((TaskFunction_t)&process_ui_events, "process_ui_events_task", 4096, NULL, TASK_PRIORITY_UI_EVENTS, NULL, 1);

	#ifdef I2C_HW_CHECK
	if(hw_check_result) //i2c_errors + found << 8;
	{
		printf("main(): I2C devices found, hw_check_result = 0x%x\n", hw_check_result);
		if(hw_check_result)
		{
			printf("main(): I2C errors found!\n");
			error_blink(ERROR_BLINK_PATTERN_1357_2468, 500); //alternate pattern
		}
	}
	#endif

	printf("enabling MIDI, drum_init_MIDI(%d,%d)\n", MIDI_UART, 1);
    drum_init_MIDI(MIDI_UART, 1); //midi out enabled

    load_all_settings();

    //touch_pad_test();
    printf("main(): starting task [touch_pad_process]\n");
	xTaskCreatePinnedToCore((TaskFunction_t)&touch_pad_process, "touch_pad_process_task", 4096, NULL, TASK_PRIORITY_TOUCH_PADS, NULL, 1);

	#ifdef ENABLE_MCLK_ON_START
    init_i2s_and_gpio(CODEC_BUF_COUNT_DEFAULT, CODEC_BUF_LEN_DEFAULT, SAMPLE_RATE_DEFAULT);
	start_MCLK();
	#endif

	echo_dynamic_loop_current_step = persistent_settings.ECHO_LOOP_LENGTH_STEP;
	if(echo_dynamic_loop_current_step<0)
	{
		echo_dynamic_loop_length = -echo_dynamic_loop_current_step;
		echo_dynamic_loop_current_step = -1;
	}
	else
	{
		echo_dynamic_loop_length = echo_dynamic_loop_steps[echo_dynamic_loop_current_step];
	}
	printf("load_all_settings() => echo_dynamic_loop_length = %d, echo_dynamic_loop_current_step = %d\n", echo_dynamic_loop_length, echo_dynamic_loop_current_step);

    int tuning_coeffs_found;
	#ifdef DEBUG_CONFIG_PARSING
    int tuning_coeffs_array_size =
	#endif
    get_tuning_coeffs(&tuning_coeffs, &tuning_coeffs_found);

    #ifdef DEBUG_CONFIG_PARSING
    printf("main(): get_tuning_coeffs() returned array of %d tuning coefficients (%d values present), structure pointer = %p\n", tuning_coeffs_array_size, tuning_coeffs_found, tuning_coeffs);

    for(int i=0;i<tuning_coeffs_array_size;i++)
    {
        printf("tuning coefficient #%d = %f\n", i, tuning_coeffs[i]);
    }
	#endif

    int *samples_metal, *samples_wood;
    //float *tuning_metal, *tuning_wood;
    int **notes_metal, **notes_wood;

    patches_found_metal = get_patches("metal", &samples_metal, /*&tuning_metal,*/ &notes_metal);
	#ifdef DEBUG_CONFIG_PARSING
    printf("main(): get_patches(\"metal\") returned %d patches, notes structure pointer = %p\n", patches_found_metal, notes_metal);

    for(int i=0;i<patches_found_metal;i++)
    {
        //printf("main(): patch #%d: sample = %d, tuning = %f, notes = [ ", i, samples_metal[i], tuning_metal[i]);
        printf("main(): patch #%d: sample = %d, notes = [ ", i, samples_metal[i]);
        for(int n=0;n<9;n++)
        {
        	printf("%d ", notes_metal[i][n]);
        }
        printf("]\n");
    }
	#endif

    patches_found_wood = get_patches("wood", &samples_wood, /*&tuning_wood,*/ &notes_wood);
	#ifdef DEBUG_CONFIG_PARSING
    printf("main(): get_patches(\"wood\") returned %d patches, notes structure pointer = %p\n", patches_found_wood, notes_wood);

    for(int i=0;i<patches_found_wood;i++)
    {
        //printf("main(): patch #%d: sample = %d, tuning = %f, notes = [ ", i, samples_wood[i], tuning_wood[i]);
        printf("main(): patch #%d: sample = %d, notes = [ ", i, samples_wood[i]);
        for(int n=0;n<9;n++)
        {
        	printf("%d ", notes_wood[i][n]);
        }
        printf("]\n");
    }
	#endif

    //allocate one more int for scale in reverb
    selected_scale = (int*)malloc((patches_found_metal+patches_found_wood+1)*sizeof(int));
    memset(selected_scale, 0, (patches_found_metal+patches_found_wood+1)*sizeof(int));
    patch_reverb_n = patches_found_metal+patches_found_wood; //the last one is reverb

    int patches_found_reverb = get_reverb_scale("reverb", patch_reverb_n, &notes_reverb);
    //printf("patches_found_reverb = %d\n", patches_found_reverb);

    micro_tuning = (float*)malloc(NOTES_PER_SCALE * SCALES_PER_PATCH * sizeof(float));
    //memset(micro_tuning, 0, NOTES_PER_SCALE * SCALES_PER_PATCH * sizeof(float));

    for(int i=0;i<NOTES_PER_SCALE*SCALES_PER_PATCH;i++)
    {
    	micro_tuning[i] = 1.0f;
    }

    midi_scale_clipboard = (int*)malloc(NOTES_PER_SCALE * sizeof(int));
    memset(midi_scale_clipboard, 0, NOTES_PER_SCALE * sizeof(int));
    micro_tuning_clipboard = (float*)malloc(NOTES_PER_SCALE * sizeof(float));

    power_on_animation();
    drum_LED_expanders_set_blink_rates();

	accelerometer_active = 1;
	//accelerometer_test();

	#ifdef ACC_CALIBRATION_ENABLED_ON_POWERUP
    if(global_settings.ACCELEROMETER_CALIBRATE_ON_POWERUP)
    {
    	acc_calibrating = ACC_CALIBRATION_LOOPS;
    	accelerometer_calibrate();
    }
    else
	#endif
    {
		acc_calibrated[0] = persistent_settings.ACC_CALIBRATION_X;
		acc_calibrated[1] = persistent_settings.ACC_CALIBRATION_Y;
    }

	#ifdef ANALYSE_THRESHOLDS
	print_thresholds_info(0);
	#endif

    while(BUTTON_U1_ON); //wait till PWR button released

#ifdef INIT_CODEC_AT_START
    init_i2s_and_gpio(CODEC_BUF_COUNT_DEFAULT, CODEC_BUF_LEN_DEFAULT, SAMPLE_RATE_DEFAULT);//persistent_settings.SAMPLING_RATE);
	start_MCLK();
	codec_init();
	if(persistent_settings.AGC_ENABLED_OR_PGA==0)
	{
		//disable AGC
		codec_configure_AGC(0, global_settings.AGC_MAX_GAIN, global_settings.AGC_TARGET_LEVEL);
	}
#endif

    init_free_mem = xPortGetFreeHeapSize();
    int memory_loss;

    int patch_metal_ptr = -1, patch_wood_ptr = -1;

    event_next_channel = EVENT_NEXT_CHANNEL_METAL; //start with the first metal patch

    while(!touch_pad_calibrated); //wait until touch pad calibrated

    while(glo_run)
    {
		printf("----START-----[select patch loop] Free heap before init: %u\n", xPortGetFreeHeapSize());
		memory_loss = init_free_mem - xPortGetFreeHeapSize();
		printf("----START-----[select patch loop] Memory loss = %d\n", memory_loss);

		#ifdef ACC_CALIBRATION_ENABLED_ON_PATCH_SWITCH
		acc_calibrating = ACC_CALIBRATION_LOOPS;
		accelerometer_calibrate();
		#endif

		if(event_next_channel==EVENT_NEXT_CHANNEL_METAL)
		{
			patch_metal_ptr++;
			if(patch_metal_ptr >= patches_found_metal)
			{
				patch_metal_ptr = 0;
			}
			printf("main(): starting \"metal\" patch #%d: sample = %d, tuning = %f, notes = [ ", patch_metal_ptr, samples_metal[patch_metal_ptr], tuning_coeffs[samples_metal[patch_metal_ptr]]);
			for(int n=0;n<9;n++) { printf("%d ", notes_metal[patch_metal_ptr][n]); } printf("]\n");
			//patch scales are arranged in one array, starting with wood, then metal. here the patch number = patches_found_wood + patch_metal_ptr
			sample_drum(samples_metal[patch_metal_ptr], tuning_coeffs[samples_metal[patch_metal_ptr]], notes_metal[patch_metal_ptr], patches_found_wood + patch_metal_ptr);
		}
		else if(event_next_channel==EVENT_NEXT_CHANNEL_WOOD)
		{
			patch_wood_ptr++;
			if(patch_wood_ptr >= patches_found_wood)
			{
				patch_wood_ptr = 0;
			}
			printf("main(): starting \"wood\" patch #%d: sample = %d, tuning = %f, notes = [ ", patch_wood_ptr, samples_wood[patch_wood_ptr], tuning_coeffs[samples_wood[patch_wood_ptr]]);
			for(int n=0;n<9;n++) { printf("%d ", notes_wood[patch_wood_ptr][n]); } printf("]\n");
			//patch number = patch_wood_ptr
			sample_drum(samples_wood[patch_wood_ptr], tuning_coeffs[samples_wood[patch_wood_ptr]], notes_wood[patch_wood_ptr], patch_wood_ptr);
		}

		else if(event_next_channel==EVENT_NEXT_CHANNEL_BOTH)
		{
			printf("main(): starting \"reverb\" patch\n");
			channel_reverb();
		}

		else if(event_next_channel==EVENT_NEXT_CHANNEL_PWR_OFF)
		{
			int animation_ended = power_off_animation(0, LED_PWR_OFF_DELAY);
			if(!animation_ended) //if animation finished and user did not release the PWR button
			{
				drum_shutdown();
				while(1); //halt, just in case
			}
			else
			{
				power_on_animation_reversed(animation_ended);
			}
			event_next_channel = event_next_channel0;

			if(event_next_channel==EVENT_NEXT_CHANNEL_METAL)
			{
				patch_metal_ptr--;
			}
			if(event_next_channel==EVENT_NEXT_CHANNEL_WOOD)
			{
				patch_wood_ptr--;
			}
		}

		channel_deinit();
		printf("----END-------[select patch loop] Free heap after deinit: %u\n", xPortGetFreeHeapSize());
		memory_loss = init_free_mem - xPortGetFreeHeapSize();
		printf("----END-------[select patch loop] Memory loss = %d\n", memory_loss);

		printf("main(): event_next_channel = %d\n", event_next_channel);
    }

	patch_wood_ptr+=2;
	sample_drum(samples_wood[patch_wood_ptr], tuning_coeffs[samples_wood[patch_wood_ptr]], notes_wood[patch_wood_ptr], patch_wood_ptr);

	decaying_reverb(1);
	channel_deinit();

	drum_LED_expander_test();

	printf("main(): program finished\n");
}
