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
#ifndef _IR_SSA_H_
#define _IR_SSA_H_

namespace xoc {

class IR_SSA_MGR;

//Dominace Frontier manager
class DfMgr {
    IR_SSA_MGR * m_ssa_mgr;
    BitSetMgr m_bs_mgr;
    Vector<BitSet*> m_df_vec;
public:
    explicit DfMgr(IR_SSA_MGR * sm);
    COPY_CONSTRUCTOR(DfMgr);

    void clean();
    void build(DGraph & g);
    void dump(DGraph & g);

    //Return the BB set controlled by bbid.
    BitSet const* get_df_ctrlset_c(UINT bbid) const
    { return m_df_vec.get(bbid); }

    //Get the BB set controlled by v.
    BitSet * get_df_ctrlset(Vertex const* v);

    inline void rebuild(DGraph & g) { clean(); build(g); }
};


//SSAGraph
class SSAGraph : Graph {
    Region * m_ru;
    IR_SSA_MGR * m_ssa_mgr;
    TMap<UINT, VP*> m_vdefs;
public:
    SSAGraph(Region * ru, IR_SSA_MGR * ssamgr);
    COPY_CONSTRUCTOR(SSAGraph);
    void dump(IN CHAR const* name = NULL, bool detail = true);
};


typedef Vector<Vector<VP*>*> BB2VP;


//Perform SSA based optimizations.
class IR_SSA_MGR : public Pass {
protected:
    Region * m_ru;
    SMemPool * m_vp_pool;
    TypeMgr * m_dm;
    IR_CFG * m_cfg;
    DefSegMgr * m_seg_mgr;
    bool m_is_ssa_constructed;
    UINT m_vp_count;
    IRIter m_iter; //for tmp use.

    //Record versions for each PRs.
    UINT2VPvec m_map_prno2vp_vec;

    //Record version stack during renaming.
    UINT2VPstack m_map_prno2stack;
    Vector<VP*> m_vp_vec;
    Vector<UINT> m_max_version; //record version number counter for pr.

    //Record the duplicated IR* to each prno.
    //Be used to generate phi for each prno.
    Vector<IR*> m_prno2ir;

protected:
    inline void init()
    {
        if (m_vp_pool != NULL) { return; }
        m_vp_count = 1;
        m_is_ssa_constructed = false;
        m_vp_pool = smpoolCreate(sizeof(VP)*2, MEM_CONST_SIZE);
    }

    void clean()
    {
        m_ru = NULL;
        m_dm = NULL;
        m_seg_mgr = NULL;
        m_cfg = NULL;
        m_vp_count = 1;
        m_is_ssa_constructed = false;
        m_vp_pool = NULL;
    }
    void cleanPRSSAInfo();
    void constructMDDUChainForPR();
    void cleanPRNO2Stack();
    void collectDefinedPR(IN IRBB * bb, OUT DefSBitSet & mustdef_pr);
    void computeEffectPR(IN OUT BitSet & effect_prs,
                         IN BitSet & defed_prs,
                         IN IRBB * bb,
                         IN PRDF & live_mgr,
                         IN Vector<BitSet*> & pr2defbb);

    void destructBBSSAInfo(IRBB * bb, IN OUT bool & insert_stmt_after_call);
    void destructionInDomTreeOrder(IRBB * root, Graph & domtree);

    void handleBBRename(IRBB * bb,
                        IN DefSBitSet & defed_prs,
                        IN OUT BB2VP & bb2vp);

    Stack<VP*> * mapPRNO2VPStack(UINT prno);
    inline IR * mapPRNO2IR(UINT prno)
    { return m_prno2ir.get(prno); }

    VP * newVP()
    {
        ASSERT(m_vp_pool != NULL, ("not init"));
        VP * p = (VP*)smpoolMallocConstSize(sizeof(VP), m_vp_pool);
        ASSERT0(p);
        memset(p, 0, sizeof(VP));
        return p;
    }

    void refinePhi(List<IRBB*> & wl);
    void rename(DefSBitSet & effect_prs,
                Vector<DefSBitSet*> & defed_prs_vec,
                Graph & domtree);
    void rename_bb(IRBB * bb);
    void renameInDomTreeOrder(
                IRBB * root,
                Graph & dtree,
                Vector<DefSBitSet*> & defed_prs_vec);

    void stripVersionForBBList();
    void stripVersionForAllVP();
    bool stripPhi(IR * phi, C<IR*> * phict);
    void stripSpecifiedVP(VP * vp);
    void stripStmtVersion(IR * stmt, BitSet & visited);

    void placePhiForPR(UINT prno,
                       IN List<IRBB*> * defbbs,
                       DfMgr & dfm,
                       BitSet & visited,
                       List<IRBB*> & wl);
    void placePhi(IN DfMgr & dfm,
                  IN OUT DefSBitSet & effect_prs,
                  DefMiscBitSetMgr & bs_mgr,
                  Vector<DefSBitSet*> & defed_prs_vec,
                  List<IRBB*> & wl);

public:
    explicit IR_SSA_MGR(Region * ru)
    {
        clean();
        ASSERT0(ru);
        m_ru = ru;

        m_dm = ru->get_type_mgr();
        ASSERT0(m_dm);

        ASSERT0(ru->getMiscBitSetMgr());
        m_seg_mgr = ru->getMiscBitSetMgr()->get_seg_mgr();
        ASSERT0(m_seg_mgr);

        m_cfg = ru->get_cfg();
        ASSERT(m_cfg, ("cfg is not available."));
    }
    COPY_CONSTRUCTOR(IR_SSA_MGR);
    ~IR_SSA_MGR() { destroy(false); }

