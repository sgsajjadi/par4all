 /* pipsmake: call by need (make), rule selection (activate), explicit call (apply)
  *
  * Remi Triolet, Francois Irigoin, Pierre Jouvelot, Bruno Baron,
  * Arnauld Leservot, Guillaume Oget
  *
  * Notes: 

  *  - pismake uses some RI fields explicitly

  *  - see Bruno Baron's DEA thesis for more details

  *  - do not forget the difference between *virtual* resources like CALLERS.CODE
  *  and *real* resources like FOO.CODE; CALLERS is a variable (or a function) whose
  *  value depends on the current module; it is expanded into a list of real resources;
  *  the variables are CALLEES, CALLERS, ALL and MODULE (the current module itself);
  *  these variables are used to implement top-down and bottom-up traversals
  *  of the call tree; they make pipsmake different from make

  *  - memoization added to make() to speed-up a sequence of interprocedural 
  *  requests on real applications; a resource r is up-to-date if it already has been
  *  proved up-to-date, or if all its arguments have been proved up-to-date and
  *  all its arguments are in the database and all its arguments are
  *  older than the requested resource r; this scheme is correct as soon as activate()
  *  destroys the resources produced by the activated (and de-activated) rule

  *  - include of an automatically generated builder_map

  *  - explicit *recursive* destruction of obsolete resources by
  *  activate() but not by apply(); beware! You cannot assume that all
  *  resources in the database are consistent;
  *
  */
#include <stdio.h>
extern int fprintf();
/* #include <stdlib.h> */
#include <sys/types.h>

#include "genC.h"

#include "ri.h"
#include "database.h"
#include "makefile.h"

#include "misc.h"

#include "pipsdbm.h"
#include "ri-util.h"
#include "resources.h"
#include "phases.h"
#include "builder_map.h"
#include "pipsmake.h"

/*
 * static functions 
 */
static list build_real_resources();
static void update_preserved_resources();
static void rmake();
static void apply_a_rule();
static void apply_without_reseting_up_to_date_resources();
static bool up_date_without_reseting_up_to_date_p();
static void make_pre_transformation();
static void make_required();

/*
 * Apply an instanciated rule with a given ressource owner 
 */

static void apply_a_rule(oname, ru)
string oname;
rule ru;
{
    struct builder_map *pbm = builder_maps;
    bool first_time = TRUE;
    string run = rule_phase(ru);
    string rname;
    string rowner;
    bool is_required;

    if (interrupt_pipsmake_asap_p())
	return;

    MAPL(prr, {
	rname = real_resource_resource_name(REAL_RESOURCE(CAR(prr)));
	rowner = real_resource_owner_name(REAL_RESOURCE(CAR(prr)));
	is_required = FALSE;

	MAPL (prrr, {
	    if ((same_string_p (rname, real_resource_resource_name(REAL_RESOURCE(CAR(prrr))))) &&
		(same_string_p (rowner, real_resource_owner_name(REAL_RESOURCE(CAR(prrr))))))
	    {
		is_required = TRUE;
		break;
	    }
	}, build_real_resources(oname,rule_required (ru)));

	user_log("  %-30.60s %8s   %s(%s)\n", 
		 first_time == TRUE ? (first_time = FALSE,run) : "",
		 is_required == TRUE ? "updating" : "building",
		 rname, rowner);

    }, build_real_resources(oname,rule_produced(ru)));

    for (pbm = builder_maps; pbm->builder_name != NULL; pbm++) {
	if (same_string_p(pbm->builder_name, run)) {
	    (*pbm->builder_func)(oname);

	    pips_malloc_debug();

	    update_preserved_resources(oname, ru);

	    return;
	}
    }

    pips_error("apply_a_rule", "could not find function %s\n", run);
}

/* FI: make is very slow when interprocedural analyzes have been selected;
 * some memorization has been added; we need to distinguish betweeen an
 * external make which initializes a set of up-to-date resources and
 * an internal recursive make which updates and exploits that set.
 *
 * This new functionality is extremely useful when old databases
 * are re-opened.
 *
 * apply(), which calls make many times, does not fully benefit from
 * this memoization scheme.
 */
static set up_to_date_resources = set_undefined;

void make(rname, oname)
string rname, oname;
{
    debug_on("PIPSMAKE_DEBUG_LEVEL");
    debug(1, "make", "%s(%s) - requested\n", rname, oname);

    up_to_date_resources = set_make(set_pointer);

    dont_interrupt_pipsmake_asap();

    rmake(rname, oname);

    if ( signal_occured() ) {
	accounting_signal();
	make_close_program();
	exit(1);
    }

    set_free(up_to_date_resources);
    up_to_date_resources = set_undefined;

    debug(1, "make", "%s(%s) - made\n", rname, oname);
    debug_off();
}

