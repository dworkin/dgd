/*
 * This file is part of DGD, https://github.com/dworkin/dgd
 * Copyright (C) 1993-2010 Dworkin B.V.
 * Copyright (C) 2010-2015 DGD Authors (see the commit log for details)
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

String *asn_add	   (Frame*, String*, String*, String*);
String *asn_sub	   (Frame*, String*, String*, String*);
int     asn_cmp    (Frame*, String*, String*);
String *asn_mult   (Frame*, String*, String*, String*);
String *asn_div	   (Frame*, String*, String*, String*);
String *asn_mod	   (Frame*, String*, String*);
String *asn_pow	   (Frame*, String*, String*, String*);
String *asn_lshift (Frame*, String*, Int, String*);
String *asn_rshift (Frame*, String*, Int);
String *asn_and    (Frame*, String*, String*);
String *asn_or     (Frame*, String*, String*);
String *asn_xor    (Frame*, String*, String*);
