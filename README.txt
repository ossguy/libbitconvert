This is the README for libbitconvert, a library that decodes bits from magnetic
stripe cards into the ALPHA and BCD formats and provides a list of fields and
values for cards described in the formats file.

Before checking out the project on Windows, please turn on the "core.autocrlf"
option in git (run "git config --global core.autocrlf true").  This will convert
newlines in the checked out files into Windows-style newlines so that editors
like Notepad can read them easily.

To build the library and test driver, run "make" from the root directory.  This
should work on any POSIX system with a C compiler, such as Ubuntu with build
tools (run "sudo apt-get install build-essential"), Mac OS X with Xcode, or
Windows with MinGW and MSYS.  libbitconvert requires libpcre, which is available
in Ubuntu 8.04 and 8.10 by running "sudo apt-get install libpcre3 libpcre3-dev".
Mac OS X users can acquire libpcre by installing the "pcre" port in MacPorts.
libpcre for Windows is at http://gnuwin32.sourceforge.net/packages/pcre.htm in
"Complete package, except sources".

To use the library, you can run "./driver" (the test driver), which reads ASCII
1s and 0s from standard input.  The driver expects Track 1 data to be on the
first line of standard input, Track 2 data on the second line of standard
input, and Track 3 data on the third line of standard input.  To specify no
data for a particular track, simply use an empty line.

Example bitstreams are available in the test_data directory.  You can run them
through the test driver using a command like "./driver < test_data/eb_edge".

Alternatively, you can write your own application that #includes bitconvert.h
and links with libbitconvert.a, but beware that the API is not yet stable so
you may have to update your application regularly to keep up with the changes.

For more information on the ALPHA and BCD formats, see
http://www.cyberd.co.uk/support/technotes/isocards.htm.

--
  Copyright (C) 2009  Denver Gingerich <denver@ossguy.com>

  Copying and distribution of this file, with or without modification,
  are permitted in any medium without royalty provided the copyright
  notice and this notice are preserved.
