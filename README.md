![libvidcap][2]
====

[libvidcap][1] is a cross-platform library for capturing video from webcams and other video capture devices. 

This repo is created to add and test some new features and improvements from the original sources. Development targets are:

- ~~cmake support~~
- ~~doxygen documentation~~
- testing supported backends
- folder input support
- v4l2 support
- different camera SDK support
- code refactoring and simplification

For more information about the library and it's internals, please visit the [wiki][3].

----------

Building libvidcap
----

Clone the repo (lets assume that you have a folder `/Users/YOU/src`)

  ```bash
  cd /Users/YOU/src
  git clone https://github.com/daar/libvidcap.git
  ```
 the above will create the folder `/Users/YOU/libvidcap`
 
 To build libvidcap:
 
  ```bash
  cd /Users/YOU/
  mkdir build
  cd build
  cmake ../libvidcap
  make
  ```
Supported video backends are;
- Linux : V4L
- Windows : DirectShow
- Apple : Quicktime

[1]: https://sourceforge.net/projects/libvidcap/
[2]: https://github.com/daar/libvidcap/blob/master/doc/libvidcap-logo.png
[3]: https://github.com/daar/libvidcap/wiki
