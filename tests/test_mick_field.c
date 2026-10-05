#include "mick_field.h"

#include <assert.h>
#include <string.h>

static void expect_bounds(const mick_field_t *field)
{
    assert(field->count > 0);
    assert(field->count <= MICK_BLOB_MAX);
    for (uint8_t i = 0; i < field->count; i++) {
        assert(mick_blob_in_bounds(&field->blobs[i]));
    }
}

static void expect_specks(const mick_field_t *field)
{
    mick_speck_t specks[MICK_SPECK_MAX];
    for (uint8_t i = 0; i < field->count; i++) {
        uint8_t kind = mick_blob_kind(field, i);
        int count = mick_blob_specks(&field->blobs[i], kind, field->grain, specks, MICK_SPECK_MAX);
        assert(count > 0);
        for (int s = 0; s < count; s++) {
            assert(mick_point_inside(&field->blobs[i], kind, specks[s].x, specks[s].y));
        }
    }
}

static bool same_positions(const mick_field_t *field, const int16_t *xs, const int16_t *ys)
{
    for (uint8_t i = 0; i < field->count; i++) {
        if (field->blobs[i].x != xs[i] || field->blobs[i].y != ys[i]) return false;
    }
    return true;
}

static void snapshot(const mick_field_t *field, int16_t *xs, int16_t *ys)
{
    for (uint8_t i = 0; i < field->count; i++) {
        xs[i] = field->blobs[i].x;
        ys[i] = field->blobs[i].y;
    }
}

static bool arrived(const mick_field_t *field)
{
    for (uint8_t i = 0; i < field->count; i++) {
        if (field->blobs[i].x != field->blobs[i].tx) return false;
        if (field->blobs[i].y != field->blobs[i].ty) return false;
    }
    return true;
}

static void test_geometry(void)
{
    mick_blob_t blob = {.x = 0, .y = 0, .w = 100, .h = 100, .seed = 3};
    assert(mick_point_inside(&blob, MICK_FORM_DISC, 50, 50));
    assert(!mick_point_inside(&blob, MICK_FORM_DISC, 0, 0));
    assert(mick_point_inside(&blob, MICK_FORM_BAR, 0, 0));
    assert(mick_point_inside(&blob, MICK_FORM_BAR, 99, 99));
    assert(!mick_point_inside(&blob, MICK_FORM_BAR, 100, 50));
    assert(mick_point_inside(&blob, MICK_FORM_DIAMOND, 50, 50));
    assert(!mick_point_inside(&blob, MICK_FORM_DIAMOND, 0, 0));
    assert(mick_point_inside(&blob, MICK_FORM_TRI, 50, 0));
    assert(mick_point_inside(&blob, MICK_FORM_TRI, 50, 99));
    assert(!mick_point_inside(&blob, MICK_FORM_TRI, 0, 0));
    assert(!mick_point_inside(&blob, MICK_FORM_RING, 50, 50));
    assert(mick_point_inside(&blob, MICK_FORM_RING, 50, 2));
    assert(mick_point_inside(&blob, MICK_FORM_ARC, 50, 70));
    assert(!mick_point_inside(&blob, MICK_FORM_ARC, 50, 20));
    assert(mick_point_inside(&blob, MICK_FORM_CROSS, 50, 50));
    assert(!mick_point_inside(&blob, MICK_FORM_CROSS, 2, 2));
    assert(mick_point_inside(&blob, MICK_FORM_WEDGE, 4, 4));
    assert(!mick_point_inside(&blob, MICK_FORM_WEDGE, 90, 90));
    assert(mick_point_inside(&blob, MICK_FORM_LINE, 0, 0));
    assert(mick_point_inside(&blob, MICK_FORM_LINE, 99, 99));

    mick_speck_t specks[MICK_SPECK_MAX];
    for (uint8_t kind = MICK_FORM_DISC; kind < MICK_FORM_MIX; kind++) {
        int count = mick_blob_specks(&blob, kind, 0, specks, MICK_SPECK_MAX);
        assert(count >= 5);
        for (int i = 0; i < count; i++) {
            assert(mick_point_inside(&blob, kind, specks[i].x, specks[i].y));
        }
        int again = mick_blob_specks(&blob, kind, 1, specks, MICK_SPECK_MAX);
        assert(again >= 5);
    }
}

