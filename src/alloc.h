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

# ifdef DEBUG

# define ALLOC(type, size)						      \
			((type *) (m_alloc(sizeof(type) * (size_t) (size),    \
					   __FILE__, __LINE__)))
# define REALLOC(mem, type, size1, size2)				      \
			((type *) (m_realloc((char *) (mem),		      \
					     sizeof(type) * (size_t) (size1), \
					     sizeof(type) * (size_t) (size2), \
					     __FILE__, __LINE__)))
extern char *m_alloc	(size_t, char*, int);
extern char *m_realloc	(char*, size_t, size_t, char*, int);

# else

# define ALLOC(type, size)						      \
			((type *) (m_alloc(sizeof(type) * (size_t) (size))))
# define REALLOC(mem, type, size1, size2)				      \
			((type *) (m_realloc((char *) (mem),		      \
					     sizeof(type) * (size_t) (size1), \
					     sizeof(type) * (size_t) (size2))))
extern char *m_alloc	(size_t);
extern char *m_realloc	(char*, size_t, size_t);

# endif

# define FREE(mem)	m_free((char *) (mem))

extern void  m_init	(size_t, size_t);
extern void  m_free	(char*);
extern void  m_dynamic	(void);
extern void  m_static	(void);
extern bool  m_check	(void);
extern void  m_purge	(void);
extern void  m_finish	(void);

typedef struct {
    size_t smemsize;	/* static memory size */
    size_t smemused;	/* static memory used */
    size_t dmemsize;	/* dynamic memory used */
    size_t dmemused;	/* dynamic memory used */
} allocinfo;

extern allocinfo *m_info (void);
