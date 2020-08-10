#ifndef UTILS_H
#define UTILS_H

#include <algorithm>
#include <cstring>
#include <gio/gio.h>
#include <glib.h>
#include <string>

namespace wolkabout
{
void free_properties(GVariantIter* properties, GVariant* value);

std::string to_object(std::string address);

std::string str_toupper(std::string s);

}    // namespace wolkabout
#endif