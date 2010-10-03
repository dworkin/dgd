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

# include "dgd.h"
# include "str.h"
# include "array.h"
# include "object.h"
# include "interpret.h"
# include "data.h"
# include "xfloat.h"
# include "csupport.h"


Int  ext_frame_push_int		(frame*, Int);
void ext_frame_push_lvalue	(frame*, value*, unsigned int);
void ext_frame_push_lvclass	(frame*, Int);
Int  ext_frame_pop_int		(frame*);
int  ext_frame_pop_truthval	(frame*);
void ext_frame_store		(frame*);
Int  ext_frame_store_int	(frame*);
void ext_frame_kfun		(frame*, int);
void ext_frame_kfun_arg		(frame*, int, int);
Int  ext_int_div		(Int, Int);
Int  ext_int_mod		(Int, Int);
Int  ext_int_lshift		(Int, Int);
Int  ext_int_rshift		(Int, Int);
void ext_runtime_rlimits	(frame*);
int  ext_runtime_rswitch	(Int, Int*, int);
int  ext_runtime_sswitch	(frame*, char*, int);

# define lpc_frame_push_int	ext_frame_push_int
# define lpc_frame_push_lvalue	ext_frame_push_lvalue
# define lpc_frame_push_lvclass	ext_frame_push_lvclass
# define lpc_frame_pop_int	ext_frame_pop_int
# define lpc_frame_pop_truthval	ext_frame_pop_truthval
# define lpc_frame_store	ext_frame_store
# define lpc_frame_store_int	ext_frame_store_int
# define lpc_frame_kfun		ext_frame_kfun
# define lpc_frame_kfun_arg	ext_frame_kfun_arg
# define lpc_int_div		ext_int_div
# define lpc_int_mod		ext_int_mod
# define lpc_int_lshift		ext_int_lshift
# define lpc_int_rshift		ext_int_rshift
# define lpc_runtime_rlimits	ext_runtime_rlimits
# define lpc_runtime_rswitch	ext_runtime_rswitch
# define lpc_runtime_sswitch	ext_runtime_sswitch


/*
 * unconverted below
 */

# define i_foffset(n)		(&f->ctrl->funcalls[2L * (f->foffset + (n))])

/*
 * prototypes for kfuns that might be called directly from precompiled code
 */
int kf_add (frame*), kf_add1 (frame*), kf_and (frame*),
    kf_div (frame*), kf_eq (frame*), kf_ge (frame*), kf_gt (frame*),
    kf_le (frame*), kf_lshift (frame*), kf_lt (frame*),
    kf_mod (frame*), kf_mult (frame*), kf_ne (frame*),
    kf_neg (frame*), kf_not (frame*), kf_or (frame*),
    kf_rangeft (frame*), kf_rangef (frame*), kf_ranget (frame*),
    kf_range (frame*), kf_rshift (frame*), kf_sub (frame*),
    kf_sub1 (frame*), kf_tofloat (frame*), kf_toint (frame*),
    kf_tst (frame*), kf_umin (frame*), kf_xor (frame*),
    kf_tostring (frame*), kf_ckrangeft (frame*), kf_ckrangef (frame*),
    kf_ckranget (frame*), kf_sum (frame*, int);

int kf_this_object (frame*), kf_call_trace (frame*),
    kf_this_user (frame*), kf_users (frame*), kf_time (frame*),
    kf_swapout (frame*), kf_dump_state (frame*), kf_shutdown (frame*);
