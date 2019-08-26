[![Build Status](https://travis-ci.org/clearlinux/telemetrics-client.svg?branch=master)](https://travis-ci.org/clearlinux/telemetrics-client)

Telemetrics-client
==================

This package provides the front end component of a complete telemetrics
solution for Linux-based operating systems. Specifically, the front end
component includes:

- telemetrics probes that collect specific types of data from the operating
  system. For more info on probes go [here](./src/probes).

- a library, libtelemetry, that telemetrics probes use to create telemetrics
  records and send them to the daemon for further processing.

- telemprobd, a daemon that handles communication with probes and forwards records
  to a second daemon.

- telempostd, this daemon sends telemetry records to a telemetrics server
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
  socket/path activation of telemprobd and telempostd by systemd.


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

Starting the client
---------------------
To use the telemetrics client a one time explicit ```opt-in``` is required (this is
also true when the contents of the directory ```/etc/telemetrics/``` are removed).
To opt-in to telemetrics-client use the command:

```{r, engine='bash', count_lines}
 telemctl opt-in
```

**Note** this is a change from previous versions, before 2.3.0 installation of
telemetrics client was enough to enable the client and if needed the client could
be disabled with ```telemctl opt-out```. This command in previous versions created
```/etc/telemetrics/opt-out``` file (after telemetrics-client version 2.3.0 this
file can be safely removed).

If the client was compiled with systemd support the respective activation units
should be already in place (after a ```make install``` invocation). In this case
the client wil start automatically when data is made available to it. i.e. when
executing an ```/usr/bin/hprobe``` command. Otherwise use the following command:

```{r, engine='bash', count_lines}
 telemctl start
```
Note: the above invocation technically readies the service for both socket and
path activation, so you may not see an "active" status. To check the status of
telemetrics-client use:

```{r, engine='bash', count_lines}
 telemctl is-active
   telemprobd : active
   telempostd : active
```

Starting individual service units ```telempostd.service``` or ```telemeprobd.service```
is discouraged.

Configure the client to autostart at boot
---------------------

As longs as the first time ```opt-in``` was performed, the following methods are valid:


Method 1 (recommended):

Enable the socket-activated service and path unit:

```{r, engine='bash', count_lines}
systemctl enable telemprobd.socket telempostd.path
```

Method 2:

Enable services, which automatically enables the socket and path
units as well:

```{r, engine='bash', count_lines}
systemctl enable telemprobd.service
systemctl enable telempostd.service
```

Usage
---------------------

Once the client is running, the telemetrics probes will be ready to use.

Available probes:

* hprobe: A test program that utilizes libtelemetry to ensure
that telemetrics-client works. It sends a "hello world" record to the server.

* crash probe: A handler for core files that sends the corresponding backtraces
to the server.

The client uses the following configuration options from the configuration file:

* server: This specifies the web server that the telempostd sends the telemetry records to
* socket_path: This specifies the path of the unix domain socket that the
  telemprobd listens on for connections from the probes
* spool_dir: This config option is related to spooling. If the daemon is not
  able to send the telemetry records to the backend server due to reasons such
  as the network availability, then it stores the records in a spool directory.
  This option specifies that path of the spool directory. This directory should
  be owned by the same user that the daemon is running as.

```{r, engine='bash', count_lines}
    mkdir -p /var/spool/telemetry
    chown -R telemetry:telemetry /var/spool/telemetry
    systemctl restart telemprobd.service
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


Data reported
---------------------

The data reported by the telemetry client could be understood as two main sets:
metadata and a payload.

The metadata is used to report details of a machine's architecture. The
following are the metadata values currently collected (Record Format Version 4):

* record_format_version: version of the record, currently is 'Version 4'. This
  value changes when new metadata is added.
* classification: this field is used to identify the type of record sent by a
  specific client probe; classifications use the format DOMAIN/PROBE/REST, where
  DOMAIN is the vendor of the probe, PROBE is the probe name, and REST is a
  probe-defined field to classify what is contained in the payload.
* severity: this is an integer value between 1 and 4 where 1 is "low" and 4 is
  "critical"
* machine_id: a machine identifier that is rotate every 3 days for privacy reasons.
* creation_timestamp: timestamp when the record was collected.
* arch: a string describing machine architecture i.e. 'x86_64'.
* host_type: a string with the combination of 'System Vendor', 'Product Name', and
  'Product Version' read from dmi file system.
* build: OS build number.
* kernel_version: Kernel version.
* payload_format_version: version of the payload, currently is 'Version 1'.
* system_name: the value after 'ID=' from '/etc/os-release' (or distribution
  provided folder)
* board_name: a string read from dmi file system that combines 'Board Name' and
  'Board Vendor'.
* cpu_model: cpu model name extracted from '/proc/cpuinfo'.
* bios_version: BIOS version.
* event_id: an id to group multiple records if these were generated by a single
  event occurrence.

The payload as mentioned above is reported by probes. The telemetry library adds
the metadata to the payload (done programatically when using library API) for
more information about probes go [here](./src/probes).


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

The telemetry client reads, at most, the first 32 characters from this file
uses it for the machine id. You can put a string like 'my-machine-name' in this
file to easily identify your machine.  Restart telemprobd for the machine id
changes to take effect by running:

```{r, engine='bash', count_lines}
# systemctl restart telemprobd.service
```

You can switch back to the rotating machine id by deleting the override file
and restarting the client.  You can do a quick test to check that your
machine-id has changed by running "hprobe" and verifying that a record has
landed on your backend telemetrics server, with the specified machine id.

Event Id
----------------------

This is a 32 character lowercase hexadecimal string i.e. '5de9de8d5f3c6a7d445d75ba01cc3322'.
This header is used to group multiple records by an event id. Before this header
every single record could have been thought of an event, however this is not always
the case. There are "events" that trigger the creation of multiple records (i.e.
updates). The ```event_id``` header was added for probes with the capability to
detect events and group records based on such events. This header was added to
```telem-record-gen``` and can be specified using the ```-e``` (```--event-id```
long form) switch.

```{r, engine=bash, count_lines}
  -e, --event-id        Event id to use in the record
```

Debugging locally with telemetrics-client
-----------------------------------------------

The function of the telemetrics-client is to handle the transport of information
reported by a probe to a backend (see ```server``` key in configuration). This
information is helpful for developers to debug and fix reported crashes, however
developers not always have access to the backend in these case users can leverage
features added for local debugging. The following is a list of steps to enable
local debug:

* *Enabling record retention*: this step configures telempostd to keep
copies of telemetry records locally. To enable record retention set the value of
```record_retention_enabled``` from ```false``` to ```true```. Optionally set
```record_server_delivery_enabled`` to ```false``` to keep records local only.
Remember to restart the client after configuration values are updated
(```telemctl restart```).

* *Creating a record*: run ```hprobe``` command to create a record for the purposes
of this step by step guide. Once we have the record or records that you need to
capture locally you can display the data.

* *Displaying record metadata*: telempostd keeps metadata of any valid record,
to display this data a new option to telemctl was added ```telemctl journal```.
Assuming that the last record created was the record from previous step `hprobe` we
can use `tail -n 1` to print the last created record only, i.e.

```
$ sudo telemctl journal | tail -n 1
$ org.clearlinux/hello/world     Mon 2018-04-02 17:48:01 UTC a19a0d41ba16788881e274b19b8a1be4 5de9de8d5f3c6a7d445d75ba01cc3322 60c014cd-4693-40f1-b334-548cd932949b
```

The headers for the metadata (along with other information) can be printed using the
```-V``` switch with ```telemctl journal``` command, i.e.

```
$ sudo telemctl journal -V | head -n 1
$ Classification             Time stamp              Record ID                    Event ID                     Boot ID
```

* *Displaying record payload*: to print the content of a record payload you can use
the ```-i``` (```--include_record``` long format) option to ```telemctl journal```
command. To print the specific record you created you can use the option ```-r```
(```--record_id``` long format) with the Record Id of the generated record, i.e.

```
$ sudo telemctl journal --record_id a19a0d41ba16788881e274b19b8a1be4 --include_record
$ org.clearlinux/hello/world     Mon 2018-04-02 17:48:01 UTC a19a0d41ba16788881e274b19b8a1be4 5de9de8d5f3c6a7d445d75ba01cc3322 60c014cd-4693-40f1-b334-548cd932949b
$ hello
```

## Security Disclosures

To report a security issue or receive security advisories please follow procedures
in this [link](https://01.org/security).
