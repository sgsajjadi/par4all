 /* Interface between the untyped database manager and clean code and
  * between pipsmake and clean code.
  *
  * There are other interface routines in prettyprint.c
  *
  * The top routines should be called by the PIPS make utility. They might
  * eventually be integrated into it.
  *
  * The lower level routines are dealing with "statement_mapping"s. They
  * might have to be updated when mappings are integrated into NewGen.
  * At that time, the whole file should disappear in a typed pipsmake.
  *
  * Francois Irigoin, August 1990
  */

 /* the lowest level routines dealing with "statement_mapping"s are now 
  * generated by a macro defined in mapping.h : DEFINE_CURRENT_MAPPING.
  * see this file for more details.
  *
  * Be'atrice Apvrille, August 1993
  */

/* $Id$ */

#include <stdio.h>
#include <string.h>

#include "genC.h"
#include "database.h"

#include "resources.h"
#include "linear.h"
#include "ri.h"
#include "ri-util.h"
#include "pipsdbm.h"
#include "effects-generic.h"
#include "effects-simple.h"
#include "control.h"

#include "misc.h"

#include "transformer.h"
#include "semantics.h"

#include "properties.h"

/* three mappings used throughout semantics analysis:
 *  - transformer_map is computed in a first phase
 *  - precondition_map is computed in a second phase
 *  - cumulated_effects is used and assumed computed before  (defined in effects.c)
 *  - proper_effects might be useless... only DO loop analysis could use it (idem)
 */


/* declaration of the previously described mappings with their access functions :
 *
 * (DEFINE_CURRENT_MAPPING is a macro defined in ~pips/Newgen/mapping.h)
 *
 * BA, August 26, 1993
 */

DEFINE_CURRENT_MAPPING(transformer, transformer)
DEFINE_CURRENT_MAPPING(precondition, transformer)
DEFINE_CURRENT_MAPPING(total_precondition, transformer)


transformer (* transformer_fix_point_operator)(transformer);

static void
select_fix_point_operator()
{
    if(get_bool_property(SEMANTICS_FIX_POINT)) {
	string fp_name = get_string_property("SEMANTICS_FIX_POINT_OPERATOR");
	if(strcmp(fp_name, "transfer")==0) {
	    transformer_fix_point_operator = transformer_equality_fix_point;
	}
	else if(strcmp(fp_name, "pattern")==0) {
	    transformer_fix_point_operator = transformer_pattern_fix_point;
	}
	else if(strcmp(fp_name, "derivative")==0) {
	    transformer_fix_point_operator = transformer_derivative_fix_point;
	}
	else {
	    user_error("select_fix_point_operator", "Unknown value %s for property %s\n",
		       fp_name, "SEMANTICS_FIX_POINT_OPERATOR");
	}
    }
    else {
	transformer_fix_point_operator = transformer_basic_fix_point;
    }
    /* This fix-point is not debugged nor used */
    /*
    if(pips_flag_p(SEMANTICS_INEQUALITY_INVARIANT)) {
      tf = transformer_halbwachs_fix_point(tfb);
    }
    */
}

static void add_declaration_information(transformer pre, entity m, bool precondition_p)
{
  //list decls = code_declarations(value_code(entity_initial(m)));
  list decls = current_module_declarations();

  ifdebug(8) {
      pips_debug(8, "Begin for module %s with precondition\n", module_local_name(m));
      print_transformer(pre);
  }

  MAP(ENTITY, v, {
    type t = entity_type(v);

    if(type_variable_p(t)) {
      variable tv = type_variable(t);

      MAP(DIMENSION, d, {
	normalized nl = NORMALIZE_EXPRESSION(dimension_lower(d));
	normalized nu = NORMALIZE_EXPRESSION(dimension_upper(d));

	if(normalized_linear_p(nl) && normalized_linear_p(nu)) {
	  Pvecteur vl = normalized_linear(nl);
	  Pvecteur vu = normalized_linear(nu);

	  if(value_mappings_compatible_vector_p(vl) &&
	     value_mappings_compatible_vector_p(vu)) {
	    Pvecteur cv = vect_substract(vl, vu);

	    if(!precondition_p) {
	      upwards_vect_rename(cv, pre);
	    }

	    if(!vect_constant_p(cv) || vect_coeff(TCST, cv) > 0) {
	      transformer_inequality_add(pre, cv);
	    }
	  }
	}
      }, variable_dimensions(tv));

    }
  }, decls);

  ifdebug(8) {
      pips_debug(8, "End for module %s with precondition\n", module_local_name(m));
      print_transformer(pre);
  }

}

static void transformer_add_declaration_information(transformer pre, entity m)
{
  add_declaration_information(pre, m, FALSE);
}

static void precondition_add_declaration_information(transformer pre, entity m)
{
  add_declaration_information(pre, m, TRUE);
}

/* Functions to make transformers */

bool transformers_intra_fast(char * module_name)
{
    set_bool_property(SEMANTICS_INTERPROCEDURAL, FALSE);
    set_bool_property(SEMANTICS_FLOW_SENSITIVE, TRUE);
    set_bool_property(SEMANTICS_FIX_POINT, FALSE);
    select_fix_point_operator();
    set_bool_property(SEMANTICS_STDOUT, FALSE);
    /* set_int_property(SEMANTICS_DEBUG_LEVEL, 0); */
    return module_name_to_transformers(module_name);
}

