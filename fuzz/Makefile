bin_PROGRAMS = fuzz_libtelem fuzz_config fuzz_iorec

CC = clang
CFLAGS = \
	-g \
	-I../. \
	-I../src/. \
	-DLOCALSTATEDIR=\"/tmp\" \
	-DDATADIR="" \
	-DBACKEND_ADDR=\"abcde\"  

# Define the source files for each binary
fuzz_libtelem_SOURCES = \
	fuzz_libtelem.c

fuzz_config_SOURCES = \
	../src/configuration.c \
	../src/nica/hashmap.c \
	../src/nica/inifile.c \
	utils.c \
	fuzz_config.c

fuzz_iorec_SOURCES = \
	utils.c \
	fuzz_iorec.c

fuzz_iorec_CFLAGS = \
	$(CFLAGS) \
	-fsanitize=address,fuzzer

fuzz_libtelem_CFLAGS = \
	$(CFLAGS) \
	-fsanitize=address,fuzzer \
	-ltelemetry

fuzz_config_CFLAGS = \
	$(CFLAGS) \
	-fsanitize=fuzzer # There's a false positive in configuration.c:189:56

all: $(bin_PROGRAMS)

fuzz_libtelem: $(fuzz_libtelem_SOURCES)
	$(CC) $(fuzz_libtelem_CFLAGS) -o $@ $^

fuzz_config: $(fuzz_config_SOURCES)
	$(CC) $(fuzz_config_CFLAGS) -o $@ $^

fuzz_iorec: $(fuzz_iorec_SOURCES)
	$(CC) $(fuzz_iorec_CFLAGS) -o $@ $^

clean:
	rm -f $(bin_PROGRAMS) *.o

.PHONY: all clean
