/*
 * serial_cmd.h
 *
 *  Copyright 2024 Phonicbloom Ltd.
 *
 *  Serial command interface — receives text commands over UART0 (USB serial)
 *  and exposes control over scales, patches, tuning, and per-pad pitch.
 *
 *  This file can be used within the terms of GNU GPLv3 license: https://www.gnu.org/licenses/gpl-3.0.en.html
 */

#ifndef SERIAL_CMD_H_
#define SERIAL_CMD_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Tuning reference multiplier applied at sample_drum() call time.
   1.0 = 440 Hz reference.  Set via SET_TUNING command. */
extern float tuning_global_multiplier;
extern float tuning_global_hz;

/* Patch-switch request, written by serial task, consumed by app_main dispatch loop.
   type = EVENT_NEXT_CHANNEL_* value, or -1 for no pending request.
   index = desired 0-based patch index within the type, or -1 for next-in-cycle. */
extern int serial_patch_request_type;
extern int serial_patch_request_index;

/* Current patch state, written by app_main, read by GET_PATCH handler. */
extern int current_patch_type;
extern int current_patch_index;

void serial_command_task(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif /* SERIAL_CMD_H_ */
