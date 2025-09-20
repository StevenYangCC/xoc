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
#ifndef _LINEAR_SCAN_H_
#define _LINEAR_SCAN_H_

namespace xoc {

class LifeTime;
class LifeTimeMgr;
class TargInfoMgr;
class ArgPasser;
class ApplyToRegion;

//
//START LexBackwardJump
//
#define BKJUMP_srcbb(b) ((b)->srcbb)
#define BKJUMP_dstbb(b) ((b)->dstbb)
class LexBackwardJump {
public:
    IRBB const* srcbb;
    IRBB const* dstbb;

    LexBackwardJump(IRBB const* src, IRBB const* dst) : srcbb(src), dstbb(dst)
    { }
    void init(IRBB const* src, IRBB const* dst)
    {
        ASSERT0(src && dst);
        srcbb = src;
        dstbb = dst;
    }
};
//END LexBackwardJump

//
//START Occurence
//
//Lightweight structure used in backward jump analysis.
#define OCC_first(r) ((r)->first)
#define OCC_last(r) ((r)->last)
class Occurence {
public:
    Pos first;
    Pos last;

    Occurence() : first(POS_UNDEF), last(POS_UNDEF) {}
};
//END Occurence

//The attribute flag responding to a POS.
typedef enum {
    //Undefined value.
    POS_ATTR_UNDEF = 0x0,

    //Indicate this IR should not generate the final instruction.
    //If this attribute is set, the IR responding to this POS should be deleted
    //after the register assignment.
    POS_ATTR_NO_CODE_GEN = 0x1,
} POS_ATTR_FLAG;

//This enum is used to describe the lexical sequence of BB for the fake-use IRs
//related to the same PR.
typedef enum {
    BB_SEQ_UNDEF = 0,

    //The fake-use IR in this BB is the first in lexical sequence.
    BB_SEQ_FIRST,
} BB_SEQ;

//
//START PosAttr
//
class PosAttr : public UFlag {
    IRBB const* m_bb;
    IR const* m_ir;
    BB_SEQ m_seq;
public:
    PosAttr(UINT v, IRBB const* bb, IR const* ir) :
            UFlag(v), m_bb(bb), m_ir(ir) {}

    void init(UINT v, IRBB const* bb, IR const* ir)
    {
        set(v);
        setBB(bb);
        setIR(ir);
        m_seq = BB_SEQ_UNDEF;
    }

    IRBB const* getBB() const { return m_bb; }
    IR const* getIR() const { return m_ir; }
    BB_SEQ getSequence() const { return m_seq; }
    void setBB(IRBB const* bb) { m_bb = bb; }
    void setIR(IR const* ir) { m_ir = ir; }
    void setSequence(BB_SEQ seq) { m_seq = seq; }
};
//END PosAttr

//The insert mode used for Backward jump analysis. The fake-use IR should
//be added to the BB at the specified location per the INSERT mode.
typedef enum {
    INSERT_MODE_UNDEF = 0,

    //Indicate the fake-use IR should be appended at the head of BB.
    // eg.
    // LABEL: X
    // -- BB start --
    //                      <-- Append fake IR here.
    // add $2:u32
    // sub $3:u32
    // mul $4:u32
    // goto label L
    // -- BB end --
    INSERT_MODE_HEAD = 1,

    //Indicate the fake-use IR should be appended at the end of BB but before
    //the branch IR.
    // eg.
    // LABEL: X
    // -- BB start --
    // add $2:u32
    // sub $3:u32
    // mul $4:u32
    //                      <-- Append fake IR here.
    //  goto label L
    // -- BB end --
    INSERT_MODE_TAIL = 2,

    //Indicate the fake-use IR should be inserted in the middle of BB, usually a
    //marker IR is provided to indicate the relative position inside BB.
    INSERT_MODE_MID = 3,
} INSERT_MODE;

//
//START BBPos
//
#define BBPOS_prno(r) ((r).m_prno)
#define BBPOS_bbid(r) ((r).m_bbid)
#define BBPOS_flag(r) ((r).m_flag)
class BBPos {
public:
    PRNO m_prno;
    UINT m_bbid;
    UINT m_flag;
public:
    BBPos(UINT v) : m_prno(v), m_bbid(0), m_flag(0) {}
    BBPos(PRNO pr, UINT id, UINT flag) : m_prno(pr), m_bbid(id), m_flag(flag)
    {}
};
//END BBPos

//
//START BBPosCmp
//
class BBPosCmp : public CompareKeyBase<BBPos> {
public:
    bool is_less(BBPos t1, BBPos t2) const
    {
        return (BBPOS_prno(t1) < BBPOS_prno(t2)) &&
               (BBPOS_bbid(t1) < BBPOS_bbid(t2)) &&
               (BBPOS_flag(t1) < BBPOS_flag(t2));
    }
    bool is_equ(BBPos t1, BBPos t2) const
    {
        return (BBPOS_prno(t1) == BBPOS_prno(t2)) &&
               (BBPOS_bbid(t1) == BBPOS_bbid(t2)) &&
               (BBPOS_flag(t1) == BBPOS_flag(t2));
    }
};
//END BBPosCmp

#define FAKEUSE_bb(r) ((r)->bb)
#define FAKEUSE_pos(r) ((r)->pos)
class FakeUse {
public:
    IRBB const* bb;
    Pos pos;
};

//
//START BackwardJumpAnalysisResMgr
//
//BackwardJumpAnalysisResMgr responsible to manager the resources that needed
//during the backward jump analysis.
class BackwardJumpAnalysisResMgr {
    COPY_CONSTRUCTOR(BackwardJumpAnalysisResMgr);
    SMemPool * m_pool;
public:
    BackwardJumpAnalysisResMgr();
    ~BackwardJumpAnalysisResMgr();

    //Generate a PosAttr object and append the pointer in the m_pos_attr_list.
    PosAttr * genPosAttr(UINT v, IRBB const* bb, IR const* ir);

    //Generate a Occurence object and append the pointer in the m_occ_list.
    LexBackwardJump * genLexBackwardJump(IRBB const* srcbb, IRBB const* dstbb);

    //Generate a Occurence object and append the pointer in the m_occ_list.
    Occurence * genOccurence()
    { return (Occurence*)xmalloc(sizeof(Occurence)); }

    FakeUse * genFakeUse()
    { return (FakeUse*)xmalloc(sizeof(FakeUse)); }

    void * xmalloc(size_t size);
};
//END BackwardJumpAnalysisResMgr

//
//START FakeVarMgr
//
class FakeVarMgr {
    COPY_CONSTRUCTOR(FakeVarMgr);
    Region * m_rg;
    Var * m_fake_scalar_var;
    Var * m_fake_vec_var;
public:
    FakeVarMgr(Region * rg)
    {
        m_rg = rg;
        m_fake_scalar_var = nullptr;
        m_fake_vec_var = nullptr;
    }
    ~FakeVarMgr() { }

