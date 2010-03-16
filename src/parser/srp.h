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

typedef struct _srp_ srp;

extern srp     *srp_new		(char*);
extern void	srp_del		(srp*);
extern srp     *srp_load	(char*, char*, Uint);
extern bool	srp_save	(srp*, char**, Uint*);
extern short	srp_check	(srp*, unsigned int, unsigned short*,
				   char**);
extern short	srp_shift	(srp*, unsigned int, unsigned int);
extern short	srp_goto	(srp*, unsigned int, unsigned int);
