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

typedef struct _cbuf_ cbuf;

extern bool	co_init		P((unsigned int));
extern Uint	co_check	P((unsigned int, Int, unsigned int,
				   Uint*, unsigned short*, cbuf**));
extern void	co_new		P((unsigned int, unsigned int, Uint,
				   unsigned int, cbuf*));
extern Int	co_remaining	P((Uint, unsigned short*));
extern void	co_del		P((unsigned int, unsigned int, Uint,
				   unsigned int));
extern void	co_list		P((array*));
extern void	co_call		P((frame*));
extern void	co_info    	P((uindex*, uindex*));
extern Uint	co_decode	P((Uint, unsigned short*));
extern Uint	co_time		P((unsigned short*));
extern Uint	co_delay	P((Uint, unsigned int, unsigned short*));
extern void	co_swapcount	P((unsigned int));
extern long	co_swaprate1 	P((void));
extern long	co_swaprate5 	P((void));
extern bool	co_dump		P((int));
extern void	co_restore	P((int, Uint, int));
