telemctl(1) -- Telemetry service administration tool
====================================================

## SYNOPSIS

`telemctl`

`/etc/telemetrics/opt-out`

## DESCRIPTION

Control actions for telemetry services. The command can be used to start,
restart, or stop `telemd(1)`, or to opt-in or opt-out of telemetry delivery
of records to a central telemetry service.

## OPTIONS

 * `start`|`stop`|`restart`:
   Starts, stops or restarts all running telemetry services.

 * `opt-in`: 
   Opts in to telemetry, and starts telemetry services. The opt-out file
   `/etc/telemetrics/opt-out` is removed.

 * `opt-out`: 
   Opts out of telemetry, and stops telemetry services. The opt-out file
   `/etc/telemetrics/opt-out` is created.

## RETURN VALUES

0 on success. A non-zero exit code indicates a failure occurred.

## COPYRIGHT

 * Copyright (C) 2016 Intel Corporation, License: CC-BY-SA-3.0

## SEE ALSO

`telemd(1)`

https://github.com/clearlinux/telemetrics-client

https://clearlinux.org/documentation/

## NOTES

Creative Commons Attribution-ShareAlike 3.0 Unported

 * http://creativecommons.org/licenses/by-sa/3.0/
