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

static void dumpFakeUse(LinearScanRA * lsra, IR const* fake_ir,
                        IR const* marker, PRNO prno,
                        UINT bbid, bool head, CHAR const* context)
{
    if (!lsra->getRegion()->isLogMgrInit()) { return; }
    CHAR const* preStr = "fake-use: insert fake-use IR id:";
    if (marker != nullptr) {
        lsra->getActMgr().dump(
            "%s %u of prno:%u before IR id:%u in bb:%u for %s", preStr,
            fake_ir->id(), prno, marker->id(), bbid, context);
        return;
    }
    lsra->getActMgr().dump(
        "%s %u of prno:%u at the %s of bb:%u for %s", preStr,
        fake_ir->id(), prno, head ? "head" : "tail", bbid, context);
}


//This func maps the position with the input attribute.
static void setPosAttr(BBPos const& pos, PosAttr const* attr,
                       MOD BBPos2Attr * pos2attr)
{
    ASSERT0(pos2attr && attr);
    //Check the duplicated insertion. At the moment, duplicated insertion for
    //same fake-use pr at the same BB position is meaningless.
    if (pos2attr->find(pos)) { return; }
    pos2attr->set(pos, attr);
}


void dumpBBListWithReg(Region const* rg)
{
    ASSERT0(rg && rg->getPassMgr());
    LinearScanRA const* lsra = (LinearScanRA const*)rg->getPassMgr()->
        queryPass(PASS_LINEAR_SCAN_RA);
    if (lsra == nullptr) { return; }
    lsra->dumpBBListWithReg();
}


void dumpDOTWithReg(Region const* rg, CHAR const* name, UINT flag)
{
    ASSERT0(rg && rg->getPassMgr());
    LinearScanRA const* lsra = (LinearScanRA const*)rg->getPassMgr()->
        queryPass(PASS_LINEAR_SCAN_RA);
    if (lsra == nullptr) { return; }
    lsra->dumpDOTWithReg(name, flag);
}


//
//START PosAttrProc
//
class PosAttrProc {
    COPY_CONSTRUCTOR(PosAttrProc);
protected:
    Region * m_rg;
    BBPos2Attr const& m_pos2attr;
    LinearScanRA * m_lsra;
public:
    PosAttrProc(Region * rg, BBPos2Attr const& ir2attr, LinearScanRA * lsra) :
        m_rg(rg), m_pos2attr(ir2attr), m_lsra(lsra)
    { }
    virtual ~PosAttrProc() {}

    virtual void preProcess() { }

    //This func shall traverse all the attribute map, and process every
    //attribute. This func is a template for the attribute processor, and
    //the derived class should not override this method.
    void process();
protected:
    //Check the attributes is related to the current attributes processor.
    //This function must be implemented by the derived class.
    virtual bool checkRelatedAttrs(PosAttr const* attr) const = 0;

    //Process attr at the specified pos. This function must be implemented
    //by the derived class.
    virtual bool processAttr(BBPos const& pos, PosAttr const* attr) = 0;
};


void PosAttrProc::process()
{
    preProcess();

    BBPos2AttrIter it;
    PosAttr const* attr = nullptr;
    for (BBPos pos = m_pos2attr.get_first(it, &attr); !it.end();
         pos = m_pos2attr.get_next(it, &attr)) {
        if (!checkRelatedAttrs(attr)) { continue; }
        processAttr(pos, attr);
    }
}
//END PosAttrProc


//
//START PosAttrNoCodeGenProc
//
//This class processes the attribute with POS_ATTR_NO_CODE_GEN.
class PosAttrNoCodeGenProc : public PosAttrProc {
    COPY_CONSTRUCTOR(PosAttrNoCodeGenProc);
public:
    PosAttrNoCodeGenProc(Region * rg, BBPos2Attr const& ir2attr,
                         LinearScanRA * lsra) :
        PosAttrProc(rg, ir2attr, lsra)
    { }
    virtual ~PosAttrNoCodeGenProc() {}

protected:
    virtual bool checkRelatedAttrs(PosAttr const* attr) const
    {
        ASSERT0(attr);
        return attr->have(POS_ATTR_NO_CODE_GEN);
    }

    virtual bool processAttr(BBPos const& pos, PosAttr const* attr)
    {
        ASSERT0(attr);
        if (!processAttrNoCodeGen(pos, attr)) { return false; }
        return true;
    }

    bool processAttrNoCodeGen(BBPos const& pos, PosAttr const* attr);
};


bool PosAttrNoCodeGenProc::processAttrNoCodeGen(BBPos const& pos,
                                                PosAttr const* attr)
{
    ASSERT0(attr);
    if (!attr->have(POS_ATTR_NO_CODE_GEN)) { return true; }
    IR * tmp_ir = const_cast<IR*>(attr->getIR());

    //Check the fake-use IR responding at the pos.
    ASSERT0(tmp_ir && tmp_ir->is_st());
    ASSERT0(tmp_ir->getRHS() && tmp_ir->getRHS()->is_pr());
    ASSERT0(m_lsra->isPrnoAlias(tmp_ir->getRHS()->getPrno(), BBPOS_prno(pos)));
    IRBB const* bb = attr->getBB();
    ASSERT0(bb && BBPOS_bbid(pos) == bb->id());
    tmp_ir = BB_irlist(const_cast<IRBB*>(bb)).remove(tmp_ir);
    m_rg->freeIRTree(tmp_ir);
    return true;
}
//END PosAttrNoCodeGenProc


class OccRecorderVF {
    LexBackwardJumpAnalysis * m_ana;
public:
    Pos pos;
public:
    OccRecorderVF(LexBackwardJumpAnalysis * ana) : m_ana(ana), pos(POS_UNDEF) {}
    bool visitIR(IR const* ir, OUT bool & is_term)
    {
        if (ir->is_pr() || ir->is_stpr()) {
            m_ana->recordOccurenceForPR(ir->getPrno(), pos);
        }
        return true;
    }
};


//
//START OccRecorder
//
class OccRecorder : public VisitIRTree<OccRecorderVF> {
    COPY_CONSTRUCTOR(OccRecorder);
public:
    OccRecorder(IR const* ir, OccRecorderVF & vf) : VisitIRTree(vf)
    {
        ASSERT0(ir);
        visit(ir);
    }
};
//END OccRecorder


//
//START BackwardJumpAnalysisResMgr
//
BackwardJumpAnalysisResMgr::BackwardJumpAnalysisResMgr()
{
    UINT const mem_pool_init_size = 64;
    m_pool = smpoolCreate(mem_pool_init_size, MEM_COMM);
    ASSERT0(m_pool);
}


BackwardJumpAnalysisResMgr::~BackwardJumpAnalysisResMgr()
{
    if (m_pool != nullptr) {
        smpoolDelete(m_pool);
        m_pool = nullptr;
    }
}


void * BackwardJumpAnalysisResMgr::xmalloc(size_t size)
{
    ASSERTN(m_pool != nullptr, ("pool does not initialized"));
    void * p = smpoolMalloc(size, m_pool);
    ASSERT0(p != nullptr);
    ::memset(p, 0, size);
    return p;
}


PosAttr * BackwardJumpAnalysisResMgr::genPosAttr(UINT v, IRBB const* bb,
                                                 IR const* ir)
{
    ASSERT0(bb && ir);
    PosAttr * pos_attr = (PosAttr*)xmalloc(sizeof(PosAttr));
    pos_attr->init(v, bb, ir);
    return pos_attr;
}


LexBackwardJump * BackwardJumpAnalysisResMgr::genLexBackwardJump(
    IRBB const* srcbb, IRBB const* dstbb)
{
    ASSERT0(srcbb && dstbb);
    LexBackwardJump * e = (LexBackwardJump*)xmalloc(sizeof(LexBackwardJump));
    e->init(srcbb, dstbb);
    return e;
}
//END BackwardJumpAnalysisResMgr

//
//START FakeVarMgr
//
Var * FakeVarMgr::genFakeVar(Type const* ty)
{
    ASSERT0(ty);
    // The fake-use IR can use the same fake var as a flag.
    if (ty->is_vector()) {
        if (m_fake_vec_var != nullptr) { return m_fake_vec_var; }
        m_fake_vec_var = REGION_region_mgr(m_rg)->getVarMgr()->registerVar(
            "#fake_vec_var", ty, 1, VAR_LOCAL | VAR_FAKE, SS_UNDEF);
        return m_fake_vec_var;
    }
    if (m_fake_scalar_var == nullptr) {
        m_fake_scalar_var = REGION_region_mgr(m_rg)->getVarMgr()->registerVar(
            "#fake_scalar_var", ty, 1, VAR_LOCAL | VAR_FAKE, SS_UNDEF);
    }
    return m_fake_scalar_var;
}
//END FakeVarMgr

//
//START LexBackwardJumpAnalysis
//
LexBackwardJumpAnalysis::LexBackwardJumpAnalysis(Region * rg,
    BBPos2Attr * pos2attr, FakeVarMgr * fake_var_mgr, LinearScanRA * lsra)
{
    ASSERT0(rg && pos2attr && fake_var_mgr && lsra);
    m_rg = rg;
    m_pos2attr = pos2attr;
    m_bb_list = nullptr;
    m_irmgr = rg->getIRMgr();
    m_live_mgr = nullptr;
    m_fake_var_mgr = fake_var_mgr;
    m_resource_mgr = nullptr;
    m_lsra = lsra;
    init();
}


LexBackwardJumpAnalysis::~LexBackwardJumpAnalysis()
{
    destroy();
}


void LexBackwardJumpAnalysis::reset()
{
    destroy();
    init();
}


void LexBackwardJumpAnalysis::init()
{
    m_pos2attr->init();
    m_prno2occ.init();
    m_pr2fakeuse_head.init();
    m_pr2fakeuse_tail.init();
    m_backward_edges.init();
    if (m_resource_mgr == nullptr) {
        m_resource_mgr = new BackwardJumpAnalysisResMgr();
    }
}


void LexBackwardJumpAnalysis::destroy()
{
    m_pos2attr->destroy();
    m_backward_edges.destroy();
    m_prno2occ.clean();
    m_bb_entry_pos.clean();
    m_bb_exit_pos.clean();
    m_pr2fakeuse_head.clean();
    m_pr2fakeuse_tail.clean();
    if (m_resource_mgr != nullptr) {
        delete m_resource_mgr;
        m_resource_mgr = nullptr;
    }
}


void LexBackwardJumpAnalysis::recordOccurenceForPR(PRNO prno, Pos pos)
{
    //The occurence information of each prno contains the first position
    //and the last position only in the IRBB List. The last position need to be
    //updated if the previous record position is less than the new position.
    //Here is an example:
    // prno pos:|1   4   7  9  12 13|
    //          | ----   ---    --  |
    //
    // the final occ is: first = 1, last = 13
    ASSERT0(prno != PRNO_UNDEF);
    bool find = false;
    Occurence * occ = const_cast<Occurence*>(m_prno2occ.get(prno, &find));
    if (find) {
        ASSERT0(occ);
        ASSERT0(OCC_first(occ) != POS_UNDEF);
        OCC_last(occ) = MAX(OCC_last(occ), pos);
        m_prno2occ.setAlways(prno, occ);
        return;
    }

    occ = m_resource_mgr->genOccurence();
    ASSERT0(occ);
    OCC_first(occ) = pos;
    OCC_last(occ) = pos;
    m_prno2occ.set(prno, occ);
}


//Determine an edge is backward jump or not.
static bool isBackwardJump(
    Vector<UINT> const& bb_seqid, UINT src_bbid, UINT dst_bbid)
{
    return bb_seqid[dst_bbid] <= bb_seqid[src_bbid];
}


void LexBackwardJumpAnalysis::collectBackwardJumps(Vector<UINT> const& bb_seqid)
{
    BBListIter bbit;
    for (IRBB const* bb = m_bb_list->get_head(&bbit);
         bb != nullptr; bb = m_bb_list->get_next(&bbit)) {
        AdjVertexIter ito;
        for (Vertex const* o = Graph::get_first_out_vertex(bb->getVex(), ito);
             o != nullptr; o = Graph::get_next_out_vertex(ito)) {
            if (!isBackwardJump(bb_seqid, bb->id(), o->id())) {
                //Edge bb->o is not lex-backward-jump.
                continue;
            }
            addBackwardJump(bb, m_rg->getBB(o->id()));
        }
    }
}


