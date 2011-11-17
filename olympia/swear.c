
#include	<stdio.h>
#include	<stdlib.h>
#include	"z.h"
#include	"oly.h"



int
lord(int n)
{

	if (kind(n) == T_player)
		return n;

	return rp_char(n) ? rp_char(n)->unit_lord : 0;
}


int
player(int n)
{
#if 1
	int loop_check = 5000;
#endif

	while (n && kind(n) != T_player)
	{
		n = lord(n);
#if 1
		assert(loop_check-- > 0);
#endif
	}

	return n;
}



void
set_loyal(int who, int k, int lev)
{
	struct entity_char *p;

	p = p_char(who);

	p->loy_kind = k;
	p->loy_rate = lev;
}


/*
 *  is b sworn somewhere beneath a?
 */

int
sworn_beneath(int a, int b)
{

	if (a == b)
		return FALSE;

	while (kind(b) != T_player)
	{
		b = lord(b);
		if (a == b)
			return TRUE;
	}

	return FALSE;
}


static void
reswear_all_sworn(int who)
{
	int pl = player(who);
	int i;
	int new_lord = lord(who);

	loop_units(pl, i)
	{
		if (lord(i) == who)
			set_lord(i, new_lord, LOY_UNCHANGED, 0);
	}
	next_unit;
}


void
set_lord(int who, int new_lord, int k, int lev)
{
	int old_pl;
	int new_pl;
	extern int int_comp();
	int flag = FALSE;
	int prev_lord;

	old_pl = player(who);

	if (new_lord)
		new_pl = player(new_lord);
	else
		new_pl = 0;

	if (old_pl && old_pl != new_pl)
	{
		flush_unit_orders(old_pl, who);
		interrupt_order(who);
		clear_all_att(who);
		reswear_all_sworn(who);

		if (char_pledge(who))
			p_magic(who)->pledge = 0;

		ilist_rem_value(&p_player(old_pl)->units, who);
	}

	prev_lord = p_char(who)->prev_lord;
	if (prev_lord == 0)
		prev_lord = old_pl;

	if (new_pl && old_pl != new_pl)
		p_char(who)->prev_lord = old_pl;

	p_char(who)->unit_lord = new_lord;

	if (new_lord && new_pl != old_pl)
	{
		ilist_append(&p_player(new_pl)->units, who);

		qsort(p_player(new_pl)->units,
				ilist_len(p_player(new_pl)->units),
				sizeof(int),
				int_comp);

		init_load_sup(who);	/* load command from new owner */
		touch_loc(who);
	}

/*
 *  NOTYET: figure out who pops out of what stack
 *	perhaps they don't?  Just ignore it?
 *	no auto-unstack?  Have to manually force them out?
 */

	/*
	 *  player -> player	NP cost and return
	 *  player -> indep	no return, no cost
	 *  indep  -> player	charge cost, return to NP prev owner
	 */

	if (old_pl == indep_player)
		old_pl = prev_lord;

	if (old_pl && new_pl && old_pl != new_pl && new_pl != indep_player)
	{
		if (kind(old_pl) == T_player)
		{
			int nps = char_np_total(who);
			add_np(old_pl, nps);
			wout(old_pl, "Received %d NP%s for %s.", nps,
					add_s(nps), box_name(who));
		}

		flag = TRUE;
	}

	if (k != LOY_UNCHANGED)
		set_loyal(who, k, lev);

	if (flag)
	{
		int nps = char_np_total(who);
		if (!deduct_np(new_pl, nps))
#if 0
			assert(FALSE);
#else
		log_write(LOG_CODE, "assert fail: new_pl=%s, who=%s, nps=%d, old_pl=%s", box_code(new_pl), box_code(who), nps, box_code(old_pl));
#endif
		wout(new_pl, "Paid %d NP%s for %s.", nps, add_s(nps),
					box_name(who));
	}
}


int
np_to_acquire(int who, int target)
{

	if (player(target) == indep_player &&
	    p_char(target)->prev_lord == player(who))
		return 0;

	return char_np_total(target);
}


