Geany - A fast and lightweight IDE
----------------------------------


About
-----
Geany is a small and lightweight integrated development environment.
It was developed to provide a small and fast IDE, which has only a
few dependencies from other packages. Another goal was to be as independent
as possible from a special Desktop Environment like KDE or GNOME. So it
is using only the GTK+ toolkit and therefore you need only the
GTK+ runtime libraries to run Geany.


Features
--------
The basic features of Geany are:

- syntax highlighting
- code completion
- auto completion of often used constructs like if, for and while
- auto completion of XML and HTML tags
- call tips
- folding
- many supported filetypes like C, Java, PHP, HTML, Python, Perl, Pascal
- symbol lists
- embedded terminal emulation
- extensibility through plugins


Installation from distribution packages
---------------------------------------
Using distribution packages on Linux, BSD and similar distributions
is the easiest and recommended way. This way you will also benefit
from automatic Geany updates by the package manager of the distribution.

Packages are available for most distributions including Debian, Fedora, Ubuntu
and many more.


Installation on Mac OS and Windows
----------------------------------
Prebuilt binary packages for Mac OS and Windows can be found on
https://www.geany.org.


Installation from sources
-------------------------

Requirements
++++++++++++
For compiling Geany yourself, you will need the GTK2 (>= 2.24) or
GTK3 libraries and header files. You will also need its dependency libraries
and header files, such as Pango, Glib and ATK. All these files are
available at https://www.gtk.org.

Furthermore you need, of course, a C compiler and the Make tool; a C++
compiler is also needed for the required Scintilla library included. The
GNU versions of these tools are recommended.


To build the user manual you need *rst2html* from Docutils. A pre-built
version of the manual is available in distribution tarballs and will be used as
fallback if *rst2html* is missing. When building from Git however, that
pre-built version is not included and *rst2html* is required by default.
You can explicitly disable building the user manual using the
``--disable-html-docs`` *configure* flag, but this will result in not
installing a local version of the user manual, and Geany will then try
and open the online version instead when requested.


.. note::
    Building Geany from source on Mac OS and Windows is more complicated
    and is out of scope of this document. For more information on
    building instructions for these platforms, please check the wiki
    at https://wiki.geany.org/howtos/.

Installing from a Git clone
+++++++++++++++++++++++++++

Install Autotools (*automake*, *autoconf* and *libtool*), *intltool*,
and the GLib development files **before** running any of the following
commands, as well as *rst2html* from Docutils (see above for details).
Then, run ``./autogen.sh`` and then follow the instructions for
`installing from a release tarball`_.

Installing from a release tarball
+++++++++++++++++++++++++++++++++

Run the the following three commands::

    $ ./configure
    $ make
    (as root, or using sudo)
    % make install

For more configuration details run ``./configure --help``.

If there are any errors during compilation, check your build environment
and try to find the error, otherwise contact the mailing list or one of
the authors.

See the manual for details (geany.txt/geany.html).


Usage
-----
To run Geany just type::

    $ geany

on a console or use the applications menu from your desktop environment.
For command line options, see the manual page of Geany or run::

    $ geany --help

for details. Or look into the documentation in the *doc/* directory.
The most important option probably is ``-c`` or ``--config``, where you can
specify an alternate configuration directory.


License
-------
Geany is distributed under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.  A copy of this license
can be found in the file COPYING included with the source code of this
program.
The included Scintilla library (found in the subdirectory scintilla/)
has its own license, which can be found in the file scintilla/License.txt
included with the source code of this program.


Ideas, questions, patches and bug reports
-----------------------------------------
See https://www.geany.org/.
If you add something, or fix a bug, please create a pull request at
https://github.com/geany/geany/. Also see the HACKING file.