bool transformers_intra_full(char * module_name)
{
    set_bool_property(SEMANTICS_INTERPROCEDURAL, FALSE);
    set_bool_property(SEMANTICS_FLOW_SENSITIVE, TRUE);
    set_bool_property(SEMANTICS_FIX_POINT, TRUE);
    select_fix_point_operator();
    set_bool_property(SEMANTICS_STDOUT, FALSE);
    /* set_int_property(SEMANTICS_DEBUG_LEVEL, 0); */
    return module_name_to_transformers(module_name);
}

bool transformers_inter_fast(char * module_name)
{
    set_bool_property(SEMANTICS_INTERPROCEDURAL, TRUE);
    set_bool_property(SEMANTICS_FLOW_SENSITIVE, TRUE);
    set_bool_property(SEMANTICS_FIX_POINT, FALSE);
    select_fix_point_operator();
    set_bool_property(SEMANTICS_STDOUT, FALSE);
    /* set_int_property(SEMANTICS_DEBUG_LEVEL, 0); */
    return module_name_to_transformers(module_name);
}

bool transformers_inter_full(char * module_name)
{
    set_bool_property(SEMANTICS_INTERPROCEDURAL, TRUE);
    set_bool_property(SEMANTICS_FLOW_SENSITIVE, TRUE);
    set_bool_property(SEMANTICS_FIX_POINT, TRUE);
    select_fix_point_operator();
    set_bool_property(SEMANTICS_STDOUT, FALSE);
    /* set_int_property(SEMANTICS_DEBUG_LEVEL, 0); */
    return module_name_to_transformers(module_name);
}

/* Transformer recomputation cannot be of real use unless an
   interprocedural analysis is performed. For intraprocedural analyses,
   using property SEMANTICS_COMPUTE_TRANSFORMERS_IN_CONTEXT is
   sufficient. */
bool refine_transformers_p = FALSE;

bool refine_transformers(char * module_name)
{
  bool res;
  set_bool_property(SEMANTICS_INTERPROCEDURAL, TRUE);
  set_bool_property(SEMANTICS_FLOW_SENSITIVE, TRUE);
  set_bool_property(SEMANTICS_FIX_POINT, TRUE);
  select_fix_point_operator();
  set_bool_property(SEMANTICS_STDOUT, FALSE);
  /* set_int_property(SEMANTICS_DEBUG_LEVEL, 0); */
  refine_transformers_p = TRUE;
  res = module_name_to_transformers_in_context(module_name);
  refine_transformers_p = FALSE;
  return res;
}

bool summary_transformer(char * module_name)
{
  /* There is a choice: do nothing and leave the effective computation
     in module_name_to_transformers, or move it here */
  /* There is another choice: distinguish between inter- and
     intra-procedural analyses at the summary level or in
     module_name_to_transformers(). The choice does not have to be
     consistent with the similar choice made for summary_precondition. */
  pips_debug(1, "considering module %s\n", module_name);
  return TRUE;
}

bool preconditions_intra(char * module_name)
{
    /* nothing to do: transformers are preconditions for this
       intraprocedural option */

    set_bool_property(SEMANTICS_INTERPROCEDURAL, FALSE);
    set_bool_property(SEMANTICS_FLOW_SENSITIVE, TRUE);
    /* Maybe we should have an intra fast and an intra full as with other
       semantics entries */
    /* set_bool_property(SEMANTICS_FIX_POINT, FALSE); */
    set_bool_property(SEMANTICS_FIX_POINT, TRUE);
    set_bool_property(SEMANTICS_INEQUALITY_INVARIANT, FALSE);
    select_fix_point_operator();
    set_bool_property(SEMANTICS_STDOUT, FALSE);
    /* set_int_property(SEMANTICS_DEBUG_LEVEL, 0); */
    return module_name_to_preconditions(module_name);
}

bool preconditions_intra_fast(char * module_name)
{
    /* nothing to do: transformers are preconditions for this
       intraprocedural option */

    set_bool_property(SEMANTICS_INTERPROCEDURAL, FALSE);
    set_bool_property(SEMANTICS_FLOW_SENSITIVE, TRUE);
    /* Maybe we should have an intra fast and an intra full as with other
       semantics entries */
    /* set_bool_property(SEMANTICS_FIX_POINT, FALSE); */
    set_bool_property(SEMANTICS_FIX_POINT, FALSE);
    set_bool_property(SEMANTICS_INEQUALITY_INVARIANT, FALSE);
    select_fix_point_operator();
    set_bool_property(SEMANTICS_STDOUT, FALSE);
    /* set_int_property(SEMANTICS_DEBUG_LEVEL, 0); */
    return module_name_to_preconditions(module_name);
}

bool preconditions_inter_fast(char * module_name)
{
    set_bool_property(SEMANTICS_INTERPROCEDURAL, TRUE);
    set_bool_property(SEMANTICS_FLOW_SENSITIVE, TRUE);
    set_bool_property(SEMANTICS_FIX_POINT, FALSE);
    set_bool_property(SEMANTICS_INEQUALITY_INVARIANT, FALSE);
    select_fix_point_operator();
    set_bool_property(SEMANTICS_STDOUT, FALSE);
    /* set_int_property(SEMANTICS_DEBUG_LEVEL, 0); */
    return module_name_to_preconditions(module_name);
}

