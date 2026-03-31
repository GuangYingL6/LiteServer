FROM ubuntu:24.04

RUN apt update && apt install -y \
    g++ \
    cmake \
    make \
    git

WORKDIR /app