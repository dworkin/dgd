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

# define ec_push(cleanup)	setjmp(*_ec_push_(cleanup))

typedef void  (*ec_ftn)		(frame*, Int);

extern void	ec_clear	(void);
extern jmp_buf *_ec_push_	(ec_ftn);
extern void	ec_pop		(void);

extern void	serror		(string*);
extern string  *errorstr	(void);

extern void	message		(char *, ...);
extern void	error		(char *, ...);
extern void	fatal		(char *, ...);
