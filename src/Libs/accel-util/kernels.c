/*
  Copyright 1989-2010 MINES ParisTech

  This file is part of PIPS.

  PIPS is free software: you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  any later version.

  PIPS is distributed in the hope that it will be useful, but WITHOUT ANY
  WARRANTY; without even the implied warranty of MERCHANTABILITY or
  FITNESS FOR A PARTICULAR PURPOSE.

  See the GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with PIPS.  If not, see <http://www.gnu.org/licenses/>.

*/

/**
 * @file kernels.c
 * kernels manipulation
 * @author Serge Guelton <serge.guelton@enst-bretagne.fr>
 * @date 2010-01-03
 */
#ifdef HAVE_CONFIG_H
    #include "pips_config.h"
#endif
#include <ctype.h>


#include "genC.h"
#include "linear.h"
#include "ri.h"
#include "effects.h"
#include "ri-util.h"
#include "effects-util.h"
#include "text.h"
#include "pipsdbm.h"
#include "resources.h"
#include "properties.h"
#include "misc.h"
#include "control.h"
#include "callgraph.h"
#include "effects-generic.h"
#include "effects-simple.h"
#include "effects-convex.h"
#include "text-util.h"
#include "transformations.h"
#include "transformer.h"
#include "semantics.h"
#include "parser_private.h"
#include "accel-util.h"

struct dma_pair {
  entity new_ent;
  enum region_to_dma_switch s;
};

typedef enum {
    dma1D=1,
    dma2D,
    dma3D
} dma_dimension;

static dma_dimension get_dma_dimension(entity to) {
    size_t n = type_dereferencement_depth(entity_type(to)) - 1; /* -1 because we always have pointer to area ... in our case*/
    switch(n) {
        case 1:return dma1D;
        case 2:return dma2D;
        case 3:return dma3D;
        default: pips_internal_error("no dma for dimension %zd\n",n);
                 return dma1D;
    };
}

/** 
 * converts a region_to_dma_switch to coressponding dma name
 * according top properties
 */
static string get_dma_name(enum region_to_dma_switch m,dma_dimension d) {
    char *seeds[] = {
        "KERNEL_LOAD_STORE_LOAD_FUNCTION",
        "KERNEL_LOAD_STORE_STORE_FUNCTION",
        "KERNEL_LOAD_STORE_ALLOCATE_FUNCTION",
        "KERNEL_LOAD_STORE_DEALLOCATE_FUNCTION"
    };
    char * propname = seeds[(int)m];
    if(d!=dma1D && (int)m <2)
        asprintf(&propname,"%s_%dD",seeds[(int)m],(int)d);
    string dmaname = get_string_property(propname);
    if(d!=dma1D && (int)m <2) free(propname);
    return dmaname;
}

static list simple_effect_to_dimensions(effect eff)
{
    entity re = reference_variable(effect_any_reference(eff));
    type te= ultimate_type(entity_type(re));
    return gen_full_copy_list(variable_dimensions(type_variable(te)));
}
static bool region_to_dimensions(region reg, transformer tr, list *dimensions, list * offsets) {
    if(region_to_minimal_dimensions(reg,tr,dimensions,offsets,true)) {
        return true;
    }
    else
    {
        pips_user_warning("failed to convert regions to minimal array dimensions, using whole array instead\n");
        return false;
    }
}

static void effect_to_dimensions(effect eff, transformer tr, list *dimensions, list * offsets) {
    if(effect_region_p(eff) && region_to_dimensions(eff,tr,dimensions,offsets)) {
        /* ok */
    }
    else {
        *dimensions= simple_effect_to_dimensions(eff);
        FOREACH(DIMENSION,d,*dimensions)
            *offsets = CONS(EXPRESSION,int_to_expression(0),*offsets);
    }
}

/** 
 * converts dimensions to a dma call from a memory @a from to another memory @a to
 * 
 * @param from expression giving the adress of the input memory
 * @param to expression giving the adress of the output memory
 * @param ld list of dimensions to analyze
 * @param m kind of call to generate
 * 
 * @return 
 */
