# syntax=docker/dockerfile:1.7

ARG UBUNTU_VERSION=24.04

FROM --platform=$BUILDPLATFORM ubuntu:${UBUNTU_VERSION} AS build

ARG DEBIAN_FRONTEND=noninteractive
ARG PREBYTE_VERSION=1.0.3

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        ca-certificates \
        liblua5.4-dev \
        ninja-build \
        pkg-config \
        python3 \
        python3-venv \
    && rm -rf /var/lib/apt/lists/*

RUN python3 -m venv /opt/prebyte-build-env \
    && /opt/prebyte-build-env/bin/pip install --no-cache-dir cmake==3.31.10

ENV PATH="/opt/prebyte-build-env/bin:${PATH}"

WORKDIR /src
COPY . .

RUN cmake -S . -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DPREBYTE_BUILD_TESTS=OFF \
        -DPREBYTE_BUILD_BENCHMARKS=OFF \
    && cmake --build build --parallel \
    && cmake --install build --prefix /opt/prebyte

FROM ubuntu:${UBUNTU_VERSION} AS runtime

ARG DEBIAN_FRONTEND=noninteractive
ARG PREBYTE_VERSION=1.0.3
ARG BUILD_DATE

RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        ca-certificates \
        liblua5.4-0 \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --create-home --uid 10001 prebyte

COPY --from=build /opt/prebyte/ /usr/local/

LABEL org.opencontainers.image.title="Prebyte" \
      org.opencontainers.image.description="Templating CLI and C++ library" \
      org.opencontainers.image.version="${PREBYTE_VERSION}" \
      org.opencontainers.image.created="${BUILD_DATE}"

USER prebyte
WORKDIR /work

ENTRYPOINT ["/usr/local/bin/prebyte"]
CMD ["-h"]
