/*
 * $Id$
 *
 * Here is the database!
 *
 * Here are just the internals. 
 * They rely on pipsdbm private data structures, which are not exported!
 * all exported functions should check that DB_OK.
 */

#include "private.h"
#include "pipsdbm_private.h"

/******************************************************************** UTILS */

/* shorthands
 */
#define db_resource_stored_p(r) db_status_stored_p(db_resource_db_status(r))
#define db_resource_loaded_p(r) db_status_loaded_p(db_resource_db_status(r))
#define db_resource_required_p(r) \
        db_status_required_p(db_resource_db_status(r))

/* module names must use some characters.
 */
#define MODULE_NAME_CHARS "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_"

static bool simple_name_p(string name)
{
    return strlen(name)==strspn(name, MODULE_NAME_CHARS);
}

static db_symbol find_or_create_db_symbol(string name)
{
    db_symbol s = gen_find_tabulated(name, db_symbol_domain);
    if (!simple_name_p(name)) pips_user_warning("strange name \"%s\"\n", name);
    return db_symbol_undefined_p(s)? make_db_symbol(strdup(name)): s;
}


/************************************************************** THE DATABASE */

/* the pips_database stores pips resources.
 *
 * {init,close,set_reset,get}_pips_database()
 * {store,load,delete,update,store_or_update}_pips_database()
 * bound_pips_database_p()
 */
GENERIC_LOCAL_FUNCTION(pips_database, db_resources)

#define DB_OK    pips_assert("defined database", !pips_database_undefined_p())
#define DB_UNDEF pips_assert("undefined database", pips_database_undefined_p())

/* exported interface is minimal.
 */
void db_create_pips_database(void)
{
    DB_UNDEF; init_pips_database(); DB_OK;
}

/* @return whether okay.
 */
bool db_open_pips_database(FILE * fd)
{
    db_resources rs;
    DB_UNDEF; 
    rs = read_db_resources(fd);
    if (db_resources_undefined_p(rs)) return FALSE;
    set_pips_database(rs); 
    DB_OK;
    return TRUE;
}

void db_save_pips_database(FILE * fd)
{
    DB_OK; 
    /* ??? check for required resources left over? */
    write_db_resources(fd, get_pips_database());
}

void db_close_pips_database(void)
{
    DB_OK; close_pips_database(); DB_UNDEF;
}

void db_reset_pips_database_if_necessary(void)
{
    /* rough! memory leak... */
    if (!pips_database_undefined_p()) reset_pips_database();
}

/******************************************************** LOAD/SAVE RESOURCE */

static string db_status_string(db_status s)
{
  if (db_status_undefined_p(s))
    return "undefined";
  switch (db_status_tag(s))
  {
  case is_db_status_stored: 
    return "stored";
  case is_db_status_loaded: 
    return "loaded";
  case is_db_status_required: 
    return "required";
  default:
    pips_internal_error("unexpected db_status tag %d\n", db_status_tag(s));
    return NULL;
  }
}

static void dump_db_resource(string rname, string oname, db_resource r)
{
    pips_debug(1, "rname=%s, oname=%s, r=%p\n", rname, oname, r);
    if (!db_resource_undefined_p(r)) {
	db_status s = db_resource_db_status(r);
	pips_debug(1, "pointer=%p, status=%s, time=%d, file_time=%d\n", 
		   db_resource_pointer(r), db_status_string(s),
		   db_resource_time(r), db_resource_file_time(r));
    }
}

#define debug_db_resource(l, r, o, p) ifdebug(l) { dump_db_resource(r, o, p);}

static void init_owned_resources_if_necessary(string name)
{
    db_symbol s = find_or_create_db_symbol(name);
    /* set the owner_resources in the pips_database. */
    if (!bound_pips_database_p(s))
	store_pips_database(s, make_db_owned_resources());;
}

static db_owned_resources get_db_owned_resources(string oname)
{
    db_symbol o = find_or_create_db_symbol(oname);
    return bound_pips_database_p(o)? 
	load_pips_database(o): db_owned_resources_undefined;
}

static db_resource get_resource(string rname, db_owned_resources or)
{
    db_symbol rs = find_or_create_db_symbol(rname);
    db_resource r = bound_db_owned_resources_p(or, rs)?
	apply_db_owned_resources(or, rs): db_resource_undefined;
    debug_db_resource(9, rname, "?", r);
    return r;
}

