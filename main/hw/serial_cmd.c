/*
 * serial_cmd.c
 *
 *  Copyright 2024 Phonicbloom Ltd.
 *
 *  Serial command interface — receives text commands over UART0 (USB serial)
 *  and exposes control over scales, patches, tuning, and per-pad pitch.
 *
 *  Protocol: newline-terminated ASCII commands.
 *  Responses: "OK", "OK:<value>", or "ERR:<reason>"
 *
 *  This file can be used within the terms of GNU GPLv3 license: https://www.gnu.org/licenses/gpl-3.0.en.html
 */

#include "serial_cmd.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "glo_config.h"
#include "hw/ui.h"
#include "hw/init.h"
#include "hw/midi.h"
#include "hw/signals.h"

#define SERIAL_CMD_UART     UART_NUM_0
#define SERIAL_CMD_BUF_SIZE 2048
#define SERIAL_CMD_LINE_MAX 256

/* Global definitions */
float tuning_global_multiplier = 1.0f;
float tuning_global_hz         = 440.0f;
int   serial_patch_request_type  = -1;
int   serial_patch_request_index = -1;
int   current_patch_type  = EVENT_NEXT_CHANNEL_METAL;
int   current_patch_index = 0;
volatile int serial_cmd_boot_ready = 0;
int   serial_effect_override = 0;

/* Depth as reported/accepted by SET_EFFECT:depth / GET_EFFECT_DEPTH is a
   0-100 percentage of the same range the accelerometer drives via
   ECHO_MIXING_GAIN_MUL_TARGET (see SampleDrum.cpp's SENSOR_DELAY_9 case
   and ui.c's long_press_wood_metal_both handler, both of which use 8.0f
   as the deepest setting). 100% == fully tilted. */
#define ECHO_DEPTH_GAIN_MAX 8.0f

/* ------------------------------------------------------------------ */
/* Helpers                                                              */
/* ------------------------------------------------------------------ */

/* Parse 9 MIDI note integers out of the raw text line returned by
   get_scale() (format "name: n, n, n, ...").  Returns number of notes
   written into out[].  out[] must have room for NOTES_PER_SCALE ints. */
static int parse_scale_text(char *text, int *out)
{
    char *p = strstr(text, ":");
    if (!p) return 0;
    p++;
    while (*p == ' ' || *p == '\t') p++;

    /* get_scale() expects lower-case note names */
    for (int i = 0; i < (int)strlen(p); i++)
        p[i] = tolower((unsigned char)p[i]);

    int n = 0;
    while (n < NOTES_PER_SCALE && p && *p) {
        out[n++] = parse_note(p);
        p = strstr(p, ",");
        if (p) p++;
    }
    return n;
}

/* ------------------------------------------------------------------ */
/* Event notifications (EVT:*)                                          */
/*                                                                      */
/* Unsolicited lines announcing state changes (typically made on the    */
/* hardware itself) so a connected host can keep its UI in sync.        */
/* Hosts that only parse OK/ERR lines silently discard these, so the    */
/* addition is backward compatible by design.                           */
/* ------------------------------------------------------------------ */

/* Shared formatter for GET_EFFECT ("OK:") and event ("EVT:EFFECT:").
   Effect state is fully encoded in echo_dynamic_loop_current_step:
   0-7 = "delay" bank (long->short), 8 and 17 = off, 9-16 = "short" bank,
   -1 = custom length set via fine adjust / SET_ECHO_LEN.
   Both banks drive the same echo_buffer/echo_dynamic_loop_length — this
   is one delay effect with two preset ranges, NOT the polyphonic reverb
   engine (that only exists in the SET_PATCH:reverb easter-egg channel
   and has no remotely controllable parameters). */
static void print_effect_state(const char *prefix)
{
    int step = echo_dynamic_loop_current_step;

    if (step == 8 || step == ECHO_DYNAMIC_LOOP_LENGTH_ECHO_OFF ||
        echo_dynamic_loop_length == 0)
        printf("%soff\n", prefix);
    else if (step >= 0 && step <= 7)
        printf("%sdelay,%d\n", prefix, step);
    else if (step >= 9 && step <= 16)
        printf("%sshort,%d\n", prefix, step - 9);
    else
        printf("%scustom,%d\n", prefix, echo_dynamic_loop_length);
}

void serial_evt_slot(void)
{
    printf("EVT:SLOT:%d\n", selected_scale[current_patch]);
}

void serial_evt_effect(void)
{
    print_effect_state("EVT:EFFECT:");
}

