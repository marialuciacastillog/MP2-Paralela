// th_filterer.cpp - Disenio 3: Memoria compartida con pthreads
//
// Uso (igual que filterer):
//   ./th_filterer entrada salida --f blur
//   ./th_filterer entrada salida --f blur --f sharpen      (filtros encadenados)
//   ./th_filterer entrada salida --f laplace --ascii
//
// IDEA GENERAL
//   1. Se lee la imagen completa (un solo arreglo plano en memoria).
//   2. La imagen se divide en CUATRO regiones (cuadrantes):
//
//            col 0        mitadC        ancho
//        fila 0 +------------+------------+
//               |   Hilo 0   |   Hilo 1   |
//               | arriba-izq | arriba-der |
//        mitadF +------------+------------+
//               |   Hilo 2   |   Hilo 3   |
//               | abajo-izq  | abajo-der  |
//          alto +------------+------------+
//
//   3. Se crean 4 hilos con pthread_create. A cada hilo se le entrega
//      (en un struct TareaHilo) su region y el filtro a aplicar.
//   4. Cada hilo llama filtro->aplicar(entrada, salida, suRegion), es decir,
//      usa EXACTAMENTE las mismas clases BlurFilter, LaplaceFilter y
//      SharpenFilter del Disenio 2. No hubo que modificar ningun filtro.
//   5. El hilo principal espera a los 4 con pthread_join.
//
// POR QUE NO SE NECESITA MUTEX
//   - "entrada" es compartida pero SOLO se lee (nadie la modifica).
//   - "salida" es compartida, pero cada hilo escribe UNICAMENTE en los
//     pixeles de su cuadrante y los cuadrantes no se solapan.
//   => No hay condiciones de carrera.
//   Los pixeles en la frontera entre cuadrantes leen vecinos de otro
//   cuadrante en "entrada", lo cual es seguro porque es solo lectura.
//   Por eso el resultado es identico al de la version secuencial.
//
// FILTROS ENCADENADOS
//   La salida de un filtro es la entrada del siguiente. Antes de pasar al
//   siguiente filtro hay que esperar a que los 4 hilos terminen (pthread_join
//   funciona como barrera), porque un hilo necesita los pixeles de frontera
//   ya filtrados por sus vecinos.

#include <pthread.h>
#include <iostream>
#include <cstring>
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
#define NUM_HILOS   4     // una region (cuadrante) por hilo


// -------------------------------------------------------------
// Datos que recibe cada hilo
// -------------------------------------------------------------
struct TareaHilo {
    int id;
    const char* nombreRegion;
    const Filter* filtro;
    const Image* entrada;    // compartida, solo lectura
    Image* salida;           // compartida, cada hilo escribe solo su region
    Region region;

    // Resultados que llena el hilo
    double msTotal;
    double msCPU;
};


// Tiempo de CPU consumido SOLO por el hilo que llama esta funcion.
// (std::clock suma la CPU de todos los hilos del proceso)
static double tiempoCPUHiloMs() {
#ifdef CLOCK_THREAD_CPUTIME_ID
    timespec ts;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1.0e6;
#else
    return 0.0;   // el sistema no permite medir CPU por hilo
#endif
}


// -------------------------------------------------------------
// Funcion que ejecuta cada hilo
// -------------------------------------------------------------
static void* trabajoHilo(void* arg) {
    TareaHilo* tarea = (TareaHilo*) arg;

    Timer t;
    t.iniciar();
    double cpuInicio = tiempoCPUHiloMs();

    tarea->filtro->aplicar(*tarea->entrada, *tarea->salida, tarea->region);

    tarea->msCPU = tiempoCPUHiloMs() - cpuInicio;
    t.detener();
    tarea->msTotal = t.getTiempoTotalMs();

    return nullptr;
}


