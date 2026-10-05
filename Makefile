# Makefile - Micro-Proyecto 2
CXX = g++
CXXFLAGS = -std=c++11 -O2 -Wall

COMUNES = Image.o ImageIO.o

all: processor

processor: processor.o $(COMUNES)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $<

processor.o: processor.cpp Image.h ImageIO.h Filter.h Timer.h
Image.o: Image.cpp Image.h
ImageIO.o: ImageIO.cpp ImageIO.h Image.h

clean:
	rm -f *.o processor
