#ifndef BLUR_FILTER_H
#define BLUR_FILTER_H

#include "Filter.h"

class BlurFilter : public Filter {
public:
    const char* getNombre() const;

    void aplicar(
        const Image& entrada,
        Image& salida,
        const Region& region
    ) const;
};

#endif