static void test_catalog_and_density(void)
{
    mick_field_t field;
    mick_field_init(&field, 1);
    assert(field.phase == MICK_PHASE_CATALOG);
    assert(field.density == MICK_DENSITY_2);
    assert(field.count == 5);
    assert(mick_density_count(MICK_DENSITY_1) == 2);
    assert(mick_density_count(MICK_DENSITY_2) == 5);
    assert(mick_density_count(MICK_DENSITY_3) == 9);
    assert(mick_density_count(MICK_DENSITY_4) == 14);
    assert(mick_density_count(MICK_DENSITY_5) == 19);
    expect_bounds(&field);
    expect_specks(&field);

    int16_t xs[MICK_BLOB_MAX];
    int16_t ys[MICK_BLOB_MAX];
    snapshot(&field, xs, ys);
    mick_field_tick(&field, 5000);
    assert(same_positions(&field, xs, ys));
    assert(field.jumps == 0);
    assert(field.grain == 0);

    assert(field.row == MICK_ROW_DENSITY);
    mick_field_on_input(&field, MICK_INPUT_UP);
    assert(field.row == MICK_ROW_GROUND);
    mick_field_on_input(&field, MICK_INPUT_DOWN);
    assert(field.row == MICK_ROW_DENSITY);

    mick_field_on_input(&field, MICK_INPUT_OK);
    assert(field.density == MICK_DENSITY_3);
    assert(field.count == 9);
    mick_field_on_input(&field, MICK_INPUT_OK);
    assert(field.density == MICK_DENSITY_4);
    assert(field.count == 14);
    mick_field_on_input(&field, MICK_INPUT_OK);
    assert(field.density == MICK_DENSITY_5);
    assert(field.count == 19);
    expect_bounds(&field);
    expect_specks(&field);
    mick_field_on_input(&field, MICK_INPUT_OK);
    assert(field.density == MICK_DENSITY_1);
    assert(field.count == 2);
    mick_field_on_input(&field, MICK_INPUT_OK);
    assert(field.density == MICK_DENSITY_2);
    assert(field.count == 5);

    assert(field.ground == MICK_GROUND_GRAY);
    mick_field_on_input(&field, MICK_INPUT_DOWN);
    mick_field_on_input(&field, MICK_INPUT_DOWN);
    mick_field_on_input(&field, MICK_INPUT_DOWN);
    assert(field.row == MICK_ROW_GROUND);
    mick_field_on_input(&field, MICK_INPUT_OK);
    assert(field.ground == MICK_GROUND_BLACK);
    mick_field_on_input(&field, MICK_INPUT_OK);
    assert(field.ground == MICK_GROUND_BEIGE);
    mick_field_on_input(&field, MICK_INPUT_OK);
    assert(field.ground == MICK_GROUND_CREAM);
    mick_field_on_input(&field, MICK_INPUT_OK);
    assert(field.ground == MICK_GROUND_WHEAT);
    mick_field_on_input(&field, MICK_INPUT_OK);
    assert(field.ground == MICK_GROUND_WHITE);
    mick_field_on_input(&field, MICK_INPUT_OK);
    assert(field.ground == MICK_GROUND_GRAY);
}

static void test_size_contrast(void)
{
    mick_field_t field;
    mick_field_init(&field, 1);
    int min_area = 1000000;
    int max_area = 0;
    bool huge = false;
    bool small = false;
    for (uint8_t i = 0; i < field.count; i++) {
        int area = (int)field.blobs[i].w * (int)field.blobs[i].h;
        if (area < min_area) min_area = area;
        if (area > max_area) max_area = area;
        if (field.blobs[i].w >= 100 || field.blobs[i].h >= 90) huge = true;
        if (field.blobs[i].w <= 36 && field.blobs[i].h <= 36) small = true;
    }
    assert(huge);
    assert(small);
    assert(max_area >= min_area * 4);
}

