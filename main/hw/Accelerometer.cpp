/*
 * Accelerometer.cpp
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

#include <Accelerometer.h>
#include <hw/init.h>
#include <hw/gpio.h>
#include <hw/ui.h>

float acc_res[ACC_RESULTS] = {0,0,-1.0f}, acc_res1[ACC_RESULTS];//, acc_res2[ACC_RESULTS], acc_res3[ACC_RESULTS];
#ifdef USE_KIONIX_ACCELEROMETER
	KX123 acc(I2C_MASTER_NUM);
	//int acc_i, acc_stable_cnt;
#endif

int segment_x = 0, segment_x0 = 0, segment_y = 0, segment_y0 = 0, segment_y1 = 0, center_y;
int ignore_acc_octave_switch;

int acc_calibrating = ACC_CALIBRATION_LOOPS;
float acc_calibrated[2] = {0.0f,0.0f};

void init_accelerometer()
{
	//printf("init_accelerometer()\n");

	static AccelerometerParam_t AccelerometerParam;

	#ifdef USE_KIONIX_ACCELEROMETER
	int error = acc.set_defaults();
	if(error)
	{
		printf("ERROR in acc.set_defaults() code=%d\n",error);
	}
	//acc.getresults_g(acc_res); //read initial value immediately, otherwise KX122 hangs

	AccelerometerParam.measure_delay = 5;	//works with dev prototype
	//AccelerometerParam.cycle_delay = 10;
	#endif

	#ifdef USE_MMA8452_ACCELEROMETER
	AccelerometerParam.measure_delay = 100;
	#endif

	#ifdef USE_WMA6981_ACCELEROMETER
	AccelerometerParam.measure_delay = ACCELEROMETER_MEASURE_DELAY;
	#endif

	#ifdef ACCELEROMETER_TEST
		accelerometer_test();
		while(1);
	#endif

	accelerometer_power_up();

	xTaskCreatePinnedToCore((TaskFunction_t)&process_accelerometer, "process_accelerometer_task", 2048, &AccelerometerParam, TASK_PRIORITY_ACCELEROMETER, NULL, 1);
}

/*
void process_accelerometer(void *pvParameters)
{
	#ifdef USE_KIONIX_ACCELEROMETER
	int measure_delay = ((AccelerometerParam_t*)pvParameters)->measure_delay;
	int cycle_delay = ((AccelerometerParam_t*)pvParameters)->cycle_delay;

	printf("process_accelerometer(): task running on core ID=%d\n",xPortGetCoreID());
	printf("process_accelerometer(): measure_delay = %d\n",measure_delay);
	printf("process_accelerometer(): cycle_delay = %d\n",cycle_delay);

	while(1)
	{
		//Delay(1); //can restart faster than measure_delay (e.g. if skipped due to driver off)
		if(i2c_driver_installed)
		{
			acc.getresults_g(acc_res1);
		}
		else
		{
			printf("process_accelerometer(): I2C driver not installed, skipping iteration[1]\n");
			continue;
		}
		//printf("acc_res=%f,%f,%f\n",acc_res1[0],acc_res1[1],acc_res1[2]);

		Delay(measure_delay);
		if(i2c_driver_installed)
		{
			acc.getresults_g(acc_res2);
		}
		else
		{
			printf("process_accelerometer(): I2C driver not installed, skipping iteration[2]\n");
			continue;
		}
		//printf("acc_res=%f,%f,%f\n",acc_res2[0],acc_res2[1],acc_res2[2]);

		Delay(measure_delay);
		if(i2c_driver_installed)
		{
			acc.getresults_g(acc_res3);
		}
		else
		{
			printf("process_accelerometer(): I2C driver not installed, skipping iteration[3]\n");
			continue;
		}
		//printf("acc_res=%f,%f,%f\n",acc_res3[0],acc_res3[1],acc_res3[2]);

		acc_stable_cnt = 0;
		for(acc_i=0;acc_i<3;acc_i++)
		{
			if(fabs(acc_res1[acc_i]-acc_res2[acc_i])<0.1
			&& fabs(acc_res2[acc_i]-acc_res3[acc_i])<0.1
			&& fabs(acc_res1[acc_i]-acc_res3[acc_i])<0.1)
			{
				//acc_res[acc_i] = acc_res3[acc_i];
				acc_stable_cnt++;
				//printf("acc.param[%d] stable... ", acc_i);
			}
			else
			{
				//printf("acc.param[%d] NOT STABLE... ", acc_i);
			}
		}

		if(acc_stable_cnt==3)
		{
			//acc_res[0] = acc_res2[0];
			//acc_res[1] = acc_res2[1];
			//acc_res[2] = acc_res2[2];
			memcpy(acc_res,acc_res2,sizeof(acc_res2));
		}
		//printf("\n");
		Delay(cycle_delay);
	}
	#endif
}
*/


