Disk Storage
============

.. highlight:: bash

Provides support for using disk storage devices.


Block Devices
-------------

Internal devices such as SPI flash may be read and written in bytes, however disk devices
must be accessed in blocks. Historically these blocks are referred to as 'sectors',
which are 512 bytes for SD cards.

Sector-addressable (block) devices should be based on :cpp:class:`Disk::BlockDevice`.
An example is the :cpp:class:`Storage::SD::Card` provided by the :library:`SD Storage <SdStorage>` library.
Note that devices larger than 4GB will require the :envvar:`ENABLE_STORAGE_SIZE64` project setting.


Partitioning
------------

Partitions can be enumerated and created using this library. Both
`MBR <https://en.wikipedia.org/wiki/Master_boot_record>`__
and
`GPT <https://en.wikipedia.org/wiki/GUID_Partition_Table>`__
partitioning schemes are supported.

Call :cpp:func:`Storage::Disk::scanPartitions` to populate the device partition table.
For each partition, additional information can be obtained via :cpp:func:`Storage::Partition::diskpart` call.

Partition information is written using :cpp:func:`Storage::Disk::formatDisk`.


Buffering
---------

Block devices are typically used with a sector-based filing system such as ``FAT`` (see :library:`FatIFS`).
Note that these filing systems are generally unsuitable for use with SPI flash without some kind
of intermediate wear-levelling management layer as provided by SD card controllers.

By default, block devices must be strictly accessed only by sector, i.e. for SD cards aligned chunks of 512 bytes.
This is rather inflexible so :cpp:class:`BlockDevice` supports byte-level access using internal buffering,
which applications may enable using the `allocateBuffers` method.

This allows other filing systems to be used. :library:`LittleFS` seems to work OK, although :library:`Spiffs` does not.
Partitions may also be used directly without any filing system.

.. important::

   The :cpp:func:`Device::sync` method must be called at appropriate times to ensure data is actually written
   to disk. Filing systems should take care of this internally when files are closed, for example.


Testing
-------

Linux provides excellent support for testing generated image files.

Windows users may find this tool useful: https://www.diskinternals.com/linux-reader/.


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
