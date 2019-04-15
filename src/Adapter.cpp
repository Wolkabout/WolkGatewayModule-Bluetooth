#include "Adapter.h"

namespace wolkabout
{
Adapter::Adapter()
{
    is_scanning = FALSE;
}

int Adapter::call_method(const char* method, GVariant* param)
{
    GVariant* result;
    GError* error = NULL;

    result = g_dbus_connection_call_sync(s_connection, "org.bluez", "/org/bluez/hci0", "org.bluez.Adapter1", method,
                                         param, NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
    if (error != NULL)
        return 1;

    g_variant_unref(result);
    return 0;
}

int Adapter::set_property(const char* prop, GVariant* value)
{
    GVariant* result;
    GError* error = NULL;

    result = g_dbus_connection_call_sync(
      s_connection, "org.bluez", "/org/bluez/hci0", "org.freedesktop.DBus.Properties", "Set",
      g_variant_new("(ssv)", "org.bluez.Adapter1", prop, value), NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
    if (error != NULL)
        return 1;

    g_variant_unref(result);
    return 0;
}

void Adapter::signal_changed(GDBusConnection* conn, const gchar* sender, const gchar* path, const gchar* interface,
                             const gchar* signal, GVariant* params, void* userdata)
{
    (void)conn;
    (void)sender;
    (void)path;
    (void)interface;
    (void)userdata;

    GVariantIter* properties = NULL;
    GVariantIter* unknown = NULL;
    const char* iface;
    const char* key;
    GVariant* value = NULL;
    const gchar* signature = g_variant_get_type_string(params);

    if (g_strcmp0(signature, "(sa{sv}as)") != 0)
    {
        std::cout << "Invalid signature for " << signal << ":" << signature << "!= (sa{sv}as)\n";
        free_properties(properties, value);
        return;
    }

    g_variant_get(params, "(&sa{sv}as)", &iface, &properties, &unknown);
    while (g_variant_iter_next(properties, "{&sv}", &key, &value))
    {
        if (!g_strcmp0(key, "Powered"))
        {
            if (!g_variant_is_of_type(value, G_VARIANT_TYPE_BOOLEAN))
            {
                std::cout << "Invalid argument type for " << key << ":" << g_variant_get_type_string(value) << "!= b\n";
                free_properties(properties, value);
                return;
            }
            std::cout << "Adapter is Powered " << (g_variant_get_boolean(value) ? "on" : "off") << "\n";
        }
        if (!g_strcmp0(key, "Discovering"))
        {
            if (!g_variant_is_of_type(value, G_VARIANT_TYPE_BOOLEAN))
            {
                std::cout << "Invalid argument type for " << key << ":" << g_variant_get_type_string(value) << "!= b\n";
                free_properties(properties, value);
                return;
            }
            std::cout << "Adapter scan " << (g_variant_get_boolean(value) ? "on" : "off") << "\n";
        }
    }
}

int Adapter::subscribe_adapter_changed()
{
    return g_dbus_connection_signal_subscribe(s_connection, "org.bluez", "org.freedesktop.DBus.Properties",
                                              "PropertiesChanged", NULL, "org.bluez.Adapter1", G_DBUS_SIGNAL_FLAGS_NONE,
                                              Adapter::signal_changed, NULL, NULL);
}

int Adapter::subscribe_device_added(void (*f)(GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar*,
                                              GVariant*, gpointer))
{
    return g_dbus_connection_signal_subscribe(s_connection, "org.bluez", "org.freedesktop.DBus.ObjectManager",
                                              "InterfacesAdded", NULL, NULL, G_DBUS_SIGNAL_FLAGS_NONE, (*f), s_loop,
                                              NULL);
}

int Adapter::subscribe_device_removed(void (*f)(GDBusConnection*, const gchar*, const gchar*, const gchar*,
                                                const gchar*, GVariant*, gpointer))
{
    return g_dbus_connection_signal_subscribe(s_connection, "org.bluez", "org.freedesktop.DBus.ObjectManager",
                                              "InterfacesRemoved", NULL, NULL, G_DBUS_SIGNAL_FLAGS_NONE, (*f), s_loop,
                                              NULL);
}

void Adapter::run_loop()
{
    g_main_loop_run(s_loop);
}

int Adapter::remove_device(const char* device)
{
    return Adapter::call_method("RemoveDevice", g_variant_new("(o)", device));
}

int Adapter::power_on()
{
    return Adapter::set_property("Powered", g_variant_new("b", TRUE));
}

int Adapter::start_scan()
{
    is_scanning = true;
    return Adapter::call_method("StartDiscovery", NULL);
}

int Adapter::stop_scan()
{
    is_scanning = false;
    return Adapter::call_method("StopDiscovery", NULL);
}

bool Adapter::scanning()
{
    return is_scanning;
}

}    // namespace wolkabout