/*
 * time smoothing constant for low-pass filter
 * 0 <= alpha <= 1 ; a smaller value basically means more smoothing
 * See: http://en.wikipedia.org/wiki/Low-pass_filter#Discrete-time_realization
 */
//#define lpALPHA 0.15f
//#define lpALPHA 0.025f
#define lpALPHA 0.25f

void acc_lowPass(float *input, float *output)
{
	for(int i=0;i<3; i++)
	{
		output[i] = output[i] + lpALPHA * (input[i] - output[i]);
	}
}

#ifdef USE_KIONIX_ACCELEROMETER
void process_accelerometer(void *pvParameters)
{
	int measure_delay = ((AccelerometerParam_t*)pvParameters)->measure_delay;
	//int cycle_delay = ((AccelerometerParam_t*)pvParameters)->cycle_delay;

	//#ifdef DEBUG_ACCELEROMETER
	printf("process_accelerometer(): task running on core ID=%d\n",xPortGetCoreID());
	printf("process_accelerometer(): measure_delay = %d\n",measure_delay);
	//printf("process_accelerometer(): cycle_delay = %d\n",cycle_delay);
	//#endif

	while(1)
	{
		Delay(measure_delay);

		if(!accelerometer_active)
		{
			continue;
		}

		if(i2c_driver_installed)
		{
			acc.getresults_g(acc_res1);
		}
		else
		{
			#ifdef DEBUG_ACCELEROMETER
			printf("process_accelerometer(): I2C offline\n");
			#endif
			continue;
		}

		if(persistent_settings.ACC_INVERT)
		{
			if(persistent_settings.ACC_INVERT & 0x01)
			{
				acc_res1[0] = -acc_res1[0];
			}
			if(persistent_settings.ACC_INVERT & 0x02)
			{
				acc_res1[1] = -acc_res1[1];
			}
			if(persistent_settings.ACC_INVERT & 0x04)
			{
				acc_res1[2] = -acc_res1[2];
			}
		}

		if(persistent_settings.ACC_ORIENTATION)
		{
			float tmp;
			if(persistent_settings.ACC_ORIENTATION==ACC_ORIENTATION_XZY)
			{
				tmp = acc_res1[1];
				acc_res1[1] = acc_res1[2];
				acc_res1[2] = tmp;
			}
			if(persistent_settings.ACC_ORIENTATION==ACC_ORIENTATION_YXZ)
			{
				tmp = acc_res1[1];
				acc_res1[1] = acc_res1[0];
				acc_res1[0] = tmp;
			}
			#ifdef ACC_ORIENTATION_YZX
			if(persistent_settings.ACC_ORIENTATION==ACC_ORIENTATION_YZX)
			{
				tmp = acc_res1[0];
				acc_res1[0] = acc_res1[1];
				acc_res1[1] = acc_res1[2];
				acc_res1[2] = tmp;
			}
			#endif
			#ifdef ACC_ORIENTATION_ZXY
			if(persistent_settings.ACC_ORIENTATION==ACC_ORIENTATION_ZXY)
			{
				tmp = acc_res1[2];
				acc_res1[2] = acc_res1[1];
				acc_res1[1] = acc_res1[0];
				acc_res1[0] = tmp;
			}
			#endif
			if(persistent_settings.ACC_ORIENTATION==ACC_ORIENTATION_ZYX)
			{
				tmp = acc_res1[2];
				acc_res1[2] = acc_res1[0];
				acc_res1[0] = tmp;
			}
		}

		acc_lowPass(acc_res1,acc_res);

		/*
		normalized_params[0] = acc_res[0]/2.0f+0.5f;
		normalized_params[3] = acc_res[1]/2.0f+0.5f;
		normalized_params[1] = acc_res[2]/2.0f+0.5f;
		normalized_params[2] = 0;//(normalized_params[0] + normalized_params[1] + normalized_params[2]) / 3.0f;
		*/

		#ifdef DEBUG_ACCELEROMETER
		printf("process_accelerometer(): in=%f,%f,%f out=%f,%f,%f\n",acc_res1[0],acc_res1[1],acc_res1[2],acc_res[0],acc_res[1],acc_res[2]);
		#endif
	}
}
#endif

