=========
telemetry
=========

----------------------------------------------
C programming interface for telemetrics-client
----------------------------------------------

:Copyright: \(C) 2017 Intel Corporation, CC-BY-SA-3.0
:Manual section: 3


SYNOPSIS
========

``#include "telemetry.h"``
  
``struct telem_ref { struct telem_record *record; };``
  
``int tm_create_record(struct telem_ref **t_ref, uint32_t severity, char *classification, uint32_t payload_version)``

``int tm_set_payload(struct telem_ref *t_ref, char *payload)``

``int tm_send_record(struct telem_ref *t_ref)``

``void tm_free_record(struct telem_ref *t_ref)``

``void tm_set_config_file(char *c_file)``


DESCRIPTION
===========

The functions in the telemetry library facilitate the delivery of
telemetry data to the ``telemd``\(1) service.

The function ``tm_create_record()`` initializes a telemetry record and
sets the severity and classification of that record, as well as the
payload version number. The memory needed to store the telemetry record
is allocated and should be freed with ``tm_free_record()`` when no longer
needed.

The function ``tm_set_payload()`` attaches the provided telemetry record
data to the telemetry record. The current maximum payload size is 8192b.

The function ``tm_send_record()`` delivers the record to the local
``telemd``\(1) service.

The function ``tm_set_config_file()`` can be used to provide an alternate
configuration path to the telemetry library.


RETURN VALUES
=============

All these functions return ``0`` on success, or a non-zero return value
if an error occurred. The functions ``tm_free_record()`` and ``tm_set_config_file()``
do not return any values.


SEE ALSO
========

* ``telemd``\(1)
* https://github.com/clearlinux/telemetrics-client
* https://clearlinux.org/documentation/

