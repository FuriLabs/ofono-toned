/**
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (C) 2025 Bardia Moshiri <bardia@furilabs.com>
 */

#include "ofono.h"
#include "test.h"

static GMainLoop *main_loop = NULL;

static void
cleanup(void)
{
    pulse_cleanup_listener();
    cleanup_ofono();
    cleanup_test();
}

static void
signal_handler(int sig)
{
    g_print("Received signal %d, cleaning up...\n", sig);
    cleanup();
    if (main_loop)
        g_main_loop_quit(main_loop);
}

static void
print_usage(const char *prog_name)
{
    g_print("Usage: %s [--test]\n", prog_name);
    g_print("  --test: Run in test mode (automatically start/stop tone)\n");
}

int
main(int argc, char *argv[])
{
    gboolean test_mode = FALSE;

    for (int i = 1; i < argc; i++) {
        if (g_strcmp0(argv[i], "--test") == 0) {
            test_mode = TRUE;
        } else if (g_strcmp0(argv[i], "--help") == 0 || g_strcmp0(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            g_print("Unknown argument: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    agent_data = g_new0(AgentData, 1);
    test_data = g_new0(TestData, 1);
    test_data->test_mode = test_mode;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if (test_mode) {
        g_print("Starting in TEST MODE\n");
        g_print("Will start tone after %d seconds and stop after %d seconds\n",
                TEST_START_DELAY / 1000, TEST_STOP_DELAY / 1000);
    }

    g_autoptr(GError) system_error = NULL;
    agent_data->system_bus = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, &system_error);
    if (system_error) {
        g_warning("Failed to connect to system bus: %s", system_error->message);
        cleanup();
        return 1;
    }

    g_autoptr(GError) session_error = NULL;
    agent_data->session_bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &session_error);

    if (!pulse_setup_listener(NULL)) {
        g_warning("Failed to set up Pulse listener");
    }

    if (!session_error) {
        g_autoptr(GDBusNodeInfo) introspection_data = g_dbus_node_info_new_for_xml(introspection_xml, NULL);

        static const GDBusInterfaceVTable interface_vtable = {
            method_call,
            NULL,
            NULL
        };

        g_autoptr(GError) register_error = NULL;
        agent_data->agent_id = g_dbus_connection_register_object(
            agent_data->system_bus,
            AGENT_PATH,
            introspection_data->interfaces[0],
            &interface_vtable,
            NULL,
            NULL,
            &register_error
        );

        if (register_error) {
            g_warning("Failed to register object: %s", register_error->message);
            cleanup();
            return 1;
        }

        g_debug("Object registered at path: %s", AGENT_PATH);

        setup_ofono_monitoring();

        if (test_mode)
            start_test_sequence();
    } else {
        g_warning("Failed to connect to session bus: %s", session_error->message);
    }

    main_loop = g_main_loop_new(NULL, FALSE);
    g_debug("Starting main loop...");
    g_main_loop_run(main_loop);

    cleanup();
    g_main_loop_unref(main_loop);
    return 0;
}