bool preconditions_inter_full(char * module_name)
{
    set_bool_property(SEMANTICS_INTERPROCEDURAL, TRUE);
    set_bool_property(SEMANTICS_FLOW_SENSITIVE, TRUE);
    set_bool_property(SEMANTICS_FIX_POINT, TRUE);
    set_bool_property(SEMANTICS_INEQUALITY_INVARIANT, FALSE);
    select_fix_point_operator();
    set_bool_property(SEMANTICS_STDOUT, FALSE);
    /* set_int_property(SEMANTICS_DEBUG_LEVEL, 0); */
    return module_name_to_preconditions(module_name);
}

bool total_preconditions_intra(char * module_name)
{
    set_bool_property(SEMANTICS_INTERPROCEDURAL, FALSE);
    set_bool_property(SEMANTICS_FLOW_SENSITIVE, TRUE);
    set_bool_property(SEMANTICS_FIX_POINT, TRUE);
    set_bool_property(SEMANTICS_INEQUALITY_INVARIANT, FALSE);
    select_fix_point_operator();
    set_bool_property(SEMANTICS_STDOUT, FALSE);
    /* set_int_property(SEMANTICS_DEBUG_LEVEL, 0); */
    return module_name_to_total_preconditions(module_name);
}

bool total_preconditions_inter(char * module_name)
{
    set_bool_property(SEMANTICS_INTERPROCEDURAL, TRUE);
    set_bool_property(SEMANTICS_FLOW_SENSITIVE, TRUE);
    set_bool_property(SEMANTICS_FIX_POINT, TRUE);
    set_bool_property(SEMANTICS_INEQUALITY_INVARIANT, FALSE);
    select_fix_point_operator();
    set_bool_property(SEMANTICS_STDOUT, FALSE);
    /* set_int_property(SEMANTICS_DEBUG_LEVEL, 0); */
    return module_name_to_total_preconditions(module_name);
}


bool old_summary_precondition(char * module_name)
{
  /* do not nothing because it has been computed by side effects;
   * or provide an empty precondition for root modules;
   * maybe a touch to look nicer? 
   */

  transformer t;

  debug_on(SEMANTICS_DEBUG_LEVEL);

  debug(8, "summary_precondition", "begin\n");

  if(db_resource_p(DBR_SUMMARY_PRECONDITION, module_name)) {
    /* touch it */
    t = (transformer) db_get_memory_resource(DBR_SUMMARY_PRECONDITION,
					     module_name,
					     TRUE);
  }
  else {
    t = transformer_identity();
  }

  DB_PUT_MEMORY_RESOURCE(DBR_SUMMARY_PRECONDITION, 
			 module_name, (char * )t);

  ifdebug(8) {
    debug(8, "summary_precondition", 
	  "initial summary precondition %x for %s:\n",
	  t, module_name);
    dump_transformer(t);
    debug(8, "summary_precondition", "end\n");
  }

  debug_off();

  return TRUE;
}

bool intraprocedural_summary_precondition(char * module_name)
{
  /* The current module is sufficient to derive it. */
  set_bool_property(SEMANTICS_INTERPROCEDURAL, FALSE);
  return summary_precondition(module_name);
}

bool interprocedural_summary_precondition(char * module_name)
{
  /* The DATA statement from all modules, called or not called, are used,
     as well as the preconditions at all call sites. */
  set_bool_property(SEMANTICS_INTERPROCEDURAL, TRUE);
  return summary_precondition(module_name);
}

/* Special case: main module
 *
 * Intraprocedural case: use the local DATA statement to define the initial store.
 *
 * Interprocedural case: use the program precondition
 *
 * */

static transformer main_summary_precondition(entity callee)
{
  transformer t = transformer_undefined;

  if (get_bool_property(SEMANTICS_INTERPROCEDURAL)) {
    t = copy_transformer((transformer)
      db_get_memory_resource(DBR_PROGRAM_PRECONDITION, "", FALSE));
    if(transformer_empty_p(t)) {
      pips_user_warning(
			"Initial preconditions are not consistent.\n"
			" The Fortran standard rules about variable initialization"
			" with DATA statements are likely to be violated.\n"
			"set property PARSER_ACCEPT_ANSI_EXTENSIONS to false\n"
			"and CHECK_FORTRAN_SYNTAX_BEFORE_PIPS to true.\n");
    }
  }
  else {
    /* Do we need to initialize the mappings before calling this subroutine? */
    /* Why not use a DB_GET of DBR_INITIAL_PRECONDITION? */
    /* t = data_to_precondition(callee); */
    t = copy_transformer
      ((transformer) db_get_memory_resource(DBR_INITIAL_PRECONDITION,
					    module_local_name(callee),
					    FALSE));
  }

  return t;
}

/* Standard cases: called modules
 *
 * If a main is present, modules which are never called receive an
 * unfeasible summary_precondition.
 *
 * If no main is present in the current workspace, modules which are never
 * called receive an identity summary precondition, i.e. no information.
 *
 * If an interprocedural analysis is required, the preconditions of all
 * call sites are translated and the unioned.
 *
 * */

