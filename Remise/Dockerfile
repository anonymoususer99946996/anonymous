FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    g++-12 make \
    libbsd-dev \
    iproute2 \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN mkdir -p /opt/boost-1.81/include && \
    cp -r boost-headers /opt/boost-1.81/include/boost

RUN make remise BOOST_ROOT=/opt/boost-1.81

CMD ["./remise"]