void LexBackwardJumpAnalysis::generateOccurenceForBB(
    IRBB const* bb, MOD Pos & pos)
{
    ASSERT0(bb);
    BBIRList const& irlst = const_cast<IRBB*>(bb)->getIRList();

    if (irlst.is_empty()) {
        //If the irlist is empty, increment the position, and record the
        //entry and exit pos of BB.
        pos++;
        m_bb_entry_pos.set(bb->id(), pos);
        m_bb_exit_pos.set(bb->id(), pos);
        return;
    }
    BBIRListIter bbirit;
    m_bb_entry_pos.set(bb->id(), pos + 1);
    OccRecorderVF vf(this);
    for (IR * ir = irlst.get_head(&bbirit); ir != nullptr;
         ir = irlst.get_next(&bbirit)) {
        vf.pos = ++pos;
        OccRecorder occurence_recorder(ir, vf);
    }
    m_bb_exit_pos.set(bb->id(), pos);
}


void LexBackwardJumpAnalysis::generateOccurence()
{
    BBListIter bbit;
    Pos pos = POS_UNDEF;
    for (IRBB * bb = m_bb_list->get_head(&bbit); bb != nullptr;
         bb = m_bb_list->get_next(&bbit)) {
        generateOccurenceForBB(bb, pos);
    }
}


void LexBackwardJumpAnalysis::assignLexSeqIdForBB(OUT Vector<UINT> & bb_seqid)
{
    //Iterate BB list.
    BBListIter bbit;
    bb_seqid.set(m_bb_list->get_elem_count(), 0);
    UINT id = 0;
    for (IRBB * bb = m_bb_list->get_head(&bbit); bb != nullptr;
         bb = m_bb_list->get_next(&bbit)) {
        bb_seqid.set(bb->id(), id++);
    }
}


bool LexBackwardJumpAnalysis::analyze()
{
    START_TIMER_FMT(t, ("Backward Jump Analysis"));
    m_live_mgr = (LivenessMgr*)m_rg->getPassMgr()->
        queryPass(PASS_PRLIVENESS_MGR);
    m_bb_list = m_rg->getBBList();
    if (m_bb_list == nullptr || m_bb_list->get_elem_count() == 0) {
        END_TIMER_FMT(t, ("Backward Jump Analysis"));
        return true;
    }
    Vector<UINT> bb_seqid;
    assignLexSeqIdForBB(bb_seqid);

    //Step1: Collect the backward jump info in the CFG.
    collectBackwardJumps(bb_seqid);

    //If there is no backward jump, nothing need to do.
    if (m_backward_edges.get_elem_count() == 0) {
        END_TIMER_FMT(t, ("Backward Jump Analysis"));
        return true;
    }

    //Step2: Generate the simple occurence info for the relative live PRs.
    generateOccurence();

    //Step3: Generate the fake-use IR info.
    generateFakeUse();

    //Step4: Insert the fake-use IR.
    insertFakeUse();
    END_TIMER_FMT(t, ("Backward Jump Analysis"));
    return true;
}


void LexBackwardJumpAnalysis::dump()
{
    note(m_rg, "\n==-- DUMP LexBackwardJumpAnalysis --==");

    Prno2OccMapIter it;
    Occurence const* occ = nullptr;
    note(m_rg, "\nPR occ interval:\n");
    for (PRNO pr = m_prno2occ.get_first(it, &occ); occ != nullptr;
         pr = m_prno2occ.get_next(it, &occ)) {
        ASSERT0(OCC_first(occ) != POS_UNDEF);
        note(m_rg, "PR %u, [%u, %u]\n", pr, OCC_first(occ), OCC_last(occ));
    }

    note(m_rg, "\nBackward jump BB interval:\n");
    for (LexBackwardJump const* e = m_backward_edges.get_head();
         e != nullptr; e = m_backward_edges.get_next()) {
        UINT src_bb = BKJUMP_srcbb(e)->id();
        UINT dst_bb = BKJUMP_dstbb(e)->id();
        note(m_rg, "SRC bb: %u [%u, %u] ----> DST bb: %u [%u, %u]\n",
             src_bb, m_bb_entry_pos[src_bb], m_bb_exit_pos[src_bb],
             dst_bb, m_bb_entry_pos[dst_bb], m_bb_exit_pos[dst_bb]);
    }

    note(m_rg, "\nfake-use IR insert details:\n");
    BBPos2AttrIter iter;
    PosAttr const* attr = nullptr;
    for (BBPos pos = m_pos2attr->get_first(iter, &attr); !iter.end();
         pos = m_pos2attr->get_next(iter, &attr)) {
        ASSERT0(attr);
        UINT bbid = BBPOS_bbid(pos);
        UINT insert_flag = BBPOS_flag(pos);
        UINT prno = BBPOS_prno(pos);
        note(m_rg, "insert fake IR for prno %u in BB %u at %s\n", prno, bbid,
             (insert_flag == INSERT_MODE_TAIL) ? "tail" : "head");
    }
}


//This func inserts the fake-use IR at BB entry per the steps below:
//  1. Build a fake-use IR by write the pr into a fake var.
//  2. Insert the fake-use IR at the entry of BB.
//  3. Map the pos and the new fake-use IR with three attributes:
//        --- POS_ATTR_NO_CODE_GEN
//        --- POS_ATTR_LT_NO_TERM_AFTER
//        --- POS_ATTR_LT_SHRINK_BEFORE
void LexBackwardJumpAnalysis::insertFakeUseAtBBEntry(
    IRBB const* bb, PRNO prno, BBPos const& pos)
{
    ASSERT0(bb);
    ASSERT0(prno != PRNO_UNDEF);
    Type const* ty = m_lsra->getVarTypeOfPRNO(prno);
    IR * pr_ir = m_irmgr->buildPRdedicated(prno, ty);
    IR * st = m_irmgr->buildStore(m_fake_var_mgr->genFakeVar(ty), ty, pr_ir);
    const_cast<IRBB*>(bb)->getIRList().append_head(st);
    m_lsra->setFakeUseAtLexFirstBBInLoop(st);

    //POS_ATTR_LT_SHRINK_BEFORE is used to avoid the lifetime of PR is from the
    //entry of region because the first occurence of this PR is a USE.
    //e.g. the lifetime of a PR
    //   Original lifetime: <2-17><34-67>
    //    | ----------------                ----------------------------------
    //    |                u                d      u           u             u
    //
    //   After POS_ATTR_LT_SHRINK_BEFORE is processed:
    //
    //   Modified lifetime: <17><34-67>
    //    |                -                ----------------------------------
    //    |                u                d      u           u             u
    PosAttr * attr = m_resource_mgr->genPosAttr(POS_ATTR_NO_CODE_GEN, bb, st);
    setPosAttr(pos, attr, m_pos2attr);
    dumpFakeUse(m_lsra, st, nullptr, prno, bb->id(), true,
                "backward jump analysis");
}


//This func inserts the fake-use IR at BB exit per the steps below:
//  1. Build a fake-use IR by write the pr into a fake var.
//  2. Insert the fake-use IR at the tail of BB, but before the branch IR.
//  3. Map the pos and the new fake-use IR with attribute POS_ATTR_NO_CODE_GEN.
void LexBackwardJumpAnalysis::insertFakeUseAtBBExit(
    IRBB const* bb, PRNO prno, BBPos const& pos)
{
    ASSERT0(bb);
    ASSERT0(prno != PRNO_UNDEF);
    IR const* tail = const_cast<IRBB*>(bb)->getIRList().get_tail();
    ASSERT0(tail);
    if (!tail->isBranch()) { return; }

    //Insert before the branch.
    Type const* ty = m_lsra->getVarTypeOfPRNO(prno);
    IR * pr_ir = m_irmgr->buildPRdedicated(prno, ty);
    IR * st = m_irmgr->buildStore(m_fake_var_mgr->genFakeVar(ty), ty, pr_ir);
    const_cast<IRBB*>(bb)->getIRList().insert_before(st, tail);

    //Store the fake-use IR in the table, because the fake-use IR at the
    //last BB of loop by lexicographical order will not particaipated into
    //the register allocation, and this table will be used to identify such
    //kind of fake-use IR.
    m_lsra->setFakeUseAtLexLastBBInLoop(st);

    //Attribute POS_ATTR_LT_EXTEND_BB_END is set, because the multiple fake-use
    //IRs are inserted at the tail part of the same BB, which would lead to
    //this lifetime cannot live to the real ending position of BB.
    PosAttr * attr = m_resource_mgr->genPosAttr(POS_ATTR_NO_CODE_GEN, bb, st);
    setPosAttr(pos, attr, m_pos2attr);
    dumpFakeUse(m_lsra, st, nullptr, prno, bb->id(), false,
                "backward jump analysis");
}


void LexBackwardJumpAnalysis::insertFakeUse(
    IRBB const* bb, PRNO prno, INSERT_MODE mode)
{
    ASSERT0(bb);
    ASSERT0(mode != INSERT_MODE_UNDEF);
    ASSERT0(prno != PRNO_UNDEF);

    //Generate a customerized number to represent the position. This number
    //is unique for any pr and bbid with a sepcified INSERT_MODE, which
    //could avoid the duplicate insert for the same IR at a same BB.
    BBPos bbpos(prno, bb->id(), mode);

    //Check the duplicated insertion. At the moment, duplicated insertion for
    //same fake-use pr at the same BB position is meaningless.
    if (m_pos2attr->find(bbpos)) { return; }

    switch (mode) {
    case INSERT_MODE_HEAD: insertFakeUseAtBBEntry(bb, prno, bbpos); break;
    case INSERT_MODE_TAIL: insertFakeUseAtBBExit(bb, prno, bbpos); break;
    default: UNREACHABLE(); break;
    }
}


void LexBackwardJumpAnalysis::recordFakeUse(
    PRNO prno, IRBB const* bb, INSERT_MODE mode)
{
    ASSERT0(bb);
    ASSERT0(prno != PRNO_UNDEF);
    UINT bbid = bb->id();
    ASSERT0(bbid != BBID_UNDEF);

    if (mode == INSERT_MODE_HEAD) {
        bool find = false;
        FakeUse * fakeuse = m_pr2fakeuse_head.get(prno, &find);
        if (!find) {
            fakeuse = m_resource_mgr->genFakeUse();
            FAKEUSE_bb(fakeuse) = bb;
            FAKEUSE_pos(fakeuse) = m_bb_entry_pos[bbid];
            m_pr2fakeuse_head.set(prno, fakeuse);
            return;
        }
        if (m_bb_entry_pos[bbid] >= FAKEUSE_pos(fakeuse)) { return; }
        FAKEUSE_bb(fakeuse) = bb;
        FAKEUSE_pos(fakeuse) = m_bb_exit_pos[bbid];
        return;
    }

    if (mode == INSERT_MODE_TAIL) {
        bool find = false;
        FakeUse * fakeuse = m_pr2fakeuse_tail.get(prno, &find);
        if (!find) {
            fakeuse = m_resource_mgr->genFakeUse();
            FAKEUSE_bb(fakeuse) = bb;
            FAKEUSE_pos(fakeuse) = m_bb_exit_pos[bbid];
            m_pr2fakeuse_tail.set(prno, fakeuse);
            return;
        }
        if (m_bb_exit_pos[bbid] <= FAKEUSE_pos(fakeuse)) { return; }
        FAKEUSE_bb(fakeuse) = bb;
        FAKEUSE_pos(fakeuse) = m_bb_exit_pos[bbid];
        return;
    }
    UNREACHABLE();
}


bool LexBackwardJumpAnalysis::canSkipPreAssignedReg(PRNO prno) const
{
    ASSERT0(m_lsra->isPreAssigned(prno));
    Reg reg = m_lsra->getPreAssignedReg(prno);
    return m_lsra->isZeroRegister(reg);
}


