/*
 * $Id$
 *
 * $Log: optimize.c,v $
 * Revision 1.13  1998/11/18 22:43:06  coelho
 * switch_nary_to_binary and optimize_simplify_patterns are called.
 *
 * Revision 1.12  1998/11/09 14:50:35  zory
 * use make_constant function instead of specific float and integer
 * make_float/integer_constant_entity funcions
 *
 * Revision 1.11  1998/11/04 09:42:57  coelho
 * use properties for eole and its options.
 *
 * Revision 1.10  1998/11/04 08:59:06  zory
 * double and float format updated
 *
 * Revision 1.9  1998/10/22 11:30:29  zory
 * double type for const values added
 *
 * Revision 1.8  1998/10/20 14:51:23  zory
 * move the free statement for all strings inside the if statement
 *
 * Revision 1.7  1998/10/20 14:48:47  zory
 * free the list of unoptimized expressions only when there are some
 * expressions in the module ! (if statement)
 *
 * Revision 1.6  1998/09/17 12:08:43  zory
 * taking into account new entity from eole
 *
 * Revision 1.5  1998/09/14 12:50:15  coelho
 * more comments.
 *
 * Revision 1.4  1998/09/14 12:34:11  coelho
 * added import from eole and substitution in module code.
 *
 * Revision 1.3  1998/09/11 12:18:39  coelho
 * new version thru a reference (suggested by PJ).
 *
 * Revision 1.2  1998/09/11 09:42:49  coelho
 * write equalities...
 *
 * Revision 1.1  1998/04/29 09:07:42  coelho
 * Initial revision
 *
 *
 * expression optimizations by Julien.
 */

#include <stdio.h>

#include "linear.h"

#include "genC.h"
#include "ri.h"
#include "ri-util.h"

#include "misc.h"
#include "properties.h"

#include "resources.h"
#include "pipsdbm.h"


#define DEBUG_NAME "TRANSFORMATION_OPTIMIZE_EXPRESSIONS_DEBUG_LEVEL"


/********************************************************* INTERFACE TO EOLE */

/* the list of right hand side expressions.
 */
static list /* of expression */ rhs;

/* the current list of loop indices for the expression (not used yet).
 */
static list /* of entity */ indices; 

/* rhs expressions of assignments.
 */
static bool call_filter(call c)
{
    if (ENTITY_ASSIGN_P(call_function(c)))
    {
	expression e = EXPRESSION(CAR(CDR(call_arguments(c))));
	rhs = CONS(EXPRESSION, e, rhs);
    }
    return FALSE;
}

/* other expressions may be found in loops and so?
 */
static bool expr_filter(expression e)
{
    rhs = CONS(EXPRESSION, e, rhs);
    return FALSE;
}

static list /* of expression */ 
get_list_of_rhs(statement s)
{
    list result;

    rhs = NIL;
    gen_multi_recurse(s,
		      expression_domain, expr_filter, gen_null,
		      call_domain, call_filter, gen_null,
		      NULL);
    
    result = gen_nreverse(rhs);
    rhs = NIL;
    return result;
}

/* export a list of expression of the current module.
 * done thru a convenient reference.
 */
static void write_list_of_rhs(FILE * out, list /* of expression */ le)
{
    reference astuce = make_reference(get_current_module_entity(), le);
    write_reference(out, astuce);
    reference_indices(astuce) = NIL;
    free_reference(astuce);
}

/* export expressions to eole thru the newgen format.
 * both entities and rhs expressions are exported. 
 */
static void write_to_eole(string module, list le, string file_name)
{
    FILE * toeole;

    pips_debug(3, "writing to eole for module %s\n", module);

    toeole = safe_fopen(file_name, "w");
    write_tabulated_entity(toeole);
    write_list_of_rhs(toeole, le);

    safe_fclose(toeole, file_name);
}



#define SIZE_OF_BUFFER 100

static string
read_and_allocate_string_from_file(FILE * file)
{
  int test; 
  char buffer[SIZE_OF_BUFFER];

  test = fscanf(file, "%s",buffer);
  pips_assert("fscanf - read string from file \n",(test==1));

  return strdup(buffer);
}


/* import a list of entity that have been created during the eole
 * transformations and create them
 */
static void 
read_new_entities_from_eole(FILE * file, string module){
  int num = 0;
  int i, test;
  string ent_type;
  string const_type;
  int const_size = 0;
  string const_value;
  
  entity e; 
  
  /* read the number of new entities to create */
  test = fscanf(file,"%d\n",&num);
  pips_assert("fscanf - read number of entity \n",(test==1));
  pips_debug(3,"reading %d new entity from module %s\n", num, module);
  for (i=0;i<num;i++){
    
    ent_type = read_and_allocate_string_from_file(file);
    
    if (!strcmp(ent_type,"constant")) { /* constant */
      
      const_type = read_and_allocate_string_from_file(file);
      
      test = fscanf(file," %d\n", &const_size);
      pips_assert("fscanf - read entity basic type size \n",(test==1));

      const_value = read_and_allocate_string_from_file(file);

      if (!strcmp(const_type,"int")) {/* int */
	
	/* create integer entity */
	e = make_constant_entity(const_value, is_basic_int, const_size);
	pips_assert("make integer constant entity\n",
		    entity_consistent_p(e));  
      }
      else 
	if (!strcmp(const_type,"float")) {/* float */
	  
	  /* create float entity */
	  e = make_constant_entity(const_value, is_basic_float, const_size);
	  pips_assert("make float constant entity\n",
		      entity_consistent_p(e));  
	}
      else 
	pips_error("read_new_entities_from_eole", 
		   "can't create this kind of constant entity : %s",
		   const_type);

      free(const_type);
      free(const_value);
    }
    else 
      pips_error("read_new_entities_from_eole", 
		 "can't create this kind of entity : %s",
		 ent_type);
    
    free(ent_type);
  }
}   


