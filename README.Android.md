
Telemetrics-client on Android
=============================

This package provides the front end component of a complete telemetrics
solution for Android operating systems. Specifically, the front end
component includes:

- telemetrics probes that collect specific types of data from the Android operating
  system. For more info on probes go [here](./src/probes).

- a library, libtelemetry, that telemetrics probes use to create telemetrics
  records and send them to the daemon for further processing.

- telemprobd, a daemon that handles communication with probes and forwards records
  to a second daemon.

- telempostd, this daemon sends telemetry records to a telemetrics server
  (not included in this source tree), or spools the records on disk in case
  it's unable to successfully deliver them.

Currently, Android reuse the telemetrics backend with clear linux
[clearlinux/telemetrics-backend](https://github.com/clearlinux/telemetrics-backend).


Where to get source code
------------------------

The source code is maintained in the Git repository at https://github.com/clearlinux/telemetrics-client. 
you can browse it online or use "git clone https://github.com/clearlinux/telemetrics-client.git" to download it.


How to build telemetrics-client on android
------------------------------------------

After telemetrics-client integrate to the project celadon, telemetrics-client will be built into android image will following command:
* source build/envsetup.sh
* lunch, select the target to build
* make flashfiles -j $(nproc)
for more detail, following the [projectceladon/build-source](https://01.org/projectceladon/documentation/getting_started/build-source).


How to flash/install on android devices
---------------------------------------

In case of the android build, the generated image is packaged at the following location: out/target/product/$(TARGET)/$(TARGET)-${USER}.tar.gz
Following the [projectceladon/run-install](https://01.org/projectceladon/documentation/getting_started/install-run) to install image.


Starting the telemetrics-client on android
------------------------------------------

telemprobd starts automatically at boot.

Once the telemprobd is running, the telemetrics probes will be ready to use.

Available probes:

* hprobe: A test program that utilizes libtelemetry to ensure
that telemetrics-client works. It sends a "hello world" record to the server.

* crash probe: A handler for core files that sends the corresponding backtraces
to the server.

* pstore probe: A handler to detect the pstore and parse the ramoops, send the corresponding backtraces
to the server.

* klog_scanner probe: scan the crash, fault, error from kernel log, send the corresponding backtraces
to the server.

* bert probe: A handler to detect the firmware crash, send the corresponding backtraces
to the server.

```
using pstore probe for example:
* run "adb root" and "adb shell" to entry Android shell command
* run "echo c > /proc/sysrq-trigger" to trigger panic
* if panic happens, device reboot. After reboot, there is pstore event under /data/vendor/telemetrics/spool/
* Also you can find the kernel/bug event from https://clr.telemetry.intel.com/telemetryui/
```

The client uses the following configuration options from the configuration file:

* server: This specifies the web server that the telempostd sends the telemetry records to
* socket_path: This specifies the path of the unix domain socket that the
  telemprobd listens on for connections from the probes
* spool_dir: This config option is related to spooling. If the daemon is not
  able to send the telemetry records to the backend server due to reasons such
  as the network availability, then it stores the records in a spool directory.
  This option specifies that path of the spool directory. This directory should
  be owned by the same user that the daemon is running as.
* record_expiry: This is the time in minutes after which the records in the
  spool directory are deleted by the daemon.
* spool_max_size: This specifies the maximum size of the spool directory. When
  the size of the spool directory reaches this limit, new telemetry records are
  dropped by the daemon.
* spool_process_time: This specifies the time interval in seconds that the
  daemon waits for before checking the spool directory for records. The daemon
  picks up the records in the order of modification date and tries to send the
  record to the server. It sends a maximum of 10 records at a time. If it was
  able to send a record successfully, it deletes the record from the spool.  If
  the daemon finds a record older than the "record_expiry" time, then it
  deletes that record. The daemon looks at a maximum of 20 records in a single
  spool run loop.
* rate_limit_enabled: This determines whether rate-limiting is enabled or
  disabled. When enabled, there is a threshold on both records sent within a
  window of time, and record bytes sent within a window a time.
* record_burst_limit: This is the maximum amount of records allowed to be
  passed by the daemon within the record_window_length of time. If set to -1,
  the rate-limiting for record bursts is disabled.
* record_window_length: This is the time, in minutes (0-59), that establishes
  the window length for the record_burst_limit. EX: if record_burst_window=1000
  and record_window_length=15, then no more than 1000 records can be passed
  within any given fifteen minute window.
* byte_burst_limit: This is the maximum amount of bytes that can be passed by
  the daemon within the byte_window_length of time. If set to -1, the
  rate-limiting for byte bursts is disabled.
* byte_window_length: This is the time, in minutes (0-59), that establishes the
  window length for the byte_burst_limit.
* rate_limit_strategy: This is the strategy chosen once the rate-limiting
  threshold has been reached. Currently the options are 'drop' or 'spool', with
  spool being the default. If spool is chosen, records will be spooled and sent
  at a later time.
* record_retention_enabled: When this key is enabled (true) the daemon saves a
  copy of the payload on disk from all valid records. To avoid the excessive use
  of disk space only the latest 100 records are kept. The default value for this
  configuration key is false.
* record_server_delivery_enabled: This key controls the delivery of records to
  ```server```, when enabled (default value) the record will be posted to the
  address in the configuration file. If this configuration key is disabled (false)
  records will not be spooled or posted to backend. This configuration key can
  be used in combination with ```record_retention_enabled``` to keep copies of
  telemetry records locally only.

Help
----

If any problem about build/flash/install telemetrics-client on Android, Please contact with celadon@lists.01.org 


