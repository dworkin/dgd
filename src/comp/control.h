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

extern void		 ctrl_init	(void);
extern bool		 ctrl_inherit	(frame*, char*, object*, string*,
					   int);
extern void		 ctrl_convert	(control*);
extern void		 ctrl_create	(void);
extern long		 ctrl_dstring	(string*);
extern void		 ctrl_dproto	(string*, char*, string*);
extern void		 ctrl_dfunc	(string*, char*, string*);
extern void		 ctrl_dprogram	(char*, unsigned int);
extern void		 ctrl_dvar	(string*, unsigned int,
					   unsigned int, string*);
extern char		*ctrl_ifcall	(string*, char*, string**, long*);
extern char		*ctrl_fcall	(string*, string**, long*, int);
extern unsigned short	 ctrl_gencall	(long);
extern unsigned short	 ctrl_var	(string*, long*, string**);
extern int		 ctrl_ninherits	(void);
extern bool		 ctrl_chkfuncs	(void);
extern void		 ctrl_mkvtypes	(control*);
extern dsymbol		*ctrl_symb	(control*, char*, unsigned int);
extern control		*ctrl_construct	(void);
extern void		 ctrl_clear	(void);
extern unsigned short	*ctrl_varmap	(control*, control*);
extern array		*ctrl_undefined	(dataspace*, control*);

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
# define FCALL			3