void LexBackwardJumpAnalysis::generateFakeUse()
{
    for (LexBackwardJump const* e = m_backward_edges.get_head();
         e != nullptr; e = m_backward_edges.get_next()) {
        IRBB const* src_bb = BKJUMP_srcbb(e);
        IRBB const* dst_bb = BKJUMP_dstbb(e);
        ASSERT0(src_bb && dst_bb);

        //For the source BB of the backward jump edge, prno of the live out
        //need to be processed. If the last occurence of the pr is before the
        //end of BB, the fake-use of pr will be inserted at the tail of source
        //BB.
        // e.g case: the last occurence of the p1 is before the end of BB4.
        //
        //                    BB0:int p1 =
        //                        |
        //                        v
        //      |----------->  BB1:p1 < XX
        //      |                |     |
        //      |           _____|     |
        //      |           |          |
        //      |           v          v
        //      |       BB2:p1 =  | BB3:ret
        //      |            |
        //      |            v
        //      |          BB4:...
        //      |              int p2 =
        //      |              ...
        //      |                <-------- insert 'fake_var = p1' here.
        //      |______________goto BB1
        PRLiveSet const* live_out = m_live_mgr->get_liveout(src_bb->id());
        ASSERT0(live_out);
        PRLiveSetIter * iter = nullptr;
        for (BSIdx pr = (PRNO)live_out->get_first(&iter);
             pr != BS_UNDEF; pr = live_out->get_next(pr, &iter)) {
            if (m_lsra->isPreAssigned(pr) && canSkipPreAssignedReg(pr)) {
                continue;
            }
            Occurence const* occ = m_prno2occ.get(pr);
            ASSERT0(occ);
            //Since some new fake-use IRs will be inserted at the end of the
            //src BB, so the exit boundary of src BB should be included.
            if (OCC_last(occ) < m_bb_exit_pos[src_bb->id()]) {
                recordFakeUse(pr, src_bb, INSERT_MODE_TAIL);
            }
        }

        //For the destination BB of the backward jump edge, prno of the live in
        //need to be processed. If the first occurence of the pr is after the
        //end of BB, the fake-use IR of pr will be inserted at the head of
        //destination BB.
        // e.g case: first occurence of the p0 is after the entry of BB1.
        //
        //                    BB0:int p0
        //                        int p1 =
        //                        |
        //                        v
        //      |-----------> BB1:  <-------- insert 'fake_var = p0' here.
        //      |                 ...
        //      |                 int p2 =
        //      |                 p1 < XX
        //      |                 |     |
        //      |           ______|     |
        //      |           |           |
        //      |           v           v
        //      |        BB2:p1 =    | BB3:ret
        //      |            p0 = p1
        //      |            |
        //      |            v
        //      |          BB4:...
        //      |              int p3 = p0
        //      |              p1 =
        //      |______________goto BB1
        PRLiveSet const* live_in = m_live_mgr->get_livein(dst_bb->id());
        ASSERT0(live_in);
        iter = nullptr;
        for (PRNO pr = (PRNO)live_in->get_first(&iter);
             pr != BS_UNDEF; pr = (PRNO)live_in->get_next(pr, &iter)) {
            Occurence const* occ = m_prno2occ.get(pr);
            ASSERT0(occ);
            if (m_lsra->getPreAssignedMgr().isPreAssigned(pr)) { continue; }
            //Since some new fake-use IRs will be inserted at the start of the
            //dst BB, so the entry boundary of the dst BB should be included.
            if (OCC_first(occ) >= m_bb_entry_pos[dst_bb->id()]) {
                recordFakeUse(pr, dst_bb, INSERT_MODE_HEAD);
            }
        }
    }
}


void LexBackwardJumpAnalysis::insertFakeUse()
{
    Prno2FakeUseIter it;
    FakeUse * fake = nullptr;
    for (PRNO pr = m_pr2fakeuse_head.get_first(it, &fake); fake != nullptr;
         pr = m_pr2fakeuse_head.get_next(it, &fake)) {
        insertFakeUse(FAKEUSE_bb(fake), pr, INSERT_MODE_HEAD);
    }

    for (PRNO pr = m_pr2fakeuse_tail.get_first(it, &fake); fake != nullptr;
         pr = m_pr2fakeuse_tail.get_next(it, &fake)) {
        insertFakeUse(FAKEUSE_bb(fake), pr, INSERT_MODE_TAIL);
    }
}
//END LexBackwardJumpAnalysis


//
//START PRNOConstraintsTab
//
bool PRNOConstraintsTab::hasTwoOrMoreCommonElements(
    PRNOConstraintsTab const& src_tab) const
{
    PRNOConstraintsTabIter it;
    UINT common_count = 0;
    for (PRNO pr = src_tab.get_first(it); pr != PRNO_UNDEF;
         pr = src_tab.get_next(it)) {
        if (!find(pr)) { continue; }
        common_count++;
        if (common_count >= 2) { return true; }
    }
    return false;
}
//END PRNOConstraintsTab


//
//START RegSetImpl
//
RegSetImpl::RegSetImpl(LinearScanRA & ra) : m_ra(ra)
{
    m_target_callee_scalar = nullptr;
    m_target_caller_scalar = nullptr;
    m_target_param_scalar = nullptr;
    m_target_return_value_scalar = nullptr;
    m_target_callee_vector = nullptr;
    m_target_caller_vector = nullptr;
    m_target_param_vector = nullptr;
    m_target_return_value_vector = nullptr;
    m_target_allocable_scalar = nullptr;
    m_target_allocable_vector = nullptr;
}


TargInfoMgr & RegSetImpl::getTIMgr() const
{
    return m_ra.getTIMgr();
}


void RegSetImpl::destroyDebugRegSet()
{
    if (m_target_callee_scalar != nullptr) {
        delete m_target_callee_scalar;
        m_target_callee_scalar = nullptr;
    }
    if (m_target_caller_scalar != nullptr) {
        delete m_target_caller_scalar;
        m_target_caller_scalar = nullptr;
    }
    if (m_target_param_scalar != nullptr) {
        delete m_target_param_scalar;
        m_target_param_scalar = nullptr;
    }
    if (m_target_return_value_scalar != nullptr) {
        delete m_target_return_value_scalar;
        m_target_param_scalar = nullptr;
    }

    if (m_target_callee_vector != nullptr) {
        delete m_target_callee_vector;
        m_target_callee_vector = nullptr;
    }
    if (m_target_caller_vector != nullptr) {
        delete m_target_caller_vector;
        m_target_caller_vector = nullptr;
    }
    if (m_target_param_vector != nullptr) {
        delete m_target_param_vector;
        m_target_param_vector = nullptr;
    }
    if (m_target_return_value_vector != nullptr) {
        delete m_target_return_value_vector;
        m_target_return_value_vector = nullptr;
    }

    if (m_target_allocable_scalar != nullptr) {
        delete m_target_allocable_scalar;
        m_target_allocable_scalar = nullptr;
    }
    if (m_target_allocable_vector != nullptr) {
        delete m_target_allocable_vector;
        m_target_allocable_vector = nullptr;
    }
}


void RegSetImpl::destroyRegSet()
{
    if (!g_do_lsra_debug) { return; }
    destroyDebugRegSet();
}


void RegSetImpl::initRegSet()
{
    if (g_do_lsra_debug) {
        initDebugRegSet();
    } else {
        m_target_callee_scalar = getTIMgr().getCalleeScalarRegSet();
        m_target_caller_scalar = getTIMgr().getCallerScalarRegSet();
        m_target_param_scalar = getTIMgr().getParamScalarRegSet();
        m_target_return_value_scalar = getTIMgr().getRetvalScalarRegSet();
        m_target_allocable_scalar = getTIMgr().getAllocableScalarRegSet();
        m_target_callee_vector = getTIMgr().getCalleeVectorRegSet();
        m_target_caller_vector = getTIMgr().getCallerVectorRegSet();
        m_target_param_vector = getTIMgr().getParamVectorRegSet();
        m_target_return_value_vector = getTIMgr().getRetvalVectorRegSet();
        m_target_allocable_vector = getTIMgr().getAllocableVectorRegSet();
    }
    initAvailRegSet();
}


void RegSetImpl::initAvailRegSet()
{
    if (m_target_callee_scalar != nullptr) {
        m_avail_callee_scalar.copy(*m_target_callee_scalar);
    }
    if (m_target_caller_scalar != nullptr) {
        m_avail_caller_scalar.copy(*m_target_caller_scalar);
    }
    if (m_target_param_scalar != nullptr) {
        m_avail_param_scalar.copy(*m_target_param_scalar);
    }
    if (m_target_return_value_scalar != nullptr) {
        m_avail_return_value_scalar.copy(*m_target_return_value_scalar);
    }
    if (m_target_allocable_scalar != nullptr) {
        m_avail_allocable.bunion(*m_target_allocable_scalar);
    }
    if (m_target_callee_vector!= nullptr) {
        m_avail_callee_vector.copy(*m_target_callee_vector);
    }
    if (m_target_caller_vector != nullptr) {
        m_avail_caller_vector.copy(*m_target_caller_vector);
    }
    if (m_target_param_vector != nullptr) {
        m_avail_param_vector.copy(*m_target_param_vector);
    }
    if (m_target_return_value_vector != nullptr) {
        m_avail_return_value_vector.copy(*m_target_return_value_vector);
    }
    if (m_target_allocable_vector != nullptr) {
        m_avail_allocable.bunion(*m_target_allocable_vector);
    }

    //Try to collect available registers occupied by other modules.
    collectOtherAvailableRegister();
}


void RegSetImpl::pickOutAliasRegSet(Reg reg, OUT RegSet & alias_set)
{
    ASSERT0(reg != REG_UNDEF);
    ASSERTN(0, ("Target Dependent Code"));
}


//Pick up a physical register from allocable register set by the incremental
//order.
Reg RegSetImpl::pickRegByIncrementalOrder(RegSet & set)
{
    BSIdx i = set.get_first();
    if (i == BS_UNDEF) {
        return REG_UNDEF;
    }
    set.diff(i);
    return (Reg)i;
}


//Pick up a physical register from allocable register set.
Reg RegSetImpl::pickReg(RegSet & set)
{
    return pickRegByIncrementalOrder(set);
}


void RegSetImpl::pickReg(RegSet & set, Reg r)
{
    if (set.is_contain(r)) {
        set.diff(r);
    }
}


Reg RegSetImpl::handleOnlyConsistency(
    OUT RegSet & set, PRNOConstraintsTab const& consist_prs)
{
    PRNOConstraintsTabIter it;

    //Check in consist_prs whether a register has already been allocated.
    //If a register has been allocated, return it directly.
    for (PRNO pr = consist_prs.get_first(it); pr != PRNO_UNDEF;
         pr = consist_prs.get_next(it)) {
        Reg r = m_ra.getReg(pr);
        if (r != REG_UNDEF) {
            return r;
        }
    }

    //Attempt to find an available register from the set.
    BSIdx available_reg = set.get_first();
    if (available_reg != BS_UNDEF) {
        set.diff(available_reg);
        return (Reg)available_reg;
    }
    return REG_UNDEF;
}


void RegSetImpl::removeConflictingReg(
    OUT RegSet & set, PRNOConstraintsTab const& conflict_prs,
    OUT RegSetWrap & removed_regs_wrap)
{

    PRNOConstraintsTabIter it;
    PRNO pr = conflict_prs.get_first(it);
    if (pr != PRNO_UNDEF) {
        removed_regs_wrap.alloc();
    }

    //Traverse conflict_prs and delete its registers from the set.
    //`set` represents the available (free) register set. It contains
    //registers that can be allocated. Suppose the conflicting PRs are
    //$2 and $3. This may lead to the following cases:
    //Case 1: If `set = {R2, R3, R4, R5, R6}`, and $2 is assigned to
    //register R2, and R2 is still in the free set. When allocating
    //a register for $3, we must remove R2 from the free set and select
    //one from the remaining set {R3, R4, R5, R6}. Additionally, we
    //record the removal of R2.
    //
    //Case 2: If $2 is assigned to register R2, but the free set is
    //`{R3, R4, R5, R6}` (meaning R2 is no longer in the free set), we
    //can freely select a register from the set without needing to record
    //any information.
    for (; pr != PRNO_UNDEF; pr = conflict_prs.get_next(it)) {
        Reg r = m_ra.getReg(pr);

        //The current PR has already been assigned a register,
        //and the assigned register is in the free set.
        if (r != REG_UNDEF && set.is_contain(r)) {
            set.diff(r);
            ASSERT0(removed_regs_wrap.getRegSet());
            removed_regs_wrap.getRegSet()->bunion(r);
        }
    }
}


