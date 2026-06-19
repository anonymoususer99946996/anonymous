# Remise: Authorized Anonymous Communication Systems

**Adithya Vadapalli** — IIT Kanpur — [avadapalli@cse.iitk.ac.in](mailto:avadapalli@cse.iitk.ac.in)
**Rohan Ravi** — IIT Kanpur — [rohanra@cse.iitk.ac.in](mailto:rohanra@cse.iitk.ac.in)
**Paritosh Shukla** — IIT Kanpur — [paritoshs24@cse.iitk.ac.in](mailto:paritoshs24@cse.iitk.ac.in)

---

## 1. Overview

This repository contains the implementation and experimental artifacts for:

> **Remise: Authorized Anonymous Communication Systems**

Remise is a two-server authorized anonymous communication system built on
Distributed Oblivious RAM (DORAM) and Distributed Point Functions (DPFs/FSS). A
client writes to an anonymous bulletin board only if it is *authorized*, and the
two servers jointly (a) check that the request is well-formed (the **audit**) (Remise has built-in auditing, while comparitors need this step),
(b) verify authorization, and (c) apply the write to the shared database —
all without either server learning which row was written.

This artifact ships **two server-side variants**, each as a standalone benchmark
binary:

| Binary           | Paper name           | Authorization mechanism                                  |
|------------------|----------------------|----------------------------------------------------------|
| `remise`         | **RemiseBB**         | XOR-tag audit over the proof database                    |
| `remise_sabre`   | **RemiseBB (Sabre)** | MPC evaluation of a bitsliced **LowMC** PRF (`encrypt2_p0p1`) |

Both variants run the two servers (`p0`, the server/acceptor, and `p1`, the
client) as **separate processes** that communicate over a TCP socket using a
coroutine-based asynchronous networking layer. They share an almost identical
command-line interface, emit the same kinds of measurements, and are driven by
the same harness.

This README walks through (1) building, (2) the two-container Docker workflow we
recommend for reproducibility, (3) running a single experiment by hand, (4) the
conceptual breakdown of what is being measured, and (5) reproducing every figure
and table, one experiment at a time, including what each result *means* and which
paper claim it supports.

---

## 2. What the experiments measure (read this first)

Every request a server processes is decomposed into three timed phases. Getting
these definitions straight is essential for interpreting the plots, because
different figures report different *combinations* of them.

| Phase          | Code                                   | What it is                                                              |
|----------------|----------------------------------------|-------------------------------------------------------------------------|
| **preprocess** | `__evalinterval_mpc`                   | DPF interval evaluation ("eval"). The expensive oblivious-access work.  |
| **audit**      | XOR-tags (`remise`) / `encrypt2_p0p1` (`remise_sabre`) | The request-validity check.                              |
| **access**     | `finalize` + FCW exchange + DB update  | Producing the final write share and applying it to the shared database. |

In addition, **DPF generation** (`gen`) produces the DPF keys. Generation is an
**offline** step and is **never** inside any timed region.
The purpose of the step is just to cleanly produce a DPF structure that can passed to evalinterval. It has no protocol relavance, rather is a hack to get around C++ compile errors. The authors plan to make this clean in the future.  

From these we report two composite timings that differ *only* in whether
**preprocess (eval)** is counted:

- **online** = `audit + access` (eval **excluded**). This is the "with
  preprocessed DPFs" setting: it assumes DPF keys were generated and evaluated
  offline, so the online critical path is just audit + access.
- **total** (a.k.a. **on-the-fly**) = `eval + audit + access` (eval
  **included**; generation still excluded). This is the "DPFs produced/evaluated
  on the fly" setting.

The corresponding rates are:

- **throughput** = requests / wall-clock; **goodput** = authorized-requests /
  wall-clock — both using the *total* (on-the-fly) time.
- **online throughput / online goodput** — the same counts divided by the
  *online* time (eval excluded).

**Important:** both the on-the-fly and the with-preproc numbers come out of a
**single run**. There is no separate "mode" to select — they are the same work
measured against two different denominators.

Two more conventions used throughout:

- **Message size (bytes) = 16 × `leafsize`.** A leaf is `std::array<__m128i,
  leafsize>` and each `__m128i` is 16 bytes. So `leafsize=2` → 32-byte messages,
  `leafsize=64` → 1024-byte messages, `leafsize=256` → 4096-byte messages.
- **`auth_fraction` is the fraction of *authorized* requests.** The paper's
  "*f*% unauthorized traffic" corresponds to `auth_fraction = 1 − f/100`
  (e.g. 30% unauthorized → `auth_fraction = 0.7`).

---

## 3. Dependencies

Tested on **Ubuntu 22.04 LTS**, x86-64.

### Hardware

An x86-64 CPU with **AES-NI**, **SSE2**, **SSSE3**, and **AVX2**. AES-NI backs
the DPF pseudorandom generator; AVX2 (256-bit vectors) backs the bitsliced LowMC
audit in `remise_sabre`. Verify support with:

```bash
lscpu | grep -E "aes|sse2|ssse3|avx2"
```

Memory scales with database size and message size. The paper's largest runs
(2^30 entries) used a 251 GiB machine; the headline points up to 2^26 are
comfortable with ~16–32 GiB. The paper's reference platform: two Intel Xeon Gold
6430 CPUs, 251 GiB RAM, 64-bit Linux.

### Software (native build)