// -------------------------------------------------------------
// Divide la imagen en 4 cuadrantes. Si el ancho o el alto son impares,
// la mitad de abajo / derecha queda con una fila / columna mas.
// -------------------------------------------------------------
static void dividirEnCuadrantes(int alto, int ancho, Region regiones[NUM_HILOS]) {
    int mitadF = alto / 2;
    int mitadC = ancho / 2;

    Region arribaIzq = {0,      mitadF, 0,      mitadC};
    Region arribaDer = {0,      mitadF, mitadC, ancho};
    Region abajoIzq  = {mitadF, alto,   0,      mitadC};
    Region abajoDer  = {mitadF, alto,   mitadC, ancho};

    regiones[0] = arribaIzq;
    regiones[1] = arribaDer;
    regiones[2] = abajoIzq;
    regiones[3] = abajoDer;
}


static Filter* crearFiltro(const char* nombre) {
    if (strcmp(nombre, "blur") == 0)    return new BlurFilter();
    if (strcmp(nombre, "laplace") == 0) return new LaplaceFilter();
    if (strcmp(nombre, "sharpen") == 0) return new SharpenFilter();
    return nullptr;
}


static void mostrarUso(const char* programa) {
    cerr << "Uso: " << programa
         << " <entrada|-> <salida|-> --f <blur|laplace|sharpen> [--f <filtro> ...] [--ascii|--binary]"
         << endl;
}


static void liberarFiltros(Filter* filtros[], int cantidad) {
    for (int i = 0; i < cantidad; i++) delete filtros[i];
}