    void buildDomiateFrontier(OUT DfMgr & dfm);
    void buildDUChain(IR * def, IR * use)
    {
        ASSERT0(def->is_write_pr() || def->isCallHasRetVal());
        ASSERT0(use->is_read_pr());
        SSAInfo * ssainfo = def->get_ssainfo();
        if (ssainfo == NULL) {
            ssainfo = newSSAInfo(def->get_prno());
            def->set_ssainfo(ssainfo);
            SSA_def(ssainfo) = def;

            //You may be set multiple defs for use.
            ASSERT(use->get_ssainfo() == NULL, ("use already has SSA info."));

            use->set_ssainfo(ssainfo);
        }

        SSA_uses(ssainfo).append(use);
    }

    //is_reinit: this function is invoked in reinit().
    void destroy(bool is_reinit)
    {
        if (m_vp_pool == NULL) { return; }

        ASSERT(!m_is_ssa_constructed,
            ("Still in ssa mode, you should out of SSA before the destruction."));

        for (INT i = 0; i <= m_map_prno2vp_vec.get_last_idx(); i++) {
            Vector<VP*> * vpv = m_map_prno2vp_vec.get((UINT)i);
            if (vpv != NULL) { delete vpv; }
        }

        cleanPRNO2Stack();

        for (INT i = 0; i <= m_vp_vec.get_last_idx(); i++) {
            VP * v = m_vp_vec.get((UINT)i);
            if (v != NULL) {
                v->destroy();
            }
        }

        if (is_reinit) {
            m_map_prno2vp_vec.clean();
            m_vp_vec.clean();
            m_max_version.clean();
            m_prno2ir.clean();
        }

        //Do not free irs in m_prno2ir.
        smpoolDelete(m_vp_pool);
        m_vp_pool = NULL;
    }

    void destruction(DomTree & domtree);
    void destructionInBBListOrder();
    void dump();
    void dump_all_vp(bool have_renamed);
    CHAR * dump_vp(IN VP * v, OUT CHAR * buf);
    void dump_ssa_graph(CHAR * name = NULL);

    void construction(OptCTX & oc);
    void construction(DomTree & domtree);
    UINT count_mem();

    inline Vector<VP*> const* get_vp_vec() const { return &m_vp_vec; }
    inline VP * get_vp(UINT id) const { return m_vp_vec.get(id); }

    IR * initVP(IN IR * ir);
    void insertPhi(UINT prno, IN IRBB * bb);

    //Return true if PR ssa is constructed.
    //This flag will direct the behavior of optimizations.
    //If SSA constructed, DU mananger will not compute any information for PR.
    bool is_ssa_constructed() const { return m_is_ssa_constructed; }

    /* Return true if phi is redundant, otherwise return false.
    If all opnds have same defintion or defined by current phi,
    the phi is redundant.
    common_def: record the common_def if the definition of all opnd is the same. */
    bool is_redundant_phi(IR const* phi, OUT IR ** common_def) const;

    //Allocate VP and ensure it is unique according to 'version' and 'prno'.
    VP * newVP(UINT prno, UINT version)
    {
        ASSERT0(prno > 0);
        Vector<VP*> * vec = m_map_prno2vp_vec.get(prno);
        if (vec == NULL) {
            vec = new Vector<VP*>();
            m_map_prno2vp_vec.set(prno, vec);
        }

        VP * v = vec->get(version);
        if (v != NULL) {
            return v;
        }

        ASSERT(m_seg_mgr, ("SSA manager is not initialized"));
        v = newVP();
        v->initNoClean(m_seg_mgr);
        VP_prno(v) = prno;
        VP_ver(v) = version;
        SSA_id(v) = m_vp_count++;
        SSA_def(v) = NULL;
        vec->set(version, v);
        m_vp_vec.set(SSA_id(v), v);
        return v;
    }


    //Allocate SSAInfo for specified PR indicated by 'prno'.
    inline SSAInfo * newSSAInfo(UINT prno)
    {
        ASSERT0(prno > 0);
        return (SSAInfo*)newVP(prno, 0);
    }

    //Reinitialize SSA manager.
    //This function will clean all informations and recreate them.
    inline void reinit()
    {
        destroy(true);
        init();
    }

    bool verifyPhi(bool is_vpinfo_avail);
    bool verifyPRNOofVP(); //Only used in IR_SSA_MGR.
    bool verifyVP(); //Only used in IR_SSA_MGR.
    bool verifySSAInfo(); //Can be used in any module.

    virtual CHAR const* get_pass_name() const
    { return "SSA Optimization Manager"; }

    PASS_TYPE get_pass_type() const { return PASS_SSA_MGR; }
};


bool verifySSAInfo(Region * ru);

} //namespace xoc
#endif
