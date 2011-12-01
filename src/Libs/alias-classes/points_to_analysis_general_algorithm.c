/*

  $Id: scalarization.c 14503 2009-07-10 15:11:52Z mensi $

  Copyright 1989-2009 MINES ParisTech

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
/*
This file is used to compute points-to sets intraprocedurally.
At first we get SUMMARY_POINTS_TO, then we use it as input set for the
function points_to_statement().
The function points_to_statement() calls
points_to_recursive_statement(). At the level of
points_to_recursive_statement() we dispatch according to the
instruction's type : if it's a test we call points_to_test(), if it's
a sequence we call points_to_sequence() ...
When we the instruction is  call we call points_to_call() and
according to the nature of the call we affect the appropriate
treatment.
If the call is an intrinsics, preciselly the operator "=", we call
points_to_assignment(). This latter dispatch the treatment according
to the nature of the left hand side and the right hand side.
To summarize this is a call graph simulating the points-to analysis :

points_to_statement
      |
      |-->points_to_recursive_statement
                 |
		 |-->points_to_call------>points_to_intrinsics
		 |-->points_to_sequence       |
		 |-->points_to_while          |-->points_to_expression
		 |-->points_to_expression     |-->points_to_general_assignment
		 |...                         |-->points_to_filter_with_effects
                                              |-->points_to_assignment
					           |
						   |->basic_ref_ref
						   |->basic_ref_addr
						   |->basic_ref_deref
						   |...



*/
#include <stdlib.h>
#include <stdio.h>
#include "genC.h"
#include "linear.h"
#include "ri.h"
#include "effects.h"
#include "database.h"
#include "ri-util.h"
#include "effects-util.h"
#include "control.h"
#include "constants.h"
#include "misc.h"
#include "parser_private.h"
#include "syntax.h"
#include "top-level.h"
#include "text-util.h"
#include "text.h"
#include "properties.h"
#include "pipsmake.h"
#include "semantics.h"
#include "effects-generic.h"
#include "effects-simple.h"
#include "effects-convex.h"
#include "transformations.h"
#include "preprocessor.h"
#include "pipsdbm.h"
#include "resources.h"
#include "prettyprint.h"
#include "newgen_set.h"
#include "points_to_private.h"
#include "alias-classes.h"


/* Static sets containing poits-to relations computed before the function's call.
   points_to_in contains only formal parameters and gloabl/static variables.
   points_to_out contains only formal parameters, global/static  and heap allocated variables
*/
static set pts_to_in ;
static set pts_to_out; 


/* Same as previous function, but for double pointers. */
bool expression_double_pointer_p(expression e) {
  bool double_pointer_p = false;
  type et = expression_to_type(e);
  type t = ultimate_type(et);

  if (pointer_type_p(t)) {
    type pt = basic_pointer(variable_basic(type_variable(t)));
    double_pointer_p = pointer_type_p(pt);
  }

  return double_pointer_p;
}

/* given an expression, return the referenced entity in case exp is a
   reference */
entity argument_entity(expression exp) {
  syntax syn = expression_syntax(exp);
  entity var = entity_undefined;

  if (syntax_reference_p(syn)) {
    reference ref = syntax_reference(syn);
    var = reference_variable(ref);
  }

  return copy_entity(var);
}


/* storing the points to associate to a statement s, in the case of
   loops the field store is set to false to prevent from key
   redefenition's */
void points_to_storage(set pts_to_set, statement s, bool store) {
  list pt_list = NIL;
  statement_consistent_p(s);
  if(statement_unstructured_p(s) || statement_goto_p(s)){
    pt_list = set_to_sorted_list(pts_to_set,
				 (int(*)(const void*, const void*))
				 points_to_compare_cells);
    points_to_list new_pt_list = make_points_to_list(pt_list);
    store_or_update_pt_to_list(s, new_pt_list);
  }
  else if (!set_empty_p(pts_to_set) && store == true) {
    instruction i = statement_instruction(s);
    if(instruction_sequence_p(i)){
      sequence seq = instruction_sequence(i);
	pt_list = set_to_sorted_list(pts_to_set,
				     (int(*)(const void*, const void*))
				     points_to_compare_cells);
	points_to_list new_pt_list = make_points_to_list(pt_list);
	points_to_list_consistent_p(new_pt_list);
	store_or_update_pt_to_list(s, new_pt_list);
      FOREACH(statement, stm, sequence_statements(seq)){
	pt_list = set_to_sorted_list(pts_to_set,
				     (int(*)(const void*, const void*))
				     points_to_compare_cells);
	points_to_list new_pt_list = make_points_to_list(pt_list);
	points_to_list_consistent_p(new_pt_list);
	store_or_update_pt_to_list(stm, new_pt_list);
      }
    }
    else{
    pt_list = set_to_sorted_list(pts_to_set,
				 (int(*)(const void*, const void*))
				 points_to_compare_cells);
    points_to_list new_pt_list = make_points_to_list(pt_list);
    store_or_update_pt_to_list(s, new_pt_list);
    }
  }
  else if(set_empty_p(pts_to_set)){
    points_to_list new_pt_list = make_points_to_list(pt_list);
    store_or_update_pt_to_list(s, new_pt_list);
  }
}


/* to compute m.a = n.a where a is of pointer type*/
set struct_double_pointer(set pts_to_set, expression lhs, expression rhs) {
  set gen_pts_to = set_generic_make(set_private, points_to_equal_p,
				    points_to_rank);
  set written_pts_to = set_generic_make(set_private, points_to_equal_p,
					points_to_rank);
  effect e1 = effect_undefined, e2 = effect_undefined;
  reference r1 = reference_undefined;
  reference r2 = reference_undefined;

  if (expression_reference_p(lhs) && expression_reference_p(rhs)) {
    list l_ef = NIL;
    /* init the effect's engine. */
    set_methods_for_proper_simple_effects();
    list l1 = generic_proper_effects_of_complex_address_expression(lhs,
								   &l_ef, true);
    e1 = EFFECT(CAR(l_ef)); /* In fact, there should be a FOREACH to scan all elements of l_ef */
    gen_free_list(l_ef); /* free the spine */
    l_ef = NIL;
    list l2 = generic_proper_effects_of_complex_address_expression(rhs,
								   &l_ef, false);
    e2 = EFFECT(CAR(l_ef)); /* In fact, there should be a FOREACH to scan all elements of l_ef */
    gen_free_list(l_ef); /* free the spine */

    effects_free(l1);
    effects_free(l2);
    generic_effects_reset_all_methods();
    r1= effect_any_reference(e1);
    cell source = make_cell_reference(r1);
    r2 = effect_any_reference(e2);
    cell sink = make_cell_reference(r2);
    set s = set_generic_make(set_private, points_to_equal_p,
			   points_to_rank);

    SET_FOREACH(points_to, i, pts_to_set){
	if(locations_equal_p(points_to_source(i), sink))
	  s = set_add_element(s, s, (void*)i);
      }
    
    SET_FOREACH(points_to, j, s){
	cell new_sink = copy_cell(points_to_sink(j));
	approximation rel = points_to_approximation(j);
	points_to pt_to = make_points_to(source,
					 new_sink,
					 rel,
					 make_descriptor_none());
	gen_pts_to = set_add_element(gen_pts_to,gen_pts_to,
				     (void*) pt_to );
      }
    
    SET_FOREACH(points_to, k, pts_to_set){
      if(locations_equal_p(points_to_source(k), source))
	written_pts_to = set_add_element(written_pts_to,
					 written_pts_to, (void *)k);
    }

    pts_to_set = set_difference(pts_to_set, pts_to_set, written_pts_to);
    pts_to_set = set_union(pts_to_set, gen_pts_to, pts_to_set);
    ifdebug    (1)
      print_points_to_set("Points to for the case x is a double pointer to struct\n",
			  pts_to_set);
  } else
    pips_internal_error("Don't know how to handle this kind of expression");

  return pts_to_set;

}

/* to compute m.a = n.a where a is of pointer type*/
set struct_pointer(set pts_to_set, expression lhs, expression rhs) {
  set gen_pts_to = set_generic_make(set_private, points_to_equal_p,
				    points_to_rank);
  set written_pts_to = set_generic_make(set_private, points_to_equal_p,
					points_to_rank);
  effect e1 = effect_undefined, e2 = effect_undefined;
  reference r1 = reference_undefined;
  reference r2 = reference_undefined;
  list l_ef = NIL;

  /* init the effect's engine.*/
  set_methods_for_proper_simple_effects();
  list l1 = generic_proper_effects_of_complex_address_expression(lhs, &l_ef,
								 true);
  e1 = EFFECT(CAR(l_ef)); /* In fact, there should be a FOREACH to scan all elements of l_ef */
  gen_free_list(l_ef); /* free the spine */
  l_ef = NIL;
  list l2 = generic_proper_effects_of_complex_address_expression(rhs, &l_ef,
								 false);
  e2 = EFFECT(CAR(l_ef)); /* In fact, there should be a FOREACH to scan all elements of l_ef */
  gen_free_list(l_ef); /* free the spine */

  effects_free(l1);
  effects_free(l2);
  generic_effects_reset_all_methods();
  r1 = effect_any_reference(e1);
  cell source = make_cell_reference(r1);
  // add the points_to relation to the set generated
  // by this assignement
  r2 = effect_any_reference(e2);
  cell sink = make_cell_reference(r2);
  set s = set_generic_make(set_private, points_to_equal_p, points_to_rank);

  SET_FOREACH(points_to, i, pts_to_set){
      if(locations_equal_p(points_to_source(i), sink))
	s = set_add_element(s, s, (void*)i);
    }
  
  SET_FOREACH(points_to, j, s){
      cell new_sink = copy_cell(points_to_sink(j));
      approximation rel = points_to_approximation(j);
      points_to pt_to = make_points_to(source,
				       new_sink,
				       rel,
				       make_descriptor_none());

      gen_pts_to = set_add_element(gen_pts_to,gen_pts_to,
				   (void*) pt_to );
    }

  SET_FOREACH(points_to, k, pts_to_set) {
    if(locations_equal_p(points_to_source(k), source))
      written_pts_to = set_add_element(written_pts_to,
				       written_pts_to, (void *)k);
  }

  pts_to_set = set_difference(pts_to_set, pts_to_set, written_pts_to);
  pts_to_set = set_union(pts_to_set, gen_pts_to, pts_to_set);
  ifdebug(1)
    print_points_to_set("Points to for the case <x = y>\n",
			pts_to_set);

  return pts_to_set;

}

// to decompose the assignment m = n where  m and n are respectively
// of type struct
// the result should be m.field1 = n.field2... A.M
set struct_decomposition(expression lhs, expression rhs, set pt_in) {
  set pt_out = set_generic_make(set_private, points_to_equal_p,
				points_to_rank);
  entity e1 = expression_to_entity(lhs);
  entity e2 = expression_to_entity(rhs);
  type t1 = entity_type(e1);
  type t2 = entity_type(e2);
  variable v1 = type_variable(t1);
  variable v2 = type_variable(t2);
  basic b1 = variable_basic(v1);
  basic b2 = variable_basic(v2);
  entity ent1 = basic_derived(b1);
  entity ent2 = basic_derived(b2);
  type tt1 = entity_type(ent1);
  type tt2 = entity_type(ent2);
  list l1 = type_struct(tt1);
  list l2 = type_struct(tt2);

  FOREACH(ENTITY, i, l1) {
    if(expression_double_pointer_p(entity_to_expression(i))
       || expression_pointer_p(entity_to_expression(i))) {
      ent2 = ENTITY (CAR(l2));
      expression ex1 = MakeBinaryCall(entity_intrinsic(FIELD_OPERATOR_NAME),
				      lhs,
				      entity_to_expression(i));
      expression ex2 = MakeBinaryCall(entity_intrinsic(FIELD_OPERATOR_NAME),
				      rhs,
				      entity_to_expression(ent2));
      expression_consistent_p(ex1);
      expression_consistent_p(ex1);
      pt_out = set_union(pt_out, pt_out,
			 struct_pointer(pt_in,
					copy_expression(ex1),
					copy_expression(ex2)));
    }
    l2 = CDR(l2);
  }

  return pt_out;
}


