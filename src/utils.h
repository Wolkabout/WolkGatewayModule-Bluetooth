#ifndef UTILS_H
#define UTILS_H

#include <glib.h>
#include <gio/gio.h>

void free_properties(GVariantIter* properties, GVariant* value);

#endif