telemd(1) -- Telemetry client service
=====================================

## SYNOPSIS

`telemd` <flags> 
`/etc/telemetrics/telemetrics.conf`

## DESCRIPTION

The `telemd` program delivers locally generated telemetry records to a remote
telemetry service. Telemetry data can be in any format, and is relayed as-is.

## OPTIONS

  * `-f`, `--config_file` [<file>]:
    Configuration file. This overides the other parameters.

  * `-h`, `--help`:
    Display this help message.

  * `-V`, `--version`:
    Print the program version.


## EXIT STATUS

0 when no errors occurred. A non-zero exit status indicates a failure occurred.

## COPYRIGHT

 * Copyright (C) 2016 Intel Corporation, License: CC-BY-SA-3.0

## SEE ALSO

`telemetry(3)`
`telemetrics-client.conf(5)`

https://github.com/clearlinux/telemetrics-client

https://clearlinux.org/documentation/

## NOTES

Creative Commons Attribution-ShareAlike 3.0 Unported

 * http://creativecommons.org/licenses/by-sa/3.0/
