FROM gcc:latest

WORKDIR /rebind

RUN apt-get update && apt-get -y install \
cmake \
python3-dev \
build-essential