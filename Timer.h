// Timer.h - Mide tiempo de CPU y tiempo total (pared)
//
// Tiempo total: lo que pasa en el reloj real (chrono::steady_clock).
// Tiempo CPU:   lo que el procesador trabajo para el proceso (std::clock).
//               Con varios hilos, el tiempo CPU SUMA el de todos los hilos,
//               por eso puede ser mayor que el tiempo total.
// Nota: en Linux y MSYS2 std::clock mide CPU; en Visual Studio mide tiempo real.

#ifndef TIMER_H
#define TIMER_H

#include <chrono>
#include <ctime>

class Timer {
public:
    void iniciar() {
        inicioPared = std::chrono::steady_clock::now();
        inicioCPU = std::clock();
    }

    void detener() {
        finPared = std::chrono::steady_clock::now();
        finCPU = std::clock();
    }

    double getTiempoTotalMs() const {
        return std::chrono::duration<double, std::milli>(finPared - inicioPared).count();
    }

    double getTiempoCPUMs() const {
        return 1000.0 * (finCPU - inicioCPU) / CLOCKS_PER_SEC;
    }

private:
    std::chrono::steady_clock::time_point inicioPared, finPared;
    std::clock_t inicioCPU = 0, finCPU = 0;
};

#endif
