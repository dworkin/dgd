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

string *asn_add	   P((frame*, string*, string*, string*));
string *asn_sub	   P((frame*, string*, string*, string*));
int     asn_cmp    P((frame*, string*, string*));
string *asn_mult   P((frame*, string*, string*, string*));
string *asn_div	   P((frame*, string*, string*, string*));
string *asn_mod	   P((frame*, string*, string*));
string *asn_pow	   P((frame*, string*, string*, string*));
string *asn_lshift P((frame*, string*, Int, string*));
string *asn_rshift P((frame*, string*, Int));
string *asn_and    P((frame*, string*, string*));
string *asn_or     P((frame*, string*, string*));
string *asn_xor    P((frame*, string*, string*));
