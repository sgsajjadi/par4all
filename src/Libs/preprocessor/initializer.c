/* 
 * $Id$
 */
#include <stdio.h>
#include <string.h>

#include "genC.h"
#include "text.h"

#include "text-util.h"
#include "misc.h"
#include "properties.h"
#include "linear.h"
#include "ri.h"
#include "ri-util.h"
#include "pipsdbm.h"

#include "resources.h"
#include "phases.h"

#define FILE_WARNING 							\
 "!\n"									\
 "!     This module was automatically generated by PIPS and should\n"	\
 "!     be updated by the user with READ and WRITE effects on\n"	\
 "!     formal parameters to be useful...\n"				\
 "!\n"

static string 
nth_formal_name(int n)
{
    char buf[20]; /* hummm... */
    sprintf(buf, "f%d", n);
    return strdup(buf);
}

static sentence
stub_var_decl(parameter p, int n)
{
    sentence result;
    string name = nth_formal_name(n);
    type t = parameter_type(p);
    pips_assert("is a variable", type_variable_p(t));

    if(basic_overloaded_p(variable_basic(type_variable(t)))) 
    {
	string comment = strdup(concatenate(
        "!     Unable to determine the type of parameter number ", name, "\n", 
	"!     ", basic_to_string(variable_basic(type_variable(t))),
	" ", name, "\n", 0));
	free(name);
	result = make_sentence(is_sentence_formatted, comment);
    }
    else
    {
	result = make_sentence(is_sentence_unformatted,
	  make_unformatted(string_undefined, 0, 0, 
            CONS(STRING, 
		 strdup(basic_to_string(variable_basic(type_variable(t)))),
	    CONS(STRING, strdup(" "),
	    CONS(STRING, name, NIL)))));
    }
    return result;
}

static sentence
stub_head(entity f)
{
    list ls = NIL;
    type t = entity_type(f);
    functional fu = type_functional(t);
    int number, n = gen_length(functional_parameters(fu));
    
    /* is it a subroutine or a function? */
    if(!type_void_p(functional_result(fu)))
    {
	type tf = functional_result(fu);
	pips_assert("result is a variable", type_variable_p(tf));
	ls = CONS(STRING, strdup(" FUNCTION "), 
	     CONS(STRING,
		  strdup(basic_to_string(variable_basic(type_variable(tf)))),
		  NIL));
    }
    else 
	ls = CONS(STRING, strdup("SUBROUTINE "), NIL);

    ls = CONS(STRING, strdup(module_local_name(f)), ls);
    
    /* generate the formal parameter list. */
    for(number=1; number<=n; number++) 
	ls = CONS(STRING, nth_formal_name(number),
	     CONS(STRING, strdup(number==1? "(": ", "), ls));

    /* close if necessary. */
    if (number>1) ls = CONS(STRING, strdup(")"), ls);

    return make_sentence(is_sentence_unformatted, 
	   make_unformatted(string_undefined, 0, 0, gen_nreverse(ls)));
}

/* generates the text for a missing module.
 */
static text 
stub_text(entity module)
{
    sentence warning, head;
    type t = entity_type(module);
    int n=1;
    list /* of sentence */ ls = NIL;

    if (type_undefined_p(t))
	pips_user_error("undefined type for %s\n", entity_name(module));

    if (!type_functional_p(t))
	pips_user_error("non functional type for %s\n", entity_name(module));

    warning = make_sentence(is_sentence_formatted, strdup(FILE_WARNING));
    head = stub_head(module);
    
    MAP(PARAMETER, p, 
	ls = CONS(SENTENCE, stub_var_decl(p, n++), ls),
	functional_parameters(type_functional(t)));

    ls = CONS(SENTENCE, make_sentence(is_sentence_unformatted,
	     make_unformatted(string_undefined, 0, 0, 
			      CONS(STRING, strdup("END"), NIL))), ls);
    
    ls = CONS(SENTENCE, warning, CONS(SENTENCE, head, gen_nreverse(ls)));

    return make_text(ls);
}

/* generates a source file for some module, if none available.
 */
static bool 
missing_file_initializer(string module_name)
{
    boolean success_p = TRUE;
    entity m = local_name_to_top_level_entity(module_name);
    string file_name, dir_name, src_name, full_name, init_name, finit_name; 
    /* relative to the current directory */
    FILE * f;
    text stub;
    
    pips_user_warning("no source file for %s: synthetic code is generated\n",
		      module_name);

    if(entity_undefined_p(m))
	pips_internal_error("No entity defined for module %s although it must"
			    " have been encountered at a call site\n");
    
    /* pips' current directory is just above the workspace
     */
    file_name = strdup(concatenate(module_name, ".f", 0));
    file_name = strlower(file_name, file_name);
    dir_name = db_get_current_workspace_directory();
    src_name = strdup(concatenate(WORKSPACE_TMP_SPACE, "/", file_name, 0));
    full_name = strdup(concatenate(dir_name, "/", src_name, 0));
    init_name = 
      db_build_file_resource_name(DBR_INITIAL_FILE, module_name, ".f_initial");
    finit_name = strdup(concatenate(dir_name, "/", init_name, 0));

    /* builds the stub.
     */
    stub = stub_text(m);

    /* put it in the source file and link the initial file.
     */
    db_make_subdirectory(WORKSPACE_TMP_SPACE);
    f = safe_fopen(full_name, "w");
    print_text(f, stub);
    safe_fclose(f, full_name);
    safe_link(finit_name, full_name);

    /* Add the new file as a file resource...
     * should only put a new user file, I guess?
     */
    user_log("Registering synthesized file %s\n", file_name);
    DB_PUT_FILE_RESOURCE(DBR_INITIAL_FILE, module_name, init_name);
    DB_PUT_FILE_RESOURCE(DBR_USER_FILE, module_name, src_name);

    free(file_name), free(dir_name), free(full_name), free(finit_name);
    return success_p;
}

extern bool process_user_file(string); /* ??? in top-level */

static bool
ask_a_missing_file(string module)
{
    string file;
    bool ok, cont;
    
    do {
	file = user_request("please enter a file for module %s\n", module);
	if (file)
	  {
	    if (same_string_p(file, "generate"))
		ok = missing_file_initializer(module);
	    else
		ok = process_user_file(file);
	  }
	cont = file && !same_string_p(file, "quit") &&
	    !db_resource_p(DBR_INITIAL_FILE, module);
	if (file) free(file);
    } while (cont);
    return db_resource_p(DBR_INITIAL_FILE, module);
}

/* there is no real rule to produce source or user files; it was introduced
 * by Remi to deal with source and user files as with any other kind
 * of resources
 */
bool 
initializer(string module_name)
{
    bool success_p = FALSE;
    string missing = get_string_property("PREPROCESSOR_MISSING_FILE_HANDLING");

    if (same_string_p(missing, "error"))
	pips_user_error("no source file for %s (might be an ENTRY point)\n"
			"set PREPROCESSOR_MISSING_FILE_HANDLING"
			" to \"query\" or \"generate\"...\n", module_name);
    else if (same_string_p(missing, "generate")) 
	success_p = missing_file_initializer(module_name);
    else if (same_string_p(missing, "query"))
	success_p = ask_a_missing_file(module_name);
    else 
	pips_user_error("invalid value of property "
			" PREPROCESSOR_MISSING_FILE_HANDLING = \"%s\"",
			missing);

    return success_p;
}
