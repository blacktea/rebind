FROM ubuntu:24.04 AS gcc-builder

ARG DEBIAN_FRONTEND=noninteractive
ARG GCC_GIT_REF=master
ARG GCC_INSTALL_PREFIX=/opt/gcc-trunk

RUN apt-get update && apt-get install -y --no-install-recommends \
    bison \
    build-essential \
    ca-certificates \
    flex \
    gawk \
    git \
    libgmp-dev \
    libisl-dev \
    libmpc-dev \
    libmpfr-dev \
    python3 \
    texinfo \
    wget \
    xz-utils \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /tmp

# GCC trunk is the active development line in the git repository.
RUN git clone --depth 1 --branch "${GCC_GIT_REF}" https://gcc.gnu.org/git/gcc.git gcc

WORKDIR /tmp/gcc

RUN ./contrib/download_prerequisites

RUN mkdir /tmp/gcc-build

WORKDIR /tmp/gcc-build

RUN /tmp/gcc/configure \
    --prefix="${GCC_INSTALL_PREFIX}" \
    --disable-multilib \
    --enable-languages=c,c++ \
 && make -j"$(nproc)" \
 && make install-strip


FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
ARG GCC_INSTALL_PREFIX=/opt/gcc-trunk

WORKDIR /rebind

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    cmake \
    libunwind-dev \
    python3-dev \
    python3-venv \
    curl \
 && rm -rf /var/lib/apt/lists/*

# install poetry
RUN curl -sSL https://install.python-poetry.org | python3 - &&\
    echo "PATH=\"/root/.local/bin:$PATH\"" >> ~/.bashrc

COPY --from=gcc-builder "${GCC_INSTALL_PREFIX}" "${GCC_INSTALL_PREFIX}"

ENV PATH="${GCC_INSTALL_PREFIX}/bin:${PATH}"
ENV LD_LIBRARY_PATH="${GCC_INSTALL_PREFIX}/lib64:${GCC_INSTALL_PREFIX}/lib:${LD_LIBRARY_PATH}"
ENV CC="${GCC_INSTALL_PREFIX}/bin/gcc"
ENV CXX="${GCC_INSTALL_PREFIX}/bin/g++"
