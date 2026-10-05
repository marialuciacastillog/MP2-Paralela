// Image.h - Disenio 1: Aplicacion base
//
// Jerarquia de clases para imagenes Netpbm:
//
//            Image  (abstracta)
//           |      |
//     PGMImage   PPMImage
//     (1 canal)  (3 canales: R, G, B)
//
// Los pixeles se guardan en UN SOLO arreglo plano (int*), fila por fila.
// En PPM los canales van intercalados: R G B R G B ...
//   indice(fila, col, canal) = (fila * ancho + col) * canales + canal
//
// Ventajas del arreglo plano frente a vector<vector<...>>:
//   - Memoria contigua: mejor uso de cache.
//   - Facil de partir en regiones para hilos (Disenio 3) y OpenMP.
//   - Se puede enviar directo con MPI_Send/MPI_Scatter (Disenio 4).

#ifndef IMAGE_H
#define IMAGE_H

class Image {
public:
    Image(int ancho, int alto, int valorMaximo, int canales);
    Image(const Image& otra);               // copia profunda
    Image& operator=(const Image& otra);
    virtual ~Image();

    int getAncho() const       { return ancho; }
    int getAlto() const        { return alto; }
    int getValorMaximo() const { return valorMaximo; }
    int getCanales() const     { return canales; }
    int getTotalValores() const { return ancho * alto * canales; }

    // Acceso a un pixel (canal = 0 para PGM; 0,1,2 = R,G,B para PPM)
    int  get(int fila, int col, int canal = 0) const {
        return datos[(fila * ancho + col) * canales + canal];
    }
    void set(int fila, int col, int canal, int valor) {
        datos[(fila * ancho + col) * canales + canal] = valor;
    }

    // Acceso directo al arreglo (para hilos, OpenMP y MPI)
    int*       getDatos()       { return datos; }
    const int* getDatos() const { return datos; }

    // Limita un valor al rango valido [0, valorMaximo]
    int recortar(int valor) const;

    // Numero magico segun el tipo de imagen ("P2"/"P5" o "P3"/"P6")
    virtual const char* getNumeroMagico(bool binario) const = 0;
    virtual const char* getTipo() const = 0;

    // Crea una imagen del mismo tipo y tamanio, con pixeles en 0
    // (sirve como imagen de salida de un filtro)
    virtual Image* crearVacia() const = 0;
    virtual Image* clonar() const = 0;

protected:
    int ancho;
    int alto;
    int valorMaximo;
    int canales;
    int* datos;
};


class PGMImage : public Image {
public:
    PGMImage(int ancho, int alto, int valorMaximo);
    const char* getNumeroMagico(bool binario) const { return binario ? "P5" : "P2"; }
    const char* getTipo() const { return "PGM (escala de grises)"; }
    Image* crearVacia() const;
    Image* clonar() const;
};


class PPMImage : public Image {
public:
    PPMImage(int ancho, int alto, int valorMaximo);
    const char* getNumeroMagico(bool binario) const { return binario ? "P6" : "P3"; }
    const char* getTipo() const { return "PPM (color)"; }
    Image* crearVacia() const;
    Image* clonar() const;
};

#endif
