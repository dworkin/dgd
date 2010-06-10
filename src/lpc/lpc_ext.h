/*
 * This file is part of DGD, http://www.dworkin.nl/dgd/
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


Int  ext_frame_push_int		P((frame*, Int));
void ext_frame_push_lvalue	P((frame*, value*, unsigned int));
void ext_frame_push_lvclass	P((frame*, Int));
Int  ext_frame_pop_int		P((frame*));
int  ext_frame_pop_truthval	P((frame*));
void ext_frame_store		P((frame*));
Int  ext_frame_store_int	P((frame*));
void ext_frame_kfun		P((frame*, int));
void ext_frame_kfun_arg		P((frame*, int, int));
Int  ext_int_div		P((Int, Int));
Int  ext_int_mod		P((Int, Int));
Int  ext_int_lshift		P((Int, Int));
Int  ext_int_rshift		P((Int, Int));
void ext_runtime_rlimits	P((frame*));
int  ext_runtime_rswitch	P((Int, Int*, int));
int  ext_runtime_sswitch	P((frame*, char*, int));

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
int kf_add P((frame*)), kf_add1 P((frame*)), kf_and P((frame*)),
    kf_div P((frame*)), kf_eq P((frame*)), kf_ge P((frame*)), kf_gt P((frame*)),
    kf_le P((frame*)), kf_lshift P((frame*)), kf_lt P((frame*)),
    kf_mod P((frame*)), kf_mult P((frame*)), kf_ne P((frame*)),
    kf_neg P((frame*)), kf_not P((frame*)), kf_or P((frame*)),
    kf_rangeft P((frame*)), kf_rangef P((frame*)), kf_ranget P((frame*)),
    kf_range P((frame*)), kf_rshift P((frame*)), kf_sub P((frame*)),
    kf_sub1 P((frame*)), kf_tofloat P((frame*)), kf_toint P((frame*)),
    kf_tst P((frame*)), kf_umin P((frame*)), kf_xor P((frame*)),
    kf_tostring P((frame*)), kf_ckrangeft P((frame*)), kf_ckrangef P((frame*)),
    kf_ckranget P((frame*)), kf_sum P((frame*, int));

int kf_this_object P((frame*)), kf_call_trace P((frame*)),
    kf_this_user P((frame*)), kf_users P((frame*)), kf_time P((frame*)),
    kf_swapout P((frame*)), kf_dump_state P((frame*)), kf_shutdown P((frame*));
