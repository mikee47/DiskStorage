Disk Storage
============

.. highlight:: bash

Provides support for using disk storage devices.

Partitions can be enumerated and created using this library.

`MBR <https://en.wikipedia.org/wiki/Master_boot_record>`__
and
`GPT <https://en.wikipedia.org/wiki/GUID_Partition_Table>`__
partitioning schemes are supported.


Configuration
-------------

.. envvar:: DISK_MAX_SECTOR_SIZE

   default: 512

   Determines the minimum supported read/write block size for storage devices.

   For example, AF disks use 4096-byte sectors so internal reads and writes must be a multiple of this value.
   This will increase the internal buffer sizes and so consume more RAM.


Acknowledgements
----------------

Code in this library is based on http://elm-chan.org/fsw/ff/00index_e.html.
It has been heavily reworked using structures (from the Linux kernel)
and separated from the actual FAT filing system implementation (:library:`FatIFS`).


API
---

.. doxygennamespace:: Storage::Disk
   :members:
