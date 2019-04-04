#include "utils.h"

namespace wolkabout{

void free_properties(GVariantIter* properties, GVariant* value)
{
    if(properties != NULL)
        g_variant_iter_free(properties);
    if(value != NULL)
        g_variant_unref(value);

    return;
}

char* to_object(std::string address)
{
	std::string pre = "/org/bluez/hci0/dev_";
	std::replace(address.begin(), address.end(), ':', '_');
	pre.append(address);
	char* result = new char [pre.length() + 1];
	std::strcpy(result, pre.c_str());
	return result;
}

std::string str_toupper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), 
                   [](unsigned char c){ return std::toupper(c); }
                  );
    return s;
}

}//namespace wolkabout