Reg RegSetImpl::handleOnlyConflicts(
    OUT RegSet & set, PRNOConstraintsTab const& conflict_prs)
{
    RegSetWrap removed_regs_wrap;

    //Remove conflicting registers from the set to avoid conflicts.
    //Example 1: set = {r2, r3, r4}, removed_regs_wrap = {r2}.
    //Example 2: set = {r2}, removed_regs_wrap = {r2}.
    removeConflictingReg(set, conflict_prs, removed_regs_wrap);

    //Attempt to find an available register from the set after
    //removing conflict set.
    BSIdx available_reg = set.get_first();

    //Immediately restore previously removed conflicting registers
    //to ensure the set remains complete after conflict resolution.
    //In Example 1 and Example 2, r2 will be restored back to set.
    RegSet const* rs = removed_regs_wrap.getRegSet();
    if (rs != nullptr) { set.bunion(*rs); }

    if (available_reg != BS_UNDEF) {
        set.diff(available_reg);
        return (Reg)available_reg;
    }
    return REG_UNDEF;
}


Reg RegSetImpl::handleConflictsAndConsistency(
    OUT RegSet & set, PRNOConstraintsTab const& conflict_prs,
    PRNOConstraintsTab const& consist_prs)
{
    //Check for conflicts between conflict_prs and consist_prs.
    //conflict_prs contains elements like {pr0, pr1, pr2},
    //indicating that these registers cannot be allocated
    //to the same physical register in the future.
    //consist_prs contains elements like {pr1, pr2},
    //indicating that pr1 and pr2 must be allocated to the
    //same physical register. If there are common elements
    //between these two sets,
    //it represents a conflict and violates the intended semantics.
    ASSERT0(!conflict_prs.hasTwoOrMoreCommonElements(consist_prs));
    PRNOConstraintsTabIter it;

    //Check if any register has already been allocated in consist_prs
    //Return the allocated register directly.
    for (PRNO pr = consist_prs.get_first(it); pr != PRNO_UNDEF;
         pr = consist_prs.get_next(it)) {
        Reg r = m_ra.getReg(pr);
        if (r != REG_UNDEF) { return r; }
    }

    //If no registers were allocated in consist_prs, handle conflict set.
    return handleOnlyConflicts(set, conflict_prs);
}


Reg RegSetImpl::pickRegWithConstraints(
    OUT RegSet & set, LTConstraints const* lt_constraints)
{
    ASSERT0(lt_constraints);

    PRNOConstraintsTab const& conflict_prs = lt_constraints->getConflictTab();
    PRNOConstraintsTab const& consist_prs = lt_constraints->getConsistTab();

    //Check different conditions based on the state of
    //conflict_prs and consist_prs.
    //1. If both conflict_prs and consist_prs are not empty, handle that case.
    //2. If only conflict_prs is not empty, handle that case.
    //3. If only consist_prs is not empty, handle that case.
    //4. If both are empty, assert an error (indicating an unexpected state).
    if (!conflict_prs.is_empty() && !consist_prs.is_empty()) {
        return handleConflictsAndConsistency(set, conflict_prs, consist_prs);
    }

    if (!conflict_prs.is_empty()) {
        return handleOnlyConflicts(set, conflict_prs);
    }

    if (!consist_prs.is_empty()) {
        return handleOnlyConsistency(set, consist_prs);
    }

    ASSERT0(0); return REG_UNDEF;
}


void RegSetImpl::pickRegFromAllocable(Reg reg)
{
    ASSERT0(isAvailAllocable(reg));
    return pickRegisterFromAliasSet(reg);
}


void RegSetImpl::recordUsedCallee(Reg reg)
{
    ASSERT0(isAvailAllocable(reg));
    ASSERT0(isCallee(reg));
    m_used_callee.bunion(reg);
}


void RegSetImpl::recordUsedCaller(Reg reg)
{
    ASSERT0(isAvailAllocable(reg));
    ASSERT0(isCaller(reg));
    m_used_caller.bunion(reg);
}


Reg RegSetImpl::pickCallee(IR const* ir, LTConstraints const* lt_constraints)
{
    RegSet & set = ir->is_vec() ? m_avail_callee_vector :
                                  m_avail_callee_scalar;
    if (lt_constraints != nullptr) {
        return pickRegWithConstraints(set, lt_constraints);
    }
    return pickReg(set);
}


Reg RegSetImpl::pickCaller(IR const* ir, LTConstraints const* lt_constraints)
{
    RegSet & set = ir->is_vec() ? m_avail_caller_vector :
                                  m_avail_caller_scalar;
    if (lt_constraints != nullptr) {
        return pickRegWithConstraints(set, lt_constraints);
    }
    return pickReg(set);
}


void RegSetImpl::pickRegisterFromAliasSet(Reg r)
{
    ASSERT0(isAvailAllocable(r));
    if (isCallee(r)) {
        return pickRegisterFromCalleeAliasSet(r);
    }
    if (isCaller(r)) {
        return pickRegisterFromCallerAliasSet(r);
    }
    if (isParam(r)) {
        return pickRegisterFromParamAliasSet(r);
    }
    if (isReturnValue(r)) {
        return pickRegisterFromReturnValueAliasSet(r);
    }
    ASSERT0(m_ra.isPreAssignedReg(r));
}


void RegSetImpl::pickRegisterFromParamAliasSet(Reg r)
{
    pickReg(m_avail_param_scalar, r);
    pickReg(m_avail_param_vector, r);
}


void RegSetImpl::pickRegisterFromReturnValueAliasSet(Reg r)
{
    pickReg(m_avail_return_value_scalar, r);
    pickReg(m_avail_return_value_vector, r);
}


void RegSetImpl::pickRegisterFromCallerAliasSet(Reg r)
{
    pickReg(m_avail_caller_scalar, r);
    pickReg(m_avail_caller_vector, r);
}


void RegSetImpl::pickRegisterFromCalleeAliasSet(Reg r)
{
    pickReg(m_avail_callee_scalar, r);
    pickReg(m_avail_callee_vector, r);
}


void RegSetImpl::freeRegisterFromAliasSet(Reg r)
{
    ASSERT0(isAvailAllocable(r));
    if (isCallee(r)) {
        return freeRegisterFromCalleeAliasSet(r);
    }
    if (isCaller(r)) {
        return freeRegisterFromCallerAliasSet(r);
    }
    if (isParam(r)) {
        return freeRegisterFromParamAliasSet(r);
    }
    if (isReturnValue(r)) {
        return freeRegisterFromReturnValueAliasSet(r);
    }
    ASSERT0(m_ra.isPreAssignedReg(r));
}


void RegSetImpl::freeRegisterFromCallerAliasSet(Reg r)
{
    if (m_target_caller_scalar && m_target_caller_scalar->is_contain(r)) {
        m_avail_caller_scalar.bunion((BSIdx)r);
    }
    if (m_target_caller_vector && m_target_caller_vector->is_contain(r)) {
        m_avail_caller_vector.bunion((BSIdx)r);
    }
}


void RegSetImpl::freeRegisterFromCalleeAliasSet(Reg r)
{
    if (m_target_callee_scalar && m_target_callee_scalar->is_contain(r)) {
        m_avail_callee_scalar.bunion((BSIdx)r);
    }
    if (m_target_callee_vector && m_target_callee_vector->is_contain(r)) {
        m_avail_callee_vector.bunion((BSIdx)r);
    }
}


void RegSetImpl::freeRegisterFromParamAliasSet(Reg r)
{
    if (m_target_param_scalar && m_target_param_scalar->is_contain(r)) {
        m_avail_param_scalar.bunion((BSIdx)r);
    }
    if (m_target_param_vector && m_target_param_vector->is_contain(r)) {
        m_avail_param_vector.bunion((BSIdx)r);
    }
}


void RegSetImpl::freeRegisterFromReturnValueAliasSet(Reg r)
{
    if (m_target_return_value_scalar &&
        m_target_return_value_scalar->is_contain(r)) {
        m_avail_return_value_scalar.bunion((BSIdx)r);
    }
    if (m_target_return_value_vector&&
        m_target_return_value_vector->is_contain(r)) {
        m_avail_return_value_vector.bunion((BSIdx)r);
    }
}


void RegSetImpl::freeReg(Reg reg)
{
    return freeRegisterFromAliasSet(reg);
}


void RegSetImpl::freeReg(LifeTime const* lt)
{
    Reg reg = m_ra.getReg(lt->getPrno());
    ASSERT0(reg != REG_UNDEF);
    freeReg(reg);
}


bool RegSetImpl::isCalleeScalar(Reg r) const
{
    //True if input register is callee saved scalar register. Note that the
    //frame pointer register can be used as callee saved register if this
    //register can be allocated and the dynamic stack function has not used it.
    //Note that special handling can be done based on the architecture.
    bool is_callee_scalar = m_target_callee_scalar != nullptr &&
        m_target_callee_scalar->is_contain(r);
    //GCOVR_EXCL_START
    bool use_fp_as_callee_scalar = isFP(r) && m_ra.isFPAllocable();
    //GCOVR_EXCL_STOP
    return is_callee_scalar || use_fp_as_callee_scalar;
}


void RegSetImpl::collectOtherAvailableRegister()
{
    //Collect FP register.
    //If the frame pointer register can be allocated and the dynamic stack
    //function and prologue&epilogue inserter function have not used this
    //register during the current compilation process, it can be used to
    //participate in scalar callee saved register allocation.
    //In addition, if debug mode is turned on, the frame pointer register
    //has a special role and cannot participate in allocation. Also, it can
    //not be used if debugging linear scan register allocation.
    if (m_ra.isFPAllocable()) {
        Reg reg = getTIMgr().getFP();
        m_avail_callee_scalar.bunion(reg);
        m_avail_allocable.bunion(reg);
    }
}


void RegSetImpl::dump() const
{
    note(m_ra.getRegion(), "\n==-- DUMP RegSetImpl RegisterSet --==");
    m_ra.getRegion()->getLogMgr()->incIndent(2);
    dumpAvailRegSet();
    dumpUsedRegSet();
    m_ra.getRegion()->getLogMgr()->decIndent(2);
}


void RegSetImpl::dumpUsedRegSet() const
{
    note(m_ra.getRegion(), "\n==-- DUMP UsedRegisterSet --==");
    StrBuf buf(32);
    m_used_callee.dump(buf);
    note(m_ra.getRegion(), "\nUSED_CALLEE:%s", buf.getBuf());

    buf.clean();
    m_used_caller.dump(buf);
    note(m_ra.getRegion(), "\nUSED_CALLER:%s", buf.getBuf());
}


