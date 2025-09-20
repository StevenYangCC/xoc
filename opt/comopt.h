/*@
XOC Release License

Copyright (c) 2013-2014, Alibaba Group, All rights reserved.

    compiler@aliexpress.com

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the Su Zhenyu nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

author: Su Zhenyu
@*/
//Optimizations
#include "preana.h"
#include "apply_to_region.h"
#include "liveness_mgr.h"
#include "md_liveness_mgr.h"
#include "pr_liveness_mgr.h"
#include "ssaregion.h"
#include "ir_ssa.h"
#include "ir_mdssa.h"
#include "mdssalive_mgr.h"
#include "cfs_mgr.h"
#include "cfs_opt.h"
#include "goto_opt.h"
#include "ir_refine.h"
#include "if_opt.h"
#include "linear_rep.h"
#include "chain_recur.h"
#include "alge_reasscociate.h"
#include "ir_gvn.h"
#include "ir_ivr.h"
#include "ir_lcse.h"
#include "ir_gcse.h"
#include "ir_dce.h"
#include "lftr.h"
#include "ir_rce.h"
#include "insert_guard_helper.h"
#include "cfg_lifting.h"
#include "loop_dep_ana.h"
#include "multi_res_convert.h"
#include "targinfo_handler.h"

#ifdef REF_TARGMACH_INFO
//Header files in XGEN are referred to by the following passes since they
//reference target machine informations, such as Reg, RegFile etc.
//It looks like backward invoking functions that defined in XGEN.
#include "../xgen/xgeninc.h"

//xgeninc.h will also include comopt.h that cause forward reference namespace
//xgen, thus redeclarate namespace xgen here.
using namespace xgen;
#include "regd.h"
#include "regssainfo.h"
#include "ir_regssa.h"
#include "update_pos.h"
#include "targinfo_mgr.h"
#include "range.h"
#include "lifetime.h"
#include "var_liveness_mgr.h"
#include "lsra_var_liveness_mgr.h"
#include "var_lifetime.h"
#include "lt_interf_graph.h"
#include "linear_scan.h"
#include "lsra_impl.h"
#include "lsra_post_opt.h"
#include "lsra_scan_in_pos.h"
#include "lt_prio_mgr.h"
#include "lsra_scan_in_prio.h"
#include "arg_passer.h"
#include "prologue_epilogue_inserter.h"
#include "var2offset.h"
#include "ir_reloc_mgr.h"
#include "dynamic_stack.h"
#include "gp_adjustment.h"
#include "br_opt.h"
#include "igoto_opt.h"
#include "inst_sched.h"
#include "ir_ddg.h"
#include "ir_sim.h"
#include "ir_lis.h"
#include "local_var_liveness_mgr.h"
#include "local_var_lifetime_mgr.h"
#endif

#ifdef FOR_IP
#include "ir_vect.h"
#include "derivative.h"
#include "ir_dse.h"
#include "scop.h"
#include "ir_poly.h"
#include "ir_vrp.h"
#include "ir_ccp.h"
#include "ir_pre.h"
#endif

#include "ir_decl_ext.h"
#include "ir_mgr.h"
#include "ir_mgr_ext.h"
#include "ir_cp.h"
#include "ir_bcp.h"
#include "ir_rp.h"
#include "ir_licm.h"
#include "ir_loop_cvt.h"
#include "ipa.h"
#include "inliner.h"
#include "refine_duchain.h"
#include "scalar_opt.h"
#include "infer_type.h"
#include "insert_cvt.h"
#include "invert_brtgt.h"
#include "targ_opt.inc"
