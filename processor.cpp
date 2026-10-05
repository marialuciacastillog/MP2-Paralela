// processor.cpp - Disenio 2: Version secuencial
//
// Uso:
//
// ./filterer entrada salida --f blur
// ./filterer entrada salida --f laplace
// ./filterer entrada salida --f sharpen
//
// Tambien permite varios filtros:
//
// ./filterer entrada salida --f blur --f sharpen
//
// Opciones:
//
// --ascii
// --binary

#include <iostream>
#include <cstring>

#include "Image.h"
#include "ImageIO.h"
#include "Filter.h"
#include "BlurFilter.h"
#include "LaplaceFilter.h"
#include "SharpenFilter.h"
#include "Timer.h"

using namespace std;


// ---------------------------------------------------------
// Crea un filtro segun su nombre
// ---------------------------------------------------------

static Filter* crearFiltro(const char* nombre) {

    if (strcmp(nombre, "blur") == 0) {
        return new BlurFilter();
    }

    if (strcmp(nombre, "laplace") == 0) {
        return new LaplaceFilter();
    }

    if (strcmp(nombre, "sharpen") == 0) {
        return new SharpenFilter();
    }

    return nullptr;
}


// ---------------------------------------------------------
// Muestra la forma correcta de ejecutar el programa
// ---------------------------------------------------------

static void mostrarUso(const char* programa) {

    cerr
        << "Uso: "
        << programa
        << " <entrada|-> <salida|-> "
        << "--f <blur|laplace|sharpen> "
        << "[--f <filtro> ...] "
        << "[--ascii|--binary]"
        << endl;
}


// ---------------------------------------------------------
// Programa principal
// ---------------------------------------------------------

