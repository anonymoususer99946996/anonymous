FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive

# Toolchain + deps:
#   g++-12, make                 : build both projects (C++20 for remise, C++17 for prg)
#   libbsd-dev                   : arc4random
#   iproute2                     : tc/netem latency shaping
#   libssl-dev                   : OpenSSL (-lcrypto) for prg-bitsliced (LowMC)
#   libboost-system/thread-dev   : COMPILED Boost libs for prg-bitsliced
#                                  (-lboost_system -lboost_thread; header-only won't do)
# Build with --network=host (see up.sh) to avoid build-network interception.
RUN apt-get update && apt-get install -y --no-install-recommends \
    g++-12 make \
    libbsd-dev \
    iproute2 \
    libssl-dev \
    libboost-system-dev libboost-thread-dev \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

# --- remise: needs Boost 1.81 headers (header-only Asio + awaitable_operators) ---
# Shipped in the build context as ./boost-headers; Ubuntu's 1.74 lacks the header.
RUN mkdir -p /opt/boost-1.81/include && \
    cp -r boost-headers /opt/boost-1.81/include/boost
    RUN make remise BOOST_ROOT=/opt/boost-1.81
    RUN make remise_sabre BOOST_ROOT=/opt/boost-1.81

 

CMD ["./remise"]