static
call dimensions_to_dma(entity from,
		  entity to,
		  list/*of dimensions*/ ld,
		  list/*of offsets*/    lo,
		  enum region_to_dma_switch m)
{
  expression dest;
  list args = NIL;
  bool scalar_entity=false;
  dma_dimension function_dimension = get_dma_dimension(to);
  string function_name = get_dma_name(m,function_dimension);

  entity mcpy = module_name_to_entity(function_name);
  if (entity_undefined_p(mcpy)) {
      mcpy=make_empty_subroutine(function_name,copy_language(module_language(get_current_module_entity())));
    pips_user_warning("Cannot find \"%s\" method. Are you sure you have set\n"
		    "KERNEL_LOAD_STORE_..._FUNCTION "
		    "to a defined entity and added the correct .c file?\n",function_name);
  }


  /*scalar detection*/
  scalar_entity=entity_scalar_p(from);

  if (dma_allocate_p(m))
    /* Need the address for the allocator to modify the pointer itself: */
    dest = MakeUnaryCall(entity_intrinsic(ADDRESS_OF_OPERATOR_NAME), entity_to_expression(to));
 
  else if (!dma_allocate_p(m) && !scalar_entity)
    /* Except for the deallocation, the original array is referenced
       throudh pointer dereferencing: */
    dest = MakeUnaryCall(entity_intrinsic(DEREFERENCING_OPERATOR_NAME), entity_to_expression(to));
  else if(!dma_allocate_p(m) && scalar_entity){
    dest=entity_to_expression(to);
  }

  reference rtmp = make_reference(from,lo);
  type element_type = make_type_variable(
          make_variable(
              copy_basic(basic_of_reference(rtmp)),
              NIL,NIL)
          );
  reference_indices(rtmp)=NIL;
  free_reference(rtmp);


  switch(m) {
      case dma_deallocate:
          args = make_expression_list(dest);
          break;
      case dma_allocate:
          {
              expression transfer_size = SizeOfDimensions(ld);
              transfer_size=MakeBinaryCall(
                      entity_intrinsic(MULTIPLY_OPERATOR_NAME),
                      make_expression(
                          make_syntax_sizeofexpression(make_sizeofexpression_type(element_type)),
                          normalized_undefined
                          ),
                      transfer_size);

              args = make_expression_list(dest, transfer_size);
          } break;
      case dma_load:
      case dma_store:
          {
              list /*of expressions*/ transfer_sizes = NIL;
              FOREACH(DIMENSION,d,ld) {
                  expression transfer_size=
                      MakeBinaryCall(
                          entity_intrinsic(MULTIPLY_OPERATOR_NAME),
                          make_expression(
                              make_syntax_sizeofexpression(make_sizeofexpression_type(element_type)),
                              normalized_undefined
                              ),
                          SizeOfDimension(d));
                  transfer_sizes=CONS(EXPRESSION,transfer_size,transfer_sizes);
              }
              expression source;
              if(scalar_entity)
                  source = MakeUnaryCall(entity_intrinsic(ADDRESS_OF_OPERATOR_NAME), entity_to_expression(from));
              else
                  source = MakeUnaryCall(entity_intrinsic(ADDRESS_OF_OPERATOR_NAME), 
                          reference_to_expression(
                              make_reference(from,lo)
                              )
                          );
              args = CONS(EXPRESSION,source,CONS(EXPRESSION,dest,gen_nreverse(transfer_sizes)));
          } break;
      default:
          pips_internal_error("should not happen");
  }
  return make_call(mcpy, args);
}




/* Compute a call to a DMA function from the effects of a statement

   @return a statement of the DMA transfers or statement_undefined if
   nothing needed or if the dma function has been set to "" in the relevant property
 */
