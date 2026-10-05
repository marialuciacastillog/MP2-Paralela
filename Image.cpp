#include "Image.h"
#include <cstring>   // memcpy

Image::Image(int ancho, int alto, int valorMaximo, int canales)
    : ancho(ancho), alto(alto), valorMaximo(valorMaximo), canales(canales) {
    datos = new int[ancho * alto * canales]();   // () -> inicializa en 0
}

Image::Image(const Image& otra)
    : ancho(otra.ancho), alto(otra.alto),
      valorMaximo(otra.valorMaximo), canales(otra.canales) {
    datos = new int[getTotalValores()];
    memcpy(datos, otra.datos, sizeof(int) * getTotalValores());
}

Image& Image::operator=(const Image& otra) {
    if (this != &otra) {
        delete[] datos;
        ancho = otra.ancho;
        alto = otra.alto;
        valorMaximo = otra.valorMaximo;
        canales = otra.canales;
        datos = new int[getTotalValores()];
        memcpy(datos, otra.datos, sizeof(int) * getTotalValores());
    }
    return *this;
}

Image::~Image() {
    delete[] datos;
}

int Image::recortar(int valor) const {
    if (valor < 0) return 0;
    if (valor > valorMaximo) return valorMaximo;
    return valor;
}

// ---------- PGM ----------
PGMImage::PGMImage(int ancho, int alto, int valorMaximo)
    : Image(ancho, alto, valorMaximo, 1) {}

Image* PGMImage::crearVacia() const { return new PGMImage(ancho, alto, valorMaximo); }
Image* PGMImage::clonar() const     { return new PGMImage(*this); }

// ---------- PPM ----------
PPMImage::PPMImage(int ancho, int alto, int valorMaximo)
    : Image(ancho, alto, valorMaximo, 3) {}

Image* PPMImage::crearVacia() const { return new PPMImage(ancho, alto, valorMaximo); }
Image* PPMImage::clonar() const     { return new PPMImage(*this); }
