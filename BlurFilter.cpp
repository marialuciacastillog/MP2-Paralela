#include "BlurFilter.h"

const char* BlurFilter::getNombre() const {
    return "blur";
}

void BlurFilter::aplicar(
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

                    int suma = 0;

                    for (int df = -1; df <= 1; df++) {
                        for (int dc = -1; dc <= 1; dc++) {

                            suma += entrada.get(
                                fila + df,
                                col + dc,
                                canal
                            );
                        }
                    }

                    int valor = suma / 9;

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