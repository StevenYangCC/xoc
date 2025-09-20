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
#ifndef _INVERT_BRTGT_H_
#define _INVERT_BRTGT_H_

namespace xoc {

class InvertBrTgt;

//The optimization attempt to invert branch target.
class InvertBrTgt : public Pass {
    COPY_CONSTRUCTOR(InvertBrTgt);
    ConstIRList * m_changed_irlist; //used for dump
    OptCtx * m_oc;
protected:
    //Record BB order.
    xcom::Vector<UINT> m_bb_order;

    void addDump(IR const* ir) const;
    void dumpInit();
    void dumpFini();
public:
    explicit InvertBrTgt(Region * rg) : Pass(rg)
    {
        ASSERT0(rg != nullptr);
        m_oc = nullptr;
        m_changed_irlist = nullptr;
    }
    virtual ~InvertBrTgt() { dumpFini(); }

    //The function dump pass relative information before performing the pass.
    //The dump information is always used to detect what the pass did.
    //Return true if dump successed, otherwise false.
    virtual bool dumpBeforePass() const { return Pass::dumpBeforePass(); }

    //The function dump pass relative information.
    //The dump information is always used to detect what the pass did.
    //Return true if dump successed, otherwise false.
    virtual bool dump() const;

    virtual CHAR const* getPassName() const
    { return "Invert Branch Target"; }
    virtual PASS_TYPE getPassType() const { return PASS_INVERT_BRTGT; }
    OptCtx * getOptCtx() const { return m_oc; }

    //This function is used to further validate whether this optimization
    //is feasible based on the characteristics of different architectures.
    //These characteristics may limit the performance of the optimization.
    //For example, though the inverted br optimization can reduce a 'goto'
    //instruction when the program jump back to loop, it may occurs branch
    //prediction failure. Ultimately, there may be no performance gains
    //from it. Thus the characteristics of branch prediction needs to be
    //considered before inverted falsebr/truebr in br instruction according
    //to different architectures.
    //'head_bb': the head BB of loop.
    //'br_bb':  the BB to which br instruction belong.
    //'out_bb': the first outside BB of loop.
    //return: whether the program can benefits from this optimization
    //        after other characteristics in architecture considered.
    bool canBeInvertedBRCandidate(IRBB const* head_bb, IRBB const* br_bb,
                                  IRBB const* out_bb);

    //Branch prediction may affect the performance of inverted br optimization.
    //Thus this characteristics needs to be considered. And there is different
    //branch prediction implemented in different architectures, this function
    //is used to validate the effect. For example, the sign of value in br
    //instruction may be used to determined the predicted direction in static
    //branch prediction, then BB order may affect the performance.
    //e.g.:
    //  the format of branch instruction: br disp.
    //    a.'disp' represents the distance of the target BB from current pc.
    //    b.if the value in 'disp' is negative, the predicted of br is taken.
    //      if the value in 'disp' is positive, the predicted of br is no-taken.
    //Based on the above, the predicted direction will be determined by the
    //BB order. Thus 'm_bb_order' that record the BB order will be used to
    //judge the br direction and whether can be gained from this optimization.
    //'head_bb': the head BB of loop.
    //'br_bb':  the BB to which br instruction belong.
    //'out_bb': the first outside BB of loop.
    //return: whether the program can benefits from this optimization
    //        after branch prediction characteristics considered.
    virtual bool canBeGainedAfterConsideredBranchPrediction(
        IRBB const* head_bb, IRBB const* br_bb, IRBB const* out_bb)
    {
        ASSERT0(head_bb && br_bb && out_bb);
        return true;
    }

    //Implement the inverted br function.
    //'br': represent falsebr/truebr instrctuion.
    //'br_tgt': BB to which 'br' belong.
    //'jmp': 'goto LABEL' instruction, 'LABEL' will be changed after inverted.
    //'jmp': BB to which 'jmp' belong.
    //Return true if there is loop changed.
    bool invertLoop(MOD IR * br, MOD IRBB * br_tgt,
                    MOD IR * jmp, MOD IRBB * jmp_tgt);

    //The entry function to iterative loop to implement inverted br function.
    //'li': LoopInfo Tree:
    //Return true if there is loop changed.
    bool iterLoopTree(LI<IRBB> const* li);

    //Record BB order into 'm_bb_order'.
    void recordBBOrder();

    //Find the suitable loop and implement the inverted br function.
    //'li': LoopInfo Tree:
    //Return true if there is loop changed.
    bool tryInvertLoop(LI<IRBB> const* li);

    virtual bool perform(OptCtx & oc);
};

} //namespace xoc
#endif
