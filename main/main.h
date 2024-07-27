/*
 * main.h
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

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h> //for uint16_t type

#include "glo_config.h"
#include <hw/init.h>
#include <hw/codec.h>
#include <hw/gpio.h>
#include <hw/signals.h>
#include <hw/Accelerometer.h>
#include <hw/ui.h>
#include <hw/midi.h>

#include <ChannelsDef.h>
#include <InitChannels.h>
#include <Interface.h>
#include <dsp/SampleDrum.h>

//-----------------------------------------------------------------------------------