    //This func generates a fake var per the type.
    Var * genFakeVar(Type const* ty);
};
//END FakeVarMgr

//Record the attribute of IR.
typedef xcom::TMap<BBPos, PosAttr const*, BBPosCmp> BBPos2Attr;
typedef xcom::TMapIter<BBPos, PosAttr const*> BBPos2AttrIter;

//Map from prno to the Occurence.
typedef xcom::TMap<PRNO, Occurence const*> Prno2OccMap;
typedef xcom::TMapIter<PRNO, Occurence const*> Prno2OccMapIter;

//Map from prno to the fake-use info.
typedef xcom::TMap<PRNO, FakeUse*> Prno2FakeUse;
typedef xcom::TMapIter<PRNO, FakeUse*> Prno2FakeUseIter;

//Used to store the PRLiveSet related to the backward jump BBs.
typedef List<PRLiveSet const*> PRLiveSetList;

//Used to store the backward jump edges.
typedef List<LexBackwardJump const*> BackwardEdgeList;

//
//START LexBackwardJumpAnalysis
//
//LexBackwardJumpAnalysis is used to check the CFG whether have the backward
//jump, and insert some fake-use IRs for some prno after analysis.
//eg. BB3-->BB2 is a backward jump:
//    BB1
//     |
//     |
//     V
//    BB2 <------
//     |        |
//     |        |
//     V        |
//    BB3 -------
//     |
//     |
//     V
//    BB4
class LexBackwardJumpAnalysis {
    COPY_CONSTRUCTOR(LexBackwardJumpAnalysis);
    friend class OccRecorderVF;
protected:
    Region * m_rg;
    BBPos2Attr * m_pos2attr;
    IRMgr * m_irmgr;
    BBList * m_bb_list;
    LivenessMgr * m_live_mgr;
    BackwardEdgeList m_backward_edges;
    Vector<Pos> m_bb_entry_pos;
    Vector<Pos> m_bb_exit_pos;
    Prno2OccMap m_prno2occ;
    FakeVarMgr * m_fake_var_mgr;
    LinearScanRA * m_lsra;
    Prno2FakeUse m_pr2fakeuse_head;
    Prno2FakeUse m_pr2fakeuse_tail;
    BackwardJumpAnalysisResMgr * m_resource_mgr;
public:
    LexBackwardJumpAnalysis(Region * rg, BBPos2Attr * pos2attr,
                            FakeVarMgr * fake_var_mgr, LinearScanRA * lsra);
    virtual ~LexBackwardJumpAnalysis();

    //The main function controls the analysis flow.
    bool analyze();

    //Release the resources used during the analysis.
    void destroy();

    void dump();

    //Init the resources for the analysis.
    void init();

    //Reset the resources for the analysis.
    void reset();
protected:
    //Add the backward jump to the set m_backward_edges.
    void addBackwardJump(IRBB const* srcbb, IRBB const* dstbb)
    {
        LexBackwardJump const* e = m_resource_mgr->genLexBackwardJump(
            srcbb, dstbb);
        m_backward_edges.append_tail(e);
    }

    //Assign each BB a lexical sequence ID.
    void assignLexSeqIdForBB(OUT Vector<UINT> & bb_seqid);

    //Collect all the backward jumps in the control flow graph.
    void collectBackwardJumps(Vector<UINT> const& bb_seqid);

    //Return true if 'prno' has been pre-assigned a physical-register, and
    //we can neglect the affect of the special 'prno' during
    //FakeUse generation.
    bool canSkipPreAssignedReg(PRNO prno) const;

    void generateFakeUse();

    //Generate the occurence for pr in m_pr_live_set_list. The occurence is a
    //simple line/IR sequence ID.
    void generateOccurence();

    //Generate the occurence for BB.
    //pos: the BB start position.
    void generateOccurenceForBB(IRBB const* bb, MOD Pos & pos);

    //This func records the end/exit position of the input BB.
    Pos getBBEndPos(UINT bbid) const { return m_bb_exit_pos.get(bbid); }

    //This func records the start/entry position of the input BB.
    Pos getBBStartPos(UINT bbid) const { return m_bb_entry_pos.get(bbid); }

    void insertFakeUse();

    //This func inserts the fake-use IR at the head or tail of input BB.
    void insertFakeUse(IRBB const* bb, PRNO prno, INSERT_MODE mode);

    //This func inserts the fake-use IR at the head of input BB.
    void insertFakeUseAtBBEntry(IRBB const* bb, PRNO prno, BBPos const& pos);

    //This func inserts the fake-use IR at the tail of input BB.
    void insertFakeUseAtBBExit(IRBB const* bb, PRNO prno, BBPos const& pos);

    //Record the fake-use information.
    void recordFakeUse(PRNO prno, IRBB const* bb, INSERT_MODE mode);

    //This func records the occurence of the input prno at the specified pos.
    void recordOccurenceForPR(PRNO prno, Pos pos);
};
//END LexBackwardJumpAnalysis


//
//START FakeIRMgr
//
class FakeIRMgr {
    COPY_CONSTRUCTOR(FakeIRMgr);
    Region * m_rg;
    LinearScanRA * m_lsra;
    LexBackwardJumpAnalysis * m_back_jump_ana;
    FakeVarMgr m_fake_var_mgr;
    BBPos2Attr m_pos2attr;
public:
    FakeIRMgr(Region * rg, LinearScanRA * lsra) : m_rg(rg), m_lsra(lsra),
        m_fake_var_mgr(rg)
    {
        m_back_jump_ana = new LexBackwardJumpAnalysis(m_rg,
            &m_pos2attr, &m_fake_var_mgr, lsra);
    }
    ~FakeIRMgr()
    {
        if (m_back_jump_ana) { delete m_back_jump_ana; }
    }

    //Do the lexicographic backward jump analysis.
    void doBackwardJumpAnalysis() { m_back_jump_ana->analyze(); }

    //Removed the fake-use IR inserted by the backward jump analysis.
    void removeFakeUseIR();
};
//END FakeIRMgr


//
//START RematCtx
//
class RematCtx {
public:
    //Record the expression that used in rematerialization.
    IR const* material_exp;
};
//END RematCtx


//
//START PRNOConstraintsTab
//
//This class is specifically used to store some PRs,
//which have certain relationships with each other.
class PRNOConstraintsTab : public xcom::TTab<PRNO> {
public:
    //Checks if the current object (this) and src_tab share two or
    //more common elements.
    //e.g: if `this` contains {1, 2, 3} and `src_tab` contains {2, 3, 4},
    //the function will return true because they share two elements (2 and 3).
    //TODO: Use this function with caution; it has low efficiency and is
    //recommended only for ASSERT checks.
    bool hasTwoOrMoreCommonElements(PRNOConstraintsTab const& src_tab) const;
};
//END PRNOConstraintsTab


typedef List<LifeTime*>::Iter LTSetIter;
typedef xcom::TMap<LifeTime*, xcom::C<LifeTime*>*> LT2Holder;
class LTSet : public xcom::EList<LifeTime*, LT2Holder> {
    COPY_CONSTRUCTOR(LTSet);
public:
    LTSet() {}
};

typedef xcom::TTabIter<LifeTime*> LTTabIter;
class LTTab : public xcom::TTab<LifeTime*> {
};

typedef TMap<PRNO, Var*> PRNO2Var;
typedef TMapIter<PRNO, Var*> PRNO2VarIter;


//
//START RegSetImpl
//
//This class represents the main manipulating interfaces to target dependent
//RegSet information.
class RegSetImpl {
    COPY_CONSTRUCTOR(RegSetImpl);
protected:
    LinearScanRA & m_ra;

    //Target machine defined scalar register.
    RegSet const* m_target_callee_scalar;
    RegSet const* m_target_caller_scalar;
    RegSet const* m_target_param_scalar;
    RegSet const* m_target_return_value_scalar;