void serial_evt_patch(void)
{
    const char *type;
    if      (current_patch_type == EVENT_NEXT_CHANNEL_METAL) type = "metal";
    else if (current_patch_type == EVENT_NEXT_CHANNEL_WOOD)  type = "wood";
    else                                                      type = "reverb";
    printf("EVT:PATCH:%s,%d\n", type, current_patch_index);
}

/* ------------------------------------------------------------------ */
/* Command handlers                                                     */
/* ------------------------------------------------------------------ */

static void cmd_set_scale(const char *arg)
{
    char *notes_text = NULL;
    int count = get_scale((char *)arg, &notes_text);

    if (count != NOTES_PER_SCALE) {
        if (notes_text) free(notes_text);
        printf("ERR:SCALE_NOT_FOUND\n");
        return;
    }

    /* Write into slot 7 — reserved for serial-loaded scales so slots 0-6 are untouched */
    int slot = SCALES_PER_PATCH - 1;
    int tmp[NOTES_PER_SCALE];
    int parsed = parse_scale_text(notes_text, tmp);
    free(notes_text);

    if (parsed != NOTES_PER_SCALE) {
        printf("ERR:PARSE_FAILED\n");
        return;
    }

    for (int n = 0; n < NOTES_PER_SCALE; n++)
        patch_notes[slot * NOTES_PER_SCALE + n] = tmp[n];

    selected_scale[current_patch] = slot;
    change_scale();
    store_patch_scales_if_changed(current_patch, patch_notes);
    printf("OK\n");
}

static void cmd_get_scale(void)
{
    printf("OK:");
    for (int n = 0; n < NOTES_PER_SCALE; n++)
        printf("%d%s", midi_scale_selected[n], n < NOTES_PER_SCALE - 1 ? "," : "");
    printf("\n");
}

static void cmd_set_patch(const char *arg)
{
    char type_str[16] = {0};
    int  index = -1;

    /* arg is "metal", "metal,2", "wood,0", "reverb" etc. */
    const char *comma = strchr(arg, ',');
    if (comma) {
        int len = (int)(comma - arg);
        if (len >= (int)sizeof(type_str)) len = (int)sizeof(type_str) - 1;
        strncpy(type_str, arg, len);
        index = atoi(comma + 1);
    } else {
        strncpy(type_str, arg, sizeof(type_str) - 1);
    }

    int req_type;
    int max_index;
    if (strcmp(type_str, "metal") == 0) {
        req_type  = EVENT_NEXT_CHANNEL_METAL;
        max_index = patches_found_metal - 1;
    } else if (strcmp(type_str, "wood") == 0) {
        req_type  = EVENT_NEXT_CHANNEL_WOOD;
        max_index = patches_found_wood - 1;
    } else if (strcmp(type_str, "reverb") == 0) {
        req_type  = EVENT_NEXT_CHANNEL_BOTH;
        max_index = 0;
        index     = -1; /* reverb has no index */
    } else {
        printf("ERR:INVALID_TYPE\n");
        return;
    }

    if (index != -1 && index > max_index) {
        printf("ERR:INDEX_OUT_OF_RANGE\n");
        return;
    }

    serial_patch_request_type  = req_type;
    serial_patch_request_index = index;
    event_next_channel         = req_type; /* wake DSP inner loop */
    printf("OK\n");
}

static void cmd_get_patch(void)
{
    const char *type;
    if      (current_patch_type == EVENT_NEXT_CHANNEL_METAL) type = "metal";
    else if (current_patch_type == EVENT_NEXT_CHANNEL_WOOD)  type = "wood";
    else                                                      type = "reverb";
    printf("OK:%s,%d\n", type, current_patch_index);
}

static void cmd_set_tuning(const char *arg)
{
    float hz = (float)atof(arg);
    if (hz < 400.0f || hz > 500.0f) {
        printf("ERR:INVALID_HZ (range 400-500)\n");
        return;
    }
    tuning_global_hz         = hz;
    tuning_global_multiplier = hz / 440.0f;

    /* Trigger reload of current patch so multiplier takes effect immediately */
    serial_patch_request_type  = current_patch_type;
    serial_patch_request_index = current_patch_index;
    event_next_channel         = current_patch_type;
    printf("OK\n");
}

static void cmd_get_tuning(void)
{
    printf("OK:%.2f\n", tuning_global_hz);
}

