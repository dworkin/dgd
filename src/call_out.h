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

extern bool	co_init		(unsigned int);
extern Uint	co_check	(unsigned int, Int, unsigned int,
				   Uint*, unsigned short*, cbuf**);
extern void	co_new		(unsigned int, unsigned int, Uint,
				   unsigned int, cbuf*);
extern Int	co_remaining	(Uint, unsigned short*);
extern void	co_del		(unsigned int, unsigned int, Uint,
				   unsigned int);
extern void	co_list		(array*);
extern void	co_call		(frame*);
extern void	co_info    	(uindex*, uindex*);
extern Uint	co_decode	(Uint, unsigned short*);
extern Uint	co_time		(unsigned short*);
extern Uint	co_delay	(Uint, unsigned int, unsigned short*);
extern void	co_swapcount	(unsigned int);
extern long	co_swaprate1 	(void);
extern long	co_swaprate5 	(void);
extern bool	co_dump		(int);
extern void	co_restore	(int, Uint, int);
