#ifndef STRING_VIEW_H
#define STRING_VIEW_H

#include <stddef.h>

typedef struct StringView StringView;

// fn main() { return 0;}
//             ^
//             6
// StringView para 'return' = endereço do caractere + tamanho da string, evita cópia, deve ser imutável
struct StringView {
    const char *data;
    size_t len; // 'size_t' = tamanho do ponteiro, definida pela arquitetura, normalmente 64 bit ou 32 bits
};

StringView string_view_create(const char *data, size_t len);

#endif /* STRING_VIEW_H */