static void test_color_and_form(void)
{
    mick_field_t field;
    mick_field_init(&field, 7);
    mick_field_on_input(&field, MICK_INPUT_OK_LONG);
    assert(field.phase == MICK_PHASE_CANVAS);
    assert(field.form == MICK_FORM_MIX);

    bool mixed = false;
    uint8_t first_kind = mick_blob_kind(&field, 0);
    uint8_t first_color = mick_blob_color(&field, 0);
    bool multi = false;
    for (uint8_t i = 1; i < field.count; i++) {
        if (mick_blob_kind(&field, i) != first_kind) mixed = true;
        if (mick_blob_color(&field, i) != first_color) multi = true;
    }
    assert(mixed);
    assert(multi);

    mick_field_on_input(&field, MICK_INPUT_UP);
    assert(field.color_index == 1);
    assert(mick_blob_color(&field, 0) != first_color);

    mick_field_on_input(&field, MICK_INPUT_DOWN);
    assert(field.form == MICK_FORM_DISC);
    for (uint8_t i = 0; i < field.count; i++) {
        assert(mick_blob_kind(&field, i) == MICK_FORM_DISC);
    }
    mick_field_on_input(&field, MICK_INPUT_DOWN);
    assert(field.form == MICK_FORM_TRI);

    mick_tempo_t tempo = field.tempo;
    mick_form_t form = field.form;
    uint8_t color = field.color_index;
    mick_field_on_input(&field, MICK_INPUT_OK);
    assert(field.tempo == tempo);
    assert(field.form == form);
    assert(field.color_index == color);

    mick_field_on_input(&field, MICK_INPUT_OK_LONG);
    assert(field.phase == MICK_PHASE_CATALOG);
    mick_field_on_input(&field, MICK_INPUT_DOWN);
    assert(field.row == MICK_ROW_CHROMA);
    mick_field_on_input(&field, MICK_INPUT_OK);
    assert(field.chroma == MICK_CHROMA_MONO);
    mick_field_on_input(&field, MICK_INPUT_OK_LONG);
    for (uint8_t i = 0; i < field.count; i++) {
        assert(mick_blob_color(&field, i) == field.color_index);
    }
    mick_field_on_input(&field, MICK_INPUT_UP);
    for (uint8_t i = 0; i < field.count; i++) {
        assert(mick_blob_color(&field, i) == field.color_index);
    }
}

static void test_step_and_drift(void)
{
    mick_field_t field;
    int16_t xs[MICK_BLOB_MAX];
    int16_t ys[MICK_BLOB_MAX];

    mick_field_init(&field, 1);
    mick_field_on_input(&field, MICK_INPUT_OK_LONG);
    assert(field.tempo == MICK_TEMPO_STEP_1S);
    assert(mick_step_period_ms(field.tempo) == 1000);
    snapshot(&field, xs, ys);
    mick_field_tick(&field, 699);
    assert(field.grain == 0);
    assert(same_positions(&field, xs, ys));
    mick_field_tick(&field, 1);
    assert(field.grain == 1);
    assert(field.jumps == 0);
    mick_field_tick(&field, 299);
    assert(field.jumps == 0);
    assert(same_positions(&field, xs, ys));
    mick_field_tick(&field, 1);
    assert(field.jumps == 1);

    for (int i = 0; i < 24; i++) mick_field_tick(&field, 30);
    assert(field.jumps == 1);
    assert(arrived(&field));
    assert(!same_positions(&field, xs, ys));
    expect_bounds(&field);
    expect_specks(&field);

    snapshot(&field, xs, ys);
    assert(field.since_ms < 1000);
    mick_field_tick(&field, 1000 - field.since_ms - 1);
    assert(field.jumps == 1);
    assert(same_positions(&field, xs, ys));
    mick_field_tick(&field, 1);
    assert(field.jumps == 2);

    mick_field_init(&field, 3);
    mick_field_on_input(&field, MICK_INPUT_DOWN);
    mick_field_on_input(&field, MICK_INPUT_DOWN);
    assert(field.row == MICK_ROW_TEMPO);
    mick_field_on_input(&field, MICK_INPUT_OK);
    assert(field.tempo == MICK_TEMPO_STEP_2S);
    mick_field_on_input(&field, MICK_INPUT_OK_LONG);
    mick_field_tick(&field, 1999);
    assert(field.jumps == 0);
    mick_field_tick(&field, 1);
    assert(field.jumps == 1);

    mick_field_init(&field, 11);
    mick_field_on_input(&field, MICK_INPUT_DOWN);
    mick_field_on_input(&field, MICK_INPUT_DOWN);
    mick_field_on_input(&field, MICK_INPUT_OK);
    mick_field_on_input(&field, MICK_INPUT_OK);
    assert(field.tempo == MICK_TEMPO_STEP_5S);
    assert(mick_step_period_ms(field.tempo) == 5000);
    mick_field_on_input(&field, MICK_INPUT_OK);
    assert(field.tempo == MICK_TEMPO_STEP_10S);
    assert(mick_step_period_ms(field.tempo) == 10000);
    mick_field_on_input(&field, MICK_INPUT_OK);
    assert(field.tempo == MICK_TEMPO_DRIFT);
    mick_field_on_input(&field, MICK_INPUT_OK_LONG);
    for (uint8_t i = 0; i < field.count; i++) {
        assert(field.blobs[i].vx != 0);
        assert(field.blobs[i].vy != 0);
    }
    snapshot(&field, xs, ys);
    mick_field_tick(&field, 400);
    assert(!same_positions(&field, xs, ys));
    for (int i = 0; i < 400; i++) {
        mick_field_tick(&field, 50);
        expect_bounds(&field);
    }
    expect_specks(&field);
}

