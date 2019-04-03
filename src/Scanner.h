#ifndef SCANNER_H
#define SCANNER_H

#include <glib.h>
#include <gio/gio.h>
#include <iostream>
#include <map>
#include <vector>
#include <algorithm>

#include "Adapter.h"
#include "Wolk.h"

#define BT_ADDRESS_STRING_SIZE 18

namespace wolkabout
{

class Scanner
{
	friend class Adapter;
public:

	Scanner();

	static void device_disappeared(GDBusConnection *sig,
                					const gchar *sender_name,
                					const gchar *object_path,
                					const gchar *interface,
                					const gchar *signal_name,
                					GVariant *parameters,
                					gpointer user_data);

	static void device_appeared(GDBusConnection *sig,
                					const gchar *sender_name,
                					const gchar *object_path,
                					const gchar *interface,
                					const gchar *signal_name,
                					GVariant *parameters,
                					gpointer user_data);
        static std::vector<std::string> s_addr_found;
};


}
#endif