int
enough_np_to_acquire(int who, int target)
{
	int nps = np_to_acquire(who, target);

	if (player_np(player(who)) < nps)
	{
		wout(who, "Don't have %d NP%s to take control of %s.",
				nps, add_s(nps), box_name(target));
		return FALSE;
	}

	return TRUE;
}



int
v_swear(struct command *c)
{
	int target = c->a;
	int targ_lord, targ_pl;
	int old_lord, old_pl;

#if 1
	out(c->who, "The SWEAR order has been removed from the game.");
	return FALSE;
#endif

	if (target == 0 && numargs(c) > 0 && *c->parse[1] == '0')
	{
		unit_deserts(c->who, indep_player, FALSE, LOY_unsworn, 0);
		return TRUE;
	}

	if (numargs(c) <= 0)
	{
		wout(c->who, "Must specify a character to swear fealty to.");
		return FALSE;
	}

	if (!check_char_here(c->who, target))
		return FALSE;

	if (sworn_beneath(c->who, target))
	{
		wout(c->who, "Cannot swear to a character beneath "
			"you in the command hierarchy.");
		return FALSE;
	}

	old_lord = lord(c->who);
	old_pl = player(old_lord);

	targ_lord = lord(target);
	targ_pl = player(targ_lord);

	if (old_lord == targ_lord)
	{
		wout(c->who, "Already sworn to %s.", box_name(old_lord));
		return FALSE;
	}

	if (rp_player(old_pl)->swear_this_turn)
	{
		wout(c->who, "Allowed at most one SWEAR per turn.");
		return FALSE;
	}

	rp_player(old_pl)->swear_this_turn++;

	if (old_pl != targ_pl)
	{
		wout(old_pl, "%s renounces loyalty.", box_name(c->who));
		wout(targ_pl, "%s swears loyalty.", box_name(c->who));
	}

	wout(target, "%s swears loyalty to us.", box_name(c->who));

	{
		show_day = FALSE;
		out(c->who, "");
		show_day = TRUE;
	}

	set_lord(c->who, targ_lord, LOY_UNCHANGED, 0);

	return TRUE;
}


int
is_unit(int pl, int v)
{

	assert(kind(pl) == T_player);

	return ilist_lookup(p_player(pl)->units, v) >= 0;
}


void
unit_deserts(int who, int to_who, int loy_check, int k, int lev)
{
	int sp = player(who);

	if (to_who && sp)
	{
		wout(sp, "%s renounces loyalty to us.", box_name(who));
		wout(who, "%s renounces loyalty.", box_name(who));
		show_day = FALSE;
		out(who, "");
		show_day = TRUE;
	}

/*
 *  If a prisoner deserts to the faction of the unit holding it
 *  prisoner, don't extract it from the stack.  Instead, simply
 *  clear the prisoner bit.
 */ 

	if (to_who &&
	    is_prisoner(who) &&
	    player(to_who) == player(stack_parent(who)))
	{
		p_char(who)->prisoner = FALSE;
	}
	else if (!is_prisoner(who))
	{
		extract_stacked_unit(who);
	}

	set_lord(who, to_who, k, lev);

	if (to_who)
	{
		wout(who, "%s pledges fealty to us.", box_name(who));
		wout(to_who, "%s pledges fealty to us.", box_name(who));

		p_char(who)->new_lord = 1;
	}
}


int
v_bribe(struct command *c)
{
	int target = c->a;
	int amount = c->b;

	if (has_skill(c->who, sk_bribe_noble) < 1)
	{
		wout(c->who, "BRIBE requires knowledge of %s.",
				cap(box_name(sk_bribe_noble)));
		return FALSE;
	}

	if (!check_char_here(c->who, target))
		return FALSE;

	if (char_new_lord(target))
	{
		wout(c->who, "%s just switched employers this month, and is "
			"not looking for a new one so soon.", box_name(target));
		return FALSE;
	}

	if (is_npc(target))
	{
		wout(c->who, "NPC's cannot be bribed.");
		return FALSE;
	}

	if (player(target) == player(c->who))
	{
		wout(c->who, "%s already belongs to our faction.",
				box_name(target));
		return FALSE;
	}

	if (amount == 0)
	{
		wout(c->who, "Must specify an amount of gold to use as "
					"a bribe.");
		return FALSE;
	}

	if (!can_pay(c->who, amount))
	{
		wout(c->who, "Don't have %s for a bribe.", gold_s(amount));
		return FALSE;
	}

	wout(c->who, "Attempt to bribe %s with a gift of %s.",
			box_name(target), gold_s(amount));

	return TRUE;
}