static void rmake(rname, oname)
string rname, oname;
{
    resource res;
    rule ru;
    
    debug(2, "rmake", "%s(%s) - requested\n", rname, oname);

    if (interrupt_pipsmake_asap_p())
	return;

    /* do we have this resource in our database ? */
    res = db_find_resource(rname, oname);

    /* is it up to date ? */
    if (res != resource_undefined) {
	if(set_belong_p(up_to_date_resources, (char *) res)) {
	    debug(8, "rmake", "resource %s(%s) found in up_to_date\n",
		  rname, oname);
	    return;
	}
    }
    
    /* we look for the active rule to produce this resource */
    if ((ru = find_rule_by_resource(rname)) == rule_undefined) {
	pips_error("rmake", "could not find a rule for %s\n", rname);
    }

    /* we recursively make the pre transformations */
    (void) make_pre_transformation(oname, ru);

    if (interrupt_pipsmake_asap_p())
	return;

    /* we recursively make required resources */
    make_required(oname, ru);


    if (up_date_without_reseting_up_to_date_p (rname,oname)) {
	
	debug (8,"rmake",
	       "Resource %s(%s) becomes up-to-date after applying pre-transformations and building required resources\n",
	       rname,oname);
    } else {

	if (interrupt_pipsmake_asap_p())
	    return;

	/* we build the resource */
	apply_a_rule(oname, ru);

	if (interrupt_pipsmake_asap_p())
	    return;

	/* set up-to-date all the produced resources for that rule */
	MAPL(prr, {
	    real_resource rr = REAL_RESOURCE(CAR(prr));

	    string rron = real_resource_owner_name(rr);
	    string rrrn = real_resource_resource_name(rr);

	    res = db_find_resource(rrrn, rron);

	    if (res != resource_undefined) {
		debug(8, "rmake", "resource %s(%s) added to up_to_date\n",
		      rname, oname);
		set_add_element(up_to_date_resources,
				up_to_date_resources, (char *) res);
	    }
	    else {
		pips_error("rmake", "resource %s(%s) just built is not found!\n", rname, oname );
	    }
	}, build_real_resources(oname, rule_produced(ru)));
    }
}

void apply(pname, oname)
string pname, oname;
{
    debug_on("PIPSMAKE_DEBUG_LEVEL");
    debug(1, "apply", "%s.%s - requested\n", oname, pname);

    up_to_date_resources = set_make(set_pointer);

    dont_interrupt_pipsmake_asap();

    apply_without_reseting_up_to_date_resources (pname,oname);

    set_free(up_to_date_resources);
    up_to_date_resources = set_undefined;

    debug(1, "apply", "%s.%s - done", oname, pname);
    debug_off();
}

static void apply_without_reseting_up_to_date_resources(pname, oname)
string pname, oname;
{
    rule ru;
    
    debug(2, "apply_without_reseting_up_to_date_resources",
	  "apply %s on %s\n", pname, oname);

    if (interrupt_pipsmake_asap_p())
	return;

    /* we look for the rule describing this phase */
    if ((ru = find_rule_by_phase(pname)) == rule_undefined)
	pips_error("apply", "could not find rule %s\n", pname);

    (void) make_pre_transformation(oname, ru);

    if (interrupt_pipsmake_asap_p())
	return;

    make_required(oname, ru);

    if (interrupt_pipsmake_asap_p())
	return;

    apply_a_rule (oname, ru);
}

/* this function returns the active rule to produce resource rname */
rule find_rule_by_resource(rname)
string rname;
{
    makefile m = parse_makefile();

    debug(5, "find_rule_by_resource", 
	  "searching rule for resource %s\n", rname);

    /* walking thru rules */
    MAPL(pr, {
	rule r = RULE(CAR(pr));
	bool resource_required_p = FALSE;


	/* walking thru resources required by this rule */
	MAPL(pvr, {
	    virtual_resource vr = VIRTUAL_RESOURCE(CAR(pvr));
	    string vrn = virtual_resource_name(vr);
	    owner vro = virtual_resource_owner(vr);

	    /* We do not check callers and callees */
	    if ( owner_callers_p(vro) || owner_callees_p(vro) ) {}
	    /* Is this resource required ?? */
	    else if (same_string_p(vrn, rname))
		resource_required_p = TRUE;

	}, rule_required(r));

	/* If this particular resource is not required */
	if (!resource_required_p) {
	    /* walking thru resources made by this particular rule */
	    MAPL(pvr, {
		virtual_resource vr = VIRTUAL_RESOURCE(CAR(pvr));
		string vrn = virtual_resource_name(vr);

		if (same_string_p(vrn, rname)) {

		    debug(5, "find_rule_by_resource", 
			  "made by phase %s\n", rule_phase(r));

		    /* is this phase an active one ? */
		    MAPL(pp, {
			if (same_string_p(STRING(CAR(pp)), rule_phase(r))) {
			    debug(5, "find_rule_by_resource", "active phase\n");
			    return(r);
			}
		    }, makefile_active_phases(m));

		    debug(5, "find_rule_by_resource", "inactive phase\n");
		}
	    }, rule_produced(r));
	}
    }, makefile_rules(m));

    return(rule_undefined);
}

