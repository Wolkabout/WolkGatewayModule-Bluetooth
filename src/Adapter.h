#ifndef ADAPTER_H
#define ADAPTER_H

#include "utils.h"

#include <gio/gio.h>
#include <glib.h>
#include <iostream>

namespace wolkabout
{
static GDBusConnection* s_connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
static GMainLoop* s_loop = g_main_loop_new(NULL, FALSE);

class Adapter
{
public:
    Adapter();

    static int call_method(const char* method, GVariant* param);

    static int set_property(const char* prop, GVariant* value);

    int power_on();

    int remove_device(const char* device);

    int start_scan();

    int stop_scan();

    int subscribe_adapter_changed();

    int subscribe_device_added(void (*f)(GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar*,
                                         GVariant*, gpointer));
    int subscribe_device_removed(void (*f)(GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar*,
                                           GVariant*, gpointer));

    void run_loop();

    bool scanning();

private:
    bool is_scanning;

    static void signal_changed(GDBusConnection* conn, const gchar* sender, const gchar* path, const gchar* interface,
                               const gchar* signal, GVariant* params, void* userdata);
};

}    // namespace wolkabout
#endif