static void
thanks_for_gift(int who, int target)
{

	switch (rnd(1,3))
	{
	case 1:
		wout(who, "%s graciously accepts our gift.",
					box_name(target));
		break;

	case 2:
		wout(who, "%s thanks us for the gift.",
						box_name(target));
		break;

	case 3:
		wout(who, "%s pockets the gold.", box_name(target));
		break;

	default:
		assert(FALSE);
	}
}


/*
 *	over threshold			under threshold
 *	--------------			---------------
 *	35%	switch			50% pocket
 *	30%	pocket			50% report
 *	25%	report bribe
 *	10%	go independent
 */

#define		SWITCH		1
#define		POCKET		2
#define		REPORT		3
#define		HEAD_FOR_HILLS	4

int
d_bribe(struct command *c)
{
	int target = c->a;
	int amount = c->b;
	int flag = c->c;
	int bribe_thresh = 0;
	int outcome;

	if (!check_still_here(c->who, target))
		return FALSE;

	if (char_new_lord(target))
	{
		wout(c->who, "%s just switched employers this month, and is "
			"not looking for a new one so soon.", box_name(target));
		return FALSE;
	}

	if (!charge(c->who, amount))
	{
		wout(c->who, "Don't have %s for a bribe.", gold_s(amount));
		return FALSE;
	}

	switch (loyal_kind(target))
	{
	case LOY_unsworn:
	case LOY_contract:
		bribe_thresh = loyal_rate(target);
		if (bribe_thresh < 250)
			bribe_thresh = 250;
		break;

	case LOY_fear:
		bribe_thresh = 250;
		break;

	case LOY_oath:
		break;

	default:
		assert(FALSE);
	}

	if (bribe_thresh <= 0 || amount < bribe_thresh)
	{
		if (rnd(1,2) == 1)
			outcome = POCKET;
		else
			outcome = REPORT;
	}
	else
	{
		int n = rnd(1,100);

		if (n <= 35)
			outcome = SWITCH;
		else if (n <= 65)
			outcome = POCKET;
		else if (n <= 90)
			outcome = REPORT;
		else
			outcome = HEAD_FOR_HILLS;
	}

	if (outcome == SWITCH && !enough_np_to_acquire(c->who, target))
	{
		outcome = POCKET;
	}

	switch (outcome)
	{
	case SWITCH:
		wout(c->who, "%s accepts the gift, and has decided "
				"to join us.", box_name(target));
		unit_deserts(target, player(c->who), TRUE, LOY_contract, 250);
		p_char(target)->fresh_hire = TRUE;

		if (flag)
			join_stack(target, c->who);
		break;

	case HEAD_FOR_HILLS:
		thanks_for_gift(c->who, target);
		wout(c->who, "%s left the service of %s, but didn't "
				"join us.", box_name(target),
				box_name(player(target)));
		unit_deserts(target, indep_player, TRUE, LOY_unsworn, 0);
		break;

	case POCKET:
		thanks_for_gift(c->who, target);
		break;

	case REPORT:
		thanks_for_gift(c->who, target);
		gen_item(c->who, item_gold, amount);
		wout(target, "%s tried to bribe us with %s.",
				box_name(c->who), gold_s(amount));
		break;

	default:
		assert(FALSE);
	}

	return TRUE;
}


int
v_honor(struct command *c)
{
	int amount = c->a;

	if (amount == 0)
	{
		wout(c->who, "Must specify an amount of gold to use as "
					"a gift.");
		return FALSE;
	}

	if (loyal_kind(c->who) == LOY_oath)
	{
		wout(c->who, "%s graciously declines the offer.",
				box_name(c->who));
		return FALSE;
	}

	if (!charge(c->who, amount))
	{
		wout(c->who, "Do not have %s.", gold_s(amount));
		return FALSE;
	}

	if (loyal_kind(c->who) != LOY_contract)
	{
		p_char(c->who)->loy_kind = LOY_contract;
		p_char(c->who)->loy_rate = 0;
	}

	p_char(c->who)->loy_rate += amount;
	wout(c->who, "%s now bound with %s.",
				box_name(c->who), loyal_s(c->who));

	return TRUE;
}


