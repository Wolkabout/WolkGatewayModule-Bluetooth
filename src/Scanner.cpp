#include "Scanner.h"

namespace wolkabout
{

std::vector<std::string> Scanner::s_addr_found = {};

Scanner::Scanner()
{}

void Scanner::device_disappeared(GDBusConnection *sig,
                const gchar *sender_name,
                const gchar *object_path,
                const gchar *interface,
                const gchar *signal_name,
                GVariant *parameters,
                gpointer user_data)
{
    (void)sig;
    (void)sender_name;
    (void)object_path;
    (void)interface;
    (void)signal_name;

    GVariantIter *interfaces;
    const char *object;
    const gchar *interface_name;
    char address[BT_ADDRESS_STRING_SIZE] = {'\0'};

    g_variant_get(parameters, "(&oas)", &object, &interfaces);
    while(g_variant_iter_next(interfaces, "s", &interface_name)) {
        if(g_strstr_len(g_ascii_strdown(interface_name, -1), -1, "device")) {
            int i;
            char *tmp = g_strstr_len(object, -1, "dev_") + 4;

            for(i = 0; *tmp != '\0'; i++, tmp++) {
                if(*tmp == '_') {
                    address[i] = ':';
                    continue;
                }
                address[i] = *tmp;
            }
            auto it = std::find(s_addr_found.begin(), s_addr_found.end(), address);
            if(it != s_addr_found.end()){
            	//std::cout<<"Removed device w address "<<address<<"\n";
            	//std::cout<<"Vector size before removing"<<s_addr_found.size()<<"\n";
                s_addr_found.erase(it);
                //std::cout<<"Vector size after removing"<<s_addr_found.size()<<"\n";
            }
        }
    }
    return;
}

void Scanner::device_appeared(GDBusConnection *sig,
                const gchar *sender_name,
                const gchar *object_path,
                const gchar *interface,
                const gchar *signal_name,
                GVariant *parameters,
                gpointer user_data)
{
    (void)sig;
    (void)sender_name;
    (void)object_path;
    (void)interface;
    (void)signal_name;
    (void)user_data;

    GVariantIter *interfaces;
    const char *object;
    const gchar *interface_name;
    GVariant *properties;
    int rc;

    g_variant_get(parameters, "(&oa{sa{sv}})", &object, &interfaces);
    while(g_variant_iter_next(interfaces, "{&s@a{sv}}", &interface_name, &properties)) {
        if(g_strstr_len(g_ascii_strdown(interface_name, -1), -1, "device")) {
            const gchar *property_name;
            GVariantIter i;
            GVariant *prop_val;
            const gchar* value;
            g_variant_iter_init(&i, properties);
            while(g_variant_iter_next(&i, "{&sv}", &property_name, &prop_val))
                if(!(g_strcmp0(property_name, "Address"))){
                        /*find the device*/
                        value = g_variant_get_string(prop_val, NULL);
                        //std::cout<<"Found device with adress:"<<value<<"\n";
                        //std::cout<<"Vector size before adding"<<s_addr_found.size()<<"\n";
                        s_addr_found.push_back(value);
                        //std::cout<<"Vector size after adding"<<s_addr_found.size()<<"\n";
                }
            g_variant_unref(prop_val);
        }
        g_variant_unref(properties);
    }
    return;
}

}