#ifndef ADAPTER_H
#define ADAPTER_H

#include <iostream>

#include <glib.h>
#include <gio/gio.h>

#include "utils.h"

namespace wolkabout
{

static GDBusConnection* s_connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
static GMainLoop* s_loop = g_main_loop_new(NULL, FALSE);

class Adapter
{
public:
	Adapter();

	static int call_method(const char *method, GVariant *param);

	static int set_property(const char *prop, GVariant *value);

	guint subscribe_adapter_changed();

	guint subscribe_device_added(void (*f)(GDBusConnection*, 
											const gchar*, 
											const gchar*,
											const gchar*, 
											const gchar*, 
											GVariant*, 
											gpointer));
	guint subscribe_device_removed(void (*f)(GDBusConnection*, 
											const gchar*, 
											const gchar*,
											const gchar*, 
											const gchar*, 
											GVariant*, 
											gpointer));

	void run_loop();

private:

	static void signal_changed(GDBusConnection *conn,
                    const gchar *sender,
                    const gchar *path,
                    const gchar *interface,
                    const gchar *signal,
                    GVariant *params,
                    void *userdata);
};

}//namespace wolkabout
#endif