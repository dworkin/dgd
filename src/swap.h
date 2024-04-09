/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2024 DGD Authors (see the commit log for details)
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

class Swap {
public:
    struct SwapSlot {		/* swap slot header */
	SwapSlot *prev;		/* previous in swap slot list */
	SwapSlot *next;		/* next in swap slot list */
	Sector sec;		/* the sector that uses this slot */
	Sector swap;		/* the swap sector (if any) */
	bool dirty;		/* has the swap slot been written to? */
    };

    static void init(char *file, unsigned int total, unsigned int secsize);
    static void finish();
    static bool write(int fd, void *buffer, size_t size);
    static void wipev(Sector *vec, unsigned int size);
    static void delv(Sector *vec, unsigned int size);
    static Sector alloc(Uint size, Sector nsectors, Sector **sectors);
    static void readv(char*, Sector*, Uint, Uint);
    static void writev(char*, Sector*, Uint, Uint);
    static void dreadv(char*, Sector*, Uint, Uint);
    static void conv(char*, Sector*, Uint, Uint);
    static void conv2(char*, Sector*, Uint, Uint);
    static Uint convert(char *m, Sector *vec, const char *layout, Uint n,
			Uint idx, void (*readv) (char*, Sector*, Uint, Uint));
    static Uint compress(char *data, char *text, Uint size);
    static char *decompress(Sector *sectors,
			    void (*readv) (char*, Sector*, Uint, Uint),
			    Uint size, Uint offset, Uint *dsize);
    static Sector count();
    static int save(char *snapshot, bool keep);
    static void save2(SnapshotInfo *header, int size, bool incr);
    static void restore(int fd, unsigned int secsize);
    static void restore2(int fd);

private:
    static void create();
    static Sector mapsize(unsigned int size);
    static void newv(Sector *vec, unsigned int size);
    static SwapSlot *load(Sector sec, bool restore, bool fill);
};