static db_resource get_db_resource(string rname, string oname)
{
    db_owned_resources or;
    or = get_db_owned_resources(oname);
    if (db_owned_resources_undefined_p(or)) { /* lazy... */ 
	pips_debug(1, "creating or for %s...\n", oname);
	init_owned_resources_if_necessary(oname);
	or = get_db_owned_resources(oname);
    } /* pips_internal_error("no owned resources for %s\n", oname, rname);*/
    return get_resource(rname, or);
}

static db_resource get_real_db_resource(string rname, string oname)
{
    db_resource r = get_db_resource(rname, oname);
    if (db_resource_undefined_p(r) || db_resource_required_p(r))
	pips_internal_error("no resource %s of %s\n", rname, oname);
    return r;
}

static db_resource find_or_create_db_resource(string rname, string oname)
{
    db_resource r;
    db_owned_resources or;
    or = get_db_owned_resources(oname);
    if (db_owned_resources_undefined_p(or))
    { /* still lazy... */
	pips_debug(1, "module %s should have been registered\n", oname);
	init_owned_resources_if_necessary(oname);
	or = get_db_owned_resources(oname);
    }
    r = get_resource(rname, or);
    if (db_resource_undefined_p(r))
    { /* create it */
	db_symbol rs = find_or_create_db_symbol(rname); 
	r = make_db_resource(string_undefined, db_status_undefined, 0, 0);
	extend_db_owned_resources(or, rs, r);
    }
    return r;
}

void db_delete_resource(string rname, string oname)
{
    db_resource r;
    db_owned_resources or;
    DB_OK;

    or = get_db_owned_resources(oname);
    pips_assert("valid owned resources", !db_owned_resources_undefined_p(or));
    r = get_resource(rname, or);
    if (!db_resource_undefined_p(r))
    { /* let us do it! */
	db_symbol rs = find_or_create_db_symbol(rname);
	char * p = db_resource_pointer(r);
	if (db_resource_loaded_p(r) && !string_undefined_p(p))
	    dbll_free_resource(rname, oname, p);
	db_resource_pointer(r) = string_undefined;
	free_db_resource(r);
	delete_db_owned_resources(or, rs);
    }
}

/* this should really be a put. Just there for upward compatibility.
 */
bool db_update_time(string rname, string oname)
{
  db_resource r;
  DB_OK;
  pips_assert("displayable resource", displayable_file_p(rname));
  r = get_real_db_resource(rname, oname);
  db_resource_time(r) = db_get_logical_time();
  db_resource_file_time(r) =
    dbll_stat_local_file(db_resource_pointer(r), FALSE); 
  /*dbll_stat_resource_file(rname, oname, TRUE); */
  return TRUE;
}

void db_print_all_required_resources(FILE * file)
{
  DB_RESOURCES_MAP(os, or,
  {
    DB_OWNED_RESOURCES_MAP(rs, r,
    {
      string rn = db_symbol_name(rs);
      string on = db_symbol_name(os);
      pips_debug(8, "considering %s of %s (%p)\n", rn, on, (char*) r);

      if (db_resource_required_p(r)) {
	fprintf(file, "%s of %s is in 'required' status since %d\n", 
		rn, on, db_resource_time(r));
      }
    },
      or);
  },
		   get_pips_database());
}

void db_clean_all_required_resources(void)
{
  DB_RESOURCES_MAP(os, or,
  {
    DB_OWNED_RESOURCES_MAP(rs, r,
    {
      string rn = db_symbol_name(rs);
      string on = db_symbol_name(os);
      pips_debug(8, "considering %s of %s (%p)\n", rn, on, (char*) r);

      if (db_resource_required_p(r))
      {
	pips_debug(1, "deleting %s of %s\n", rn, on);
	dump_db_resource(rn, on, r);
	db_delete_resource(rn, on);
      }
    },
      or);
  },
		   get_pips_database());

}

/******************************************************* RESOURCE MANAGEMENT */

/* from now on we must not know about the database internals? */

/* TRUE if exists and in *ANY* state. */
bool db_resource_required_or_available_p(string rname, string oname)
{
  DB_OK;
  return !db_resource_undefined_p(get_db_resource(rname, oname));
}

