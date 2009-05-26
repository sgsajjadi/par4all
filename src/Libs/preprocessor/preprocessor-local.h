/* Preprocessing and splitting of Fortran and C files
 *
 * $Id$
 */

/* the name of the environment variable where source files are searched for. */
#define SRCPATH "PIPS_SRCPATH"

#define FORTRAN_FILE_SUFFIX ".f"
#define RATFOR_FILE_SUFFIX ".F"
#define C_FILE_SUFFIX ".c"
#define FORTRAN_INITIAL_FILE_SUFFIX ".f_initial"

/* an issue is that the preprocessor used for .F must be Fortran 77 aware.
 */

/* The extensions used by the various source file types involved by the
   preprocessor: */
#define PP_FORTRAN_ED		 	".fpp_processed.f"
#define PP_C_ED		 	".cpp_processed.c"
#define PP_ERR			".stderr"

/* pre-processor and added options from environment
 */
#define CPP_PIPS_ENV		"PIPS_CPP"
#define CPP_PIPS_OPTIONS_ENV 	"PIPS_CPP_FLAGS"
#define FPP_PIPS_ENV		"PIPS_FPP"
#define FPP_PIPS_OPTIONS_ENV 	"PIPS_FPP_FLAGS"

/* default preprocessor and basic options
 */
#define CPP_CPP			"cpp -C" /* alternative values: "gcc -E -C" */
/* #define CPP_CPPFLAGS		" -P -D__PIPS__ -D__HPFC__ " */
/* -U__GNUC__ seems to be still useful to avoid spoiling the libC files
    with too many GCC extensions: */
#define CPP_CPPFLAGS		" -D__PIPS__ -D__HPFC__ -U__GNUC__ "
#define FPP_CPP			"cpp -C" /* alternative values: "gcc -E -C" or "fpp" */
#define FPP_CPPFLAGS		" -P -D__PIPS__ -D__HPFC__ "

#define DEFAULT_PIPS_FLINT "f77 -c -ansi"


/* Define some functions from the .l or .y since cproto cannot dig them out: */
void MakeTypedefStack();
void ResetTypedefStack();
