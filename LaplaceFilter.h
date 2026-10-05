#ifndef LAPLACE_FILTER_H
#define LAPLACE_FILTER_H

#include "Filter.h"

class LaplaceFilter : public Filter {
public:
    const char* getNombre() const;

    void aplicar(
        const Image& entrada,
        Image& salida,
        const Region& region
    ) const;
};

#endif