/* import expressions from eole.
 */
static list /* of expression */
read_from_eole(string module, string file_name)
{
    FILE * fromeole;
    reference astuce;
    list result;

    pips_debug(3, "reading from eole for module %s\n", module);
    
    fromeole = safe_fopen(file_name, "r");

    /* read entities to create... 
     * should use some newgen type to do so (to share buffers...)
     */
    read_new_entities_from_eole(fromeole, module);

    astuce = read_reference(fromeole);
    result = reference_indices(astuce);
    reference_indices(astuce) = NIL;
    free_reference(astuce);

    return result;
}

/* swap term to term syntax field in expression list, as a side effect...
 */
static void 
swap_syntax_in_expression(  list /* of expression */ lcode,
			    list /* of expression */ lnew)
{
  pips_assert("equal length lists", gen_length(lcode)==gen_length(lnew));
  
  for(; lcode; lcode=CDR(lcode), lnew=CDR(lnew))
    {
      expression old, new;
      syntax tmp;
      
      old = EXPRESSION(CAR(lcode));
      new = EXPRESSION(CAR(lnew));
      
      tmp = expression_syntax(old);
      expression_syntax(old) = expression_syntax(new);
      expression_syntax(new) = tmp;	
    }
}

/************************************************************** EOLE PROCESS */

/* file name prefixes to deal with eole.
 * /tmp should be fast (may be mapped into memory).
 */
#define OUT_FILE_NAME 	"/tmp/pips_to_eole"
#define IN_FILE_NAME	"/tmp/eole_to_pips"

/* property names.
 */
#define EOLE		"EOLE"		/* eole binary */
#define EOLE_FLAGS	"EOLE_FLAGS"	/* default options */
#define EOLE_OPTIONS	"EOLE_OPTIONS"	/* additionnal options */

/* returns the eole command to be executed in an allocated string.
 */
static string 
get_eole_command
  (string in, /* input file from eole. */
   string out /* output file to eole. */)
{
  return strdup(concatenate(get_string_property(EOLE), " ", 
			    get_string_property(EOLE_FLAGS), " ", 
			    get_string_property(EOLE_OPTIONS), 
			    " -o ", in, " ", out, NULL));
}

/*************************************************** INTERFACE FROM PIPSMAKE */

/* pipsmake interface.
 */
bool optimize_expressions(string module_name)
{
    statement s;
    list /* of expression */ le, ln;

    ln = NIL;

    debug_on(DEBUG_NAME);

    /* get needed stuff.
     */
    set_current_module_entity(local_name_to_top_level_entity(module_name));
    set_current_module_statement((statement)
        db_get_memory_resource(DBR_CODE, module_name, TRUE));

    s = get_current_module_statement();

    /* do something here.
     */

    /* Could perform more optimizations here...
     */

    /* check consistency before optimizations */
    pips_assert("consistency checking before optimizations",
		statement_consistent_p(s));

    /* begin EOLE stuff
     */
    le = get_list_of_rhs(s);
    if (gen_length(le)) /* not empty list */
    {
      string in, out, cmd;

      /* create temporary files */
      in = safe_new_tmp_file(IN_FILE_NAME);
      out = safe_new_tmp_file(OUT_FILE_NAME);
      
      /* write informations in out file for EOLE */
      write_to_eole(module_name, le, out);

      /* run eole (Evaluation Optimization for Loops and Expressions) 
       * as a separate process.
       */
      cmd = get_eole_command(in, out);

      pips_debug(2, "executing: %s\n", cmd);
      
      safe_system(cmd);
      
      /* read optimized expressions from eole */
      ln = read_from_eole(module_name, in);

      /* replace the syntax values inside le by the syntax values from ln */
      swap_syntax_in_expression(le, ln);

      /* must now free the useless expressions */
      

      /* remove temorary files and free allocated memory.
       */
      safe_unlink(out);
      safe_unlink(in);

      /* free strings */
      free(out), out = NULL;
      free(in), in = NULL;
      free(cmd), cmd = NULL;

    }
    else 
      pips_debug(3, "no expression for module %s\n", module_name);


    /* free lists */
    gen_free_list(ln);
    gen_free_list(le);

    pips_debug(3,"EOLE transformations ... Done for module %s\n", module_name);

    /* end EOLE stuff.
     */

    /* check consistency after optimizations */
    pips_assert("consistency checking after optimizations",
		statement_consistent_p(s));
    

    /* Could perform more optimizations here...
     */
    /* CSE/ICM + atom
     */
    switch_nary_to_binary(s);
    optimize_simplify_patterns(s);
    /* others?
     */

    /* return result to pipsdbm
     */
    DB_PUT_MEMORY_RESOURCE(DBR_CODE, module_name, s);

    reset_current_module_entity();
    reset_current_module_statement();

    debug_off();

    return TRUE; /* okay ! */
}



