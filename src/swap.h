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

extern void	sw_init		(char*, unsigned int, unsigned int,
				   unsigned int);
extern void	sw_finish	(void);
extern void	sw_newv		(sector*, unsigned int);
extern void	sw_wipev	(sector*, unsigned int);
extern void	sw_delv		(sector*, unsigned int);
extern void	sw_readv	(char*, sector*, Uint, Uint);
extern void	sw_writev	(char*, sector*, Uint, Uint);
extern void	sw_creadv	(char*, sector*, Uint, Uint);
extern void	sw_dreadv	(char*, sector*, Uint, Uint);
extern void	sw_conv		(char*, sector*, Uint, Uint);
extern sector	sw_mapsize	(unsigned int);
extern sector	sw_count	(void);
extern bool	sw_copy		(Uint);
extern int	sw_dump		(char*);
extern void	sw_restore	(int, unsigned int);
