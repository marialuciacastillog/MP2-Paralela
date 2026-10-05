// mpi_filterer.cpp - Disenio 4: Memoria distribuida con MPI
//
// Uso (igual que filterer, pero lanzado con mpirun):
//   mpirun -np 3 --hostfile hostfile ./mpi_filterer entrada salida --f blur
//   mpirun -np 3 --hostfile hostfile ./mpi_filterer entrada salida --f blur --f sharpen
//
// IDEA GENERAL
//   Cada proceso (nodo) tiene su PROPIA memoria. Nadie ve la imagen de otro;
//   los datos solo se comparten enviando mensajes.
//
//   1. El rank 0 (coordinador) lee la imagen. Es el UNICO que toca archivos.
//   2. Envia a todos el encabezado (ancho, alto, canales...) con MPI_Bcast.
//   3. Reparte la imagen por bloques de filas con MPI_Scatterv.
//   4. Cada proceso pide a sus vecinos una fila extra arriba y abajo
//      (filas "fantasma" o halo), porque los filtros 3x3 necesitan
//      la fila anterior y la siguiente de cada pixel.
//   5. Cada proceso aplica los filtros a SUS filas, con las mismas clases
//      BlurFilter, LaplaceFilter y SharpenFilter del Disenio 2.
//      Si hay varios filtros encadenados, despues de cada uno se vuelven
//      a intercambiar las filas fantasma con los vecinos.
//   6. El rank 0 junta los bloques con MPI_Gatherv y escribe el resultado.
//   7. Cada nodo mide su tiempo y el rank 0 imprime la tabla de todos.
//
// POR QUE FUNCIONAN LOS FILTROS SIN CAMBIOS
//   Cada proceso arma una imagen local = [halo arriba] + [sus filas] + [halo abajo].
//   Los filtros tratan como borde la fila 0 y la ultima de la imagen que reciben.
//     - En el rank 0 la fila local 0 es la fila 0 real (borde verdadero).
//     - En el ultimo rank la ultima fila local es la ultima real (borde verdadero).
//     - En los demas, la fila local 0 y la ultima son halo y NO se procesan
//       (la Region solo cubre las filas propias).
//   Asi el resultado es identico al de la version secuencial.

#include <mpi.h>
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

#define MAX_FILTROS 32
#define MAX_NOMBRE 64

// Posiciones del arreglo que se envia con MPI_Bcast
#define H_ANCHO   0
#define H_ALTO    1
#define H_MAXIMO  2
#define H_CANALES 3
#define H_BINARIO 4
#define H_ERROR   5
#define H_TOTAL   6

// Posiciones del arreglo de tiempos de cada nodo
#define T_FILTRO_TOTAL 0
#define T_FILTRO_CPU   1
#define T_COMUNICACION 2
#define T_FILAS        3
#define T_TOTAL        4


static Filter* crearFiltro(const char* nombre) {
    if (strcmp(nombre, "blur") == 0)    return new BlurFilter();
    if (strcmp(nombre, "laplace") == 0) return new LaplaceFilter();
    if (strcmp(nombre, "sharpen") == 0) return new SharpenFilter();
    return nullptr;
}


static Image* crearImagen(int ancho, int alto, int maximo, int canales) {
    if (canales == 3) {
        return new PPMImage(ancho, alto, maximo);
    }
    return new PGMImage(ancho, alto, maximo);
}