static
statement effects_to_dma(statement stat,
			 enum region_to_dma_switch s,
			 hash_table e2e) {
    /* if no dma is provided, skip the computation
     * it is used for scalope at least */
  if(empty_string_p(get_dma_name(s,dma1D)))
    return statement_undefined;

  list rw_effects= load_cumulated_rw_effects_list(stat);
  transformer tr = transformer_range(load_statement_precondition(stat));

  list effects = NIL;

  /* filter out relevant effects depending on operation mode */
  FOREACH(EFFECT,e,rw_effects) {


    if ((dma_load_p(s) || dma_allocate_p(s) || dma_deallocate_p(s))
	&& action_read_p(effect_action(e)))
      effects=CONS(EFFECT,e,effects);
    else if ((dma_store_p(s)  || dma_allocate_p(s) || dma_deallocate_p(s))
	     && action_write_p(effect_action(e)))
      effects=CONS(EFFECT,e,effects);

    /*Little hack to avoid effects weaknesses*/
    if(get_bool_property("KERNEL_LOAD_STORE_FORCE_LOAD")
       &&(dma_load_p(s) && action_write_p(effect_action(e)))){
	effect hack = copy_effect(e);
	effect_action(hack) = make_action_read_memory();
	effects=CONS(EFFECT,hack,effects);
      }
  }

  /* builds out transfer from gathered effects */
  list statements = NIL;
  FOREACH(EFFECT,eff,effects) {
    statement the_dma = statement_undefined;
    reference r = effect_any_reference(eff);
    entity re = reference_variable(r);
    struct dma_pair * val = (struct dma_pair *) hash_get(e2e, re);

    if( val == HASH_UNDEFINED_VALUE || (val->s != s) ) {
      if(!entity_scalar_p(re) || get_bool_property("KERNEL_LOAD_STORE_SCALAR")) {
    list /*of dimensions*/ the_dims = NIL,
         /*of expressions*/the_offsets = NIL;
	effect_to_dimensions(eff,tr,&the_dims,&the_offsets);

	entity eto;
	if(val == HASH_UNDEFINED_VALUE) {

	  /* initialized with NULL value */
	  expression init = int_to_expression(0);

	  /* Replace the reference to the array re to *eto: */
      entity renew = make_new_array_variable(get_current_module_entity(),copy_basic(entity_basic(re)),the_dims);
	  eto = make_temporary_pointer_to_array_entity_with_prefix(entity_local_name(re),renew,init);
	  AddEntityToCurrentModule(eto);
      isolate_patch_entities(stat,re,eto,the_offsets);

	  val=malloc(sizeof(*val));
	  val->new_ent=eto;
	  val->s=s;
	  hash_put(e2e,re,val);
	}
	else {
	  eto = val->new_ent;
	  val->s=s;/*to avoid duplicate*/
	}
	the_dma = instruction_to_statement(make_instruction_call(dimensions_to_dma(re,eto,the_dims,the_offsets,s)));
	statements=CONS(STATEMENT,the_dma,statements);
      }
    }
  }
  gen_free_list(effects);
  if (statements == NIL)
    return statement_undefined;
  else
    return make_block_statement(statements);
}

void do_isolate_statement(statement s) {
    statement allocates, loads, stores, deallocates;
    /* this hash table holds an entity to (entity + tag ) binding */
    hash_table e2e = hash_table_make(hash_pointer,HASH_DEFAULT_SIZE);
    allocates = effects_to_dma(s,dma_allocate,e2e);
    loads = effects_to_dma(s,dma_load,e2e);
    stores = effects_to_dma(s,dma_store,e2e);
    deallocates = effects_to_dma(s,dma_deallocate,e2e);
    HASH_MAP(k,v,free(v),e2e);
    hash_table_free(e2e);

    /* Add the calls now if needed, in the correct order: */
    if (loads != statement_undefined)
        insert_statement(s, loads,true);
    if (allocates != statement_undefined)
        insert_statement(s, allocates,true);
    if (stores != statement_undefined)
        insert_statement(s, stores,false);
    if (deallocates != statement_undefined)
        insert_statement(s, deallocates,false);
}

static void
kernel_load_store_generator(statement s, string module_name)
{
    if(statement_call_p(s))
    {
        call c = statement_call(s);
        if(!call_intrinsic_p(c) &&
                same_string_p(module_local_name(call_function(c)),module_name))
        {
            do_isolate_statement(s);
        }
    }
}

/* run kernel load store using either region or effect engine,
 */
