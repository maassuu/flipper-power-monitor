/**
 * @file pm_pope.h
 * Splash screen art, stored as ASCII rows and drawn pixel by pixel.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define PM_POPE_W (54u)
#define PM_POPE_H (64u)

extern const char* const pm_pope_rows[PM_POPE_H];

/** True if the pixel at (x, y) is black. Out of range returns false. */
bool pm_pope_pixel(uint8_t x, uint8_t y);
