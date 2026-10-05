// omp_filterer.cpp - Disenio 3: Memoria compartida con OpenMP
//
// Uso:
//   ./omp_filterer sulfur.pgm sulfur_N.pgm
//        Sin --f: aplica LOS TRES filtros (blur, laplace, sharpen) a la imagen
//        de entrada y escribe un archivo por filtro:
//            sulfur_N_blur.pgm  sulfur_N_laplace.pgm  sulfur_N_sharpen.pgm
//
//   ./omp_filterer entrada salida --f blur [--f sharpen ...]
//        Con --f: igual que filterer (filtros encadenados, un solo archivo).
//
// Opciones:
//   --t N       numero de hilos (si no se pone, usa OMP_NUM_THREADS o
//               todos los nucleos disponibles)
//   --ascii | --binary
//
// IDEA GENERAL
//   OpenMP reparte las FILAS de la imagen entre los hilos con:
//
//       #pragma omp for schedule(static)
//       for (fila = 0; fila < alto; fila++) ...
//
//   schedule(static) divide las filas en bloques contiguos del mismo tamanio
//   (hilo 0 las primeras alto/N filas, hilo 1 las siguientes, ...), lo que
//   aprovecha bien la cache porque la imagen esta guardada fila por fila.
//
//   Para cada fila se llama filtro->aplicar(entrada, salida, regionDeUnaFila),
//   es decir, se reutilizan SIN CAMBIOS las clases del Disenio 2.
//
// POR QUE NO SE NECESITA "critical" NI "atomic"
//   - "entrada" solo se lee.
//   - Cada fila de "salida" la escribe un unico hilo.
//   => No hay condiciones de carrera y el resultado es identico al secuencial.
//   Al final del "omp parallel" hay una barrera implicita: cuando termina un
//   filtro, TODAS las filas estan listas para el siguiente.

#include <omp.h>
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <ctime>

#include "Image.h"
#include "ImageIO.h"
#include "Filter.h"
#include "BlurFilter.h"
#include "LaplaceFilter.h"
#include "SharpenFilter.h"
#include "Timer.h"

using namespace std;

#define MAX_FILTROS 32
#define MAX_RUTA    1024


// Tiempo de CPU consumido SOLO por el hilo que llama esta funcion
static double tiempoCPUHiloMs() {
#ifdef CLOCK_THREAD_CPUTIME_ID
    timespec ts;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1.0e6;
#else
    return 0.0;
#endif
}


// -------------------------------------------------------------
// Estadisticas por hilo (arreglos, uno por hilo)
// -------------------------------------------------------------
struct EstadisticasHilos {
    int hilosUsados;
    int* filas;        // filas procesadas por cada hilo
    double* msTotal;   // tiempo de pared de cada hilo
    double* msCPU;     // tiempo de CPU de cada hilo
};


// -------------------------------------------------------------
// Aplica un filtro en paralelo, repartiendo las filas entre los hilos
// -------------------------------------------------------------
static void aplicarParalelo(const Filter* filtro, const Image& entrada, Image& salida,
                            EstadisticasHilos& est) {
    const int alto = entrada.getAlto();
    const int ancho = entrada.getAncho();

    #pragma omp parallel
    {
        int id = omp_get_thread_num();
        int misFilas = 0;

        double inicio = omp_get_wtime();
        double cpuInicio = tiempoCPUHiloMs();

        #pragma omp for schedule(static) nowait
        for (int fila = 0; fila < alto; fila++) {
            Region unaFila = {fila, fila + 1, 0, ancho};
            filtro->aplicar(entrada, salida, unaFila);
            misFilas++;
        }

        // Cada hilo escribe solo en SU posicion del arreglo: sin carreras
        est.filas[id]   = misFilas;
        est.msTotal[id] = (omp_get_wtime() - inicio) * 1000.0;
        est.msCPU[id]   = tiempoCPUHiloMs() - cpuInicio;

        #pragma omp single nowait
        est.hilosUsados = omp_get_num_threads();
    }   // <- barrera implicita: todos los hilos terminaron
}


// Aplica el filtro, mide e imprime la tabla de hilos
static void aplicarYReportar(int numero, const Filter* filtro, const Image& entrada,
                             Image& salida, EstadisticasHilos& est, ostream& info) {
    Timer t;
    t.iniciar();
    aplicarParalelo(filtro, entrada, salida, est);
    t.detener();

    info << endl;
    info << "Filtro " << numero << " (" << filtro->getNombre() << "): "
         << t.getTiempoTotalMs() << " ms total, "
         << t.getTiempoCPUMs() << " ms CPU (suma de los hilos)" << endl;

    info << "  Hilo\tFilas\tTotal(ms)\tCPU(ms)" << endl;
    for (int h = 0; h < est.hilosUsados; h++) {
        info << "  " << h << "\t" << est.filas[h] << "\t"
             << est.msTotal[h] << "\t\t" << est.msCPU[h] << endl;
    }
}


