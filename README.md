Telemetrics-client
==================

This package provides the front end component of a complete telemetrics
solution for Linux-based operating systems. Specifically, the front end
component includes:

- telemetrics probes that collect specific types of data from the operating
  system.

- a library, libtelemetry, that telemetrics probes use to create telemetrics
  records and send them to the daemon for further processing.

- a daemon, telemd, that prepares the records to send to a telemetrics server
  (not included in this source tree), or spools the records on disk in case
  it's unable to successfully deliver them.

A telemetrics server implementation that works with this component is available
from
[clearlinux/telemetrics-backend](https://github.com/clearlinux/telemetrics-backend).


Build dependencies
---------------------

- libcheck

- libcurl

- elfutils, which provides libelf and libdwfl libraries..

- (optional) libsystemd, for syslog-style logging to the systemd journal, and
  socket/path activation of telemd by systemd.


Build and installation
---------------------

```{r, engine='bash', count_lines}
$ ./autogen.sh
$ ./configure
$ make
```

Set up
---------------------

There is a config file installed at

```{r, engine='bash', count_lines}
/usr/share/defaults/telemetrics/telemetrics.conf
```

To make modifications, copy that file to

```{r, engine='bash', count_lines}
/etc/telemetrics/telemetrics.conf
```
and modify the /etc version.

Descriptions of config options are listed below in the Usage section.

Starting the daemon
---------------------

Method 1 (recommended):

```{r, engine='bash', count_lines}
 systemctl start telemd.socket telemd.path
```

Note: the above invocation technically readies the service for both socket and
path activation, so you may not see the daemon start right away.

Method 2:

```{r, engine='bash', count_lines}
systemctl start telemd.service
```

Method 3:

```{r, engine='bash', count_lines}
telemd &
```

Configure the daemon to autostart at boot
---------------------

Method 1 (recommended):

Enable the socket-activated service and path unit:

```{r, engine='bash', count_lines}
systemctl enable telemd.socket telemd.path
```

Method 2:

Enable the service itself, which automatically enables the socket and path
units as well:

```{r, engine='bash', count_lines}
systemctl enable telemd.service
```

Usage
---------------------

Once the daemon is running, the telemetrics probes will be ready to use.

Available probes:

* hprobe: A test program that utilizes libtelemetry to ensure
that telemetrics-client works. It sends a "hello world" record to the server.

* crash probe: A handler for core files that sends the corresponding backtraces
to the server.

The daemon uses the following configuration options from the configuration file:

* server: This specifies the web server that the daemon sends the telemetry records to
* socket_path: This specifies the path of the unix domain socket that the
  daemon listens on for connections from the probes
* spool_dir: This config option is related to spooling. If the daemon is not
  able to send the telemetry records to the backend server due to reasons such
  as the network availability, then it stores the records in a spool directory.
  This option specifies that path of the spool directory. This directory should
  be owned by the same user that the daemon is running as.

```{r, engine='bash', count_lines}
    mkdir -p /var/spool/telemetry
    chown -R telemetry:telemetry /var/spool/telemetry
    systemctl restart telemd.service
```

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

Machine id
---------------------

The machine id reported by the telemetry client is rotated every 3 days for
privacy reasons. If you wish to have a static machine id for testing purposes,
you can opt in by creating a static machine id file named
"opt-in-static-machine-id" under the directory  "/etc/telemetrics/". Where
"unique machine ID" is your desired static machine ID:

```{r, engine='bash', count_lines}
# mkdir -p /etc/telemetrics
# echo "unique machine ID" > /etc/telemetrics/opt-in-static-machine-id
```

The telemetry daemon reads, at most, the first 32 characters from this file
uses it for the machine id. You can put a string like 'my-machine-name' in this
file to easily identify your machine.  Restart telemd for the machine id
changes to take effect by running:

```{r, engine='bash', count_lines}
# systemctl restart telemd.service
```

You can switch back to the rotating machine id by deleting the override file
and restarting the daemon.  You can do a quick test to check that your
machine-id has changed by running "hprobe" and verifying that a record has
landed on your backend telemetrics server, with the specified machine id.