    //Target machine defined vector register.
    RegSet const* m_target_callee_vector;
    RegSet const* m_target_caller_vector;
    RegSet const* m_target_param_vector;
    RegSet const* m_target_return_value_vector;

    //Target machine defined allocable register.
    RegSet const* m_target_allocable_scalar;
    RegSet const* m_target_allocable_vector;

    //RegSetImpl customized scalar register.
    RegSet m_avail_callee_scalar;
    RegSet m_avail_caller_scalar;
    RegSet m_avail_param_scalar;
    RegSet m_avail_return_value_scalar;

    //RegSetImpl customized vector register.
    RegSet m_avail_callee_vector;
    RegSet m_avail_caller_vector;
    RegSet m_avail_param_vector;
    RegSet m_avail_return_value_vector;

    //Record used register that indicates the allocation behaviors.
    RegSet m_used_callee;
    RegSet m_used_caller;

    //Allocable register.
    RegSet m_avail_allocable;
protected:
    //Some other modules will occupy additional registers, making these
    //registers unable to participate in normal register allocation. This
    //interface checks these modules and collects available registers.
    //Note that special handling can be done based on the architecture.
    virtual void collectOtherAvailableRegister();

    void destroyDebugRegSet();
    void destroyRegSet();

    void initAvailRegSet();

    //Init the register set for debug purpose.
    //The function is always used by pass writer to manipulate the number and
    //the content of specific register set.
    //E.g: given a new linear-scan algorithm, the pass writer is going to test
    //the ability of the new algo. However, if the target machine has a lot of
    //physical registers, the pass writer expects to use smaller register set,
    //say 2 physical registers, to test the algo's ablity, pass writer can
    //overwrite the allocable-register-set through this inteface to
    //cut down the number of alloable-register-set.
    virtual void initDebugRegSet() { ASSERTN(0, ("Target Depedent Code")); }
public:
    RegSetImpl(LinearScanRA & ra);
    virtual ~RegSetImpl() { destroyRegSet(); }

    void dump() const;
    void dumpAvailRegSet() const;
    void dumpUsedRegSet() const;

    //Free register.
    void freeReg(xgen::Reg reg);

    //Free register.
    void freeReg(LifeTime const* lt);

    //Free register from alias register set.
    virtual void freeRegisterFromAliasSet(Reg r);

    //Free register from all caller alias register set.
    virtual void freeRegisterFromCallerAliasSet(Reg r);

    //Free register from all callee alias register set.
    virtual void freeRegisterFromCalleeAliasSet(Reg r);

    //Free register from all param alias register set.
    virtual void freeRegisterFromParamAliasSet(Reg r);

    //Free register from all return value alias register set.
    virtual void freeRegisterFromReturnValueAliasSet(Reg r);

    //Get allocable regsets.
    RegSet const& getAvailAllocable() const { return m_avail_allocable; }

    //Get the type of callee-save register. This function ensures the
    //correct register type is used when saving callee-saved registers.
    virtual Type const* getCalleeRegisterType(Reg, TypeMgr *) const
    { ASSERTN(0, ("Target Dependent Code")); return nullptr; }

    TargInfoMgr & getTIMgr() const;

    //Get the total register numbers on the target.
    UINT const getTotalRegNum() const { return getTIMgr().getNumOfRegister(); }

    //Handles the case where both conflict and consistency sets are present.
    Reg handleConflictsAndConsistency(
        OUT RegSet & set, PRNOConstraintsTab const& conflict_prs,
        PRNOConstraintsTab const& consist_prs);

    //Handles the case where only conflict set is present.
    Reg handleOnlyConflicts(
        OUT RegSet & set, PRNOConstraintsTab const& conflict_prs);

    //Handles the case where only consistency set is present.
    //TODO:
    //  Currently, the constraints of the consistency set are not
    //  considered during the register splitting process.
    //  Future implementation needs to support this.
    Reg handleOnlyConsistency(
        OUT RegSet & set, PRNOConstraintsTab const& consist_prs);

    //Init all register sets.
    //Subclass can have different sets of registers,
    //so this is a virtual function.
    virtual void initRegSet();

    //True if reg is allocable.
    bool isAvailAllocable(Reg r) const
    { return m_avail_allocable.is_contain(r); }

    //True if input reg is callee register.
    bool isCallee(Reg r) const
    { return isCalleeScalar(r) || isCalleeVector(r); }

    //True if input register is callee saved scalar register. Note that the
    //frame pointer register can be used as callee saved register if this
    //register can be allocated and the dynamic stack function has not used it.
    //Note that special handling can be done based on the architecture.
    virtual bool isCalleeScalar(Reg r) const;

    //True if input reg is callee vector register.
    bool isCalleeVector(Reg r) const
    {
        return m_target_callee_vector != nullptr &&
            m_target_callee_vector->is_contain(r);
    }

    //True if input reg is caller register.
    bool isCaller(Reg r) const
    { return isCallerScalar(r) || isCallerVector(r); }

    //True if reg is a caller scalar register.
    virtual bool isCallerScalar(Reg r) const
    {
        return m_target_caller_scalar != nullptr &&
            m_target_caller_scalar->is_contain(r);
    }

    //True if reg is a caller vector register.
    bool isCallerVector(Reg r) const
    {
        return m_target_caller_vector != nullptr &&
            m_target_caller_vector->is_contain(r);
    }

    //Whether current register "r" is frame pointer register.
    bool isFP(Reg r) const { return r == getTIMgr().getFP(); }

    //True if input reg is param register.
    bool isParam(Reg r) const
    { return isParamScalar(r) || isParamVector(r); }

    //True if input reg is scalar param register.
    virtual bool isParamScalar(Reg r) const
    {
        return m_target_param_scalar != nullptr &&
            m_target_param_scalar->is_contain(r);
    }

    //True if input reg is vector param register.
    bool isParamVector(Reg r) const
    {
        return m_target_param_vector != nullptr &&
            m_target_param_vector->is_contain(r);
    }

    //GCOVR_EXCL_START
    //Return true if Type matches the register type.
    //e.g: given 'r' is a scalar register R15, however 'ty' is a vector type
    //<f32x128>, the function returns false because R15 can not be used as
    //a <f32x128> vector register.
    virtual bool isRegTypeMatch(Type const* ty, Reg r) const
    {
        ASSERT0(ty->is_vector() || ty->is_int() || ty->is_fp() || ty->is_any());
        ASSERTN(0, ("Target Dependent Code"));

        //The following code implements the default behaviours.
        return (ty->is_vector() && isVector(r)) ||
            (!ty->is_vector() && (isCalleeScalar(r) || isCallerScalar(r)));
    }
    //GCOVR_EXCL_STOP

    //True if input reg is return value register.
    bool isReturnValue(Reg r) const
    { return isReturnValueScalar(r) || isReturnValueVector(r); }

    //True if input reg is scalar return value register.
    virtual bool isReturnValueScalar(Reg r) const
    {
        return m_target_return_value_scalar != nullptr &&
            m_target_return_value_scalar->is_contain(r);
    }

    //True if input reg is vector return value register.
    bool isReturnValueVector(Reg r) const
    {
        return m_target_return_value_vector != nullptr &&
            m_target_return_value_vector->is_contain(r);
    }

