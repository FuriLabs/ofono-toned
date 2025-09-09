/**
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2025 Bardia Moshiri <bardia@furilabs.com>
 */

#include "ofono.h"

AgentData *agent_data = NULL;

const gchar introspection_xml[] =
  "<node>"
  "  <interface name='org.ofono.VoiceCallAgent'>"
  "    <method name='Release'>"
  "    </method>"
  "    <method name='RingbackTone'>"
  "      <arg type='b' name='playTone' direction='in'/>"
  "    </method>"
  "  </interface>"
  "</node>";

static const GDBusInterfaceVTable interface_vtable = {
    method_call,
    NULL,
    NULL
};

void
start_event_tone(guint key)
{
    if (!agent_data->session_bus) {
        g_debug("Session bus not available");
        return;
    }

    g_autoptr(GError) error = NULL;
    g_autoptr(GVariant) result = g_dbus_connection_call_sync(
        agent_data->session_bus,
        NOKIA_TONES_SERVICE,
        NOKIA_TONES_PATH,
        NOKIA_TONES_INTERFACE,
        "StartEventTone",
        g_variant_new("(uiu)", key, 0, 0),
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error
    );

    if (error)
        g_warning("Failed to start tone: %s", error->message);
    else
        g_debug("Started tone with key: %u", key);
}

void
stop_tone(void)
{
    if (!agent_data->session_bus)
        return;

    g_autoptr(GError) error = NULL;
    g_autoptr(GVariant) result = g_dbus_connection_call_sync(
        agent_data->session_bus,
        NOKIA_TONES_SERVICE,
        NOKIA_TONES_PATH,
        NOKIA_TONES_INTERFACE,
        "StopTone",
        NULL,
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error
    );

    if (error)
        g_warning("Failed to stop tone: %s", error->message);
    else
        g_debug("Stopped tone");
}

gboolean
stop_tone_and_schedule_next(gpointer user_data)
{
    g_debug("Stopping tone and scheduling next");
    stop_tone();
    agent_data->playing_tone = FALSE;

    agent_data->tone_timer_id = 0;

    if (agent_data->should_play_tone)
        agent_data->tone_timer_id = g_timeout_add(TONE_PAUSE, start_tone_cycle, NULL);

    return G_SOURCE_REMOVE;
}

gboolean
start_tone_cycle(gpointer user_data)
{
    if (!agent_data->should_play_tone)
        return G_SOURCE_REMOVE;

    g_debug("Starting tone cycle");
    start_event_tone(DIALING_TONE_KEY);
    agent_data->playing_tone = TRUE;

    agent_data->tone_timer_id = g_timeout_add(TONE_DURATION, stop_tone_and_schedule_next, NULL);

    return G_SOURCE_REMOVE;
}

static void
cleanup_agent_state(void)
{
    if (agent_data->tone_timer_id) {
        g_source_remove(agent_data->tone_timer_id);
        agent_data->tone_timer_id = 0;
    }

    if (agent_data->retry_timer_id) {
        g_source_remove(agent_data->retry_timer_id);
        agent_data->retry_timer_id = 0;
    }

    agent_data->should_play_tone = FALSE;
    stop_tone();

    if (agent_data->vcm_proxy) {
        g_object_unref(agent_data->vcm_proxy);
        agent_data->vcm_proxy = NULL;
    }

    agent_data->ofono_available = FALSE;
}

void
method_call(GDBusConnection *connection,
            const gchar *sender,
            const gchar *object_path,
            const gchar *interface_name,
            const gchar *method_name,
            GVariant *parameters,
            GDBusMethodInvocation *invocation,
            gpointer user_data)
{
    if (g_strcmp0(method_name, "Release") == 0) {
        g_debug("Agent got Release");
        g_dbus_method_invocation_return_value(invocation, NULL);
        cleanup_agent_state();
        g_debug("Agent released, waiting for oFono to reappear...");
    } else if (g_strcmp0(method_name, "RingbackTone") == 0) {
        gboolean play_tone;
        g_variant_get(parameters, "(b)", &play_tone);
        g_debug("Agent got playTone notification: %d", play_tone);

        if (play_tone && !agent_data->should_play_tone) {
            g_debug("Starting GSM dial tone pattern");
            agent_data->should_play_tone = TRUE;
            start_tone_cycle(NULL);
        } else if (!play_tone && agent_data->should_play_tone) {
            g_debug("Stopping GSM dial tone pattern");
            agent_data->should_play_tone = FALSE;
            if (agent_data->tone_timer_id) {
                g_source_remove(agent_data->tone_timer_id);
                agent_data->tone_timer_id = 0;
            }
            stop_tone();
        }

        g_dbus_method_invocation_return_value(invocation, NULL);
    }
}

