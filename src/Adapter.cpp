#include "Adapter.h"

namespace wolkabout
{
/*Adapter::Adapter()
{
	this.m_connection = g_bus_get_sync(G_BUS_TYPE_SYSTEM, NULL, NULL);
}


int Adapter::adapter_call_method(const char *method, GVariant *param)
{
    GVariant *result;
    GError *error = NULL;

    result = g_dbus_connection_call_sync(m_connection,
                         "org.bluez",
                         "/org/bluez/hci0",
                         "org.bluez.Adapter1",
                         method,
                         param,
                         NULL,
                         G_DBUS_CALL_FLAGS_NONE,
                         -1,
                         NULL,
                         &error);
    if(error != NULL)
        return 1;

    g_variant_unref(result);
    return 0;
}*/

}	//namespace wolkabout