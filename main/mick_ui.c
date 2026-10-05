#include "mick_ui.h"

#include "mick_copy.h"

#include "lvgl.h"

#include <stdio.h>

LV_FONT_DECLARE(mick_latin_16);
LV_FONT_DECLARE(mick_latin_28);

#define PAPER 0xF6F1E8
#define INK 0x161616
#define BLUE 0x1E4E9A
#define RED 0xE23B2A
#define YELLOW 0xF2C14E
#define MUTED 0x5C564E

static const uint32_t PAINT[MICK_COLOR_COUNT] = {
    0xE23B2A,
    0x1E4E9A,
    0xF2C14E,
    0x161616,
    0x2F7D3A,
    0xE36A1E,
};

static lv_obj_t *s_art;
static lv_obj_t *s_screen;
static lv_obj_t *s_catalog;
static lv_obj_t *s_form;
static lv_obj_t *s_battery;
static lv_obj_t *s_title;
static lv_obj_t *s_sub;
static lv_obj_t *s_cards[MICK_ROW_COUNT];
static lv_obj_t *s_keys[MICK_ROW_COUNT];
static lv_obj_t *s_values[MICK_ROW_COUNT];
static mick_field_t s_paint;
static int s_shown_battery = -2;

static void flatten(lv_obj_t *obj)
{
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_shadow_width(obj, 0, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static void show(lv_obj_t *obj, bool visible)
{
    if (visible) lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    return label;
}

static const char *form_name(mick_form_t form)
{
    switch (form) {
    case MICK_FORM_DISC: return MICK_KIND_DISC;
    case MICK_FORM_TRI: return MICK_KIND_TRI;
    case MICK_FORM_BAR: return MICK_KIND_BAR;
    case MICK_FORM_RING: return MICK_KIND_RING;
    case MICK_FORM_DIAMOND: return MICK_KIND_DIAMOND;
    case MICK_FORM_ARC: return MICK_KIND_ARC;
    case MICK_FORM_CROSS: return MICK_KIND_CROSS;
    case MICK_FORM_LINE: return MICK_KIND_LINE;
    case MICK_FORM_WEDGE: return MICK_KIND_WEDGE;
    default: return MICK_KIND_MIX;
    }
}

static const char *row_name(mick_row_t row)
{
    if (row == MICK_ROW_CHROMA) return MICK_ROW_COLOR_LABEL;
    if (row == MICK_ROW_TEMPO) return MICK_ROW_TEMPO_LABEL;
    if (row == MICK_ROW_GROUND) return MICK_ROW_PAPER_LABEL;
    return MICK_ROW_COUNT_LABEL;
}

static const char *chroma_name(mick_chroma_t chroma)
{
    return chroma == MICK_CHROMA_MONO ? MICK_MONO : MICK_MULTI;
}

static const char *density_text(mick_density_t density)
{
    static const char *names[MICK_DENSITY_COUNT] = {"1", "2", "3", "4", "5"};
    if ((unsigned)density >= MICK_DENSITY_COUNT) return names[0];
    return names[density];
}

static const char *tempo_name(mick_tempo_t tempo)
{
    if (tempo == MICK_TEMPO_STEP_2S) return MICK_SEC2;
    if (tempo == MICK_TEMPO_STEP_5S) return MICK_SEC5;
    if (tempo == MICK_TEMPO_STEP_10S) return MICK_SEC10;
    if (tempo == MICK_TEMPO_DRIFT) return MICK_DRIFT;
    if (tempo == MICK_TEMPO_IMPACT) return MICK_IMPACT;
    return MICK_SEC1;
}

static uint32_t ground_hex(mick_ground_t ground)
{
    static const uint32_t colors[MICK_GROUND_COUNT] = {
        0xF4F4F2,
        0xC5C3BE,
        0x141414,
        0xC6A67A,
        0xF6F1E8,
        0xF8E6B8,
    };
    if ((unsigned)ground >= MICK_GROUND_COUNT) return colors[MICK_GROUND_GRAY];
    return colors[ground];
}

static const char *ground_name(mick_ground_t ground)
{
    static const char *names[MICK_GROUND_COUNT] = {
        MICK_PAPER_WHITE,
        MICK_PAPER_GRAY,
        MICK_PAPER_BLACK,
        MICK_PAPER_BEIGE,
        MICK_PAPER_CREAM,
        MICK_PAPER_WHEAT,
    };
    if ((unsigned)ground >= MICK_GROUND_COUNT) return names[MICK_GROUND_GRAY];
    return names[ground];
}

static bool ground_is_dark(mick_ground_t ground)
{
    return ground == MICK_GROUND_BLACK;
}

static uint32_t paint_hex(uint8_t index, mick_ground_t ground)
{
    uint32_t color = PAINT[index % MICK_COLOR_COUNT];
    if (ground_is_dark(ground) && color == INK) return 0xF4F1EA;
    return color;
}

static void draw_disc_or_bar(lv_layer_t *layer, const lv_area_t *area, lv_color_t color,
                             lv_opa_t opa, bool disc)
{
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color = color;
    dsc.bg_opa = opa;
    dsc.radius = disc ? LV_RADIUS_CIRCLE : 0;
    dsc.border_width = 0;
    lv_draw_rect(layer, &dsc, area);
}

static void draw_ring(lv_layer_t *layer, const mick_blob_t *blob, const lv_area_t *area,
                      lv_color_t color, lv_opa_t opa)
{
    int span = blob->w < blob->h ? blob->w : blob->h;
    int width = span / 7;
    if (width < 3) width = 3;
    if (width > 12) width = 12;
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_opa = LV_OPA_TRANSP;
    dsc.radius = LV_RADIUS_CIRCLE;
    dsc.border_width = width;
    dsc.border_color = color;
    dsc.border_opa = opa;
    lv_draw_rect(layer, &dsc, area);
}

static void draw_triangle(lv_layer_t *layer, int x0, int y0, int x1, int y1,
                          int x2, int y2, lv_color_t color, lv_opa_t opa)
{
    lv_draw_triangle_dsc_t dsc;
    lv_draw_triangle_dsc_init(&dsc);
    dsc.color = color;
    dsc.opa = opa;
    dsc.p[0].x = x0;
    dsc.p[0].y = y0;
    dsc.p[1].x = x1;
    dsc.p[1].y = y1;
    dsc.p[2].x = x2;
    dsc.p[2].y = y2;
    lv_draw_triangle(layer, &dsc);
}

static void draw_half(lv_layer_t *layer, const mick_blob_t *blob, lv_color_t color,
                      lv_opa_t opa, int ox, int oy)
{
    static const int arc_c[7] = {1000, 866, 500, 0, -500, -866, -1000};
    static const int arc_s[7] = {0, 500, 866, 1000, 866, 500, 0};
    int x = blob->x + ox;
    int y = blob->y + oy;
    int cx = x + blob->w / 2;
    int cy = y + blob->h / 2;
    int rx = blob->w / 2;
    int ry = blob->h / 2;
    if (rx < 1) rx = 1;
    if (ry < 1) ry = 1;
    bool upper = (blob->seed & 1u) == 0;
    int px[7];
    int py[7];
    for (int i = 0; i < 7; i++) {
        px[i] = cx + arc_c[i] * rx / 1000;
        int sy = arc_s[i] * ry / 1000;
        py[i] = upper ? cy - sy : cy + sy;
    }
    for (int i = 0; i < 6; i++) {
        draw_triangle(layer, cx, cy, px[i], py[i], px[i + 1], py[i + 1], color, opa);
    }
}

static void draw_cross(lv_layer_t *layer, const mick_blob_t *blob, lv_color_t color,
                       lv_opa_t opa, int ox, int oy)
{
    int x = blob->x + ox;
    int y = blob->y + oy;
    int cx = x + blob->w / 2;
    int cy = y + blob->h / 2;
    int span = blob->w < blob->h ? blob->w : blob->h;
    int thickness = span / 4;
    if (thickness < 4) thickness = 4;
    int y0 = cy - thickness / 2;
    int x0 = cx - thickness / 2;
    lv_area_t horizontal = {
        .x1 = x,
        .y1 = y0,
        .x2 = x + blob->w - 1,
        .y2 = y0 + thickness - 1,
    };
    lv_area_t vertical = {
        .x1 = x0,
        .y1 = y,
        .x2 = x0 + thickness - 1,
        .y2 = y + blob->h - 1,
    };
    draw_disc_or_bar(layer, &horizontal, color, opa, false);
    draw_disc_or_bar(layer, &vertical, color, opa, false);
}
static void draw_shape(lv_layer_t *layer, const mick_blob_t *blob, uint8_t kind,
                       lv_color_t color, lv_opa_t opa, int ox, int oy, bool specks,
                       uint16_t phase)
{
    int x = blob->x + ox;
    int y = blob->y + oy;
    lv_area_t area = {
        .x1 = x,
        .y1 = y,
        .x2 = x + blob->w - 1,
        .y2 = y + blob->h - 1,
    };
    if (kind == MICK_FORM_DISC) {
        draw_disc_or_bar(layer, &area, color, opa, true);
    } else if (kind == MICK_FORM_BAR || kind == MICK_FORM_LINE) {
        draw_disc_or_bar(layer, &area, color, opa, false);
    } else if (kind == MICK_FORM_RING) {
        draw_ring(layer, blob, &area, color, opa);
    } else if (kind == MICK_FORM_ARC) {
        draw_half(layer, blob, color, opa, ox, oy);
    } else if (kind == MICK_FORM_CROSS) {
        draw_cross(layer, blob, color, opa, ox, oy);
    } else if (kind == MICK_FORM_WEDGE) {
        draw_triangle(layer, x, y, x + blob->w - 1, y, x, y + blob->h - 1, color, opa);
    } else if (kind == MICK_FORM_TRI) {
        int xs[3];
        int ys[3];
        int cx = x + blob->w / 2;
        int bottom = y + blob->h - 1;
        int right = x + blob->w - 1;
        if ((blob->seed & 4u) == 0) {
            xs[0] = cx; ys[0] = y; xs[1] = x; ys[1] = bottom; xs[2] = right; ys[2] = bottom;
        } else {
            xs[0] = cx; ys[0] = bottom; xs[1] = x; ys[1] = y; xs[2] = right; ys[2] = y;
        }
        draw_triangle(layer, xs[0], ys[0], xs[1], ys[1], xs[2], ys[2], color, opa);
    } else {
        int cx = x + blob->w / 2;
        int cy = y + blob->h / 2;
        int right = x + blob->w - 1;
        int bottom = y + blob->h - 1;
        draw_triangle(layer, cx, y, right, cy, cx, bottom, color, opa);
        draw_triangle(layer, cx, y, cx, bottom, x, cy, color, opa);
    }

    if (!specks) return;
    mick_speck_t dots[MICK_SPECK_MAX];
    int count = mick_blob_specks(blob, kind, phase, dots, MICK_SPECK_MAX);
    for (int i = 0; i < count; i++) {
        lv_draw_rect_dsc_t dot;
        lv_draw_rect_dsc_init(&dot);
        dot.radius = 0;
        dot.bg_opa = dots[i].hot ? LV_OPA_COVER : LV_OPA_80;
        dot.bg_color = lv_color_hex(dots[i].hot ? 0xFFF8EC : INK);
        lv_area_t speck = {
            .x1 = dots[i].x + ox,
            .y1 = dots[i].y + oy,
            .x2 = dots[i].x + ox + 1,
            .y2 = dots[i].y + oy + 1,
        };
        lv_draw_rect(layer, &dot, &speck);
    }
}

static void draw_cover(lv_layer_t *layer, mick_ground_t ground)
{
    lv_area_t blue = {.x1 = 168, .y1 = 36, .x2 = 215, .y2 = 83};
    lv_area_t yellow = {.x1 = 180, .y1 = 92, .x2 = 203, .y2 = 115};
    lv_area_t red = {.x1 = 164, .y1 = 122, .x2 = 219, .y2 = 131};
    lv_area_t rule = {.x1 = 20, .y1 = 146, .x2 = 219, .y2 = 147};
    uint32_t rule_color = ground_is_dark(ground) ? 0xF6F1E8 : INK;
    draw_disc_or_bar(layer, &blue, lv_color_hex(BLUE), LV_OPA_COVER, true);
    draw_disc_or_bar(layer, &yellow, lv_color_hex(YELLOW), LV_OPA_COVER, true);
    draw_disc_or_bar(layer, &red, lv_color_hex(RED), LV_OPA_COVER, false);
    draw_disc_or_bar(layer, &rule, lv_color_hex(rule_color), LV_OPA_COVER, false);
}

static void on_draw(lv_event_t *event)
{
    lv_layer_t *layer = lv_event_get_layer(event);
    if (layer == NULL) return;
    if (s_paint.phase != MICK_PHASE_CANVAS) {
        draw_cover(layer, s_paint.ground);
        return;
    }
    if (s_paint.count == 0) return;
    for (uint8_t i = 0; i < s_paint.count; i++) {
        uint8_t kind = mick_blob_kind(&s_paint, i);
        uint8_t color = mick_blob_color(&s_paint, i);
        draw_shape(layer, &s_paint.blobs[i], kind,
                   lv_color_hex(paint_hex(color, s_paint.ground)),
                   LV_OPA_COVER, 0, 0, true, s_paint.grain);
    }
}

static void paint_label(lv_obj_t *label, uint32_t color)
{
    if (label != NULL) lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
}

static void paint_pad(lv_obj_t *label, uint32_t bg, uint32_t fg)
{
    if (label == NULL) return;
    lv_obj_set_style_bg_color(label, lv_color_hex(bg), 0);
    paint_label(label, fg);
}

static void refresh_chrome(const mick_field_t *field, int battery_soc)
{
    bool canvas = field->phase == MICK_PHASE_CANVAS;
    uint32_t bg = ground_hex(field->ground);
    uint32_t fg = ground_is_dark(field->ground) ? 0xF6F1E8 : INK;
    uint32_t muted = ground_is_dark(field->ground) ? 0xC8C4BC : MUTED;
    show(s_catalog, !canvas);
    show(s_form, canvas);
    if (canvas) lv_label_set_text(s_form, form_name(field->form));

    lv_obj_set_style_bg_color(s_screen, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_color(s_art, lv_color_hex(bg), 0);
    paint_pad(s_form, bg, fg);
    paint_pad(s_battery, bg, fg);
    paint_label(s_title, fg);
    paint_label(s_sub, muted);

    char battery[16];
    if (battery_soc < 0) {
        lv_label_set_text(s_battery, MICK_BATT_UNKNOWN);
    } else {
        unsigned shown = (unsigned)battery_soc;
        if (shown > 100u) shown = 100u;
        snprintf(battery, sizeof(battery), "%u%%", shown);
        lv_label_set_text(s_battery, battery);
    }

    lv_label_set_text(s_values[MICK_ROW_DENSITY], density_text(field->density));
    lv_label_set_text(s_values[MICK_ROW_CHROMA], chroma_name(field->chroma));
    lv_label_set_text(s_values[MICK_ROW_TEMPO], tempo_name(field->tempo));
    lv_label_set_text(s_values[MICK_ROW_GROUND], ground_name(field->ground));
    for (int row = 0; row < MICK_ROW_COUNT; row++) {
        bool selected = (int)field->row == row;
        uint32_t text = selected ? INK : fg;
        lv_obj_set_style_bg_color(s_cards[row], lv_color_hex(YELLOW), 0);
        lv_obj_set_style_bg_opa(s_cards[row], selected ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(s_keys[row], lv_color_hex(text), 0);
        lv_obj_set_style_text_color(s_values[row], lv_color_hex(text), 0);
        lv_label_set_text(s_keys[row], row_name((mick_row_t)row));
    }
}

void mick_ui_sync(const mick_field_t *field, int battery_soc)
{
    bool art = (field->dirty & MICK_DIRTY_ART) != 0;
    bool chrome = (field->dirty & MICK_DIRTY_CHROME) != 0 || battery_soc != s_shown_battery;
    s_paint = *field;
    if (art && s_art != NULL) lv_obj_invalidate(s_art);
    if (chrome) {
        s_shown_battery = battery_soc;
        refresh_chrome(field, battery_soc);
    }
}

static void make_row(lv_obj_t *parent, int index, int y)
{
    s_cards[index] = lv_obj_create(parent);
    flatten(s_cards[index]);
    lv_obj_set_pos(s_cards[index], 20, y);
    lv_obj_set_size(s_cards[index], 200, 30);
    lv_obj_set_style_bg_opa(s_cards[index], LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(s_cards[index], LV_OBJ_FLAG_CLICKABLE);

    s_keys[index] = make_label(s_cards[index], &mick_latin_16, INK);
    lv_obj_align(s_keys[index], LV_ALIGN_LEFT_MID, 12, 0);
    s_values[index] = make_label(s_cards[index], &mick_latin_16, INK);
    lv_obj_align(s_values[index], LV_ALIGN_RIGHT_MID, -12, 0);
}

void mick_ui_create(const mick_field_t *field, int battery_soc)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    s_screen = screen;
    flatten(screen);
    lv_obj_set_style_bg_color(screen, lv_color_hex(PAPER), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    s_art = lv_obj_create(screen);
    flatten(s_art);
    lv_obj_set_pos(s_art, 0, 0);
    lv_obj_set_size(s_art, MICK_SCREEN_W, MICK_SCREEN_H);
    lv_obj_set_style_bg_color(s_art, lv_color_hex(PAPER), 0);
    lv_obj_set_style_bg_opa(s_art, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s_art, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(s_art, on_draw, LV_EVENT_DRAW_POST, NULL);

    s_form = make_label(screen, &mick_latin_16, INK);
    lv_obj_set_pos(s_form, 12, 8);
    lv_obj_set_style_bg_opa(s_form, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_form, lv_color_hex(PAPER), 0);
    lv_obj_set_style_pad_hor(s_form, 4, 0);
    lv_obj_set_style_pad_ver(s_form, 1, 0);

    s_battery = make_label(screen, &mick_latin_16, INK);
    lv_obj_align(s_battery, LV_ALIGN_TOP_RIGHT, -12, 8);
    lv_obj_set_style_bg_opa(s_battery, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_battery, lv_color_hex(PAPER), 0);
    lv_obj_set_style_pad_hor(s_battery, 4, 0);
    lv_obj_set_style_pad_ver(s_battery, 1, 0);

    s_catalog = lv_obj_create(screen);
    flatten(s_catalog);
    lv_obj_set_pos(s_catalog, 0, 0);
    lv_obj_set_size(s_catalog, MICK_SCREEN_W, MICK_SCREEN_H);
    lv_obj_set_style_bg_opa(s_catalog, LV_OPA_TRANSP, 0);
    lv_obj_remove_flag(s_catalog, LV_OBJ_FLAG_CLICKABLE);

    s_title = make_label(s_catalog, &mick_latin_28, INK);
    lv_label_set_text(s_title, MICK_WORD_CYBER);
    lv_obj_set_pos(s_title, 20, 24);
    lv_obj_t *city = make_label(s_catalog, &mick_latin_16, BLUE);
    lv_label_set_text(city, MICK_WORD_CITY);
    lv_obj_set_pos(city, 20, 56);
    s_sub = make_label(s_catalog, &mick_latin_16, MUTED);
    lv_label_set_text(s_sub, MICK_SUBTITLE);
    lv_obj_set_pos(s_sub, 20, 80);

    make_row(s_catalog, MICK_ROW_DENSITY, 156);
    make_row(s_catalog, MICK_ROW_CHROMA, 188);
    make_row(s_catalog, MICK_ROW_TEMPO, 220);
    make_row(s_catalog, MICK_ROW_GROUND, 252);

    lv_obj_move_foreground(s_form);
    lv_obj_move_foreground(s_battery);

    s_shown_battery = -2;
    mick_ui_sync(field, battery_soc);
    lv_screen_load(screen);
}