static transformer ordinary_summary_precondition(string module_name,
						 entity callee)
{
  transformer t = transformer_undefined;

  if(get_bool_property(SEMANTICS_INTERPROCEDURAL)) {
    /* Look for all call sites in the callers
     */
    callees callers = (callees) db_get_memory_resource(DBR_CALLERS,
						       module_name,
						       TRUE);
    list lc = callees_callees(callers);

    ifdebug(1) {
      debug(1, "summary_precondition", "begin for %s with %d callers\n",
	    module_name,
	    gen_length(lc));
      MAP(STRING, caller_name, {
	(void) fprintf(stderr, "%s, ", caller_name);
      }, lc);
      (void) fprintf(stderr, "\n");
    }

    reset_call_site_number();

    MAP(STRING, caller_name, 
    {
      entity caller = module_name_to_entity(caller_name);
      t = update_precondition_with_call_site_preconditions(t, caller, callee);
    }, callees_callees(callers));

    if (ENDP(callees_callees(callers)) && 
	some_main_entity_p()) {
      /* no callers => empty precondition if a main is being analyzed
	 FC. 08/01/1999.
      */
      pips_user_warning("empty precondition to %s "
			"because not in call tree from main.\n", module_name);
      t = transformer_empty();
    } 
    else if (transformer_undefined_p(t)) {
      /* No main in the application? Do declare every module executed. */
      t = transformer_identity();
    }
    else {
      /* Try to eliminate (some) redundancy at a reasonnable cost. */
      /* What is a reasonnable cost? */

      /* t = transformer_normalize(t, 2); */
      t = transformer_normalize(t, 7);

      /* Corinne's best one... for YPENT2 in ARC2D, but be ready to pay
	 the price! And in case an overflow occurs, you may loose a lot of
	 accuracy without any control. */
      /* t = transformer_normalize(t, 8); */

      /* No consistency check possible here because value_mappings are
	 not available */
      /* pips_assert("The summary precondition is consistent",
	 transformer_consistency_p(t));*/
    }
  }
  else {
    /* Intraprocedural case */
    t = transformer_identity();
  }

  return t;
}

bool summary_precondition(char * module_name)
{
  /* transformer t = transformer_identity(); */
  transformer t = transformer_undefined;
  entity callee = module_name_to_entity(module_name);

  debug_on(SEMANTICS_DEBUG_LEVEL);

  set_current_module_entity(callee);

  if(entity_main_module_p(callee)) {
    t = main_summary_precondition(callee);
  }
  else {
    t = ordinary_summary_precondition(module_name, callee);
  }

  /* Add declaration information: arrays cannot be empty (Fortran
   * standard, Section 5.1.2)
   *
   * It does not seem to be a good idea for the semantics of
   * SUMMARY_PRECONDITION. It seems better to have this information in the
   * summary transformer as an input validity condition.
   *
   */
  if(FALSE && get_bool_property("SEMANTICS_TRUST_ARRAY_DECLARATIONS")) {
    set_current_module_statement(
				 (statement) db_get_memory_resource(DBR_CODE, module_name, TRUE)); 
    set_cumulated_rw_effects((statement_effects) 
			     db_get_memory_resource(DBR_CUMULATED_EFFECTS, module_name, TRUE));
    module_to_value_mappings( get_current_module_entity() );
    transformer_add_declaration_information(t,
					    get_current_module_entity());
    reset_cumulated_rw_effects();
    reset_current_module_statement();
    free_value_mappings();
  }
    
  pips_assert("t is defined", !transformer_undefined_p(t));

  /* Try to put the summary precondition in a (partially) canonical form. */
  t = transformer_normalize(t, 4);
  t = transformer_normalize(t, 4);

  ifdebug(3) {
    pips_debug(1, "considering summary precondition for %s\n", module_name);
    dump_transformer(t);
  }

  DB_PUT_MEMORY_RESOURCE(DBR_SUMMARY_PRECONDITION, 
			 module_name, (char * )t);

  ifdebug(1) {
    pips_debug(1,
	       "initial summary precondition %p for %s (%d call sites):\n",
	       t, module_name, get_call_site_number());
    dump_transformer(t);
    pips_debug(1, "end for module %s\n", module_name);
  }

  reset_current_module_entity();
  debug_off();

  return TRUE;
}

bool summary_total_postcondition(char * module_name)
{
  /* Look for all call sites in the callers
   */
  callees callers = (callees) db_get_memory_resource(DBR_CALLERS,
						     module_name,
						     TRUE);
  entity callee = module_name_to_entity(module_name);
  /* transformer t = transformer_identity(); */
  transformer t = transformer_undefined;

  debug_on(SEMANTICS_DEBUG_LEVEL);

  pips_assert("Not implemented yet", FALSE);

  set_current_module_entity(callee);

  ifdebug(1) {
    debug(1, "summary_precondition", "begin for %s with %d callers\n",
	  module_name,
	  gen_length(callees_callees(callers)));
    MAP(STRING, caller_name, {
      (void) fprintf(stderr, "%s, ", caller_name);
    }, callees_callees(callers));
    (void) fprintf(stderr, "\n");
  }

  reset_call_site_number();

  MAP(STRING, caller_name, 
  {
    entity caller = module_name_to_entity(caller_name);
    t = update_precondition_with_call_site_preconditions(t, caller, callee);
  }, callees_callees(callers));

  if (!callees_callees(callers) && 
      !entity_main_module_p(callee) && 
      some_main_entity_p())
    {
      /* no callers => empty precondition (but the main). 
	 FC. 08/01/1999.
      */
      pips_user_warning("empty precondition to %s "
			"because not in call tree from main.\n", module_name);
      t = transformer_empty();
    } else if (transformer_undefined_p(t)) {
      t = transformer_identity();
    } else {
      /* try to eliminate (some) redundancy at a reasonnable cost */
      /* t = transformer_normalize(t, 2); */

      /* what cost? */
      t = transformer_normalize(t, 7);
    }

  /* Add declaration information: arrays cannot be empty (Fortran
   * standard, Section 5.1.2)
   *
   * It does not seem to be a good idea for the semantics of
   * SUMMARY_PRECONDITION. It seems better to have this information in the
   * summary transformer as an input validity condition.
   *
   */
  if(FALSE && get_bool_property("SEMANTICS_TRUST_ARRAY_DECLARATIONS")) {
    set_current_module_statement(
				 (statement) db_get_memory_resource(DBR_CODE, module_name, TRUE)); 
    set_cumulated_rw_effects((statement_effects) 
			     db_get_memory_resource(DBR_CUMULATED_EFFECTS, module_name, TRUE));
    module_to_value_mappings( get_current_module_entity() );
    transformer_add_declaration_information(t,
					    get_current_module_entity());
    reset_cumulated_rw_effects();
    reset_current_module_statement();
    free_value_mappings();
  }

  DB_PUT_MEMORY_RESOURCE(DBR_SUMMARY_TOTAL_POSTCONDITION, 
			 module_name, (char * )t);

  ifdebug(1) {
    pips_debug(1,
	       "summary total postcondition %p for %s (%d call sites):\n",
	       t, module_name, get_call_site_number());
    dump_transformer(t);
    pips_debug(1, "end for module %s\n", module_name);
  }

  reset_current_module_entity();
  debug_off();

  return TRUE;
}

