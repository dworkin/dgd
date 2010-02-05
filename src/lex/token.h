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

extern void		 tk_init	P((void));
extern void		 tk_clear	P((void));
extern bool		 tk_include	P((char*, string**, int));
extern void		 tk_endinclude	P((void));
extern unsigned short	 tk_line	P((void));
extern char		*tk_filename	P((void));
extern void		 tk_setline	P((unsigned int));
extern void		 tk_setfilename	P((char*));
extern void		 tk_header	P((int));
extern void		 tk_setpp	P((int));
extern int		 tk_gettok	P((void));
extern void		 tk_skiptonl	P((int));
extern int		 tk_expand	P((macro*));

extern char *yytext;
extern int yyleng;
extern long yynumber;
extern xfloat yyfloat;