#ifdef USE_MMA8452_ACCELEROMETER

uint16_t MMA8452_read_register(uint8_t reg)
{
    //uint16_t val;
    uint8_t val;
    //esp_err_t result;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    /*result =*/ i2c_master_write_byte(cmd, MMA8452_I2C_ADDRESS << 1 | WRITE_BIT, ACK_CHECK_EN);
    //printf("MMA8452_read_register(): result = %d\n", result);
    /*result =*/ i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    //printf("MMA8452_read_register(): result = %d\n", result);

    i2c_master_start(cmd);
    /*result =*/ i2c_master_write_byte(cmd, MMA8452_I2C_ADDRESS << 1 | READ_BIT, ACK_CHECK_EN);
    //printf("MMA8452_read_register(): result = %d\n", result);
    /*result =*/ i2c_master_read_byte(cmd, &val, I2C_MASTER_NACK);
    //printf("MMA8452_read_register(): result = %d\n", result);
    //i2c_master_read(cmd, (unsigned char*)&val, 2, I2C_MASTER_LAST_NACK);

    i2c_master_stop(cmd);
    /*result =*/ i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    //printf("MMA8452_read_register(): result = %d\n", result);
    i2c_cmd_link_delete(cmd);

    return val;
}

void MMA8452_write_register(uint8_t reg, uint8_t val)
{
    esp_err_t result;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    result = i2c_master_write_byte(cmd, MMA8452_I2C_ADDRESS << 1 | WRITE_BIT, ACK_CHECK_EN);
    printf("MMA8452_write_register(): result = %d\n", result);
    result = i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    printf("MMA8452_write_register(): result = %d\n", result);
    //i2c_master_start(cmd);
    //result = i2c_master_write_byte(cmd, MMA8452_I2C_ADDRESS << 1 | WRITE_BIT, ACK_CHECK_EN);
    //printf("MMA8452_write_register(): result = %d\n", result);
    result = i2c_master_write_byte(cmd, val, ACK_CHECK_EN);
    printf("MMA8452_write_register(): result = %d\n", result);
    i2c_master_stop(cmd);
    result = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    printf("MMA8452_write_register(): result = %d\n", result);
    i2c_cmd_link_delete(cmd);
}

void process_accelerometer(void *pvParameters)
{
	int measure_delay = ((AccelerometerParam_t*)pvParameters)->measure_delay;

	printf("process_accelerometer(): task running on core ID=%d\n",xPortGetCoreID());
	printf("process_accelerometer(): measure_delay = %d\n",measure_delay);

	int16_t acc_val;
	int8_t byte_reg;

	while(i2c_bus_mutex)
	{
		printf("process_accelerometer(): waiting for I2C\n");
		Delay(10);
	}
	i2c_bus_mutex = 1;

	byte_reg = MMA8452_read_register(0x0B); //0x0B: SYSMOD system mode register
	printf("process_accelerometer(): SYSMOD reg = %d\n", byte_reg);

	MMA8452_write_register(0x2A, 0x01); //0x2A: CTRL_REG1 system control 1 register -> set ACTIVE bit

	byte_reg = MMA8452_read_register(0x0B); //0x0B: SYSMOD system mode register
	printf("process_accelerometer(): SYSMOD reg = %d\n", byte_reg);

	i2c_bus_mutex = 0;

	while(1)
	{
		Delay(measure_delay);

		if(!accelerometer_active)
		{
			continue;
		}

		if(i2c_driver_installed)
		{
			if(i2c_bus_mutex)
			{
				continue;
			}

			i2c_bus_mutex = 1;

			acc_val = MMA8452_read_register(0x01);
			acc_res1[0] = (float)acc_val;
			//printf("process_accelerometer(): acc_val[0] = %d\n", acc_val);

			acc_val = MMA8452_read_register(0x03);
			acc_res1[1] = (float)acc_val;
			//printf("process_accelerometer(): acc_val[1] = %d\n", acc_val);

			acc_val = MMA8452_read_register(0x05);
			acc_res1[2] = (float)acc_val;
			//printf("process_accelerometer(): acc_val[2] = %d\n", acc_val);

			i2c_bus_mutex = 0;
		}
		else
		{
			#ifdef DEBUG_ACCELEROMETER
			printf("process_accelerometer(): I2C offline\n");
			#endif
			continue;
		}

		acc_lowPass(acc_res1,acc_res);

		#ifdef DEBUG_ACCELEROMETER
		printf("process_accelerometer(): in=%f,%f,%f out=%f,%f,%f\n",acc_res1[0],acc_res1[1],acc_res1[2],acc_res[0],acc_res[1],acc_res[2]);
		#endif
	}
}
#endif


