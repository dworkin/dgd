/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2019 DGD Authors (see the commit log for details)
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
extern Object	*c_compile	(Frame*, char*, Object*, String**, int, int);
extern bool	 c_upgrade	(Object**, unsigned int);
extern int	 c_autodriver	();
extern void	 c_error	(const char *, ...);

extern bool	 c_typechecking	();
extern bool	 c_inherit	(char*, Node*, int);
extern String	*c_objecttype	(Node*);
extern void	 c_global	(unsigned int, Node*, Node*);
extern void	 c_function	(unsigned int, Node*, Node*);
extern void	 c_funcbody	(Node*);
extern void	 c_local	(unsigned int, Node*, Node*);
extern void	 c_startcond	();
extern void	 c_startcond2	();
extern void	 c_endcond	();
extern void	 c_matchcond	();
extern bool	 c_nil		(Node*);
extern Node	*c_concat	(Node*, Node*);
extern Node	*c_exp_stmt	(Node*);
extern Node	*c_if		(Node*, Node*);
extern Node	*c_endif	(Node*, Node*);
extern void	 c_loop		();
extern Node	*c_do		(Node*, Node*);
extern Node	*c_while	(Node*, Node*);
extern Node	*c_for		(Node*, Node*, Node*, Node*);
extern void	 c_startrlimits	();
extern Node	*c_endrlimits	(Node*, Node*, Node*);
extern void	 c_startcatch	();
extern void	 c_endcatch	();
extern Node	*c_donecatch	(Node*, Node*);
extern void	 c_startswitch	(Node*, int);
extern Node	*c_endswitch	(Node*, Node*);
extern Node	*c_case		(Node*, Node*);
extern Node	*c_default	();
extern Node	*c_label	(Node*);
extern Node	*c_goto		(Node*);
extern Node	*c_break	();
extern Node	*c_continue	();
extern Node	*c_return	(Node*, int);
extern void	 c_startcompound();
extern Node	*c_endcompound	(Node*);
extern Node	*c_flookup	(Node*, int);
extern Node	*c_iflookup	(Node*, Node*);
extern Node	*c_aggregate	(Node*, unsigned int);
extern Node	*c_local_var	(Node*);
extern Node	*c_global_var	(Node*);
extern short	 c_vtype	(int);
extern Node	*c_funcall	(Node*, Node*);
extern Node	*c_arrow	(Node*, Node*, Node*);
extern Node	*c_address	(Node*, Node*, int);
extern Node	*c_extend	(Node*, Node*, int);
extern Node	*c_call		(Node*, Node*, int);
extern Node	*c_new_object	(Node*, Node*);
extern Node	*c_instanceof	(Node*, Node*);
extern Node	*c_checkcall	(Node*, int);
extern Node	*c_tst		(Node*);
extern Node	*c_not		(Node*);
extern Node	*c_lvalue	(Node*, const char*);
extern Node	*c_assign	(Node*);
extern unsigned short c_tmatch	(unsigned int, unsigned int);
