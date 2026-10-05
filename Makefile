# Makefile - Disenio 3: Memoria compartida (pthreads y OpenMP)
#
#   make           compila los tres programas: filterer, th_filterer, omp_filterer
#   make th        solo la version con pthreads
#   make omp       solo la version con OpenMP
#   make clean     borra objetos y ejecutables

CXX = g++
CXXFLAGS = -std=c++11 -O2 -Wall

# ---------------------------------------------------------------
# Banderas de hilos y de OpenMP segun el sistema operativo
#   Linux / Windows (MSYS2):  -pthread  y  -fopenmp
#   macOS (Apple clang):      no trae OpenMP; se usa libomp de Homebrew
#                             (brew install libomp)
# ---------------------------------------------------------------
PTHREAD_FLAGS = -pthread
OMP_CXXFLAGS = -fopenmp
OMP_LDFLAGS = -fopenmp

UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    LIBOMP := $(shell brew --prefix libomp 2>/dev/null)
    OMP_CXXFLAGS = -Xpreprocessor -fopenmp -I$(LIBOMP)/include
    OMP_LDFLAGS = -L$(LIBOMP)/lib -lomp
endif

TARGET = filterer
TH_TARGET = th_filterer
OMP_TARGET = omp_filterer

# Archivos comunes a todos los disenios (los filtros NO cambian)
COMUNES = Image.o \
          ImageIO.o \
          BlurFilter.o \
          LaplaceFilter.o \
          SharpenFilter.o

HEADERS = Image.h ImageIO.h Filter.h BlurFilter.h LaplaceFilter.h SharpenFilter.h Timer.h


all: $(TARGET) $(TH_TARGET) $(OMP_TARGET)

th: $(TH_TARGET)

omp: $(OMP_TARGET)


# Disenio 2: secuencial (se deja para comparar tiempos)
$(TARGET): processor.o $(COMUNES)
	$(CXX) $(CXXFLAGS) -o $(TARGET) processor.o $(COMUNES)

# Disenio 3: pthreads (4 cuadrantes)
$(TH_TARGET): th_filterer.o $(COMUNES)
	$(CXX) $(CXXFLAGS) $(PTHREAD_FLAGS) -o $(TH_TARGET) th_filterer.o $(COMUNES)

# Disenio 3: OpenMP
$(OMP_TARGET): omp_filterer.o $(COMUNES)
	$(CXX) $(CXXFLAGS) -o $(OMP_TARGET) omp_filterer.o $(COMUNES) $(OMP_LDFLAGS)


processor.o: processor.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c processor.cpp

th_filterer.o: th_filterer.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) $(PTHREAD_FLAGS) -c th_filterer.cpp

omp_filterer.o: omp_filterer.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) $(OMP_CXXFLAGS) -c omp_filterer.cpp

Image.o: Image.cpp Image.h
	$(CXX) $(CXXFLAGS) -c Image.cpp

ImageIO.o: ImageIO.cpp ImageIO.h Image.h
	$(CXX) $(CXXFLAGS) -c ImageIO.cpp

BlurFilter.o: BlurFilter.cpp BlurFilter.h Filter.h Image.h
	$(CXX) $(CXXFLAGS) -c BlurFilter.cpp

LaplaceFilter.o: LaplaceFilter.cpp LaplaceFilter.h Filter.h Image.h
	$(CXX) $(CXXFLAGS) -c LaplaceFilter.cpp

SharpenFilter.o: SharpenFilter.cpp SharpenFilter.h Filter.h Image.h
	$(CXX) $(CXXFLAGS) -c SharpenFilter.cpp


clean:
	rm -f *.o $(TARGET) $(TH_TARGET) $(OMP_TARGET) processor processor.exe *.exe

.PHONY: all th omp clean