static void cmd_set_pad_tuning(const char *arg)
{
    int   pad;
    float cents;
    if (sscanf(arg, "%d,%f", &pad, &cents) != 2 ||
        pad < 0 || pad >= NOTES_PER_SCALE) {
        printf("ERR:INVALID (usage: SET_PAD_TUNING:<pad 0-8>,<cents>)\n");
        return;
    }
    float mult = powf(2.0f, cents / 1200.0f);
    micro_tuning_selected[pad] = mult;
    /* Mirror into the backing array so NVS persist and reload are consistent */
    micro_tuning[selected_scale[current_patch] * NOTES_PER_SCALE + pad] = mult;
    store_micro_tuning_if_changed(current_patch, micro_tuning);
    printf("OK\n");
}

static void cmd_get_pad_tuning(const char *arg)
{
    int pad = atoi(arg);
    if (pad < 0 || pad >= NOTES_PER_SCALE) {
        printf("ERR:INVALID_PAD\n");
        return;
    }
    float cents = log2f(micro_tuning_selected[pad]) * 1200.0f;
    printf("OK:%.2f\n", cents);
}

/* SET_SCALE_DEF:<slot>,<scale_name>  — load named scale into a specific slot */
static void cmd_set_scale_def(const char *arg)
{
    const char *comma = strchr(arg, ',');
    if (!comma) { printf("ERR:USAGE SET_SCALE_DEF:<slot>,<name>\n"); return; }

    int slot = atoi(arg);
    if (slot < 0 || slot >= SCALES_PER_PATCH) {
        printf("ERR:INVALID_SLOT (0-%d)\n", SCALES_PER_PATCH - 1);
        return;
    }

    const char *name = comma + 1;
    char *notes_text = NULL;
    int count = get_scale((char *)name, &notes_text);

    if (count != NOTES_PER_SCALE) {
        if (notes_text) free(notes_text);
        printf("ERR:SCALE_NOT_FOUND\n");
        return;
    }

    int tmp[NOTES_PER_SCALE];
    int parsed = parse_scale_text(notes_text, tmp);
    free(notes_text);

    if (parsed != NOTES_PER_SCALE) {
        printf("ERR:PARSE_FAILED\n");
        return;
    }

    for (int n = 0; n < NOTES_PER_SCALE; n++)
        patch_notes[slot * NOTES_PER_SCALE + n] = tmp[n];

    store_patch_scales_if_changed(current_patch, patch_notes);
    printf("OK\n");
}

/* SET_SCALE_NOTES:<slot>,<n0>,<n1>,...,<n8>  — write raw MIDI notes into a slot */
static void cmd_set_scale_notes(const char *arg)
{
    int slot;
    int notes[NOTES_PER_SCALE];
    const char *p = arg;

    slot = atoi(p);
    if (slot < 0 || slot >= SCALES_PER_PATCH) {
        printf("ERR:INVALID_SLOT (0-%d)\n", SCALES_PER_PATCH - 1);
        return;
    }
    p = strchr(p, ',');
    if (!p) { printf("ERR:USAGE SET_SCALE_NOTES:<slot>,<n0>,...,<n8>\n"); return; }
    p++;

    for (int n = 0; n < NOTES_PER_SCALE; n++) {
        notes[n] = atoi(p);
        p = strchr(p, ',');
        if (p) p++;
        else if (n < NOTES_PER_SCALE - 1) {
            printf("ERR:NEED_%d_NOTES\n", NOTES_PER_SCALE);
            return;
        }
    }

    for (int n = 0; n < NOTES_PER_SCALE; n++)
        patch_notes[slot * NOTES_PER_SCALE + n] = notes[n];

    store_patch_scales_if_changed(current_patch, patch_notes);
    printf("OK\n");
}

/* ------------------------------------------------------------------ */
/* Slot (scale preset) selection                                        */
/* ------------------------------------------------------------------ */

static void cmd_get_slot(void)
{
    printf("OK:%d\n", selected_scale[current_patch]);
}

static void cmd_set_slot(const char *arg)
{
    int slot = atoi(arg);
    if (slot < 0 || slot >= SCALES_PER_PATCH) {
        printf("ERR:INVALID_SLOT (0-%d)\n", SCALES_PER_PATCH - 1);
        return;
    }
    /* Mirror change_scale_up()/change_scale_down() exactly */
    previous_scale = selected_scale[current_patch];
    selected_scale[current_patch] = slot;
    change_scale();
    printf("OK:%d\n", slot);
}

