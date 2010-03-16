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

typedef struct _dfa_ dfa;

# define DFA_EOS	-1
# define DFA_REJECT	-2
# define DFA_TOOBIG	-3

extern dfa     *dfa_new		(char*, char*);
extern void	dfa_del		(dfa*);
extern dfa     *dfa_load	(char*, char*, char*, Uint);
extern bool	dfa_save	(dfa*, char**, Uint*);
extern short	dfa_scan	(dfa*, string*, ssizet*, char**, ssizet*);
