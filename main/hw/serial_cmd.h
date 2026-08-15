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
   1.0 = 440 Hz reference.  Set via SET_TUNING command; persists across a
   power cycle (see store_tuning_hz()/load_tuning_hz() in glo_config.c and
   serial_cmd_load_persisted_tuning() below). */
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

/* Set to 1 whenever SET_EFFECT/SET_ECHO_LEN successfully writes a value
   (other than SET_EFFECT:tilt, which clears it back to 0). While set, the
   accelerometer-driven effect depth control (SampleDrum.cpp) is suspended
   so a remotely-set value sticks — e.g. the drum sitting flat on a desk.
   Cleared by ui.c when the player changes patch with a physical wood/metal
   button press, since that means they've picked the drum up to play it
   directly — or explicitly by SET_EFFECT:tilt, without forcing a patch
   change. */
extern int serial_effect_override;

/* Set to 1 by app_main once the full boot sequence (including I2S init) is
   complete.  The serial task must not install the UART0 driver before this. */
extern volatile int serial_cmd_boot_ready;

void serial_command_task(void *pvParameters);

/* Event notification helpers (EVT:* lines).  Call from wherever the
   corresponding state changes so a connected host can stay in sync.
   Safe to call from any task — plain printf to UART0. */
void serial_evt_slot(void);
void serial_evt_effect(void);
void serial_evt_patch(void);

/* Commits any tuning/scale NVS write that SET_PAD_TUNING/SET_SCALE_NOTES
   deferred (see serial_cmd.c). Must be called from task context (never an
   ISR — it can call into NVS/flash) before anything that would either
   reload the current patch's data from NVS (discarding the unflushed RAM
   change) or cut power (losing it outright): patch switches (serial or
   physical button), SET_TUNING's reload, and any power-off path. */
void serial_cmd_flush_pending_nvs(void);

/* Restores the last SET_TUNING value from NVS. Call once from app_main()
   right after nvs_flash_init(), before anything reads
   tuning_global_multiplier (see serial_cmd.c). */
void serial_cmd_load_persisted_tuning(void);

#ifdef __cplusplus
}
#endif

#endif /* SERIAL_CMD_H_ */
