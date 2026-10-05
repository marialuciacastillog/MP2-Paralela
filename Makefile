CXX = g++

CXXFLAGS = -std=c++11 -O2 -Wall

TARGET = filterer

OBJS = processor.o \
       Image.o \
       ImageIO.o \
       BlurFilter.o \
       LaplaceFilter.o \
       SharpenFilter.o


all: $(TARGET)


$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)


processor.o: processor.cpp \
             Image.h \
             ImageIO.h \
             Filter.h \
             BlurFilter.h \
             LaplaceFilter.h \
             SharpenFilter.h \
             Timer.h
	$(CXX) $(CXXFLAGS) -c processor.cpp


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
	rm -f *.o filterer processor