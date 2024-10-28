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

class CallOut {
public:
    static bool init(unsigned int max);
    static Uint check(unsigned int n, LPCint delay, unsigned int mdelay,
		      Uint *tp, unsigned short *mp, cindex **qp);
    static void create(unsigned int oindex, unsigned int handle, Uint t,
		       unsigned int m, cindex *q);
    static LPCint remaining(Uint t, unsigned short *m);
    static void del(unsigned int oindex, unsigned int handle, Uint t,
		    unsigned int m);
    static void list(Array *a);
    static void call(Frame *f);
    static void info(cindex *n1, cindex *n2);
    static Uint cotime(unsigned short *mtime);
    static Uint delay(Uint rtime, unsigned short rmtime, unsigned short *mtime);
    static void swapcount(unsigned int count);
    static long swaprate1();
    static long swaprate5();
    static bool save(int fd);
    static void restore(int fd, Uint t, bool conv16);

private:
    static CallOut *enqueue(Uint t, unsigned short m);
    static void dequeue(cindex i);
    static CallOut *newcallout(cindex *list, Uint t);
    static void freecallout(cindex *cyc, cindex j, cindex i, Uint t);
    static bool rmshort(cindex *cyc, uindex i, uindex handle, Uint t);
    static void expire();

    union {
	Time time;	/* when to call */
	struct {
	    cindex count;	/* # in list */
	    cindex prev;	/* previous in list */
	    cindex next;	/* next in list */
	} r;
    };
    uindex handle;	/* callout handle */
    uindex oindex;	/* index in object table */
};

# define CO1_LAYOUT	"[l|fff]uu"
# define CO2_LAYOUT	"[fff|l]uu"
