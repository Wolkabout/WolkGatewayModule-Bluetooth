#ifndef SCANNER_H
#define SCANNER_H

#include "Adapter.h"
#include "Wolk.h"

#include <algorithm>
#include <gio/gio.h>
#include <glib.h>
#include <iostream>
#include <map>
#include <vector>

#define BT_ADDRESS_STRING_SIZE 18

namespace wolkabout
{
class Scanner
{
public:
    Scanner();

    static void device_disappeared(GDBusConnection* sig, const gchar* sender_name, const gchar* object_path,
                                   const gchar* interface, const gchar* signal_name, GVariant* parameters,
                                   gpointer user_data);

    static void device_appeared(GDBusConnection* sig, const gchar* sender_name, const gchar* object_path,
                                const gchar* interface, const gchar* signal_name, GVariant* parameters,
                                gpointer user_data);

    static std::vector<std::string> getDevices();

    int add_timer(unsigned interval, int (*f)(void*), void* user_data);

    static std::vector<std::string> s_addr_found;
};

}    // namespace wolkabout
#endif