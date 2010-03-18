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

extern void		 tk_init	(void);
extern void		 tk_clear	(void);
extern bool		 tk_include	(char*, string**, int);
extern void		 tk_endinclude	(void);
extern unsigned short	 tk_line	(void);
extern char		*tk_filename	(void);
extern void		 tk_setline	(unsigned short);
extern void		 tk_setfilename	(char*);
extern void		 tk_header	(int);
extern void		 tk_setpp	(int);
extern int		 tk_gettok	(void);
extern void		 tk_skiptonl	(int);
extern int		 tk_expand	(macro*);

extern char *yytext;
extern int yyleng;
extern long yynumber;
extern xfloat yyfloat;
