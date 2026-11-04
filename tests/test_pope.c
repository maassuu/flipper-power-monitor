#include "test_framework.h"
#include "../pm_pope.h"

static void test_rows_shape(void) {
    int bad_len = 0, bad_char = 0, black = 0;
    for(uint8_t y = 0; y < PM_POPE_H; y++) {
        const char* r = pm_pope_rows[y];
        if(strlen(r) != PM_POPE_W) bad_len++;
        for(const char* p = r; *p; p++) {
            if(*p != '#' && *p != '.') bad_char++;
            if(*p == '#') black++;
        }
    }
    CHECK(bad_len == 0);
    CHECK(bad_char == 0);
    CHECK(black > 100); /* not empty */
    /* fits on the 128x64 display next to the title block */
    CHECK(PM_POPE_H <= 64u);
    CHECK(PM_POPE_W <= 58u);
}

static void test_pixel_bounds(void) {
    CHECK(!pm_pope_pixel(PM_POPE_W, 0));
    CHECK(!pm_pope_pixel(0, PM_POPE_H));
    CHECK(!pm_pope_pixel(255, 255));
    /* pixel accessor agrees with the ASCII rows */
    int mismatch = 0;
    for(uint8_t y = 0; y < PM_POPE_H; y++) {
        for(uint8_t x = 0; x < PM_POPE_W; x++) {
            if(pm_pope_pixel(x, y) != (pm_pope_rows[y][x] == '#')) mismatch++;
        }
    }
    CHECK(mismatch == 0);
}

void test_pope_suite(void) {
    printf("[pope]\n");
    RUN(test_rows_shape);
    RUN(test_pixel_bounds);
}
