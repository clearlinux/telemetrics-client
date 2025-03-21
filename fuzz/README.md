# Fuzzing telemetrics-client

## Build
```
make
```

## Run
```
#fuzz_config
fuzz_config -close_fd_mask=2

#fuzz_libtelem
fuzz_libtelem
```

# Link runs in background
```
sudo systemd-run --unit=fuzzing.service -r --uid=$(id -u) \
    --working-directory=${PWD}/fuzz -E LD_LIBRARY_PATH=${PWD}/src/.libs/ fuzz/<fuzzer_> -max_total_time=86400 -runs=10000000 fuzz/corpus
```
