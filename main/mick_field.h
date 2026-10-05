#pragma once

#include <stdbool.h>
#include <stdint.h>

// Kinetic composition for the moving-art canvas.
// Coordinates are pixels. The art box fills the 240x320 panel inside a
// small inset. Pure logic: no LVGL and no ESP-IDF.

#define MICK_SCREEN_W 240
#define MICK_SCREEN_H 320
#define MICK_ART_X 6
#define MICK_ART_Y 4
#define MICK_ART_W 228
#define MICK_ART_H 308
#define MICK_BLOB_MAX 22
#define MICK_COLOR_COUNT 6
#define MICK_SPECK_MAX 18

#define MICK_DIRTY_ART 1u
#define MICK_DIRTY_CHROME 2u

typedef enum {
    MICK_PHASE_CATALOG = 0,
    MICK_PHASE_CANVAS,
} mick_phase_t;

typedef enum {
    MICK_ROW_DENSITY = 0,
    MICK_ROW_CHROMA,
    MICK_ROW_TEMPO,
    MICK_ROW_GROUND,
    MICK_ROW_COUNT,
} mick_row_t;

typedef enum {
    MICK_DENSITY_1 = 0,
    MICK_DENSITY_2,
    MICK_DENSITY_3,
    MICK_DENSITY_4,
    MICK_DENSITY_5,
    MICK_DENSITY_COUNT,
} mick_density_t;

typedef enum {
    MICK_CHROMA_MONO = 0,
    MICK_CHROMA_MULTI,
} mick_chroma_t;

typedef enum {
    MICK_TEMPO_STEP_1S = 0,
    MICK_TEMPO_STEP_2S,
    MICK_TEMPO_STEP_5S,
    MICK_TEMPO_STEP_10S,
    MICK_TEMPO_DRIFT,
    MICK_TEMPO_IMPACT,
    MICK_TEMPO_COUNT,
} mick_tempo_t;

typedef enum {
    MICK_GROUND_WHITE = 0,
    MICK_GROUND_GRAY,
    MICK_GROUND_BLACK,
    MICK_GROUND_BEIGE,
    MICK_GROUND_CREAM,
    MICK_GROUND_WHEAT,
    MICK_GROUND_COUNT,
} mick_ground_t;

typedef enum {
    MICK_FORM_DISC = 0,
    MICK_FORM_TRI,
    MICK_FORM_BAR,
    MICK_FORM_RING,
    MICK_FORM_DIAMOND,
    MICK_FORM_ARC,
    MICK_FORM_CROSS,
    MICK_FORM_LINE,
    MICK_FORM_WEDGE,
    MICK_FORM_MIX,
    MICK_FORM_COUNT,
} mick_form_t;

typedef enum {
    MICK_INPUT_UP = 0,
    MICK_INPUT_DOWN,
    MICK_INPUT_OK,
    MICK_INPUT_OK_LONG,
} mick_input_t;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    int16_t tx;
    int16_t ty;
    int8_t vx;
    int8_t vy;
    int32_t rem_x;
    int32_t rem_y;
    uint16_t turn_ms;
    uint8_t mix_kind;
    uint8_t slot;
    uint16_t seed;
} mick_blob_t;

typedef struct {
    int16_t x;
    int16_t y;
    uint8_t hot;
} mick_speck_t;

typedef struct {
    mick_phase_t phase;
    mick_row_t row;
    mick_density_t density;
    mick_chroma_t chroma;
    mick_tempo_t tempo;
    mick_ground_t ground;
    mick_form_t form;
    uint8_t color_index;
    uint8_t count;
    uint8_t dirty;
    uint16_t jumps;
    uint16_t grain;
    uint32_t since_ms;
    uint32_t grain_ms;
    uint32_t rng;
    mick_blob_t blobs[MICK_BLOB_MAX];
} mick_field_t;

void mick_field_init(mick_field_t *field, uint32_t seed);
void mick_field_on_input(mick_field_t *field, mick_input_t input);
void mick_field_tick(mick_field_t *field, uint32_t dt_ms);

uint8_t mick_density_count(mick_density_t density);
uint32_t mick_step_period_ms(mick_tempo_t tempo);
uint8_t mick_blob_color(const mick_field_t *field, int index);
uint8_t mick_blob_kind(const mick_field_t *field, int index);
bool mick_blob_in_bounds(const mick_blob_t *blob);
bool mick_point_inside(const mick_blob_t *blob, uint8_t kind, int px, int py);
int mick_blob_specks(const mick_blob_t *blob, uint8_t kind, uint16_t phase,
                     mick_speck_t *out, int cap);