static Filter* crearFiltro(const char* nombre) {
    if (strcmp(nombre, "blur") == 0)    return new BlurFilter();
    if (strcmp(nombre, "laplace") == 0) return new LaplaceFilter();
    if (strcmp(nombre, "sharpen") == 0) return new SharpenFilter();
    return nullptr;
}


// "sulfur_N.pgm" + "blur" -> "sulfur_N_blur.pgm"
static void construirRuta(const char* base, const char* sufijo, char* destino) {
    const char* punto = strrchr(base, '.');
    const char* barra = strrchr(base, '/');
    if (punto == nullptr || (barra != nullptr && punto < barra)) {
        snprintf(destino, MAX_RUTA, "%s_%s", base, sufijo);
    } else {
        int largoNombre = (int) (punto - base);
        snprintf(destino, MAX_RUTA, "%.*s_%s%s", largoNombre, base, sufijo, punto);
    }
}


static void mostrarUso(const char* programa) {
    cerr << "Uso: " << programa
         << " <entrada|-> <salida> [--f <blur|laplace|sharpen> ...] [--t N] [--ascii|--binary]"
         << endl
         << "     Sin --f se aplican los tres filtros y se escribe un archivo por filtro."
         << endl;
}


static void liberarFiltros(Filter* filtros[], int cantidad) {
    for (int i = 0; i < cantidad; i++) delete filtros[i];
}


