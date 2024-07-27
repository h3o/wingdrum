/*
 * glo_config.h
 *
 *  Copyright 2024 Phonicbloom Ltd.
 *
 *  Created on: Jan 31, 2019
 *      Author: mario
 *
 *  This file can be used within the terms of GNU GPLv3 license: https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 *  Find more information at:
 *  http://phonicbloom.com/
 *  http://gechologic.com/
 *
 */

#ifndef GLO_CONFIG_H_
#define GLO_CONFIG_H_

#include "hw/init.h"

#ifdef BOARD_WHALE_V1
#define CONFIG_ADDRESS 0x90000
#define CONFIG_SIZE 0x10000
#endif

#if defined(BOARD_GECHO) || defined(BOARD_WHALE_V2) || defined(BOARD_WINGDRUM)
#define CONFIG_ADDRESS 0x20000
#define CONFIG_SIZE 0x10000
#endif

#define CONFIG_LINE_MAX_LENGTH 500

//settings

typedef struct
{
	float TUNING_DEFAULT;
	float TUNING_MIN;
	float TUNING_MAX;
	double MICRO_TUNING_STEP;
	/*
	float GRANULAR_DETUNE_COEFF_SET;
	float GRANULAR_DETUNE_COEFF_MUL;
	float GRANULAR_DETUNE_COEFF_MAX;

	int TEMPO_BPM_DEFAULT;
	int TEMPO_BPM_MIN;
	int TEMPO_BPM_MAX;
	int TEMPO_BPM_STEP;
	*/

	int AUTO_POWER_OFF_TIMEOUT_ALL;
	int AUTO_POWER_OFF_TIMEOUT_REVERB;
	//uint8_t AUTO_POWER_OFF_ONLY_IF_NO_MOTION;

	//uint8_t DEFAULT_ACCESSIBLE_CHANNELS;

	int8_t AGC_ENABLED, AGC_MAX_GAIN, AGC_TARGET_LEVEL, AGC_MAX_GAIN_STEP, AGC_MAX_GAIN_LIMIT, MIC_BIAS;
	uint8_t CODEC_ANALOG_VOLUME_DEFAULT;
	uint8_t CODEC_DIGITAL_VOLUME_DEFAULT;

	int CLOUDS_HARD_LIMITER_POSITIVE;
	int CLOUDS_HARD_LIMITER_NEGATIVE;
	int CLOUDS_HARD_LIMITER_MAX;
	int CLOUDS_HARD_LIMITER_STEP;
	/*
	float DRUM_THRESHOLD_ON;
	float DRUM_THRESHOLD_OFF;
	int DRUM_LENGTH1;
	int DRUM_LENGTH2;
	int DRUM_LENGTH3;
	int DRUM_LENGTH4;
	uint64_t IDLE_SET_RST_SHORTCUT;
	uint64_t IDLE_RST_SET_SHORTCUT;
	uint64_t IDLE_LONG_SET_SHORTCUT;
	*/
	int8_t ACCELEROMETER_CALIBRATE_ON_POWERUP;
	int8_t SAMPLE_FORMAT_HEADER_LENGTH;

	int16_t SCALE_PLAY_SPEED_MICROTUNING;
	int16_t SCALE_PLAY_SPEED_TRANSPOSE;
	int16_t SCALE_PLAY_SPEED_INITIAL;

	int TOUCHPAD_LED_THRESHOLD_A_MUL;
	int TOUCHPAD_LED_THRESHOLD_A_DIV;
	int TOUCH_PAD_DEFAULT_MAX;

	float SPLIT_TIMBRE_NOTE_THRESHOLD1_COEFF;
	float SPLIT_TIMBRE_NOTE_THRESHOLD2_COEFF;

} settings_t;