void RegSetImpl::dumpAvailRegSet() const
{
    note(m_ra.getRegion(), "\n==-- DUMP AvaiableRegisterSet --==");
    StrBuf buf(32);
    m_avail_allocable.dump(buf);
    note(m_ra.getRegion(), "\nAVAIL_ALLOCABLE:%s", buf.getBuf());

    buf.clean();
    m_avail_caller_scalar.dump(buf);
    note(m_ra.getRegion(), "\nAVAIL_CALLER_SCALAR:%s", buf.getBuf());

    buf.clean();
    m_avail_callee_scalar.dump(buf);
    note(m_ra.getRegion(), "\nAVAIL_CALLEE_SCALAR:%s", buf.getBuf());

    buf.clean();
    m_avail_param_scalar.dump(buf);
    note(m_ra.getRegion(), "\nAVAIL_PARAM_SCALAR:%s", buf.getBuf());

    buf.clean();
    m_avail_return_value_scalar.dump(buf);
    note(m_ra.getRegion(), "\nAVAIL_RETURN_VALUE:%s", buf.getBuf());

    buf.clean();
    m_avail_caller_vector.dump(buf);
    note(m_ra.getRegion(), "\nAVAIL_CALLER_VECTOR:%s", buf.getBuf());

    buf.clean();
    m_avail_callee_vector.dump(buf);
    note(m_ra.getRegion(), "\nAVAIL_CALLEE_VECTOR:%s", buf.getBuf());

    buf.clean();
    m_avail_param_vector.dump(buf);
    note(m_ra.getRegion(), "\nAVAIL_PARAM_VECTOR:%s", buf.getBuf());

    buf.clean();
    m_avail_return_value_vector.dump(buf);
    note(m_ra.getRegion(), "\nAVAIL_RETURN_VALUE_VECTOR:%s", buf.getBuf());
}
//END RegSetImpl


//
//START RoundRobinRegSetImpl
//
//Pick up a physical register from allocable register set by the roundrobin
//way.
Reg RoundRobinRegSetImpl::pickRegRoundRobin(RegSet & set)
{
    BSIdx i = BS_UNDEF;
    if (m_bsidx_marker == BS_UNDEF) {
        i = m_bsidx_marker = set.get_first();
    } else if (m_bsidx_marker != BS_UNDEF) {
        i = set.get_next(m_bsidx_marker);
        if (i == BS_UNDEF || i > set.get_last()) { i = set.get_first(); }
        m_bsidx_marker = i;
    }

    if (i == BS_UNDEF) { return REG_UNDEF; }
    set.diff(i);
    return (Reg)i;
}


Reg RoundRobinRegSetImpl::pickReg(RegSet & set)
{
    return pickRegRoundRobin(set);
}
//END RoundRobinRegSetImpl


//
//START LRURegSetImpl
//
void LRURegSetImpl::freeRegisterFromCallerAliasSet(Reg r)
{
    bool handled = false;
    if (m_target_caller_scalar && m_target_caller_scalar->is_contain(r)) {
        m_avail_caller_scalar.bunion((BSIdx)r);
        m_free_caller_scalar_queue.enqueue(r);
        handled = true;
    }
    if (m_target_caller_vector && m_target_caller_vector->is_contain(r)) {
        m_avail_caller_vector.bunion((BSIdx)r);
        m_free_caller_vector_queue.enqueue(r);
        handled = true;
    }
    ASSERTN(handled, ("Target Dependent Code"));
}


void LRURegSetImpl::freeRegisterFromCalleeAliasSet(Reg r)
{
    bool handled = false;
    if (m_target_callee_scalar && m_target_callee_scalar->is_contain(r)) {
        m_avail_callee_scalar.bunion((BSIdx)r);
        m_free_callee_scalar_queue.enqueue(r);
        handled = true;
    }
    if (m_target_callee_vector && m_target_callee_vector->is_contain(r)) {
        m_avail_callee_vector.bunion((BSIdx)r);
        m_free_callee_vector_queue.enqueue(r);
        handled = true;
    }

    //In debug mode, callee register may be empty.
    if (!g_do_lsra_debug) { ASSERTN(handled, ("Target Dependent Code")); }
}


void LRURegSetImpl::pickReg(RegSet & set, Reg r)
{
    ASSERT0(r != REG_UNDEF);
    if (&set == &m_avail_caller_scalar) {
        if (set.is_contain(r)) {
            set.diff(r);
            m_free_caller_scalar_queue.remove(r);
        }
        return;
    }
    if (&set == &m_avail_caller_vector) {
        if (set.is_contain(r)) {
            set.diff(r);
            m_free_caller_vector_queue.remove(r);
        }
        return;
    }
    if (&set == &m_avail_callee_scalar) {
        if (set.is_contain(r)) {
            set.diff(r);
            m_free_callee_scalar_queue.remove(r);
        }
        return;
    }
    if (&set == &m_avail_callee_vector) {
        if (set.is_contain(r)) {
            set.diff(r);
            m_free_callee_vector_queue.remove(r);
        }
        return;
    }
    ASSERTN(0, ("Target Dependent Code"));
}


void LRURegSetImpl::initFreeQueue()
{
    bool handled = false;
    for (BSIdx i = m_avail_caller_scalar.get_first(); i != BS_UNDEF;
         i = m_avail_caller_scalar.get_next((UINT)i)) {
        m_free_caller_scalar_queue.enqueue((Reg)i);
        handled = true;
    }
    for (BSIdx i = m_avail_caller_vector.get_first(); i != BS_UNDEF;
         i = m_avail_caller_vector.get_next((UINT)i)) {
        m_free_caller_vector_queue.enqueue((Reg)i);
        handled = true;
    }
    for (BSIdx i = m_avail_callee_scalar.get_first(); i != BS_UNDEF;
         i = m_avail_callee_scalar.get_next((UINT)i)) {
        m_free_callee_scalar_queue.enqueue((Reg)i);
        handled = true;
    }
    for (BSIdx i = m_avail_callee_vector.get_first(); i != BS_UNDEF;
         i = m_avail_callee_vector.get_next((UINT)i)) {
        m_free_callee_vector_queue.enqueue((Reg)i);
        handled = true;
    }
    ASSERTN(handled, ("Target Dependent Code"));
}


void LRURegSetImpl::initRegSet()
{
    RegSetImpl::initRegSet();
    initFreeQueue();
}


Reg LRURegSetImpl::pickRegLRU(RegSet & set)
{
    Reg r = REG_UNDEF;
    if (&set == &m_avail_caller_scalar) {
        if (!m_free_caller_scalar_queue.isEmpty()) {
            r = (Reg)m_free_caller_scalar_queue.dequeue();
        }
    } else if (&set == &m_avail_caller_vector) {
        if (!m_free_caller_vector_queue.isEmpty()) {
            r = (Reg)m_free_caller_vector_queue.dequeue();
        }
    } else if (&set == &m_avail_callee_scalar) {
        if (!m_free_callee_scalar_queue.isEmpty()) {
            r = (Reg)m_free_callee_scalar_queue.dequeue();
        }
    } else if (&set == &m_avail_callee_vector) {
        if (!m_free_callee_vector_queue.isEmpty()) {
            r = (Reg)m_free_callee_vector_queue.dequeue();
        }
    } else {
        ASSERTN(0, ("Target Dependent Code"));
    }
    if (r != REG_UNDEF) { set.diff(r); }
    return r;
}
//END LRURegSetImpl


//START LTConstraintsMgr
void LTConstraintsMgr::init()
{
    m_ltc_list.init();
}


LTConstraints * LTConstraintsMgr::allocLTConstraints()
{
    LTConstraints * lt_constraints = new LTConstraints();
    ASSERT0(lt_constraints);
    m_ltc_list.append_tail(lt_constraints);
    return lt_constraints;
}


void LTConstraintsMgr::destroy()
{
    for (LTConstraints * ltc = m_ltc_list.get_head(); ltc!= nullptr;
         ltc = m_ltc_list.get_next()) {
        delete ltc;
    }
    m_ltc_list.destroy();
}
//END LTConstraintsMgr


//
//START LTConstraints
//
void LTConstraints::updateConflictPR(PRNO renamed_pr, PRNO old_pr)
{
    ASSERT0(m_conflicting_prs.find(old_pr));
    m_conflicting_prs.remove(old_pr);
    m_conflicting_prs.append(renamed_pr);
}
//END LTConstraints


//
//START FakeIRMgr
//
void FakeIRMgr::removeFakeUseIR()
{
    //Remove the fake-use IR with no code gen attribute after register
    //assignment.
    PosAttrNoCodeGenProc no_code_gen_proc(m_rg, m_pos2attr, m_lsra);
    no_code_gen_proc.process();
}
//END FakeIRMgr


//
//START LTInterfGraph
//
void LTInterfGraphLSRAChecker::build()
{
    erase();
    LTListIter it1;
    LTList const& ltlst = m_lt_mgr.getLTList();
    for (LifeTime * lt1 = ltlst.get_head(&it1);
         lt1 != nullptr; lt1 = ltlst.get_next(&it1)) {
        addVertex(lt1->getPrno());
        LTListIter it2;
        for (LifeTime * lt2 = ltlst.get_head(&it2);
             lt2 != nullptr; lt2 = ltlst.get_next(&it2)) {
            if (lt1->getPrno() == lt2->getPrno()) { continue; }
            if (lt1->is_intersect(lt2)) {
                addEdge(lt1->getPrno(), lt2->getPrno());
                continue;
            }
            addVertex(lt2->getPrno());
        }
    }
}


bool LTInterfGraphLSRAChecker::canSkipCheck(
    LinearScanRA const* lsra, xcom::Edge const* e) const
{
    ASSERT0(lsra && e);
    if (lsra->canInterfereWithOtherLT(e->from()->id()) ||
        lsra->canInterfereWithOtherLT(e->to()->id())) {
        //If any prno of the 'e' is expected to interfere with others, so
        //this edge can be skipped.
        return true;
    }
    return false;
}


bool LTInterfGraphLSRAChecker::check(LinearScanRA * lsra)
{
    //Build the interference graph first.
    build();

    //Check the LSRA result by the following steps on the interference graph:
    // 1. Traverse each edge of the interference graph.
    // 2. If the regsiters assigned to the src node and dst node is the ZERO
    //    register, the edge can be skip.
    // 3. If the prno responding to the src or dst node is not participated
    //    into the LSRA (e.g: callee saved registers), or the lifetime has
    //    no def occurence, the edge can be skip.
    xcom::EdgeIter it;
    for (xcom::Edge * e = get_first_edge(it); e != nullptr;
         e = get_next_edge(it)) {
        ASSERT0(e->from()->id() != PRNO_UNDEF);
        ASSERT0(e->to()->id() != PRNO_UNDEF);
        ASSERT0(e->from() != e->to());
        if (canSkipCheck(lsra, e)) { continue; }
        ASSERT0(lsra->getReg(e->from()->id()) != lsra->getReg(e->to()->id()));
    }
    return true;
}
//END LTInterfGraph


//
//START LinearScanRA
//
LinearScanRA::LinearScanRA(Region * rg) : Pass(rg), m_act_mgr(rg)
{
    ASSERT0(rg != nullptr);
    m_cfg = nullptr;
    m_bb_list = nullptr;
    m_irmgr = rg->getIRMgr();
    m_func_level_var_count = 0;
    m_is_apply_to_region = false;
    m_is_fp_allocable_allowed = true;
    m_lt_mgr = nullptr;

    //Since some passes, such as ArgPass, might invoke the
    //APIs of LifeTimeMgr before LSRA's perform(), we initialize the
    //LifeTimeMgr ahead of time here.
    initLTMgr();

    ////////////////////////////////////////////////////////////////////////////
    // DO NOT ALLOCATE CLASS MEMBER HERE. INITIALIZE THEM AFTER ApplyToRegion.//
    ////////////////////////////////////////////////////////////////////////////
    m_lt_constraints_strategy = nullptr;
    m_lt_constraints_mgr = nullptr;
    m_fake_irmgr = nullptr;
    ////////////////////////////////////////////////////////////////////////////
    // DO NOT ALLOCATE CLASS MEMBER HERE. INITIALIZE THEM AFTER ApplyToRegion.//
    ////////////////////////////////////////////////////////////////////////////
}


LinearScanRA::~LinearScanRA()
{
    destroy();
}


bool LinearScanRA::isCalleePermitted(LifeTime const* lt) const
{
    //The first priority allocable register set is callee-saved. Callee
    //is the best choose if lt crossed function-call as well.
    //The second priority allocable register set is caller-saved.
    //Note if User has used caller-saved register before, it should be spilled
    //before enter a function-call, and reload the reigster if it is used again
    //after the function.
    return true;
}


void LinearScanRA::initLocalUsage()
{
    initFakeIRMgr();
}


void LinearScanRA::destroy()
{
    destroyLocalUsage();
    if (m_lt_mgr != nullptr) {
        delete m_lt_mgr;
        m_lt_mgr = nullptr;
    }
    if (m_reg_lt_mgr != nullptr) {
        delete m_reg_lt_mgr;
        m_reg_lt_mgr = nullptr;
    }
}


