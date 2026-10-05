// ImageIO.h - Lectura y escritura de imagenes Netpbm
//
// Formatos soportados:
//   P2 = PGM texto     P5 = PGM binario
//   P3 = PPM texto     P6 = PPM binario
//
// El lector reconoce el numero magico y crea el objeto correcto
// (PGMImage o PPMImage). Los comentarios (#) se ignoran en cualquier
// parte del encabezado.

#ifndef IMAGEIO_H
#define IMAGEIO_H

#include "Image.h"
#include <istream>
#include <ostream>

class ImageIO {
public:
    // Devuelve una imagen nueva (el llamador hace delete) o nullptr si falla.
    // "binario" queda en true si el archivo era P5/P6.
    static Image* leer(std::istream& entrada, bool& binario);
    static Image* leerArchivo(const char* ruta, bool& binario);

    // Escribe la imagen en texto (P2/P3) o binario (P5/P6)
    static bool escribir(std::ostream& salida, const Image& img, bool binario);
    static bool escribirArchivo(const char* ruta, const Image& img, bool binario);

private:
    static void saltarComentarios(std::istream& entrada);
};

#endif
