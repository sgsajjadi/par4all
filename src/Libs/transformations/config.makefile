#
# Source, header and object files used to build the library.
# Do not include the main program source file.

LIB_CFILES =	replace.c \
		loop_unroll.c \
		partial_eval.c \
		prettyprintcray.c \
		strip_mine.c \
		loop_interchange.c \
		interchange.c \
		target.c \
		nest_parallelization.c\
                coarse_grain_parallelization.c\
		dead_code_elimination.c \
		privatize.c \
		array_privatization.c \
		simple_atomize.c \
		standardize_structure.c \
		use_def_elimination.c \
		loop_normalize.c  \
		declarations.c \
		clone.c

LIB_HEADERS =	transformations-local.h

LIB_OBJECTS =	$(LIB_CFILES:%.c=%.o)