    //True if reg is a vector register.
    bool isVector(Reg r) const
    { return isCalleeVector(r) || isCallerVector(r); }

    //Get used caller regiser regset.
    RegSet & getUsedCaller() { return m_used_caller; }

    //Get used callee regiser regset.
    RegSet & getUsedCallee() { return m_used_callee; }

    //Pick a callee register, considering lifetime constraints.
    virtual Reg pickCallee(IR const* ir, LTConstraints const* lt_constraints);

    //Pick a caller register, considering lifetime constraints.
    virtual Reg pickCaller(IR const* ir, LTConstraints const* lt_constraints);

    //Pick reg in a given regset and return it.
    virtual Reg pickReg(RegSet & set);

    //Pick reg in a given regset.
    virtual void pickReg(RegSet & set, Reg r);

    //Pick reg by the incramental order in a given regset, usually the
    //arg passer and epilog/prolog pick the reg by this order.
    static Reg pickRegByIncrementalOrder(RegSet & set);

    //Select the registers that meet the conditions
    //based on the current set of lifetime constraints.
    //Currently, the restrictions are divided into two sets; in other words,
    //it picks a register while considering various
    //constraints related to conflicts and consistency.
    //The function's selection process involves three main steps:
    //If both conflict and consistency sets are present,
    //it uses `handleConflictsAndConsistency()`.
    //If only the conflict set is present, it calls `handleOnlyConflicts()`.
    //If only the consistency set is present,
    //it calls `handleOnlyConsistency()`.
    Reg pickRegWithConstraints(
        OUT RegSet & set, LTConstraints const* lt_constraints);

    //Pick out all registers that alias with 'reg' and record in 'alias_set'.
    void pickOutAliasRegSet(Reg reg, OUT RegSet & alias_set);

    virtual void pickRegisterFromAliasSet(Reg r);

    //Pick reg from all caller alias register sets.
    virtual void pickRegisterFromCallerAliasSet(Reg r);

    //Pick reg from all callee alias register sets.
    virtual void pickRegisterFromCalleeAliasSet(Reg r);

    //Pick register for all param register set.
    virtual void pickRegisterFromParamAliasSet(Reg r);

    //Pick register for all return value register set.
    virtual void pickRegisterFromReturnValueAliasSet(Reg r);

    //Pick reg from all allocable register sets.
    void pickRegFromAllocable(Reg reg);

    //Record the allocation of callee.
    void recordUsedCallee(Reg r);

    //Record the allocation of caller.
    void recordUsedCaller(Reg r);

    //This function checks the registers listed in conflict_prs to see
    //if they have been allocated physical registers. If so, it removes
    //those registers from the provided set and stores the removed
    //registers in removed_regs_wrap.
    void removeConflictingReg(
        OUT RegSet & set, PRNOConstraintsTab const& conflict_prs,
        OUT RegSetWrap & removed_regs_wrap);
};
//END RegSetImpl


//
//START RoundRobinRegSetImpl
//
class RoundRobinRegSetImpl : public RegSetImpl {
    COPY_CONSTRUCTOR(RoundRobinRegSetImpl);
    BSIdx m_bsidx_marker;
public:
    RoundRobinRegSetImpl(LinearScanRA & ra) : RegSetImpl(ra)
    { m_bsidx_marker = BS_UNDEF; }
    virtual ~RoundRobinRegSetImpl() {}

    //Pick up a physical register from allocable register set by the roundrobin
    //way.
    Reg pickRegRoundRobin(RegSet & set);

    //Pick reg in a given regset and return it.
    virtual Reg pickReg(RegSet & set) override;
};
//END RoundRobinRegSetImpl


//
//START LRURegSetImpl
//
class LRURegSetImpl : public RegSetImpl {
    COPY_CONSTRUCTOR(LRURegSetImpl);
protected:
    //This class is an adapter class of the List class,
    //used to implement a queue (enqueue from tail, dequeue from head),
    //but this queue allows the deletion of elements at any position.
    //This class maintains a collection of free physical registers.
    //When a physical register is unused or released, it will be added
    //to the queue. Each time a physical register is requested,
    //it only needs to be retrieved from the head of the queue.
    //When it is empty, it returns REG-UNDEF, indicating that
    //there are no idle physical registers and may need to be split or spilled.
    //Through this class, the physical register selection strategy of LRU
    //can be implemented, as the newly used register is always placed at the
    //end of the queue, which means it will only be selected again when other
    //physical registers in the queue are exhausted.
    //At the beginning, it is assumed that the physical register is idle,
    //so it is queued sequentially. When a physical register is needed for
    //register allocation, the 'dequeue' interface is called to queue the
    //queue header element. When the lifetime of a PRNO ends, release
    //the physical registers it occupies and call the 'enqueue' interface
    //to join the physical registers from the end of the queue.
    //This ensures that the newly used physical register always appears
    //at the end of the queue, thus ensuring that the selected physical
    //register is always the oldest unused.
    class FreeRegQueue {
        COPY_CONSTRUCTOR(FreeRegQueue);
        typedef xcom::List<Reg> RegList;
        typedef xcom::List<Reg>::Iter RegIter;
        typedef xcom::Vector<RegIter> Reg2Iter;
        RegList m_list;

        //Mapping from Reg to RegIter.
        //Used for quickly deleting elements at any position
        //in 'm_list', with a time complexity of O(1).
        Reg2Iter m_reg2iter;

        void init()
        {
            m_reg2iter.grow(REG_NUM + 1);
            m_reg2iter.clean();
        }
    public:
        FreeRegQueue() { init(); }
        ~FreeRegQueue() {}

        //Extract an element from the head of the queue.
        Reg dequeue()
        {
            Reg reg = m_list.get_head();
            m_list.remove_head();
            m_reg2iter[reg] = nullptr;
            return reg;
        }

        //Add reg to the queue.
        void enqueue(Reg reg)
        {
            RegIter iter = m_list.append_tail(reg);
            ASSERT0(iter && m_reg2iter[reg] == nullptr);
            m_reg2iter[reg] = iter;
        }

        //Return true if m_list is empty.
        bool isEmpty() const
        { return m_list.get_elem_count() == 0; }

        //Remove 'reg' from the queue.
        void remove(Reg reg)
        {
            RegIter iter = m_reg2iter[reg];
            if (iter == nullptr) { return; }
            m_list.remove(iter);
            m_reg2iter[reg] = nullptr;
        }
    };

    //The 'm_free_xx' object stores free physical registers of different types.
    //Note that at this stage, it is not necessary to separately consider
    //registers of categories such as 'param', 'return_value', and 'div_rem',
    //as they all belong to the caller register. Only the caller and caller
    //types need to be distinguished at this stage.
    FreeRegQueue m_free_caller_scalar_queue;
    FreeRegQueue m_free_caller_vector_queue;
    FreeRegQueue m_free_callee_scalar_queue;
    FreeRegQueue m_free_callee_vector_queue;

    //Initialize the free register queue.
    virtual void initFreeQueue();

    //Init all register sets.
    virtual void initRegSet() override;

    //Pick up a physical register from allocable register set by the LRU way.
    virtual Reg pickRegLRU(RegSet & set);
public:
    LRURegSetImpl(LinearScanRA & ra) : RegSetImpl(ra) {}
    virtual ~LRURegSetImpl() {}