void LinearScanRA::destroyLocalUsage()
{
    //NOTE:Since other passes, such as ProEpiInserter, might invoke the
    //APIs of LifeTimeMgr, we retain the LifeTimeMgr untill the
    //pass object destroy.
    //if (m_lt_mgr != nullptr) {
    //    delete m_lt_mgr;
    //    m_lt_mgr = nullptr;
    //}
    if (m_fake_irmgr != nullptr) {
        delete m_fake_irmgr;
        m_fake_irmgr = nullptr;
    }
    if (m_lt_constraints_mgr != nullptr) {
        delete m_lt_constraints_mgr;
        m_lt_constraints_mgr = nullptr;
    }
    if (m_lt_constraints_strategy != nullptr) {
        delete m_lt_constraints_strategy;
        m_lt_constraints_strategy = nullptr;
    }
}


//Reset all resource before allocation.
void LinearScanRA::reset()
{
    getTIMgr().reset();
    m_prno2reg.clean();
    m_unhandled.clean();
    m_handled.clean();
    m_active.clean();
    m_inactive.clean();
    m_spill_tab.clean();
    m_reload_tab.clean();
    m_remat_tab.clean();
    m_move_tab.clean();
    m_prno2var.clean();
    m_act_mgr.clean();
    if (getLTConstraintsMgr() != nullptr) {
        getLTConstraintsMgr()->reset();
    }
    if (m_lt_mgr != nullptr) {
        m_lt_mgr->reset();
    }
    if (m_reg_lt_mgr != nullptr) {
        m_reg_lt_mgr->reset();
    }

    //Detect the alloca and stack realign.
    StackBehaviorDetector stack_behavior_detector(m_rg);
    stack_behavior_detector.detect(m_has_alloca, m_may_need_to_realign_stack);
}


Type const* LinearScanRA::getSpillType(PRNO prno) const
{
    Type const* var_ty = getVarTypeOfPRNO(prno);
    ASSERT0(var_ty);
    if (var_ty->is_any()) {
        //The spill location type should not less than register type at least.
        return m_rg->getTypeMgr()->getTargMachRegisterType();
    }
    return var_ty;
}


Var * LinearScanRA::getSpillLoc(PRNO prno)
{
    prno = getAnctPrno(prno);
    ASSERT0(prno != PRNO_UNDEF);
    return m_prno2var.get(prno);
}


Var * LinearScanRA::genSpillLoc(PRNO prno, Type const* ty)
{
    prno = getAnctPrno(prno);
    ASSERT0(prno != PRNO_UNDEF);
    TypeMgr const* tm = m_rg->getTypeMgr();
    if (ty->is_any()) {
        //We intend to give the same size as PTR type as the placeholder size
        //of ANY type, because ANY type always be represented by Object Pointer
        //in runtime system.
        ty = tm->getTargMachRegisterType();
    }
    //NOTE: ty may be vector type that byte size is greater than register type.
    ASSERT0(!ty->is_scalar() ||
            tm->getByteSize(ty) <=
            tm->getByteSize(tm->getTargMachRegisterType()));
    Var * v = getSpillLoc(prno);
    if (v == nullptr) {
        //The alignment of vector register is greater than STACK_ALIGNMENT.
        v = genFuncLevelVar(ty, MAX(tm->getByteSize(ty), STACK_ALIGNMENT));
        m_prno2var.set(prno, v);
    }
    return v;
}


Var * LinearScanRA::getSpillLoc(Type const* ty)
{
    ASSERT0(ty);
    TypeMgr const* tm = m_rg->getTypeMgr();

    //NOTE: ty may be vector type that byte size is greater than register type.
    ASSERT0(!ty->is_scalar() ||
            tm->getByteSize(ty) <=
            tm->getByteSize(tm->getTargMachRegisterType()));
    bool find = false;
    Var * v = m_ty2var.get(ty, &find);
    if (find) { return v; }
    //The alignment of vector register is greater than STACK_ALIGNMENT.
    v = genFuncLevelVar(ty, MAX(tm->getByteSize(ty), STACK_ALIGNMENT));
    m_ty2var.set(ty, v);
    return v;
}


//Return physical register by given pre-assigned prno.
Reg LinearScanRA::getPreAssignedReg(PRNO prno) const
{
    Reg r = const_cast<LinearScanRA*>(this)->m_preassigned_mgr.get(prno);
    if (r != REG_UNDEF) {
        return r;
    }
    return const_cast<LinearScanRA*>(this)->m_dedicated_mgr.get(prno);
}


PRNO LinearScanRA::buildPrnoDedicated(Type const* type, Reg reg)
{
    ASSERT0(type);
    ASSERT0(reg != REG_UNDEF && isDedicatedReg(reg));
    PRNO prno = getDedicatedPRNO(reg);
    if (prno != PRNO_UNDEF) { return prno; }
    ASSERT0(m_irmgr);
    prno = m_irmgr->buildPrno(type);
    setPreAssignedReg(prno, reg);
    return prno;
}


PRNO LinearScanRA::buildPrnoPreAssigned(Type const* type, Reg reg)
{
    ASSERT0(type);
    ASSERT0(reg != REG_UNDEF);
    ASSERT0(m_irmgr);
    PRNO prno = m_irmgr->buildPrno(type);
    setPreAssignedReg(prno, reg);
    return prno;
}


PRNO LinearScanRA::buildPrnoAndSetReg(Type const* type, Reg reg)
{
    ASSERT0(type);
    ASSERT0(reg != REG_UNDEF);
    PRNO prno = isDedicatedReg(reg) ? buildPrnoDedicated(type, reg) :
        buildPrnoPreAssigned(type, reg);
    setReg(prno, reg);
    return prno;
}


IR * LinearScanRA::buildSpillByLoc(PRNO prno, Var * spill_loc, Type const* ty)
{
    ASSERT0(spill_loc && ty);
    ASSERT0(prno != PRNO_UNDEF);
    IR * pr = m_irmgr->buildPRdedicated(prno, ty);
    m_rg->getMDMgr()->allocRef(pr);
    IR * stmt = m_irmgr->buildStore(spill_loc, pr);
    m_rg->getMDMgr()->allocRef(stmt);
    m_rg->addToVarTab(spill_loc);
    stmt->setAligned(true);
    return stmt;
}


IR * LinearScanRA::buildSpill(PRNO prno, Type const* ty)
{
    ASSERT0(ty);
    ASSERT0(prno != PRNO_UNDEF);
    Var * spill_loc = genSpillLoc(prno, ty);
    ASSERT0(spill_loc);
    return buildSpillByLoc(prno, spill_loc, ty);
}


IR * LinearScanRA::getMoveSrcPr(IR const* mov) const
{
    ASSERT0(mov && mov->is_stpr() && mov->getRHS());
    return mov->getRHS();
}


IR * LinearScanRA::buildMoveRHS(PRNO pr, Type const* ty) const
{
    ASSERT0(pr != PRNO_UNDEF);
    ASSERT0(ty);
    return m_irmgr->buildPRdedicated(pr, ty);
}


IR * LinearScanRA::buildMove(PRNO to, PRNO from, Type const* fromty,
     Type const* toty)
{
    ASSERT0(from != PRNO_UNDEF && to != PRNO_UNDEF);
    ASSERT0(fromty && toty);
    return m_irmgr->buildStorePR(to, toty, buildMoveRHS(from, fromty));
}


IR * LinearScanRA::buildReload(PRNO prno, Var * spill_loc, Type const* ty)
{
    ASSERT0(spill_loc && ty);
    ASSERT0(prno != PRNO_UNDEF);
    IR * ld = m_irmgr->buildLoad(spill_loc, ty);
    m_rg->getMDMgr()->allocRef(ld);
    IR * stmt = m_irmgr->buildStorePR(prno, ty, ld);
    m_rg->getMDMgr()->allocRef(stmt);
    ld->setAligned(true);
    return stmt;
}


IR * LinearScanRA::buildRemat(PRNO prno, RematCtx const& rematctx,
                              Type const* ty)
{
    ASSERT0(rematctx.material_exp && ty);
    ASSERT0(prno != PRNO_UNDEF);
    IR * e = m_rg->dupIRTree(rematctx.material_exp);
    m_rg->getMDMgr()->allocRefForIRTree(e, true);
    IR * stmt = m_irmgr->buildStorePR(prno, ty, e);
    m_rg->getMDMgr()->allocRef(stmt);
    return stmt;
}


void LinearScanRA::setReg(PRNO prno, Reg reg)
{
    ASSERT0(prno != PRNO_UNDEF);
    m_prno2reg.set(prno, reg);
}


bool LinearScanRA::hasReg(LifeTime const* lt) const
{
    return hasReg(lt->getPrno());
}


bool LinearScanRA::hasReg(PRNO prno) const
{
    return getReg(prno) != REG_UNDEF;
}


Type const* LinearScanRA::getVarTypeOfPRNO(PRNO prno) const
{
    Var * var = m_rg->getVarByPRNO(prno);
    ASSERT0(var);
    TypeMgr * tm = m_rg->getTypeMgr();
    Type const* varty = var->getType();
    //[BUG FIX] If the type of var is ANY, size cannot be obtainer.
    if (varty->is_vector() || varty->is_fp() || varty->is_any()) {
        return varty;
    }
    Type const* tm_word_ty = tm->getTargMachRegisterType();
    UINT var_sz = var->getByteSize(tm);
    UINT tm_word_sz = tm->getByteSize(tm_word_ty);
    ASSERT0(varty->isInt());
    if (var_sz <= tm_word_sz) { return tm_word_ty; }
    return varty; //varty might be ANY.
}


CHAR const* LinearScanRA::getRegFileName(REGFILE rf) const
{
    return const_cast<LinearScanRA*>(this)->getTIMgr().getRegFileName(rf);
}


CHAR const* LinearScanRA::getRegName(Reg r) const
{
    return const_cast<LinearScanRA*>(this)->getTIMgr().getRegName(r);
}


Reg LinearScanRA::getReg(PRNO prno) const
{
    ASSERT0(prno != PRNO_UNDEF);
    return m_prno2reg.get(prno);
}


Reg LinearScanRA::getReg(LifeTime const* lt) const
{
    return getReg(lt->getPrno());
}


REGFILE LinearScanRA::getRegFile(Reg r) const
{
    LinearScanRA * pthis = const_cast<LinearScanRA*>(this);
    return pthis->getTIMgr().getRegFile(r);
}


LifeTime * LinearScanRA::getLT(PRNO prno) const
{
    return const_cast<LinearScanRA*>(this)->getLTMgr().getLifeTime(prno);
}


void LinearScanRA::removeOpInPosGapRecord(IR const* ir)
{
    ASSERT0(isOpInPosGap(ir));
    if (isSpillOp(ir)) {
        removeSpillOp(const_cast<IR*>(ir));
        return;
    }
    if (isMoveOp(ir)) {
        removeMoveOp(const_cast<IR*>(ir));
        return;
    }
    if (isReloadOp(ir)) {
        removeReloadOp(const_cast<IR*>(ir));
        return;
    }
    if (isRematOp(ir)) {
        removeRematOp(const_cast<IR*>(ir));
        return;
    }
}


//The function check the uniquenuess of four LT list that used in RA.
bool LinearScanRA::verify4List() const
{
    xcom::TTab<Reg> used;
    xcom::TTab<LifeTime const*> visit;
    LTListIter it;
    for (LifeTime const* lt = m_unhandled.get_head(&it);
         lt != nullptr; lt = m_unhandled.get_next(&it)) {
        ASSERT0(!visit.find(lt));
        visit.append(lt);
    }
    for (LifeTime const* lt = m_active.get_head(&it);
         lt != nullptr; lt = m_active.get_next(&it)) {
        ASSERT0(!visit.find(lt));
        visit.append(lt);
        Reg r = getReg(lt);
        if (r == REG_UNDEF) { continue; }
        ASSERTN(!used.find(r), ("lt overlapped that has same register"));
        used.append(r);
    }
    for (LifeTime const* lt = m_inactive.get_head(&it);
         lt != nullptr; lt = m_inactive.get_next(&it)) {
        ASSERT0(!visit.find(lt));
        visit.append(lt);
        Reg r = getReg(lt);
        if (r == REG_UNDEF) { continue; }
        ASSERTN(!used.find(r), ("lt overlapped that has same register"));
        used.append(r);
    }
    for (LifeTime const* lt = m_handled.get_head(&it);
         lt != nullptr; lt = m_handled.get_next(&it)) {
        ASSERT0(!visit.find(lt));
        visit.append(lt);
    }
    return true;
}