/* Using pt_in, compute the new points-to set for any assignment lhs =
   rhs  that meets Emami's patterns.

   If the assignment cannot be analyzed according to Emami's rules,
   returns an empty set. So the assignment can be treated by
   points_to_general_assignment().

   To be able to apply Emami rules we have to test the lhs and the rhs:
   are they references, fields of structs, &operator...lhs and rhs can
   be one of Emami enum's fields.
*/
set points_to_assignment(statement current, expression lhs, expression rhs,
			 set in) {
  set cur = set_generic_make(set_private, points_to_equal_p,
				points_to_rank);
  set incur = set_generic_make(set_private, points_to_equal_p,
				points_to_rank);
  set in_may = set_generic_make(set_private, points_to_equal_p,
			   points_to_rank);
  set in_must = set_generic_make(set_private, points_to_equal_p,
			   points_to_rank);
  set kill_must = set_generic_make(set_private, points_to_equal_p,
			   points_to_rank);
  set kill_may = set_generic_make(set_private, points_to_equal_p,
			   points_to_rank);
  set gen_must = set_generic_make(set_private, points_to_equal_p,
			   points_to_rank);
  set gen_may = set_generic_make(set_private, points_to_equal_p,
			   points_to_rank);
  set out = set_generic_make(set_private, points_to_equal_p,
			   points_to_rank);
  set kill = set_generic_make(set_private, points_to_equal_p,
			   points_to_rank);
  set gen = set_generic_make(set_private, points_to_equal_p,
			   points_to_rank);
  set out1 = set_generic_make(set_private, points_to_equal_p,
			   points_to_rank);
  set out2 = set_generic_make(set_private, points_to_equal_p,
			   points_to_rank);
  list L = NIL, R = NIL, args = NIL;
  bool address_of_p = false;
   
  if (instruction_expression_p(statement_instruction(current))) {
    //expression e = instruction_expression(statement_instruction(current));
    ;
  }


  /* Compute side effects of rhs and lhs */
  set_assign(cur, points_to_expression(rhs, in, true));
  set_assign(incur, points_to_expression(lhs, cur, true));
 
  if( expression_reference_p(lhs) ) {
    entity e = expression_to_entity(lhs);
    if( entity_variable_p(e)  ) {
	if(top_level_entity_p(e)|| formal_parameter_p(e) ) {
	  reference nr = make_reference(e, NIL);
	  cell nc = make_cell_reference(nr);
	  if (!source_in_set_p(nc, incur)) {
	    incur = set_union(incur, incur,
			      formal_points_to_parameter(nc));
	    /* set_assign(in_may, points_to_may_filter(incur)); */
	    /* set_assign(in_must, points_to_must_filter(incur)); */
	    /* set_assign(in, incur); */
	  }
	}
    }
  }
 
  /*Change lhs into a contant memory path*/
  L = expression_to_constant_paths(current, lhs, incur);

  /* rhs should be a lvalue, if not we should transform it into a
     lvalue or call the adequate function according to its type */
  bool type_sensitive_p = !get_bool_property("ALIASING_ACROSS_TYPES");
  if(array_argument_p(rhs)){
    R = array_to_constant_paths(rhs,incur);
    if(!expression_pointer_p(rhs))
      address_of_p = true;
  }else if(expression_reference_p(rhs)){
    if (array_argument_p(rhs)){
      R = array_to_constant_paths(rhs, incur);
      if(!expression_pointer_p(rhs))
	address_of_p = true;
    }
    /* scalar case, rhs is already a lvalue */
    entity e = expression_to_entity(rhs);
    /* type tt = ultimate_type(entity_type(e)); */
    if(same_string_p(entity_local_name(e),"NULL")){
      entity ne = entity_null_locations();
      reference nr = make_reference(ne, NIL);
      cell nc = make_cell_reference(nr);
      R = CONS(CELL, nc, NIL);
      address_of_p = true;
	  }
    else if(entity_variable_p(e)) {
      /* add points-to relations in demand for global pointers */
      if(entity_pointer_p(e)  ) {
	if(top_level_entity_p(e) || formal_parameter_p(e) ) {
	  reference nr = make_reference(e, NIL);
	  cell nc = make_cell_reference(nr);
	  if (!source_in_set_p(nc, incur)) {
	  incur = set_union(incur, incur,
			    formal_points_to_parameter(nc));
	  /* set_assign(in_may, points_to_may_filter(incur)); */
	  /* set_assign(in_must, points_to_must_filter(incur)); */
	  /* set_assign(in, incur); */
      }
    }
      }
  
      R = expression_to_constant_paths(current, rhs, incur);
      /* basic b = variable_basic(type_variable(tt)); */
      /* if(basic_pointer_p(b)){ */
      /* 	basic bt = variable_basic(type_variable(basic_pointer(b))); */
      /* 	if(basic_typedef_p(bt)){ */
      /* 	  basic ulb = basic_ultimate(bt); */
      /* 	  if(basic_derived_p(ulb)){ */
      /* 	    return points_to_pointer_to_derived_assignment(current, lhs, rhs, incur); */
      /* 	  } */
      /* 	}else if(basic_derived_p(bt)) */
      /* 	  return points_to_derived_assignment(current, lhs, rhs, incur); */
      /* 	/\* else *\/ */
      /* 	/\*  R = expression_to_constant_paths(rhs,incur); *\/ */
      /* } */
    }
  }
    /* (derived_entity_p(e) || typedef_entity_p(e)) */
    /*       return points_to_derived_assignment(current, lhs, rhs, in); */
    /* else */
    /*   R = expression_to_constant_paths(rhs,incur); */

  else if (expression_cast_p(rhs)){
    expression nrhs = cast_expression(expression_cast(rhs));
    return points_to_assignment(current, lhs, nrhs, incur);
  }
  else if(expression_equal_integer_p(rhs, 0)){
    entity ne = entity_null_locations();
    reference nr = make_reference(ne, NIL);
    cell nc = make_cell_reference(nr);
    R = CONS(CELL, nc, NIL);
    address_of_p = true;

  }
  else if(assignment_expression_p(rhs)){
    call c = expression_call(rhs);
    args = call_arguments(c);
    expression nrhs = EXPRESSION(CAR(args));
    return  points_to_assignment(current, lhs, nrhs, incur);
  }
  else if(comma_expression_p(rhs)){
    /* comma case, lhs should point to the same location as the last
       pointer which appears into comma arguments*/
    call c = expression_call(rhs);
    args = call_arguments(c);
    expression nrhs = expression_undefined;
    FOREACH(expression, ex, args){
      incur = set_assign(incur, points_to_expression(ex, incur, true));
      nrhs = copy_expression(ex);
    }
    return  points_to_assignment(current, lhs, nrhs, incur);

  }

  else if(address_of_expression_p(rhs)){
    /* case & opeator */
    call c = expression_call(rhs);
    args = call_arguments(c);
    expression nrhs = EXPRESSION(CAR(args));
   /*  set_assign(R, expression_to_constant_paths(nrhs,incur)); */
    if(array_argument_p(nrhs))
      R = array_to_constant_paths(nrhs,incur);
    else
      R = expression_to_constant_paths(current, nrhs,incur);
    address_of_p = true;

  }
  else if(subscript_expression_p(rhs)){
    /* case [] */
  /*   syntax sy = expression_syntax(rhs); */
/*     subscript sb = syntax_subscript(sy); */
/*     rhs = subscript_array(sb); */
    R = expression_to_constant_paths(current, rhs, incur);
  }
  else if(operator_expression_p(rhs, POINT_TO_OPERATOR_NAME)){
    /* case -> operator */
    R = expression_to_constant_paths(current, rhs, incur);
   
  }
  else if(expression_field_p(rhs)){
    R = expression_to_constant_paths(current, rhs,incur);
  }
  else if(operator_expression_p(rhs, DEREFERENCING_OPERATOR_NAME)){
    R = expression_to_constant_paths(current, rhs,incur);
  }
  else if(operator_expression_p(rhs, C_AND_OPERATOR_NAME)){
    /* case && operator */

  }
  else if(operator_expression_p(rhs, C_OR_OPERATOR_NAME)){
    /* case || operator */

  }
  else if(operator_expression_p(rhs, CONDITIONAL_OPERATOR_NAME)){
    /* case ? operator is similar to an if...else instruction */
    call c = expression_call(rhs);
    args = call_arguments(c);
    expression cond = EXPRESSION(CAR(args));
    expression arg1 = EXPRESSION(CAR(CDR(args)));
    expression arg2 = EXPRESSION(CAR(CDR(CDR(args))));
    incur = points_to_expression(cond, incur, true);
    out1 = points_to_assignment(current, lhs, arg1, incur);
    out2 = points_to_assignment(current,lhs, arg2, incur);
    return merge_points_to_set(out1, out2);
  }
  else if(expression_call_p(rhs)){
    if(ENTITY_MALLOC_SYSTEM_P(expression_to_entity(rhs)) ||
       ENTITY_CALLOC_SYSTEM_P(expression_to_entity(rhs))){
      expression sizeof_exp = EXPRESSION (CAR(call_arguments(expression_call(rhs))));
      type t = expression_to_type(lhs);
      reference nr = original_malloc_to_abstract_location(lhs,
							  t,
							  type_undefined,
							  sizeof_exp,
							  get_current_module_entity(),
							  current);
      cell nc = make_cell_reference(nr);
      R = CONS(CELL, nc, NIL);
      address_of_p = true;
    }
    else if(user_function_call_p(rhs)){
      type t = entity_type(call_function(expression_call(rhs)));
      entity ne = entity_undefined;
      if(type_sensitive_p)
	ne = entity_all_xxx_locations_typed(ANYWHERE_LOCATION,t);
      else
	ne = entity_all_xxx_locations(ANYWHERE_LOCATION);
    
      reference nr = make_reference(ne, NIL);
      cell nc = make_cell_reference(nr);
      R = CONS(CELL, nc, NIL);
      address_of_p = true;
    }
    else{
     /*  entity ne = entity_all_locations(); */
      type t = entity_type(call_function(expression_call(rhs)));
      entity ne = entity_undefined;
      if(type_sensitive_p)
	 ne = entity_all_xxx_locations_typed(ANYWHERE_LOCATION,t);
      else
	 ne = entity_all_xxx_locations(ANYWHERE_LOCATION);
      reference nr = make_reference(ne, NIL);
      cell nc = make_cell_reference(nr);
      R = CONS(CELL, nc, NIL);
      address_of_p = true;
    }
  }
  else{
    type t = expression_to_type(rhs);
    type lt = expression_to_type(lhs);
    /* Handle strut assignment m = n */
    type ct = compute_basic_concrete_type(t);
    type lct = compute_basic_concrete_type(lt);
    if(type_struct_p(ct) && type_struct(lct)) {
	list l1 = type_struct(ct);
	list l2 = type_struct(lct);

	FOREACH(ENTITY, i, l1) {
	  if( expression_pointer_p(entity_to_expression(i)) ) {
	    entity ent2 = ENTITY (CAR(l2));
	    expression ex1 = MakeBinaryCall(entity_intrinsic(FIELD_OPERATOR_NAME),
					    lhs,
					    entity_to_expression(i));
	    expression ex2 = MakeBinaryCall(entity_intrinsic(FIELD_OPERATOR_NAME),
					    rhs,
					    entity_to_expression(ent2));
	    expression_consistent_p(ex1);
	    expression_consistent_p(ex1);
	    return points_to_assignment(current, ex1, ex2, incur);
	  }
	  l2 = CDR(l2);
	}


    }
    else  {
    entity ne = entity_undefined;
    if(type_sensitive_p)
      ne = entity_all_xxx_locations_typed(ANYWHERE_LOCATION,t);
    else
      ne = entity_all_xxx_locations(ANYWHERE_LOCATION);
    reference nr = make_reference(ne, NIL);
    cell nc = make_cell_reference(nr);
    R = CONS(CELL, nc, NIL);
    address_of_p = true;
  }
  }
  /* set_assign(kill_may, kill_may_set(L, in_may)); */
 /*Extract MAY/MUST points to relations from the input set*/
  set_assign(in_may, points_to_may_filter(incur));
  set_assign(in_must, points_to_must_filter(incur));

  set_assign(kill_may, kill_may_set(L, in_may));
  set_assign(kill_must, kill_must_set(L, incur));
  set_assign(gen_may, gen_may_set(L, R, in_may, &address_of_p));
  set_assign(gen_must, gen_must_set(L, R, in_must, &address_of_p));
  set_union(kill, kill_may, kill_must);
  set_union(gen, gen_may, gen_must);
  if(set_empty_p(gen)) {
    if(type_sensitive_p)
    set_assign(gen, points_to_anywhere_typed(L, incur));
    else
      set_assign(gen, points_to_anywhere(L, incur));
  }
  set_difference(incur, incur, kill);
  set_union(out, incur, gen);
  return out;
}


/* set points_to_pointer_to_derived_assignment(statement current, expression lhs, expression rhs, set in){ */

/*   set out = set_generic_make(set_private, points_to_equal_p, */
/* 			     points_to_rank); */
/*   set cur = set_generic_make(set_private, points_to_equal_p, */
/* 				points_to_rank); */
/*   set incur = set_generic_make(set_private, points_to_equal_p, */
/* 				points_to_rank); */
/*   set in_may = set_generic_make(set_private, points_to_equal_p, */
/* 			   points_to_rank); */
/*   set in_must = set_generic_make(set_private, points_to_equal_p, */
/* 			   points_to_rank); */
/*   set kill_must = set_generic_make(set_private, points_to_equal_p, */
/* 			   points_to_rank); */
/*   set kill_may = set_generic_make(set_private, points_to_equal_p, */
/* 			   points_to_rank); */
/*   set gen_must = set_generic_make(set_private, points_to_equal_p, */
/* 			   points_to_rank); */
/*   set gen_may = set_generic_make(set_private, points_to_equal_p, */
/* 			   points_to_rank); */
/*   set kill = set_generic_make(set_private, points_to_equal_p, */
/* 			   points_to_rank); */
/*   set gen = set_generic_make(set_private, points_to_equal_p, */
/* 			   points_to_rank); */
/*   list L = NIL, R = NIL; */
/*   bool address_of_p = false; */

/*   set_assign(in_may, points_to_may_filter(in)); */
/*   set_assign(in_must, points_to_must_filter(in)); */
/*   bool type_sensitive_p = !get_bool_property("ALIASING_ACROSS_TYPES"); */
/*   entity el = expression_to_entity(lhs); */
/*   if(entity_variable_p(el)){ */
/*     basic b = variable_basic(type_variable(entity_type(el))); */
/*     if(basic_pointer_p(b)){ */
/*       set_assign(cur, points_to_expression(rhs, in, true)); */
/*       set_assign(incur, points_to_expression(lhs, cur, true)); */
/*       R = expression_to_constant_paths(current, rhs,incur); */
/*       L = expression_to_constant_paths(current, lhs,incur); */
/*       set_assign(kill_may, kill_may_set(L, in_may)); */
/*       set_assign(kill_must, kill_must_set(L, in_must)); */
/*       set_assign(gen_may, gen_may_set(L, R, in_may, &address_of_p)); */
/*       set_assign(gen_must, gen_must_set(L, R, in_must, &address_of_p)); */
/*       set_union(kill, kill_may, kill_must); */
/*       set_union(gen, gen_may, gen_must); */
/*       if(set_empty_p(gen)) { */
/* 	if(type_sensitive_p ) */
/* 	set_assign(gen, points_to_anywhere_typed(L, incur)); */
/* 	else */
/* 	  set_assign(gen, points_to_anywhere(L, incur)); */
/*       } */
/*       set_difference(in, in, kill); */
/*       set_union(out, in, gen); */

/*       basic bt = variable_basic(type_variable(basic_pointer(b))); */
/*       if(basic_typedef_p(bt)){ */
/* 	basic ulb = basic_ultimate(bt); */
/* 	if(basic_derived_p(ulb)){ */
/* 	  entity e1  = basic_derived(ulb); */
/* 	  type t1 = entity_type(e1); */
/* 	  if(type_struct_p(t1)){ */
/* 	    entity rl = expression_to_entity(rhs); */
/* 	    basic b1 = variable_basic(type_variable(entity_type(rl))); */
/* 	    basic bt1 = variable_basic(type_variable(basic_pointer(b1))); */
/* 	    basic ulb1 = basic_ultimate(bt1); */
/* 	    entity e2  = basic_derived(ulb1); */
/* 	    type t2 = entity_type(e2); */
/* 	    list l_lhs = type_struct(t1); */
/* 	    list l_rhs = type_struct(t2); */
/* 	    for (; !ENDP(l_lhs) &&!ENDP(l_rhs)  ; POP(l_lhs), POP(l_rhs)){ */
/* 	      expression ex1 = entity_to_expression(ENTITY(CAR(l_lhs))); */
/* 	      expression nlhs = MakeBinaryCall(entity_intrinsic(POINT_TO_OPERATOR_NAME), */
/* 					       lhs, */
/* 					       ex1); */
/* 	      expression ex2 = entity_to_expression(ENTITY(CAR(l_rhs))); */
/* 	      expression nrhs = MakeBinaryCall(entity_intrinsic(POINT_TO_OPERATOR_NAME), */
/* 					       rhs, */
/* 					       ex2); */
/* 	      if(expression_pointer_p(nlhs)) */
/* 		set_assign(out, points_to_assignment(current, nlhs, nrhs, out)); */
/* 	    } */
/* 	  } */
/* 	  else if(type_union_p(t1)) */
/* 	    pips_user_warning("union case not handled yet\n"); */
/* 	  else if(type_enum_p(t1)) */
/* 	    pips_user_warning("enum case not handled yet\n"); */
/* 	} */
/*       } */
/*     } */
/*   } */
/*   return out; */
/* } */


set points_to_derived_assignment(statement current, expression lhs, expression rhs, set in){

  set out = set_generic_make(set_private, points_to_equal_p,
			     points_to_rank);
  set_assign(out, in);
  entity e_lhs = expression_to_entity(lhs);
  type ct_lhs = entity_basic_concrete_type(e_lhs);
  entity e_rhs = expression_to_entity(rhs);
  type ct_rhs = entity_basic_concrete_type(e_rhs);
  if( type_struct_variable_p(ct_lhs) && type_struct_variable_p(ct_rhs) ) {
    variable v_r = type_variable(ct_lhs);
    variable v_l = type_variable(ct_rhs);
    basic b_r = variable_basic(v_r);
    basic b_l = variable_basic(v_l);
    entity e1 = basic_derived(b_r);
    entity e2 = basic_derived(b_l);
    type t1 = compute_basic_concrete_type(entity_type(e1));
    type t2 = compute_basic_concrete_type(entity_type(e2));
		list l_lhs = type_struct(t1);
		list l_rhs = type_struct(t2);
		for (; !ENDP(l_lhs) &&!ENDP(l_rhs)  ; POP(l_lhs), POP(l_rhs)){
		  expression ex1 = entity_to_expression(ENTITY(CAR(l_lhs)));
		  expression nlhs = MakeBinaryCall(entity_intrinsic(FIELD_OPERATOR_NAME),
						   lhs,
						   ex1);
		  expression ex2 = entity_to_expression(ENTITY(CAR(l_rhs)));
		  expression nrhs = MakeBinaryCall(entity_intrinsic(FIELD_OPERATOR_NAME),
						   rhs,
						   ex2);
		  if(expression_pointer_p(nlhs))
	set_assign(out, points_to_assignment(current, nlhs, nrhs, out));
		}
	      }
  else if(type_union_p(ct_lhs))
	  pips_user_warning("union case not handled yet\n");
  else if(type_enum_p(ct_lhs))
	  pips_user_warning("enum case not handled yet\n");

  return  out;
}








/* iterate over the lhs_set, if it contains more than an element
   approximations are set to MAY, otherwise it's set to EXACT
   Set the sink to null value .*/
set points_to_null_pointer(list lhs_list, set input)
{
  /* lhs_path matches the kill set.*/
  set kill = set_generic_make(set_private, points_to_equal_p,
				    points_to_rank);
  set gen = set_generic_make(set_private, points_to_equal_p,
				    points_to_rank);
  set res= set_generic_make(set_private, points_to_equal_p,
				    points_to_rank);
  set input_kill_diff = set_generic_make(set_private, points_to_equal_p,
				    points_to_rank);
  entity e = entity_undefined;
  approximation a = make_approximation_exact();
  bool type_sensitive_p = !get_bool_property("ALIASING_ACROSS_TYPES");
  SET_FOREACH(points_to, p, input)
    {
      FOREACH(cell, c, lhs_list)
	{
	  if(cell_equal_p(points_to_source(p),c))
	    set_add_element(kill,kill,(void*) p);
	}
    }

  /* input - kill */
  set_difference(input_kill_diff, input, kill);

  /* if the lhs_set or the rhs set contains more than an element, we
     set the approximation to MAY. */
  if(gen_length(lhs_list) > 1)// || set_size(rhs_set)>1)
    a = make_approximation_may();

  /* Computing the gen set*/
  FOREACH(cell, c, lhs_list){
      /* we test if lhs is an array, so we change a[i] into a[*] */
      bool to_be_freed = false;
      type c_type = cell_to_type(c, &to_be_freed);
      if(array_type_p(c_type))
	{
	  c = array_to_store_independent(c);
	  if (to_be_freed) free_type(c_type);
	  c_type = cell_to_type(c, &to_be_freed);
	}

      /* create a new points to with as source the current
	 element of lhs_set and sink the null value .*/
      if(type_sensitive_p)
	 e = entity_all_xxx_locations_typed(NULL_POINTER_NAME,c_type);
      else
	 e = entity_all_xxx_locations(NULL_POINTER_NAME);
      reference r = make_reference(e, NIL);
      cell sink = make_cell_reference(r);
      points_to pt_to = make_points_to(c, sink, a, make_descriptor_none());
      set_add_element(gen, gen, (void*) pt_to);
      
      if (to_be_freed) free_type(c_type);
    }
  /* gen + input_kill_diff*/
  set_union(res, gen, input_kill_diff);

  return res;
}


/* initialize in demand global variables and formal parameters */
set points_to_init_global(statement s, list l, set in)
{
  list l_eval = NIL;
  bool eval = false;
  if ( !ENDP(l) ) {
    FOREACH ( cell, c, l ) {
      reference r = cell_any_reference(c);
      entity e = reference_variable(r);
      if( top_level_entity_p(e) || formal_parameter_p(e) ) {
	  value v_init = entity_initial(e);
	  reference nr = make_reference(e, NIL);
	  cell nc = make_cell_reference(nr);
	  bool exact_p = false;
	  list l_cell = CONS(CELL, nc, NIL);
	  if ( effect_reference_dereferencing_p(r, &exact_p) ) {
	    /* search for the corresponding sinks */
	    set_methods_for_proper_simple_effects();
	    list l_in = set_to_sorted_list(in,
				      (int(*)(const void*, const void*))
				      points_to_compare_location);
	    l_eval = eval_cell_with_points_to(c, l_in, &exact_p);
	    generic_effects_reset_all_methods();
	  /* if( ENDP(l_eval) ){ */
	/*     l_eval = gen_nconc(l_eval, l_cell); */
	/*     l_eval= gen_nconc(l_eval, l_cell); */
	/*   } */
	/*   else */
	/*     eval=true; */
	/* } */
	}
	if(ENDP(l_eval)){
	      l_eval = gen_nconc(l_eval, l_cell);
	  }
	else{
	eval = true;
	}
	/* l_eval = gen_nconc(l_eval, l_cell); */
	  FOREACH ( cell, cel, l_eval ) {
	  bool changed = false;
	  cell new_cel = simple_cell_to_store_independent_cell(cel, &changed);
	  if ( !source_in_set_p(new_cel, in )) {
	      if( value_expression_p(v_init) ){
		expression exp_init = value_expression(v_init);
	      bool t_to_be_freed = false;
	      type t = cell_reference_to_type(r, &t_to_be_freed);
	      if(pointer_type_p(t)){
		expression lhs = entity_to_expression(e);
		  in = set_assign(in,points_to_assignment(s,
							  copy_expression(lhs),
							  copy_expression(exp_init),
							  in));
		if (t_to_be_freed) free_type(t);
	      }
	    }
	    else if(eval)
		in = set_union(in, in,
			     formal_points_to_parameter( cel));
	    else
	      in = set_union(in, in,
			     formal_points_to_parameter( nc));
	      }
	    }
	  }
      }
    }

  return in;
}

set points_to_general_assignment(__attribute__ ((__unused__)) statement st,
				 expression lhs,
				 expression rhs,
				 set pts_in,
				 __attribute__ ((__unused__)) list l)
{
  set res = set_generic_make(set_private, points_to_equal_p,
				    points_to_rank);
  list lhs_list = NIL/*, rhs_list = NIL*/ ;
  bool nowhere_lhs_p = false, nowhere_rhs_p = false;

/* we test the type of lhs by using expression_pointer_p(), I'm  not
   sure if it's cover all the possibilitie, to be checked later ... */
  if(expression_pointer_p(lhs)) {
     /* we call expression_to_constant_path() which calls
	1- get_memory_path() to change **p into p[0][0]
        2- eval_cell_with_points_to() to eval the memory path using
	points to
        3- possible_constant_path_list() which depending in
	eval_cell_with_points_to() result's computes a constant path
	or an nowhere points to*/
    lhs_list = expression_to_constant_paths(st, lhs, pts_in); //, &nowhere_lhs_p);
    if(nowhere_lhs_p) {
      //return lhs_list;
      ;
    }
     /* Now treat the rhs, the call to & requires a special treatment */
     syntax s = expression_syntax(rhs);
     if(syntax_call_p(s)){
       call c = syntax_call(s);
       /* if it's a call to &, replace rhs by the first argument of & */
       if (entity_an_operator_p(call_function(c), ADDRESS_OF)){
	 rhs = EXPRESSION(CAR(call_arguments(c)));
	 // rhs_list = expression_to_constant_paths(st, rhs, pts_in);//, &nowhere_rhs_p);
	 if(nowhere_rhs_p)
	   set_assign(res, points_to_nowhere_typed(lhs_list, pts_in));
	 else{
	   /* change basic_ref_addr into basic_ref_addr_emami*/
	   //set_assign(res, basic_ref_addr_emami(lhs_set, rhs_set,
	   //pts_in));
	   ;
	 }
       }
     }else if(syntax_cast_p(s)){
       set_assign(res, points_to_null_pointer(lhs_list, pts_in));
     }else if(expression_reference_p(rhs)){
       //rhs_list = expression_to_constant_paths(st, rhs, pts_in);//, &nowhere_rhs_p);
       if(nowhere_rhs_p)
	 set_assign(res, points_to_nowhere_typed(lhs_list, pts_in));
       else{
	 /* change basic_ref_ref() into basic_ref_ref_emami()*/
	 //set_assign(res, basic_ref_ref_emami(lhs_set, rhs_set,
	 //pts_in));
	 ;
       }
     }else if(array_argument_p(rhs)){
       ;
     }
  }
  return res;
}




/* compute the points to set associate to a sequence of statements */
set points_to_sequence(sequence seq, set pt_in, bool store) {
  set pt_out = set_generic_make(set_private, points_to_equal_p,
				points_to_rank);
  list dl = NIL;
   set_assign(pt_out, pt_in);
   FOREACH(statement, st, sequence_statements(seq)) {
     set_assign(pt_out, points_to_recursive_statement(st,pt_out,store));
     if(statement_block_p(st) && !ENDP(dl=statement_declarations(st)))
	pt_out = points_to_block_projection(pt_out, dl);
   }
  
  return pt_out;
}

/* compute the points-to set for an intrinsic call */
set points_to_intrinsic(statement s, call c __attribute__ ((__unused__)), entity e, list pc, set pt_in,
			list el) {
  set pt_out = set_generic_make(set_private, points_to_equal_p,
				points_to_rank);
  set pt_cur = set_generic_make(set_private, points_to_equal_p,
				points_to_rank);
  expression lhs = expression_undefined;
  expression rhs = expression_undefined;
  list l_cell = NIL, l = NIL;
  bool *nowhere_p = false;
  pips_debug(8, "begin\n");

  /* Recursive descent on subexpressions for cases such as "p=q=t=u;" */
  set_assign(pt_cur, pt_in);
  FOREACH(EXPRESSION, ex, pc) {
    pt_cur = set_assign(pt_cur, points_to_expression(copy_expression(ex),
						     pt_cur, true));
  }
  if (ENTITY_ASSIGN_P(e)) {
    lhs = EXPRESSION(CAR(pc));
    rhs = EXPRESSION(CAR(CDR(pc)));
    /* test if lhs is a pointer */
    set_assign(pt_out, pt_cur);
   
      if(expression_pointer_p(lhs))
	{
	set_assign(pt_out,points_to_assignment(s, copy_expression(lhs),
					       copy_expression( rhs), pt_cur));
	if (set_empty_p(pt_out))
	  /* Use effects to remove broken points-to relations and to link
	     broken pointers towards default sinks.
	     pt_out = points_to_filter_with_expression_effects(e, pt_cur);
	  */
	  pt_out = points_to_general_assignment(s, copy_expression(lhs),copy_expression(rhs), pt_out, el);
	}
      else if(expression_reference_p(lhs))
	{
	entity e = expression_to_entity(lhs);
	type t = entity_type(e);
	if(entity_variable_p(e))
	  {
	  basic b = variable_basic(type_variable(t));
	  if(basic_typedef_p(b))
	    {
	    basic ulb = basic_ultimate(b);
		if(basic_derived_p(ulb)){
		  set_assign(pt_out,points_to_derived_assignment(s, lhs, rhs, pt_out));
		  set_assign(pt_cur, pt_out);
	    }
	  }
	}
  }
  }
  else if (/* ENTITY_PLUS_UPDATE_P(e) ||  ENTITY_MINUS_UPDATE_P(e)
	      ||*/ ENTITY_MULTIPLY_UPDATE_P(e) || ENTITY_DIVIDE_UPDATE_P(e)
	   || ENTITY_MODULO_UPDATE_P(e) || ENTITY_LEFT_SHIFT_UPDATE_P(e)
	   || ENTITY_RIGHT_SHIFT_UPDATE_P(e) || ENTITY_BITWISE_AND_UPDATE_P(e)
	   || ENTITY_BITWISE_XOR_UPDATE_P(e) || ENTITY_BITWISE_OR_UPDATE_P(e)) {
    /* Look at the lhs with
       generic_proper_effects_of_complex_address_expression(). If the
       main effect is an effect on a pointer, occurences of this
       pointer must be removed from pt_cur to build pt_out.
    */

    pt_out = points_to_filter_with_effects(pt_cur, el);
  } else if (ENTITY_POST_INCREMENT_P(e) || ENTITY_POST_DECREMENT_P(e)
	     || ENTITY_PRE_INCREMENT_P(e) || ENTITY_PRE_DECREMENT_P(e)) {
    lhs = EXPRESSION(CAR(pc));
    if( expression_reference_p(lhs) ) {
      entity e = expression_to_entity(lhs);
      if( entity_variable_p(e) ) {
	if(top_level_entity_p(e) || formal_parameter_p(e)) {
	  bool eval_p = true;
	  bool exact_p = false;
	  cell c = get_memory_path(lhs, &eval_p);
	   if( !eval_p ) {
	     l_cell = CONS(CELL, c, NIL);
	     pt_cur =  set_assign(pt_cur, points_to_init_global(s, l_cell, pt_cur));
	   }
	   else {
	     set_methods_for_proper_simple_effects();
	     list l_in = set_to_sorted_list(pt_cur,
				       (int(*)(const void*, const void*))
				       points_to_compare_location);
	     list l_eval = eval_cell_with_points_to(c, l_in, &exact_p);
	     generic_effects_reset_all_methods();
	     l_cell = gen_nconc(l,possible_constant_paths(l_eval,c,nowhere_p));
	     pt_cur =  set_assign(pt_cur, points_to_init_global(s, l_cell, pt_cur));
	   }
	}
      }
    }
    pt_out = set_assign(pt_out, points_to_post_increment(s, lhs, pt_cur, el ));
  }
  else if(ENTITY_PLUS_UPDATE_P(e) || ENTITY_MINUS_UPDATE_P(e)) {
    lhs = EXPRESSION(CAR(pc));
    rhs = EXPRESSION(CAR(CDR(pc)));
    if( expression_reference_p(lhs) ) {
      entity e = expression_to_entity(lhs);
      if( entity_variable_p(e) ) {
	if(top_level_entity_p(e) || formal_parameter_p(e)) {
	  bool eval_p = true;
	  bool exact_p = false;
	  cell c = get_memory_path(lhs, &eval_p);
	   if( !eval_p ) {
	     l_cell = CONS(CELL, c, NIL);
	     pt_cur =  set_assign(pt_cur, points_to_init_global(s, l_cell, pt_cur));
	   }
	   else {
	     set_methods_for_proper_simple_effects();
	     list l_in = set_to_sorted_list(pt_cur,
				       (int(*)(const void*, const void*))
				       points_to_compare_location);
	     list l_eval = eval_cell_with_points_to(c, l_in, &exact_p);
	     generic_effects_reset_all_methods();
	     l_cell = gen_nconc(l,possible_constant_paths(l_eval,c,nowhere_p));
	     pt_cur =  set_assign(pt_cur, points_to_init_global(s, l_cell, pt_cur));
	   }
	}
      }
    }
    pt_out = set_assign(pt_out, points_to_plus_update(s, lhs, rhs, pt_cur, el));

  }
  else if (ENTITY_STOP_P(e) || ENTITY_ABORT_SYSTEM_P(e)
	   || ENTITY_EXIT_SYSTEM_P(e)) {
    /* The call is never returned from. No information is available
       for the dead code that follows. pt_out is already set to the
       empty set. */
    set_clear(pt_out);
  }else if (ENTITY_C_RETURN_P(e)) {
    /* The call is never returned from. No information is available
       for the dead code that follows. pt_out is already set to the
       empty set. */
    set_assign(pt_out, pt_cur);
  }else if(ENTITY_AND_P(e)) {
    FOREACH(EXPRESSION, exp, pc) {
      pt_out = set_assign(pt_out, points_to_expression(copy_expression(exp),
						       pt_cur,
						       true));
    }
  } else if(ENTITY_OR_P(e)) {
    FOREACH(EXPRESSION, exp, pc) {
      pt_out = set_assign(pt_out, points_to_expression(copy_expression(exp),
						       pt_cur,
						       true));
    }
  }else if (ENTITY_COMMA_P(e)) {
    FOREACH(EXPRESSION, exp, pc) {
      pt_out = set_assign(pt_out, points_to_expression(copy_expression(exp),
						       pt_cur,
						       true));
    }
  }
  else {
    /*By default, use the expression effects to filter cur_pt */
    //if(!set_empty_p(pt_out))
    //pt_out =
    //set_assign(pt_out,points_to_filter_with_expression_effects(pt_cur,
    //effects));
    pt_out = points_to_filter_with_effects(pt_cur, el);
  }
  /* if pt_out != pt_cur, do not forget to free pt_cur... */
  pips_debug(8, "end\n");
 
  return pt_out;
}

/* input:
 *  a set of points-to pts
 *  a list of effects el
 *
 * output
 *  a updated set of points-to pts (side effects)
 *
 * Any pointer written in el does not point to its old target anymore
 * but points to any memory location. OK, this is pretty bad, but it
 * always is correct.
 */
set points_to_filter_with_effects(set pts, list el) {
  list lhs_list = NIL;
  bool type_sensitive_p = !get_bool_property("ALIASING_ACROSS_TYPES");

  FOREACH(EFFECT, e, el) {
    if(effect_pointer_type_p(e) && effect_write_p(e)) {

      cell c = effect_cell(e);
      /* Theheap problem with the future extension to GAP is hidden
	 within cell_any_reference */
      reference r = cell_any_reference(c);
      cell nc = make_cell_reference(r);
      if(ENDP(reference_indices(r))) {
	/* Simple case: a scalar pointer is written */
	/* Remove previous targets */
	SET_FOREACH(points_to, pt, pts) {
	  cell ptc = points_to_source(pt);
	  if(points_to_compare_cell(nc, ptc)){
	    if(! cell_in_list_p(ptc, lhs_list))
	      lhs_list = gen_concatenate(lhs_list, CONS(CELL, ptc, NIL));
	  }
	}
	if(type_sensitive_p )
	set_assign(pts, points_to_anywhere_typed(lhs_list, pts));
	else
	  set_assign(pts, points_to_anywhere(lhs_list, pts));
      }
      else {
	/* Complex case: is the reference usable with the current pts?
	 * If it uses an indirection, check that the indirection is
	 * not thru nowhere, not thru NULL, and maybe not thru
	 * anywhere...
	 */
	/* Remove previous targets */
	SET_FOREACH(points_to, pt, pts) {
	  cell ptc = points_to_source(pt);
	  if(points_to_compare_cell(ptc, nc)){
	    if(! cell_in_list_p(ptc, lhs_list))
	      lhs_list = gen_concatenate(lhs_list,CONS(CELL, ptc, NIL));
	  }
	}

	/* add the anywhere points-to*/
	if(type_sensitive_p )
	set_assign(pts, points_to_anywhere_typed(lhs_list, pts));
	else
	  set_assign(pts, points_to_anywhere(lhs_list, pts));
	//pips_internal_error("Complex pointer write effect."
	//" Not implemented yet\n");
      }
    }
  }
  return pts;
}


/*
See C standard, section 6:
if the expression lhs points to the i-th element of an array object,
the expression (lhs)+N (equivalently, N+(lhs)) and (lhs)-N (where N has the value n)
point to, respectively, the i+n-th and i-n-th elements of the array object.

Since we can't always keep the element's indice we only keep the information about
the array object as tab[*]
See the example below :

int n = 4, m = 3;
int a[n][m];
int (*p)[m] = a; // {(p,a[0],-EXACT)}
p += 1; // {(p,a[*],-EXACT)}


If the property POINTS_TO_STRICT_POINTER_TYPES is set to false we apply the C standard
whatever the sink is an integer or an array. When it's set to true we stop the analysis
with a pips_user_error.
For more details see the description  of the property at pipsmake-rc.tex
*/

set points_to_plus_update(statement s, expression lhs, expression rhs, set pts, list eff_list)
{
  set res = set_generic_make(set_private, points_to_equal_p,
			     points_to_rank);
  list new_inds = NIL;
  bool type_strict_p = !get_bool_property("POINTS_TO_STRICT_POINTER_TYPES");
  int i;
  res = set_assign(res, pts);
  if( expression_integer_constant_p(rhs)) {
    if(expression_reference_p(lhs)) {
      reference r = expression_reference(lhs);
      cell c = make_cell_reference(r);
      SET_FOREACH(points_to, pt, pts) {
	cell pt_source = points_to_source(pt);
	if(points_to_compare_cell(c, pt_source)) {
	  cell pt_sink = points_to_sink(pt);
	  reference ref_sink = cell_any_reference(pt_sink);
	  entity ent_sink = reference_variable(ref_sink);
	  /* if(array_reference_p(ref_sink)) { */
	  if(array_entity_p(ent_sink)) {
	    list l_ind = reference_indices(ref_sink);
	    if(ENDP(l_ind)) {
	      int dim = variable_entity_dimension(reference_variable(ref_sink));
	      for(i = 0; i< dim; i++) {
		expression new_ind = make_unbounded_expression();
		l_ind = gen_nconc(CONS(EXPRESSION, new_ind, NIL),new_inds);
	      }
	    }

	    FOREACH(EXPRESSION, exp, l_ind){
	      if(expression_integer_constant_p(exp)) {
		expression new_ind = make_unbounded_expression();
		new_inds = gen_nconc(CONS(EXPRESSION, new_ind, NIL),new_inds);
	      }
	      else
		new_inds = gen_nconc(new_inds, l_ind);
	    }
	    reference new_ref = make_reference(reference_variable(ref_sink),new_inds);
	    expression new_rhs = reference_to_expression(new_ref);
	    res =  set_assign(res, points_to_assignment( s, copy_expression(lhs), copy_expression(new_rhs), pts ));
	    new_inds = NIL;
	  }
	  else if( type_strict_p ) {
	    res = set_assign(res, points_to_filter_with_effects(pts, eff_list));
	}
	  else {
	    entity e =  reference_variable(r);
	    pips_user_warning("Illegal arithmetic pointer %s at statement %d \n", entity_user_name(e), statement_number(s));
	    pips_user_error("Illegal arithmetic pointer");
      }
    }
  }
    }
  }


  return res;
}

/* See the description of the property POINTS_TO_STRICT_POINTER_TYPES at
   pipsmake-rc.text
*/
set points_to_post_increment(statement s, expression lhs, set pts, list eff_list)
{
  set res = set_generic_make(set_private, points_to_equal_p,
			     points_to_rank);
  bool type_strict_p = !get_bool_property("POINTS_TO_STRICT_POINTER_TYPES");
  res = set_assign(res, pts);
  list new_inds = NIL;
  int i;

  if(expression_reference_p(lhs))  {
    reference r = expression_reference(lhs);
    cell c = make_cell_reference(r);
    SET_FOREACH(points_to, pt, pts)
      {
      cell pt_source = points_to_source(pt);
      if(points_to_compare_cell(c, pt_source))
	{
	cell pt_sink = points_to_sink(pt);
	reference ref_sink = cell_any_reference(pt_sink);
	entity ent_sink = reference_variable(ref_sink);
	    if( array_entity_p(ent_sink) ) {
	  list l_ind = reference_indices(ref_sink);
	  if(ENDP(l_ind)) {
	    int dim = variable_entity_dimension(reference_variable(ref_sink));
	    for(i = 0; i< dim; i++) {
	      expression new_ind = make_unbounded_expression();
	      l_ind = gen_nconc(CONS(EXPRESSION, new_ind, NIL),new_inds);
	    }
	  }

	      FOREACH( EXPRESSION, exp, l_ind ) {
	    if(expression_integer_constant_p(exp))
	      {
	      expression new_ind = make_unbounded_expression();
	      new_inds = gen_nconc(CONS(EXPRESSION, new_ind, NIL),new_inds);
	      }
	    else
	      new_inds = gen_nconc( l_ind, new_inds);
	    }
	  reference new_ref = make_reference(reference_variable(ref_sink),new_inds);
	  expression new_rhs = reference_to_expression(new_ref);
	  res =  set_assign(res, points_to_assignment( s, copy_expression(lhs), copy_expression(new_rhs), res ));
	  new_inds = NIL;
	  }
	    else if( type_strict_p ) {
	      res = set_assign(res, points_to_filter_with_effects(pts, eff_list));
	}
	    else {
	      entity e =  reference_variable(r);
	      pips_user_warning("Illegal arithmetic pointer %s at statement %d \n", entity_user_name(e), statement_number(s));
	      pips_user_error("Illegal arithmetic pointer");
      }
    }
      }
  }


  return res;
}



/* computing the points-to set of a while loop by iterating over its
   body until reaching a fixed-point. For the moment without taking
   into account the condition's side effect. */
set points_to_whileloop(whileloop wl, set pt_in, bool store __attribute__ ((__unused__))) {
  set pt_out = set_generic_make(set_private, points_to_equal_p,
				points_to_rank);
  set cur = set_generic_make(set_private, points_to_equal_p,
				 points_to_rank);
  entity anywhere = entity_undefined;
  statement while_body = whileloop_body(wl);
  expression cond = whileloop_condition(wl);
  int i = 0;
  int k = get_int_property("POINTS_TO_K_LIMITING");
  bool type_sensitive_p = !get_bool_property("ALIASING_ACROSS_TYPES");
  pt_in = set_assign(pt_in, points_to_expression(cond, pt_in, true));

  for(i = 0; i< k; i++){
    cur = set_assign(cur, set_clear(cur));
    cur = set_assign(cur, pt_in);
    set_clear(pt_out);
    set_assign(pt_out, points_to_recursive_statement(while_body,
						     cur,
						     false));
    SET_FOREACH(points_to, pt, pt_out){
      cell sc = points_to_source(pt);
      reference sr = cell_any_reference(sc);
      list sl = reference_indices(sr);
      cell kc = points_to_sink(pt);
      reference kr = cell_any_reference(kc);
      list kl = reference_indices(kr);
      if((int)gen_length(sl)>k){
	bool to_be_freed = false;
	type sc_type = cell_to_type(sc, &to_be_freed);
	if(type_sensitive_p){
	  anywhere =entity_all_xxx_locations_typed(ANYWHERE_LOCATION,
					 sc_type);
	} else
	  anywhere =entity_all_xxx_locations(ANYWHERE_LOCATION);

	reference r = make_reference(anywhere,NIL);
	sc = make_cell_reference(r);
	if (to_be_freed) free_type(sc_type);
      }
      if((int)gen_length(kl)>k){
	bool to_be_freed = false;
	type kc_type = cell_to_type(kc, &to_be_freed);
	if( type_sensitive_p )
	  anywhere = entity_all_xxx_locations_typed(ANYWHERE_LOCATION,
					 kc_type);
	else
	  anywhere = entity_all_xxx_locations(ANYWHERE_LOCATION);
	reference r = make_reference(anywhere,NIL);
	kc = make_cell_reference(r);
	if (to_be_freed) free_type(kc_type);
      }
      points_to npt = make_points_to(sc, kc, points_to_approximation(pt), make_descriptor_none());
      if(!points_to_equal_p(npt,pt)){
      pt_out = set_del_element(pt_out, pt_out, (void*)pt);
      pt_out = set_add_element(pt_out, pt_out, (void*)npt);
      }
    }
    set_assign(pt_in, set_clear(pt_in));
    set_assign(pt_in, merge_points_to_set(cur, pt_out));
    if(set_equal_p(cur, pt_in))
      break;
  }
  points_to_storage(pt_in, while_body , true);
  set_assign(pt_in, points_to_independent_store(pt_in));
  return pt_in;
}

/* computing the points to of a for loop, before processing the body,
   compute the points to of the initialization. */
set points_to_forloop(forloop fl, set pt_in, bool store __attribute__ ((__unused__))) {
  statement for_body = forloop_body(fl);
  set pt_out = set_generic_make(set_private, points_to_equal_p,
				points_to_rank);
  set cur = set_generic_make(set_private, points_to_equal_p,
				 points_to_rank);
  int i = 0;
  int k = get_int_property("POINTS_TO_K_LIMITING");
  entity anywhere = entity_undefined;
  bool type_sensitive_p = !get_bool_property("ALIASING_ACROSS_TYPES");
  expression exp_ini = forloop_initialization(fl);
  expression exp_cond = forloop_condition(fl);
  expression exp_inc = forloop_increment(fl);

  pt_in = points_to_expression(exp_ini, pt_in, true);
  pt_in = points_to_expression(exp_cond, pt_in, true);
  pt_in = points_to_expression(exp_inc, pt_in, true);

  for(i = 0; i< k; i++){
    cur = set_assign(cur, set_clear(cur));
    cur = set_assign(cur, pt_in);
    set_clear(pt_out);
    set_assign(pt_out, points_to_recursive_statement(for_body,
						     cur,
						     false));
    SET_FOREACH(points_to, pt, pt_out){
      cell sc = points_to_source(pt);
      reference sr = cell_any_reference(sc);
      list sl = reference_indices(sr);
      cell kc = points_to_sink(pt);
      reference kr = cell_any_reference(kc);
      list kl = reference_indices(kr);
      if((int)gen_length(sl)>k){
	bool to_be_freed = false;
	type sc_type = cell_to_type(sc, &to_be_freed);
	if(type_sensitive_p)
	   anywhere =entity_all_xxx_locations_typed(ANYWHERE_LOCATION,
							sc_type);
	else
	   anywhere =entity_all_xxx_locations(ANYWHERE_LOCATION);

	reference r = make_reference(anywhere,NIL);
	sc = make_cell_reference(r);
	if(to_be_freed) free_type(sc_type);
      }
      if((int)gen_length(kl)>k){
	bool to_be_freed = false;
	type kc_type = cell_to_type(kc, &to_be_freed);
	if(type_sensitive_p)
	 anywhere =entity_all_xxx_locations_typed(ANYWHERE_LOCATION,
							kc_type);
	else
	  anywhere =entity_all_xxx_locations(ANYWHERE_LOCATION);

	reference r = make_reference(anywhere,NIL);
	kc = make_cell_reference(r);
	if(to_be_freed) free_type(kc_type);
      }
      points_to npt = make_points_to(sc, kc, points_to_approximation(pt), make_descriptor_none());
      if(!points_to_equal_p(npt,pt)){
	pt_out = set_del_element(pt_out, pt_out, (void*)pt);
	pt_out = set_add_element(pt_out, pt_out, (void*)npt);
      }
    }
    set_assign(pt_in, set_clear(pt_in));
    set_assign(pt_in, merge_points_to_set(cur, pt_out));
    if(set_equal_p(cur, pt_in))
      break;
  }
  points_to_storage(pt_in, for_body , true);
  set_assign(pt_in, points_to_independent_store(pt_in));
  return pt_in;
}

/* computing the points to of a  loop. to have more precise
 * information, maybe should transform the loop into a do loop by
 * activating the property FOR_TO_DO_LOOP_IN_CONTROLIZER. */
set points_to_loop(loop fl, set pt_in, bool store __attribute__ ((__unused__))) {
  statement loop_body = loop_body(fl);
  set pt_out = set_generic_make(set_private, points_to_equal_p,
				points_to_rank);
  set cur = set_generic_make(set_private, points_to_equal_p,
				 points_to_rank);
  int i = 0;
  int k = get_int_property("POINTS_TO_K_LIMITING");
  entity anywhere = entity_undefined;
  bool type_sensitive_p = !get_bool_property("ALIASING_ACROSS_TYPES");
  for(i = 0; i< k; i++){
    cur = set_assign(cur, set_clear(cur));
    cur = set_assign(cur, pt_in);
    set_clear(pt_out);
    set_assign(pt_out, points_to_recursive_statement(loop_body,
						     cur,
						     false));
    SET_FOREACH(points_to, pt, pt_out){
      cell sc = points_to_source(pt);
      reference sr = cell_any_reference(sc);
      list sl = reference_indices(sr);
      cell kc = points_to_sink(pt);
      reference kr = cell_any_reference(kc);
      list kl = reference_indices(kr);
      if((int)gen_length(sl)>k){
	bool to_be_freed = false;
	type sc_type = cell_to_type(sc, &to_be_freed);
	if(type_sensitive_p)
	 anywhere =entity_all_xxx_locations_typed(ANYWHERE_LOCATION,
							sc_type);
	else
	  anywhere =entity_all_xxx_locations(ANYWHERE_LOCATION);

	reference r = make_reference(anywhere,NIL);
	sc = make_cell_reference(r);
	if(to_be_freed) free_type(sc_type);
      }
      if((int)gen_length(kl)>k){
	bool to_be_freed = false;
	type kc_type = cell_to_type(kc, &to_be_freed);
	if( type_sensitive_p )
	 anywhere =entity_all_xxx_locations_typed(ANYWHERE_LOCATION,
							kc_type);
	else
	  anywhere =entity_all_xxx_locations(ANYWHERE_LOCATION);

	reference r = make_reference(anywhere,NIL);
	kc = make_cell_reference(r);
	if(to_be_freed) free_type(kc_type);
      }
      points_to npt = make_points_to(sc, kc, points_to_approximation(pt), make_descriptor_none());
      if(!points_to_equal_p(npt,pt)){
	pt_out = set_del_element(pt_out, pt_out, (void*)pt);
	pt_out = set_add_element(pt_out, pt_out, (void*)npt);
      }
    }
    set_assign(pt_in, set_clear(pt_in));
    set_assign(pt_in, merge_points_to_set(cur, pt_out));
    if(set_equal_p(cur, pt_in))
      break;
  }
  points_to_storage(pt_in, loop_body, true);
   set_assign(pt_in, points_to_independent_store(pt_in));
  return pt_in;
}

/*Computing the points to of a do while loop, we have to process the
  body a least once, before iterating until reaching the fixed-point. */
set points_to_do_whileloop(whileloop fl, set pt_in, bool store __attribute__ ((__unused__))) {
  statement dowhile_body = whileloop_body(fl);
  set pt_out = set_generic_make(set_private, points_to_equal_p,
				points_to_rank);
  set cur = set_generic_make(set_private, points_to_equal_p,
				 points_to_rank);
  int i = 0;
  int k = get_int_property("POINTS_TO_K_LIMITING");
  expression cond = whileloop_condition(fl);
  set_assign(pt_in, points_to_recursive_statement(dowhile_body,
						   pt_in, false));
  for(i = 0; i< k; i++){
    cur = set_assign(cur, set_clear(cur));
    cur = set_assign(cur, pt_in);
    set_clear(pt_out);
    set_assign(pt_out, points_to_recursive_statement(dowhile_body,
						     cur,
						     false));
    SET_FOREACH(points_to, pt, pt_out){
      cell sc = points_to_source(pt);
      reference sr = cell_any_reference(sc);
      list sl = reference_indices(sr);
      cell kc = points_to_sink(pt);
      reference kr = cell_any_reference(kc);
      list kl = reference_indices(kr);
      if((int)gen_length(sl)>k){
	bool to_be_freed = false;
	type sc_type = cell_to_type(sc, &to_be_freed);
	entity anywhere =entity_all_xxx_locations_typed(ANYWHERE_LOCATION,
							sc_type);

	reference r = make_reference(anywhere,NIL);
	sc = make_cell_reference(r);

	if(to_be_freed) free_type(sc_type);
      }
      if((int)gen_length(kl)>k){
	bool to_be_freed = false;
	type kc_type = cell_to_type(sc, &to_be_freed);
	entity anywhere =entity_all_xxx_locations_typed(ANYWHERE_LOCATION,
							kc_type);

	reference r = make_reference(anywhere,NIL);
	kc = make_cell_reference(r);

	if(to_be_freed) free_type(kc_type);
      }
      points_to npt = make_points_to(sc, kc, points_to_approximation(pt), make_descriptor_none());
      if(!points_to_equal_p(npt,pt)){
	pt_out = set_del_element(pt_out, pt_out, (void*)pt);
	pt_out = set_add_element(pt_out, pt_out, (void*)npt);
      }
    }
    set_assign(pt_in, set_clear(pt_in));
    pt_out =  points_to_expression(cond, pt_out, false);
    set_assign(pt_in, merge_points_to_set(cur, pt_out));
    if(set_equal_p(cur, pt_in))
      break;
  }

  points_to_storage(pt_in, dowhile_body, true);
  set_assign(pt_in, points_to_independent_store(pt_in));
  return pt_in;

}

/*Computing the points to of a test, all the relationships are of type
  MAY, can be refined later by using preconditions. */
set points_to_test(test test_stmt, set pt_in, bool store) {
  statement true_stmt = statement_undefined;
  statement false_stmt = statement_undefined;
  set true_pts_to = set_generic_make(set_private, points_to_equal_p,
				     points_to_rank);
  set false_pts_to = set_generic_make(set_private, points_to_equal_p,
				      points_to_rank);
  set res = set_generic_make(set_private, points_to_equal_p,
				    points_to_rank);
  
  /* condition's side effect and information are taked into account :
     if(p==q) or if(*p++) */
  expression cond = test_condition(test_stmt);
  pt_in = set_assign(pt_in, points_to_expression(cond, pt_in, store));
  true_stmt = test_true(test_stmt);
  true_pts_to = set_assign(true_pts_to,
			   points_to_recursive_statement(true_stmt,
							 pt_in, true));
  false_stmt = test_false(test_stmt);
  if(empty_statement_p(false_stmt))
    false_pts_to = set_assign(false_pts_to, pt_in);
  else
  false_pts_to = set_assign(false_pts_to,
			    points_to_recursive_statement(false_stmt,
							 pt_in, true));
  
  res = merge_points_to_set(true_pts_to, false_pts_to);
  return res;
}

/* computing the points-to of a call, user_functions not yet implemented. */
set points_to_call(statement s, call c, set pt_in, bool store __attribute__ ((__unused__))) {
  entity e = call_function(c);
  cons *pc = call_arguments(c);
  tag tt;
  set pt_out = set_generic_make(set_private, points_to_equal_p,
				points_to_rank);
  set_assign(pt_in, points_to_init(s, pt_in));
  switch (tt = value_tag(entity_initial(e))) {
  case is_value_code:{
    /* call to an external function; preliminary version*/
    pips_user_warning("The function call to \"%s\" is ignored\n"
		      "On going implementation...\n", entity_user_name(e));
    set_assign(pt_out, pt_in);
  }
    break;
  case is_value_symbolic:
    set_assign(pt_out, pt_in);
    break;
  case is_value_constant:
    pt_out = set_assign(pt_out, pt_in);
    break;
  case is_value_unknown:
    pips_internal_error("function %s has an unknown value\n",
			entity_name(e));
    break;
  case is_value_intrinsic: {
    set_methods_for_proper_simple_effects();
    list el = call_to_proper_effects(c);
    generic_effects_reset_all_methods();
    pips_debug(5, "intrinsic function %s\n", entity_name(e));
    set_assign(pt_out,
		   points_to_intrinsic(s, c, e, pc, pt_in, el));
  }
    break;
  default:
    pips_internal_error("unknown tag %d\n", tt);
    break;
  }

  return pt_out;
}

/*We need call effects, which is not implemented yet, so we call
 * expression_to_proper_effects after creating an expression from the
 * call. Will be later moved at effects-simple/interface.c
 */
list call_to_proper_effects(call c) {
  expression e = call_to_expression(c);
  list el = expression_to_proper_effects(e);

  syntax_call( expression_syntax( e)) = call_undefined;
  free_expression(e);

  return el;
}

/* Process an expression, test if it's a call or a reference*/
set points_to_expression(expression e, set pt_in, bool store) {
  set pt_out = set_generic_make(set_private, points_to_equal_p,
				points_to_rank);
  call c = call_undefined;
  statement st = statement_undefined;
  syntax s = expression_syntax(copy_expression(e));
  switch (syntax_tag(s)) {
  case is_syntax_call: {
    c = syntax_call(s);
    st = make_expression_statement(e);
    set_assign(pt_out, points_to_call(st, c, pt_in, store));
  }
    break;
  case is_syntax_cast: {
    /* The cast is ignored, although it may generate aliases across
       types */
    cast ct = cast_undefined;
    expression e = expression_undefined;
    ct = syntax_cast(s);
    e = cast_expression(ct);
    st = make_expression_statement(e);
    set_assign(pt_out, points_to_expression(e, pt_in, store));
    break;
  }
  case is_syntax_reference: {
    set_assign(pt_out, pt_in);
    break;
  }

  case is_syntax_range: {
    set_assign(pt_out, pt_in);
    break;
  }
  case is_syntax_sizeofexpression: {
     set_assign(pt_out, pt_in);
    break;
  }
  case is_syntax_subscript: {
    set_assign(pt_out, pt_in);
    break;
  }
  case is_syntax_application: {
    set_assign(pt_out, pt_in);
    break;
  }
  case is_syntax_va_arg: {
    set_assign(pt_out, pt_in);
    break;
  }

  default:
    pips_internal_error("unexpected syntax tag (%d)", syntax_tag(s));
  }
  return pt_out;
}

/*
   For unstructured code, we ignore the statements order and we construct a
   sequence of statments from the entry and the exit control. This sequence 
   will be analyzed in an flow-insensitive way.
   After the analysis of the unstructured we have to change the exact points-to
   relations into may points-to.
*/
set points_to_unstructured(unstructured uns, set pt_in, bool __attribute__ ((__unused__))store){
  set pt_in_n = set_generic_make(set_private, points_to_equal_p,
				 points_to_rank);
  set out = set_generic_make(set_private, points_to_equal_p,
			     points_to_rank);
  list Pred_l = NIL, nodes_l = NIL ;
  list blocs = NIL ;
  set Pred =  set_make(set_pointer);
  set Nodes =  set_make(set_pointer);
  set Reachable =  set_make(set_pointer);
  set Processed =  set_make(set_pointer);
  set rtbp =  set_make(set_pointer);
  set rtbp_tmp =  set_make(set_pointer);
  set inter =  set_make(set_pointer);
  set tmp =  set_make(set_pointer);
  control entry = unstructured_control(uns) ;
  control exit = unstructured_exit(uns) ;
  Pred_l = control_predecessors(entry);
  Pred = set_assign_list(Pred, Pred_l);
 
  /* Get all the nodes of the unstructured*/
#if 0
  CONTROL_MAP(c, {
      ;
    }, entry, nodes_l) ;
#endif
  Nodes = set_assign_list(Nodes,nodes_l);

  /* Gel all the reachable nodes of the unstructured */
#if 0
  FORWARD_CONTROL_MAP(c, {
      ;
    }, entry, blocs) ;
#endif
  Reachable = set_assign_list(Reachable, blocs);

  bool inter_p = set_intersection_p(Reachable, Pred);
  if(!inter_p)
    set_add_element(rtbp, rtbp, (void*)entry);

  while(!set_empty_p(rtbp)) {
    rtbp_tmp = set_assign(rtbp_tmp,rtbp);
    SET_FOREACH(control, n , rtbp) {
      // to test the control's equality, I test if their statements are equals
      if ( statement_number(control_statement(n)) == statement_number(control_statement(entry)) ) {
	statement ss = control_statement(n);
	statement m = control_statement(entry);
	print_statement(ss);
	print_statement(m);
	pt_in_n = set_assign(pt_in_n, pt_in);
	out = set_assign(out, pt_in_n);
      }
      Pred_l = NIL;
      Pred_l = control_predecessors(n);
      set_clear(Pred);
      Pred = set_assign_list(Pred, Pred_l);
      inter = set_intersection(inter,Reachable, Pred);
      SET_FOREACH(control, p , inter){
	out = set_assign(out, points_to_recursive_control(p, out,
							    true));
	/* pt_in_n = set_union(pt_in_n, pt_in_n, out); */
      }
      out = set_assign(out, points_to_recursive_control(n, out/* pt_in_n */,
							  true));
      Processed = set_union(Processed, Processed, set_add_element(Processed,Processed, (void*)n ));
      rtbp_tmp = set_del_element(rtbp_tmp,rtbp_tmp,(void*)n);
      rtbp_tmp = set_union(rtbp_tmp,rtbp_tmp,ready_to_be_precessed_set(n, Processed, Reachable));
      set_clear(rtbp);
      rtbp = set_assign(rtbp,rtbp_tmp);
    }
    
  }

  tmp = set_difference(tmp, Nodes, Reachable);
  SET_FOREACH(control, cc, tmp) {
    pt_in_n = set_clear(pt_in_n);
    out = set_assign(out, points_to_recursive_control(cc, out,
							true));
  }

  free(blocs);
  free(nodes_l);
    statement exit_s = control_statement(exit);
  out = set_assign(out, points_to_recursive_statement(exit_s, out,
						      true)); 
  print_points_to_set("out apres unstructured", out);
  return out;
}


/* A node is ready to be processed if its predecessors are not reachable or processed */
bool Ready_p(control c, set Processed, set Reachable)
{ 
  set Pred =  set_make(set_pointer);
  list Pred_l = control_predecessors(c);
  Pred = set_assign_list(Pred, Pred_l);
  bool ready_p = false;
  SET_FOREACH(control, p , Pred) {
    ready_p = set_belong_p(Processed,(void*) p) || !set_belong_p(Reachable,(void*) p); 
  }
  return ready_p;
}


/* A set containing all the successors of n that are ready to be processed */
set ready_to_be_precessed_set(control n, set Processed, set Reachable)
{
   set Succ =  set_make(set_pointer);
   set rtbp =  set_make(set_pointer);
   list Succ_l = control_successors(n);
   Succ = set_assign_list(Succ, Succ_l);
   SET_FOREACH(control, p , Succ){
     if(Ready_p(p, Processed, Reachable))
       set_add_element(rtbp, rtbp, (void*)p);
   }
   return rtbp;
}

set points_to_recursive_control(control c, set in, bool store)
{
  set out = set_generic_make(set_private, points_to_equal_p,
			     points_to_rank);
  set out1 = set_generic_make(set_private, points_to_equal_p,
			      points_to_rank);
  set out2 = set_generic_make(set_private, points_to_equal_p,
			      points_to_rank);
  out = set_assign(out, in);
  statement s = control_statement(c);
  if(statement_test_p(s)) {
    list Succ_l = control_successors(c);
    if( (int)gen_length(Succ_l) > 1 ) {
      control true_c = CONTROL(CAR(Succ_l));
      control false_c = CONTROL(CAR(CDR(Succ_l)));
      out =  points_to_recursive_statement(s, in,  true);
      out1 =  points_to_recursive_statement(control_statement(true_c), out,  true);
      out2 =  points_to_recursive_statement(control_statement(false_c), out,  true);
      out = merge_points_to_set(out1, out2);
    }
    else
      out =  points_to_recursive_statement(s, in,  true);
  }
  else
    out =  points_to_recursive_statement(s, in,  true);

  return out;
}

/* For debug*/
void print_control_set(set s)
{
  SET_FOREACH(control, c, s) {
    print_control_node(c);
  }
}

set points_to_goto(__attribute__ ((__unused__))statement current, set pt_in,__attribute__ ((__unused__)) bool store){
 /*  set_assign( pt_in, points_to_recursive_statement(current, pt_in, true)); */
  return pt_in;

}

/* Process recursively statement "current". Use pt_list as the input
   points-to set. Save it in the statement mapping. Compute and return
   the new points-to set which holds after the execution of the statement. */
set points_to_recursive_statement(statement current, set pt_in, bool store) {
  set pt_out = set_generic_make(set_private, points_to_equal_p,
				points_to_rank);
  list dl = NIL;
  set_assign(pt_out, pt_in);
  instruction i = statement_instruction(current);
  points_to_storage(pt_in, current, store);
  
  ifdebug(1) print_statement(current);
  switch (instruction_tag(i)){
      /* instruction = sequence + test + loop + whileloop +
	 goto:statement +
	 call + unstructured + multitest + forloop  + expression ;*/

  case is_instruction_call: {
    set_assign(pt_out, points_to_call(current,instruction_call(i), pt_in, store));
  }
    break;
  case is_instruction_sequence: {
    set_assign(pt_out, points_to_sequence(instruction_sequence(i),pt_in, store));
  }
    break;
  case is_instruction_test: {
    set_assign(pt_out, points_to_test(instruction_test(i), pt_in,
				      store));
    statement true_stmt = test_true(instruction_test(i));
    statement false_stmt = test_false(instruction_test(i));
    if(statement_block_p(true_stmt) && !ENDP(dl=statement_declarations(true_stmt))){
      pt_out = points_to_block_projection(pt_out, dl);
      pt_out = merge_points_to_set(pt_out, pt_in);
    }
    if(statement_block_p(false_stmt) && !ENDP(dl=statement_declarations(false_stmt))){
      pt_out = points_to_block_projection(pt_out, dl);
      pt_out = merge_points_to_set(pt_out, pt_in);
     }
  }
    break;
  case is_instruction_whileloop: {
    store = false;
    if (evaluation_tag(whileloop_evaluation(instruction_whileloop(i))) == 0)
      set_assign(pt_out, points_to_whileloop(
					     instruction_whileloop(i), pt_in, store));
    else
      set_assign(pt_out, points_to_do_whileloop(
						instruction_whileloop(i), pt_in, store));
    
      statement ws = whileloop_body(instruction_whileloop(i));
      if(statement_block_p(ws) && !ENDP(dl=statement_declarations(ws)))
	pt_out = points_to_block_projection(pt_out, dl);
  }
    break;
  case is_instruction_loop: {
    store = false;
    set_assign(pt_out, points_to_loop(instruction_loop(i), pt_in, store));
    statement ls = loop_body(instruction_loop(i));
      if(statement_block_p(ls) && !ENDP(dl=statement_declarations(ls)))
	pt_out = points_to_block_projection(pt_out, dl);
  }
    break;
  case is_instruction_forloop: {
    store = false;
    set_assign(pt_out, points_to_forloop(instruction_forloop(i),
					 pt_in, store));
    statement ls = forloop_body(instruction_forloop(i));
    if(statement_block_p(ls) && !ENDP(dl=statement_declarations(ls)))
      pt_out = points_to_block_projection(pt_out, dl);
  }
    break;
  case is_instruction_expression: {
    set_assign(pt_out, points_to_expression(
					    instruction_expression(i), pt_in, store));
  }
    break;
  case is_instruction_unstructured: {
    set_assign(pt_out, points_to_unstructured(instruction_unstructured(i), pt_in, false));
    points_to_storage(pt_out,current,true);
  }
    break;
  case is_instruction_goto:{
    store = false;
    set_assign(pt_out, points_to_goto(current, pt_in, false));
  }
    break;
  default:
    pips_internal_error("Unexpected instruction tag %d\n", instruction_tag(
									   i));
    break;
  }
 
  return pt_out;
}
/* Entry point: intialize the entry poitns-to set; intraprocedurally,
   it's an empty set. */
void points_to_statement(statement current, set pt_in) {
  set pt_out = set_generic_make(set_private, points_to_equal_p,
				points_to_rank);
  set_assign(pt_out, points_to_recursive_statement(current, pt_in,
							    true));
  set_assign(pts_to_out, pt_out);
}

set points_to_init(statement s, set pt_in)
{
  set pt_out = set_generic_make(set_private, points_to_equal_p,
				points_to_rank);
  list l = NIL;
  bool type_sensitive_p = !get_bool_property("ALIASING_ACROSS_TYPES");
  set_assign(pt_out, pt_in);
  /* test if the statement is a declaration. */
  if (c_module_p(get_current_module_entity()) &&
      (declaration_statement_p(s))) {
    list l_decls = statement_declarations(s);
    pips_debug(1, "declaration statement \n");

    FOREACH(ENTITY, e, l_decls){
      if(type_variable_p(entity_type(e))){
	if( !storage_rom_p(entity_storage(e)) ){
	value v_init = entity_initial(e);
	/* generate points-to due to the initialisation */
	if(value_expression_p(v_init)){
	  expression exp_init = value_expression(v_init);
	  expression lhs = entity_to_expression(e);
	  if(expression_pointer_p(lhs))
	    pt_out = set_assign(pt_out,points_to_assignment(s,
							    copy_expression(lhs),
							    copy_expression(exp_init),
								  pt_out));
	}
	else{
	  l = points_to_init_variable(e);
	  FOREACH(CELL, cl, l){
	    list l_cl = CONS(CELL, cl, NIL);
	    if(type_sensitive_p)
	    set_union(pt_out, pt_out, points_to_nowhere_typed(l_cl, pt_out));
	    else
	      set_union(pt_out, pt_out, points_to_nowhere(l_cl, pt_out));
	  }
	}
      }
    }
  }
  }
  /* } */

  return pt_out;
}

