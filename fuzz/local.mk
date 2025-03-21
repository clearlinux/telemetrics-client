COMPILER = clang
CLANG_FLAGS = \
	-g \
	-fsanitize=address,fuzzer
SOURCES = fuzz/libtelem_fuzzer.c
BIN = fuzz/libtelem_fuzzer
HALF_DAY_SEC = 43200
FUZZ_FLAGS = \
	-max_len=16384 \
	-timeout=10 \
	-only_ascii=1 \
	-runs=1000000 \
	-max_total_time=$(HALF_DAY_SEC)

build_fuzz_libtelem:
	LD_LIBRARY_PATH=src/.libs $(COMPILER) $(CLANG_FLAGS) -ltelemetry  $(SOURCES) -o $(BIN)

fuzz: build_fuzz_kibtelem
	$(BIN) fuzz/corpus $(FUZZ_FLAGS)

# vim: filetype=automake tabstop=8 shiftwidth=8 noexpandtab