int
v_oath(struct command *c)
{
	int flag = c->a;
	int lk, lr;
	int pl = player(c->who);
	int np_cost;

	lk = loyal_kind(c->who);
	lr = loyal_rate(c->who);

	if (flag > 2)
		flag = 2;
	if (flag < 1)
		flag = 1;

	if (lk == LOY_oath && lr >= 2)
	{
		wout(c->who, "%s already is at %s, the maximum loyalty.",
				box_name(c->who), loyal_s(c->who));
		return FALSE;
	}

#if 1
	if (flag == 2 && lk == LOY_oath && lr == 1)
		flag = 1;

	np_cost = flag;
#else
	if (flag == 1 && lk == LOY_oath && lr == 1)
	{
		wout(c->who, "%s already is at %s.",
				box_name(c->who), loyal_s(c->who));
		return FALSE;
	}

	np_cost = flag;
	if (lk == LOY_oath)
		np_cost -= lr;
#endif

	assert(np_cost > 0);

	if (player_np(pl) < 1)
	{
		wout(c->who, "Player %s has no Noble Points.",
				box_code(pl));
		return FALSE;
	}

	if (player_np(pl) < np_cost)
	{
		wout(c->who, "Player %s only has %d Noble Points.",
				box_code(pl), player_np(pl));
		np_cost = player_np(pl);
	}

	if (lk != LOY_oath)
	{
		p_char(c->who)->loy_kind = LOY_oath;
		p_char(c->who)->loy_rate = 0;
	}

	p_char(c->who)->loy_rate += np_cost;
	deduct_np(pl, np_cost);

	wout(c->who, "%s now bound with %s.",
				box_name(c->who), loyal_s(c->who));

	return TRUE;
}


int
terrorize_vassal(struct command *c)
{
	int target = c->a;
	int severity = c->b;

	if (severity < 1)
		severity = 3;

	if (severity > char_health(target))
		severity = char_health(target);

	add_char_damage(target, severity, c->who);

	if (!alive(target))
		return FALSE;

	if (loyal_kind(target) != LOY_fear)
	{
		p_char(target)->loy_kind = LOY_fear;
		p_char(target)->loy_rate = 0;
	}

	p_char(target)->loy_rate += severity;

	wout(c->who, "%s now bound with %s.",
				box_name(target), loyal_s(target));

	return TRUE;
}


int
terrorize_prisoner(struct command *c)
{
	int target = c->a;
	int severity = c->b;

	if (severity < 1)
		severity = 1;

	add_char_damage(target, severity, c->who);

	if (!alive(target))
		return FALSE;

	wout(c->who, "Health of %s is now %d.", box_name(target),
			 char_health(target));

	if (loyal_kind(target) != LOY_oath &&
	    rnd(1, 100) <= severity)
	{
		if (!enough_np_to_acquire(c->who, target))
			return FALSE;

		wout(c->who, "%s has been convinced to join us.",
					box_name(target));

		unit_deserts(target, player(c->who), TRUE, LOY_fear, severity);
		p_char(target)->fresh_hire = TRUE;

		return TRUE;
	}

	wout(c->who, "%s refuses to swear fealty to us.", box_name(target));

	return TRUE;
}


