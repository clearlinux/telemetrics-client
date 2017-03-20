======
telemd
======

------------------------
Telemetry client service
------------------------

:Copyright: \(C) 2017 Intel Corporation, CC-BY-SA-3.0
:Manual section: 1


SYNOPSIS
========

``telemd`` \<flags\>

``/etc/telemetrics/telemetrics.conf``


DESCRIPTION
===========

The ``telemd`` program delivers locally generated telemetry records to a remote
telemetry service. Telemetry data can be in any format, and is relayed as-is.


OPTIONS
=======

  * ``-f``, ``--config_file`` \[\<file\>\]:
    Configuration file. This overides the other parameters.

  * ``-h``, ``--help``:
    Display this help message.

  * ``-V``, ``--version``:
    Print the program version.


EXIT STATUS
===========

0 when no errors occurred. A non-zero exit status indicates a failure occurred.


SEE ALSO
========

* ``telemetry``\(3)
* ``telemetrics-client.conf``\(5)
* https://github.com/clearlinux/telemetrics-client
* https://clearlinux.org/documentation/

