/* Fabien Coelho, May 1993
 *
 * $RCSfile: compiler.c,v $ ($Date: 1995/08/01 10:06:09 $, )
 * version $Revision$
 *
 * Compiler
 *
 * stat is the current statement to be compiled, and there are
 * pointers to the current statement building of the node and host
 * codes. the module of these are also kept in order to add
 * the needed declarations generated by the compilation.
 *
 * however, every entities of the compiled program, and of
 * both generated programs will be mixed, due to the
 * tabulated nature of these objects. 
 * some objects will be shared.
 * I don't think this is a problem.
 */

#include "defines-local.h"

#include "control.h"     /* for CONTROL_MAP() */

/* global variables
 */
entity host_module, node_module;

#define debug_print_control(c)\
  fprintf(stderr, \
	  "control 0x%x (stat 0x%x) , %d predecessors, %d successors\n", \
          (unsigned int) c, (unsigned int) control_statement(c), \
	  gen_length(control_predecessors(c)), \
	  gen_length(control_successors(c))); \
  print_statement(control_statement(c));

static void hpf_compile_block(stat,hoststatp,nodestatp)
statement stat;
statement *hoststatp,*nodestatp;
{
    list /* of statements */ lhost=NIL, lnode=NIL;
    statement hostcd, nodecd;

    assert(instruction_block_p(statement_instruction(stat)));

    (*hoststatp) = MakeStatementLike(stat, is_instruction_block);
    (*nodestatp) = MakeStatementLike(stat, is_instruction_block);

    MAP(STATEMENT, s,
     {
	 hpf_compiler(s,&hostcd,&nodecd);
	 
	 lhost = CONS(STATEMENT, hostcd, lhost);
	 lnode = CONS(STATEMENT, nodecd, lnode);
     },
	 instruction_block(statement_instruction(stat)));

    instruction_block(statement_instruction(*hoststatp)) = gen_nreverse(lhost);
    instruction_block(statement_instruction(*nodestatp)) = gen_nreverse(lnode);
}

static void hpf_compile_test(s,hoststatp,nodestatp)
statement s;
statement *hoststatp,*nodestatp;
{
    statement
	s_true,	s_hosttrue, s_nodetrue,
	s_false, s_hostfalse, s_nodefalse;
    test the_test;
    expression condition;
    
    assert(instruction_test_p(statement_instruction(s)));

    the_test = instruction_test(statement_instruction(s));
    condition = test_condition(the_test);
    
    (*hoststatp) = MakeStatementLike(s, is_instruction_test);
    (*nodestatp) = MakeStatementLike(s, is_instruction_test);

    /* if it may happen that a condition modifies the value
     * of a distributed variable, this condition is to be
     * put out of the statement, for separate compilation.
     */

    s_true = test_true(the_test);
    s_false = test_false(the_test);

    hpf_compiler(s_true, &s_hosttrue, &s_nodetrue);
    hpf_compiler(s_false, &s_hostfalse, &s_nodefalse);

    instruction_test(statement_instruction(*hoststatp)) =
	make_test(UpdateExpressionForModule(host_module, condition),
		  s_hosttrue,
		  s_hostfalse);

    instruction_test(statement_instruction(*nodestatp))=
	make_test(UpdateExpressionForModule(node_module, condition),
		  s_nodetrue,
		  s_nodefalse);

    DEBUG_STAT(9, entity_name(host_module), *hoststatp);
    DEBUG_STAT(9, entity_name(node_module), *nodestatp);
}

static void hpf_compile_call(stat, hoststatp, nodestatp)
statement stat;
statement *hoststatp,*nodestatp;
{
    call c = instruction_call(statement_instruction(stat));

    assert(instruction_call_p(statement_instruction(stat)));

    debug(7,"hpf_compile_call", "function %s\n", entity_name(call_function(c)));

    /* IO functions should be detected earlier, in hpf_compiler
     */
    assert(!IO_CALL_P(c));

    /* no reference to distributed arrays...
     * the call is just translated into local objects.
     */
    if (!ref_to_dist_array_p(c))
    {
	list /* of expressions */
	    leh=lUpdateExpr(host_module, call_arguments(c)),
	    len=lUpdateExpr(node_module, call_arguments(c));
	
	debug(7,"hpf_compile_call","no reference to distributed variable\n");

	(*hoststatp)=MakeStatementLike(stat, is_instruction_call);
	(*nodestatp)=MakeStatementLike(stat, is_instruction_call);
	
	instruction_call(statement_instruction((*hoststatp)))=
	    make_call(call_function(c), leh);

	instruction_call(statement_instruction((*nodestatp)))=
	    make_call(call_function(c), len);

	DEBUG_STAT(8, entity_name(host_module), *hoststatp);
	DEBUG_STAT(8, entity_name(node_module), *nodestatp);

	return;
    }

    /* should consider read and written variables
     */    
    if (ENTITY_ASSIGN_P(call_function(c)))
    {
	list /* of expressions */
	    lh = NIL, ln = NIL, args = call_arguments(c) ;
	expression 
	    w = EXPRESSION(CAR(args)),
	    r = EXPRESSION(CAR(CDR(args)));

	assert(syntax_reference_p(expression_syntax(w)));

	if (array_distributed_p
	    (reference_variable(syntax_reference(expression_syntax(w)))))
	{
	    debug(8,"hpf_compile_call","c1-alpha\n");
	    
	    generate_c1_alpha(stat, &lh, &ln); /* C1-ALPHA */
	}
	else
	{
	    syntax s = expression_syntax(r);

	    /* reductions are detected here. They are not handled otherwise
	     */
	    if (syntax_call_p(s) && call_reduction_p(syntax_call(s)))
	    {
		statement
		    sh = statement_undefined,
		    sn = statement_undefined;

		if (!compile_reduction(stat, &sh, &sn))
		    pips_error("hpf_compile_call", 
			       "reduction compilation failed\n");

		lh = CONS(STATEMENT, sh, NIL);
		ln = CONS(STATEMENT, sn, NIL);
	    }
	    else
	    {
		debug(8,"hpf_compile_call","c1-beta\n");
		
		generate_c1_beta(stat, &lh, &ln); /* C1-BETA */
	    }
	}

	(*hoststatp) = MakeStatementLike(stat, is_instruction_block);
	(*nodestatp) = MakeStatementLike(stat, is_instruction_block);
	
	instruction_block(statement_instruction(*hoststatp)) = lh;
	instruction_block(statement_instruction(*nodestatp)) = ln;
	
	DEBUG_STAT(8, entity_name(host_module), *hoststatp);
	DEBUG_STAT(8, entity_name(node_module), *nodestatp);

	return;
    }

    /* call to something with distributed variables, which is not an
     * assignment. Since I do not use the effects as I should, nothing is
     * done...
     */
    hpfc_warning("hpf_compile_call", "not implemented yet\n");
}

static void hpf_compile_unstructured(stat,hoststatp,nodestatp)
statement stat;
statement *hoststatp,*nodestatp;
{
    instruction inst=statement_instruction(stat);

    assert(instruction_unstructured_p(inst));

    if (one_statement_unstructured(instruction_unstructured(inst)))
    {
	debug(7,"hpf_compile_unstructured","one statement recognize\n");
	/* 
	 * nothing spacial is done! 
	 *
	 * ??? there may be a problem with the label of the statement, if any.
	 */
	hpf_compiler
	    (control_statement
	     (unstructured_control(instruction_unstructured(inst))),
	     hoststatp, nodestatp);
    }
    else
    {
	control_mapping 
	    hostmap = MAKE_CONTROL_MAPPING(),
	    nodemap = MAKE_CONTROL_MAPPING();
	unstructured 
	    u = instruction_unstructured(inst);
	control 
	    ct = unstructured_control(u),
	    ce = unstructured_exit(u),
	    new_ct, new_ce, nodec, hostc;
	statement statc, stath, statn;
	list blocks = NIL;

	debug(6, "hpf_compile_unstructured", "beginning\n");

	CONTROL_MAP
	    (c,
	 {
	     statc=control_statement(c);
	     
	     hpf_compiler(statc, &stath, &statn);
	     
	     DEBUG_STAT(7, "statc", statc);
	     DEBUG_STAT(7, "host stat", stath);
	     DEBUG_STAT(7, "node stat", statn);
	     
	     hostc = make_control(stath, NIL, NIL);
	     SET_CONTROL_MAPPING(hostmap, c, hostc);
	     
	     nodec = make_control(statn, NIL, NIL);
	     SET_CONTROL_MAPPING(nodemap, c, nodec);
	 },
	     ct,
	     blocks);
	
	MAP(CONTROL, c,
	 {
	     update_control_lists(c, hostmap);
	     update_control_lists(c, nodemap);
	 },
	    blocks);

	ifdebug(9)
	{
	    control h_tmp, n_tmp;

	    fprintf(stderr, "[hpf_compile_unstructured] controls:\n");
	    
	    MAP(CONTROL, c_tmp,
	     {
		 h_tmp = (control) GET_CONTROL_MAPPING(hostmap, c_tmp);
		 n_tmp = (control) GET_CONTROL_MAPPING(nodemap, c_tmp);
		 
		 fprintf(stderr, "\nOriginal:\n");
		 debug_print_control(c_tmp);
		 fprintf(stderr, "Host:\n");
		 debug_print_control(h_tmp);
		 fprintf(stderr, "Node:\n");
		 debug_print_control(n_tmp);
	     },
		 blocks);
	}

	/*    HOST statement
	 */
	(*hoststatp) = MakeStatementLike(stat, is_instruction_unstructured);

	new_ct = (control) GET_CONTROL_MAPPING(hostmap, ct);
	new_ce = (control) GET_CONTROL_MAPPING(hostmap, ce);

	assert(!control_undefined_p(new_ct) && !control_undefined_p(new_ce));

	ifdebug(9)
	{
	    fprintf(stderr,
		    "[hpf_compile_unstructured] host unstructured controls:\n");
	    
		 fprintf(stderr, "Main:\n");
		 debug_print_control(new_ct);
		 fprintf(stderr, "Exit:\n");
		 debug_print_control(new_ce);
	}

	instruction_unstructured(statement_instruction(*hoststatp)) =
	    make_unstructured(new_ct, new_ce);

	DEBUG_STAT(7, "host new stat", *hoststatp);

	/*    NODE statement
	 */
	(*nodestatp) = MakeStatementLike(stat, is_instruction_unstructured);

	new_ct = (control) GET_CONTROL_MAPPING(nodemap, ct);
	new_ce = (control) GET_CONTROL_MAPPING(nodemap, ce);

	assert(!control_undefined_p(new_ct) && !control_undefined_p(new_ce));

	instruction_unstructured(statement_instruction(*nodestatp)) =
	    make_unstructured(new_ct, new_ce);

	DEBUG_STAT(7, "host new stat (again)", *hoststatp);
	
	gen_free_list(blocks);
	FREE_CONTROL_MAPPING(hostmap);
	FREE_CONTROL_MAPPING(nodemap);
    }
}

static void hpf_compile_sequential_loop(stat,hoststatp,nodestatp)
statement stat, *hoststatp, *nodestatp;
{
    loop the_loop=statement_loop(stat);
    statement body = loop_body(the_loop), hostbody, nodebody;
    range r=loop_range(the_loop);
    list /* of entities */ locals=loop_locals(the_loop);
    entity
	label = loop_label(the_loop),
	index = loop_index(the_loop),
	nindex = NewVariableForModule(node_module, index),
	hindex = NewVariableForModule(host_module, index);
    expression
	lower = range_lower(r),
	upper = range_upper(r),
	increment = range_increment(r);
    
    hpf_compiler(body, &hostbody, &nodebody);
    
    if (hpfc_empty_statement_p(hostbody))
    {
	/* ??? memory leak, hostbody is lost whatever it was.
	 */
	(*hoststatp)=make_continue_statement(entity_undefined);
    }
    else
    {
	(*hoststatp)=MakeStatementLike(stat, is_instruction_loop);

	instruction_loop(statement_instruction(*hoststatp))=
	  make_loop(hindex,
		    make_range(UpdateExpressionForModule(host_module,lower),
			       UpdateExpressionForModule(host_module,upper),
			    UpdateExpressionForModule(host_module,increment)),
		    hostbody,
		    label,
		    make_execution(is_execution_sequential,UU),
		    lNewVariableForModule(host_module,locals));
    }

    DEBUG_STAT(8, "host stat", *hoststatp);

    (*nodestatp)=MakeStatementLike(stat, is_instruction_loop);

    instruction_loop(statement_instruction(*nodestatp))=
	make_loop(nindex,
		  make_range(UpdateExpressionForModule(node_module,lower),
			     UpdateExpressionForModule(node_module,upper),
			     UpdateExpressionForModule(node_module,increment)),
		  nodebody,
		  label,
		  make_execution(is_execution_sequential,UU),
		  lNewVariableForModule(node_module,locals));

    DEBUG_STAT(8, "node stat", *nodestatp);
}

static void hpf_compile_parallel_body(body, hoststatp, nodestatp)
statement body, *hoststatp, *nodestatp;
{
    list lw = NIL, lr = NIL, li = NIL, ls = NIL, lbs = NIL;

    /* ???
     * dependances are not surely respected respected in the definitions list...
     * should check that only locals variables, that are not replicated,
     * may be defined during the body of the loop...
     */
    FindRefToDistArrayInStatement(body, &lw, &lr);
    li = AddOnceToIndicesList(lIndicesOfRef(lw), lIndicesOfRef(lr));
    ls = FindDefinitionsOf(body, li);
    gen_free_list(li), li=NIL;

    if (gen_length(lw)==0) /* very partial */
    {
	message_assert("parallel body with only read distributed arrays",
		       gen_length(lr)==0);

	(*hoststatp) = copy_statement(body);
	(*nodestatp) = copy_statement(body);
    }
    else
    {
	generate_parallel_body(body, &lbs, lw, lr);
	
	(*hoststatp) = NULL;
	(*nodestatp) = make_block_statement(gen_nconc(ls, lbs));
    }

    gen_free_list(lw), lw=NIL;
    gen_free_list(lr), lr=NIL;
}

static void hpf_compile_parallel_loop(stat, hoststatp, nodestatp)
statement stat, *hoststatp, *nodestatp;
{
    loop the_loop = statement_loop(stat);
    statement s, nodebody, body = loop_body(the_loop);
    instruction bodyinst = statement_instruction(body);
    entity
	label = loop_label(the_loop),
	index = loop_index(the_loop),
	nindex = NewVariableForModule(node_module,index);
    range r = loop_range(the_loop);
    expression
	lower = range_lower(r),
	upper = range_upper(r),
	increment = range_increment(r);
    
    assert(execution_parallel_p(loop_execution(the_loop)));

    if ((instruction_loop_p(bodyinst)) &&
	(execution_parallel_p(loop_execution(instruction_loop(bodyinst)))))
	hpf_compile_parallel_loop(body, &s, &nodebody);
    else
	hpf_compile_parallel_body(body, &s, &nodebody);
    
    (*hoststatp) = make_continue_statement(entity_undefined);
    (*nodestatp) = MakeStatementLike(stat, is_instruction_loop);

    instruction_loop(statement_instruction(*nodestatp))=
	make_loop(nindex,
		  make_range(UpdateExpressionForModule(node_module,lower),
			     UpdateExpressionForModule(node_module,upper),
			     UpdateExpressionForModule(node_module,increment)),
		  nodebody,
		  label,
		  make_execution(is_execution_sequential,UU),
		  NULL);
}

static void hpf_compile_loop(stat, hoststatp, nodestatp)
statement stat;
statement *hoststatp, *nodestatp;
{
    loop the_loop = instruction_loop(statement_instruction(stat));

    assert(statement_loop_p(stat));

    if (execution_parallel_p(loop_execution(the_loop)))
    {
	entity var=entity_undefined;
	list l=NIL;
	bool
	    is_sh = subarray_shift_p(stat, &var, &l),
	    /* 
	     * should verify that only listed in labels and distributed
	     * entities are defined inside the body of the loop
	     */
	    at_ac = atomic_accesses_only_p(stat),
	    in_in = indirections_inside_statement_p(stat);
	
	debug(5, "hpf_compile_loop", "condition results: sh %d, aa %d, in %d\n",
	      is_sh, at_ac, in_in);

	if (is_sh)
	{
	    debug(4, "hpf_compile_loop", "shift detected\n");
	    
	    *nodestatp = generate_subarray_shift(stat, var, l);
	    *hoststatp = make_continue_statement(entity_empty_label());
	}
	else if (at_ac && !in_in)
	{
	    statement
		overlapstat;

	    debug(7, "hpf_compile_loop", "compiling a parallel loop\n");

	    if (Overlap_Analysis(stat, &overlapstat))
	    {
		debug(7, "hpf_compile_loop", "overlap analysis succeeded\n");

		*hoststatp = make_continue_statement(entity_empty_label());
		*nodestatp = overlapstat;
		statement_comments(*nodestatp) = statement_comments(stat);
	    }
	    else
	    {
		debug(7, "hpf_compile_loop", "overlap analysis is not ok...\n");

		hpf_compile_parallel_loop(stat, hoststatp, nodestatp);
	    }
	}
	else
	{
	    debug(7,"hpf_compile_loop",
		  "compiling a parallel loop sequential...\n");
	    hpf_compile_sequential_loop(stat, hoststatp, nodestatp);
	}
    }
    else
    {
	debug(7,"hpf_compile_loop","compiling a sequential loop\n");
    
	hpf_compile_sequential_loop(stat, hoststatp, nodestatp);
    }
}

/* void hpf_compiler(stat, hoststatp, nodestatp)
 * statement stat, *hoststatp, *nodestatp;
 *
 * what: compile a statement intoi a host and SPMD node code.
 * how: double code rewriting in a recursive traversal of stat.
 * input: statement stat.
 * output: statements *hoststatp and *nodestatp
 * side effects: ?
 * bugs or features:
 *  - special care is made here of I/O and remappings.
 */
void hpf_compiler(stat, hoststatp, nodestatp)
statement stat, *hoststatp, *nodestatp;
{
    if (load_statement_only_io(stat)==TRUE) /* necessary */
    {
	io_efficient_compile(stat,  hoststatp, nodestatp);
	return;
    }
    else if (bound_renamings_p(stat)) /* remapping */
    {
	remapping_compile(stat, hoststatp, nodestatp);
	return;
    }
    
    /* else usual stuff 
     */
    switch(instruction_tag(statement_instruction(stat)))
    {
    case is_instruction_block:
	hpf_compile_block(stat, hoststatp, nodestatp);
	break;
    case is_instruction_test:
	hpf_compile_test(stat, hoststatp, nodestatp);
	break;
    case is_instruction_loop:
	hpf_compile_loop(stat, hoststatp, nodestatp);
	break;
    case is_instruction_call:
	hpf_compile_call(stat, hoststatp, nodestatp);
	break;
    case is_instruction_unstructured:
	hpf_compile_unstructured(stat, hoststatp, nodestatp);
	break;
    case is_instruction_goto:
    default:
	pips_error("hpf_compiler", "unexpected instruction tag\n");
	break;
    }
}

/*   That is all
 */
