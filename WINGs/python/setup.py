#!/usr/bin/env python

"""Setup script for the WINGs module distribution."""

import os, sys
from distutils.core import setup
from distutils.extension import Extension

## Get the include dirs
wings = os.popen("get-wings-flags --cflags", "r")
lines = [x.strip() for x in wings.readlines()]
flags = reduce(lambda x,y: x+y, [x.split() for x in lines if x])
include_dirs = [x[2:] for x in flags]
wings.close()

## Get the library dirs
wings = os.popen("get-wings-flags --ldflags", "r")
lines = [x.strip() for x in wings.readlines()]
flags = reduce(lambda x,y: x+y, [x.split() for x in lines if x])
library_dirs = [x[2:] for x in flags]
wings.close()

## Get the libraries
wings = os.popen("get-wings-flags --libs", "r")
lines = [x.strip() for x in wings.readlines()]
flags = reduce(lambda x,y: x+y, [x.split() for x in lines if x])
libraries = [x[2:] for x in flags]
wings.close()

runtime_library_dirs = []
extra_objects = []
extra_compile_args = ['-Wno-strict-prototypes', '-Wno-unused']

long_description = \
"""Python interface to the WINGs library

Python WINGs is an interface to WINGs, a small widget set with the
N*XTSTEP look and feel. It's API is inspired in OpenStep and it's
implementation borrows some ideas from Tk. It has a reasonable set of
widgets, sufficient for building small applications (like a CDPlayer
or hacking something like rxvt). It also has other functions that are
usefull for applications, like a User Defaults alike configuration
manager and a notification system.

"""

setup (# Distribution meta-data
        name = "Python-WINGs",
        version = "0.81.0",
        description = "A python interface to WINGs",
        long_description=long_description,
        author = "Dan Pascu",
        author_email = "dan@windowmaker.org",
        license = "GPL",
        platforms = "ALL",
        url = "http://windowmaker.org/",

        # Description of the modules and packages in the distribution

        py_modules = ["WINGs"],

        ext_modules = [Extension(
                name='wings',
                sources=['WINGs.c'],
                include_dirs=include_dirs,
                library_dirs=library_dirs,
                runtime_library_dirs=runtime_library_dirs,
                libraries=libraries,
                extra_objects=extra_objects,
                extra_compile_args=extra_compile_args,
                )],
)