int
v_terrorize(struct command *c)
{
	int target = c->a;
	int severity = c->b;

	if (loyal_kind(c->who) == LOY_fear)
	{
		wout(c->who, "Units of fear loyalty may not terrorize.");
		return FALSE;
	}

	if (!check_char_here(c->who, target))
		return FALSE;

	if (is_prisoner(target))
	{
		if (stack_leader(target) != stack_leader(c->who))
		{
			wout(c->who, "%s is not a prisoner of %s.",
				box_code(target), box_name(c->who));
			return FALSE;
		}

		if (is_npc(target))
		{
			wout(c->who, "NPC's cannot swear to player factions.");
			return FALSE;
		}

		wout(c->who, "Attempt to gain the loyalty of %s "
				"through terror.", box_name(target));
		return TRUE;
	}

	if (player(target) != player(c->who))
	{
		wout(c->who, "%s does not belong to our faction.",
				box_code(target));
		return FALSE;
	}

	if (!stacked_beneath(c->who, target))
	{
		wout(c->who, "Unit to be terrorized must be stacked "
					"beneath us.");
		return FALSE;
	}

	if (loyal_kind(target) == LOY_oath)
	{
		wout(c->who, "Oathbound units do not need to have their "
			"loyalty reinforced through terror.");
		return FALSE;
	}

	wout(c->who, "Increase the loyalty of %s through terror.",
			box_name(target));

	return TRUE;
}


int
d_terrorize(struct command *c)
{
	int target = c->a;
	int severity = c->b;

	if (!check_still_here(c->who, target))
		return FALSE;

	if (is_prisoner(target))
	{
		if (stack_leader(target) != stack_leader(c->who))
		{
			wout(c->who, "%s is not a prisoner of %s.",
				box_code(target), box_name(c->who));
			return FALSE;
		}

		return terrorize_prisoner(c);
	}

	if (player(target) != player(c->who))
	{
		wout(c->who, "%s does not belong to our faction.",
				box_code(target));
		return FALSE;
	}

	if (!stacked_beneath(c->who, target))
	{
		wout(c->who, "Unit to be terrorized must be stacked "
					"beneath us.");
		return FALSE;
	}

	if (loyal_kind(target) == LOY_oath)
	{
		wout(c->who, "Oathbound units do not need to have their "
			"loyalty reinforced through terror.");
		return FALSE;
	}

	return terrorize_vassal(c);
}


int
v_raise(struct command *c)
{
	int where = subloc(c->who);

	if (!check_skill(c->who, sk_raise_mob))
		return FALSE;

	if (!may_cookie_npc(c->who, where, item_mob_cookie))
		return FALSE;

	return TRUE;
}


int
d_raise(struct command *c)
{
	int where = subloc(c->who);
	int mob;

	mob = do_cookie_npc(c->who, where, item_mob_cookie, where);

	if (mob <= 0)
	{
		log_write(LOG_CODE, "d_raise mob <= 0");
		wout(c->who, "Failed to raise peasant mob.");
		return FALSE;
	}

	add_skill_experience(c->who, sk_raise_mob);

	queue(mob, "guard 1");
	init_load_sup(mob);   /* make ready to execute commands immediately */

	wout(c->who, "Raised %s.", box_name(mob));
	wout(where, "A speech by %s has raised %s.",
				box_name(c->who),
				liner_desc(mob));

	return TRUE;
}


int
v_rally(struct command *c)
{
	int mob = c->a;

	if (!check_skill(c->who, sk_rally_mob))
		return FALSE;

	if (!check_char_here(c->who, mob))
		return FALSE;

	if (noble_item(mob) != item_peasant &&
	    noble_item(mob) != item_angry_peasant)
	{
		wout(c->who, "%s is not a peasant mob.", box_name(mob));
		return FALSE;
	}

	return TRUE;
}


int
d_rally(struct command *c)
{
	int mob = c->a;
	int n;

	if (!check_char_gone(c->who, mob))
		return FALSE;

	if (noble_item(mob) != item_peasant &&
	    noble_item(mob) != item_angry_peasant)
	{
		wout(c->who, "%s is not a peasant mob.", box_name(mob));
		return FALSE;
	}

	add_skill_experience(c->who, sk_rally_mob);

	if (n = stack_parent(mob))
	{
		set_loyal(mob, LOY_summon, min(loyal_rate(mob) + 3, 5));

		wout(c->who, "Renewed enthusiasm of %s for %s.",
				box_name(mob), box_name(n));

		wout(c->who, "The peasants will stay spirited for %d months.",
					loyal_rate(mob));

		return TRUE;
	}

	join_stack(mob, c->who);
	set_loyal(mob, LOY_summon, 3);

/*
 *  auto_mob() may have queued some orders, with a preceeding wait. 
 *  Get rid of them now that the mob is LOY_summon
 */

	flush_unit_orders(player(mob), mob);
	interrupt_order(mob);

	return FALSE;
}