- **g++-12** with C++20 (required — the networking layer uses C++20 coroutines;
  Ubuntu's default g++-11 is not sufficient for our coroutine usage).
- **Boost 1.81 headers** (required — the async layer uses
  `boost/asio/experimental/awaitable_operators.hpp`, which is **absent** from
  Ubuntu 22.04's packaged Boost 1.74). Header-only; no compiled Boost libs are
  needed for `remise`.
- **libbsd** (`arc4random`), **OpenSSL / libcrypto** (used by the LowMC audit
  streams in `remise_sabre`), **iproute2** (`tc`, for latency/bandwidth
  emulation), **make**.

> The packaged `libboost-all-dev` on Ubuntu 22.04 is Boost 1.74 and will **not**
> compile the coroutine networking. Use Boost 1.81 headers (point the Makefile at
> them with `BOOST_ROOT=$HOME/boost-1.81`), or use the Docker path below, which
> vendors the correct headers automatically.

### Software (Docker — recommended)

Just a working **Docker Engine with the `compose` plugin** on a Linux host. The
provided `Dockerfile` (based on `ubuntu:22.04`) installs g++-12, the Boost 1.81
headers, libbsd, OpenSSL, iproute2, and builds both binaries.

---

## 4. Building

### Option A — Docker (recommended)

```bash
./up.sh            # build the image, then start containers p0 and p1
./up.sh --rebuild  # force a rebuild
```

`up.sh` builds an image that compiles **both** binaries (`make remise` and
`make remise_sabre`), verifies that `/app/remise` and `/app/remise_sabre` exist,
then starts two long-lived containers — `p0` (server) and `p1` (client) — on a
private Docker bridge network. The containers idle (`sleep infinity`) until you
launch an experiment into them. The host directory `./results/` is mounted into
both containers, so all CSV output lands there.

Tear down when finished:

```bash
./down.sh          # stop & remove the two containers
./down.sh --rmi    # also remove the built image
```

### Option B — native build

With g++-12 and the Boost 1.81 headers available:

```bash
make remise        BOOST_ROOT=$HOME/boost-1.81
make remise_sabre  BOOST_ROOT=$HOME/boost-1.81
```

You then run the two parties as two processes (see §6). Network emulation with
`tc` requires `sudo`/`NET_ADMIN` on the host.

> **Why the containers need elevated capabilities.** To shape latency/bandwidth
> *inside* the containers, the two services are granted the `NET_ADMIN`
> capability and run with `apparmor=unconfined`. These are scoped to the two
> experiment containers on a private bridge network and do not change the host's
> networking. If you prefer not to grant them, run at zero emulated latency (LAN)
> — only the latency-dependent rows of the figures need shaping.

---

## 5. Network latency / bandwidth emulation

Wide-area conditions are emulated with `tc netem` (delay) and `tbf` (rate),
applied **inside** the containers by `netshape.sh`. The requested round-trip time
(RTT) is split across the two endpoints (RTT/2 one-way each).

```bash
./netshape.sh set 30 100mbit   # 30 ms RTT, 100 Mbit/s each direction
./netshape.sh set 30           # 30 ms RTT, unshaped bandwidth
./netshape.sh clear            # remove all shaping
./netshape.sh show             # show current qdisc state
```

The run scripts (§6, §7) call `netshape.sh` for you, so you normally do not need
to invoke it directly. The paper uses RTTs of **10, 30, and 60 ms**.

---

## 6. Running a single experiment

Both binaries share this interface (only `remise` accepts the optional
`batch_size`):

```
remise        [p0|p1] <log_nitems> <leafsize> <auth_fraction> <num_requests> [batch_size]
remise_sabre  [p0|p1] <log_nitems> <leafsize> <auth_fraction> <num_requests>
```

| Argument         | Meaning                                                                 |
|------------------|-------------------------------------------------------------------------|
| `p0` / `p1`      | Party (server / client). `p0` listens on port 9200; `p1` connects to it.|
| `log_nitems`     | log2 of the database size (so DB has 2^`log_nitems` rows).              |
| `leafsize`       | Leaf size; message size in bytes = 16 × `leafsize`.                     |
| `auth_fraction`  | Fraction of authorized requests, in [0,1].                              |
| `num_requests`   | Number of requests issued in the run.                                   |
| `batch_size`     | (`remise` only) requests pipelined concurrently. Default 64; use 1 to disable batching. |

### Via Docker (recommended)

`run.sh` (for `remise`) and `run_remise_sabre.sh` (for `remise_sabre`) shape the
network, launch `p0` in the background and `p1` in the foreground inside the
running containers, wait for completion, and clear the shaping:

```
./run.sh              <RTT_ms> [bw] [log_nitems] [leafsize] [auth_fraction] [num_requests] [batch_size]
./run_remise_sabre.sh <RTT_ms> [bw] [log_nitems] [leafsize] [auth_fraction] [num_requests]
```

Example — RemiseBB, 30 ms RTT, 100 Mbit/s, DB 2^20, 32-byte messages, 70%
authorized, 10 requests, unbatched:

```bash
./run.sh 30 100mbit 20 2 0.7 10 1
```

The run scripts also forward two environment variables into the container so the
binary can self-label its CSV rows:

- `REMISE_RTT_MS` — the RTT, stamped into the `latency_ms` column.
- `REMISE_TRIAL` — a trial index, stamped into the `trial` column (the sweep
  scripts set this per repetition).

### Via native processes (two terminals)

```bash
# terminal 1 (server)
./remise p0 26 2 0.7 10
# terminal 2 (client)
P0_HOST=127.0.0.1 ./remise p1 26 2 0.7 10
```

`p1` resolves its peer from `P0_HOST` (default `p0` in Docker, set it to the
server's address/hostname for a two-machine deployment), connecting on port 9200.

Each run prints an **Experiment Summary** (per-phase latencies, bandwidth,
throughput, goodput) and appends rows to CSV files under `./results/`.

---

## 7. Reproducing the figures and tables

Each experiment has a **sweep script** (collects data into a CSV, one row per
trial) and a **plot script** (averages over trials with 95% confidence intervals
and renders the figure). All commands assume the containers are up (`./up.sh`).
The sweeps default to averaging over **30 trials** for the latency figures and a
handful of trials for the throughput figures; pass arguments to scale down for a
quick smoke test.

The plot scripts require Python 3 with `matplotlib`:

```bash
pip install matplotlib
```

### 7.1 Figure 3a — RemiseBB validity-check time (Claim C1)

**What it shows.** Per-request validity-check time of **RemiseBB** vs database
size, at 10/30/60 ms, for two quantities: **online** (audit only, eval excluded)
and **total** (eval + audit). Supports **C1**: considering only the online phase,
RemiseBB attains an order-of-magnitude speedup over Spectrum (PACL), nearing
80× at 2^26.

```bash
./sweep_fig3a.sh                 # full sweep (2^16..2^26, 10/30/60 ms, 30 trials)
./sweep_fig3a.sh 18 3            # quick check: cap at 2^18, 3 trials
python3 plot_fig3a.py --combined
```

- **Data:** `results/fig3a_remisebb.csv`
  — columns `variant, latency_ms, log_nitems, trial, online_ms, total_ms`.
- **Plots:** one per latency plus a combined panel; each shows the *online* and
  *total* curves vs database size.
- **Read it as:** the two curves coincide at small DB sizes (eval negligible) and
  separate as DB grows (eval dominates *total*). To get the C1 speedup factor,
  divide the Spectrum (PACL) online time by RemiseBB's `online_ms` at the same
  size; at 2^26 this should be ≈ 80×.

### 7.2 Figure 3b — RemiseBB (Sabre) audit time (Claim C2)

**What it shows.** Audit time of **RemiseBB (Sabre)** — the bitsliced-LowMC MPC
(`encrypt2_p0p1`) — vs database size, at 10/30/60 ms. Supports **C2**: the audit
cost is independent of the database size (authorization is O(1), audit is
O(log n)), so the curve is essentially flat, in contrast to Express (PACL) whose
authorization grows linearly.

```bash
./sweep_fig3b.sh
./sweep_fig3b.sh 18 3            # quick check
python3 plot_fig3b.py --combined
```

- **Data:** `results/fig3b_remisebb_sabre.csv`
  — columns `variant, latency_ms, log_nitems, trial, audit_ms`.
- **Read it as:** a near-horizontal line at each latency (the audit time is
  dominated by the fixed LowMC round trips, not the database size).

### 7.3 Figures 4a / 4b — throughput & goodput (Claim C3)

**What it shows.** For RemiseBB (4a) and RemiseBB (Sabre) (4b): throughput and
goodput vs database size, in a three-panel row for **30% / 60% / 90%
unauthorized traffic**, with four curves per panel — *with-preproc*
throughput/goodput and *on-the-fly* throughput/goodput. Supports **C3**: with
preprocessed DPFs, more unauthorized traffic *raises* throughput/goodput (cover
requests skip the expensive access phase); on-the-fly DPFs lower both; and the
Sabre on-the-fly curve overtakes Express (PACL) at smaller DB sizes as the
unauthorized fraction grows.

```bash
./sweep_fig4.sh both             # both variants; fixed 30 ms, leafsize 2
./sweep_fig4.sh a 18 2 50        # quick check: RemiseBB only, 2^18, 2 trials, 50 reqs
python3 plot_fig4.py results/fig4a_remisebb.csv \
    --out fig4a --title "RemiseBB"
python3 plot_fig4.py results/fig4b_remisebb_sabre.csv \
    --out fig4b --title "RemiseBB (Sabre)"
```

- **Data:** `results/fig4a_remisebb.csv`, `results/fig4b_remisebb_sabre.csv`
  — columns `variant, unauth_frac, log_nitems, trial, throughput, goodput,
  online_throughput, online_goodput`.
- **Curve mapping:** the `online_*` columns are the *with-preproc* curves; the
  plain `throughput`/`goodput` columns are the *on-the-fly* curves.
- The sweep iterates `auth_fraction ∈ {0.7, 0.4, 0.1}` (= 30/60/90%
  unauthorized) automatically and uses many requests per run (default 200) so the
  rates are stable.

### 7.4 Message-size experiment (Claim C4)

**What it shows.** For both variants at fixed DB = 2^20 and 30 ms RTT, the three
phase times — **preprocess** (eval), **audit**, **access** (finalize + write) —
as message size varies over `leafsize ∈ {2, 64, 256}` (32 / 1024 / 4096 bytes).
Supports **C4**: preprocess and audit are independent of message size, while
access grows linearly with message size.

```bash
./sweep_msgsize.sh both
./sweep_msgsize.sh a 5           # quick check: RemiseBB only, 5 trials
python3 plot_msgsize.py results/msgsweep_remisebb.csv \
    --out msg_remisebb --title "RemiseBB"
python3 plot_msgsize.py results/msgsweep_remisebb_sabre.csv \
    --out msg_sabre --title "RemiseBB (Sabre)"
```

- **Data:** `results/msgsweep_remisebb.csv`, `results/msgsweep_remisebb_sabre.csv`
  — columns `variant, latency_ms, leafsize, log_nitems, trial, preprocess_ms,
  audit_ms, access_ms`.
- **Plots:** a stacked bar per message size (preprocess / audit / access) with CI
  error bars; the script also prints a mean ± 95% CI table to stdout.
- **Read it as:** the preprocess and audit segments stay flat across the three
  bars; the access segment grows roughly linearly with message size.

### 7.5 Baselines (PACL)

The Spectrum (PACL) and Express (PACL) baseline curves in Figures 3–5 are **not** currently
part of this artifact; they come from the PACL codebase
(<https://github.com/sachaservan/pacl>).

**Figure 6** compares Remise against a two-party DORAM. The DORAM implementation
used is Duoram, available at
<https://git-crysp.uwaterloo.ca/avadapal/duoram>. Build and run it with the
parameters reported in the paper, then compare throughput/latency/bandwidth
against the Remise results. Since Figure 6 can be generated by running 2P-DORAM, it is not a major claim of this paper.

---

## 8. Output, metrics, and statistics

Every run prints an **Experiment Summary** and appends per-trial rows to the
relevant CSV under `./results/`. The plot scripts aggregate across the repeated
trials and report, for each point, the **mean** and a **95% confidence
interval** (computed from the per-trial spread).

Reported quantities:

- **throughput** — completed requests / second (on-the-fly time).
- **goodput** — authorized requests / second (on-the-fly time).
- **online throughput / goodput** — the same, using the online time (eval
  excluded).
- **preprocess / eval latency** — DPF interval evaluation time.
- **audit latency** — validity-check time.
- **access latency** — finalize + FCW exchange + DB update time.
- **bandwidth** — bytes moved per phase (eval / audit / write) and in total.

---

## 9. Implementation notes

### Two parties, two processes

`p0` (server/acceptor) and `p1` (client) run as separate processes and
communicate only through explicit send/receive operations over a TCP socket;
synchronization happens solely through protocol messages. In the Docker setup the
two processes live in two containers on a private bridge network; natively they
are two terminals (or two machines, via `P0_HOST`). This mirrors a real
distributed deployment while remaining easy to benchmark.

### Stream-based communication API

The networking layer overloads the C++ stream operators — `<<` to send, `>>` to
receive — over the coroutine-based async transport:

```cpp
co_await (peer << value);   // send
co_await (peer >> value);   // receive
co_await ((peer << a) && (peer >> b));   // overlap a send and a receive
```

The operators handle serialization, buffering, transmission, and deserialization
of protocol data structures, keeping the MPC/protocol code concise. Sends and
receives can be composed with `&&` to run concurrently; this is used, for
example, in the FCW exchange.

### RemiseBB vs RemiseBB (Sabre), structurally

- **`remise`** batches requests across independent "lanes," each with its own TCP
  connection, and runs eval / audit / FCW-exchange concurrently across lanes
  (only the shared-DB XOR update is sequential). The audit is an XOR over the
  proof database.
- **`remise_sabre`** performs the LowMC-PRF audit (`encrypt2_p0p1`) per request
  over the same coroutine channel; its eval runs only for authorized requests.

---

## 10. License

Released for academic and research use.

---

## 11. Warning

This implementation is provided **solely for research artifact evaluation and
benchmarking**. It has **not** been hardened for production and may contain
security vulnerabilities, including (but not limited to) missing input
validation, insecure networking assumptions, debugging code paths, unsafe memory
handling, side-channel leakage, and incomplete fault tolerance. Do **not** deploy
it in production or expose it to untrusted networks.

---

## 12. Contact

- Adithya Vadapalli — [avadapalli@cse.iitk.ac.in](mailto:avadapalli@cse.iitk.ac.in)