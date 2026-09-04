FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive

# 1. OS Dependencies
RUN apt-get update && apt-get install -y \
    build-essential cmake curl git wget tar unzip make \
    python3 python3-dev pkg-config libffi-dev libgmp-dev zlib1g-dev gdb
    
# 2. Install PyPy
RUN wget https://downloads.python.org/pypy/pypy3.11-v7.3.21-linux64.tar.bz2 && \
    tar -xjf pypy3.11-v7.3.21-linux64.tar.bz2 && \
    mv pypy3.11-v7.3.21-linux64 /opt/pypy && \
    ln -s /opt/pypy/bin/pypy3 /usr/local/bin/pypy3 && \
    rm pypy3.11-v7.3.21-linux64.tar.bz2

# 3. Environment Variables
ENV PATH="/opt/pypy/bin:$PATH"
ENV PYTHONPATH="/vcml-pydrofoil"
ENV LD_LIBRARY_PATH=/vcml-pydrofoil/pypy-pydrofoil-scripting-experimental/bin:/vcml-pydrofoil:/vcml-pydrofoil/build:/vcml-pydrofoil/sysc_vp:${LD_LIBRARY_PATH:-}

# 4. Arbeitsverzeichnis setzen (bleibt im Image erstmal leer)
WORKDIR /vcml-pydrofoil