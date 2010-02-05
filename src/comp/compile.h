/*
 * This file is part of DGD, http://dgd-osr.sourceforge.net/
 * Copyright (C) 1993-2010 Dworkin B.V.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

extern void	 c_init		P((char*, char*, char*, char**, int));
extern object	*c_compile	P((frame*, char*, object*, string**, int, int));
extern bool	 c_upgrade	P((object**, unsigned int));
extern int	 c_autodriver	P((void));
extern void 	 c_error	();

extern bool	 c_typechecking	P((void));
extern bool	 c_inherit	P((char*, node*, int));
extern string	*c_objecttype	P((node*));
extern void	 c_global	P((unsigned int, node*, node*));
extern void	 c_function	P((unsigned int, node*, node*));
extern void	 c_funcbody	P((node*));
extern void	 c_local	P((unsigned int, node*, node*));
extern void	 c_startcond	P((void));
extern void	 c_startcond2	P((void));
extern void	 c_endcond	P((void));
extern void	 c_matchcond	P((void));
extern bool	 c_nil		P((node*));
extern node	*c_concat	P((node*, node*));
extern node	*c_exp_stmt	P((node*));
extern node	*c_if		P((node*, node*));
extern node	*c_endif	P((node*, node*));
extern void	 c_loop		P((void));
extern node	*c_do		P((node*, node*));
extern node	*c_while	P((node*, node*));
extern node	*c_for		P((node*, node*, node*, node*));
extern void	 c_startrlimits	P((void));
extern node	*c_endrlimits	P((node*, node*, node*));
extern void	 c_startcatch	P((void));
extern void	 c_endcatch	P((void));
extern node	*c_donecatch	P((node*, node*));
extern void	 c_startswitch	P((node*, int));
extern node	*c_endswitch	P((node*, node*));
extern node	*c_case		P((node*, node*));
extern node	*c_default	P((void));
extern node	*c_break	P((void));
extern node	*c_continue	P((void));
extern node	*c_return	P((node*, int));
extern void	 c_startcompound P((void));
extern node	*c_endcompound	P((node*));
extern node	*c_flookup	P((node*, int));
extern node	*c_iflookup	P((node*, node*));
extern node	*c_aggregate	P((node*, unsigned int));
extern node	*c_variable	P((node*));
extern short	 c_vtype	P((int));
extern node	*c_funcall	P((node*, node*));
extern node	*c_arrow	P((node*, node*, node*));
extern node	*c_instanceof	P((node*, node*));
extern node	*c_checkcall	P((node*, int));
extern node	*c_tst		P((node*));
extern node	*c_not		P((node*));
extern node	*c_lvalue	P((node*, char*));
extern node	*c_assign	P((node*));
extern unsigned short c_tmatch	P((unsigned int, unsigned int));