#ifdef USE_WMA6981_ACCELEROMETER

uint16_t WMA6981_read_register(uint8_t reg)
{
    //uint16_t val;
    uint8_t val = 0;
    //esp_err_t result;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    /*result =*/ i2c_master_write_byte(cmd, WMA6981_I2C_ADDRESS << 1 | WRITE_BIT, ACK_CHECK_EN);
    //printf("WMA6981_read_register(): result = %d\n", result);
    /*result =*/ i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    //printf("WMA6981_read_register(): result = %d\n", result);

    i2c_master_start(cmd);
    /*result =*/ i2c_master_write_byte(cmd, WMA6981_I2C_ADDRESS << 1 | READ_BIT, ACK_CHECK_EN);
    //printf("WMA6981_read_register(): result = %d\n", result);
    /*result =*/ i2c_master_read_byte(cmd, &val, I2C_MASTER_NACK);
    //printf("WMA6981_read_register(): result = %d\n", result);
    //i2c_master_read(cmd, (unsigned char*)&val, 2, I2C_MASTER_LAST_NACK);

    i2c_master_stop(cmd);
    /*result =*/ i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    //printf("WMA6981_read_register(): result = %d\n", result);

    i2c_cmd_link_delete(cmd);
    return val;
}

int16_t WMA6981_read_register_2b(uint8_t reg)
{
    int16_t val;
    //esp_err_t result;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    /*result =*/ i2c_master_write_byte(cmd, WMA6981_I2C_ADDRESS << 1 | WRITE_BIT, ACK_CHECK_EN);
    //printf("WMA6981_read_register_2b(): result = %d\n", result);
    /*result =*/ i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    //printf("WMA6981_read_register_2b(): result = %d\n", result);

    i2c_master_start(cmd);
    /*result =*/ i2c_master_write_byte(cmd, WMA6981_I2C_ADDRESS << 1 | READ_BIT, ACK_CHECK_EN);
    //printf("WMA6981_read_register_2b(): result = %d\n", result);
    i2c_master_read(cmd, (unsigned char*)&val, 2, I2C_MASTER_LAST_NACK);
    //printf("WMA6981_read_register_2b(): result = %d\n", result);

    i2c_master_stop(cmd);
    /*result =*/ i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    //printf("WMA6981_read_register_2b(): result = %d\n", result);

    i2c_cmd_link_delete(cmd);
    return val;
}

void WMA6981_read_3_registers(int16_t *dest, uint8_t reg)
{
    //esp_err_t result;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    /*result =*/ i2c_master_write_byte(cmd, WMA6981_I2C_ADDRESS << 1 | WRITE_BIT, ACK_CHECK_EN);
    //printf("WMA6981_read_register(): result = %d\n", result);
    /*result =*/ i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    //printf("WMA6981_read_register(): result = %d\n", result);

    i2c_master_start(cmd);
    /*result =*/ i2c_master_write_byte(cmd, WMA6981_I2C_ADDRESS << 1 | READ_BIT, ACK_CHECK_EN);
    //printf("WMA6981_read_register(): result = %d\n", result);
    i2c_master_read(cmd, (unsigned char*)dest, 6, I2C_MASTER_LAST_NACK);
    //printf("WMA6981_read_register(): result = %d\n", result);

    i2c_master_stop(cmd);
    /*result =*/ i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    //printf("WMA6981_read_register(): result = %d\n", result);

    i2c_cmd_link_delete(cmd);
}

void WMA6981_write_register(uint8_t reg, uint8_t val)
{
    esp_err_t result;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);
    result = i2c_master_write_byte(cmd, WMA6981_I2C_ADDRESS << 1 | WRITE_BIT, ACK_CHECK_EN);
    //printf("WMA6981_write_register(): result = %d\n", result);
    result = i2c_master_write_byte(cmd, reg, ACK_CHECK_EN);
    //printf("WMA6981_write_register(): result = %d\n", result);
    result = i2c_master_write_byte(cmd, val, ACK_CHECK_EN);
    //printf("WMA6981_write_register(): result = %d\n", result);
    i2c_master_stop(cmd);
    result = i2c_master_cmd_begin(I2C_MASTER_NUM, cmd, 1000 / portTICK_RATE_MS);
    //printf("WMA6981_write_register(): result = %d\n", result);

    i2c_cmd_link_delete(cmd);
}

