#ifndef UTILS_H
#define UTILS_H

#include <glib.h>
#include <gio/gio.h>
#include <string>
#include <cstring>
#include <algorithm>

namespace wolkabout{

void free_properties(GVariantIter* properties, GVariant* value);

char* to_object(std::string address);

}//namespace wolkabout
#endif