    //Free register from all callee alias register set.
    virtual void freeRegisterFromCalleeAliasSet(Reg r) override;

    //Free register from all caller alias register set.
    virtual void freeRegisterFromCallerAliasSet(Reg r) override;

    //Pick reg in a given regset and return it.
    virtual Reg pickReg(RegSet & set) override
    { return pickRegLRU(set); }

    //Pick reg in a given regset.
    //When a register has an alias, this function needs to
    //be called to remove the register 'r' that exists in the collection.
    virtual void pickReg(RegSet & set, Reg r) override;
};
//END LRURegSetImpl


//
//START LTConstraintsStrategy
//
//This class represents a strategy for imposing constraints on lifetime.
//This is a base class that different architectural strategies
//may need to inherit from, as the requirements vary.
class LTConstraintsStrategy {
    COPY_CONSTRUCTOR(LTConstraintsStrategy);
protected:
    LinearScanRA & m_ra;
public:
    LTConstraintsStrategy(LinearScanRA & ra) : m_ra(ra) {}
    virtual ~LTConstraintsStrategy() {}

    //Set the constraints for the current IR's lifetime.
    //Note that the constraints for each IR vary depending on the architecture.
    virtual void applyConstraints(IR *)
    { ASSERTN(0, ("Target Dependent Code")); }
};
//END LTConstraintsStrategy


//
//START LTConstraints
//
//This class represents a virtual register that requires additional constraints
//during allocation, with the specific restrictions being dependent
//on the hardware architecture.
//The class maintains two key sets of constraints:
//1. m_conflicting_prs: This table contains PRs that
//    cannot be allocated simultaneously with the current PR. If a PR is
//    listed in this set, it indicates a conflict that must be avoided to
//    maintain correct operation of the program.
//
//2. m_consistent_prs: This table includes PRs that must always be allocated
//    to the same physical register as the current PR across all uses. This
//    ensures that the same register is used consistently in all relevant
//    scenarios, preventing errors due to inconsistent allocations.
//
//If there is a need to introduce additional or more complex constraints,
//this class can be inherited to create specialized constraint management
//for specific architectures or use cases. The LTConstraints class
//represents the manifestation of lifetime constraints. The specific
//implementation of how these constraints are generated will require
//extending the LTConstraintsStrategy class, which is also a base
//class that should be inherited and expanded according to the needs
//of different architectures.
typedef xcom::TTabIter<PRNO> PRNOConstraintsTabIter;
class LTConstraints {
    COPY_CONSTRUCTOR(LTConstraints);
public:
    LTConstraints() {}
    ~LTConstraints() {}

    //Add the PR to the conflict table.
    void addConflictPR(PRNO pr) { m_conflicting_prs.append(pr); }

    //Add the PR to the consist table.
    void addConsistPR(PRNO pr) { m_consistent_prs.append(pr); }

    PRNOConstraintsTab const& getConflictTab() const
    { return m_conflicting_prs; }
    PRNOConstraintsTab const& getConsistTab() const { return m_consistent_prs; }

    bool isConflictPR(PRNO pr) const { return m_conflicting_prs.find(pr); }
    bool isConsistPR(PRNO pr) const { return m_consistent_prs.find(pr); }

    //In the process of register allocation, a PR may be split,
    //which results in changes to the PR.
    //It is necessary to update the conflicting
    //registers promptly to maintain the integrity of the allocation.
    //This function updates the set of conflicting registers
    //by replacing the old PR with the newly renamed PR,
    //ensuring that all references to the old PR are updated.
    void updateConflictPR(PRNO renamed_pr, PRNO old_pr);
protected:
    //The current PR can NOT be the same as the physical registers
    //with PRs in the conflict PR tab.
    PRNOConstraintsTab m_conflicting_prs;

    //The current PR can be the same as the physical registers
    //with PRs in the consistent PR tab.
    PRNOConstraintsTab m_consistent_prs;
};
//END LTConstraints


typedef List<LTConstraints*> LTConstraintsList;
typedef List<LTConstraints*>::Iter LTConstraintsListIter;
//
//START LTConstraintsMgr
//
//This class serves as a base class for managing lifetime constraints.
//It is designed to be overridden for different architectures,
//allowing users to implement their own architecture-specific
//LTConstraintsMgr by inheriting from this class.
class LTConstraintsMgr {
    COPY_CONSTRUCTOR(LTConstraintsMgr);
protected:
    LTConstraintsList m_ltc_list;
protected:
    void destroy();
    void init();
public:
    LTConstraintsMgr() { init(); }
    virtual ~LTConstraintsMgr() { destroy(); }

    virtual LTConstraints * allocLTConstraints();

    //Clean the lifetime constraints info before computation.
    void reset() { destroy(); init(); }
};
//END LTConstraintsMgr


typedef xcom::TMap<Type const*, Var *> Ty2Var;

//
//START LinearScanRA
//
//The class represents the basic structure and interface of linear-scan register
//allocation.
class LinearScanRA : public Pass {
    COPY_CONSTRUCTOR(LinearScanRA);
protected:
    bool m_is_apply_to_region;
    //Used to control the FP can be allocable or not based on the user's input.
    bool m_is_fp_allocable_allowed;
    bool m_has_alloca; //Whether alloca opeartion exists.

    //True to indicate that the stack space may need to be realigned.
    bool m_may_need_to_realign_stack;
    LifeTimeMgr * m_lt_mgr;

    //This life time manger is used to store the register lifetime, which can
    //be used to get the top view usage of the all phisical registers,
    //which will be used in the spill reload promotion algorithm to find
    //whether there is a proper register to eliminate the spill/reload in
    //a certain live range.
    RegLifeTimeMgr * m_reg_lt_mgr;

    IRCFG * m_cfg;
    IRMgr * m_irmgr;
    BBList * m_bb_list;
    LTConstraintsMgr * m_lt_constraints_mgr;
    LTConstraintsStrategy * m_lt_constraints_strategy;
    FakeIRMgr * m_fake_irmgr;
    UINT m_func_level_var_count;
    LTSet m_unhandled;
    LTSet m_handled;
    LTSet m_active;
    LTSet m_inactive;
    Vector<Reg> m_prno2reg;
    IRTab m_spill_tab;
    IRTab m_reload_tab;
    IRTab m_remat_tab;
    IRTab m_move_tab;
    ConstIRTab m_fake_use_head_tab;
    ConstIRTab m_fake_use_tail_tab;
    ConstBBTab m_hoist_bb_tab; //Store the inserted hoist BB.
    ConstBBTab m_latch_bb_tab; //Store the inserted latch BB.
    PreAssignedMgr m_preassigned_mgr;
    DedicatedMgr m_dedicated_mgr;
    PRNO2Var m_prno2var;
    ActMgr m_act_mgr;
    Ty2Var m_ty2var;
protected:
    LifeTimeMgr * allocLifeTimeMgr(Region * rg)
    { ASSERT0(rg); return new LifeTime2DMgr(rg); }
    FakeIRMgr * allocFakeIRMgr() { return new FakeIRMgr(m_rg, this); }
    RegLifeTimeMgr * allocRegLifeTimeMgr(Region * rg)
    { ASSERT0(rg); return new RegLifeTimeMgr(rg, this); }
    virtual RegSetImpl * allocRegSetImpl() { return new RegSetImpl(*this); }

