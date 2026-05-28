# Remise: Authorized Anonymous Communication Systems

**Rohan Ravi**
IIT Kanpur
[rohanra@cse.iitk.ac.in](mailto:rohanra@cse.iitk.ac.in)

**Paritosh Shukla**
IIT Kanpur
[paritoshs24@cse.iitk.ac.in](mailto:paritoshs24@cse.iitk.ac.in)

**Adithya Vadapalli**
IIT Kanpur
[avadapalli@cse.iitk.ac.in](mailto:avadapalli@cse.iitk.ac.in)

---

## Overview

This repository contains the implementation and experimental artifacts for:

> **Remise: Authorized Anonymous Communication Systems**

Remise is an authorized anonymous communication system that combines privacy-preserving communication with cryptographic authorization mechanisms. The implementation includes the complete experimental framework used for evaluating throughput, latency, and communication overhead across different system configurations.

---

# Dependencies

The implementation has been tested on:

* Ubuntu 22.04 LTS
* GCC/G++ 11+
* C++20 compatible toolchain

---

## Required System Packages

Install the required dependencies using:

```bash
sudo apt update

sudo apt install -y \
    build-essential \
    g++ \
    make \
    pkg-config \
    libboost-all-dev \
    libbsd-dev
```

---

## Required Libraries

### Boost.Asio

The implementation uses Boost.Asio for asynchronous TCP networking and coroutine-based communication.

Required headers include:

* `boost/asio.hpp`
* `boost/asio/awaitable.hpp`
* `boost/asio/use_awaitable.hpp`
* `boost/asio/experimental/awaitable_operators.hpp`

---

### SIMD Intrinsics

The implementation uses Intel SIMD intrinsics for efficient cryptographic computation.

Required instruction sets:

* SSE2
* SSSE3

Relevant headers:

* `emmintrin.h`
* `tmmintrin.h`

You can verify CPU support using:

```bash
lscpu | grep -E "sse2|ssse3"
```

---

## Compiler Requirements

The project requires:

* C++20 support
* Coroutine support
* SIMD intrinsic support

Example compilation flags:

```bash
-std=c++20 -O3 -march=native
```

---

# Building

Compile the project using:

```bash
make -j
```

---

# Running a Single Instance

A single instance of Remise can be executed by launching the two parties (`p0` and `p1`) in separate terminals.

## Usage

```bash
./remise [p0|p1] <log_num_items> <leafsize> <authorized_fraction> <num_requests>
```

## Parameters

* `p0|p1` — Party identifier
* `log_num_items` — Logarithm (base 2) of the number of database items
* `leafsize` — ORAM leaf bucket size
* `authorized_fraction` — Fraction of authorized requests
* `num_requests` — Total number of requests

## Example

In the first terminal:

```bash
./remise p0 26 2 0.7 10
```

In the second terminal:

```bash
./remise p1 26 2 0.7 10
```

This launches a Remise experiment instance with:

* Database size: (2^{26}) items
* Leaf size: 2
* Authorized request fraction: 70%
* Total requests: 10

---

# Running Experiments

All experiments can be executed using:

```bash
sh run-all.sh config.txt
```

The experiment parameters are specified using a configuration file.

---

## Configuration File Format

Each line in the configuration file represents a single experiment instance with the following format:

```text
# LOG_NITEMS LEAFSIZE AUTH_FRAC NUM_REQUESTS NUM_RUNS
```

Where:

* `LOG_NITEMS` — Logarithm (base 2) of the number of database items
* `LEAFSIZE` — ORAM leaf bucket size
* `AUTH_FRAC` — Fraction of authorized requests
* `NUM_REQUESTS` — Number of requests per experiment
* `NUM_RUNS` — Number of repeated executions

---

## Example Configuration

```text
# LOG_NITEMS LEAFSIZE AUTH_FRAC NUM_REQUESTS NUM_RUNS

16 2 0.7 10 5
18 2 0.7 10 5
20 2 0.7 10 5
22 2 0.7 10 5
24 2 0.7 10 5
26 2 0.7 10 5
```

The above configuration evaluates the system across increasing database sizes while keeping the remaining parameters fixed.

---

## Sample Output

```text
===================================================
RUN CONFIGURATION
===================================================
log_nitems        = 26
leafsize          = 2
authorized_frac   = 0.7
num_requests      = 10
num_runs          = 5

======================================
throughput           mean = 0.3001   stddev = 0.0010   95CI = ±0.0009
goodput              mean = 0.1801   stddev = 0.0006   95CI = ±0.0005
avg_eval_ms          mean = 1941.8600   stddev = 11.2680   95CI = ±9.8768
avg_audit_ms         mean = 347.8400   stddev = 0.2408   95CI = ±0.2111
avg_write_ms         mean = 1040.9600   stddev = 3.2677   95CI = ±2.8643
eval_bandwidth       mean = 1040.0000   stddev = 0.0000   95CI = ±0.0000
audit_bandwidth      mean = 0.0000   stddev = 0.0000   95CI = ±0.0000
write_bandwidth      mean = 384.0000   stddev = 0.0000   95CI = ±0.0000
total_bandwidth      mean = 1424.0000   stddev = 0.0000   95CI = ±0.0000
```

The results of this run will be saved in the `results` folder.

---

## Metrics

The reported metrics correspond to:

* `throughput` — Total completed requests per second
* `goodput` — Successfully authorized requests per second
* `avg_eval_ms` — Average evaluation latency (milliseconds)
* `avg_audit_ms` — Average audit latency (milliseconds)
* `avg_write_ms` — Average write latency (milliseconds)
* `eval_bandwidth` — Bandwidth consumed during evaluation
* `audit_bandwidth` — Bandwidth consumed during auditing
* `write_bandwidth` — Bandwidth consumed during writes
* `total_bandwidth` — Total communication bandwidth

For each metric, the framework reports:

* Mean across all runs
* Standard deviation
* 95% confidence interval

---

# Networking Notes

Some experiments may require network latency emulation.

Latency can be configured using:

```
sh set-latency.sh 30ms
```

For example, the above command sets the network latency to 30 milliseconds.

The latency configuration script internally uses Linux traffic control (`tc`) to emulate network delay.


---

# License

This project is released for academic and research use.

---
# Warning

This implementation is provided solely for research artifact evaluation and benchmarking purposes.

The codebase has **not** been hardened for production deployment and may contain numerous security vulnerabilities, including but not limited to:

* Missing input validation
* Insecure networking assumptions
* Debugging code paths
* Unsafe memory handling
* Side-channel leakage risks
* Incomplete fault tolerance

This software should **not** be used in production environments or exposed to untrusted networks.

---

# Contact

For questions regarding the implementation or paper, please contact:

* Rohan Ravi — [rohanra@cse.iitk.ac.in](mailto:rohanra@cse.iitk.ac.in)
* Paritosh Shukla — [paritoshs24@cse.iitk.ac.in](mailto:paritoshs24@cse.iitk.ac.in)
* Adithya Vadapalli — [avadapalli@cse.iitk.ac.in](mailto:avadapalli@cse.iitk.ac.in)
