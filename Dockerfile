FROM gcc:16.2


RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    cmake \
    libunwind-dev \
    python3 \
    python3-dev \
    python3-venv \
    curl \
 && rm -rf /var/lib/apt/lists/*

# install poetry
RUN curl -sSL https://install.python-poetry.org | python3 - &&\
    echo "PATH=\"/root/.local/bin:$PATH\"" >> ~/.bashrc


WORKDIR /rebind