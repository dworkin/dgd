/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 2020-2022 DGD Authors (see the commit log for details)
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

class Ext {
public:
# ifndef NOFLOAT
    static double getFloat(const Float *flt);
    static bool checkFloat(double *d);
    static void constrainFloat(double *d);
    static void putFloat(Float *flt, double d);
# ifdef LARGENUM
    static bool smallFloat(unsigned short *fhigh, Uint *flow, Float *flt);
    static void largeFloat(Float *flt, unsigned short fhigh, Uint flow);
# endif
# endif
    static void kfuns(char *protos, int size, int nkfun);
    static bool execute(const Frame *f, int func);
    static void release(uint64_t index, uint64_t instance);
    static bool load(char *module, char *config, void (**fdlist)(int*, int),
		     void (**finish)(int));
    static void finish();

private:
    static void spawn(void (*fdlist)(int*, int), void (*finish)(int));
    static void cleanup();
    static void jit(int (*init)(int, int, size_t, size_t, int, int, int,
				uint8_t*, size_t, void**),
		    void (*finish)(),
		    void (*compile)(uint64_t, uint64_t, int, uint8_t*, size_t,
				    int, uint8_t*, size_t, uint8_t*, size_t),
		    int (*execute)(uint64_t, uint64_t, int, int, void*),
		    void (*release)(uint64_t, uint64_t),
		    int (*functions)(uint64_t, uint64_t, int, void*));
    static void compile(const Frame *f, Control *ctrl);
};

Value *ext_value_temp(Dataspace *data);