static gboolean
register_agent(void)
{
    if (!agent_data->ofono_available) {
        g_debug("oFono not available, skipping agent registration");
        return FALSE;
    }

    g_autoptr(GError) error = NULL;
    g_autoptr(GVariant) result = g_dbus_connection_call_sync(
        agent_data->system_bus,
        OFONO_SERVICE,
        OFONO_MANAGER_PATH,
        OFONO_MANAGER_INTERFACE,
        "GetModems",
        NULL,
        NULL,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        NULL,
        &error
    );

    if (error) {
        g_warning("Failed to get modems: %s", error->message);
        return FALSE;
    }

    g_autoptr(GVariantIter) iter = NULL;
    g_variant_get(result, "(a(oa{sv}))", &iter);

    g_autofree gchar *modem_path = NULL;
    g_autoptr(GVariant) properties = NULL;

    while (g_variant_iter_next(iter, "(o@a{sv})", &modem_path, &properties)) {
        g_autoptr(GVariantIter) prop_iter = NULL;
        g_variant_get(properties, "a{sv}", &prop_iter);

        gboolean has_vcm = FALSE;
        g_autofree gchar *prop_key = NULL;
        g_autoptr(GVariant) prop_value = NULL;

        while (g_variant_iter_next(prop_iter, "{sv}", &prop_key, &prop_value)) {
            if (g_strcmp0(prop_key, "Interfaces") == 0) {
                g_autoptr(GVariantIter) iface_iter = NULL;
                g_variant_get(prop_value, "as", &iface_iter);

                g_autofree gchar *iface = NULL;
                while (g_variant_iter_next(iface_iter, "s", &iface)) {
                    if (g_strcmp0(iface, OFONO_VCM_INTERFACE) == 0) {
                        has_vcm = TRUE;
                        break;
                    }
                    g_clear_pointer(&iface, g_free);
                }
            }

            g_clear_pointer(&prop_key, g_free);
            g_clear_pointer(&prop_value, g_variant_unref);

            if (has_vcm)
                break;
        }

        if (has_vcm) {
            g_debug("Found modem with VoiceCallManager: %s", modem_path);

            g_autoptr(GError) proxy_error = NULL;
            agent_data->vcm_proxy = g_dbus_proxy_new_sync(
                agent_data->system_bus,
                G_DBUS_PROXY_FLAGS_NONE,
                NULL,
                OFONO_SERVICE,
                modem_path,
                OFONO_VCM_INTERFACE,
                NULL,
                &proxy_error
            );

            if (proxy_error) {
                g_warning("Failed to create VCM proxy: %s", proxy_error->message);
                g_clear_pointer(&modem_path, g_free);
                g_clear_pointer(&properties, g_variant_unref);
                continue;
            }

            g_autoptr(GError) reg_error = NULL;
            g_autoptr(GVariant) reg_result = g_dbus_proxy_call_sync(
                agent_data->vcm_proxy,
                "RegisterVoicecallAgent",
                g_variant_new("(o)", AGENT_PATH),
                G_DBUS_CALL_FLAGS_NONE,
                -1,
                NULL,
                &reg_error
            );

            if (reg_error) {
                g_warning("Failed to register agent: %s", reg_error->message);
                g_object_unref(agent_data->vcm_proxy);
                agent_data->vcm_proxy = NULL;
            } else {
                g_debug("Agent registered successfully");
                g_clear_pointer(&modem_path, g_free);
                g_clear_pointer(&properties, g_variant_unref);
                return TRUE;
            }
        }

        g_clear_pointer(&modem_path, g_free);
        g_clear_pointer(&properties, g_variant_unref);
    }

    return FALSE;
}

static gboolean
retry_register_agent(gpointer user_data)
{
    if (!agent_data->ofono_available) {
        g_debug("oFono disappeared, stopping retry attempts");
        agent_data->retry_timer_id = 0;
        return G_SOURCE_REMOVE;
    }

    g_debug("Retrying agent registration...");
    if (register_agent()) {
        g_debug("Agent registration successful on retry");
        agent_data->retry_timer_id = 0;
        return G_SOURCE_REMOVE;
    }

    g_debug("Agent registration failed, will retry in 5 seconds");
    return G_SOURCE_CONTINUE;
}

static void
on_ofono_name_appeared(GDBusConnection *connection,
                       const gchar *name,
                       const gchar *name_owner,
                       gpointer user_data)
{
    g_debug("oFono appeared on D-Bus");
    agent_data->ofono_available = TRUE;

    if (register_agent()) {
        g_debug("Agent registered successfully");
    } else {
        g_debug("Agent registration failed, starting retry timer");
        agent_data->retry_timer_id = g_timeout_add_seconds(5, retry_register_agent, NULL);
    }
}

static void
on_ofono_name_vanished(GDBusConnection *connection,
                       const gchar *name,
                       gpointer user_data)
{
    g_debug("oFono disappeared from D-Bus");
    cleanup_agent_state();
}

void
setup_ofono_monitoring(void)
{
    agent_data->ofono_watch_id = g_bus_watch_name(
        G_BUS_TYPE_SYSTEM,
        OFONO_SERVICE,
        G_BUS_NAME_WATCHER_FLAGS_NONE,
        on_ofono_name_appeared,
        on_ofono_name_vanished,
        NULL,
        NULL
    );
}

void
cleanup_ofono(void)
{
    if (agent_data) {
        if (agent_data->ofono_watch_id) {
            g_bus_unwatch_name(agent_data->ofono_watch_id);
            agent_data->ofono_watch_id = 0;
        }

        cleanup_agent_state();

        if (agent_data->vcm_proxy && agent_data->ofono_available) {
            g_autoptr(GError) error = NULL;
            g_autoptr(GVariant) unreg_result = g_dbus_proxy_call_sync(
                agent_data->vcm_proxy,
                "UnregisterVoicecallAgent",
                g_variant_new("(o)", AGENT_PATH),
                G_DBUS_CALL_FLAGS_NONE,
                -1,
                NULL,
                &error
            );

            if (error)
                g_warning("Failed to unregister agent: %s", error->message);
            else
                g_debug("Agent unregistered");
        }

        if (agent_data->vcm_proxy) {
            g_object_unref(agent_data->vcm_proxy);
            agent_data->vcm_proxy = NULL;
        }

        if (agent_data->system_bus) {
            if (agent_data->agent_id)
                g_dbus_connection_unregister_object(agent_data->system_bus, agent_data->agent_id);
            g_object_unref(agent_data->system_bus);
        }

        if (agent_data->session_bus)
            g_object_unref(agent_data->session_bus);

        g_free(agent_data);
        agent_data = NULL;
    }
}
