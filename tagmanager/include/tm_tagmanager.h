/*
*
*   Copyright (c) 2001-2002, Biswapesh Chattopadhyay
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*/

#ifndef TM_TAGMANAGER_H
#define TM_TAGMANAGER_H

#include "tm_tag.h"
#include "tm_symbol.h"
#include "tm_file_entry.h"
#include "tm_workspace.h"
#include "tm_work_object.h"
#include "tm_source_file.h"
#include "tm_project.h"

/*! \mainpage Introduction
 \section Introduction
 TagManager is a library and a set of utility programs which can be integrated into
 Integrated Development Environments to provide features such as code completion,
 calltips, etc. Tag Manager is based on <a href="http://ctags.sourceforge.net">
 Exuberent Ctags</a> with some added features.
 \section Licence
 TagManager is <a href="http://www.gnu.org/philosophy/free-sw.html">free software</a>,
 licenced under the <a href="http://www.gnu.org/licenses/gpl.html">GPL</a>. You can only
 use it with free software (GPL compatible) projects. This is chiefly because it uses
 code from ctags which is under GPL. I plan to replace the ctags part with a custom parser
 in the future in which case it will be placed under LGPL. If you want to use it with
 a commercial project, feel free to contribute a C/C++/Java parser.
 \section Hacking
 TagManager is tested to work on Linux. You need a basic GNOME 1.4 installation and
 the auto tools (autoconf, automake, etc) if you want to configure it to your taste.
 <a href="http://anjuta.sourceforge.net/">Anjuta</a> 0.1.7 or above is recommended
 if you want to hack on the sources. If you simply plan to use it, only GLib is required.
 \section Installation
 TagManager can be installed using the standard UNIX method, i.e.:
 
 	-# tar zxvf TagManager-[Version].tar.gz
 	-# cd TagManager-[Version]
 	-# ./configure [configure options]
	-# make
 
 Currently, 'make install' will not do anything since TagManager is meant to be
 included statically in projects. This might change in the future.
 \section Usage
 I have tried to make the API as simple as possible. Memory allocation/deallocation
 is mostly automatic, so you shouldn't have to directly malloc() or free()
 anything. The API is pseudo-OO, similar is structure to the GTK+ API. This document
 provides a good reference to the API. However, if you want to see real-world usage
 of the API, then the utility programs are the best place to start. Currently, there
 are three such programs in the 'tests' subdirectory, namely tm_tag_print.c,
 tm_project_test.c and tm_global_tags.c. It might be a good idea if you go through the
 code for these programs once before starting to use the tag manager library.
 \section Limitations
 Currently, only C, C++ and Java are supported. Adding other languages should be easy
 but I did not require them so they are missing. Tag Manager is also MT-unsafe - this is
 a deliberate design decision since making it MT-safe did not seem to be very useful
 to me and it slows things down. Previous versions has rudimentary code to support MT
 operations but they have been removed since version 0.5.
*/

/*! \file
 Include this file in all programs using the tag manager library. Including this
 automatically includes all the necessary files, namely, tm_tag.h, tm_source_file.h
 , tm_project.h and tm_workspace.h
*/

#endif /* TM_TAGMANAGER_H */
