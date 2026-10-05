#include "mick_field.h"

#include <stdlib.h>
#include <string.h>

static uint32_t xorshift(uint32_t *state)
{
    uint32_t x = *state;
    if (x == 0) x = 0x6D69636Bu;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

static int rand_range(uint32_t *state, int lo, int hi)
{
    uint32_t span = (uint32_t)(hi - lo + 1);
    return lo + (int)(xorshift(state) % span);
}

static int iabs(int value)
{
    return value < 0 ? -value : value;
}

uint8_t mick_density_count(mick_density_t density)
{
    static const uint8_t counts[MICK_DENSITY_COUNT] = {2, 5, 9, 14, 19};
    if ((unsigned)density >= MICK_DENSITY_COUNT) return counts[0];
    return counts[density];
}

uint32_t mick_step_period_ms(mick_tempo_t tempo)
{
    if (tempo == MICK_TEMPO_STEP_1S) return 1000;
    if (tempo == MICK_TEMPO_STEP_2S) return 2000;
    if (tempo == MICK_TEMPO_STEP_5S) return 5000;
    if (tempo == MICK_TEMPO_STEP_10S) return 10000;
    return 0;
}

uint8_t mick_blob_color(const mick_field_t *field, int index)
{
    const mick_blob_t *blob = &field->blobs[index];
    if (field->chroma == MICK_CHROMA_MONO) return field->color_index;
    return (uint8_t)((blob->slot + field->color_index) % MICK_COLOR_COUNT);
}

uint8_t mick_blob_kind(const mick_field_t *field, int index)
{
    if (field->form == MICK_FORM_MIX) return field->blobs[index].mix_kind;
    return (uint8_t)field->form;
}

bool mick_blob_in_bounds(const mick_blob_t *blob)
{
    if (blob->w <= 0 || blob->h <= 0) return false;
    if (blob->x < MICK_ART_X || blob->y < MICK_ART_Y) return false;
    if (blob->x + blob->w > MICK_ART_X + MICK_ART_W) return false;
    if (blob->y + blob->h > MICK_ART_Y + MICK_ART_H) return false;
    return true;
}

static bool ellipse_inside(int px, int py, int cx, int cy, int rx, int ry)
{
    int32_t dx = px - cx;
    int32_t dy = py - cy;
    int32_t rx2 = (int32_t)rx * rx;
    int32_t ry2 = (int32_t)ry * ry;
    return dx * dx * ry2 + dy * dy * rx2 <= rx2 * ry2;
}

static int32_t orient(int x1, int y1, int x2, int y2, int x3, int y3)
{
    return (int32_t)(x2 - x1) * (y3 - y1) - (int32_t)(y2 - y1) * (x3 - x1);
}

static void tri_vertices(const mick_blob_t *blob, int ox, int oy,
                         int *xs, int *ys)
{
    int x = blob->x + ox;
    int y = blob->y + oy;
    int cx = x + blob->w / 2;
    int bottom = y + blob->h - 1;
    int right = x + blob->w - 1;
    if ((blob->seed & 4u) == 0) {
        xs[0] = cx;
        ys[0] = y;
        xs[1] = x;
        ys[1] = bottom;
        xs[2] = right;
        ys[2] = bottom;
    } else {
        xs[0] = cx;
        ys[0] = bottom;
        xs[1] = x;
        ys[1] = y;
        xs[2] = right;
        ys[2] = y;
    }
}

static bool triangle_has(int x0, int y0, int x1, int y1, int x2, int y2, int px, int py)
{
    int32_t a = orient(x0, y0, x1, y1, px, py);
    int32_t b = orient(x1, y1, x2, y2, px, py);
    int32_t c = orient(x2, y2, x0, y0, px, py);
    bool neg = (a < 0) || (b < 0) || (c < 0);
    bool pos = (a > 0) || (b > 0) || (c > 0);
    return !(neg && pos);
}

static int cross_thickness(const mick_blob_t *blob)
{
    int span = blob->w < blob->h ? blob->w : blob->h;
    int thickness = span / 4;
    if (thickness < 4) thickness = 4;
    return thickness;
}

bool mick_point_inside(const mick_blob_t *blob, uint8_t kind, int px, int py)
{
    if (px < blob->x || py < blob->y) return false;
    if (px >= blob->x + blob->w || py >= blob->y + blob->h) return false;

    int cx = blob->x + blob->w / 2;
    int cy = blob->y + blob->h / 2;
    int rx = blob->w / 2;
    int ry = blob->h / 2;
    if (rx < 1) rx = 1;
    if (ry < 1) ry = 1;

    if (kind == MICK_FORM_DISC || kind == MICK_FORM_ARC) {
        int use_rx = kind == MICK_FORM_ARC ? rx * 4 / 5 : rx;
        int use_ry = kind == MICK_FORM_ARC ? ry * 4 / 5 : ry;
        if (use_rx < 1) use_rx = 1;
        if (use_ry < 1) use_ry = 1;
        if (!ellipse_inside(px, py, cx, cy, use_rx, use_ry)) return false;
        if (kind == MICK_FORM_DISC) return true;
        return (blob->seed & 1u) == 0 ? py <= cy : py >= cy;
    }
    if (kind == MICK_FORM_RING) {
        int in_rx = rx * 55 / 100;
        int in_ry = ry * 55 / 100;
        if (in_rx < 1) in_rx = 1;
        if (in_ry < 1) in_ry = 1;
        return ellipse_inside(px, py, cx, cy, rx, ry)
            && !ellipse_inside(px, py, cx, cy, in_rx, in_ry);
    }
    if (kind == MICK_FORM_BAR || kind == MICK_FORM_LINE) return true;
    if (kind == MICK_FORM_DIAMOND) {
        int dx = iabs(px - cx);
        int dy = iabs(py - cy);
        return (int32_t)dx * ry + (int32_t)dy * rx <= (int32_t)rx * ry;
    }
    if (kind == MICK_FORM_TRI) {
        int xs[3];
        int ys[3];
        tri_vertices(blob, 0, 0, xs, ys);
        return triangle_has(xs[0], ys[0], xs[1], ys[1], xs[2], ys[2], px, py);
    }
    if (kind == MICK_FORM_CROSS) {
        int thickness = cross_thickness(blob);
        int y0 = cy - thickness / 2;
        int x0 = cx - thickness / 2;
        bool horizontal = py >= y0 && py < y0 + thickness;
        bool vertical = px >= x0 && px < x0 + thickness;
        return horizontal || vertical;
    }
    if (kind == MICK_FORM_WEDGE) {
        int right = blob->x + blob->w - 1;
        int bottom = blob->y + blob->h - 1;
        return triangle_has(blob->x, blob->y, right, blob->y, blob->x, bottom, px, py);
    }
    return false;
}

int mick_blob_specks(const mick_blob_t *blob, uint8_t kind, uint16_t phase,
                     mick_speck_t *out, int cap)
{
    if (out == NULL || cap <= 0 || blob->w <= 0 || blob->h <= 0) return 0;

    int want = (blob->w * blob->h) / 220;
    if (want < 5) want = 5;
    if (want > MICK_SPECK_MAX) want = MICK_SPECK_MAX;
    if (want > cap) want = cap;

    uint32_t rng = ((uint32_t)blob->seed << 16) ^ blob->seed;
    rng ^= ((uint32_t)phase << 8) ^ (uint32_t)kind * 17u;
    if (rng == 0) rng = 1u;

    int count = 0;
    int tries = 0;
    while (count < want && tries < want * 12) {
        int px = blob->x + (int)(xorshift(&rng) % (uint32_t)blob->w);
        int py = blob->y + (int)(xorshift(&rng) % (uint32_t)blob->h);
        tries++;
        if (!mick_point_inside(blob, kind, px, py)) continue;
        out[count].x = (int16_t)px;
        out[count].y = (int16_t)py;
        out[count].hot = (uint8_t)((count % 5) == 0);
        count++;
    }
    return count;
}

static int cmp_area_desc(const void *left, const void *right)
{
    const mick_blob_t *a = left;
    const mick_blob_t *b = right;
    int area_a = a->w * a->h;
    int area_b = b->w * b->h;
    return (area_b > area_a) - (area_b < area_a);
}

static void clamp_size(int *w, int *h)
{
    if (*w > MICK_ART_W - 8) *w = MICK_ART_W - 8;
    if (*h > MICK_ART_H - 8) *h = MICK_ART_H - 8;
    if (*w < 6) *w = 6;
    if (*h < 6) *h = 6;
}

static void pick_size(mick_field_t *field, mick_blob_t *blob, int index)
{
    int w;
    int h;
    if (index < 2) {
        w = rand_range(&field->rng, 104, 156);
        h = rand_range(&field->rng, 92, 148);
    } else if (index == 2) {
        w = rand_range(&field->rng, 14, 28);
        h = rand_range(&field->rng, 14, 32);
    } else {
        int roll = (int)(xorshift(&field->rng) % 10u);
        if (roll < 2) {
            if ((xorshift(&field->rng) & 1u) != 0) {
                w = rand_range(&field->rng, 86, 176);
                h = rand_range(&field->rng, 6, 12);
            } else {
                w = rand_range(&field->rng, 6, 12);
                h = rand_range(&field->rng, 86, 168);
            }
        } else if (roll < 5) {
            w = rand_range(&field->rng, 14, 28);
            h = rand_range(&field->rng, 14, 32);
        } else if (roll < 7) {
            w = rand_range(&field->rng, 18, 36);
            h = rand_range(&field->rng, 72, 132);
        } else {
            w = rand_range(&field->rng, 42, 78);
            h = rand_range(&field->rng, 36, 74);
        }
    }
    clamp_size(&w, &h);
    blob->w = (int16_t)w;
    blob->h = (int16_t)h;
}

static void place_blob(mick_field_t *field, mick_blob_t *blob)
{
    int max_x = MICK_ART_X + MICK_ART_W - blob->w;
    int max_y = MICK_ART_Y + MICK_ART_H - blob->h;
    if (max_x < MICK_ART_X) max_x = MICK_ART_X;
    if (max_y < MICK_ART_Y) max_y = MICK_ART_Y;
    blob->x = (int16_t)rand_range(&field->rng, MICK_ART_X, max_x);
    blob->y = (int16_t)rand_range(&field->rng, MICK_ART_Y, max_y);
    blob->tx = blob->x;
    blob->ty = blob->y;
}

static void layout(mick_field_t *field)
{
    field->count = mick_density_count(field->density);
    for (uint8_t i = 0; i < field->count; i++) {
        mick_blob_t *blob = &field->blobs[i];
        memset(blob, 0, sizeof(*blob));
        pick_size(field, blob, (int)i);
        place_blob(field, blob);
        blob->mix_kind = (uint8_t)(i % (int)MICK_FORM_MIX);
        blob->slot = (uint8_t)(i % MICK_COLOR_COUNT);
        blob->seed = (uint16_t)(xorshift(&field->rng) & 0xFFFFu);
        if (blob->seed == 0) blob->seed = 1;
        blob->turn_ms = (uint16_t)rand_range(&field->rng, 600, 2400);
    }
    qsort(field->blobs, field->count, sizeof(field->blobs[0]), cmp_area_desc);
    field->dirty = (uint8_t)(MICK_DIRTY_ART | MICK_DIRTY_CHROME);
}

static int8_t signed_speed(uint32_t *state, int lo, int span)
{
    int mag = lo + (int)(xorshift(state) % (uint32_t)span);
    int sign = (xorshift(state) & 1u) ? 1 : -1;
    return (int8_t)(sign * mag);
}

static void arm_motion(mick_field_t *field)
{
    field->since_ms = 0;
    for (uint8_t i = 0; i < field->count; i++) {
        mick_blob_t *blob = &field->blobs[i];
        blob->tx = blob->x;
        blob->ty = blob->y;
        blob->rem_x = 0;
        blob->rem_y = 0;
        if (field->phase == MICK_PHASE_CANVAS && field->tempo == MICK_TEMPO_DRIFT) {
            blob->vx = signed_speed(&field->rng, 5, 9);
            blob->vy = signed_speed(&field->rng, 5, 9);
            blob->turn_ms = (uint16_t)rand_range(&field->rng, 700, 2600);
        } else if (field->phase == MICK_PHASE_CANVAS && field->tempo == MICK_TEMPO_IMPACT) {
            blob->vx = signed_speed(&field->rng, 48, 36);
            blob->vy = signed_speed(&field->rng, 48, 36);
            blob->turn_ms = 0;
        } else {
            blob->vx = 0;
            blob->vy = 0;
        }
    }
    field->dirty = (uint8_t)(field->dirty | MICK_DIRTY_ART | MICK_DIRTY_CHROME);
}

static void retarget(mick_field_t *field, mick_blob_t *blob)
{
    int16_t ox = blob->x;
    int16_t oy = blob->y;
    for (int attempt = 0; attempt < 4; attempt++) {
        place_blob(field, blob);
        if (blob->tx != ox || blob->ty != oy) break;
    }
    blob->x = ox;
    blob->y = oy;
}

static void glide(mick_blob_t *blob)
{
    int dx = blob->tx - blob->x;
    int dy = blob->ty - blob->y;
    if (dx != 0) {
        int step = dx / 3;
        if (step == 0) step = dx > 0 ? 1 : -1;
        blob->x = (int16_t)(blob->x + step);
    }
    if (dy != 0) {
        int step = dy / 3;
        if (step == 0) step = dy > 0 ? 1 : -1;
        blob->y = (int16_t)(blob->y + step);
    }
}

static void bounce(mick_blob_t *blob)
{
    int max_x = MICK_ART_X + MICK_ART_W - blob->w;
    int max_y = MICK_ART_Y + MICK_ART_H - blob->h;
    if (blob->x < MICK_ART_X) {
        blob->x = MICK_ART_X;
        blob->vx = (int8_t)iabs(blob->vx);
    } else if (blob->x > max_x) {
        blob->x = (int16_t)max_x;
        blob->vx = (int8_t)-iabs(blob->vx);
    }
    if (blob->y < MICK_ART_Y) {
        blob->y = MICK_ART_Y;
        blob->vy = (int8_t)iabs(blob->vy);
    } else if (blob->y > max_y) {
        blob->y = (int16_t)max_y;
        blob->vy = (int8_t)-iabs(blob->vy);
    }
    if (blob->vx == 0) blob->vx = 6;
    if (blob->vy == 0) blob->vy = -6;
}

static void integrate(mick_blob_t *blob, uint32_t dt_ms)
{
    blob->rem_x += (int32_t)blob->vx * (int32_t)dt_ms;
    blob->rem_y += (int32_t)blob->vy * (int32_t)dt_ms;
    int step_x = (int)(blob->rem_x / 1000);
    int step_y = (int)(blob->rem_y / 1000);
    blob->rem_x -= step_x * 1000;
    blob->rem_y -= step_y * 1000;
    blob->x = (int16_t)(blob->x + step_x);
    blob->y = (int16_t)(blob->y + step_y);
    bounce(blob);
}

static void drift(mick_field_t *field, uint32_t dt_ms)
{
    for (uint8_t i = 0; i < field->count; i++) {
        mick_blob_t *blob = &field->blobs[i];
        integrate(blob, dt_ms);
        if (blob->turn_ms <= dt_ms) {
            blob->vx = signed_speed(&field->rng, 5, 9);
            blob->vy = signed_speed(&field->rng, 5, 9);
            blob->turn_ms = (uint16_t)rand_range(&field->rng, 700, 2600);
        } else {
            blob->turn_ms = (uint16_t)(blob->turn_ms - dt_ms);
        }
    }
    field->dirty = (uint8_t)(field->dirty | MICK_DIRTY_ART);
}

static void impact(mick_field_t *field, uint32_t dt_ms)
{
    for (uint8_t i = 0; i < field->count; i++) {
        integrate(&field->blobs[i], dt_ms);
    }
    field->dirty = (uint8_t)(field->dirty | MICK_DIRTY_ART);
}

static void step_motion(mick_field_t *field, uint32_t dt_ms)
{
    uint32_t period = mick_step_period_ms(field->tempo);
    bool jumped = false;
    field->since_ms += dt_ms;
    while (period > 0 && field->since_ms >= period) {
        field->since_ms -= period;
        field->jumps++;
        jumped = true;
        for (uint8_t i = 0; i < field->count; i++) {
            retarget(field, &field->blobs[i]);
        }
    }

    bool moving = false;
    for (uint8_t i = 0; i < field->count; i++) {
        mick_blob_t *blob = &field->blobs[i];
        if (blob->x != blob->tx || blob->y != blob->ty) {
            glide(blob);
            moving = true;
        }
    }
    if (jumped || moving) {
        field->dirty = (uint8_t)(field->dirty | MICK_DIRTY_ART);
    }
}

void mick_field_init(mick_field_t *field, uint32_t seed)
{
    memset(field, 0, sizeof(*field));
    field->rng = seed != 0 ? seed : 0x4D49434Bu;
    field->phase = MICK_PHASE_CATALOG;
    field->row = MICK_ROW_DENSITY;
    field->density = MICK_DENSITY_2;
    field->chroma = MICK_CHROMA_MULTI;
    field->tempo = MICK_TEMPO_STEP_1S;
    field->ground = MICK_GROUND_GRAY;
    field->form = MICK_FORM_MIX;
    layout(field);
}

static void cycle_row_value(mick_field_t *field)
{
    if (field->row == MICK_ROW_DENSITY) {
        field->density = (mick_density_t)(((int)field->density + 1) % MICK_DENSITY_COUNT);
        layout(field);
        return;
    }
    if (field->row == MICK_ROW_CHROMA) {
        field->chroma = field->chroma == MICK_CHROMA_MONO
            ? MICK_CHROMA_MULTI : MICK_CHROMA_MONO;
    } else if (field->row == MICK_ROW_TEMPO) {
        field->tempo = (mick_tempo_t)(((int)field->tempo + 1) % MICK_TEMPO_COUNT);
    } else {
        field->ground = (mick_ground_t)(((int)field->ground + 1) % MICK_GROUND_COUNT);
    }
    field->dirty = (uint8_t)(field->dirty | MICK_DIRTY_ART | MICK_DIRTY_CHROME);
}

void mick_field_on_input(mick_field_t *field, mick_input_t input)
{
    if (field->phase == MICK_PHASE_CATALOG) {
        if (input == MICK_INPUT_UP) {
            field->row = (mick_row_t)(((int)field->row + MICK_ROW_COUNT - 1) % MICK_ROW_COUNT);
            field->dirty = (uint8_t)(field->dirty | MICK_DIRTY_CHROME);
            return;
        }
        if (input == MICK_INPUT_DOWN) {
            field->row = (mick_row_t)(((int)field->row + 1) % MICK_ROW_COUNT);
            field->dirty = (uint8_t)(field->dirty | MICK_DIRTY_CHROME);
            return;
        }
        if (input == MICK_INPUT_OK) {
            cycle_row_value(field);
            return;
        }
        if (input == MICK_INPUT_OK_LONG) {
            field->phase = MICK_PHASE_CANVAS;
            arm_motion(field);
        }
        return;
    }

    if (input == MICK_INPUT_UP) {
        field->color_index = (uint8_t)((field->color_index + 1) % MICK_COLOR_COUNT);
        field->dirty = (uint8_t)(field->dirty | MICK_DIRTY_ART);
        return;
    }
    if (input == MICK_INPUT_DOWN) {
        field->form = (mick_form_t)(((int)field->form + 1) % MICK_FORM_COUNT);
        field->dirty = (uint8_t)(field->dirty | MICK_DIRTY_ART | MICK_DIRTY_CHROME);
        return;
    }
    if (input == MICK_INPUT_OK_LONG) {
        field->phase = MICK_PHASE_CATALOG;
        arm_motion(field);
    }
}

void mick_field_tick(mick_field_t *field, uint32_t dt_ms)
{
    if (field->phase != MICK_PHASE_CANVAS || dt_ms == 0) return;

    field->grain_ms += dt_ms;
    while (field->grain_ms >= 700u) {
        field->grain_ms -= 700u;
        field->grain++;
        field->dirty = (uint8_t)(field->dirty | MICK_DIRTY_ART);
    }

    if (field->tempo == MICK_TEMPO_DRIFT) drift(field, dt_ms);
    else if (field->tempo == MICK_TEMPO_IMPACT) impact(field, dt_ms);
    else step_motion(field, dt_ms);
}
