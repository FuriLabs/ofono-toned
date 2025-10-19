/**
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2025 Bardia Moshiri <bardia@furilabs.com>
 */

#include "ofono.h"
#include "pulse.h"

#define DEFAULT_CARD_NAME "droid_card.primary"

static void
ps_set_active_profile(PulseState *ps, const char *name)
{
    const char *newname = name ? name : "";
    if (g_strcmp0(ps->active_profile, newname) != 0) {
        g_free(ps->active_profile);
        ps->active_profile = g_strdup(newname);
        g_debug("Pulse: Active Profile is '%s'", newname);
    }
}

static gboolean
ps_is_target_card_name(PulseState *ps, const pa_card_info *info)
{
    return (ps->card_name && info && g_strcmp0(info->name, ps->card_name) == 0);
}

static gboolean
ps_is_target_card_index(PulseState *ps, const pa_card_info *info)
{
    return (info && ps->card_index >= 0 && (gint)info->index == ps->card_index);
}

static void
on_card_info_initial(pa_context *c,
                     const pa_card_info *info,
                     int eol,
                     void *userdata)
{
    PulseState *ps = userdata;
    if (eol != 0 || !info || !ps)
        return;

    if (!(ps_is_target_card_index(ps, info) || ps_is_target_card_name(ps, info)))
        return;

    ps->card_index = (gint)info->index;
    const char *ap = info->active_profile2 ? info->active_profile2->name : "";
    ps_set_active_profile(ps, ap);

    if (!ps->subscribed && ps->pa_ctx) {
        pa_context_set_subscribe_callback(ps->pa_ctx, NULL, NULL);
        pa_operation *op = pa_context_subscribe(ps->pa_ctx, PA_SUBSCRIPTION_MASK_CARD, NULL, NULL);
        if (op)
            pa_operation_unref(op);
        ps->subscribed = TRUE;
    }
}

static void
on_card_info_update(pa_context *c,
                    const pa_card_info *info,
                    int eol,
                    void *userdata)
{
    PulseState *ps = userdata;
    if (eol != 0 || !info || !ps)
        return;

    if (!ps_is_target_card_index(ps, info))
        return;

    const char *ap = info->active_profile2 ? info->active_profile2->name : "";
    ps_set_active_profile(ps, ap);
}

static void
pa_subscribe_cb(pa_context *c,
                pa_subscription_event_type_t t,
                uint32_t idx,
                void *userdata)
{
    PulseState *ps = userdata;
    if (!ps)
        return;

    pa_subscription_event_type_t fac  = t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK;
    pa_subscription_event_type_t kind = t & PA_SUBSCRIPTION_EVENT_TYPE_MASK;

    if (fac != PA_SUBSCRIPTION_EVENT_CARD)
        return;

    if (ps->card_index >= 0 && (gint)idx != ps->card_index)
        return;

    if (kind == PA_SUBSCRIPTION_EVENT_CHANGE || kind == PA_SUBSCRIPTION_EVENT_NEW) {
        if (ps->pa_ctx) {
            pa_operation *op = pa_context_get_card_info_by_index(ps->pa_ctx, idx, on_card_info_update, ps);
            if (op)
                pa_operation_unref(op);
        }
    }
}

static void
pa_ctx_state_cb(pa_context *c, void *userdata)
{
    PulseState *ps = userdata;
    if (!ps)
        return;

    switch (pa_context_get_state(c)) {
    case PA_CONTEXT_READY: {
        pa_operation *op = pa_context_get_card_info_list(ps->pa_ctx, on_card_info_initial, ps);
        if (op)
            pa_operation_unref(op);

        pa_context_set_subscribe_callback(ps->pa_ctx, pa_subscribe_cb, ps);
        pa_operation *sub = pa_context_subscribe(ps->pa_ctx, PA_SUBSCRIPTION_MASK_CARD, NULL, NULL);
        if (sub)
            pa_operation_unref(sub);
        ps->subscribed = TRUE;
        break;
    }
    case PA_CONTEXT_FAILED:
    case PA_CONTEXT_TERMINATED:
        g_debug("Pulse: context terminated (state=%d)", pa_context_get_state(c));
        break;
    default:
        break;
    }
}

gboolean
pulse_setup_listener(const gchar *card_name)
{
    if (!agent_data) {
        g_warning("Pulse: agent_data is NULL");
        return FALSE;
    }

    pulse_cleanup_listener();
    agent_data->pulse = g_new0(PulseState, 1);
    PulseState *ps = agent_data->pulse;

    ps->card_index  = -1;
    ps->card_name = g_strdup(card_name ? card_name : DEFAULT_CARD_NAME);
    ps->subscribed = FALSE;
    ps->active_profile = g_strdup("");

    ps->pa_glib_ml = pa_glib_mainloop_new(NULL);
    if (!ps->pa_glib_ml) {
        g_warning("Pulse: failed to create pa_glib_mainloop");
        pulse_cleanup_listener();
        return FALSE;
    }

    ps->pa_ctx = pa_context_new(pa_glib_mainloop_get_api(ps->pa_glib_ml),
                                "oFono Toned Profile Listener");
    if (!ps->pa_ctx) {
        g_warning("Pulse: failed to create pa_context");
        pulse_cleanup_listener();
        return FALSE;
    }

    pa_context_set_state_callback(ps->pa_ctx, pa_ctx_state_cb, ps);

    if (pa_context_connect(ps->pa_ctx, NULL, PA_CONTEXT_NOFLAGS, NULL) < 0) {
        g_warning("Pulse: pa_context_connect failed");
        pulse_cleanup_listener();
        return FALSE;
    }

    return TRUE;
}

void
pulse_cleanup_listener(void)
{
    if (!agent_data || !agent_data->pulse)
        return;

    PulseState *ps = agent_data->pulse;

    if (ps->pa_ctx) {
        pa_context_disconnect(ps->pa_ctx);
        pa_context_unref(ps->pa_ctx);
        ps->pa_ctx = NULL;
    }

    if (ps->pa_glib_ml) {
        pa_glib_mainloop_free(ps->pa_glib_ml);
        ps->pa_glib_ml = NULL;
    }

    g_clear_pointer(&ps->card_name, g_free);
    g_clear_pointer(&ps->active_profile, g_free);

    ps->card_index = -1;
    ps->subscribed = FALSE;

    g_free(ps);
    agent_data->pulse = NULL;
}

const gchar *
pulse_get_active_profile(void)
{
    if (!agent_data || !agent_data->pulse)
        return "";
    return agent_data->pulse->active_profile ? agent_data->pulse->active_profile : "";
}
