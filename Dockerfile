# Dockerfile - Disenio 4: cada contenedor creado con esta imagen es un "nodo"
#
# Contiene: Ubuntu + g++ + OpenMPI + SSH + el programa mpi_filterer compilado.
# SSH es lo que usa mpirun para lanzar procesos en los otros contenedores.

FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
        g++ make openmpi-bin libopenmpi-dev openssh-server openssh-client && \
    rm -rf /var/lib/apt/lists/*

# OpenMPI no se debe ejecutar como root, asi que se crea un usuario normal
RUN useradd -m -s /bin/bash mpiuser && mkdir -p /run/sshd

USER mpiuser

# Llave SSH sin contrasenia. Como todos los contenedores salen de la misma
# imagen, todos tienen la misma llave y se pueden conectar entre si.
RUN mkdir -p /home/mpiuser/.ssh && chmod 700 /home/mpiuser/.ssh && \
    ssh-keygen -t ed25519 -N "" -f /home/mpiuser/.ssh/id_ed25519 && \
    cp /home/mpiuser/.ssh/id_ed25519.pub /home/mpiuser/.ssh/authorized_keys && \
    printf "Host *\n  StrictHostKeyChecking no\n  UserKnownHostsFile /dev/null\n  LogLevel ERROR\n" \
        > /home/mpiuser/.ssh/config && \
    chmod 600 /home/mpiuser/.ssh/authorized_keys /home/mpiuser/.ssh/config

WORKDIR /home/mpiuser/app

# Se copia el codigo y se compila DENTRO del contenedor (Linux)
COPY --chown=mpiuser:mpiuser *.cpp *.h Makefile ./

# Si los archivos vienen de Windows, se quitan los saltos de linea \r
RUN sed -i 's/\r$//' Makefile *.cpp *.h && make clean && make mpi

# hostfile: lista de nodos donde mpirun puede lanzar procesos
RUN printf "nodo1 slots=1\nnodo2 slots=1\nnodo3 slots=1\n" > hostfile

# Atajo para no escribir todas las opciones de mpirun cada vez
RUN printf '#!/bin/bash\nmpirun --hostfile hostfile --mca btl tcp,self --mca btl_tcp_if_include eth0 --mca oob_tcp_if_include eth0 "$@"\n' \
        > mpi.sh && chmod +x mpi.sh && mkdir -p output

# El contenedor queda encendido con el servidor SSH esperando conexiones
USER root
CMD ["/usr/sbin/sshd", "-D"]
