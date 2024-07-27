/*
 * SampleDrum.h
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

#ifndef EXTENSIONS_SAMPLEDRUM_H_
#define EXTENSIONS_SAMPLEDRUM_H_

#include <stdint.h> //for uint16_t type

#define S_STEP_DEFAULT_A1	((double)12000/(double)SAMPLE_RATE_DEFAULT)		//this sample is base note A1 at 48kHz
#define S_STEP_DEFAULT_A2	((double)24000/(double)SAMPLE_RATE_DEFAULT)		//this sample is base note A2 at 48kHz
#define S_STEP_DEFAULT_A3	((double)48000/(double)SAMPLE_RATE_DEFAULT)		//this sample is base note A3 at 48kHz
#define S_STEP_DEFAULT_A4	((double)96000/(double)SAMPLE_RATE_DEFAULT)		//this sample is base note A4 at 48kHz
#define S_STEP_DEFAULT_A5	((double)192000/(double)SAMPLE_RATE_DEFAULT)	//this sample is base note A5 at 48kHz

#define S_STEP_DEFAULT S_STEP_DEFAULT_A3
//#define SAMPLE_PTR_MULTIPLIER	1000

//	#define N_SAMPLES 	2
//	const uint32_t SAMPLE_POSITION[N_SAMPLES]	= {0, 150430/2}; //16-bit mono samples
//	const uint32_t SAMPLE_END[N_SAMPLES]		= {150430/2, 150430/2 + 53056/2};

//	#define N_SAMPLES 	8
//	const uint32_t SAMPLE_POSITION[N_SAMPLES]	= {0, 75215, 101743, 130542, 402748, 577956, 941496, 1127361};
//	const uint32_t SAMPLE_END[N_SAMPLES]		= {75215, 101743, 130542, 402748, 577956, 941496, 1127361, 1284209};
/*
#define N_SAMPLES 	11	//_samples_04.bin
const uint32_t SAMPLE_POSITION[N_SAMPLES]	= {0, 75215, 101743, 130542, 402748, 577956, 941496, 1127361, 1284209, 1329929, 1423529};
const uint32_t SAMPLE_POSITION_B[N_SAMPLES]	= {0, 150430, 203486, 261084, 805496, 1155912, 1882992, 2254722, 2568418, 2659858, 2847058};
const uint32_t SAMPLE_END[N_SAMPLES]		= {75215, 101743, 130542, 402748, 577956, 941496, 1127361, 1284209, 1329929, 1423529, 1491679};
const uint32_t SAMPLE_LENGHT[N_SAMPLES]		= {75215, 26528, 28799, 272206, 175208, 363540, 185865, 156848, 45720, 93600, 68150};
*/

/*
#define N_SAMPLES 	9 //_samples_05.bin
const uint32_t SAMPLE_POSITION[N_SAMPLES]	= {0, 75215, 101743, 276951, 462816, 619664, 860019, 947470, 1049243};
//const uint32_t SAMPLE_POSITION_B[N_SAMPLES]	= {0, 150430, 203486, 553902, 925632, 1239328, 1720038, 1894940, 2098486};
//const uint32_t SAMPLE_END[N_SAMPLES]		= {75215, 101743, 276951, 462816, 619664, 860019, 947470, 1049243, 1262361};
const uint32_t SAMPLE_LENGHT[N_SAMPLES]		= {75215, 26528, 175208, 185865, 156848, 240355, 87451, 101773, 213118};
*/

extern int16_t *MAPPED_SAMPLE_POSITION;
extern uint32_t MAPPED_SAMPLE_LENGHT;		//in 16-bit samples

//#define DOWNSAMPLING_ENABLED

#ifdef DOWNSAMPLING_ENABLED
extern int16_t *DOWNSAMPLED_BUFFER;
extern uint32_t DOWNSAMPLED_LENGTH;		//in 16-bit samples
extern int DOWNSAMPLE_FACTOR;
#endif


void sample_drum(int sample, float tuning, int *notes, int patch_number);

#endif /* EXTENSIONS_SAMPLEDRUM_H_ */