//The function check whether 'lt' value is simple enough to rematerialize.
//And return the information through rematctx.
bool LinearScanRA::checkLTCanBeRematerialized(
    MOD LifeTime * lt, OUT RematCtx & rematctx)
{
    //Target Dependent Code.
    ASSERT0(lt);
    if (!lt->canBeRemat()) { return false; }
    ASSERT0(lt->getRematExp());
    rematctx.material_exp = lt->getRematExp();
    lt->setRematerialized();
    return true;
}


bool LinearScanRA::verifyAfterRA() const
{
    ASSERT0(m_unhandled.get_elem_count() == 0);
    ASSERT0(m_active.get_elem_count() == 0);
    ASSERT0(m_inactive.get_elem_count() == 0);
    BBListIter bbit;
    TypeMgr * tm = m_rg->getTypeMgr();
    for (IRBB const* bb = m_bb_list->get_head(&bbit);
         bb != nullptr; bb = m_bb_list->get_next(&bbit)) {
        BBIRListIter bbirit;
        BBIRList const& irlst = const_cast<IRBB*>(bb)->getIRList();
        for (IR * ir = irlst.get_head(&bbirit); ir != nullptr;
            ir = irlst.get_next(&bbirit)) {
            if (!isSpillOp(ir) && !isReloadOp(ir)) { continue; }

            //Check the reload and spill only.
            ASSERT0(ir->getRHS());
            Type const* lhs_ty = ir->getType();
            Type const* rhs_ty = ir->getRHS()->getType();
            ASSERT0(lhs_ty && rhs_ty);
            UINT lhs_size = tm->getDTypeByteSize(lhs_ty->getDType());
            UINT rhs_size = tm->getDTypeByteSize(rhs_ty->getDType());
            ASSERT0(lhs_size >= rhs_size);
            if (lhs_size < rhs_size) { return false; }
        }
    }
    return true;
}


void LinearScanRA::dump4List() const
{
    note(m_rg, "\n==-- DUMP 4LIST --==");
    note(m_rg, "\nUNHANDLED:");
    UINT ind = 1;
    m_rg->getLogMgr()->incIndent(ind);
    LTSetIter it;
    LinearScanRA * pthis = const_cast<LinearScanRA*>(this);
    for (pthis->getUnhandled().get_head(&it); it != nullptr;
         pthis->getUnhandled().get_next(&it)) {
        LifeTime * lt = it->val();
        dumpPR2Reg(lt->getPrno());
        lt->dump(m_rg);
    }
    m_rg->getLogMgr()->decIndent(ind);

    note(m_rg, "\nHANDLED:");
    m_rg->getLogMgr()->incIndent(ind);
    for (pthis->getHandled().get_head(&it); it != nullptr;
         pthis->getHandled().get_next(&it)) {
        LifeTime * lt = it->val();
        dumpPR2Reg(lt->getPrno());
        lt->dump(m_rg);
    }
    m_rg->getLogMgr()->decIndent(ind);

    note(m_rg, "\nACTIVE:");
    m_rg->getLogMgr()->incIndent(ind);
    for (pthis->getActive().get_head(&it); it != nullptr;
         pthis->getActive().get_next(&it)) {
        LifeTime * lt = it->val();
        dumpPR2Reg(lt->getPrno());
        lt->dump(m_rg);
    }
    m_rg->getLogMgr()->decIndent(ind);

    note(m_rg, "\nINACTIVE:");
    m_rg->getLogMgr()->incIndent(ind);
    for (pthis->getInActive().get_head(&it); it != nullptr;
         pthis->getInActive().get_next(&it)) {
        LifeTime * lt = it->val();
        dumpPR2Reg(lt->getPrno());
        lt->dump(m_rg);
    }
    m_rg->getLogMgr()->decIndent(ind);
}


void LinearScanRA::dumpDOTWithReg() const
{
    dumpDOTWithReg((CHAR const*)nullptr, IRCFG::DUMP_COMBINE);
}


void LinearScanRA::dumpDOTWithReg(CHAR const* name, UINT flag) const
{
    class DumpPRWithReg : public IRDumpCustomBaseFunc {
    public:
        LinearScanRA const* lsra;
    public:
        virtual void dumpCustomAttr(
            OUT xcom::DefFixedStrBuf & buf, Region const* rg, IR const* ir,
            DumpFlag dumpflag) const override
        {
            if (!ir->isPROp()) { return; }
            Reg r = lsra->getReg(ir->getPrno());
            if (r == REG_UNDEF) { return; }
            buf.strcat(" (%s)", lsra->getRegName(r));
        }
    };
    DumpFlag f = DumpFlag::combineIRID(IR_DUMP_KID | IR_DUMP_SRC_LINE);
    DumpPRWithReg cf;
    cf.lsra = this;
    IRDumpCtx<> ctx(4, f, nullptr, &cf);
    ASSERT0(m_cfg && m_cfg->is_valid());
    m_cfg->dumpDOT(name, flag, &ctx);
}


void LinearScanRA::dumpBBListWithReg() const
{
    class DumpPRWithReg : public IRDumpCustomBaseFunc {
    public:
        LinearScanRA const* lsra;
    public:
        virtual void dumpCustomAttr(
            OUT xcom::DefFixedStrBuf & buf, Region const* rg, IR const* ir,
            DumpFlag dumpflag) const override
        {
            if (!ir->isPROp()) { return; }
            Reg r = lsra->getReg(ir->getPrno());
            if (r == REG_UNDEF) { return; }
            buf.strcat(" (%s)", lsra->getRegName(r));
        }
    };
    DumpFlag f = DumpFlag::combineIRID(IR_DUMP_KID | IR_DUMP_SRC_LINE);
    DumpPRWithReg cf;
    cf.lsra = this;
    IRDumpCtx<> irdumpctx(4, f, nullptr, &cf);
    ASSERT0(m_rg->getBBList());
    BBDumpCtxMgr<> ctx(&irdumpctx, nullptr);
    xoc::dumpBBList(m_rg->getBBList(), m_rg, false, &ctx);
}


void LinearScanRA::dumpPR2Reg(PRNO p) const
{
    Reg r = getReg(p);
    LinearScanRA * pthis = const_cast<LinearScanRA*>(this);
    REGFILE rf = pthis->getTIMgr().getRegFile(r);
    LifeTime const* lt = getLT(p);
    if (lt != nullptr) {
        ASSERT0(lt->getPrno() == p);
        note(m_rg, "\nLT:$%u:%s(%s)", lt->getPrno(), getRegName(r),
             pthis->getTIMgr().getRegFileName(rf));
        return;
    }
    //PRNO without allocated a lifetime. The prno may be appeared as a
    //temporate pseduo register, e.g:the PR that indicates the region
    //livein callee-saved physical register.
    note(m_rg, "\n--:$%u:%s(%s)", p, getRegName(r),
         pthis->getTIMgr().getRegFileName(rf));
}


void LinearScanRA::dumpPR2Reg() const
{
    note(m_rg, "\n==-- DUMP PR2Reg --==");
    m_rg->getLogMgr()->incIndent(2);
    for (PRNO p = PRNO_UNDEF + 1; p < m_prno2reg.get_elem_count(); p++) {
        dumpPR2Reg(p);
    }
    m_rg->getLogMgr()->decIndent(2);
}


void LinearScanRA::dumpPosGapIRStatistics() const
{
    UINT vec_spill_cnt = 0;
    UINT scalar_spill_cnt = 0;
    UINT vec_reload_cnt = 0;
    UINT scalar_reload_cnt = 0;
    IRTabIter it;
    for (IR * ir = m_spill_tab.get_first(it);
         ir != nullptr; ir = m_spill_tab.get_next(it)) {
        if (ir->getType()->is_vector()) {
            vec_spill_cnt++;
            continue;
        }
        scalar_spill_cnt++;
    }
    for (IR * ir = m_reload_tab.get_first(it);
         ir != nullptr; ir = m_reload_tab.get_next(it)) {
        if (ir->getType()->is_vector()) {
            vec_reload_cnt++;
            continue;
        }
        scalar_reload_cnt++;
    }
    note(m_rg, "\n==-- DUMP RA STATISTICS --==");
    note(m_rg, "\nSpill: %u[s:%u,v:%u], reload:%u[s:%u,:%u], mov:%u, remat:%u",
        m_spill_tab.get_elem_count(), scalar_spill_cnt, vec_spill_cnt,
        m_reload_tab.get_elem_count(), scalar_reload_cnt, vec_reload_cnt,
        m_move_tab.get_elem_count(), m_remat_tab.get_elem_count());
}


bool LinearScanRA::dump(bool dumpir) const
{
    if (!getRegion()->isLogMgrInit()) { return false; }
    START_TIMER_FMT(t, ("DUMP %s", getPassName()));
    note(getRegion(), "\n==---- DUMP %s '%s' ----==",
         getPassName(), m_rg->getRegionName());
    //m_rg->getLogMgr()->incIndent(2);
    //---------
    LinearScanRA * pthis = const_cast<LinearScanRA*>(this);
    pthis->getTIMgr().dump(m_rg);
    m_preassigned_mgr.dump(m_rg, pthis->getTIMgr());
    UpdatePos up(this);
    pthis->getLTMgr().dumpAllLT(up, m_bb_list, dumpir);
    dumpPR2Reg();
    dumpBBListWithReg();
    dump4List();
    m_act_mgr.dump();
    dumpPosGapIRStatistics();
    //---------
    Pass::dump();
    //m_rg->getLogMgr()->decIndent(2);
    END_TIMER_FMT(t, ("DUMP %s", getPassName()));
    return true;
}


void LinearScanRA::addUnhandled(LifeTime * lt)
{
    if (m_unhandled.find(lt)) { return; }
    ASSERT0(!hasReg(lt));
    m_unhandled.append_tail(lt);
}


void LinearScanRA::addActive(LifeTime * lt)
{
    if (m_active.find(lt)) { return; }
    m_active.append_tail(lt);
}


void LinearScanRA::addInActive(LifeTime * lt)
{
    if (m_inactive.find(lt)) { return; }
    m_inactive.append_tail(lt);
}


void LinearScanRA::addHandled(LifeTime * lt)
{
    if (m_handled.find(lt)) { return; }
    ASSERT0(hasReg(lt));
    m_handled.append_tail(lt);
}


Var * LinearScanRA::genFuncLevelVar(Type const* type, UINT align)
{
    xcom::StrBuf name(64);
    //Spill location should be stack space.
    Var * v = m_rg->getVarMgr()->registerVar(
        genFuncLevelNewVarName(name), type, align, VAR_LOCAL, SS_STACK);
    return v;
}


CHAR const* LinearScanRA::genFuncLevelNewVarName(OUT xcom::StrBuf & name)
{
    name.sprint("func_level_var_%u", ++m_func_level_var_count);
    return name.buf;
}


void LinearScanRA::recalculateSSA(OptCtx & oc) const
{
    bool rmprdu = false;
    bool rmnonprdu = false;
    //TODO:update SSA incrementally.
    MDSSAMgr * mdssamgr = (MDSSAMgr*)m_rg->getPassMgr()->queryPass(
        PASS_MDSSA_MGR);
    if (mdssamgr != nullptr && mdssamgr->is_valid()) {
        mdssamgr->destruction(oc);
        mdssamgr->construction(oc);
        oc.setInvalidNonPRDU();
        rmprdu = true;
    }
    PRSSAMgr * prssamgr = (PRSSAMgr*)m_rg->getPassMgr()->queryPass(
        PASS_PRSSA_MGR);
    if (prssamgr != nullptr && prssamgr->is_valid()) {
        prssamgr->destruction(oc);
        prssamgr->construction(oc);
        oc.setInvalidPRDU();
        rmnonprdu = true;
    }
    xoc::removeClassicDUChain(m_rg, rmprdu, rmnonprdu);
}


