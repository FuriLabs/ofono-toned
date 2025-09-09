/**
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2025 Bardia Moshiri <bardia@furilabs.com>
 */

#ifndef TEST_H
#define TEST_H

#include <gio/gio.h>

#define TEST_START_DELAY 1500   /* 1.5 seconds after startup */
#define TEST_STOP_DELAY 30000   /* 30 seconds after start */

typedef struct {
    guint test_start_timer_id;
    guint test_stop_timer_id;
    gboolean test_mode;
} TestData;

extern TestData *test_data;

/**
 * Start tone in test mode.
 * Simulates RingbackTone(true) call for testing.
 *
 * @param user_data  User data pointer (unused).
 * @return           G_SOURCE_REMOVE to stop the timer.
 */
gboolean test_start_tone(gpointer user_data);

/**
 * Stop tone in test mode.
 * Simulates RingbackTone(false) call for testing.
 *
 * @param user_data  User data pointer (unused).
 * @return           G_SOURCE_REMOVE to stop the timer.
 */
gboolean test_stop_tone(gpointer user_data);

/**
 * Start the automated test sequence.
 * Schedules tone start and stop events for testing.
 */
void start_test_sequence(void);

/**
 * Clean up test-related resources.
 * Removes timers and frees test data memory.
 */
void cleanup_test(void);

#endif /* TEST_H */