// Intercambia las filas fantasma con los vecinos de arriba y abajo.
//   - Mi PRIMERA fila propia se la mando al vecino de arriba (su halo de abajo).
//   - Mi ULTIMA fila propia se la mando al vecino de abajo (su halo de arriba).
// MPI_Sendrecv envia y recibe al mismo tiempo, asi nadie se queda bloqueado.
static void intercambiarHalos(Image& local, int haloArriba, int filasPropias,
                              int rank, int size) {
    int valoresFila = local.getAncho() * local.getCanales();
    int* datos = local.getDatos();

    int* primeraPropia = datos + haloArriba * valoresFila;
    int* ultimaPropia  = datos + (haloArriba + filasPropias - 1) * valoresFila;
    int* haloSuperior  = datos;
    int* haloInferior  = datos + (haloArriba + filasPropias) * valoresFila;

    int arriba = (rank > 0)        ? rank - 1 : MPI_PROC_NULL;
    int abajo  = (rank < size - 1) ? rank + 1 : MPI_PROC_NULL;

    // Mando mi ultima fila hacia abajo y recibo el halo de arriba
    MPI_Sendrecv(ultimaPropia, valoresFila, MPI_INT, abajo, 0,
                 haloSuperior, valoresFila, MPI_INT, arriba, 0,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    // Mando mi primera fila hacia arriba y recibo el halo de abajo
    MPI_Sendrecv(primeraPropia, valoresFila, MPI_INT, arriba, 1,
                 haloInferior, valoresFila, MPI_INT, abajo, 1,
                 MPI_COMM_WORLD, MPI_STATUS_IGNORE);
}


int main(int argc, char* argv[]) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    char nombreNodo[MPI_MAX_PROCESSOR_NAME];
    int largoNombre;
    MPI_Get_processor_name(nombreNodo, &largoNombre);

    double inicioTotal = MPI_Wtime();
    double tiempoComunicacion = 0.0;
    double marca;

    // -------------------------------------------------------------
    // Argumentos (todos los procesos los leen, son los mismos para todos)
    // -------------------------------------------------------------
    if (argc < 5) {
        if (rank == 0) {
            cerr << "Uso: mpirun -np N " << argv[0]
                 << " <entrada> <salida> --f <blur|laplace|sharpen> [--f ...] [--ascii|--binary]"
                 << endl;
        }
        MPI_Finalize();
        return 1;
    }

    const char* rutaEntrada = argv[1];
    const char* rutaSalida = argv[2];

    Filter* filtros[MAX_FILTROS];
    int cantidadFiltros = 0;
    bool forzarAscii = false;
    bool forzarBinario = false;
    bool argumentosOk = true;

    for (int i = 3; i < argc && argumentosOk; i++) {
        if (strcmp(argv[i], "--f") == 0 && i + 1 < argc && cantidadFiltros < MAX_FILTROS) {
            Filter* f = crearFiltro(argv[i + 1]);
            if (f == nullptr) {
                if (rank == 0) cerr << "Error: filtro no soportado: " << argv[i + 1] << endl;
                argumentosOk = false;
            } else {
                filtros[cantidadFiltros] = f;
                cantidadFiltros++;
            }
            i++;
        } else if (strcmp(argv[i], "--ascii") == 0) {
            forzarAscii = true;
        } else if (strcmp(argv[i], "--binary") == 0) {
            forzarBinario = true;
        } else {
            if (rank == 0) cerr << "Error: opcion no reconocida: " << argv[i] << endl;
            argumentosOk = false;
        }
    }

    if (argumentosOk && cantidadFiltros == 0) {
        if (rank == 0) cerr << "Error: debe indicar al menos un filtro con --f." << endl;
        argumentosOk = false;
    }

    if (!argumentosOk) {
        for (int i = 0; i < cantidadFiltros; i++) delete filtros[i];
        MPI_Finalize();
        return 1;
    }

    // -------------------------------------------------------------
    // 1. El rank 0 lee la imagen
    // -------------------------------------------------------------
    Image* completa = nullptr;
    int encabezado[H_TOTAL] = {0, 0, 0, 0, 0, 0};
    Timer tLectura;

    if (rank == 0) {
        tLectura.iniciar();
        bool binario = false;
        completa = ImageIO::leerArchivo(rutaEntrada, binario);
        tLectura.detener();

        if (completa == nullptr) {
            encabezado[H_ERROR] = 1;
        } else if (completa->getAlto() < size) {
            cerr << "Error: hay mas procesos (" << size << ") que filas ("
                 << completa->getAlto() << ")." << endl;
            encabezado[H_ERROR] = 1;
        } else {
            encabezado[H_ANCHO]   = completa->getAncho();
            encabezado[H_ALTO]    = completa->getAlto();
            encabezado[H_MAXIMO]  = completa->getValorMaximo();
            encabezado[H_CANALES] = completa->getCanales();
            encabezado[H_BINARIO] = binario ? 1 : 0;
        }
    }

    // Los demas procesos esperan aqui mientras el rank 0 lee el archivo.
    // Esa espera NO se cuenta como comunicacion (no se esta enviando nada).
    MPI_Barrier(MPI_COMM_WORLD);

    // -------------------------------------------------------------
    // 2. Todos reciben el encabezado
    // -------------------------------------------------------------
    marca = MPI_Wtime();
    MPI_Bcast(encabezado, H_TOTAL, MPI_INT, 0, MPI_COMM_WORLD);
    tiempoComunicacion += MPI_Wtime() - marca;

    if (encabezado[H_ERROR] == 1) {
        delete completa;
        for (int i = 0; i < cantidadFiltros; i++) delete filtros[i];
        MPI_Finalize();
        return 1;
    }

    int ancho   = encabezado[H_ANCHO];
    int alto    = encabezado[H_ALTO];
    int maximo  = encabezado[H_MAXIMO];
    int canales = encabezado[H_CANALES];
    int valoresFila = ancho * canales;

    // -------------------------------------------------------------
    // 3. Reparto de filas: si no es division exacta, los primeros
    //    procesos reciben una fila mas.
    // -------------------------------------------------------------
    int* filasPorProceso = new int[size];
    int* cantidades = new int[size];      // valores (int) por proceso
    int* desplazamientos = new int[size]; // desde donde empieza cada bloque

    int base = alto / size;
    int sobrantes = alto % size;
    int acumulado = 0;

    for (int p = 0; p < size; p++) {
        filasPorProceso[p] = base + (p < sobrantes ? 1 : 0);
        cantidades[p] = filasPorProceso[p] * valoresFila;
        desplazamientos[p] = acumulado;
        acumulado += cantidades[p];
    }

    int misFilas = filasPorProceso[rank];
    int haloArriba = (rank > 0) ? 1 : 0;
    int haloAbajo  = (rank < size - 1) ? 1 : 0;
    int altoLocal  = haloArriba + misFilas + haloAbajo;

    Image* local = crearImagen(ancho, altoLocal, maximo, canales);

    // Las filas propias se reciben justo despues del halo de arriba
    int* destino = local->getDatos() + haloArriba * valoresFila;
    int* origen = (rank == 0) ? completa->getDatos() : nullptr;

    marca = MPI_Wtime();
    MPI_Scatterv(origen, cantidades, desplazamientos, MPI_INT,
                 destino, cantidades[rank], MPI_INT,
                 0, MPI_COMM_WORLD);

    // -------------------------------------------------------------
    // 4. Primer intercambio de filas fantasma
    // -------------------------------------------------------------
    intercambiarHalos(*local, haloArriba, misFilas, rank, size);
    tiempoComunicacion += MPI_Wtime() - marca;

    // -------------------------------------------------------------
    // 5. Aplicar los filtros solo a las filas propias
    // -------------------------------------------------------------
    Region misFilasRegion = {haloArriba, haloArriba + misFilas, 0, ancho};

    Timer tFiltros;
    tFiltros.iniciar();
    double comunicacionEntreFiltros = 0.0;

    for (int i = 0; i < cantidadFiltros; i++) {
        Image* salida = local->crearVacia();
        filtros[i]->aplicar(*local, *salida, misFilasRegion);
        delete local;
        local = salida;

        // Si viene otro filtro, los vecinos necesitan las filas ya filtradas
        if (i < cantidadFiltros - 1) {
            marca = MPI_Wtime();
            intercambiarHalos(*local, haloArriba, misFilas, rank, size);
            comunicacionEntreFiltros += MPI_Wtime() - marca;
        }
    }

    tFiltros.detener();
    tiempoComunicacion += comunicacionEntreFiltros;

    // -------------------------------------------------------------
    // 6. El rank 0 junta todos los bloques
    // -------------------------------------------------------------
    Image* resultado = nullptr;
    int* destinoFinal = nullptr;

    if (rank == 0) {
        resultado = crearImagen(ancho, alto, maximo, canales);
        destinoFinal = resultado->getDatos();
    }

    marca = MPI_Wtime();
    MPI_Gatherv(local->getDatos() + haloArriba * valoresFila, cantidades[rank], MPI_INT,
                destinoFinal, cantidades, desplazamientos, MPI_INT,
                0, MPI_COMM_WORLD);
    tiempoComunicacion += MPI_Wtime() - marca;

    // -------------------------------------------------------------
    // 7. Tiempos de cada nodo
    // -------------------------------------------------------------
    double misTiempos[T_TOTAL];
    misTiempos[T_FILTRO_TOTAL] = tFiltros.getTiempoTotalMs() - comunicacionEntreFiltros * 1000.0;
    misTiempos[T_FILTRO_CPU]   = tFiltros.getTiempoCPUMs();
    misTiempos[T_COMUNICACION] = tiempoComunicacion * 1000.0;
    misTiempos[T_FILAS]        = misFilas;

    double* todosTiempos = nullptr;
    char* todosNombres = nullptr;
    char miNombre[MAX_NOMBRE];
    strncpy(miNombre, nombreNodo, MAX_NOMBRE - 1);
    miNombre[MAX_NOMBRE - 1] = '\0';

    if (rank == 0) {
        todosTiempos = new double[size * T_TOTAL];
        todosNombres = new char[size * MAX_NOMBRE];
    }

    MPI_Gather(misTiempos, T_TOTAL, MPI_DOUBLE, todosTiempos, T_TOTAL, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Gather(miNombre, MAX_NOMBRE, MPI_CHAR, todosNombres, MAX_NOMBRE, MPI_CHAR, 0, MPI_COMM_WORLD);

    // -------------------------------------------------------------
    // 8. El rank 0 escribe la imagen y el reporte
    // -------------------------------------------------------------
    int codigoSalida = 0;

    if (rank == 0) {
        bool binarioSalida = (encabezado[H_BINARIO] == 1);
        if (forzarAscii)   binarioSalida = false;
        if (forzarBinario) binarioSalida = true;

        Timer tEscritura;
        tEscritura.iniciar();
        bool ok = ImageIO::escribirArchivo(rutaSalida, *resultado, binarioSalida);
        tEscritura.detener();
        if (!ok) codigoSalida = 1;

        double tiempoTotal = (MPI_Wtime() - inicioTotal) * 1000.0;

        cout << "Imagen:        " << rutaEntrada << endl;
        cout << "Tipo:          " << resultado->getTipo() << endl;
        cout << "Dimensiones:   " << ancho << " x " << alto << endl;
        cout << "Procesos MPI:  " << size << endl;
        cout << "Filtros:       ";
        for (int i = 0; i < cantidadFiltros; i++) {
            if (i > 0) cout << " -> ";
            cout << filtros[i]->getNombre();
        }
        cout << endl << endl;

        cout << "Rank\tNodo\t\tFilas\tFiltro(ms)\tCPU(ms)\tComunicacion(ms)" << endl;
        for (int p = 0; p < size; p++) {
            double* t = todosTiempos + p * T_TOTAL;
            cout << p << "\t" << (todosNombres + p * MAX_NOMBRE) << "\t\t"
                 << (int) t[T_FILAS] << "\t"
                 << t[T_FILTRO_TOTAL] << "\t\t"
                 << t[T_FILTRO_CPU] << "\t"
                 << t[T_COMUNICACION] << endl;
        }

        cout << endl;
        cout << "Lectura (rank 0):   " << tLectura.getTiempoTotalMs() << " ms total, "
             << tLectura.getTiempoCPUMs() << " ms CPU" << endl;
        cout << "Escritura (rank 0): " << tEscritura.getTiempoTotalMs() << " ms total, "
             << tEscritura.getTiempoCPUMs() << " ms CPU" << endl;
        cout << "Tiempo total:       " << tiempoTotal << " ms" << endl;
        cout << "Salida:             " << rutaSalida << " ("
             << resultado->getNumeroMagico(binarioSalida) << ")" << endl;
    }

    // -------------------------------------------------------------
    // Liberar memoria
    // -------------------------------------------------------------
    delete completa;
    delete resultado;
    delete local;
    delete[] filasPorProceso;
    delete[] cantidades;
    delete[] desplazamientos;
    delete[] todosTiempos;
    delete[] todosNombres;
    for (int i = 0; i < cantidadFiltros; i++) delete filtros[i];

    MPI_Finalize();
    return codigoSalida;
}
