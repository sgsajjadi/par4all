#!/bin/sh

###
### Par4All Environment
### 
#
#  Run 'source <this file>' from your csh-compatible shell.
#
##########################################################

# Par4All source root. Might point to P4A_DIST if 
# sources are not installed.
setenv P4A_ROOT '$ROOT'

# Path to the Par4All installation.
setenv P4A_DIST '$DIST'

# Location of the Par4All_accelerator files.
setenv P4A_ACCEL_DIR $$P4A_ROOT/src/p4a_accel

# Location of the Par4All configuration files.
setenv P4A_ETC $$P4A_DIST/etc

# The Fortran compiler to use.
setenv PIPS_F77 gfortran

# Update PATH.
setenv PATH $$P4A_DIST/bin:$$PATH

# Update libraries search paths.
setenv PKG_CONFIG_PATH $$P4A_DIST/lib/pkgconfig:$$PKG_CONFIG_PATH

# Update Python module search path with PIPS Python bindings (PyPS).
setenv PYTHONPATH $$(echo $$P4A_DIST/lib/python*/site-packages/pips):$$PYTHONPATH

rehash