    //Allocates an instance of the lifetime constraints strategy.
    //Derived classes can override this virtual function to provide
    //their own implementation of the allocation process.
    virtual LTConstraintsStrategy * allocLTConstraintsStrategy()
    { return new LTConstraintsStrategy(*this); }

    //Allocates an instance of the lifetime constraints manager.
    //Derived classes can override this virtual function to provide
    //their own implementation of the allocation process.
    virtual LTConstraintsMgr * allocLTConstraintsMgr()
    { return new LTConstraintsMgr(); }
public:
    explicit LinearScanRA(Region * rg);
    virtual ~LinearScanRA();

    void addUnhandled(LifeTime * lt);
    void addActive(LifeTime * lt);
    void addInActive(LifeTime * lt);
    void addHandled(LifeTime * lt);

    //The function builds PRNO with given 'type', and records the relation
    //between PRNO and 'reg' in current pass.
    //When TargInfo enabled, some PR operations might correspond to specific
    //physical register. In order to preserve this information, passes such
    //as IR simplification, calls this function.
    PRNO buildPrnoAndSetReg(Type const* type, Reg reg);

    //The function builds PRNO with given 'type', and records the relation
    //between PRNO and 'reg' in current pass. Unlike buildPrno(), the function
    //records the generated PRNO as a dedicated PRNO which binds a dedicated
    //physical register 'reg'.
    //When TargInfo enabled, some PR operations might correspond to dedicated
    //physical register. In order to preserve this information, passes such
    //as IR simplification, calls this function.
    PRNO buildPrnoDedicated(Type const* type, Reg reg);

    //The function builds PRNO with given 'type', and records the relation
    //between PRNO and 'reg' in current pass. Unlike buildPrno(), the function
    //records the generated PRNO as a pre-assigned PRNO which binds a
    //pre-assigned physical register 'reg'.
    //When TargInfo enabled, some PR operations might correspond to dedicated
    //physical register. In order to preserve this information, passes such
    //as IR simplification, calls this function.
    PRNO buildPrnoPreAssigned(Type const* type, Reg reg);

    //Build the MOV IR inserted by register allocation.
    IR * buildMove(PRNO to, PRNO from, Type const* fromty, Type const* toty);

    //Build the RHS of MOV IR inserted by register allocation.
    virtual IR * buildMoveRHS(PRNO pr, Type const* ty) const;

    virtual IR * buildReload(PRNO prno, Var * spill_loc, Type const* ty);
    virtual IR * buildRemat(PRNO prno, RematCtx const& rematctx,
                            Type const* ty);
    virtual IR * buildSpill(PRNO prno, Type const* ty);
    virtual IR * buildSpillByLoc(PRNO prno, Var * spill_loc, Type const* ty);

    //The function checks whether the register-allocation result should be
    //applied to current region or just an estimiation of register pressure.
    //NOTE: If the register-allocation behaviors applied to current region,
    //the spilling, reloading operations and latch-BB that generated by LSRA
    //will be inserted into region's BB list and CFG.
    void checkAndApplyToRegion(MOD ApplyToRegion & apply);

    //Determine whether the PASS apply all modifications of CFG and BB to
    //current region. User may invoke LSRA as performance estimating tools
    //to conduct optimizations, such as RP, GCSE, UNROLLING which may increase
    //register pressure.
    void checkAndPrepareApplyToRegion(OUT ApplyToRegion & apply);

    //The function check whether 'lt' value is simple enough to rematerialize.
    //And return the information through rematctx.
    virtual bool checkLTCanBeRematerialized(
        MOD LifeTime * lt, OUT RematCtx & rematctx);

    void doBackwardJumpAnalysis()
    {
        ASSERT0(m_fake_irmgr);
        m_fake_irmgr->doBackwardJumpAnalysis();
    }

    void dumpDOTWithReg() const;
    void dumpDOTWithReg(CHAR const* name, UINT flag) const;
    void dumpPR2Reg(PRNO prno) const;
    void dumpPR2Reg() const;
    void dump4List() const;
    bool dump(bool dumpir = true) const;
    void dumpRegLTOverview() const;
    void dumpBBListWithReg() const;
    void dumpReg2LT(Pos start, Pos end, bool open_range = false) const;
    void dumpPosGapIRStatistics() const;

    void destroyLocalUsage();
    void destroy();

    void freeReg(Reg reg);
    void freeReg(LifeTime const* lt);

    //Generate the remat information by traversing the lifetime list.
    void genRematInfo();
    void genRematForLT(MOD LifeTime * lt) const;

    //This func is used to find the ancestor prno if an original prno is split
    //more than one time and is renamed many times.
    PRNO getAnctPrno(PRNO prno) const;

    //Get the BB that will used to insert the spill IR of callee registers.
    //Normally, it is the entry BB of the CFG.
    virtual IRBB * getCalleeSpilledBB() const
    { return m_rg->getCFG()->getEntry(); }

    //Get the source pr of MOV IR inserted by register allocation.
    virtual IR * getMoveSrcPr(IR const* mov) const;

    //Construct a name for Var that will lived in Region.
    CHAR const* genFuncLevelNewVarName(OUT xcom::StrBuf & name);
    Var * getSpillLoc(Type const* ty);
    Var * getSpillLoc(PRNO prno);
    Var * genSpillLoc(PRNO prno, Type const* ty);
    Var * genFuncLevelVar(Type const* type, UINT align);
    PRNO2Var const& getPrno2Var() const { return m_prno2var; }
    Reg getReg(PRNO prno) const;
    REGFILE getRegFile(Reg r) const;
    Reg getReg(LifeTime const* lt) const;
    LifeTime * getLT(PRNO prno) const;
    CHAR const* getRegName(Reg r) const;
    CHAR const* getRegFileName(REGFILE rf) const;
    LTSet & getUnhandled() { return m_unhandled; }
    LTSet & getActive() { return m_active; }
    LTSet & getInActive() { return m_inactive; }
    LTSet & getHandled() { return m_handled; }
    IRTab & getSpillTab() { return m_spill_tab; }
    IRTab & getReloadTab() { return m_reload_tab; }
    IRTab & getRematTab() { return m_remat_tab; }
    IRTab & getMoveTab() { return m_move_tab; }
    BBList * getBBList() const { return m_bb_list; }
    IRCFG * getCFG() const { return m_cfg; }
    TargInfoMgr & getTIMgr() const
    { return *(m_rg->getRegionMgr()->getTargInfoMgr()); }
    LifeTimeMgr & getLTMgr() { return *m_lt_mgr; }
    RegLifeTimeMgr & getRegLTMgr() { return *m_reg_lt_mgr; }
    LTConstraintsMgr * getLTConstraintsMgr()
    { return m_lt_constraints_mgr; }

    //Return dedicated prno by given physical register.
    //e.g: given REG_SP, return the $sp as result.
    PRNO getDedicatedPRNO(Reg reg) const
    { return const_cast<LinearScanRA*>(this)->m_dedicated_mgr.geti(reg); }

    PreAssignedMgr & getPreAssignedMgr() { return m_preassigned_mgr; }
    Reg getPreAssignedReg(LifeTime const* lt) const
    { return getPreAssignedReg(lt->getPrno()); }

    //Return physical register by given pre-assigned prno.
    Reg getPreAssignedReg(PRNO prno) const;
    ActMgr & getActMgr() { return m_act_mgr; }
    virtual CHAR const* getPassName() const
    { return "Linear Scan Register Allocation"; }
    PASS_TYPE getPassType() const { return PASS_LINEAR_SCAN_RA; }