int main(int argc, char* argv[]) {
    if (argc < 5) {
        mostrarUso(argv[0]);
        return 1;
    }

    const char* rutaEntrada = argv[1];
    const char* rutaSalida = argv[2];

    Filter* filtros[MAX_FILTROS];
    int cantidadFiltros = 0;
    bool forzarAscii = false;
    bool forzarBinario = false;

    // -------------------------------------------------------------
    // Argumentos
    // -------------------------------------------------------------
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--f") == 0) {
            if (i + 1 >= argc) {
                cerr << "Error: --f necesita un nombre de filtro." << endl;
                liberarFiltros(filtros, cantidadFiltros);
                return 1;
            }
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

    if (cantidadFiltros == 0) {
        cerr << "Error: debe indicar al menos un filtro con --f." << endl;
        return 1;
    }
    if (forzarAscii && forzarBinario) {
        cerr << "Error: no puede usar --ascii y --binary al mismo tiempo." << endl;
        liberarFiltros(filtros, cantidadFiltros);
        return 1;
    }

    // Si la imagen sale por stdout, la informacion va a stderr
    bool salidaEstandar = (strcmp(rutaSalida, "-") == 0);
    ostream& info = salidaEstandar ? cerr : cout;

    // -------------------------------------------------------------
    // LECTURA
    // -------------------------------------------------------------
    Timer tLectura;
    tLectura.iniciar();
    bool binarioEntrada = false;
    Image* actual = ImageIO::leerArchivo(rutaEntrada, binarioEntrada);
    tLectura.detener();

    if (actual == nullptr) {
        liberarFiltros(filtros, cantidadFiltros);
        return 1;
    }

    bool binarioSalida = binarioEntrada;
    if (forzarAscii)   binarioSalida = false;
    if (forzarBinario) binarioSalida = true;

    info << "Imagen:        " << rutaEntrada << endl;
    info << "Tipo:          " << actual->getTipo() << endl;
    info << "Dimensiones:   " << actual->getAncho() << " x " << actual->getAlto() << endl;
    info << "Valor maximo:  " << actual->getValorMaximo() << endl;
    info << "Canales:       " << actual->getCanales() << endl;
    info << "Hilos:         " << NUM_HILOS << " (pthreads, un cuadrante por hilo)" << endl;
    info << "Filtros:       ";
    for (int i = 0; i < cantidadFiltros; i++) {
        if (i > 0) info << " -> ";
        info << filtros[i]->getNombre();
    }
    info << endl;
    info << "Lectura:       " << tLectura.getTiempoTotalMs() << " ms total, "
         << tLectura.getTiempoCPUMs() << " ms CPU" << endl;

    // -------------------------------------------------------------
    // Regiones: se calculan una sola vez (el tamanio no cambia)
    // -------------------------------------------------------------
    const char* nombresRegion[NUM_HILOS] = {
        "arriba-izquierda", "arriba-derecha", "abajo-izquierda", "abajo-derecha"
    };
    Region regiones[NUM_HILOS];
    dividirEnCuadrantes(actual->getAlto(), actual->getAncho(), regiones);

    pthread_t hilos[NUM_HILOS];
    TareaHilo tareas[NUM_HILOS];

    // -------------------------------------------------------------
    // PROCESAMIENTO
    // -------------------------------------------------------------
    Timer tProcesamiento;
    tProcesamiento.iniciar();

    for (int i = 0; i < cantidadFiltros; i++) {
        Image* salida = actual->crearVacia();

        Timer tFiltro;
        tFiltro.iniciar();

        // Crear los 4 hilos: cada uno recibe su region y el filtro
        int creados = 0;
        for (int h = 0; h < NUM_HILOS; h++) {
            tareas[h].id = h;
            tareas[h].nombreRegion = nombresRegion[h];
            tareas[h].filtro = filtros[i];
            tareas[h].entrada = actual;
            tareas[h].salida = salida;
            tareas[h].region = regiones[h];
            tareas[h].msTotal = 0.0;
            tareas[h].msCPU = 0.0;

            if (pthread_create(&hilos[h], nullptr, trabajoHilo, &tareas[h]) != 0) {
                cerr << "Error: no se pudo crear el hilo " << h << endl;
                break;
            }
            creados++;
        }

        // Esperar a que terminen (barrera antes del siguiente filtro)
        for (int h = 0; h < creados; h++) {
            pthread_join(hilos[h], nullptr);
        }

        tFiltro.detener();

        if (creados < NUM_HILOS) {
            delete salida;
            delete actual;
            liberarFiltros(filtros, cantidadFiltros);
            return 1;
        }

        info << endl;
        info << "Filtro " << (i + 1) << " (" << filtros[i]->getNombre() << "): "
             << tFiltro.getTiempoTotalMs() << " ms total, "
             << tFiltro.getTiempoCPUMs() << " ms CPU (suma de los hilos)" << endl;

        info << "  Hilo\tRegion\t\t\tFilas\t\tColumnas\tTotal(ms)\tCPU(ms)" << endl;
        for (int h = 0; h < NUM_HILOS; h++) {
            const Region& r = tareas[h].region;
            info << "  " << tareas[h].id << "\t"
                 << tareas[h].nombreRegion << (strlen(tareas[h].nombreRegion) < 16 ? "\t\t" : "\t")
                 << "[" << r.filaInicio << "," << r.filaFin << ")\t"
                 << "[" << r.colInicio << "," << r.colFin << ")\t"
                 << tareas[h].msTotal << "\t\t"
                 << tareas[h].msCPU << endl;
        }

        // La salida pasa a ser la entrada del siguiente filtro
        delete actual;
        actual = salida;
    }

    tProcesamiento.detener();

    // -------------------------------------------------------------
    // ESCRITURA
    // -------------------------------------------------------------
    Timer tEscritura;
    tEscritura.iniciar();
    bool ok = ImageIO::escribirArchivo(rutaSalida, *actual, binarioSalida);
    tEscritura.detener();

    if (!ok) {
        delete actual;
        liberarFiltros(filtros, cantidadFiltros);
        return 1;
    }

    info << endl;
    info << "Procesamiento: " << tProcesamiento.getTiempoTotalMs() << " ms total, "
         << tProcesamiento.getTiempoCPUMs() << " ms CPU" << endl;
    info << "Escritura:     " << tEscritura.getTiempoTotalMs() << " ms total, "
         << tEscritura.getTiempoCPUMs() << " ms CPU" << endl;
    info << "Salida:        " << rutaSalida << " ("
         << actual->getNumeroMagico(binarioSalida) << ")" << endl;

    delete actual;
    liberarFiltros(filtros, cantidadFiltros);
    return 0;
}