static bool kernel_load_store_engine(char *module_name,const char * enginerc) {
    /* generate a load stores on each caller */

    debug_on("KERNEL_LOAD_STORE_DEBUG_LEVEL");

    callees callers = (callees)db_get_memory_resource(DBR_CALLERS,module_name,true);
    FOREACH(STRING,caller_name,callees_callees(callers)) {
        /* prelude */
        set_current_module_entity(module_name_to_entity( caller_name ));
        set_current_module_statement((statement) db_get_memory_resource(DBR_CODE, caller_name, true) );
        set_cumulated_rw_effects((statement_effects)db_get_memory_resource(enginerc, caller_name, true));
        module_to_value_mappings(get_current_module_entity());
        set_precondition_map( (statement_mapping) db_get_memory_resource(DBR_PRECONDITIONS, caller_name, true) );
        /*do the job */
        gen_context_recurse(get_current_module_statement(),module_name,statement_domain,gen_true,kernel_load_store_generator);
        /* validate */
        module_reorder(get_current_module_statement());
        DB_PUT_MEMORY_RESOURCE(DBR_CODE, caller_name,get_current_module_statement());
        DB_PUT_MEMORY_RESOURCE(DBR_CALLEES, caller_name, compute_callees(get_current_module_statement()));

        /*postlude*/
        reset_precondition_map();
        free_value_mappings();
        reset_cumulated_rw_effects();
        reset_current_module_entity();
        reset_current_module_statement();
    }

    /*flag the module as kernel if not done */
    callees kernels = (callees)db_get_memory_resource(DBR_KERNELS,"",true);
    bool found = false;
    FOREACH(STRING,kernel_name,callees_callees(kernels))
        if( (found=(same_string_p(kernel_name,module_name))) ) break;
    if(!found)
        callees_callees(kernels)=CONS(STRING,strdup(module_name),callees_callees(kernels));
    db_put_or_update_memory_resource(DBR_KERNELS,"",kernels,true);

    debug_off();

    return true;
}


/** Generate malloc/copy-in/copy-out on the call sites of this module.
 * existe for region or effect engine
 */
bool kernel_load_store(char *module_name) {
    return kernel_load_store_engine(module_name,DBR_CUMULATED_EFFECTS);
}
bool kernel_load_store_fine_grain(char *module_name) {
    return kernel_load_store_engine(module_name,DBR_REGIONS);
}



/**
 * create a statement eligible for outlining into a kernel
 * #1 find the loop flagged with loop_label
 * #2 make sure the loop is // with local index
 * #3 perform strip mining on this loop to make the kernel appear
 * #4 perform two outlining to separate kernel from host
 *
 * @param s statement where the kernel can be found
 * @param loop_label label of the loop to be turned into a kernel
 *
 * @return true as long as the kernel is not found
 */