bool summary_total_precondition(char * module_name)
{
    /* there is a choice: do nothing and leave the effective computation
       in module_name_to_total_preconditions or move it here */
  pips_debug(1, "considering module %s\n", module_name);
  return TRUE;
}



/* bool generic_module_name_to_transformers(char * module_name, bool in_context):
 * compute a transformer for each statement of a module with a given
 * name; compute also the global transformer for the module
 */
bool generic_module_name_to_transformers(char *module_name, bool in_context)
{
    transformer t_intra = transformer_undefined;
    transformer t_inter = transformer_undefined;
    /* intraprocedural preconditions: proper declarations */
    transformer mod_pre = transformer_undefined;
    list e_inter;

    debug_on(SEMANTICS_DEBUG_LEVEL);
    sc_variable_name_push((char* (*)(Variable)) readable_value_name);

    set_current_module_entity(module_name_to_entity(module_name));
    /* could be a gen_find_tabulated as well... */
    set_current_module_statement(
	(statement) db_get_memory_resource(DBR_CODE, module_name, TRUE)); 
    if( get_current_module_statement() == (statement) database_undefined )
	pips_error("module_name_to_transformers",
		   "no statement for module %s\n", module_name);

    /* Is it really useful? I could not find where it is used. bc. */
    set_proper_rw_effects((statement_effects) 
	db_get_memory_resource(DBR_PROPER_EFFECTS, module_name, TRUE));

    set_cumulated_rw_effects((statement_effects) 
	db_get_memory_resource(DBR_CUMULATED_EFFECTS, module_name, TRUE));

    /* cumulated_effects_map_print();*/

    e_inter = effects_to_list( (effects)
	db_get_memory_resource(DBR_SUMMARY_EFFECTS, module_name, TRUE));

    set_transformer_map( MAKE_STATEMENT_MAPPING() ); 

    /* compute the basis related to module m */
    module_to_value_mappings( get_current_module_entity() );

    /* In the main module, transformers can be computed in context of the
       initial values */
    if(entity_main_module_p(get_current_module_entity())
       && get_bool_property("SEMANTICS_COMPUTE_TRANSFORMERS_IN_CONTEXT")) 
    {
      if (get_bool_property(SEMANTICS_INTERPROCEDURAL)) 
      {
	mod_pre = (transformer)
	  db_get_memory_resource(DBR_PROGRAM_PRECONDITION, "", FALSE);
	if(transformer_empty_p(mod_pre)) {
	  pips_user_warning(
	     "Initial preconditions are not consistent.\n"
	     " The Fortran standard rules about variable initialization"
	     " with DATA statements are likely to be violated.\n"
	     "set property PARSER_ACCEPT_ANSI_EXTENSIONS to false\n"
	     "and CHECK_FORTRAN_SYNTAX_BEFORE_PIPS to true.\n");
	}
      }
      else
	mod_pre = data_to_precondition(get_current_module_entity());
    }
    else if(in_context) {
      mod_pre = 
	transformer_dup(load_summary_precondition(get_current_module_entity()));
    }
    else
      mod_pre = transformer_identity();

    /* Add declaration information: arrays cannot be empty (Fortran
       standard, Section 5.1.2) */
    if(get_bool_property("SEMANTICS_TRUST_ARRAY_DECLARATIONS")) {
        transformer_add_declaration_information(mod_pre,
						get_current_module_entity());
    }

    /* Get the preconditions: they might prove useful within loops where
       transformers cannot propagate enough information. */
    if(in_context) {
     set_precondition_map( (statement_mapping) 
	db_get_memory_resource(DBR_PRECONDITIONS, module_name, TRUE));
   }

    /* compute intraprocedural transformer */
    t_intra = statement_to_transformer(get_current_module_statement(), mod_pre);
    free_transformer(mod_pre);

    DB_PUT_MEMORY_RESOURCE(DBR_TRANSFORMERS, module_name, 
			   (char*) get_transformer_map() );  

    /* FI: side effect; compute and store the summary transformer, because
       every needed piece of data is available... */

    /* filter out local variables from the global intraprocedural effect */
    t_inter = transformer_intra_to_inter(t_intra, e_inter);
    t_inter = transformer_normalize(t_inter, 2);
    if(!transformer_consistency_p(t_inter)) {
	(void) print_transformer(t_inter);
	pips_error("module_name_to_transformers",
		   "Non-consistent summary transformer\n");
    }
    DB_PUT_MEMORY_RESOURCE(DBR_SUMMARY_TRANSFORMER, 
			   module_local_name(get_current_module_entity()), 
			   (char*) t_inter);
    debug(8,"module_name_to_transformers","t_inter=%x\n", t_inter);

    reset_current_module_entity();
    reset_current_module_statement();
    /* Two auxiliary hash tables allocated by effectsmap_to_listmap() */
    reset_proper_rw_effects();
    reset_cumulated_rw_effects();
    reset_transformer_map();
    if(in_context) reset_precondition_map();

    free_value_mappings();

    sc_variable_name_pop();
    debug_off();

    return TRUE;
}

