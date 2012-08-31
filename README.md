storaged
========

The storaged daemon is responsible for enabling and disabling Mass Storage Mode (MSM) on the platform. MSM allows the mounting of a partition of the storage on the platform as an external drive on a host computer using the mass storage device class USB protocol. Storaged is an on-demand daemon, i.e. it is only started when the platform is connected to a host computer via USB and exits as soon as it is disconnected.

How to Build on Linux
=====================

## Dependencies

Below are the tools and libraries (and their minimum versions) required to build storaged:

* cmake 2.6
* gcc 4.3
* glib-2.0 2.16.6
* make (any version)
* openwebos/cjson 1.8.0
* openwebos/luna-service2 3.0.0
* openwebos/nyx-lib 2.0.0 RC 2
* pkg-config 0.22


## Building

Once you have downloaded the source, execute the following to build it:

    $ mkdir BUILD
    $ cd BUILD
    $ cmake ..
    $ make
    $ sudo make install

The daemon will be installed under

    /usr/local/sbin

and the udev scripts under

    /usr/local/etc/udev

You can install it elsewhere by supplying a value for _CMAKE\_INSTALL\_PREFIX_ when invoking _cmake_. For example:

    $ cmake -D CMAKE_INSTALL_PREFIX:STRING=$HOME/projects/openwebos ..
    $ make
    $ make install
    
will install the files in subdirectories of $HOME/projects/openwebos instead of subdirectories of /usr/local. 

Specifying _CMAKE\_INSTALL\_PREFIX_ also causes the pkg-config files under it to be used to find headers and libraries. To have _pkg-config_ look in a different tree, set the environment variable PKG_CONFIG_PATH to the path to its _lib/pkgconfig_ subdirectory.

## Uninstalling

From the directory where you originally ran _make install_, invoke:

    $ sudo xargs rm < install_manifest.txt

# Copyright and License Information

Unless otherwise specified, all content, including all source code files and
documentation files in this repository are:

Copyright (c) 2002-2012 Hewlett-Packard Development Company, L.P.

Unless otherwise specified or set forth in the NOTICE file, all content,
including all source code files and documentation files in this repository are:
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this content except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