static
bool do_kernelize(statement s, entity loop_label)
{
  if( same_entity_p(statement_label(s),loop_label) ||
      (statement_loop_p(s) && same_entity_p(loop_label(statement_loop(s)),loop_label)))
    {
      if( !instruction_loop_p(statement_instruction(s)) )
	pips_user_error("you choosed a label of a non-doloop statement\n");



      loop l = instruction_loop(statement_instruction(s));

      /* gather and check parameters */
      int nb_nodes = get_int_property("KERNELIZE_NBNODES");
      while(!nb_nodes)
        {
	  string ur = user_request("number of nodes for your kernel?\n");
	  nb_nodes=atoi(ur);
        }

      /* verify the loop is parallel */
      if( execution_sequential_p(loop_execution(l)) )
	pips_user_error("you tried to kernelize a sequential loop\n");
      if( !entity_is_argument_p(loop_index(statement_loop(s)),loop_locals(statement_loop(s))) )
	pips_user_error("you tried to kernelize a loop whose index is not private\n");

      if(nb_nodes >1 )
      {
      /* we can strip mine the loop */
      loop_strip_mine(s,nb_nodes,-1);
      /* unfortunetly, the strip mining does not exactly does what we
	 want, fix it here

	 it is legal because we know the loop index is private,
	 otherwise the end value of the loop index may be used
	 incorrectly...
      */
      {
	statement s2 = loop_body(statement_loop(s));
	entity outer_index = loop_index(statement_loop(s));
	entity inner_index = loop_index(statement_loop(s2));
	replace_entity(s2,inner_index,outer_index);
	loop_index(statement_loop(s2))=outer_index;
	replace_entity(loop_range(statement_loop(s2)),outer_index,inner_index);
	if(!ENDP(loop_locals(statement_loop(s2)))) replace_entity(loop_locals(statement_loop(s2)),outer_index,inner_index);
	loop_index(statement_loop(s))=inner_index;
	replace_entity(loop_range(statement_loop(s)),outer_index,inner_index);
    RemoveLocalEntityFromDeclarations(outer_index,get_current_module_entity(),get_current_module_statement());
	loop_body(statement_loop(s))=make_block_statement(make_statement_list(s2));
	AddLocalEntityToDeclarations(outer_index,get_current_module_entity(),loop_body(statement_loop(s)));
	l = statement_loop(s);
      }
      }

      string kernel_name=get_string_property_or_ask("KERNELIZE_KERNEL_NAME","name of the kernel ?");
      string host_call_name=get_string_property_or_ask("KERNELIZE_HOST_CALL_NAME","name of the fucntion to call the kernel ?");

      /* validate changes */
      callees kernels=(callees)db_get_memory_resource(DBR_KERNELS,"",true);
      callees_callees(kernels)= CONS(STRING,strdup(host_call_name),callees_callees(kernels));
      DB_PUT_MEMORY_RESOURCE(DBR_KERNELS,"",kernels);

      entity cme = get_current_module_entity();
      statement cms = get_current_module_statement();
      module_reorder(get_current_module_statement());
      DB_PUT_MEMORY_RESOURCE(DBR_CODE, get_current_module_name(),get_current_module_statement());
      DB_PUT_MEMORY_RESOURCE(DBR_CALLEES, get_current_module_name(), compute_callees(get_current_module_statement()));
      reset_current_module_entity();
      reset_current_module_statement();

      /* recompute effects */
      proper_effects(module_local_name(cme));
      cumulated_effects(module_local_name(cme));
      set_current_module_entity(cme);
      set_current_module_statement(cms);
      set_cumulated_rw_effects((statement_effects)db_get_memory_resource(DBR_CUMULATED_EFFECTS, get_current_module_name(), TRUE));
      /* outline the work and kernel parts*/
      outliner(kernel_name,make_statement_list(loop_body(l)));
      (void)outliner(host_call_name,make_statement_list(s));
      reset_cumulated_rw_effects();




      /* job done */
      gen_recurse_stop(NULL);

    }
  return true;
}


/**
 * turn a loop flagged with LOOP_LABEL into a kernel (GPU, terapix ...)
 *
 * @param module_name name of the module
 *
 * @return true
 */
bool kernelize(char * module_name)
{
  /* prelude */
  set_current_module_entity(module_name_to_entity( module_name ));
  set_current_module_statement((statement) db_get_memory_resource(DBR_CODE, module_name, TRUE) );

  /* retreive loop label */
  string loop_label_name = get_string_property_or_ask("LOOP_LABEL","label of the loop to turn into a kernel ?\n");
  entity loop_label_entity = find_label_entity(module_name,loop_label_name);
  if( entity_undefined_p(loop_label_entity) )
    pips_user_error("label '%s' not found in module '%s' \n",loop_label_name,module_name);


  /* run kernelize */
  gen_context_recurse(get_current_module_statement(),loop_label_entity,statement_domain,do_kernelize,gen_null);

  /* validate */
  module_reorder(get_current_module_statement());
  DB_PUT_MEMORY_RESOURCE(DBR_CODE, module_name,get_current_module_statement());
  DB_PUT_MEMORY_RESOURCE(DBR_CALLEES, module_name, compute_callees(get_current_module_statement()));

  /*postlude*/
  reset_current_module_entity();
  reset_current_module_statement();
  return true;
}

bool flag_kernel(char * module_name)
{
  if (!db_resource_p(DBR_KERNELS, ""))
    pips_internal_error("kernels not initialized");
  callees kernels=(callees)db_get_memory_resource(DBR_KERNELS,"",true);
  callees_callees(kernels)= CONS(STRING,strdup(module_name),callees_callees(kernels));
  DB_PUT_MEMORY_RESOURCE(DBR_KERNELS,"",kernels);
  return true;
}

bool bootstrap_kernels(__attribute__((unused)) char * module_name)
{
  if (db_resource_p(DBR_KERNELS, ""))
    pips_internal_error("kernels already initialized");
  callees kernels=make_callees(NIL);
  DB_PUT_MEMORY_RESOURCE(DBR_KERNELS,"",kernels);
  return true;
}