bool module_name_to_transformers_in_context(char *module_name)
{
  bool rc = FALSE;
  bool save_prop = get_bool_property("SEMANTICS_COMPUTE_TRANSFORMERS_IN_CONTEXT");

  if(!save_prop) {
    pips_user_warning("Although property SEMANTICS_COMPUTE_TRANSFORMERS_IN_CONTEXT"
		      " is not set, it is used because it is necessary for this "
		      "recomputation to be useful\n");
    set_bool_property("SEMANTICS_COMPUTE_TRANSFORMERS_IN_CONTEXT", TRUE);
  }

  rc = generic_module_name_to_transformers(module_name, TRUE);

  set_bool_property("SEMANTICS_COMPUTE_TRANSFORMERS_IN_CONTEXT", save_prop);
  return rc;
}

bool module_name_to_transformers(char *module_name)
{
  bool rc = FALSE;
  rc = generic_module_name_to_transformers(module_name, FALSE);
  return rc;
}

/* resource module_name_to_preconditions(char * module_name):
 * compute a transformer for each statement of a module with a given
 * name; compute also the global transformer for the module
 */
bool module_name_to_preconditions(char *module_name)
{
    transformer t_inter;
    transformer pre;
    transformer post;

    /* set_debug_level(get_int_property(SEMANTICS_DEBUG_LEVEL)); */
    debug_on(SEMANTICS_DEBUG_LEVEL);
    sc_variable_name_push((char* (*)(Variable)) readable_value_name);

    set_current_module_entity(module_name_to_entity(module_name) );
    /* could be a gen_find_tabulated as well... */
    set_current_module_statement( 
	(statement) db_get_memory_resource(DBR_CODE, module_name, TRUE)); 
    if(get_current_module_statement() == (statement) database_undefined)
	pips_error("module_name_to_preconditions",
		   "no statement for module %s\n", module_name);

    /* Used to add reference information when it is trusted... which
       should always be, at least for automatic parallelization. */
    set_proper_rw_effects((statement_effects) 
	db_get_memory_resource(DBR_PROPER_EFFECTS, module_name, TRUE));

    /* cumulated effects are used to compute the value mappings */
    set_cumulated_rw_effects((statement_effects) 
	db_get_memory_resource(DBR_CUMULATED_EFFECTS, module_name, TRUE));

    set_transformer_map( (statement_mapping) 
	db_get_memory_resource(DBR_TRANSFORMERS, module_name, TRUE));


    /* p_inter is not used!!! FI, 9 February 1994 */
    /*
    if(get_bool_property(SEMANTICS_INTERPROCEDURAL)) {
	p_inter = (transformer) 
	    db_get_memory_resource(DBR_SUMMARY_PRECONDITION,
				   module_name, TRUE);
    }
    */

    t_inter = (transformer) 
	db_get_memory_resource(DBR_SUMMARY_TRANSFORMER, module_name, TRUE);

    /* debug_on(SEMANTICS_DEBUG_LEVEL); */

    set_precondition_map( MAKE_STATEMENT_MAPPING() );  

    /* compute the mappings related to module m, that is likely to be
       unavailable during interprocedural analysis; a module reference
       should be kept with the mappings to avoid useless recomputation,
       allocation and frees, including those due to the prettyprinter */
    
    module_to_value_mappings( get_current_module_entity() );

    /* set the list of global values */
    set_module_global_arguments(transformer_arguments(t_inter));

    /* debug_on(SEMANTICS_DEBUG_LEVEL); */

    pre = copy_transformer(load_summary_precondition(get_current_module_entity()));

    /* Add declaration information: arrays cannot be empty (Fortran
       standard, Section 5.1.2). But according to summary_precondition(),
       this is now supposed to be performed by the transformer phase? */
    if(get_bool_property("SEMANTICS_TRUST_ARRAY_DECLARATIONS")) {
        precondition_add_declaration_information(pre, get_current_module_entity());
    }

    /* debug_on(SEMANTICS_DEBUG_LEVEL); */

    /* propagate the module precondition */
    init_reachable(get_current_module_statement());
    post = statement_to_postcondition(pre, get_current_module_statement() );
    close_reachable();

    /* post could be stored in the ri for later interprocedural uses
       but the ri cannot be modified so early before the DRET demo;
       also our current interprocedural algorithm does not propagate
       postconditions upwards in the call tree */

    DB_PUT_MEMORY_RESOURCE(DBR_PRECONDITIONS, 
			   module_name, 
			   (char*) get_precondition_map() );

    pips_debug(8, "postcondition computed for %s\n", 
	  entity_local_name( get_current_module_entity() ));
    ifdebug(8) (void) print_transformer(post);
    debug(1, "module_name_to_preconditions", "end\n");

    reset_current_module_entity();
    reset_current_module_statement();
    reset_transformer_map();
    reset_precondition_map();
    reset_proper_rw_effects();
    reset_cumulated_rw_effects();

    free_value_mappings();

    sc_variable_name_pop();
    debug_off();

    return TRUE;
}

