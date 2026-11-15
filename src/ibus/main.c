/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <ibus.h>
#include <locale.h>
#include <stdlib.h>

void chewing_plus_ibus_register(IBusBus *bus);

static void disconnected(IBusBus *bus, gpointer user) {
    (void)bus;
    (void)user;
    ibus_quit();
}

int main(int argc, char **argv) {
    IBusBus *bus;
    setlocale(LC_ALL, "");
    ibus_init();
    bus = ibus_bus_new();
    if (!ibus_bus_is_connected(bus)) {
        g_printerr("ibus-chewing-plus: cannot connect to ibus-daemon\n");
        return 1;
    }
    g_signal_connect(bus, "disconnected", G_CALLBACK(disconnected), NULL);
    chewing_plus_ibus_register(bus);
    ibus_bus_request_name(bus, "org.freedesktop.IBus.ChewingPlus", 0);
    ibus_main();
    g_object_unref(bus);
    (void)argc;
    (void)argv;
    return 0;
}