/* ------------------------------------------------------------------ */
/* Effect (delay/reverb engine) control                                 */
/*                                                                      */
/* The hardware flow (hold wood+metal -> pick effect -> confirm with    */
/* power button) ultimately just writes echo_dynamic_loop_current_step  */
/* and echo_dynamic_loop_length, then persists via                      */
/* persistent_settings_store_delay().  These commands write the same    */
/* variables directly, so no button choreography needs emulating.       */
/* ------------------------------------------------------------------ */

static void cmd_get_effect(void)
{
    print_effect_state("OK:");
}

/* The polyphonic reverb "easter egg" patch (SET_PATCH:reverb) is a
   separate DSP path with no remotely controllable delay/reverb
   parameters of its own — echo_dynamic_loop_* and ECHO_MIXING_GAIN_MUL*
   aren't used there. Reject writes instead of silently accepting a
   value the hardware won't act on. */
static int effect_params_available(void)
{
    if (current_patch_type == EVENT_NEXT_CHANNEL_BOTH) {
        printf("ERR:UNAVAILABLE_IN_POLY_REVERB\n");
        return 0;
    }
    return 1;
}

/* SET_EFFECT:off | delay,<0-7> | short,<0-7> | depth,<0-100>
   "delay" and "short" are the same echo/delay effect's two hardware
   preset banks (wood/metal button while in the length menu) — there is
   no separate reverb engine here. "depth" sets the wet/feedback amount
   normally driven by accelerometer tilt (ECHO_MIXING_GAIN_MUL_TARGET);
   any successful SET_EFFECT suspends that tilt control so the value
   sticks — e.g. for the drum sitting flat on a desk — until the player
   changes patch with a physical button press (see ui.c). */
static void cmd_set_effect(const char *arg)
{
    if (!effect_params_available()) return;

    if (strncmp(arg, "depth", 5) == 0) {
        const char *comma = strchr(arg, ',');
        int pct = comma ? atoi(comma + 1) : -1;
        if (pct < 0 || pct > 100) { printf("ERR:INVALID_DEPTH (0-100)\n"); return; }

        ECHO_MIXING_GAIN_MUL_TARGET = (float)pct / 100.0f * ECHO_DEPTH_GAIN_MAX;
        serial_effect_override = 1;
        printf("OK:%d\n", pct);
        return;
    }

    int step;

    if (strcmp(arg, "off") == 0) {
        step = 8;  /* echo_dynamic_loop_steps[8] == 0 == effect off */
    } else if (strncmp(arg, "delay", 5) == 0) {
        const char *comma = strchr(arg, ',');
        int idx = comma ? atoi(comma + 1) : 0;
        if (idx < 0 || idx > 7) { printf("ERR:INVALID_INDEX (delay 0-7)\n"); return; }
        step = idx;
    } else if (strncmp(arg, "short", 5) == 0) {
        const char *comma = strchr(arg, ',');
        int idx = comma ? atoi(comma + 1) : 0;
        if (idx < 0 || idx > 7) { printf("ERR:INVALID_INDEX (short 0-7)\n"); return; }
        step = 9 + idx;
    } else {
        printf("ERR:USAGE SET_EFFECT:off|delay,<0-7>|short,<0-7>|depth,<0-100>\n");
        return;
    }

    echo_dynamic_loop_current_step = step;
    echo_dynamic_loop_length       = echo_dynamic_loop_steps[step];
    persistent_settings_store_delay(step); /* = confirming with the power button */
    serial_effect_override = 1;
    printf("OK\n");
}

static void cmd_get_echo_len(void)
{
    printf("OK:%d\n", echo_dynamic_loop_length);
}

/* SET_ECHO_LEN:<samples> — free-form loop length, serial equivalent of
   the hardware fine adjust (volume +/- inside the delay menu).
   0 switches the effect off; other values are clamped to the same
   limits fine_adjust_delay_length() enforces. */
static void cmd_set_echo_len(const char *arg)
{
    if (!effect_params_available()) return;

    int len = atoi(arg);

    if (len == 0) {
        echo_dynamic_loop_current_step = ECHO_DYNAMIC_LOOP_LENGTH_ECHO_OFF;
        echo_dynamic_loop_length       = 0;
        persistent_settings_store_delay(ECHO_DYNAMIC_LOOP_LENGTH_ECHO_OFF);
        serial_effect_override = 1;
        printf("OK:0\n");
        return;
    }

    if (len > ECHO_BUFFER_LENGTH)     len = ECHO_BUFFER_LENGTH;
    if (len < ECHO_BUFFER_LENGTH_MIN) len = ECHO_BUFFER_LENGTH_MIN;

    echo_dynamic_loop_current_step = -1;    /* custom, as fine_adjust does */
    echo_dynamic_loop_length       = len;
    persistent_settings_store_delay(-len);  /* negative = raw length (ui.c convention) */
    serial_effect_override = 1;
    printf("OK:%d\n", len);
}

