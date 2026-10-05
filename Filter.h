// Filter.h - Interfaz comun para TODOS los filtros
//
// Esta es la pieza que deja listo el proyecto para los siguientes disenios:
//
//   Disenio 2 (secuencial): cada filtro (Blur, Laplace, Sharpen) hereda de
//       Filter e implementa aplicar(). Se llama con la imagen completa.
//   Disenio 3 (hilos): se crean 4 Region (cuadrantes) y cada hilo llama
//       aplicar(entrada, salida, suRegion). No hace falta mutex porque
//       cada hilo ESCRIBE solo en su region y la entrada solo se LEE.
//   Disenio 3 (OpenMP): se pone #pragma omp parallel for dentro de aplicar().
//   Disenio 4 (MPI): cada nodo aplica el filtro a la region que recibio.
//
// IMPORTANTE: "entrada" siempre es la imagen completa (solo lectura), asi un
// pixel en la frontera de su region puede leer a sus vecinos de otra region.

#ifndef FILTER_H
#define FILTER_H

#include "Image.h"

// Rectangulo de la imagen: filas [filaInicio, filaFin), columnas [colInicio, colFin)
struct Region {
    int filaInicio;
    int filaFin;
    int colInicio;
    int colFin;
};

class Filter {
public:
    virtual ~Filter() {}

    virtual const char* getNombre() const = 0;

    // Aplica el filtro solo a los pixeles de "region" y escribe en "salida"
    virtual void aplicar(const Image& entrada, Image& salida, const Region& region) const = 0;

    // Atajo: aplica el filtro a la imagen completa
    void aplicar(const Image& entrada, Image& salida) const {
        Region todo = {0, entrada.getAlto(), 0, entrada.getAncho()};
        aplicar(entrada, salida, todo);
    }
};

#endif
