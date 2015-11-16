/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2015 DGD Authors (see the commit log for details)
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

extern void		 ctrl_init	();
extern bool		 ctrl_inherit	(Frame*, char*, Object*, String*,
					   int);
extern void		 ctrl_convert	(Control*);
extern void		 ctrl_create	();
extern long		 ctrl_dstring	(String*);
extern void		 ctrl_dproto	(String*, char*, String*);
extern void		 ctrl_dfunc	(String*, char*, String*);
extern void		 ctrl_dprogram	(char*, unsigned int);
extern void		 ctrl_dvar	(String*, unsigned int,
					   unsigned int, String*);
extern char		*ctrl_ifcall	(String*, const char*, String**, long*);
extern char		*ctrl_fcall	(String*, String**, long*, int);
extern unsigned short	 ctrl_gencall	(long);
extern unsigned short	 ctrl_var	(String*, long*, String**);
extern int		 ctrl_ninherits	();
extern bool		 ctrl_chkfuncs	();
extern void		 ctrl_mkvtypes	(Control*);
extern dsymbol		*ctrl_symb	(Control*, const char*, unsigned int);
extern Control		*ctrl_construct	();
extern void		 ctrl_clear	();
extern unsigned short	*ctrl_varmap	(Control*, Control*);
extern Array		*ctrl_undefined	(Dataspace*, Control*);

# define PROTO_CLASS(prot)	((prot)[0])
# define PROTO_NARGS(prot)	((prot)[1])
# define PROTO_VARGS(prot)	((prot)[2])
# define PROTO_HSIZE(prot)	((prot)[3])
# define PROTO_LSIZE(prot)	((prot)[4])
# define PROTO_SIZE(prot)	((PROTO_HSIZE(prot) << 8) | PROTO_LSIZE(prot))
# define PROTO_FTYPE(prot)	((prot)[5])
# define PROTO_ARGS(prot)	((prot) +				      \
				 (((PROTO_FTYPE(prot) & T_TYPE) == T_CLASS) ? \
				   9 : 6))

# define T_IMPLICIT		(T_VOID | (1 << REFSHIFT))

# define KFCALL			0
# define KFCALL_LVAL		1
# define DFCALL			2
# define FCALL			4