/* TRUE if exists and in required state. */
bool db_resource_is_required_p(string rname, string oname)
{
  db_resource r;
  DB_OK;
  r = get_db_resource(rname, oname);
  if (db_resource_undefined_p(r))
    return FALSE;
  else
    return db_resource_required_p(r);
}

/* TRUE if exists and in loaded or stored state. */
bool db_resource_p(string rname, string oname)
{
  db_resource r; 
  DB_OK; 
  r = get_db_resource(rname, oname);
  if (db_resource_undefined_p(r))
    return FALSE;
  else
    return db_resource_loaded_p(r) || db_resource_stored_p(r);
}

static void db_load_resource(string rname, string oname, db_resource r)
{
  pips_debug(7, "loading %s of %s\n", rname, oname);
  pips_assert("resource stored", db_resource_stored_p(r));
  db_resource_pointer(r) = dbll_load_resource(rname, oname);
  db_status_tag(db_resource_db_status(r)) = is_db_status_loaded;
  if (displayable_file_p(rname))
  {
    int its_time = dbll_stat_local_file(db_resource_pointer(r), FALSE);
    if (its_time > db_resource_file_time(r))
      /* should be an internal error? */
      pips_user_warning("internal resource %s[%s] updated!\n", rname, oname);
  }
  else
  {
    int its_time = dbll_stat_resource_file(rname, oname, TRUE);
    if (its_time > db_resource_file_time(r))
      pips_user_warning("file resource %s[%s] updated!\n", rname, oname);
  }
}

int db_time_of_resource(string rname, string oname)
{
    db_resource r = get_db_resource(rname, oname);
    if (db_resource_undefined_p(r) || db_resource_required_p(r))
	return -1;

    /* we load the resource if it is a simple file name...
     * so as to be able to check it next.
     */
    if (db_resource_stored_p(r) && displayable_file_p(rname)) {
	db_load_resource(rname, oname, r); /* does it unlink the file? */
    }

    /* loaded */
    if (db_resource_loaded_p(r) && 
	displayable_file_p(rname) &&
	dbll_database_managed_file_p(db_resource_pointer(r)))
    {
	/* check time of actual resource... 
	 * note that source files are skipped!
	 */
	int current_time = dbll_stat_local_file(db_resource_pointer(r), FALSE);
	if (current_time > db_resource_file_time(r)) 
	{
	    pips_user_warning("file '%s' has been edited (%d -> %d)...\n", 
			      db_resource_pointer(r),
			      db_resource_file_time(r), current_time);
	    db_resource_file_time(r) = current_time;

	    db_inc_logical_time();
	    db_resource_time(r) = db_get_logical_time();
	    db_inc_logical_time();
	}
    }

    return db_resource_time(r);
}

static void db_save_resource(string rname, string oname, db_resource r)
{
    pips_debug(7, "saving %s of %s\n", rname, oname);
    pips_assert("resource loaded", db_resource_loaded_p(r));
    if (!dbll_storable_p(rname))
      pips_internal_error("cannot store %s\n",rname);
    dbll_save_resource(rname, oname, db_resource_pointer(r));
    db_status_tag(db_resource_db_status(r)) = is_db_status_stored;

    /* let us set the file time if appropriate... */
    if (!displayable_file_p(rname))
      db_resource_file_time(r) = dbll_stat_resource_file(rname, oname, TRUE);
}

static void db_save_and_free_resource(
    string rname, string oname, db_resource r)
{
    pips_debug(7, "saving and freeing %s of %s\n", rname, oname);
    pips_assert("resource loaded", db_resource_loaded_p(r));
    if (dbll_storable_p(rname)) {
        dbll_save_and_free_resource(rname, oname, db_resource_pointer(r));
        db_resource_pointer(r) = string_undefined;
	db_status_tag(db_resource_db_status(r)) = is_db_status_stored;
	db_resource_file_time(r) = dbll_stat_resource_file(rname, oname, TRUE);
    } else { /* lost.. delete resource. */
        dbll_free_resource(rname, oname, db_resource_pointer(r));
        db_resource_pointer(r) = string_undefined;
	db_delete_resource(rname, oname);
    }
}

/* some way to identify a resource... count be an id...
 */
string db_get_resource_id(string rname, string oname)
{
    return (char*) get_real_db_resource(rname, oname);
}

