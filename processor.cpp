// processor.cpp - Disenio 1: Aplicacion base
//
// Uso:
//   ./processor entrada salida [--ascii | --binary]
//   ./processor lena.pgm lena2.pgm
//   ./processor - salida.ppm < lena.ppm      ("-" = entrada estandar)
//   cat lena.ppm | ./processor - -           (lee stdin, escribe stdout)
//
// Lee una imagen PGM o PPM, muestra su informacion y la vuelve a escribir.
// Por defecto la salida conserva la codificacion de la entrada (texto o
// binario); --ascii o --binary la fuerzan.

#include <iostream>
#include <cstring>
#include "Image.h"
#include "ImageIO.h"
#include "Filter.h"
#include "Timer.h"

using namespace std;

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Uso: " << argv[0] << " <entrada|-> <salida|-> [--ascii|--binary]" << endl;
        return 1;
    }

    const char* rutaEntrada = argv[1];
    const char* rutaSalida = argv[2];

    // Si la salida va a stdout, la informacion se imprime en stderr
    // para no mezclarla con la imagen
    bool salidaEstandar = (strcmp(rutaSalida, "-") == 0);
    ostream& info = salidaEstandar ? cerr : cout;

    Timer tLectura, tEscritura;

    tLectura.iniciar();
    bool binario = false;
    Image* img = ImageIO::leerArchivo(rutaEntrada, binario);
    tLectura.detener();

    if (img == nullptr) {
        return 1;
    }

    // Opciones de formato de salida
    bool binarioSalida = binario;
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--ascii") == 0)  binarioSalida = false;
        if (strcmp(argv[i], "--binary") == 0) binarioSalida = true;
    }

    info << "Imagen:        " << rutaEntrada << endl;
    info << "Tipo:          " << img->getTipo() << " ("
         << img->getNumeroMagico(binario) << ")" << endl;
    info << "Dimensiones:   " << img->getAncho() << " x " << img->getAlto() << endl;
    info << "Valor maximo:  " << img->getValorMaximo() << endl;
    info << "Canales:       " << img->getCanales() << endl;

    tEscritura.iniciar();
    bool ok = ImageIO::escribirArchivo(rutaSalida, *img, binarioSalida);
    tEscritura.detener();

    if (!ok) {
        delete img;
        return 1;
    }

    info << "Salida:        " << rutaSalida << " ("
         << img->getNumeroMagico(binarioSalida) << ")" << endl;
    info << "Lectura:       " << tLectura.getTiempoTotalMs() << " ms total, "
         << tLectura.getTiempoCPUMs() << " ms CPU" << endl;
    info << "Escritura:     " << tEscritura.getTiempoTotalMs() << " ms total, "
         << tEscritura.getTiempoCPUMs() << " ms CPU" << endl;

    delete img;
    return 0;
}
