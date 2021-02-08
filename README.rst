Disk Storage
============

.. highlight:: bash

This Component provides support for using disk storage devices.


Configuration
-------------

.. envvar:: DISK_MAX_SECTOR_SIZE

   default: 512

   Determines the minimum supported read/write block size for storage devices.

   For example, AF disks use 4096-byte sectors so internal reads and writes must be a multiple of this value.
   This will increase the internal buffer sizes and so consume more RAM.