list points_to_init_variable(entity e){
  list l = NIL;
  expression ex = entity_to_expression(e);

  if(entity_variable_p(e)){
    basic b = variable_basic(type_variable(entity_type(e)));
    if(expression_pointer_p(ex)){
      reference r = make_reference(e, NIL);
      cell c = make_cell_reference(r);
      l = CONS(CELL, c, NIL);
    }else if(entity_array_p(e)){
      if(basic_pointer_p(b)){
	expression ind = make_unbounded_expression();
	reference r = make_reference(e, CONS(EXPRESSION, ind, NULL));
	cell c = make_cell_reference(r);
	 l = CONS(CELL, c, NIL);
      }
    }else if(basic_derived_p(b)){
      entity ee = basic_derived(b);
      type t = entity_type(ee);
      if(type_struct_p(t)){
	l = points_to_init_struct(e, ee);
      }
    }else if(basic_typedef_p(b)){
      l = points_to_init_typedef(e);
    }
  }
  return l;
}

list points_to_init_pointer_to_typedef(entity e){
  list l = NIL;
  type t1 = ultimate_type(entity_type(e));
 
  if(type_variable_p(t1)){
    basic b = variable_basic(type_variable(entity_type(e)));
    if(basic_pointer_p(b)){
      type t2 = basic_pointer(variable_basic(type_variable(t1)));
      if(typedef_type_p(t2)){
	basic b1 = variable_basic(type_variable(t2));
	  if(basic_typedef_p(b1)){
	    entity e2  = basic_typedef(b1);
	    if(entity_variable_p(e2)){
	      basic b2 = variable_basic(type_variable(entity_type(e2)));
	      if(basic_derived_p(b2)){
		entity e3 = basic_derived(b2);
		l = points_to_init_pointer_to_derived(e, e3);
	      }
	    }
	  }
	}
      else if(derived_type_p(t2)){
	entity e3 = basic_derived(variable_basic(type_variable(t2)));
	l = points_to_init_pointer_to_derived(e, e3);
      }
    }
  }

  return l;
}

list points_to_init_pointer_to_derived(entity e, entity ee){
  list l = NIL;
  type tt = entity_type(ee);

  if(type_struct_p(tt))
   l = points_to_init_pointer_to_struct(e, ee);
  else if(type_union_p(tt))
    pips_user_warning("union case not handled yet \n");
  else if(type_enum_p(tt))
    pips_user_warning("union case not handled yet \n");
  return l;
}



list  points_to_init_pointer_to_struct(entity e, entity ee){
  list l = NIL;
  bool  eval = true;
  type tt = entity_type(ee);
  expression ex = entity_to_expression(e);

  if(type_struct_p(tt)){
    list l1 = type_struct(tt);
    if(!array_argument_p(ex)){
      FOREACH(ENTITY, i, l1){

	expression ef = entity_to_expression(i);
	if(expression_pointer_p(ef)){
	  expression ex1 = MakeBinaryCall(entity_intrinsic(POINT_TO_OPERATOR_NAME),
					  ex,
					  ef);
	  cell c = get_memory_path(ex1, &eval);
	  l = gen_nconc(CONS(CELL, c, NIL),l);
	}
      }
    }
    else
      l = points_to_init_array_of_struct(e, ee);
  }

  return l;
}

list points_to_init_typedef(entity e){
  list l = NIL;
   type t1 = entity_type(e);
    
  if(type_variable_p(t1)){
    basic b = variable_basic(type_variable(entity_type(e)));
    tag tt = basic_tag(b);
    switch(tt){
    case is_basic_int:;
      break;
    case is_basic_float:;
      break;
    case is_basic_logical:;
      break;
    case is_basic_overloaded:;
      break;
    case is_basic_complex:;
      break;
    case is_basic_string:;
      break;
    case is_basic_bit:;
      break;
    case is_basic_pointer:;
      break;
    case is_basic_derived:{
      bool  eval = true;
      type t = entity_type(e);
      expression ex = entity_to_expression(e);
      if(type_struct_p(t)){
	list l1 = type_struct(t);
	FOREACH(ENTITY, i, l1){
	  expression ef = entity_to_expression(i);
	  if(expression_pointer_p(ef)){
	    expression ex1 = MakeBinaryCall(entity_intrinsic(FIELD_OPERATOR_NAME),
					    ex,
					    ef);
	    cell c = get_memory_path(ex1, &eval);
	    l = gen_nconc(CONS(CELL, c, NIL),l);
	  }else if(array_argument_p(ef)){
	    /* print_expression(ef); */;
	}
	}
      }else if(type_enum_p(t))
	pips_user_warning("enum case not handled yet \n");
      else if(type_union_p(t))
	pips_user_warning("union cas not handled yet \nx");
    }
      break;
    case is_basic_typedef:
      {
	entity e1  = basic_typedef(b);
	type t1 = entity_type(e1);
	if(entity_variable_p(e1)){
	  basic b2 =  variable_basic(type_variable(t1));
	  if(basic_derived_p(b2)){
	    entity e2  = basic_derived(b2);
	    l = points_to_init_derived(e, e2);
	  }
	}
      }

      break;
    default: pips_internal_error("unexpected tag %d\n", tt);
      break;
    }
  }
  return l;
}


list points_to_init_derived(entity e, entity ee){
  list l = NIL;
  type tt = entity_type(ee);

  if(type_struct_p(tt))
   l = points_to_init_struct(e, ee);
  return l;
}



list  points_to_init_struct(entity e, entity ee){
  list l = NIL;
  bool  eval = true;
  type tt = entity_type(ee);
  expression ex = entity_to_expression(e);

  if(type_struct_p(tt)){
    list l1 = type_struct(tt);
    if(!array_argument_p(ex)){
      FOREACH(ENTITY, i, l1){

	expression ef = entity_to_expression(i);
	if(expression_pointer_p(ef)){
	  expression ex1 = MakeBinaryCall(entity_intrinsic(FIELD_OPERATOR_NAME),
					  ex,
					  ef);
	  cell c = get_memory_path(ex1, &eval);
	  l = gen_nconc(CONS(CELL, c, NIL),l);
	}
	else if(array_argument_p(ef)){
	  basic b = variable_basic(type_variable(entity_type(i)));
	  /* arrays of pointers are changed into independent store arrays
	     and initialized to nowhere_b0 */
	  if(basic_pointer_p(b)){
	    effect eff = effect_undefined;
	    expression ind = make_unbounded_expression();
	    expression ex1 = MakeBinaryCall(entity_intrinsic(FIELD_OPERATOR_NAME),
					    ex,
					    ef);
	    set_methods_for_proper_simple_effects();
	    list l_ef = NIL;
	    list l1 = generic_proper_effects_of_complex_address_expression(ex1, &l_ef,
								       true);
	    eff = EFFECT(CAR(l_ef)); /* In fact, there should be a FOREACH to scan all elements of l_ef */
	    gen_free_list(l_ef);
	    reference r2 = effect_any_reference(eff);
	    effects_free(l1);
	    generic_effects_reset_all_methods();
	    reference_indices(r2)= gen_nconc(reference_indices(r2), CONS(EXPRESSION, ind, NIL));
	    cell c = make_cell_reference(r2);
	    l = gen_nconc(CONS(CELL, c, NIL),l);
	  }
	}
      }
    }
    else
      l = points_to_init_array_of_struct(e, ee);
  }

  return l;
}

list points_to_init_array_of_struct(entity e, entity field){
  list l = NIL;
  bool eval = true;
  type tt = entity_type(field);
  if(type_struct_p(tt)){
    list l1 = type_struct(tt);
    FOREACH(ENTITY, i, l1) {
      expression ee = entity_to_expression(i);
      if(expression_pointer_p(ee)){
	expression ex = entity_to_expression(e);
	if(array_argument_p(ex)){
	  effect ef = effect_undefined;
	  reference  r = reference_undefined;
	  /*init the effect's engine*/
	  set_methods_for_proper_simple_effects();
	  list l_ef = NIL;
	  list l1 = generic_proper_effects_of_complex_address_expression(ex, &l_ef,
									 true);
	  ef = EFFECT(CAR(l_ef)); /* In fact, there should be a FOREACH to scan all elements of l_ef */
	  gen_free_list(l_ef); /* free the spine */

	  effect_add_dereferencing_dimension(ef);
	  r = effect_any_reference(ef);
	  list l_inds = reference_indices(r);
	  EXPRESSION_(CAR(l_inds)) = make_unbounded_expression();
	  ex = reference_to_expression(r);
	  effects_free(l1);
	  generic_effects_reset_all_methods();
	}
	expression ex1 = MakeBinaryCall(entity_intrinsic(FIELD_OPERATOR_NAME),
					ex,
					ee);
	cell c = get_memory_path(ex1, &eval);
	l = gen_nconc(CONS(CELL, c, NIL), l);
      }
    }
  }
  else
    pips_internal_error("type struct expected\n");
      
  return l;
}

bool points_to_analysis(char * module_name) {
  entity module;
  statement module_stat;
  set pt_in =
    set_generic_make(set_private, points_to_equal_p, points_to_rank);
  list pts_to_list = NIL;
  
  pts_to_in = set_generic_make(set_private, points_to_equal_p, points_to_rank);
  pts_to_out = set_generic_make(set_private, points_to_equal_p, points_to_rank);
 
  init_pt_to_list();
  module = module_name_to_entity(module_name);
  set_current_module_entity(module);
  make_effects_private_current_context_stack();

  debug_on("POINTS_TO_DEBUG_LEVEL");

  pips_debug(1, "considering module %s\n", module_name);
  set_current_module_statement((statement) db_get_memory_resource(DBR_CODE,
								  module_name, true));
  module_stat = get_current_module_statement();
  statement_consistent_p(module_stat);

  /*
    Get the summary_intraprocedural_points_to resource.
    This list contains formal paramters and their stub sinks
  */
  points_to_list summary_pts_to_list =
    (points_to_list) db_get_memory_resource(DBR_SUMMARY_POINTS_TO_LIST,
					    module_name, true);
  /* Transform the list of summary_points_to in set of points-to.*/
    points_to_list_consistent_p(summary_pts_to_list);
  pts_to_list = gen_full_copy_list(points_to_list_list(summary_pts_to_list));
  pt_in = set_assign_list(pt_in, pts_to_list);

    /* Initialize pts_to_in with formal parameters pts-to and add global variables
     as to the computation of their sinks. 
    */
    pts_to_in = set_assign(pts_to_in, pt_in);
    
  /* Compute the points-to relations using the summary_points_to as input.*/
  points_to_statement(module_stat, pt_in);
    /* statement_points_to_consistent_p(get_pt_to_list()); */
    /* Save points-to relations*/
  DB_PUT_MEMORY_RESOURCE(DBR_POINTS_TO_LIST, module_name, get_pt_to_list());

    /* Filter OUT points-to by deleting local variables */

    /* if( ! ENDP(dl = code_declarations(value_code(entity_initial(module)))) ) */
    
    pts_to_out = points_to_function_projection(pts_to_out);
    /* else */
    /*   pts_to_out = set_assign(pts_to_out, pt_in); */

    /* Save IN points-to relations */
    list  l_in = set_to_list(pts_to_in);
    points_to_list in_list = make_points_to_list(l_in);
    DB_PUT_MEMORY_RESOURCE(DBR_POINTS_TO_IN, module_name, in_list);

    /* Save OUT points-to relations */
    list  l_out = set_to_list(pts_to_out);
    points_to_list out_list = make_points_to_list(l_out);
    DB_PUT_MEMORY_RESOURCE(DBR_POINTS_TO_OUT, module_name, out_list);

  reset_pt_to_list();
  reset_current_module_entity();
  reset_current_module_statement();
  reset_effects_private_current_context_stack();
  debug_off();
  bool good_result_p = true;

  return (good_result_p);
}



