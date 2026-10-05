# Mini-Proyecto 2 - Programación Paralela (2026-II)

Filtros de imágenes PPM/PGM (blur, laplace, sharpen) en C++,
implementados de cuatro formas. Cada diseño está en su propia rama:

| Rama       | Diseño                              | Programa                       |
|------------|-------------------------------------|--------------------------------|
| design-1   | Aplicación base (leer/escribir)     | `processor`                    |
| design-2   | Versión secuencial                  | `filterer`                     |
| design-3   | Memoria compartida: pthreads y OpenMP | `th_filterer`, `omp_filterer` |
| design-4   | Memoria distribuida: MPI + Docker   | `mpi_filterer`                 |

Para ver un diseño: `git switch design-3` y luego `make`.

Integrantes: Maria Lucía Castillo García, Juliana González Sánchez, Ana Daniela Paredes Tovar.