PRNO LinearScanRA::getAnctPrno(PRNO prno) const
{
    ASSERT0(prno != PRNO_UNDEF);
    ASSERTN(m_lt_mgr, ("LifeTimeMgr is not initialized"));
    LifeTime const* lt = m_lt_mgr->getLifeTime(prno);
    if (lt == nullptr ) { return prno; }
    return lt->getAnctPrno();
}


bool LinearScanRA::isFPAllocable() const
{
    return !g_force_use_fp_as_sp && !xoc::g_debug && !hasAlloca() &&
        canPreservedFPInAlignStack() && isFPAllocableAllowed();
}


bool LinearScanRA::isPrnoAlias(PRNO prno1, PRNO prno2) const
{
    if (prno1 == prno2) { return true; }

    //Check the alias based on the ancestor of lifetime. Normally, the prno
    //of the ancestor is smaller, so we start from the bigger prno, and then
    //backtrace to its ancestors until the expected prno is finded or the
    //ancestor is not existed.
    PRNO big = MAX(prno1, prno2);
    PRNO small = MIN(prno1, prno2);
    LifeTime const* lt = m_lt_mgr->getLifeTime(big);
    //This assert is commented due to the below if statement.
    //ASSERT0(lt);
    if (lt == nullptr) {
        //If there is no lifetime related to the prno, which means this prno is
        //not participated the register allocation, it is assigned to dediacted
        //register, so it is not alias with any other register.
        return false;
    }
    while (lt->getParent() != nullptr) {
        ASSERT0(lt->getPrno() > lt->getParent()->getPrno());
        lt = lt->getParent();
        if (lt->getPrno() == small) { return true; }
    }
    return false;
}


bool LinearScanRA::performLsraImpl(OptCtx & oc)
{
    //The default linear-scan implementation.
    RegSetImpl * rsimpl = allocRegSetImpl();
    rsimpl->initRegSet();
    LSRAImpl impl(*this, *rsimpl);
    bool changed = impl.perform(oc);
    ASSERT0(verifyAfterRA());
    delete rsimpl;
    return changed;
}


void LinearScanRA::scanIRAndSetConstraints()
{
    if (m_lt_constraints_strategy == nullptr) {
        ASSERT0(m_lt_constraints_mgr == nullptr);
        return;
    }
    BBList * bb_list = this->getBBList();
    ASSERT0(bb_list);
    BBListIter bb_it;
    for (IRBB * bb = bb_list->get_head(&bb_it);
         bb != nullptr; bb = bb_list->get_next(&bb_it)) {
        BBIRList & ir_lst = bb->getIRList();
        BBIRListIter bb_ir_it;
        for (IR * ir = ir_lst.get_head(&bb_ir_it);
             ir != nullptr; ir = ir_lst.get_next(&bb_ir_it)) {
            //Note that different architectures have different strategy
            //implementations. For example, this architecture's addition
            //instruction requires that the source and destination registers
            //have different lifetime allocations,
            //while other architectures may not have this requirement.
            m_lt_constraints_strategy->applyConstraints(ir);
        }
    }
}


void LinearScanRA::tryComputeConstraints()
{
    //Before calculating the lifetime constraints, we need to
    //initialize both the lifetime constraint management unit
    //and the constraints strategy. After initialization, we can
    //proceed to set the constraint collection.
    //Note: initLTConstraintsMgr() and initConstraintsStrategy()
    //have different implementations depending on the architecture.
    //initLTConstraintsMgr() allocates memory for the lifetime
    //constraint specific to the architecture, while the
    //ConstraintsStrategy generates various constraint results
    //according to the architecture-specific strategies.
    //For example, the base class is LTConstraints. ARM may need
    //to create its own constraint collection, which can be done
    //with a class like ARMLTConstraints that inherits from
    //LTConstraints. Similarly, ARM can have its own
    //ARMLTConstraintsMgr, such as class ARMLTConstraintsMgr
    //that inherits from LTConstraintsMgr, with similar strategies
    //for method implementation.
    initLTConstraintsMgr();
    initConstraintsStrategy();
    scanIRAndSetConstraints();
}


void LinearScanRA::checkAndApplyToRegion(MOD ApplyToRegion & apply)
{
    if (isApplyToRegion()) { return; }
    //Stash pop current region information.
    apply.pop();
    m_cfg = m_rg->getCFG();
    m_bb_list = m_rg->getBBList();
    m_irmgr = m_rg->getIRMgr();
}


void LinearScanRA::checkAndPrepareApplyToRegion(OUT ApplyToRegion & apply)
{
    m_cfg = m_rg->getCFG();
    m_bb_list = m_rg->getBBList();
    m_irmgr = m_rg->getIRMgr();
    if (isApplyToRegion()) { return; }
    //Stash push current region information.
    apply.push();
    m_cfg = m_rg->getCFG();
    m_bb_list = m_rg->getBBList();
    m_irmgr = m_rg->getIRMgr();
    ASSERT0(m_cfg->verify());
}


void LinearScanRA::genRematForLT(MOD LifeTime * lt) const
{
    ASSERT0(lt);
    bool lt_has_only_one_def = lt->isOneDefOnly();
    OccList & occ_lst = const_cast<LifeTime*>(lt)->getOccList();
    OccListIter it = nullptr;
    for (Occ occ = occ_lst.get_head(&it); it != occ_lst.end();
         occ = occ_lst.get_next(&it)) {
        ASSERT0(occ.getIR());
        IR * occ_ir = occ.getIR();

        //Process the def occ only.
        if (!occ.is_def()) { continue; }

        //If there is a def IR which is not a remat-like Op, that means this
        //lifetime cannot be rematerialized.
        if (!isRematLikeOp(occ_ir)) {
            lt->setRematExp(nullptr);
            break;
        }
        if (lt->getRematExp() == nullptr) {
            lt->setRematExp(occ_ir->getRHS());
        }

        //If this lifetime has only one def, then exit the loop.
        if (lt_has_only_one_def) { break; }

        //If the two remat-like IRs are not same, clear the remat info stored
        //before.
        if (!lt->getRematExp()->isIREqual(occ_ir->getRHS(), m_rg->getIRMgr())) {
            lt->setRematExp(nullptr);
            break;
        }
    }
}


void LinearScanRA::genRematInfo()
{
    LTList const& ltlst = getLTMgr().getLTList();
    LTListIter it;
    for (LifeTime * lt = ltlst.get_head(&it); lt != nullptr;
         lt = ltlst.get_next(&it)) {
        genRematForLT(lt);
    }
}


void LinearScanRA::dumpRegLTOverview() const
{
    if (!getRegion()->isLogMgrInit()) { return; }

    xoc::note(m_rg, "\n==-- DUMP Reg2LifeTime in Region '%s' --==",
              m_rg->getRegionName());
    LinearScanRA * pthis = const_cast<LinearScanRA*>(this);
    PRNO2LT const& reg2lt = pthis->getRegLTMgr().getReg2LT();
    for (Reg r = 0; r < reg2lt.get_elem_count(); r++) {
        LifeTime * lt = reg2lt.get(r);
        if (lt == nullptr) { continue; }
        lt->dumpReg2LifeTime(m_rg, this, r);
    }
}


void LinearScanRA::dumpReg2LT(Pos start, Pos end, bool open_range) const
{
    PRNO2LT const& reg2lt = m_reg_lt_mgr->getReg2LT();
    for (Reg r = 0; r < reg2lt.get_elem_count(); r++) {
        LifeTime * lt = reg2lt.get(r);
        if (lt == nullptr) { continue; }
        lt->dumpReg2LifeTimeWithPos(m_rg, this, r, start, end, open_range);
    }
}


bool LinearScanRA::verifyLSRAByInterfGraph(OptCtx & oc) const
{
    START_TIMER(t, "verifyLSRAByInterfGraph");
    oc.setInvalidPass(PASS_PRLIVENESS_MGR);
    m_rg->getPassMgr()->checkValidAndRecompute(
        &oc, PASS_RPO, PASS_DOM, PASS_PRLIVENESS_MGR, PASS_UNDEF);

    VarUpdatePos up(this);
    LifeTime2DMgr lt2d_mgr(m_rg);
    lt2d_mgr.computeLifeTime(up, m_bb_list, m_preassigned_mgr);

    LTInterfGraphLSRAChecker graph(m_rg, lt2d_mgr);
    LinearScanRA * pthis = const_cast<LinearScanRA*>(this);
    ASSERT0(graph.check(pthis));
    END_TIMER(t, "verifyLSRAByInterfGraph");
    return true;
}


bool LinearScanRA::canInterfereWithOtherLT(PRNO prno) const
{
    ASSERT0(prno != PRNO_UNDEF);
    LinearScanRA * pthis = const_cast<LinearScanRA*>(this);

    Reg r = getReg(prno);
    ASSERT0(r != REG_UNDEF);
    if (isZeroRegister(r)) {
        //If the regsiters assigned to the src node and dst node is the ZERO
        //register, the lifetime can interfere with other lifetimes assigned
        //to zero register.
        return true;
    }

    LifeTimeMgr const& lt_mgr = pthis->getLTMgr();
    if (lt_mgr.getLifeTime(prno) == nullptr ||
        !lt_mgr.getLifeTime(prno)->isOccHasDef()) {
        //If the lifetime has no def occ, that means the value of the prno
        //is not important, the value can be anything.
        return true;
    }
    return false;
}


//TODO: rematerialization and spill-store-elimination
bool LinearScanRA::perform(OptCtx & oc)
{
    START_TIMER(t, getPassName());
    m_rg->getPassMgr()->checkValidAndRecompute(&oc, PASS_RPO,
        PASS_DOM, PASS_PRLIVENESS_MGR, PASS_LOOP_INFO, PASS_UNDEF);

    reset();

    //Determine whether the PASS apply all modifications of CFG and BB to
    //current region. User may invoke LSRA as performance estimating tools
    //to conduct optimizations, such as RP, GCSE, UNROLLING which may increase
    //register pressure.
    ApplyToRegion apply(m_rg);
    checkAndPrepareApplyToRegion(apply);
    if (m_bb_list == nullptr || m_bb_list->get_elem_count() == 0) {
        set_valid(true);
        return false;
    }
    initLocalUsage();

    //Do the backward-jump analysis based on the CFG and liveness.
    doBackwardJumpAnalysis();

    UpdatePos up(this);
    ASSERT0(m_lt_mgr);
    getLTMgr().computeLifeTime(up, m_bb_list, m_preassigned_mgr);

    //After the lifetime calculation is completed, begin setting constraint
    //sets for each lifetime.
    tryComputeConstraints();

    //The remat info must be generated before the priority of the lifetime is
    //computed.
    genRematInfo();

    LTPriorityMgr priomgr(m_cfg, getTIMgr());
    priomgr.computePriority(getLTMgr());
    bool changed = performLsraImpl(oc);
    if (g_dump_opt.isDumpAfterPass() && g_dump_opt.isDumpLSRA()) {
        dump(false);
    }
    destroyLocalUsage();
    checkAndApplyToRegion(apply);
    ASSERTN(getRegion()->getCFG()->verifyRPO(oc),
            ("make sure original RPO is legal"));
    ASSERTN(getRegion()->getCFG()->verifyDomAndPdom(oc),
            ("make sure original DOM/PDOM is legal"));
    set_valid(true);
    if (!changed || !isApplyToRegion()) {
        ASSERT0(m_rg->getBBMgr()->verify());
        END_TIMER(t, getPassName());
        return false;
    }
    recalculateSSA(oc);
    oc.setInvalidLoopInfo();
    ASSERT0(m_rg->getBBMgr()->verify());
    END_TIMER(t, getPassName());
    return false;
}
//END LinearScanRA

} //namespace xoc