/* Return the pointer to the resource, whatever it is.
 * Assert that the resource is available.
 * If pure is false, then the resource is saved on disk before being returned.
 */
string db_get_memory_resource(string rname, string oname, bool pure)
{
    db_resource r;
    char * result;
    DB_OK;

    debug_on("PIPSDBM_DEBUG_LEVEL");
    pips_debug(2, "getting %s of %s (%d)\n", rname, oname, pure);

    r = get_db_resource(rname, oname);
    debug_db_resource(9, rname, oname, r);
    if (db_resource_undefined_p(r) || db_resource_required_p(r))
	pips_internal_error("requested resource %s for %s not available\n", 
			    rname, oname);
    /* else we have something. */

    if (db_resource_stored_p(r))
	db_load_resource(rname, oname, r); /* does it unlink the file? */

    result = db_resource_pointer(r);
    
    if (!pure && dbll_storable_p(rname))
	db_save_resource(rname, oname, r); /* the pointer is still there... */

    if (!pure)
    {
	if (dbll_storable_p(rname)) {
            /* make as stored now... */
	    db_resource_pointer(r) = string_undefined;
	    db_status_tag(db_resource_db_status(r)) = is_db_status_stored;
	} else /* lost */
	    db_delete_resource(rname, oname);
    }

    ifdebug(7) pips_assert("resource is consistent", 
			   dbll_check_resource(rname, oname, result));
    debug_off();
    return result;
}

void db_set_resource_as_required(string rname, string oname)
{
  db_resource r;
  db_status s;
  DB_OK;
  r = find_or_create_db_resource(rname, oname);
  s = db_resource_db_status(r);
  if (db_status_undefined_p(s))
    /* newly created db_resource... */
    db_resource_db_status(r) = make_db_status(is_db_status_required, UU);
  else if (db_status_loaded_p(s) && 
	   !string_undefined_p(db_resource_pointer(r)))
  {
    dbll_free_resource(rname, oname, db_resource_pointer(r));
    db_resource_pointer(r) = string_undefined;
  }

  db_status_tag(db_resource_db_status(r)) = is_db_status_required;
  db_resource_time(r) = db_get_logical_time();
}

void db_put_or_update_memory_resource(
    string rname, string oname, char * p, bool update_is_ok)
{
    db_resource r;
    DB_OK;

    debug_on("PIPSDBM_DEBUG_LEVEL");
    pips_debug(2, "putting or updating %s of %s\n", rname, oname);
    ifdebug(7) pips_assert("resource is consistent", 
			   dbll_check_resource(rname, oname, p));

    r = find_or_create_db_resource(rname, oname);
    if (db_status_undefined_p(db_resource_db_status(r)))
	/* was just created */
	db_resource_db_status(r) = make_db_status(is_db_status_loaded, UU);
    else
    {
	if (!update_is_ok && !db_resource_required_p(r))
	    pips_internal_error("resource %s of %s already there\n", 
				rname, oname);
    }
    
    /* store data */
    db_resource_pointer(r) = p; /** ??? memory leak */
    db_status_tag(db_resource_db_status(r)) = is_db_status_loaded;
    db_resource_time(r) = db_get_logical_time();

    if (displayable_file_p(rname))
	db_resource_file_time(r) =
	    dbll_stat_local_file(db_resource_pointer(r), FALSE);

    debug_db_resource(9, rname, oname, r);
    debug_off();
}

void db_unput_resources(string rname)
{
    db_symbol r;
    DB_OK;
    r = find_or_create_db_symbol(rname);
    DB_RESOURCES_MAP(s, or,
    {
	pips_debug(7, "deleting %s of %s if any\n", rname, db_symbol_name(s));
	if (bound_db_owned_resources_p(or, r)) {
	    db_delete_resource(rname, db_symbol_name(s));
	    dbll_unlink_resource_file(rname, db_symbol_name(s), FALSE);
	}
    },
        get_pips_database());
}

void db_save_and_free_memory_resource_if_any(string rname, string oname)
{
    db_resource r;
    DB_OK;
    r = get_db_resource(rname, oname);
    if (!db_resource_undefined_p(r) && db_resource_loaded_p(r))
	db_save_and_free_resource(rname, oname, r);
}

/* FC: I added this function to clean all resources, hence avoiding
 * to save them. This speed up hpfc at low cost;-).
 */
