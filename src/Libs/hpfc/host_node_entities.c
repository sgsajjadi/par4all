/*
 * HPFC module by Fabien COELHO
 *
 * $RCSfile: host_node_entities.c,v $ ($Date: 1995/09/15 15:54:42 $, ) 
 * version $Revision$
 */

#include "defines-local.h"

#include "control.h"
#include "regions.h"
#include "semantics.h"
#include "effects.h"

/*      HOST AND NODE ENTITIES MANAGEMENT
 */
GENERIC_GLOBAL_FUNCTION(new_host, entitymap)
GENERIC_GLOBAL_FUNCTION(old_host, entitymap)
GENERIC_GLOBAL_FUNCTION(new_node, entitymap)
GENERIC_GLOBAL_FUNCTION(old_node, entitymap)

void store_new_node_variable(new, old)
entity new, old;
{
    assert(!entity_undefined_p(new) && !entity_undefined_p(old));
    store_new_node(old, new), store_old_node(new, old);
}

void store_new_host_variable(new, old)
entity new, old;
{
    assert(!entity_undefined_p(new) && !entity_undefined_p(old));
    store_new_host(old, new), store_old_host(new, old);
}

void init_entity_status()
{
    init_new_host();
    init_old_host();
    init_new_node();
    init_old_node();
    init_referenced_variables(); /* in ri-util/module.x */
}

entity_status get_entity_status()
{
    return(make_entity_status(get_new_host(),
			      get_new_node(),
			      get_old_host(),
			      get_old_node(),
			      get_referenced_variables()));
}

void set_entity_status(s)
entity_status s;
{
    set_new_host(entity_status_new_host(s));
    set_new_node(entity_status_new_node(s));
    set_old_host(entity_status_old_host(s));
    set_old_node(entity_status_old_node(s));
    set_referenced_variables(entity_status_referenced(s));
}

void reset_entity_status()
{
    reset_new_host();
    reset_old_host();
    reset_new_node();
    reset_old_node();
    reset_referenced_variables();
}

void close_entity_status()
{
    close_new_host();
    close_old_host();
    close_new_node();
    close_old_node();
    close_referenced_variables();
}

string hpfc_module_suffix(module)
entity module;
{
    if (module==node_module) return(NODE_NAME);
    if (module==host_module) return(HOST_NAME);
    /* else
     */
    pips_error("hpfc_module_suffix", "unexpected module\n");
    return(string_undefined); /* to avoid a gcc warning */
}
  

/* UPDATES
 */

static entity current_updated_module = entity_undefined;

static bool bound_p(e)
entity e;
{
    if (current_updated_module==node_module) return(bound_new_node_p(e));
    if (current_updated_module==host_module) return(bound_new_host_p(e));
    /* else
     */
    pips_error("bound_p", "invalid current module\n");
    return(FALSE);
}

static entity load(e)
entity e;
{
    
    if (current_updated_module==node_module) return(load_new_node(e));
    if (current_updated_module==host_module) return(load_new_host(e));
    /* else
     */
    pips_error("load", "invalid current module\n");
    return(FALSE);
}

static void update_for_module_rewrite(pe)
entity *pe;
{
    if (bound_p(*pe)) *pe = load(*pe);
}

/* shift the references to the right variable, in the module
 */
static void update_reference_for_module_rewrite(ref)
reference ref;
{
    update_for_module_rewrite(&reference_variable(ref));
}

/* shift the calls to the right variable, in the module
 */
static void update_call_for_module_rewrite(c)
call c;
{
    update_for_module_rewrite(&call_function(c));
}

static void update_code_for_module_rewrite(c)
code c;
{
    MAPL(ce, update_for_module_rewrite(&ENTITY(CAR(ce))), code_declarations(c));
}

static void update_loop_for_module_rewrite(l)
loop l;
{
    update_for_module_rewrite(&loop_index(l));
}

void update_object_for_module(obj, module)
gen_chunk *obj; /* loosely typed, indeed */
entity module;
{
    entity saved = current_updated_module;

    debug(8, "update_object_for_module", "updating (%s) 0x%x\n",
	  gen_domain_name(gen_type(obj)), (unsigned int) obj);

    current_updated_module = module;

    gen_multi_recurse(obj, 
		      /* 
		       *   REFERENCES
		       */
		      reference_domain, 
		      gen_true, 
		      update_reference_for_module_rewrite,
		      /*
		       *   LOOPS (indexes)
		       */
		      loop_domain,
		      gen_true,
		      update_loop_for_module_rewrite,
		      /*
		       *   CALLS
		       */
		      call_domain, 
		      gen_true, 
		      update_call_for_module_rewrite,
		      /*
		       *   CODES
		       */
		      code_domain,
		      gen_true,
		      update_code_for_module_rewrite,
		      NULL);

    current_updated_module = saved;
}

void update_list_for_module(l, module)
list l;
entity module;
{
    MAPL(cx, update_object_for_module(CHUNK(CAR(cx)), module), l);
}

/* removed unreferenced items in the common
 * the global map refenreced_variables should be set and ok
 * the variables updated are those local to the common...
 */
void clean_common_declaration(common)
entity common;
{
    type t = entity_type(common);
    list l = NIL, lnew = NIL;

    assert(type_area_p(t));

    l = area_layout(type_area(t));

    MAP(ENTITY, var,
    {
	if (bound_referenced_variables_p(var) &&
	    local_entity_of_module_p(var, common))
	    lnew = CONS(ENTITY, var, lnew);
    },
	l);

    gen_free_list(l);
    area_layout(type_area(t)) = lnew;
}

/*
 * UpdateExpressionForModule
 *
 * this function creates a new expression using the mapping of
 * old to new variables map.
 *
 * some of the structures generated may be shared...
 */

expression UpdateExpressionForModule(module, ex)
entity module;
expression ex;
{
    expression new = copy_expression(ex);
    update_object_for_module(new, module);
    return(new);
}


list lUpdateExpr(module, l)
entity module;
list l;
{
    list new = NIL, rev = NIL;

    MAP(EXPRESSION, e,
	rev = CONS(EXPRESSION, copy_expression(e), rev), l);

    new = gen_nreverse(rev); 
    update_list_for_module(new, module);    
    return(new);
}

list lNewVariableForModule(module, le)
entity module;
list le;
{
    list result, last;

    if (ENDP(le)) return(NIL);

    for (result = CONS(ENTITY, 
		       NewVariableForModule(module, ENTITY(CAR(le))),
		       NIL),
	 last = result, le = CDR(le);
	 !ENDP(le);
	 le = CDR(le), last = CDR(last))
	CDR(last) = CONS(ENTITY, NewVariableForModule(module, ENTITY(CAR(le))),
			 NIL);

    return(result);
}

entity NewVariableForModule(module,e)
entity module;
entity e;
{
    entity saved = current_updated_module, result;
    current_updated_module = module;
    assert(bound_p(e));
    result = load(e);
    current_updated_module = saved;
    return(result);
}

statement UpdateStatementForModule(module, stat)
entity module;
statement stat;
{
    statement new_stat = copy_statement(stat);
    update_object_for_module(new_stat, module);
    return(new_stat);
}

/*   That is all
 */