/* Translate and expand a list of virtual resources into a potentially much longer
 * list of real resources
 *
 * In spite of the name, no resource is actually built.
 */
static list build_real_resources(oname, lvr)
string oname;
list lvr;
{
    cons *pvr, *ps;
    list result = NIL;

    for (pvr = lvr; pvr != NIL; pvr = CDR(pvr)) {
	virtual_resource vr = VIRTUAL_RESOURCE(CAR(pvr));
	string vrn = virtual_resource_name(vr);
	tag vrt = owner_tag(virtual_resource_owner(vr));

	switch (vrt) {
	case is_owner_program:
	    result = gen_nconc(result, CONS(REAL_RESOURCE, 
					    make_real_resource(vrn, db_get_current_program_name()),
					    NIL));
	    break;

	case is_owner_module:
	    result = gen_nconc(result, 
			       CONS(REAL_RESOURCE, 
				    make_real_resource(vrn, oname),
				    NIL));
	    break;

	case is_owner_main:
	{
	    int nmodules = 0;
	    char *module_list[ARGS_LENGTH];
	    int i;
	    int number_of_main = 0;

	    db_get_module_list(&nmodules, module_list);
	    pips_assert("build_real_resources", nmodules>0);
	    for(i=0; i<nmodules; i++) {
		string on = module_list[i];

		if (entity_main_module_p(local_name_to_top_level_entity(on)) == TRUE)
		{
		    if (number_of_main)
			pips_error("build_real_resources", "More the one main\n");

		    number_of_main++;
		    debug(8, "build_real_resources", "Main is %s\n", on);
		    result = gen_nconc(result, 
				       CONS(REAL_RESOURCE, 
					    make_real_resource(vrn, on),
					    NIL));
		}
	    }
	    break;
	}
	case is_owner_callees:
	{
	    callees called_modules;
	    list lcallees;

	    rmake(DBR_CALLEES, oname);

	    if (interrupt_pipsmake_asap_p())
		return result;

	    called_modules = (callees) 
		db_get_memory_resource(DBR_CALLEES, oname, TRUE);
	    lcallees = callees_callees(called_modules);

	    debug(8, "build_real_resources", "Callees of %s are:\n", oname);

	    for (ps = lcallees; ps != NIL; ps = CDR(ps)) {
		string on = STRING(CAR(ps));

		debug(8, "build_real_resources", "\t%s\n", on);

		result = gen_nconc(result, 
				   CONS(REAL_RESOURCE, 
					make_real_resource(vrn, on),
					NIL));
	    }
	    break;
	}
	case is_owner_callers:
	{
	    /* FI: the keyword callees was badly chosen; anyway, it's just
	       a list of strings... see ri.newgen */
	    callees caller_modules;
	    list lcallers;

	    rmake(DBR_CALLERS, oname);

	    if (interrupt_pipsmake_asap_p())
		return result;

	    caller_modules = (callees) 
		db_get_memory_resource(DBR_CALLERS, oname, TRUE);
	    lcallers = callees_callees(caller_modules);

	    debug(8, "build_real_resources", "Callers of %s are:\n", oname);

	    for (ps = lcallers; ps != NIL; ps = CDR(ps)) {
		string on = STRING(CAR(ps));

		debug(8, "build_real_resources", "\t%s\n", on);

		result = gen_nconc(result, 
				   CONS(REAL_RESOURCE, 
					make_real_resource(vrn, on),
					NIL));
	    }
	    break;
	}
	case is_owner_all:
	{
	    int nmodules = 0;
	    char *module_list[ARGS_LENGTH];
	    int i;

	    db_get_module_list(&nmodules, module_list);
	    pips_assert("build_real_resource", nmodules>0);
	    for(i=0; i<nmodules; i++) {
		string on = module_list[i];

		debug(8, "build_real_resources", "\t%s\n", on);

		result = gen_nconc(result, 
				   CONS(REAL_RESOURCE, 
					make_real_resource(vrn, on),
					NIL));
	    }
	    break;
	}
	default:
	    pips_error("build_real_resources", "unknown tag : %d\n", vrt);
	}
    }

    return(result);
}

