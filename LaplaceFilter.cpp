#include "LaplaceFilter.h"

const char* LaplaceFilter::getNombre() const {
    return "laplace";
}

void LaplaceFilter::aplicar(
    const Image& entrada,
    Image& salida,
    const Region& region
) const {

    const int ancho = entrada.getAncho();
    const int alto = entrada.getAlto();
    const int canales = entrada.getCanales();

    for (int fila = region.filaInicio;
         fila < region.filaFin;
         fila++) {

        for (int col = region.colInicio;
             col < region.colFin;
             col++) {

            // Los bordes se conservan.
            if (fila == 0 ||
                fila == alto - 1 ||
                col == 0 ||
                col == ancho - 1) {

                for (int canal = 0;
                     canal < canales;
                     canal++) {

                    salida.set(
                        fila,
                        col,
                        canal,
                        entrada.get(fila, col, canal)
                    );
                }
            }
            else {

                for (int canal = 0;
                     canal < canales;
                     canal++) {

                    int valor =
                        entrada.get(
                            fila - 1,
                            col,
                            canal
                        )
                        +
                        entrada.get(
                            fila + 1,
                            col,
                            canal
                        )
                        +
                        entrada.get(
                            fila,
                            col - 1,
                            canal
                        )
                        +
                        entrada.get(
                            fila,
                            col + 1,
                            canal
                        )
                        -
                        4 * entrada.get(
                            fila,
                            col,
                            canal
                        );

                    salida.set(
                        fila,
                        col,
                        canal,
                        entrada.recortar(valor)
                    );
                }
            }
        }
    }
}