int main(int argc, char* argv[]) {
    if (argc < 3) {
        mostrarUso(argv[0]);
        return 1;
    }

    const char* rutaEntrada = argv[1];
    const char* rutaSalida = argv[2];

    Filter* filtros[MAX_FILTROS];
    int cantidadFiltros = 0;
    bool forzarAscii = false;
    bool forzarBinario = false;
    int hilosPedidos = 0;

    // -------------------------------------------------------------
    // Argumentos
    // -------------------------------------------------------------
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--f") == 0 && i + 1 < argc) {
            if (cantidadFiltros >= MAX_FILTROS) {
                cerr << "Error: se permiten como maximo " << MAX_FILTROS << " filtros." << endl;
                liberarFiltros(filtros, cantidadFiltros);
                return 1;
            }
            Filter* f = crearFiltro(argv[i + 1]);
            if (f == nullptr) {
                cerr << "Error: filtro no soportado: " << argv[i + 1] << endl;
                cerr << "Filtros disponibles: blur, laplace, sharpen" << endl;
                liberarFiltros(filtros, cantidadFiltros);
                return 1;
            }
            filtros[cantidadFiltros] = f;
            cantidadFiltros++;
            i++;
        } else if (strcmp(argv[i], "--t") == 0 && i + 1 < argc) {
            hilosPedidos = atoi(argv[i + 1]);
            if (hilosPedidos <= 0) {
                cerr << "Error: --t necesita un numero de hilos mayor que 0." << endl;
                liberarFiltros(filtros, cantidadFiltros);
                return 1;
            }
            i++;
        } else if (strcmp(argv[i], "--ascii") == 0) {
            forzarAscii = true;
        } else if (strcmp(argv[i], "--binary") == 0) {
            forzarBinario = true;
        } else {
            cerr << "Error: opcion no reconocida: " << argv[i] << endl;
            mostrarUso(argv[0]);
            liberarFiltros(filtros, cantidadFiltros);
            return 1;
        }
    }

    if (forzarAscii && forzarBinario) {
        cerr << "Error: no puede usar --ascii y --binary al mismo tiempo." << endl;
        liberarFiltros(filtros, cantidadFiltros);
        return 1;
    }

    // Sin --f: modo "tres filtros", un archivo por filtro
    bool modoTresFiltros = (cantidadFiltros == 0);
    if (modoTresFiltros) {
        if (strcmp(rutaSalida, "-") == 0) {
            cerr << "Error: sin --f se escriben 3 archivos; la salida no puede ser '-'." << endl;
            return 1;
        }
        filtros[0] = new BlurFilter();
        filtros[1] = new LaplaceFilter();
        filtros[2] = new SharpenFilter();
        cantidadFiltros = 3;
    }

    if (hilosPedidos > 0) {
        omp_set_num_threads(hilosPedidos);
    }

    bool salidaEstandar = (strcmp(rutaSalida, "-") == 0);
    ostream& info = salidaEstandar ? cerr : cout;

    // -------------------------------------------------------------
    // LECTURA
    // -------------------------------------------------------------
    Timer tLectura;
    tLectura.iniciar();
    bool binarioEntrada = false;
    Image* original = ImageIO::leerArchivo(rutaEntrada, binarioEntrada);
    tLectura.detener();

    if (original == nullptr) {
        liberarFiltros(filtros, cantidadFiltros);
        return 1;
    }

    bool binarioSalida = binarioEntrada;
    if (forzarAscii)   binarioSalida = false;
    if (forzarBinario) binarioSalida = true;

    // Arreglos para las estadisticas de cada hilo
    int maxHilos = omp_get_max_threads();
    EstadisticasHilos est;
    est.hilosUsados = 0;
    est.filas   = new int[maxHilos];
    est.msTotal = new double[maxHilos];
    est.msCPU   = new double[maxHilos];

    info << "Imagen:        " << rutaEntrada << endl;
    info << "Tipo:          " << original->getTipo() << endl;
    info << "Dimensiones:   " << original->getAncho() << " x " << original->getAlto() << endl;
    info << "Valor maximo:  " << original->getValorMaximo() << endl;
    info << "Canales:       " << original->getCanales() << endl;
    info << "Hilos OpenMP:  " << maxHilos
         << " (nucleos disponibles: " << omp_get_num_procs() << ")" << endl;
    info << "Modo:          "
         << (modoTresFiltros ? "tres filtros por separado" : "filtros encadenados") << endl;
    info << "Filtros:       ";
    for (int i = 0; i < cantidadFiltros; i++) {
        if (i > 0) info << (modoTresFiltros ? ", " : " -> ");
        info << filtros[i]->getNombre();
    }
    info << endl;
    info << "Lectura:       " << tLectura.getTiempoTotalMs() << " ms total, "
         << tLectura.getTiempoCPUMs() << " ms CPU" << endl;

    Timer tProcesamiento;
    Timer tEscritura;
    double msEscritura = 0.0, msEscrituraCPU = 0.0;
    bool ok = true;

    if (modoTresFiltros) {
        // ---------------------------------------------------------
        // Cada filtro se aplica a la imagen ORIGINAL y se guarda aparte
        // ---------------------------------------------------------
        double msProc = 0.0, msProcCPU = 0.0;

        for (int i = 0; i < cantidadFiltros && ok; i++) {
            Image* salida = original->crearVacia();

            tProcesamiento.iniciar();
            aplicarYReportar(i + 1, filtros[i], *original, *salida, est, info);
            tProcesamiento.detener();
            msProc += tProcesamiento.getTiempoTotalMs();
            msProcCPU += tProcesamiento.getTiempoCPUMs();

            char ruta[MAX_RUTA];
            construirRuta(rutaSalida, filtros[i]->getNombre(), ruta);

            tEscritura.iniciar();
            ok = ImageIO::escribirArchivo(ruta, *salida, binarioSalida);
            tEscritura.detener();
            msEscritura += tEscritura.getTiempoTotalMs();
            msEscrituraCPU += tEscritura.getTiempoCPUMs();

            info << "  Salida: " << ruta << " (" << salida->getNumeroMagico(binarioSalida) << ")" << endl;
            delete salida;
        }

        info << endl;
        info << "Procesamiento: " << msProc << " ms total, " << msProcCPU << " ms CPU" << endl;
    } else {
        // ---------------------------------------------------------
        // Filtros encadenados (como filterer)
        // ---------------------------------------------------------
        Image* actual = original;
        original = nullptr;

        tProcesamiento.iniciar();
        for (int i = 0; i < cantidadFiltros; i++) {
            Image* salida = actual->crearVacia();
            aplicarYReportar(i + 1, filtros[i], *actual, *salida, est, info);
            delete actual;
            actual = salida;
        }
        tProcesamiento.detener();

        tEscritura.iniciar();
        ok = ImageIO::escribirArchivo(rutaSalida, *actual, binarioSalida);
        tEscritura.detener();
        msEscritura = tEscritura.getTiempoTotalMs();
        msEscrituraCPU = tEscritura.getTiempoCPUMs();

        info << endl;
        info << "Procesamiento: " << tProcesamiento.getTiempoTotalMs() << " ms total, "
             << tProcesamiento.getTiempoCPUMs() << " ms CPU" << endl;
        if (ok) {
            info << "Salida:        " << rutaSalida << " ("
                 << actual->getNumeroMagico(binarioSalida) << ")" << endl;
        }
        delete actual;
    }

    info << "Escritura:     " << msEscritura << " ms total, " << msEscrituraCPU << " ms CPU" << endl;

    delete original;
    delete[] est.filas;
    delete[] est.msTotal;
    delete[] est.msCPU;
    liberarFiltros(filtros, cantidadFiltros);

    return ok ? 0 : 1;
}