void accelerometer_power_up()
{
	int8_t byte_reg;

	while(i2c_bus_mutex)
	{
		printf("accelerometer_power_up(): waiting for I2C\n");
		Delay(10);
	}
	i2c_bus_mutex = 1;

	#ifdef USE_MMA8452_ACCELEROMETER

	byte_reg = MMA8452_read_register(0x0B); //0x0B: SYSMOD system mode register
	printf("accelerometer_power_up(): SYSMOD reg = %d\n", byte_reg);

	MMA8452_write_register(0x2A, 0x01); //0x2A: CTRL_REG1 system control 1 register -> set ACTIVE bit

	byte_reg = MMA8452_read_register(0x0B); //0x0B: SYSMOD system mode register
	printf("accelerometer_power_up(): SYSMOD reg = %d\n", byte_reg);

	#endif

	#ifdef USE_WMA6981_ACCELEROMETER

	byte_reg = WMA6981_read_register(0x00); //Register 0x00 (CHIP ID)
	//printf("accelerometer_power_up(): CHIP ID reg = 0x%x\n", byte_reg);

	byte_reg = WMA6981_read_register(0x11); //Register 0x11 (POWER)
	//printf("accelerometer_power_up(): POWER reg = 0x%x\n", byte_reg);

	WMA6981_write_register(0x11, 0x80); //Register 0x11 (POWER) -> set MODE_BIT and RESV

	byte_reg = WMA6981_read_register(0x11); //Register 0x11 (POWER)
	//printf("accelerometer_power_up(): POWER reg = 0x%x\n", byte_reg);

	#endif

	i2c_bus_mutex = 0;
}

void process_accelerometer(void *pvParameters)
{
	int measure_delay = ((AccelerometerParam_t*)pvParameters)->measure_delay;

	printf("process_accelerometer(): task running on core ID=%d\n",xPortGetCoreID());
	//printf("process_accelerometer(): measure_delay = %d\n",measure_delay);

	#ifdef USE_MMA8452_ACCELEROMETER
	int16_t acc_val;
	#endif
	#ifdef USE_WMA6981_ACCELEROMETER
	//int16_t acc_val1, acc_val2;
	int16_t acc_val[3];
	#endif

	accelerometer_power_up();

	while(1)
	{
		Delay(measure_delay);

		if(!accelerometer_active)
		{
			continue;
		}

		if(i2c_driver_installed)
		{
			if(i2c_bus_mutex)
			{
				continue;
			}

			i2c_bus_mutex = 1;

			#ifdef USE_MMA8452_ACCELEROMETER
			acc_val = MMA8452_read_register(0x01);
			//printf("reg[1]=%d	", acc_val);
			acc_res1[0] = (float)acc_val;
			acc_val = MMA8452_read_register(0x03);
			//printf("reg[2]=%d	", acc_val);
			acc_res1[1] = (float)acc_val;
			acc_val = MMA8452_read_register(0x05);
			//printf("reg[3]=%d	", acc_val);
			acc_res1[2] = (float)acc_val;
			#endif

			#ifdef USE_WMA6981_ACCELEROMETER

			/*
			//reading individual registers:

			acc_val1 = WMA6981_read_register(0x01);
			//printf("reg[1]=%d	", acc_val1);
			acc_val2 = WMA6981_read_register(0x02);
			//printf("reg[2]=%d	", acc_val2);
			acc_res1[0] = (float)acc_val1 + (float)(acc_val2<<8);
			printf("process_accelerometer(): acc_val1 = %d, acc_val2 = %d, acc_res1[0] = %f\n", acc_val1, acc_val2, acc_res1[0]);

			acc_val1 = WMA6981_read_register(0x03);
			//printf("reg[3]=%d	", acc_val1);
			acc_val2 = WMA6981_read_register(0x04);
			//printf("reg[4]=%d	", acc_val2);
			acc_res1[1] = (float)acc_val1 + (float)(acc_val2<<8);
			//printf("process_accelerometer(): acc_val1 = %d, acc_val2 = %d, acc_res1[1] = %f\n", acc_val1, acc_val2, acc_res1[1]);

			acc_val1 = WMA6981_read_register(0x05);
			//printf("reg[5]=%d	", acc_val1);
			acc_val2 = WMA6981_read_register(0x06);
			//printf("reg[6]=%d\n", acc_val2);
			acc_res1[2] = (float)acc_val1 + (float)(acc_val2<<8);
			//printf("process_accelerometer(): acc_val1 = %d, acc_val2 = %d, acc_res1[2] = %f\n", acc_val1, acc_val2, acc_res1[2]);
			*/

			/*
			//reading two registers at once as signed 16 bit integer
			acc_val1 = WMA6981_read_register_2b(0x01);
			acc_res1[0] = (float)acc_val1;
			printf("process_accelerometer(): acc_val1 = %d, acc_res1[0] = %f\n", acc_val1, acc_res1[0]);
			*/

			//reading all 3 registers at once (3 signed 16 bit integers)
			WMA6981_read_3_registers(acc_val, 0x01);
			//printf("process_accelerometer(): acc_val[0..2] = %d,%d,%d\n", acc_val[0], acc_val[1], acc_val[2]);

			#define ACC_AXIS_DIV_X	(float)(16384/128)	//double resolution = spread over 90 degrees
			//#define ACC_AXIS_DIV_Y	(float)(16384/64)	//full resolution = spread over 180 degrees
			//#define ACC_AXIS_DIV_Y	ACC_AXIS_DIV_X
			#define ACC_AXIS_DIV_Y	(float)(16384/192)	//triple resolution = spread over 60 degrees
			//#define ACC_AXIS_DIV_Y	(float)(16384/256)	//quadruple resolution = spread over 45 degrees
			#define ACC_AXIS_DIV_Z	ACC_AXIS_DIV_X

			acc_res1[0] = (float)acc_val[0] / ACC_AXIS_DIV_X;
			acc_res1[1] = (float)acc_val[1] / ACC_AXIS_DIV_Y;
			acc_res1[2] = (float)acc_val[2] / ACC_AXIS_DIV_Z;
			//printf("process_accelerometer(): acc_res1[0..2] = %f,%f,%f\n", acc_res1[0], acc_res1[1], acc_res1[2]);

			#endif

			i2c_bus_mutex = 0;
		}
		else
		{
			#ifdef DEBUG_ACCELEROMETER
			printf("process_accelerometer(): I2C offline\n");
			#endif
			continue;
		}

		acc_lowPass(acc_res1, acc_res);

		Delay(2);

		#ifdef DEBUG_ACCELEROMETER
		printf("process_accelerometer(): in =	%f,	%f,	%f	out =	%f,	%f,	%f\n", acc_res1[0], acc_res1[1], acc_res1[2], acc_res[0], acc_res[1], acc_res[2]);
		#endif
	}
}
#endif

