###
### Par4All Environment
### 
#
#  Run 'source <this file>' from your csh-compatible shell.
#
##########################################################

# Par4All source root. Might point to P4A_DIST if 
# sources are not installed.
setenv P4A_ROOT '$root'

# Path to the Par4All installation.
setenv P4A_DIST '$dist'

# Location of the Par4All_accelerator files.
setenv P4A_ACCEL_DIR '$accel'

# Location of the Par4All configuration files.
setenv P4A_ETC $$P4A_DIST/etc

# The Fortran 77 compiler to use.
setenv PIPS_F77 $fortran

# Update PATH.
setenv PATH $$P4A_DIST/bin:$$PATH

# Update libraries search paths.
setenv PKG_CONFIG_PATH $$P4A_DIST/lib64/pkgconfig:$$P4A_DIST/lib/pkgconfig:$$PKG_CONFIG_PATH
setenv LD_LIBRARY_PATH $$P4A_DIST/lib64:$$P4A_DIST/lib:$$LD_LIBRARY_PATH

# Update Python module search path with PIPS Python bindings (PyPS).
setenv PYTHONPATH `pkg-config pips --variable=pkgpythondir`:/usr/share/pyshared

rehash
