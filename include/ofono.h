/**
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2025 Bardia Moshiri <bardia@furilabs.com>
 */

#ifndef OFONO_H
#define OFONO_H

#include <gio/gio.h>

#define OFONO_SERVICE "org.ofono"
#define OFONO_MANAGER_PATH "/"
#define OFONO_MANAGER_INTERFACE "org.ofono.Manager"
#define OFONO_VCM_INTERFACE "org.ofono.VoiceCallManager"
#define AGENT_PATH "/ofono/toned"
#define AGENT_INTERFACE "org.ofono.VoiceCallAgent"

#define NOKIA_TONES_SERVICE "com.Nokia.Telephony.Tones"
#define NOKIA_TONES_PATH "/com/Nokia/Telephony/Tones"
#define NOKIA_TONES_INTERFACE "com.Nokia.Telephony.Tones"

#define DIALING_TONE_KEY 66
#define TONE_DURATION 1500  /* 1.5 seconds */
#define TONE_PAUSE 1000     /* 1 second pause between tones */

typedef struct {
    GDBusConnection *system_bus;
    GDBusConnection *session_bus;
    GDBusProxy *vcm_proxy;
    guint agent_id;
    guint tone_timer_id;
    guint retry_timer_id;
    guint ofono_watch_id;
    gboolean playing_tone;
    gboolean should_play_tone;
    gboolean ofono_available;
} AgentData;

extern AgentData *agent_data;

extern const gchar introspection_xml[];

/**
 * Start playing an event tone with the specified key.
 *
 * @param key  Tone key identifier to play.
 */
void start_event_tone(guint key);

/**
 * Stop the currently playing tone.
 */
void stop_tone(void);

/**
 * Start a tone cycle.
 *
 * @param user_data  User data pointer (unused).
 * @return           G_SOURCE_REMOVE to stop the timer.
 */
gboolean start_tone_cycle(gpointer user_data);

/**
 * Stop current tone and schedule the next one in the cycle.
 *
 * @param user_data  User data pointer (unused).
 * @return           G_SOURCE_REMOVE to stop the timer.
 */
gboolean stop_tone_and_schedule_next(gpointer user_data);

/**
 * Handle D-Bus method calls for the voice call agent.
 *
 * @param connection      D-Bus connection.
 * @param sender          Method call sender.
 * @param object_path     Object path.
 * @param interface_name  Interface name.
 * @param method_name     Method name being called.
 * @param parameters      Method parameters.
 * @param invocation      Method invocation context.
 * @param user_data       User data pointer (unused).
 */
void method_call(GDBusConnection *connection,
                 const gchar *sender,
                 const gchar *object_path,
                 const gchar *interface_name,
                 const gchar *method_name,
                 GVariant *parameters,
                 GDBusMethodInvocation *invocation,
                 gpointer user_data);

/**
 * Setup oFono service monitoring.
 * Watches for oFono service appearing and disappearing on D-Bus.
 */
void setup_ofono_monitoring(void);

/**
 * Clean up oFono-related resources.
 * Unregisters agent, closes D-Bus connections, and frees memory.
 */
void cleanup_ofono(void);

#endif /* OFONO_H */