#ifdef ACCELEROMETER_TEST
void accelerometer_test()
{
	accelerometer_active = 1;
	while(1)
	{
		printf("process_accelerometer(): out=%f,%f,%f\n",acc_res[0],acc_res[1],acc_res[2]);
		Delay(250);
	}
}
#endif

#ifdef ACCELEROMETER_TEST

void accelerometer_test()
{
	float xmax = 0, xmin = 0;
	int error;
	float res[3];

	KX123 acc(I2C_MASTER_NUM);
	//e_axis axis;
	uint8_t axis;

	//e_interrupt_reason int_reason;
	uint8_t int_reason;

	error = acc.set_defaults();
	printf("acc.set_defaults() err=%d\n",error);

	while(1)
	{
		acc.get_tap_interrupt_axis((e_axis*)&axis);

		acc.get_interrupt_reason((e_interrupt_reason*)&int_reason);

		if(int_reason > 0)
		{
			printf("get_interrupt_reason=%x ... ",int_reason);
			printf("get_tap_interrupt_axis=%x \n",axis);
		}

		acc.clear_interrupt();

		error = acc.getresults_g(res);
		//error = acc.getresults_highpass_g(res);
		printf("err=%d,res=%f,%f,%f\n",error,res[0],res[1],res[2]);

		Delay(10);
	}
}

#endif

#ifdef ACC_CALIBRATION_ENABLED

void accelerometer_calibrate()
{
	printf("accelerometer_calibrate(): calibrating\n");
	while(acc_calibrating)
	{
		acc_calibrating--;

		if(acc_calibrating==1)
		{
			acc_calibrating = 0;

			acc_calibrated[0] = acc_res[0];
			acc_calibrated[1] = acc_res[1];

			printf("accelerometer_calibrate(): calibrated: %f / %f\n", acc_calibrated[0], acc_calibrated[1]);
		}
		Delay(ACCELEROMETER_MEASURE_DELAY);
	}
}

#endif