bool module_name_to_total_preconditions(char *module_name)
{
  transformer t_inter = transformer_undefined;
  transformer t_pre = transformer_undefined;
  transformer t_pre_inter = transformer_undefined;
  transformer t_post = transformer_undefined;
  list e_inter = list_undefined;

  debug_on(SEMANTICS_DEBUG_LEVEL);
  sc_variable_name_push((char* (*)(Variable)) readable_value_name);

  set_current_module_entity(module_name_to_entity(module_name) );
  set_current_module_statement( 
			       (statement) db_get_memory_resource(DBR_CODE, module_name, TRUE)); 
  if(get_current_module_statement() == (statement) database_undefined)
    pips_error("module_name_to_preconditions",
	       "no statement for module %s\n", module_name);

  set_proper_rw_effects((statement_effects) 
			db_get_memory_resource(DBR_PROPER_EFFECTS, module_name, TRUE));

  set_cumulated_rw_effects((statement_effects) 
			   db_get_memory_resource(DBR_CUMULATED_EFFECTS, module_name, TRUE));

  e_inter = effects_to_list((effects)
			    db_get_memory_resource(DBR_SUMMARY_EFFECTS, module_name, TRUE));

  set_transformer_map( (statement_mapping) 
		       db_get_memory_resource(DBR_TRANSFORMERS, module_name, TRUE));

  set_precondition_map( (statement_mapping) 
		       db_get_memory_resource(DBR_TRANSFORMERS, module_name, TRUE));


  t_inter = (transformer) 
    db_get_memory_resource(DBR_SUMMARY_TRANSFORMER, module_name, TRUE);

  set_total_precondition_map( MAKE_STATEMENT_MAPPING() );  

  module_to_value_mappings( get_current_module_entity() );

  /* set the list of global values */
  set_module_global_arguments(transformer_arguments(t_inter));

  if(entity_main_module_p(get_current_module_entity())) {
    /* The program postcondition should be used DBR_PROGRAM_POSTCONDITION */
    t_post = transformer_identity();
  }
    
  ifdebug(3) {
    pips_debug(1, "considering final total postcondition for %s\n", module_name);
    print_transformer(t_post);
  }

  if(get_bool_property(SEMANTICS_INTERPROCEDURAL)) {
    transformer ip = 
      load_summary_total_postcondition(get_current_module_entity());
    if( ip == transformer_undefined) {
      /* that might be because we are at the call tree root
	 or because no information is available;
	 maybe, every module precondition should be initialized
	 to a neutral value? */
      pips_user_warning("no interprocedural module total postcondition for %s\n",
			entity_local_name(get_current_module_entity() ));
      ;
    }
    else {
      translate_global_values(get_current_module_entity(), ip);
      ifdebug(8) {
	pips_debug(8, "\t summary_total_postcondition %p after translation:\n",
		   ip);
	print_transformer(ip);
	pips_assert("The summary total postcondition is consistent",
		    transformer_consistency_p(ip));
      }
      t_post = transformer_combine(transformer_dup(ip), t_post);
    }
  }
  else if(transformer_undefined_p(t_post)) {
    /* intra-procedural case, not a main module */
    t_post = transformer_identity();
  }

  /* propagate the module total postcondition */
  init_reachable(get_current_module_statement());
  t_pre = statement_to_total_precondition(t_post, get_current_module_statement() );
  close_reachable();

  DB_PUT_MEMORY_RESOURCE(DBR_TOTAL_PRECONDITIONS, 
			 module_name, 
			 (char*) get_total_precondition_map() );

  /* filter out local variables from the global intraprocedural effect */
  t_pre_inter = transformer_intra_to_inter(t_pre, e_inter);
  t_pre_inter = transformer_normalize(t_pre_inter, 2);
  if(!transformer_consistency_p(t_pre_inter)) {
    (void) print_transformer(t_pre_inter);
    pips_internal_error("Non-consistent summary transformer\n");
  }
  DB_PUT_MEMORY_RESOURCE(DBR_SUMMARY_TOTAL_PRECONDITION, 
			 module_local_name(get_current_module_entity()), 
			 (char*) t_pre_inter);

  pips_debug(8, "total precondition computed for %s\n", 
	     entity_local_name( get_current_module_entity() ));
  ifdebug(8) (void) print_transformer(t_pre);
  pips_debug(1, "end\n");

  reset_current_module_entity();
  reset_current_module_statement();
  reset_transformer_map();
  reset_precondition_map();
  reset_total_precondition_map();
  reset_proper_rw_effects();
  reset_cumulated_rw_effects();

  free_value_mappings();

  sc_variable_name_pop();
  debug_off();

  return TRUE;
}