static void cmd_get_effect_depth(void)
{
    int pct = (int)(ECHO_MIXING_GAIN_MUL_TARGET / ECHO_DEPTH_GAIN_MAX * 100.0f + 0.5f);
    if (pct < 0)   pct = 0;
    if (pct > 100) pct = 100;
    printf("OK:%d\n", pct);
}

/* ------------------------------------------------------------------ */
/* Dispatcher                                                           */
/* ------------------------------------------------------------------ */

static void handle_command(char *line)
{
    /* Strip trailing whitespace */
    int len = (int)strlen(line);
    while (len > 0 && (line[len-1] == '\r' || line[len-1] == '\n' || line[len-1] == ' '))
        line[--len] = '\0';

    if (len == 0) return;

    printf("[CMD] %s\n", line);

    if      (strncmp(line, "SET_SCALE:",       10) == 0) cmd_set_scale(line + 10);
    else if (strcmp (line, "GET_SCALE")            == 0) cmd_get_scale();
    else if (strncmp(line, "SET_PATCH:",       10) == 0) cmd_set_patch(line + 10);
    else if (strcmp (line, "GET_PATCH")            == 0) cmd_get_patch();
    else if (strncmp(line, "SET_TUNING:",      11) == 0) cmd_set_tuning(line + 11);
    else if (strcmp (line, "GET_TUNING")           == 0) cmd_get_tuning();
    else if (strncmp(line, "SET_PAD_TUNING:",  15) == 0) cmd_set_pad_tuning(line + 15);
    else if (strncmp(line, "GET_PAD_TUNING:",  15) == 0) cmd_get_pad_tuning(line + 15);
    else if (strncmp(line, "SET_SCALE_DEF:",   14) == 0) cmd_set_scale_def(line + 14);
    else if (strncmp(line, "SET_SCALE_NOTES:", 16) == 0) cmd_set_scale_notes(line + 16);
    else if (strcmp (line, "GET_SLOT")             == 0) cmd_get_slot();
    else if (strncmp(line, "SET_SLOT:",         9) == 0) cmd_set_slot(line + 9);
    else if (strcmp (line, "GET_EFFECT")           == 0) cmd_get_effect();
    else if (strncmp(line, "SET_EFFECT:",      11) == 0) cmd_set_effect(line + 11);
    else if (strcmp (line, "GET_ECHO_LEN")         == 0) cmd_get_echo_len();
    else if (strncmp(line, "SET_ECHO_LEN:",    13) == 0) cmd_set_echo_len(line + 13);
    else if (strcmp (line, "GET_EFFECT_DEPTH")     == 0) cmd_get_effect_depth();
    else    printf("ERR:UNKNOWN_CMD\n");
}

/* ------------------------------------------------------------------ */
/* Task entry point                                                     */
/* ------------------------------------------------------------------ */

void serial_command_task(void *pvParameters)
{
    /* Wait for the main boot sequence (including I2S driver install) to finish
       before claiming a UART0 interrupt, to avoid intr-slot conflicts. */
    while (!serial_cmd_boot_ready) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    uart_config_t cfg = {
        .baud_rate  = 115200,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 0,
        .use_ref_tick = false,
    };
    uart_param_config(SERIAL_CMD_UART, &cfg);
    /* Install RX buffer only (tx_buffer_size=0 keeps TX in direct mode so printf works unchanged) */
    uart_driver_install(SERIAL_CMD_UART, SERIAL_CMD_BUF_SIZE, 0, 0, NULL, 0);

    char line[SERIAL_CMD_LINE_MAX];
    int  pos = 0;
    uint8_t ch;

    while (1) {
        if (uart_read_bytes(SERIAL_CMD_UART, &ch, 1, pdMS_TO_TICKS(20)) > 0) {
            if (ch == '\n' || ch == '\r') {
                if (pos > 0) {
                    line[pos] = '\0';
                    handle_command(line);
                    pos = 0;
                }
            } else if (pos < SERIAL_CMD_LINE_MAX - 1) {
                line[pos++] = (char)ch;
            }
        }
    }
}
