#include <stdio.h>
#include <strings.h>
#include <string.h>

#include "genC.h"
#include "ri.h"
#include "database.h"

#include "ri-util.h"
#include "control.h"
#include "pipsdbm.h"

#include "resources.h"
#include "constants.h"

/* interface with pipsdbm and pipsmake */

void controlizer(module_name)
string module_name;
{
   
    statement module_stat;
    
    module_stat = 
	    copy_statement((statement)
			   db_get_memory_resource(DBR_PARSED_CODE, 
						  module_name, 
						  TRUE)) ;

    /* *module_stat can be re-used because control_graph reallocates
       statements; do not show that to any student! */
    statement_instruction(module_stat) =
	make_instruction(is_instruction_unstructured,
			 control_graph(module_stat));
    statement_ordering(module_stat) = MAKE_ORDERING(0,1) ;

    DB_PUT_MEMORY_RESOURCE(DBR_CODE, 
			   strdup(module_name), 
			   module_stat);
}