transformer load_summary_transformer(entity e)
{
    /* FI: I had to add a guard db_resource_p() on Nov. 14, 1995.
     * I do not understand why the problem never occured before,
     * although it should each time the intra-procedural option
     * is selected.
     *
     * This may partially be explained because summary transformers
     * are implicitly computed with transformers instead of using
     * an explicit call to summary_transformer (I guess I'm going
     * to change that).
     *
     * I think it would be better not to call load_summary_transformer()
     * at all when no interprocedural options are selected. I should
     * change that too.
     */

    /* memoization could be used to improve efficiency */
    transformer t = transformer_undefined;

    pips_assert("load_summary_transformer", entity_module_p(e));

    if(db_resource_p(DBR_SUMMARY_TRANSFORMER, module_local_name(e))) {
	t = (transformer) 
	    db_get_memory_resource(DBR_SUMMARY_TRANSFORMER, 
				   entity_local_name(e), 
				   TRUE);

	/* db_get_memory_resource never returns database_undefined or
	   resource_undefined */
	pips_assert("load_summary_transformer", t != transformer_undefined);
    }
    else {
	t = transformer_undefined;
    }

    return t;
}


/* void update_summary_precondition(e, t): t is supposed to be a precondition
 * related to one of e's call sites and translated into e's basis; 
 *
 * the current global precondition for e is replaced by its
 * convex hull with t; 
 *
 * t may be slightly modified by transformer_convex_hull
 * because of bad design (FI)
 */
void update_summary_precondition(entity e, transformer t)
{
    transformer t_old = transformer_undefined;
    transformer t_new = transformer_undefined;

    pips_assert("update_summary_precondition", entity_module_p(e));

    debug(8, "update_summary_precondition", "begin\n");

    t_old = (transformer) 
	db_get_memory_resource(DBR_SUMMARY_PRECONDITION, module_local_name(e),
			       TRUE);

    ifdebug(8) {
	debug(8, "update_summary_precondition", " old precondition for %s:\n",
	      entity_local_name(e));
	print_transformer(t_old);
    }

    if(t_old == transformer_undefined)
	t_new = transformer_dup(t);
    else {
	t_new = transformer_convex_hull(t_old, t);
	transformer_free(t_old);
    }

    DB_PUT_MEMORY_RESOURCE(DBR_SUMMARY_PRECONDITION, 
			   module_local_name(e), 
			   (char*) t_new );

    ifdebug(8) {
	debug(8, "update_summary_precondition", "new precondition for %s:\n",
	      entity_local_name(e));
	print_transformer(t_new);
	debug(8, "update_summary_precondition", "end\n");
    }
}

/* summary_preconditions are expressed in any possible frame, in fact the frame of
 * the last procedure that used/produced it
 */
transformer load_summary_precondition(entity e)
{
  /* memoization could be used to improve efficiency */
  transformer t;

  pips_assert("e is a module", entity_module_p(e));

  pips_debug(8, "begin\n for %s\n", 
	     module_local_name(e));

  t = (transformer) 
    db_get_memory_resource(DBR_SUMMARY_PRECONDITION, module_local_name(e), 
			   TRUE);
  /* Not done earlier, because the value mappings were not available. On
     the other hand, htis assumes that the value mappings have been
     initialized before a call to load_summary_precondition(0 is
     performed.*/
  translate_global_values(e, t);

  pips_assert("the summary precondition t is defined", t != transformer_undefined);
    
  ifdebug(8) {
    pips_debug(8, " precondition for %s:\n",
	       module_local_name(e));
    dump_transformer(t);
    pips_debug(8, " end\n");
  }

  return t;
}

/* summary_preconditions are expressed in any possible frame, in fact the frame of
 * the last procedure that used/produced it
 */
transformer load_summary_total_postcondition(entity e)
{
    transformer t_post = transformer_undefined;;

    pips_assert("e is a module", entity_module_p(e));

    pips_debug(8, "begin\n for %s\n", 
	  module_local_name(e));

    t_post = (transformer) 
	db_get_memory_resource(DBR_SUMMARY_TOTAL_POSTCONDITION, module_local_name(e), 
			       TRUE);

    pips_assert("t is defined", t_post != transformer_undefined);
    
    ifdebug(8) {
	pips_debug(8, " total postcondition for %s:\n",
	      module_local_name(e));
	dump_transformer(t_post);
	pips_debug(8, " end\n");
    }

    return t_post;
}


list load_summary_effects(entity e)
{
    /* memorization could be used to improve efficiency */
    list t;

    pips_assert("load_summary_effects", entity_module_p(e));

    t = effects_to_list( (effects) 
	db_get_memory_resource(DBR_SUMMARY_EFFECTS, module_local_name(e), 
			       TRUE));

    pips_assert("load_summary_effects", t != list_undefined);

    return t;
}


list load_module_intraprocedural_effects(entity e)
{
    /* memoization could be used to improve efficiency */
    list t;
    statement s;

    pips_assert("is a module", entity_module_p(e));
    pips_assert("is the current module", e == get_current_module_entity());
    s = get_current_module_statement();
    pips_assert("some statement", s!=statement_undefined);

    t = load_cumulated_rw_effects_list(s);

    pips_assert("some list", t != list_undefined);

    return t;
}


/* ca n'a rien a` faire ici, et en plus, il serait plus inte'ressant d'avoir 
 * une fonction void statement_map_print(statement_mapping htp)
 */
void cumulated_effects_map_print(void)
{
    FILE * f =stderr;
    statement_effects htp = get_cumulated_rw_effects();

    /* hash_table_print_header (htp,f); */

    STATEMENT_EFFECTS_MAP(k, v, {
	fprintf(f, "\n\n%td", statement_ordering((statement) k));
	print_effects( effects_effects(v));
    },
	     htp);
}
