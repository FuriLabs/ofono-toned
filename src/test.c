/**
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2025 Bardia Moshiri <bardia@furilabs.com>
 */

#include "test.h"
#include "ofono.h"

TestData *test_data = NULL;

gboolean
test_start_tone(gpointer user_data)
{
    g_print("TEST: Starting tone sequence (simulating RingbackTone(true))\n");

    if (!agent_data->should_play_tone) {
        g_debug("Starting GSM dial tone pattern (TEST MODE)");
        agent_data->should_play_tone = TRUE;
        start_tone_cycle(NULL);
    }

    test_data->test_start_timer_id = 0;
    return G_SOURCE_REMOVE;
}

gboolean
test_stop_tone(gpointer user_data)
{
    g_print("TEST: Stopping tone sequence (simulating RingbackTone(false))\n");

    if (agent_data->should_play_tone) {
        g_debug("Stopping GSM dial tone pattern (TEST MODE)");
        agent_data->should_play_tone = FALSE;
        if (agent_data->tone_timer_id) {
            g_source_remove(agent_data->tone_timer_id);
            agent_data->tone_timer_id = 0;
        }
        stop_tone();
    }

    test_data->test_stop_timer_id = 0;

    if (test_data->test_mode)
        g_print("TEST: Test sequence completed. You can now exit with Ctrl+C\n");

    return G_SOURCE_REMOVE;
}

void
start_test_sequence(void)
{
    g_print("TEST: Starting test sequence in %d seconds...\n", TEST_START_DELAY / 1000);
    test_data->test_start_timer_id = g_timeout_add(TEST_START_DELAY, test_start_tone, NULL);
    test_data->test_stop_timer_id = g_timeout_add(TEST_START_DELAY + TEST_STOP_DELAY, test_stop_tone, NULL);
}

void
cleanup_test(void)
{
    if (test_data) {
        if (test_data->test_start_timer_id) {
            g_source_remove(test_data->test_start_timer_id);
            test_data->test_start_timer_id = 0;
        }
        if (test_data->test_stop_timer_id) {
            g_source_remove(test_data->test_stop_timer_id);
            test_data->test_stop_timer_id = 0;
        }

        g_free(test_data);
        test_data = NULL;
    }
}