/* compute all pre-transformations to apply a rule on an object */
static void make_pre_transformation(oname, ru)
rule ru;
string oname;
{
    list reals;

    if (interrupt_pipsmake_asap_p())
	return;

    /* we build the list of pre transformation real_resources */
    reals = build_real_resources(oname, rule_pre_transformation(ru));

    /* we recursively make the resources */
    MAPL(prr, {
	real_resource rr = REAL_RESOURCE(CAR(prr));

	string rron = real_resource_owner_name(rr);
	/* actually the resource name is a phase name !! */
	string rrpn = real_resource_resource_name(rr);

	debug(3, "make_pre_transformation", "rule %s : applying %s to %s - recursive call\n",
	      rule_phase(ru),
	      rrpn,
	      rron);

	apply_without_reseting_up_to_date_resources (rrpn, rron);

	if (interrupt_pipsmake_asap_p())
	    return;

    }, reals);

}

/* compute all resources needed to apply a rule on an object */
static void make_required(oname, ru)
rule ru;
string oname;
{
    list reals;

    /* we build the list of required real_resources */
    reals = build_real_resources(oname, rule_required(ru));

    if (!interrupt_pipsmake_asap_p()) {

	/* we recursively make required resources */
	MAPL(prr, {
	    real_resource rr = REAL_RESOURCE(CAR(prr));

	    string rron = real_resource_owner_name(rr);
	    string rrrn = real_resource_resource_name(rr);

	    debug(3, "make_required", "rule %s : %s(%s) - recursive call\n",
		  rule_phase(ru),
		  rrrn,
		  rron);

	    if (interrupt_pipsmake_asap_p())
		break;

	    (void) rmake(rrrn, rron);

	    /* ici nous devons  tester si un des regles modified
	       fait partie des required. Dans ce cas on la fabrique
	       de suite. */

	}, reals);
    }

    gen_free_list (reals);
    return;
}

static void update_preserved_resources(oname, ru)
rule ru;
string oname;
{
    list reals;

    /* we build the list of preserved real_resources */
    reals = build_real_resources(oname, rule_preserved(ru));

    /* we update the timestamps of these resources */
    MAPL(prr, {
	real_resource rr = REAL_RESOURCE(CAR(prr));

	string rron = real_resource_owner_name(rr);
	string rrrn = real_resource_resource_name(rr);

	debug(3, "update_preserved_resources",
	      "%s(%s) is preserved\n", rrrn, rron);

	db_update_time(rrrn, rron);
	
    }, reals);

    gen_free_list (reals);
    /* we build the list of modified real_resources */
    reals = build_real_resources(oname, rule_modified(ru));

    /* we delete them from the uptodate set */
    MAPL(prr, {
	real_resource rr = REAL_RESOURCE(CAR(prr));

	string rron = real_resource_owner_name(rr);
	string rrrn = real_resource_resource_name(rr);

	/* is it up to date ? */
	if(set_belong_p(up_to_date_resources, (char *) rr))
	{
	    debug(3, "update_preserved_resources",
		  "resource %s(%s) deleted from up_to_date\n",
		  rrrn, rron);
	    set_del_element (up_to_date_resources,
			     up_to_date_resources,
			     (char *) rr);
	    /* GO 11/7/95: we need to del the resource from the data base
	       for a next call of pipsmake to find it unavailable */
	    db_unput_a_resource (rrrn, rron);
	}
    }, reals);

    gen_free_list (reals);
}

bool real_resource_up_to_date_p(rname, oname)
string rname, oname;
{
    bool result;

    debug_on("PIPSMAKE_DEBUG_LEVEL");

    up_to_date_resources = set_make(set_pointer);

    dont_interrupt_pipsmake_asap();

    result = up_date_without_reseting_up_to_date_p(rname,oname);

    set_free(up_to_date_resources);
    up_to_date_resources = set_undefined;

    debug_off();
    
    return result;
}

