/* static bool rule_multi_produced_consistent_p(rule mp_rule, makefile make_file)
 * input    : a rule that produces more than one resource and a the current makefile
 * output   : TRUE if the others rules of the make file that produce at least
 *            one of the resource that mp_rule produces, produce exactly the
 *            same resources, or if no other rule produces the same resources. 
 *            FALSE otherwise.
 * modifies : nothing
 * comment  :	
 */
static bool rule_multi_produced_consistent_p(mp_rule, make_file)
rule mp_rule;
makefile make_file;
{
    static bool rule_produced_consistent_p(rule rule_1, rule rule_2);
    list all_rules = makefile_rules(make_file);
    rule c_rule = rule_undefined;
    bool consistent = TRUE;
    
    
    while (consistent && !ENDP(all_rules)) {
	c_rule = RULE(CAR(all_rules));

	if ( (c_rule != mp_rule)
	    && !rule_produced_consistent_p(mp_rule, c_rule))
	    consistent = FALSE;
	all_rules = CDR(all_rules);
    }

    return(consistent);

}


/* static bool rule_produced_consistent_p(rule rule_1, rule_2)
 * input    : two rules
 * output   : TRUE if they produce exactly the same resources, or if
 *            they produce no common resource. FALSE otherwise.
 * modifies : nothing.
 * comment  :	
 */
static bool rule_produced_consistent_p(rule_1, rule_2)
rule rule_1, rule_2;
{
    list l_prod1 = rule_produced(rule_1);
    list l_prod2 = rule_produced(rule_2);
    list l2;
    bool consistent = TRUE;
    bool first_common = TRUE;
    bool first = TRUE;
    bool same_length = (gen_length(l_prod1) == gen_length(l_prod2));
    string vr1, vr2;

    while (consistent && !ENDP(l_prod1)) {
	
	bool found = FALSE;
	vr1 = virtual_resource_name(VIRTUAL_RESOURCE(CAR(l_prod1)));
	l2 = l_prod2;

	while (!found && !ENDP(l2)) {
	    vr2 = virtual_resource_name(VIRTUAL_RESOURCE(CAR(l2)));
	    if (same_string_p(vr1,vr2))
		found = TRUE;
	    else
		l2 = CDR(l2);
	}

	
	if (first && !found )  first_common = FALSE;
	if (first) first = FALSE;
 
	consistent = (first_common && found && same_length) || (!first_common && !found);

	l_prod1 = CDR(l_prod1);
    }
    return(consistent);
}


