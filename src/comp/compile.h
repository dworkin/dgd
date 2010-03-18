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

extern void	 c_init		(char*, char*, char*, char**, int);
extern object	*c_compile	(frame*, char*, object*, string**, int, int);
extern bool	 c_upgrade	(object**, unsigned int);
extern int	 c_autodriver	(void);
extern void 	 c_error	(char *, ...);

extern bool	 c_typechecking	(void);
extern bool	 c_inherit	(char*, node*, int);
extern string	*c_objecttype	(node*);
extern void	 c_global	(unsigned int, node*, node*);
extern void	 c_function	(unsigned int, node*, node*);
extern void	 c_funcbody	(node*);
extern void	 c_local	(unsigned int, node*, node*);
extern void	 c_startcond	(void);
extern void	 c_startcond2	(void);
extern void	 c_endcond	(void);
extern void	 c_matchcond	(void);
extern bool	 c_nil		(node*);
extern node	*c_concat	(node*, node*);
extern node	*c_exp_stmt	(node*);
extern node	*c_if		(node*, node*);
extern node	*c_endif	(node*, node*);
extern void	 c_loop		(void);
extern node	*c_do		(node*, node*);
extern node	*c_while	(node*, node*);
extern node	*c_for		(node*, node*, node*, node*);
extern void	 c_startrlimits	(void);
extern node	*c_endrlimits	(node*, node*, node*);
extern void	 c_startcatch	(void);
extern void	 c_endcatch	(void);
extern node	*c_donecatch	(node*, node*);
extern void	 c_startswitch	(node*, int);
extern node	*c_endswitch	(node*, node*);
extern node	*c_case		(node*, node*);
extern node	*c_default	(void);
extern node	*c_break	(void);
extern node	*c_continue	(void);
extern node	*c_return	(node*, int);
extern void	 c_startcompound (void);
extern node	*c_endcompound	(node*);
extern node	*c_flookup	(node*, int);
extern node	*c_iflookup	(node*, node*);
extern node	*c_aggregate	(node*, unsigned int);
extern node	*c_variable	(node*);
extern short	 c_vtype	(int);
extern node	*c_funcall	(node*, node*);
extern node	*c_arrow	(node*, node*, node*);
extern node	*c_instanceof	(node*, node*);
extern node	*c_checkcall	(node*, int);
extern node	*c_tst		(node*);
extern node	*c_not		(node*);
extern node	*c_lvalue	(node*, char*);
extern node	*c_assign	(node*);
extern unsigned short c_tmatch	(unsigned int, unsigned int);
