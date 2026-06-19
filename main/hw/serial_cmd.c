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
