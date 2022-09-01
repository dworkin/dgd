/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2022 DGD Authors (see the commit log for details)
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

# include "host.h"

class String;
class Array;
class Object;
class Value;
class Control;
class Dataplane;
class Dataspace;
class Frame;

# include "config.h"
# include "alloc.h"
# include "error.h"

# define BOFF(bit)		((bit) >> 5)
# define BBIT(bit)		(1 << ((bit) & 31))
# define BMAP(size)		BOFF((size) + 31)

# define BSET(map, bit)		(map[BOFF(bit)] |= BBIT(bit))
# define BCLR(map, bit)		(map[BOFF(bit)] &= ~BBIT(bit))
# define BTST(map, bit)		(map[BOFF(bit)] & BBIT(bit))

typedef uint64_t Time;

# define TIME_INT		2000

class DGD {
public:
    static bool callDriver(Frame *frame, const char *func, int narg);
    static void interrupt();
    static void endTask();
    static void errHandler(Frame *f, LPCint depth);
    static int main(int argc, char **argv);
};

extern bool intr;
