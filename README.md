# PTFS - Pass-Through Filesystem

This repository contains a simple fuse-based file system which just
pass the operations through to the underlying filesystem.
The main purpose is to make performance measurements for fuse based file systems in
contrast to other solutions.

## Usage
```
./ptfs <root> <mountpoint> [OPTIONS]
```

To umount use the fusermount command.
```
fusermount -zu <mountpoint>
```

## Dependencies

To run and build *ptfs* the following dependencies are required.

* libfuse-dev
* cmake

For Debian based systems just install them by your package manager using:
```
apt install libfuse-dev cmake
```

## Build from Source

To build from source, do the following, make sure that you satisfy the
dependencies. Check-out the repository and run:

```
cd ptfs/
mkdir build
cd build
cmake ../src
make
make install
```

If you want to install PTFS into a certain directory simply set the
CMAKE_INSTALL_PREFIX to the appropriate directory.
```
cmake ../ -DCMAKE_INSTALL_PREFIX=/tmp/foo
```
will install into /tmp/foo.

## LICENSE

PTFS is distributed under the terms of the new BSD-License, see the LICENSE file
for the full license text.
