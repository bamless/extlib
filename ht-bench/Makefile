CC      = gcc
CFLAGS  = -std=c11 -O3 -Wall -Wextra

BINS    = bench_extlib_simd bench_extlib

# Default N for all benchmark targets.  Override on the command line:
#   make bench-insert-int N=1000000
N            ?= 500000
WARMUP       ?= 2
# Extra flags forwarded verbatim to every hyperfine invocation, e.g.:
#   make bench --export-markdown=results.md
HFLAGS       ?=

BENCHMARKS = \
    insert-int \
    lookup-hit-int \
    lookup-miss-int \
    delete-int \
    delete-heavy-int \
    mixed-int \
    iterate-int \
    word-count-int \
    insert-str \
    lookup-hit-str \
    lookup-miss-str \
    delete-heavy-str \
    insert-bigkey \
    lookup-hit-bigkey \
    delete-heavy-bigkey \
    insert-large \
    lookup-hit-large \
    delete-heavy-large

# Derive per-scenario phony targets: bench-insert-int, bench-lookup-hit-int, …
BENCH_TARGETS = $(addprefix bench-,$(BENCHMARKS))

.PHONY: all clean clean-results check bench run plot run-and-plot $(BENCH_TARGETS)

all: $(BINS)

bench_extlib_simd: bench_extlib_simd.c ../extlib.h
	$(CC) $(CFLAGS) -o $@ $<

bench_extlib: bench_extlib.c extlib.h
	$(CC) $(CFLAGS) -o $@ $<

# Quick sanity check: run every benchmark in both binaries and compare outputs.
# If the numeric sinks differ the workload is not equivalent.
check: all
	@echo "=== Sanity check (N=1000) ==="
	@for bm in $(BENCHMARKS); do \
	    a=$$(./bench_extlib_simd $$bm $(N)); \
	    b=$$(./bench_extlib      $$bm $(N)); \
	    printf "%-22s  extlib (simd)=%-12s extlib=%s\n" "$$bm" "$$a" "$$b"; \
	done

# Individual scenario targets: `make bench-insert-int`, `make bench-mixed-int`, …
# Each runs hyperfine for that one scenario and exits.
$(BENCH_TARGETS): bench-% : all
	hyperfine --warmup $(WARMUP) $(HFLAGS) \
	    --command-name "extlib (simd)/$*" "./bench_extlib_simd $* $(N)" \
	    --command-name "extlib/$*"        "./bench_extlib      $* $(N)"

# Run all scenarios back-to-back.
bench: $(BENCH_TARGETS)

VENV    = tmp/venv
PYTHON  = $(VENV)/bin/python3

$(VENV): tmp/plot.py
	python3 -m venv $(VENV)
	$(VENV)/bin/pip install matplotlib --quiet

# Collect hyperfine JSON results into tmp/<benchmark>.json.
# Re-runs every benchmark; use `make plot` alone to skip re-running.
run: all
	@mkdir -p tmp
	@for bm in $(BENCHMARKS); do \
	    echo ">>> $$bm"; \
	    hyperfine --shell=none --warmup $(WARMUP) --runs 10 $(HFLAGS) \
	        --export-json "tmp/$$bm.json" \
	        --command-name "extlib_simd" "./bench_extlib_simd $$bm $(N)" \
	        --command-name "extlib"      "./bench_extlib      $$bm $(N)"; \
	done

# Render graphs from whatever JSON files are currently in tmp/.
plot: $(VENV)
	$(PYTHON) tmp/plot.py

# Run all benchmarks then immediately plot.
run-and-plot: run plot

clean:
	rm -f $(BINS)

clean-results:
	rm -f tmp/*.json tmp/*.png