    //This function mainly prepares the correct register type for a prno,
    //not the data type of lt.
    Type const* getVarTypeOfPRNO(PRNO prno) const;

    //This function returns the type when do the spill operation for a prno.
    //This function can be overidden by the derived class if the required
    //spill type is not the original type of the prno.
    //Prno: the input prno.
    virtual Type const* getSpillType(PRNO prno) const;

    //This function returns the actual type for the input type.
    //This function can be overidden by the derived class if the required
    //spill type is not the same as the original input type.
    //ty: the input type.
    virtual Type const* getSpillType(Type const* ty) const
    { ASSERT0(ty); return ty; }

    //Get the available register for LSRA optimization.
    //Target dependent code.
    Reg getAvaiRegForLsraOpt() const { return REG_UNDEF; }

    //Get end caller saved scalar register.
    Reg getCallerScalarEnd() const { return getTIMgr().getCallerScalarEnd(); }

    //Get start caller saved scalar register.
    Reg getCallerScalarStart() const
    { return getTIMgr().getCallerScalarStart(); }

    //Get frame pointer register.
    Reg getFP() const { return getTIMgr().getFP(); }

    //Get global pointer register.
    Reg getGP() const { return getTIMgr().getGP(); }

    //Get number of param passed by register.
    UINT getNumOfParamByReg() const
    { return getTIMgr().getNumOfParamByReg(); }

    //Get start parameter scalar register.
    xgen::Reg getParamScalarStart() const
    { return getTIMgr().getParamScalarStart(); }

    //Get program counter register.
    Reg getPC() const { return getTIMgr().getPC(); }

    //Get returned address register.
    Reg getRA() const { return getTIMgr().getRA(); }

    //Get stack pointer register.
    Reg getSP() const { return getTIMgr().getSP(); }

    //Get target address register.
    Reg getTA() const { return getTIMgr().getTA(); }

    //Get program counter register.
    virtual xgen::Reg getTargetPC() const
    { ASSERTN(0, ("Target Dependent Code")); return (xgen::Reg)REG_UNDEF; }

    //The temporary register is a reserved register that used to save a
    //temporary value, which is usually used after the register allocation
    //and does not be assigned in the register allocation.
    Reg getTempReg(Type const* ty) const
    { return ty->is_vector() ? getTempVector() : getTempScalar(ty); }
    Reg getTempScalar(Type const* ty) const
    { return getTIMgr().getTempScalar(ty); }
    Reg getTempVector() const { return getTIMgr().getTempVector(); }

    //Get a temp memory location per the specified type.
    //ty: the input type.
    Var * getTempVar(Type const* ty)
    { ASSERT0(ty);  return getSpillLoc(ty); }

    //Get zero register.
    Reg getZero(Type const* ty) const
    {
        ASSERT0(ty != nullptr);
        return getTIMgr().getZero(ty);
    }
    Reg getZeroScalar() const { return getTIMgr().getZeroScalar(); }
    Reg getZeroScalarFP() const { return getTIMgr().getZeroScalarFP(); }
    Reg getZeroVector() const { return getTIMgr().getZeroVector(); }

    bool hasReg(PRNO prno) const;
    bool hasReg(LifeTime const* lt) const;

    //Whether the dynamic stack function has been performed and used frame
    //pointer register before.
    bool hasAlloca() const { return m_has_alloca; }

    virtual bool isCalleePermitted(LifeTime const* lt) const;
    bool isPreAssigned(PRNO prno) const
    { return m_preassigned_mgr.isPreAssigned(prno); }

    //Check the Frame Pointer Register can be allocable or not.
    bool isFPAllocableAllowed() const { return m_is_fp_allocable_allowed; }

    //Whether the frame pointer register is allocable. If the debug mode is not
    //turned on, and dynamic stack, prologue/epilogue inserter function not use
    //this register, it can be used for other purposes.
    bool isFPAllocable() const;

    //Return true if the register-allocation result should be applied to
    //current region's BB list and CFG.
    bool isApplyToRegion() const { return m_is_apply_to_region; }

    //Return true if a fake-use IR at the first BB of loop by lexicographical
    //order.
    //e.g: "[mem]<-fake_use $1" is a fake-use IR at the first BB of loop by
    //      lexicographical order.
    //       ----->BB1
    //      |      [mem]<-fake_use $1
    //      |      ...
    //      |      |
    //      |      V
    //      |      BB2
    //      |      |
    //      |      V
    //      |      BB3
    //      |      |
    //      |      V
    //      |      BB4
    //      |      |
    //      '------|
    //             V
    bool isFakeUseAtLexFirstBBInLoop(IR const* ir) const
    { return m_fake_use_head_tab.find(ir); }

    //Return true if a fake-use IR at the last BB of loop by lexicographical
    //order.
    //e.g: "[mem]<-fake_use $1" is a fake-use IR at the last BB of loop by
    //      lexicographical order.
    //       ----->BB1
    //      |      |
    //      |      V
    //      |      BB2
    //      |      |
    //      |      V
    //      |      BB3
    //      |      |
    //      |      V
    //      |      BB4
    //      |      ...
    //      |      [mem]<-fake_use $1
    //      |      |
    //       ------|
    //             V
    bool isFakeUseAtLexLastBBInLoop(IR const* ir) const
    { return m_fake_use_tail_tab.find(ir); }

    bool isInsertOp() const
    {
        LinearScanRA * pthis = const_cast<LinearScanRA*>(this);
        return pthis->getSpillTab().get_elem_count() != 0 ||
               pthis->getReloadTab().get_elem_count() != 0 ||
               pthis->getMoveTab().get_elem_count() != 0;
    }

    //Return true if the BB is hoist BB.
    bool isHoistBB(IRBB const* bb) const
    {
        ASSERT0(bb != nullptr);
        return m_hoist_bb_tab.find(bb);
    }

    //Retun true if the lifetime of prno can overlap with other lifetime
    //assigned to the same physical register.
    bool canInterfereWithOtherLT(PRNO prno) const;

    //Return true if the BB is a latch BB.
    bool isLatchBB(IRBB const* bb) const
    {
        ASSERT0(bb != nullptr);
        return m_latch_bb_tab.find(bb);
    }

    bool isMoveOp(IR const* ir) const
    { return m_move_tab.find(const_cast<IR*>(ir)); }

    bool isPrnoAlias(PRNO prno, PRNO alias) const;
    //Return true if ir is rematerializing operation.
    virtual bool isRematLikeOp(IR const* ir) const
    {
        if (!ir->is_stpr()) { return false; }
        IR const* rhs = ir->getRHS();
        ASSERT0(rhs);
        if (rhs->is_lda() || rhs->is_const()) { return true; }
        return false;
    }
    bool isReloadOp(IR const* ir) const
    { return m_reload_tab.find(const_cast<IR*>(ir)); }

    bool isRematOp(IR const* ir) const
    { return m_remat_tab.find(const_cast<IR*>(ir)); }

    bool isSpillOp(IR const* ir) const
    { return m_spill_tab.find(const_cast<IR*>(ir)); }

