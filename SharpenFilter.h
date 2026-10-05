#ifndef SHARPEN_FILTER_H
#define SHARPEN_FILTER_H

#include "Filter.h"

class SharpenFilter : public Filter {
public:
    const char* getNombre() const;

    void aplicar(
        const Image& entrada,
        Image& salida,
        const Region& region
    ) const;
};

#endif