FROM ubuntu:24.04 AS build

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    cmake \
    git \
    libdrogon-dev \
    libjsoncpp-dev \
    librdkafka-dev \
    libpqxx-dev \
    pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

RUN if [ -f frontend/package.json ]; then \
      apt-get update && apt-get install -y --no-install-recommends nodejs npm \
      && cd frontend && npm ci && npm run build \
      && rm -rf /var/lib/apt/lists/*; \
    fi

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build --parallel

FROM ubuntu:24.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    libdrogon1 \
    libjsoncpp25 \
    librdkafka1 \
    libpqxx-7.8 \
    && rm -rf /var/lib/apt/lists/*

FROM runtime AS api
COPY --from=build /src/build/testdip-api /usr/local/bin/testdip-api
COPY --from=build /src/frontend/dist /frontend
ENV TESTDIP_FRONTEND_ROOT=/frontend
EXPOSE 8080
CMD ["testdip-api"]

FROM runtime AS worker
COPY --from=build /src/build/testdip-worker /usr/local/bin/testdip-worker
CMD ["testdip-worker"]
