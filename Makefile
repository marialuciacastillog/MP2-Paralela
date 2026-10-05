CXX = g++
MPICXX = mpic++

CXXFLAGS = -std=c++11 -O2 -Wall

TARGET = filterer
MPI_TARGET = mpi_filterer

# Archivos comunes a todos los disenios
COMUNES = Image.o \
          ImageIO.o \
          BlurFilter.o \
          LaplaceFilter.o \
          SharpenFilter.o


# "make" compila solo la version secuencial (no necesita MPI instalado)
all: $(TARGET)

# "make mpi" compila la version MPI (necesita OpenMPI: se usa dentro de Docker)
mpi: $(MPI_TARGET)


$(TARGET): processor.o $(COMUNES)
	$(CXX) $(CXXFLAGS) -o $(TARGET) processor.o $(COMUNES)

$(MPI_TARGET): mpi_filterer.o $(COMUNES)
	$(MPICXX) $(CXXFLAGS) -o $(MPI_TARGET) mpi_filterer.o $(COMUNES)


processor.o: processor.cpp Image.h ImageIO.h Filter.h BlurFilter.h LaplaceFilter.h SharpenFilter.h Timer.h
	$(CXX) $(CXXFLAGS) -c processor.cpp

mpi_filterer.o: mpi_filterer.cpp Image.h ImageIO.h Filter.h BlurFilter.h LaplaceFilter.h SharpenFilter.h Timer.h
	$(MPICXX) $(CXXFLAGS) -c mpi_filterer.cpp

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
	rm -f *.o $(TARGET) $(MPI_TARGET) processor processor.exe

.PHONY: all mpi clean