int main(int argc, char* argv[]) {

    if (argc < 5) {
        mostrarUso(argv[0]);
        return 1;
    }


    const char* rutaEntrada = argv[1];
    const char* rutaSalida = argv[2];


    // -----------------------------------------------------
    // Arreglo de filtros
    // -----------------------------------------------------

    Filter* filtros[32];

    int cantidadFiltros = 0;

    bool asciiSalidaForzado = false;
    bool binarioSalidaForzado = false;


    // -----------------------------------------------------
    // Procesar argumentos
    // -----------------------------------------------------

    for (int i = 3; i < argc; i++) {

        if (strcmp(argv[i], "--f") == 0) {

            if (i + 1 >= argc) {

                cerr
                    << "Error: --f necesita un nombre de filtro."
                    << endl;

                return 1;
            }


            if (cantidadFiltros >= 32) {

                cerr
                    << "Error: se permiten como maximo 32 filtros."
                    << endl;

                return 1;
            }


            Filter* filtro =
                crearFiltro(argv[i + 1]);


            if (filtro == nullptr) {

                cerr
                    << "Error: filtro no soportado: "
                    << argv[i + 1]
                    << endl;

                cerr
                    << "Filtros disponibles: "
                    << "blur, laplace, sharpen"
                    << endl;

                return 1;
            }


            filtros[cantidadFiltros] = filtro;

            cantidadFiltros++;

            i++;
        }

        else if (strcmp(argv[i], "--ascii") == 0) {

            asciiSalidaForzado = true;
        }

        else if (strcmp(argv[i], "--binary") == 0) {

            binarioSalidaForzado = true;
        }

        else {

            cerr
                << "Error: opcion no reconocida: "
                << argv[i]
                << endl;

            mostrarUso(argv[0]);

            return 1;
        }
    }


    // -----------------------------------------------------
    // Verificar que haya por lo menos un filtro
    // -----------------------------------------------------

    if (cantidadFiltros == 0) {

        cerr
            << "Error: debe indicar al menos un filtro "
            << "con --f."
            << endl;

        return 1;
    }


    // -----------------------------------------------------
    // Verificar opciones incompatibles
    // -----------------------------------------------------

    if (asciiSalidaForzado &&
        binarioSalidaForzado) {

        cerr
            << "Error: no puede usar --ascii "
            << "y --binary al mismo tiempo."
            << endl;

        return 1;
    }


    // -----------------------------------------------------
    // Si stdout se usa para la imagen, la informacion
    // se manda a stderr.
    // -----------------------------------------------------

    bool salidaEstandar =
        (strcmp(rutaSalida, "-") == 0);

    ostream& info =
        salidaEstandar ? cerr : cout;


    // -----------------------------------------------------
    // LECTURA
    // -----------------------------------------------------

    Timer tLectura;

    tLectura.iniciar();

    bool binarioEntrada = false;

    Image* actual =
        ImageIO::leerArchivo(
            rutaEntrada,
            binarioEntrada
        );

    tLectura.detener();


    if (actual == nullptr) {

        for (int i = 0;
             i < cantidadFiltros;
             i++) {

            delete filtros[i];
        }

        return 1;
    }


    // -----------------------------------------------------
    // Determinar formato de salida
    // -----------------------------------------------------

    bool binarioSalida =
        binarioEntrada;


    if (asciiSalidaForzado) {
        binarioSalida = false;
    }

    if (binarioSalidaForzado) {
        binarioSalida = true;
    }


    // -----------------------------------------------------
    // Mostrar informacion de la imagen
    // -----------------------------------------------------

    info
        << "Imagen:        "
        << rutaEntrada
        << endl;

    info
        << "Tipo:          "
        << actual->getTipo()
        << endl;

    info
        << "Dimensiones:   "
        << actual->getAncho()
        << " x "
        << actual->getAlto()
        << endl;

    info
        << "Valor maximo:  "
        << actual->getValorMaximo()
        << endl;

    info
        << "Canales:       "
        << actual->getCanales()
        << endl;


    // -----------------------------------------------------
    // Mostrar cadena de filtros
    // -----------------------------------------------------

    info
        << "Filtros:       ";

    for (int i = 0;
         i < cantidadFiltros;
         i++) {

        if (i > 0) {
            info << " -> ";
        }

        info
            << filtros[i]->getNombre();
    }

    info << endl;


    // -----------------------------------------------------
    // PROCESAMIENTO
    // -----------------------------------------------------

    Timer tProcesamiento;

    tProcesamiento.iniciar();


    for (int i = 0;
         i < cantidadFiltros;
         i++) {


        // Crear una imagen vacia del mismo tipo
        Image* salida =
            actual->crearVacia();


        // Medir el filtro individual
        Timer tFiltro;

        tFiltro.iniciar();


        filtros[i]->aplicar(
            *actual,
            *salida
        );


        tFiltro.detener();


        info
            << "Filtro "
            << (i + 1)
            << " ("
            << filtros[i]->getNombre()
            << "): "
            << tFiltro.getTiempoTotalMs()
            << " ms total, "
            << tFiltro.getTiempoCPUMs()
            << " ms CPU"
            << endl;


        // La salida pasa a ser la entrada
        // del siguiente filtro.

        delete actual;

        actual = salida;
    }


    tProcesamiento.detener();


    // -----------------------------------------------------
    // ESCRITURA
    // -----------------------------------------------------

    Timer tEscritura;

    tEscritura.iniciar();

    bool ok =
        ImageIO::escribirArchivo(
            rutaSalida,
            *actual,
            binarioSalida
        );

    tEscritura.detener();


    if (!ok) {

        delete actual;

        for (int i = 0;
             i < cantidadFiltros;
             i++) {

            delete filtros[i];
        }

        return 1;
    }


    // -----------------------------------------------------
    // RESULTADOS
    // -----------------------------------------------------

    info
        << "Procesamiento: "
        << tProcesamiento.getTiempoTotalMs()
        << " ms total, "
        << tProcesamiento.getTiempoCPUMs()
        << " ms CPU"
        << endl;

    info
        << "Escritura:     "
        << tEscritura.getTiempoTotalMs()
        << " ms total, "
        << tEscritura.getTiempoCPUMs()
        << " ms CPU"
        << endl;

    info
        << "Salida:        "
        << rutaSalida
        << " ("
        << actual->getNumeroMagico(
               binarioSalida
           )
        << ")"
        << endl;


    // -----------------------------------------------------
    // Liberar memoria
    // -----------------------------------------------------

    delete actual;

    for (int i = 0;
         i < cantidadFiltros;
         i++) {

        delete filtros[i];
    }


    return 0;
}