static void test_impact(void)
{
    mick_field_t field;
    mick_field_init(&field, 5);
    mick_field_on_input(&field, MICK_INPUT_DOWN);
    mick_field_on_input(&field, MICK_INPUT_DOWN);
    mick_field_on_input(&field, MICK_INPUT_OK);
    mick_field_on_input(&field, MICK_INPUT_OK);
    mick_field_on_input(&field, MICK_INPUT_OK);
    mick_field_on_input(&field, MICK_INPUT_OK);
    mick_field_on_input(&field, MICK_INPUT_OK);
    assert(field.tempo == MICK_TEMPO_IMPACT);
    assert(mick_step_period_ms(field.tempo) == 0);
    mick_field_on_input(&field, MICK_INPUT_OK_LONG);

    for (uint8_t i = 0; i < field.count; i++) {
        assert(field.blobs[i].vx >= 48 || field.blobs[i].vx <= -48);
        assert(field.blobs[i].vy >= 48 || field.blobs[i].vy <= -48);
    }

    mick_blob_t *blob = &field.blobs[0];
    blob->w = 20;
    blob->h = 20;
    blob->x = 40;
    blob->y = 40;
    blob->vx = 80;
    blob->vy = -40;
    blob->rem_x = 0;
    blob->rem_y = 0;
    mick_field_tick(&field, 100);
    assert(blob->x == 48);
    assert(blob->y == 36);
    assert(blob->vx == 80);
    assert(blob->vy == -40);

    int max_x = MICK_ART_X + MICK_ART_W - blob->w;
    blob->x = (int16_t)max_x;
    blob->y = 80;
    blob->vx = 80;
    blob->vy = 40;
    blob->rem_x = 0;
    blob->rem_y = 0;
    mick_field_tick(&field, 50);
    assert(blob->x == max_x);
    assert(blob->vx < 0);
    assert(blob->vy == 40);
    expect_bounds(&field);

    blob->x = 40;
    blob->y = MICK_ART_Y;
    blob->vx = 40;
    blob->vy = -80;
    blob->rem_x = 0;
    blob->rem_y = 0;
    mick_field_tick(&field, 50);
    assert(blob->y == MICK_ART_Y);
    assert(blob->vy > 0);
    assert(blob->vx == 40);

    for (int i = 0; i < 80; i++) {
        mick_field_tick(&field, 40);
        expect_bounds(&field);
    }
}

int main(void)
{
    test_geometry();
    test_catalog_and_density();
    test_size_contrast();
    test_color_and_form();
    test_step_and_drift();
    test_impact();
    return 0;
}
