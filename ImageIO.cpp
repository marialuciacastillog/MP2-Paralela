#include "ImageIO.h"
#include <fstream>
#include <iostream>
#include <cstring>

using namespace std;

void ImageIO::saltarComentarios(istream& entrada) {
    entrada >> ws;
    while (entrada.peek() == '#') {
        entrada.ignore(1000000, '\n');
        entrada >> ws;
    }
}


Image* ImageIO::leer(istream& entrada, bool& binario) {
    char magico[3] = {0, 0, 0};
    entrada.read(magico, 2);

    if (!entrada || magico[0] != 'P') {
        cerr << "Error: el archivo no es una imagen Netpbm." << endl;
        return nullptr;
    }

    char tipo = magico[1];
    if (tipo != '2' && tipo != '3' && tipo != '5' && tipo != '6') {
        cerr << "Error: formato " << magico << " no soportado (use P2, P3, P5 o P6)." << endl;
        return nullptr;
    }

    binario = (tipo == '5' || tipo == '6');
    bool esColor = (tipo == '3' || tipo == '6');

    int ancho, alto, valorMaximo;
    saltarComentarios(entrada); entrada >> ancho;
    saltarComentarios(entrada); entrada >> alto;
    saltarComentarios(entrada); entrada >> valorMaximo;

    if (!entrada || ancho <= 0 || alto <= 0 || valorMaximo <= 0 || valorMaximo > 65535) {
        cerr << "Error: encabezado invalido." << endl;
        return nullptr;
    }

    Image* img;
    if (esColor) {
        img = new PPMImage(ancho, alto, valorMaximo);
    } else {
        img = new PGMImage(ancho, alto, valorMaximo);
    }

    int* datos = img->getDatos();
    int total = img->getTotalValores();

    if (binario) {
        // Despues del valor maximo hay exactamente UN espacio en blanco
        entrada.get();

        if (valorMaximo < 256) {
            // 1 byte por valor
            unsigned char* buffer = new unsigned char[total];
            entrada.read(reinterpret_cast<char*>(buffer), total);
            for (int k = 0; k < total; k++) {
                datos[k] = buffer[k];
            }
            delete[] buffer;
        } else {
            // 2 bytes por valor, el mas significativo primero
            unsigned char* buffer = new unsigned char[total * 2];
            entrada.read(reinterpret_cast<char*>(buffer), total * 2);
            for (int k = 0; k < total; k++) {
                datos[k] = (buffer[2 * k] << 8) | buffer[2 * k + 1];
            }
            delete[] buffer;
        }
    } else {
        for (int k = 0; k < total; k++) {
            saltarComentarios(entrada);
            entrada >> datos[k];
        }
    }

    if (!entrada) {
        cerr << "Error: faltan pixeles en el archivo." << endl;
        delete img;
        return nullptr;
    }

    return img;
}


Image* ImageIO::leerArchivo(const char* ruta, bool& binario) {
    // "-" significa entrada estandar
    if (strcmp(ruta, "-") == 0) {
        return leer(cin, binario);
    }

    ifstream archivo(ruta, ios::binary);
    if (!archivo.is_open()) {
        cerr << "Error: no se pudo abrir " << ruta << endl;
        return nullptr;
    }
    return leer(archivo, binario);
}


bool ImageIO::escribir(ostream& salida, const Image& img, bool binario) {
    salida << img.getNumeroMagico(binario) << "\n";
    salida << img.getAncho() << " " << img.getAlto() << "\n";
    salida << img.getValorMaximo() << "\n";

    const int* datos = img.getDatos();
    int total = img.getTotalValores();

    if (binario) {
        if (img.getValorMaximo() < 256) {
            unsigned char* buffer = new unsigned char[total];
            for (int k = 0; k < total; k++) {
                buffer[k] = (unsigned char) datos[k];
            }
            salida.write(reinterpret_cast<char*>(buffer), total);
            delete[] buffer;
        } else {
            unsigned char* buffer = new unsigned char[total * 2];
            for (int k = 0; k < total; k++) {
                buffer[2 * k]     = (datos[k] >> 8) & 0xFF;
                buffer[2 * k + 1] = datos[k] & 0xFF;
            }
            salida.write(reinterpret_cast<char*>(buffer), total * 2);
            delete[] buffer;
        }
    } else {
        // Texto: una fila de la imagen por linea
        int valoresPorFila = img.getAncho() * img.getCanales();
        for (int k = 0; k < total; k++) {
            salida << datos[k];
            if ((k + 1) % valoresPorFila == 0) {
                salida << "\n";
            } else {
                salida << " ";
            }
        }
    }

    return (bool) salida;
}


bool ImageIO::escribirArchivo(const char* ruta, const Image& img, bool binario) {
    if (strcmp(ruta, "-") == 0) {
        return escribir(cout, img, binario);
    }

    ofstream archivo(ruta, ios::binary);
    if (!archivo.is_open()) {
        cerr << "Error: no se pudo crear " << ruta << endl;
        return false;
    }
    return escribir(archivo, img, binario);
}