    //If the IR is not encoded with a position, return true. Normally
    //include the remat/mov/spill/reload IR.
    bool isOpInPosGap(IR const* ir) const
    {
        ASSERT0(ir);
        return isSpillOp(ir) || isReloadOp(ir) || isRematOp(ir) ||
            isMoveOp(ir);
    }

    //This func is used to check the TMP register is available or not for the
    //input type.
    virtual bool isTmpRegAvailable(Type const*) const
    { ASSERTN(0, ("Target Dependent Code")); return false; }

    //Return true if the usage of 'reg' is unique, and there is only unique
    //PRNO corresponds to it.
    //e.g:During entire LSRA processing, the PRNO of %sp is unique.
    bool isDedicatedReg(Reg r) const
    {
        return getSP() == r || getFP() == r || getGP() == r ||
            isZeroRegister(r);
    }

    //Return true if the target machine's physical register convention
    //specifies 'r' has dedicated usage, such as ReturnAddress register.
    //And the dedicated register does not belong to any aliased register-set.
    //However, the convention does not constrain that the mapping between
    //PRNO and 'r' must be unqiue.
    //e.g: $100 may assigned 'r' and $200 also can be assigned 'r'.
    bool isPreAssignedReg(Reg r) const
    {
        return getSP() == r || getFP() == r || getTA() == r ||
               getGP() == r || getRA() == r || isZeroRegister(r);
    }

    //Check the lifetime is use the fake-use IR or not at the first BB in loop.
    bool isLTWithFakeUseAtLexFirstBBInLoop(LifeTime const* lt) const
    {
        ASSERT0(lt);
        IR * first = const_cast<LifeTime*>(lt)->getFirstOccStmt();
        return isFakeUseAtLexFirstBBInLoop(first);
    }
    void initLTMgr()
    {
        if (m_lt_mgr != nullptr) { return; }
        m_lt_mgr = allocLifeTimeMgr(m_rg);
        m_reg_lt_mgr = allocRegLifeTimeMgr(m_rg);
    }
    void initFakeIRMgr()
    {
        if (m_fake_irmgr != nullptr) { return; }
        m_fake_irmgr = allocFakeIRMgr();
    }

    //Initialize the allocation strategy for the lifetime constraint set.
    //Note that different architectures have varying strategies.
    void initConstraintsStrategy()
    {
        if (m_lt_constraints_strategy != nullptr) { return; }
        m_lt_constraints_strategy = allocLTConstraintsStrategy();
    }

    //Initializes the lifetime constraint management unit.
    //Note that different architectures have different constraints.
    void initLTConstraintsMgr()
    {
        if (m_lt_constraints_mgr != nullptr) { return; }
        m_lt_constraints_mgr = allocLTConstraintsMgr();
    }
    void initLocalUsage();

    //Return true if the reg is a ZERO register.
    bool isZeroRegister(Reg r) const
    {
        ASSERT0(r != REG_UNDEF);
        return r == getZeroScalar() || r == getZeroVector() ||
            r == getZeroScalarFP();
    }

    //Whether the FP can be preserved when stack aligned.
    //If FP registers are not required for stack alignment,
    //FP can be used as normal registers for allocation.
    bool canPreservedFPInAlignStack() const
    {
        if (!m_may_need_to_realign_stack) { return true; }
        ConstVarList const* lst = m_rg->findAndRecordFormalParamList(true);
        ASSERT0(lst != nullptr);
        return !g_force_use_fp_as_sp && !hasAlloca() &&
            lst->get_elem_count() <= getNumOfParamByReg();
    }

    virtual bool perform(OptCtx & oc);
    virtual bool performLsraImpl(OptCtx & oc);

    void recalculateSSA(OptCtx & oc) const;

    //Removed the fake-use IR inserted before the LSRA.
    void removeFakeUseIR()
    { m_fake_irmgr->removeFakeUseIR(); }

    void removeMoveOp(IR const* ir)
    { ASSERT0(ir); m_move_tab.remove(const_cast<IR*>(ir)); }
    void removeOpInPosGapRecord(IR const* ir);
    void removeReloadOp(IR const* ir)
    { ASSERT0(ir); m_reload_tab.remove(const_cast<IR*>(ir)); }
    void removeRematOp(IR const* ir)
    { ASSERT0(ir); m_remat_tab.remove(const_cast<IR*>(ir)); }
    void removeSpillOp(IR const* ir)
    { ASSERT0(ir); m_spill_tab.remove(const_cast<IR*>(ir)); }

    //Reset all resource before allocation.
    void reset();

    void setApplyToRegion(bool doit) { m_is_apply_to_region = doit; }
    void setPreAssignedReg(PRNO prno, Reg r)
    {
        if (isDedicatedReg(r)) {
            m_dedicated_mgr.add(prno, r);
        }
        m_preassigned_mgr.add(prno, r);
    }

    //Normally m_is_fp_allocable_allowed can be set to false only, because the
    //default value is true, it will be set to false if we find the fp is used
    //to implement the alloca on stack, debug option or special register, and
    //it is not expected to be allocable to other PRs in register allocation. In
    //other situations, it should be participated to the register allocation as
    //the regular registers.
    void setFPAllocableAllowed(bool allowed)
    { m_is_fp_allocable_allowed = allowed; }
    void setFakeUseAtLexFirstBBInLoop(IR * ir)
    { m_fake_use_head_tab.append(ir); }
    void setFakeUseAtLexLastBBInLoop(IR * ir)
    { m_fake_use_tail_tab.append(ir); }
    void setHoistBB(IRBB * bb) { ASSERT0(bb); m_hoist_bb_tab.append(bb); }
    void setLatchBB(IRBB * bb) { ASSERT0(bb); m_latch_bb_tab.append(bb); }
    void setMove(IR * ir) { m_move_tab.append(ir); }
    void setReg(PRNO prno, Reg reg);
    void setReload(IR * ir) { m_reload_tab.append(ir); }
    void setRemat(IR * ir) { m_remat_tab.append(ir); }
    void setSpill(IR * ir) { m_spill_tab.append(ir); }

    //Set the constraint set for each IR.
    void scanIRAndSetConstraints();

    //After the lifetime calculation is completed, begin setting constraint
    //sets for each lifetime.
    void tryComputeConstraints();

    bool verify4List() const;
    bool verifyAfterRA() const;
    bool verifyLSRAByInterfGraph(OptCtx & oc) const;
};
//END LinearScanRA

//
//START LTInterfGraph
//
class LTInterfGraphLSRAChecker : public Graph {
    COPY_CONSTRUCTOR(LTInterfGraphLSRAChecker);
protected:
    Region * m_rg;
    LifeTime2DMgr & m_lt_mgr;
protected:
    bool canSkipCheck(LinearScanRA const* lsra, xcom::Edge const* e) const;
public:
    LTInterfGraphLSRAChecker(Region * rg, LifeTime2DMgr & mgr)
        : m_rg(rg), m_lt_mgr(mgr)
    { set_direction(false); set_dense(false); }
    ~LTInterfGraphLSRAChecker() {}

    //Build interference graph.
    void build();

    bool check(LinearScanRA * lsra);

    void dumpGraph(CHAR const* name) const { dumpDOT(name); }
};
//END LTInterfGraph

void dumpBBListWithReg(Region const* rg);
void dumpDOTWithReg(
    Region const* rg, CHAR const* name = nullptr,
    UINT flag = IRCFG::DUMP_COMBINE);

} //namespace xoc
#endif
