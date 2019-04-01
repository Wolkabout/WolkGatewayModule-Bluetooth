#include "utils.h"

void free_properties(GVariantIter* properties, GVariant* value)
{
    if(properties != NULL)
        g_variant_iter_free(properties);
    if(value != NULL)
        g_variant_unref(value);

    return;
}