typedef struct
{
	int ANALOG_VOLUME;
	int8_t ANALOG_VOLUME_updated;

	int DIGITAL_VOLUME;
	int8_t DIGITAL_VOLUME_updated;

	int8_t EQ_BASS;
	int8_t EQ_BASS_updated;
	int8_t EQ_TREBLE;
	int8_t EQ_TREBLE_updated;

	//int TEMPO;
	//int8_t TEMPO_updated;

	//double FINE_TUNING;
	//int8_t FINE_TUNING_updated;

	//int8_t TRANSPOSE;
	//int8_t TRANSPOSE_updated;

	//int8_t BEEPS;
	//int8_t BEEPS_updated;

	//int8_t ALL_CHANNELS_UNLOCKED;
	//int8_t ALL_CHANNELS_UNLOCKED_updated;

	//int8_t MIDI_SYNC_MODE;
	//int8_t MIDI_SYNC_MODE_updated;

	//int8_t MIDI_POLYPHONY;
	//int8_t MIDI_POLYPHONY_updated;

	//int8_t PARAMS_SENSORS;
	//int8_t PARAMS_SENSORS_updated;

	int8_t AGC_ENABLED_OR_PGA;
	int8_t AGC_ENABLED_OR_PGA_updated;

	int8_t AGC_MAX_GAIN;
	int8_t AGC_MAX_GAIN_updated;

	int8_t MIC_BIAS;
	int8_t MIC_BIAS_updated;

	//int8_t ALL_LEDS_OFF;
	//int8_t ALL_LEDS_OFF_updated;

	//int8_t AUTO_POWER_OFF;
	//int8_t AUTO_POWER_OFF_updated;

	//int8_t SD_CARD_SPEED;
	//int8_t SD_CARD_SPEED_updated;

	//uint16_t SAMPLING_RATE;
	//int8_t SAMPLING_RATE_updated;

	//int8_t ACC_ORIENTATION;
	//int8_t ACC_ORIENTATION_updated;

	//int8_t ACC_INVERT;
	//int8_t ACC_INVERT_updated;

	float ACC_CALIBRATION_X;
	float ACC_CALIBRATION_Y;
	int8_t ACC_CALIBRATION_updated;


	int ECHO_LOOP_LENGTH_STEP;
	int8_t ECHO_LOOP_LENGTH_STEP_updated;

	int update;

} persistent_settings_t;

//samples

//#define SAMPLES_BASE	0x330000
//#define SAMPLES_BASE	0x200000 //samples space at 2MB-16MB range
#define SAMPLES_BASE	0x130000 //length 0xed0000 = 15532032 bytes

#define SAMPLES_MAX 64

#define PATCH_REVERB_NUMBER	1000

#ifdef __cplusplus
 extern "C" {
#endif

void get_samples_map(int *result);

#define SCALE_NAME_MAX_LENGTH 250

char *get_line_in_block(const char *block_name, char *prefix);

int get_scale(char *scale_name, char **buffer);
int get_tuning_coeffs(float **tuning_coeffs, int *coeffs_found);
int get_patches(char *group_name, int **samples, /*float **tuning,*/ int ***notes);
int get_reverb_scale(char *patch_name, int patch_n, int ***notes_buffer);

int load_settings(settings_t *settings, const char* block_name);
void load_persistent_settings(persistent_settings_t *settings);
void store_persistent_settings(persistent_settings_t *settings);

int load_all_settings();

void persistent_settings_store_eq();
//void persistent_settings_store_tempo();
void persistent_settings_store_tuning();

void persistent_settings_store_calibration();
void persistent_settings_store_delay(int current_step);

void settings_reset();

void store_patch_scales(int patch, int *src);
void load_patch_scales(int patch, int *dest);
void store_micro_tuning(int patch, float *src);
void load_micro_tuning(int patch, float *dest);

void store_patch_scales_if_changed(int patch, int *src);
void store_micro_tuning_if_changed(int patch, float *src);

void delete_all_custom_scales();

int backup_restore_nvs_data(const char *src, const char *dst);

int self_tested();
void store_selftest_pass(int value);
void reset_selftest_pass();

void nvs_set_text(const char *key, char *text);
int nvs_get_text(const char *key, char **buffer);


#ifdef __cplusplus
}
#endif

#endif /* GLO_CONFIG_H_ */
