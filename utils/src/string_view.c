#include "../include/string_view.h"

StringView string_view_create(const char *data, size_t len) {
    return (StringView) {
        .data = data,
        .len = len,
    };
}