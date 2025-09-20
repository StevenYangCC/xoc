/*@
Copyright (c) 2013-2021, Su Zhenyu steven.known@gmail.com

All rights reserved.

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

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
@*/
#include "cominc.h"
#include "comopt.h"

namespace xoc {

void dumpATOMINC(IR const* ir, Region const* rg, IRDumpCtx<> & ctx)
{
    bool dump_addr = ctx.dumpflag.have(IR_DUMP_ADDR);
    bool dump_kid = ctx.dumpflag.have(IR_DUMP_KID);
    StrBuf buf(64);
    TypeMgr const* xtm = rg->getTypeMgr();
    Type const* d = ir->getType();
    LogMgr * lm = rg->getLogMgr();
    note(rg, "%s:%s", IRNAME(ir), xtm->dump_type(d, buf));
    DUMPADDR(ir);
    prt(rg, "%s", ctx.attr);
    if (!dump_kid) { return; }
    lm->incIndent(ctx.dn);
    dumpIRList(ATOMINC_memory(ir), rg, (CHAR*)" memory", ctx.dumpflag);
    if (ATOMINC_addend(ir) != nullptr) {
        dumpIRList(ATOMINC_addend(ir), rg, (CHAR*)" addend", ctx.dumpflag);
    }
    dumpIRList(ATOMINC_multires(ir), rg, (CHAR*)" multi-res",
               ctx.dumpflag);
    lm->decIndent(ctx.dn);
}


void dumpATOMCAS(IR const* ir, Region const* rg, IRDumpCtx<> & ctx)
{
    bool dump_addr = ctx.dumpflag.have(IR_DUMP_ADDR);
    bool dump_kid = ctx.dumpflag.have(IR_DUMP_KID);
    StrBuf buf(64);
    TypeMgr const* xtm = rg->getTypeMgr();
    Type const* d = ir->getType();
    LogMgr * lm = rg->getLogMgr();
    note(rg, "%s:%s", IRNAME(ir), xtm->dump_type(d, buf));
    DUMPADDR(ir);
    prt(rg, "%s", ctx.attr);
    if (!dump_kid) { return; }
    lm->incIndent(ctx.dn);
    dumpIRList(ATOMCAS_memory(ir), rg, (CHAR*)" memory", ctx.dumpflag);
    dumpIRList(ATOMCAS_oldval(ir), rg, (CHAR*)" oldval", ctx.dumpflag);
    dumpIRList(ATOMCAS_newval(ir), rg, (CHAR*)" newval", ctx.dumpflag);
    if (ATOMCAS_occupy(ir) != nullptr) {
        dumpIRList(ATOMCAS_occupy(ir), rg, (CHAR*)" occupy", ctx.dumpflag);
    }
    dumpIRList(ATOMCAS_multires(ir), rg, (CHAR*)" multi-res",
               ctx.dumpflag);
    lm->decIndent(ctx.dn);
}


void dumpBROADCAST(IR const* ir, Region const* rg, IRDumpCtx<> & ctx)
{
    bool dump_addr = ctx.dumpflag.have(IR_DUMP_ADDR);
    bool dump_kid = ctx.dumpflag.have(IR_DUMP_KID);
    StrBuf buf(64);
    TypeMgr const* xtm = rg->getTypeMgr();
    Type const* d = ir->getType();
    LogMgr * lm = rg->getLogMgr();
    note(rg, "%s:%s", IRNAME(ir), xtm->dump_type(d, buf));
    DUMPADDR(ir);
    prt(rg, "%s", ctx.attr);
    if (!dump_kid) { return; }
    lm->incIndent(ctx.dn);
    dumpIRList(BROADCAST_src(ir), rg, (CHAR*)" src", ctx.dumpflag);
    dumpIRList(BROADCAST_res_list(ir), rg, (CHAR*)" multi-res",
               ctx.dumpflag);
    lm->decIndent(ctx.dn);
}


void dumpVSTPR(IR const* ir, Region const* rg, IRDumpCtx<> & ctx)
{
    bool dump_addr = ctx.dumpflag.have(IR_DUMP_ADDR);
    bool dump_kid = ctx.dumpflag.have(IR_DUMP_KID);
    StrBuf buf(64);
    TypeMgr const* xtm = rg->getTypeMgr();
    Type const* d = ir->getType();
    LogMgr * lm = rg->getLogMgr();
    note(rg, "%s %s%d:%s", IRNAME(ir), PR_TYPE_CHAR, ir->getPrno(),
         xtm->dump_type(d, buf));
    DUMPADDR(ir);
    prt(rg, "%s", ctx.attr);
    if (!dump_kid) { return; }
    lm->incIndent(ctx.dn);
    dumpIRList(VSTPR_rhs(ir), rg, nullptr, ctx.dumpflag);
    dumpIRList(VSTPR_dummyuse(ir), rg, (CHAR*)" dummyuse", ctx.dumpflag);
    lm->decIndent(ctx.dn);
}


void dumpMaskOp(IR const* ir, Region const* rg, IRDumpCtx<> & ctx)
{
    bool dump_addr = ctx.dumpflag.have(IR_DUMP_ADDR);
    bool dump_kid = ctx.dumpflag.have(IR_DUMP_KID);
    StrBuf buf(64);
    TypeMgr const* xtm = rg->getTypeMgr();
    Type const* d = ir->getType();
    LogMgr * lm = rg->getLogMgr();
    note(rg, "%s:%s", IRNAME(ir), xtm->dump_type(d, buf));
    DUMPADDR(ir);
    prt(rg, "%s", ctx.attr);
    if (!dump_kid) { return; }
    lm->incIndent(ctx.dn);
    dumpIRList(MASKOP_op(ir), rg, nullptr, ctx.dumpflag);
    dumpIRList(MASKOP_mask(ir), rg, (CHAR*)" mask", ctx.dumpflag);
    lm->decIndent(ctx.dn);
}


void dumpMaskSelectToRes(IR const* ir, Region const* rg, IRDumpCtx<> & ctx)
{
    bool dump_addr = ctx.dumpflag.have(IR_DUMP_ADDR);
    bool dump_kid = ctx.dumpflag.have(IR_DUMP_KID);
    StrBuf buf(64);
    TypeMgr const* xtm = rg->getTypeMgr();
    Type const* d = ir->getType();
    LogMgr * lm = rg->getLogMgr();
    note(rg, "%s:%s", IRNAME(ir), xtm->dump_type(d, buf));
    DUMPADDR(ir);
    prt(rg, "%s", ctx.attr);
    if (!dump_kid) { return; }
    lm->incIndent(ctx.dn);
    dumpIRList(MASKSELECTTORES_op(ir), rg, nullptr, ctx.dumpflag);
    dumpIRList(MASKSELECTTORES_mask(ir), rg, (CHAR*)" mask", ctx.dumpflag);
    lm->decIndent(ctx.dn);
}

} //namespace xoc