static bool up_date_without_reseting_up_to_date_p(rname, oname)
string rname, oname;
{
    resource res;
    list reals;
    rule ru;
    bool result = TRUE;

    res = db_find_resource(rname, oname);

    /* The resource should be in the data base */
    if (res == resource_undefined)
	return FALSE;

    /* Maybe is has already been proved true */
    if(set_belong_p(up_to_date_resources, (char *) res))
	return TRUE;

    /* We get the active rule to build this resource */
    if ((ru = find_rule_by_resource(rname)) == rule_undefined) {
	pips_error("up_date_without_reseting_up_to_date_p",
		   "could not find a rule for %s\n", rname);
    }

    /* we build the list of required real_resources */
    /* Here we are sure (thanks to find_rule_by_resource) that the rule does not
       use a resource it produces */

    reals = build_real_resources(oname, rule_required(ru));

    /* we are going to check if the required resources are 
       - in the database or in the rule_modified list
       - proved up to date (recursively)
       - have timestamps older than the tested one
       */
    MAPL(prr, {
	real_resource rr = REAL_RESOURCE(CAR(prr));

	string rron = real_resource_owner_name(rr);
	string rrrn = real_resource_resource_name(rr);

	bool res_in_modified_list_p = FALSE;
	    
	/* we build the list of modified real_resources */
	list virtuals = rule_modified(ru);
	list reals2 = build_real_resources(oname, virtuals);

	MAPL(mod_prr, {
	    real_resource mod_rr = REAL_RESOURCE(CAR(mod_prr));
	    string mod_rron = real_resource_owner_name(mod_rr);
	    string mod_rrrn = real_resource_resource_name(mod_rr);

	    if ((same_string_p(mod_rron, rron)) &&
		(same_string_p(mod_rrrn, rrrn))) {
		/* we found it */
		res_in_modified_list_p = TRUE;
		debug(3, "up_date_without_reseting_up_to_date_p",
		      "resource %s(%s) is in the rule_modified list",
		      rrrn, rron);
		break;
	    }
	}, reals2);

	gen_free_list (virtuals);
	gen_free_list (reals2);

	/* If the rule is in the modified list, then
	   don't check anything */
	if (res_in_modified_list_p == FALSE) {

	    resource resp = db_find_resource(rrrn, rron);

	    if (resp == resource_undefined) {
		debug(5, "up_date_without_reseting_up_to_date_p",
		      "resource %s(%s) is not present and not in the rule_modified list",
		      rrrn, rron);
		result = FALSE;
		break;
	    } else {
		/* Check if this resource is up to date */
		if (up_date_without_reseting_up_to_date_p(rrrn, rron) == FALSE) {
		    debug(5, "up_date_without_reseting_up_to_date_p",
			  "resource %s(%s) is not up to date", rrrn, rron);
		    result = FALSE;
		    break;
		}
		/* Check if the timestamp is OK */
		if (resource_time(res) < resource_time(resp)) {
		    debug(5, "up_date_without_reseting_up_to_date_p",
			  "resource %s(%s) is newer (%ld < %ld)\n",
			  rrrn, rron,
			  (long) resource_time(res), (long) resource_time(resp));
		    result = FALSE;
		    break;
		}
	    }
	}
    }, reals);

    gen_free_list (reals);

    /* If the resource is up to date then add it in the set */
    if (result == TRUE) {
	debug(5, "up_date_without_reseting_up_to_date_p",
	      "resource %s(%s) added to up_to_date\n",
	      rname, oname);
	set_add_element(up_to_date_resources,
			up_to_date_resources, (char *) res);
    }
    return result;
}


/* Delete from up_to_date all the resources of a given name */
void delete_named_resources (rn)
string rn;
{
    /* GO 29/6/95: many lines ...
       db_unput_resources_verbose (rn);*/
    db_unput_resources(rn);

    if (up_to_date_resources != set_undefined) {
	/* In this case we are called from a Pips phase */
	user_warning ("delete_named_resources",
		      "called within a phase (i.e. by activate())\n");
	SET_MAP(res, {
	    string res_rn = real_resource_resource_name((real_resource) res);
	    string res_on = real_resource_owner_name((real_resource) res);

	    if (same_string_p(rn, res_rn)) {
		debug(5, "delete_named_resources",
		      "resource %s(%s) deleted from up_to_date\n",
		      res_rn, res_on);
		set_del_element (up_to_date_resources,
				 up_to_date_resources,
				 (char *) res);
	    }
	}, up_to_date_resources);
    }
}

string get_first_main_module()
{
    int nmodules = 0;
    char *module_list[ARGS_LENGTH];

    /* int i;*/
    string mmn = string_undefined;

    debug_on("PIPSMAKE_DEBUG_LEVEL");

    /* Get the module list */
    db_get_module_list(&nmodules, module_list);
    if (!nmodules)
	return NULL;

    mmn = module_list[nmodules - 1];

    debug_off ();
    return mmn;
}
