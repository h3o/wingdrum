/*
 * Accelerometer.h
 *
 *  Copyright 2024 Phonicbloom Ltd.
 *
 *  Created on: 16 Jun 2018
 *      Author: mario
 *
 *  This file can be used within the terms of GNU GPLv3 license: https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 *  Find more information at:
 *  http://phonicbloom.com/
 *  http://gechologic.com/
 *
 */

#ifndef ACCELEROMETER_H_
#define ACCELEROMETER_H_

#include <stdint.h>

//#define USE_KIONIX_ACCELEROMETER
//#define USE_MMA8452_ACCELEROMETER
#define USE_WMA6981_ACCELEROMETER
//#define DEBUG_ACCELEROMETER
//#define ACCELEROMETER_TEST

#ifdef USE_KIONIX_ACCELEROMETER
#include "hw/kx123.h"
#include "hw/kx123_registers.h"
#endif

//#define MMA8452_I2C_ADDRESS 0x1c
#define WMA6981_I2C_ADDRESS 0x12

//#include <stdint.h> //for uint16_t type

typedef struct {
	int measure_delay;
	//int cycle_delay;
} AccelerometerParam_t;

#define ACC_RESULTS 3
#define ACC_X_AXIS acc_res[0]
#define ACC_Y_AXIS acc_res[1]
#define ACC_Z_AXIS acc_res[2]

extern float acc_res[ACC_RESULTS], acc_res1[ACC_RESULTS];//, acc_res2[ACC_RESULTS], acc_res3[ACC_RESULTS];
#ifdef USE_KIONIX_ACCELEROMETER
extern KX123 acc;
#endif

extern int segment_x, segment_x0, segment_y, segment_y0, segment_y1, center_y;
extern int ignore_acc_octave_switch;

#define IGNORE_ACC_OCTAVE_SW_RESET	10	//20 hz, 0.5 sec

#define THRESHOLD_SEGMENT_X_LEFT	-25
#define THRESHOLD_SEGMENT_X_RIGHT	25

#define THRESHOLD_SEGMENT_Y_UP 		20
#define THRESHOLD_SEGMENT_Y_DOWN	-20

//#define OCTAVE_SWITCH_X
//#define OCTAVE_SWITCH_Y

#define ACC_CALIBRATION_ENABLED
#define ACC_CALIBRATION_ENABLED_ON_POWERUP
//#define ACC_CALIBRATION_ENABLED_ON_PATCH_SWITCH

#define ACC_CALIBRATION_LOOPS 25

#define ACCELEROMETER_MEASURE_DELAY 20

extern int acc_calibrating;
extern float acc_calibrated[];

void init_accelerometer();
void accelerometer_power_up();
void process_accelerometer(void *pvParameters);

#ifdef ACCELEROMETER_TEST
void accelerometer_test();
#endif

#ifdef ACC_CALIBRATION_ENABLED
void accelerometer_calibrate();
#endif

#endif /* ACCELEROMETER_H_ */
