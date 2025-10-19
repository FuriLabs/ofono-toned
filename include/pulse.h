/**
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2025 Bardia Moshiri <bardia@furilabs.com>
 */

#ifndef PULSE_H
#define PULSE_H

#include <pulse/pulseaudio.h>
#include <pulse/glib-mainloop.h>

typedef struct {
    pa_glib_mainloop *pa_glib_ml;
    pa_context *pa_ctx;
    gint card_index;
    gchar *card_name;
    gboolean subscribed;
    gchar *active_profile;
} PulseState;

/**
 * Initialize and start a PulseAudio card profile listener.
 *
 * Allocates agent_data->pulse (PulseState), connects a PA context with a
 * GLib mainloop integration, subscribes to CARD changes, and keeps the
 * current active profile in both pulse->active_profile and
 * pulse->pa_active_profile.
 *
 * @param card_name  PulseAudio card name (e.g., "droid_card.primary").
 *                   If NULL, defaults to "droid_card.primary".
 * @return TRUE on successful initialization, FALSE on failure.
 */
gboolean pulse_setup_listener(const gchar *card_name);

/**
 * Stop the PulseAudio listener and release resources.
 *
 * Fully frees agent_data->pulse and sets it to NULL.
 */
void pulse_cleanup_listener(void);

/**
 * Get the last known active profile name (empty string if unknown).
 * Returns a borrowed pointer owned by agent_data->pulse; do not free.
 */
const gchar *pulse_get_active_profile(void);

#endif /* PULSE_H */