void db_delete_all_resources(void)
{
    int nr = dbll_number_of_resources(), i;
    DB_OK;
    for (i=0; i<nr; i++)
      db_unput_resources(dbll_get_ith_resource_name(i));
}

/******************************************************************* MODULES */

/* when telling the database about a module name, the module is 
 * registered as a db_symbol, and it is added to the database.
 */
static string current_module_name = NULL;

bool db_set_current_module_name(string name)
{
    bool ok = FALSE;
    DB_OK; pips_assert("no current module", !current_module_name);
    if (simple_name_p(name)) {
	current_module_name = strdup(name); 
	init_owned_resources_if_necessary(name);
	ok = TRUE;
    } else /* can be rejected softly */
	pips_user_warning("invalid module name %s\n", name);
    return ok;
}

/* Also used to check whether set... so no asserts, even DB_OK. */
string db_get_current_module_name(void)
{
    return current_module_name;
}

void db_reset_current_module_name(void)
{
    DB_OK; pips_assert("some current module name", current_module_name);
    free(current_module_name), current_module_name = NULL;
}

/***************************************************************** CLEANING */

/* delete all obsolete resources before a close.
 * return the number of resources destroyed.
 */
int db_delete_obsolete_resources(bool (*keep_p)(string, string))
{
    int ndeleted = 0;
    list /* of string */ lr = NIL, lo = NIL, lrp, lop;
    DB_OK;
    debug_on("PIPSDBM_DEBUG_LEVEL");

    /* builds the lists to delete. */
    DB_RESOURCES_MAP(os, or,
    {
      DB_OWNED_RESOURCES_MAP(rs, r,
      {
	string rn = db_symbol_name(rs);
	string on = db_symbol_name(os);
	pips_debug(8, "considering %s of %s (%p)\n", rn, on, (char*) r);
	if (!keep_p(rn, on)) {
	  pips_debug(8, "to be destroyed: %s of %s\n", rn, on);
	  ndeleted++;
	  lr = CONS(STRING, rn, lr);
	  lo = CONS(STRING, on, lo);
	}
      },
			     or);
    },
		     get_pips_database());

    /* delete the resources. */
    for(lrp=lr, lop=lo; !ENDP(lrp); POP(lrp), POP(lop)) {
	string rname = STRING(CAR(lrp)), oname = STRING(CAR(lop));
	db_delete_resource(rname, oname);
	dbll_unlink_resource_file(rname, oname, FALSE);
    }

    gen_free_list(lr);
    gen_free_list(lo);
    debug_off();
    return ndeleted;
}

/* returns whether name is a valid module.
 */
bool db_module_exists_p(string name)
{
  if (pips_database_undefined_p()) 
    return FALSE;

  DB_RESOURCES_MAP(os, or, 
  {
    ifdebug(9) {
      pips_assert("consistant stuff", 
		  db_owned_resources_consistent_p(or));
    }
    if (same_string_p(db_symbol_name(os), name)) 
      return TRUE;
  },
		   get_pips_database());

  return FALSE;
}

gen_array_t db_get_module_list_initial_order(void)
{
  list /* of db_symbol */ ls;
  db_resources dbr;
  gen_array_t a;

  DB_OK;

  /* the list returned is reversed... */
  ls = gen_filter_tabulated(gen_true, db_symbol_domain);
  a = gen_array_make(0);
  dbr = get_pips_database();
  
  MAP(DB_SYMBOL, symbol, 
  {
    string name = db_symbol_name(symbol);

    /* if it is a module, append... */
    if (!string_undefined_p(name) &&
	!same_string_p(name, "") && 
	bound_db_resources_p(dbr, symbol))
      gen_array_dupappend(a, name);
  },
      ls);

  gen_free_list(ls);
  return a;  
}

/* returns an allocated array a with the sorted list of modules. 
 * strings are duplicated.
 */
gen_array_t db_get_module_list(void)
{
    gen_array_t a = gen_array_make(0);
    DB_OK;

    DB_RESOURCES_MAP(os, or, 
    {     
	string on = db_symbol_name(os);
	pips_assert("some symbol name", on);
	pips_debug(9, "considering %s -> %p\n", on, or);
	if (!same_string_p(on, ""))
	    gen_array_dupappend(a, on);
    },
	get_pips_database());

    gen_array_sort(a);
    return a;
}
