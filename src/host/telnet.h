/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010 DGD Authors (see the commit log for details)
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

/*
 * Definitions for the TELNET protocol.
 */

# define IAC		255	/* interpret as command */
# define DONT		254	/* don't */
# define DO		253	/* do */
# define WONT		252	/* won't */
# define WILL		251	/* will */
# define SB		250	/* begin subnegotiation */
# define GA		249	/* go ahead */
# define AYT		246	/* are you there */
# define IP		244	/* interrupt process */
# define BREAK		243	/* break */
# define SE		240	/* end subnegotiation */

/* options */
# define TELOPT_ECHO	1	/* echo */
# define TELOPT_SGA	3	/* suppress go ahead */
# define TELOPT_TM	6	/* timing mark */
