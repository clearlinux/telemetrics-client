# Probes

The default probes provided along the telemetry client code are:

* bertprobe: this probe reports on the Boot Error Region Table if such
  entry is found in ```/sys/firmware/acpi/tables/BERT```.

* crashprobe: This probe processes core dump files. It can be registered as
  the kernel core file handler in /proc/sys/kernel/core_pattern.

* hprobe: a simple probe that sends a keep alive message. This probe can also
  be useful for testing

* journalprobe: a probe that monitors the systemd journal for log messages
  from failed services.

* oopsprobe: a probe to collect 'oops messages' when the kernel detects a
  problem.

* pstoreprobe: probe to collect messages left on pstore filesystem.

* pythonprobe: a probe that monitors Python generated traceback files.

* telem-record-gen: this is a "general-purpose probe" for sending custom
  records on-the-fly. This tool can be used stand alone or as a part of a
  script to implement a probe.

For example to generate an hprobe like message we can execute the following
command:

```
$ telem-record-gen --class org.example/hello/world --payload hello
```

for more options use the ```--help``` switch i.e.:

```
$ telem-record-gen --help
Usage:
  telem-record-gen [OPTIONS] - create and send a custom telemetry record

Help Options:
  -h, --help            Show help options

Application Options:
  -f, --config-file     Path to configuration file (not implemented yet)
  -V, --version         Print the program version
  -s, --severity        Severity level (1-4) - (default 1)
  -c, --class           Classification level_1/level_2/level_3
  -p, --payload         Record body (max size = 8k)
  -P, --payload-file    File to read payload from
  -R, --record-version  Version number for format of payload (default 1)
  -e, --event-id        Event id to use in the record

```