int
v_incite(struct command *c)
{
	int mob = c->a;
	int target = c->b;

	if (!check_skill(c->who, sk_incite_mob))
		return FALSE;

	if (!check_char_here(c->who, mob))
		return FALSE;

	if (!valid_box(target) || (subloc(target) != subloc(c->who)))
	{
		wout(c->who, "%s is not here.", box_code(target));
		return FALSE;
	}

	if (noble_item(mob) != item_peasant &&
	    noble_item(mob) != item_angry_peasant)
	{
		wout(c->who, "%s is not a peasant mob.", box_name(mob));
		return FALSE;
	}

	if (stack_parent(mob))
	{
		wout(c->who, "%s is stacked under a leader.",
					box_name(mob));
		return FALSE;
	}

	return TRUE;
}


int
d_incite(struct command *c)
{
	int mob = c->a;
	int target = c->b;
	int where = subloc(c->who);

	if (!check_char_gone(c->who, mob))
		return FALSE;

	if (noble_item(mob) != item_peasant &&
	    noble_item(mob) != item_angry_peasant)
	{
		wout(c->who, "%s is not a peasant mob.", box_name(mob));
		return FALSE;
	}

	if (subloc(target) != where)
	{
		wout(c->who, "%s is no longer here.", box_name(target));
		return FALSE;
	}

	if (stack_parent(mob))
	{
		wout(c->who, "%s is stacked under a leader.",
					box_name(mob));
		return FALSE;
	}

	add_skill_experience(c->who, sk_incite_mob);

	if (rnd(1,3) == 1)
	{
		int i;

		loop_here(where, i)
		{
			if (kind(i) != T_loc || subkind(i) != sub_inn)
				continue;

			wout(i, "Rumors claim that %s is trying to incite "
					"a mob to attack %s.",
					box_name(c->who),
					box_name(target));
		}
		next_here;
	}

	if (rnd(1,2) == 1)
	{
		wout(c->who, "Failed to incite the mob to violence.");
		return FALSE;
	}

	flush_unit_orders(player(mob), mob);
	interrupt_order(mob);
	queue(mob, "attack %s", box_code_less(target));
	init_load_sup(mob);   /* make ready to execute commands immediately */

	wout(c->who, "%s will attack %s!", box_name(mob), box_name(target));

	return TRUE;
}


int
v_persuade_oath(struct command *c)
{
	int target = c->a;

	if (!check_char_here(c->who, target))
		return FALSE;

	if (char_new_lord(target))
	{
		wout(c->who, "%s just switched employers this month, and is "
		   "not looking for a new one so soon.", box_name(target));
		return FALSE;
	}

	if (!can_pay(c->who, 25))
	{
		wout(c->who, "Don't have %s.", gold_s(25));
		return FALSE;
	}

	return TRUE;
}


int
d_persuade_oath(struct command *c)
{
	int target = c->a;
	int flag = c->b;

	if (!check_still_here(c->who, target))
		return FALSE;

	if (char_new_lord(target))
	{
		wout(c->who, "%s just switched employers this month, and is "
		   "not looking for a new one so soon.", box_name(target));
		return FALSE;
	}

	if (loyal_kind(target) != LOY_oath)
	{
		wout(c->who, "%s does not have oath loyalty.",
					box_name(target));
		return FALSE;
	}

	if (!charge(c->who, 25))
	{
		wout(c->who, "Don't have %s.", gold_s(25));
		return FALSE;
	}

	if (loyal_rate(target) != 1 || rnd(1,100) > 2)
	{
		wout(c->who, "Failed to convince %s to join us.",
						box_name(target));
		return TRUE;
	}

	if (!enough_np_to_acquire(c->who, target))
		return FALSE;


	wout(c->who, "%s has been convinced to join us!", box_name(target));

	unit_deserts(target, player(c->who), TRUE, LOY_UNCHANGED, 0);
	p_char(target)->fresh_hire = TRUE;

	if (flag)
		join_stack(target, c->who);

	return TRUE;
}

