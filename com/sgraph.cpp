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
#include "xcominc.h"

namespace xcom {

//Expect for unique vertex in graphic depitction
#define ALWAYS_VERTEX_UNIQUE

//
//START Vertex
//
UINT Vertex::getInDegree() const
{
    return xcom::cnt_list(VERTEX_in_list(this));
}


UINT Vertex::getOutDegree() const
{
    return xcom::cnt_list(VERTEX_out_list(this));
}


//Return the vertex that is the from-vertex of Nth in-edge.
//n: the index of vertex, start from 0.
Vertex const* Vertex::getNthInVertex(UINT n) const
{
    UINT i = 0;
    EdgeC const* ec = getInList();
    for (; i < n && ec != nullptr; ec = ec->get_next(), i++) {
    }
    if (ec != nullptr) {
        return ec->getFrom();
    }
    return nullptr;
}


//Return the vertex that is the from-vertex of Nth out-edge.
//n: the index of vertex, start from 0.
Vertex const* Vertex::getNthOutVertex(UINT n) const
{
    UINT i = 0;
    EdgeC const* ec = getOutList();
    for (; i < n && ec != nullptr; ec = ec->get_next(), i++) {
    }
    if (ec != nullptr) {
        return ec->getTo();
    }
    return nullptr;
}
//END Vertex


//
//START CompareEdgeFunc
//
Edge * CompareEdgeFunc::createKey(Edge const* ref)
{
    ASSERT0(m_g);
    EdgeC * outec;
    return m_g->newEdgeImpl(ref->from(), ref->to(), &m_inec, &outec);
}
//END CompareEdgeFunc


//DONT CALL Constructor directly.
Graph::Graph(UINT edge_hash_size, UINT vex_hash_size) : m_edgetab(self())
{
    m_edge_hash_size = edge_hash_size;
    m_vex_hash_size = vex_hash_size;
    m_ec_pool = nullptr;
    m_dense_vertex = nullptr; //default vertex layout is sparse.
    m_sparse_vertex = nullptr;
    init();
}


Graph::Graph(Graph const& g) : m_edgetab(self())
{
    m_edge_hash_size = g.m_edge_hash_size;
    m_vex_hash_size = g.m_vex_hash_size;
    m_ec_pool = nullptr;
    m_dense_vertex = nullptr; //default vertex layout is sparse.
    m_sparse_vertex = nullptr;
    init();
    clone(g, true, true);
}


void Graph::init()
{
    if (m_ec_pool != nullptr) { return; }

    m_ec_pool = smpoolCreate(sizeof(EdgeC), MEM_CONST_SIZE);
    ASSERTN(m_ec_pool, ("create mem pool failed"));

    m_vertex_pool = smpoolCreate(sizeof(Vertex) * 4, MEM_CONST_SIZE);
    ASSERTN(m_vertex_pool, ("create mem pool failed"));

    m_edge_pool = smpoolCreate(sizeof(Edge) * 4, MEM_CONST_SIZE);
    ASSERTN(m_edge_pool, ("create mem pool failed"));

    //If m_edges already initialized, this call will do nothing.
    m_edgetab.init(this);

    if (is_dense()) {
        ASSERT0(m_dense_vertex == nullptr);
        m_dense_vertex = new Vector<Vertex*>(m_vex_hash_size);
    } else {
        ASSERT0(m_sparse_vertex == nullptr);
        m_sparse_vertex = new VertexHash(m_vex_hash_size);
    }
    m_is_unique = true;
    m_is_direction = true;
    m_dense_vex_num = 0;
}


//Reconstruct vertex hash table, and edge hash table with new bucket size.
//'vertex_hash_sz': new vertex table size to be resized.
//'edge_hash_sz': new edge table size to be resized.
//NOTE: mem pool should have been initialized.
void Graph::resize(UINT vertex_hash_sz, UINT edge_hash_sz)
{
    ASSERT0(m_ec_pool);
    if (!is_dense()) {
        //Dense vertex vector does not need resize.
        ASSERTN(getVertexNum() == 0, ("graph is not empty"));
        if (m_vex_hash_size != vertex_hash_sz) {
            m_sparse_vertex->destroy();
            m_sparse_vertex->init(vertex_hash_sz);
            m_vex_hash_size = vertex_hash_sz;
        }
    }
}


size_t Graph::count_mem() const
{
    size_t count = sizeof(Graph);
    count -= sizeof(FreeList<Edge>); //has been counted in Graph.
    count -= sizeof(FreeList<EdgeC>); //has been counted in Graph.
    count -= sizeof(FreeList<Vertex>); //has been counted in Graph.
    count += m_edgetab.count_mem();
    count += m_e_free_list.count_mem();
    count += m_el_free_list.count_mem();
    count += m_v_free_list.count_mem();
    count += smpoolGetPoolSize(m_ec_pool);
    count += smpoolGetPoolSize(m_vertex_pool);
    count += smpoolGetPoolSize(m_edge_pool);
    if (is_dense()) {
        count += m_dense_vertex->count_mem();
    } else {
        count += m_sparse_vertex->count_mem();
    }
    return count;
}


void Graph::destroy()
{
    if (m_ec_pool == nullptr) { return; }
    m_edgetab.destroy();

    //Set if edge and vertex would not be redundantly.
    m_is_unique = false;
    m_is_direction = false; //Set if graph is direction.
    m_e_free_list.clean(); //edge free list
    m_el_free_list.clean(); //edge-list free list
    m_v_free_list.clean(); //vertex free list

    smpoolDelete(m_ec_pool);
    m_ec_pool = nullptr;

    smpoolDelete(m_vertex_pool);
    m_vertex_pool = nullptr;

    smpoolDelete(m_edge_pool);
    m_edge_pool = nullptr;

    if (is_dense()) {
        delete m_dense_vertex;
        m_dense_vertex = nullptr;
        m_dense_vex_num = 0;
    } else {
        delete m_sparse_vertex;
        m_sparse_vertex = nullptr;
    }
}


//Erasing graph, include all nodes and edges,
//except for EdgeInfo and VertexInfo.
void Graph::erase()
{
    ASSERTN(m_ec_pool != nullptr, ("Graph must be initialized before clone."));
    if (is_dense()) {
        m_dense_vertex->destroy();
        m_dense_vertex->init();
        m_dense_vex_num = 0;
    } else {
        m_sparse_vertex->destroy();
        //If m_sparse_vertex already initialized, this call will do nothing.
        m_sparse_vertex->init(m_vex_hash_size);
    }

    m_e_free_list.clean(); //edge free list
    m_el_free_list.clean(); //edge-list free list
    m_v_free_list.clean(); //vertex free list

    smpoolDelete(m_ec_pool);
    smpoolDelete(m_vertex_pool);
    smpoolDelete(m_edge_pool);
    m_ec_pool = smpoolCreate(sizeof(EdgeC), MEM_CONST_SIZE);
    ASSERTN(m_ec_pool, ("create mem pool failed"));
    m_vertex_pool = smpoolCreate(sizeof(Vertex) * 4, MEM_CONST_SIZE);
    ASSERTN(m_vertex_pool, ("create mem pool failed"));
    m_edge_pool = smpoolCreate(sizeof(Edge) * 4, MEM_CONST_SIZE);
    ASSERTN(m_edge_pool, ("create mem pool failed"));

    m_edgetab.destroy();
    m_edgetab.init(this);
}


//Sort vertice by RPO order, and update rpo of vertex.
//Record sorted vertex into vlst in incremental order of RPO.
//NOTE: rpo start at RPO_INIT_VAL.
void Graph::computeRPONoRecursive(Vertex * root, OUT RPOVexList & vlst) const
{
    ASSERT0(root && is_graph_entry(root));
    BitSet is_visited;
    Stack<Vertex*> stk;
    stk.push(root);
    Vertex * v;
    UINT order = RPO_INIT_VAL + getVertexNum() * RPO_INTERVAL;
    vlst.clean();
    while ((v = stk.get_top()) != nullptr) {
        is_visited.bunion(VERTEX_id(v));
        EdgeC * el = VERTEX_out_list(v);
        bool find = false; //find unvisited kid.
        while (el != nullptr) {
            Vertex * succ = el->getTo();
            if (!is_visited.is_contain(VERTEX_id(succ))) {
                stk.push(succ);
                find = true;
                break;
            }
            el = EC_next(el);
        }
        if (!find) {
            stk.pop();
            vlst.append_head(v);
            order -= RPO_INTERVAL;
            VERTEX_rpo(v) = order;
        }
    }

    //If order of BB is not zero, there must have some BBs should be
    //eliminated by CFG optimizations.
    ASSERTN(order == RPO_INIT_VAL, ("even having BB with no order assigned"));
}


bool Graph::clone(Graph const& src, bool clone_edge_info, bool clone_vex_info)
{
    erase();
    m_is_unique = src.m_is_unique;
    m_is_direction = src.m_is_direction;

    //Clone vertices
    VertexIter itv = VERTEX_UNDEF;
    for (Vertex * srcv = src.get_first_vertex(itv);
         srcv != nullptr; srcv = src.get_next_vertex(itv)) {
        Vertex * v = addVertex(VERTEX_id(srcv));

        //Calls inherited class method.
        //Vertex info of memory should allocated by inherited class method
        if (VERTEX_info(srcv) != nullptr && clone_vex_info) {
            VERTEX_info(v) = cloneVertexInfo(srcv);
        }
    }

    //Clone edges
    EdgeIter ite;
    for (Edge * srce = src.get_first_edge(ite);
         srce != nullptr; srce = src.get_next_edge(ite)) {
        Edge * e = addEdge(VERTEX_id(EDGE_from(srce)),
                           VERTEX_id(EDGE_to(srce)));

        //Calls inherited class method.
        //Edge info of memory should allocated by inherited class method
        if (EDGE_info(srce) != nullptr && clone_edge_info) {
            EDGE_info(e) = cloneEdgeInfo(srce);
        }
    }
    return true;
}


//NOTE: Do NOT use 0 as vertex id.
Vertex * Graph::newVertex(UINT vid)
{
    ASSERTN(m_vertex_pool, ("not yet initialized."));
    ASSERTN(vid != VERTEX_UNDEF, ("Use undefined vertex id"));
    Vertex * vex = m_v_free_list.get_free_elem();
    if (vex == nullptr) {
        vex = newVertex();
    }
    VERTEX_id(vex) = vid;
    return vex;
}


//Replace orginal predecessor with a list of new source vertex.
//Return the position of 'from' that is in the predecessor list of 'to'.
//newfrom: record a list of new source vertex id.
//from: original source vertex id
//to: original target vertex id
UINT Graph::replaceSource(UINT from, UINT to, List<UINT> const& newfrom)
{
    Edge * e = getEdge(from, to);
    ASSERTN(e, ("not found given edge"));
    Vertex * tp = getVertex(to);
    ASSERT0(tp);
    UINT from_pos = 0;
    removeEdge(e, nullptr, &from_pos);
    addEdgeAtPos(newfrom, tp, from_pos);
    return from_pos;
}


//Add edge from->to, whereas 'from' is the nth predecessor of 'to'.
//pos: the position of 'from' in predecessors of 'to', start at 0.
//     For any new edge, the default position is the last of in/out list,
//     thus the pos should not greater than the number of predecessors + 1.
//     e.g: there are 2 predecessors of 'to', pos can not greater than 2.
void Graph::addEdgeAtPos(List<UINT> const& fromlist, Vertex * to, UINT pos)
{
    bool swap = true;
    List<UINT>::Iter it;
    EdgeC * marker = nullptr;
    for (fromlist.get_head(&it); it != nullptr; fromlist.get_next(&it)) {
        UINT from = it->val();
        ASSERT0(from != VERTEX_UNDEF);
        Edge * newedge = addEdge(addVertex(from), to);
        if (!m_edgetab.getCompareKeyObject()->isNewElem()) {
            continue;
        }
        ASSERTN(m_edgetab.getCompareKeyObject()->getInEC()->getEdge() ==
                newedge, ("unmatch EdgeC"));
        if (marker == nullptr) {
            EdgeC * p = to->getInList();
            for (UINT i = 0; p != nullptr; i++, p = p->get_next()) {
                if (i == pos) {
                    marker = p;
                    break;
                }
            }
            if (marker->get_next() == nullptr) {
                //marker is the last position, namely all new element will be
                //append at the last of in-list.
                swap = false;
            }
        }

        //Note if marker is nullptr, the 'from' already be the source
        //vertex of 'to'.
        ASSERT0(marker);
        if (!swap) { continue; }

        //The insert position is in the middle of original source list.
        EdgeC * newedgec = m_edgetab.getCompareKeyObject()->getInEC();
        ASSERT0(newedgec);
        xcom::remove(&VERTEX_in_list(to), newedgec);
        xcom::insertbefore(&VERTEX_in_list(to), marker, newedgec);
    }
}


Edge * Graph::newEdge(Vertex * from, Vertex * to)
{
    ASSERTN(m_ec_pool != nullptr, ("not yet initialized."));
    if (from == nullptr || to == nullptr) return nullptr;
    if (m_is_unique) {
        Edge placeholder;
        EDGE_from(&placeholder) = from;
        EDGE_to(&placeholder) = to;
        if (m_is_direction) {
            m_edgetab.getCompareKeyObject()->clean();
            return m_edgetab.append(&placeholder);
        }

        Edge * e = m_edgetab.get(&placeholder);
        if (e != nullptr) {
            return e;
        }

        //Both check from->to and to->from
        EDGE_from(&placeholder) = to;
        EDGE_to(&placeholder) = from;
        m_edgetab.getCompareKeyObject()->clean();
        return m_edgetab.append(&placeholder);
    }
    EdgeC * inec;
    EdgeC * outec;
    m_edgetab.getCompareKeyObject()->clean();
    return m_edgetab.append(newEdgeImpl(from, to, &inec, &outec));
}


//Reverse edge direction
Edge * Graph::reverseEdge(Edge * e)
{
    ASSERTN(m_ec_pool != nullptr, ("not yet initialized."));
    ASSERTN(m_is_direction, ("graph is indirection"));
    void * einfo = EDGE_info(e);
    Vertex * from = EDGE_from(e);
    Vertex * to = EDGE_to(e);
    removeEdge(e);
    e = addEdge(VERTEX_id(to), VERTEX_id(from));
    EDGE_info(e) = einfo;
    return e;
}


//Reverse all edge direction
void Graph::reverseEdges()
{
    ASSERTN(m_ec_pool != nullptr, ("not yet initialized."));
    ASSERTN(m_is_direction, ("graph is indirection"));
    List<Edge*> list;
    Edge * e;
    EdgeIter c;
    for (e = get_first_edge(c); e != nullptr; e = get_next_edge(c)) {
        list.append_tail(e);
    }
    for (e = list.get_head(); e != nullptr; e = list.get_next()) {
        reverseEdge(e);
    }
}


//Insert 'newv' between 'v1' and 'v2'.
//e.g: given edge v1->v2, the result is v1->newv->v2.
//Return edge v1->newv, newv->v2.
void Graph::insertVertexBetween(IN Vertex * v1, IN Vertex * v2,
                                IN Vertex * newv, OUT Edge ** e1,
                                OUT Edge ** e2, bool sort)
{
    Edge * e = getEdge(v1, v2);

    EdgeC * v2pos_in_list = nullptr;
    EdgeC * v1pos_in_list = nullptr;
    if (sort) {
        for (EdgeC * ec = VERTEX_out_list(v1);
             ec != nullptr; ec = EC_next(ec)) {
            if (ec->getTo() == v2) {
                break;
            }
            v2pos_in_list = ec;
        }

        for (EdgeC * ec = VERTEX_in_list(v2); ec != nullptr; ec = EC_next(ec)) {
            if (ec->getFrom() == v1) {
                break;
            }
            v1pos_in_list = ec;
        }
    }

    ASSERTN(e, ("no edge in bewteen %d and %d", v1->id(), v2->id()));
    removeEdge(e);
    Edge * tmpe1 = addEdge(v1, newv);
    Edge * tmpe2 = addEdge(newv, v2);
    if (e1 != nullptr) { *e1 = tmpe1; }
    if (e2 != nullptr) { *e2 = tmpe2; }

    if (!sort) { return; }

    EdgeC * tmpe1_ec = nullptr;
    for (EdgeC * ec = VERTEX_out_list(v1); ec != nullptr; ec = EC_next(ec)) {
        if (EC_edge(ec) == tmpe1) {
            tmpe1_ec = ec;
            break;
        }
    }
    ASSERT0(tmpe1_ec);

    EdgeC * tmpe2_ec = nullptr;
    for (EdgeC * ec = VERTEX_in_list(v2); ec != nullptr; ec = EC_next(ec)) {
        if (EC_edge(ec) == tmpe2) {
            tmpe2_ec = ec;
            break;
        }
    }
    ASSERT0(tmpe2_ec);

    xcom::remove(&VERTEX_out_list(v1), tmpe1_ec);
    if (v2pos_in_list == nullptr) {
        xcom::append_head(&VERTEX_out_list(v1), tmpe1_ec);
    } else {
        xcom::insertafter_one(&v2pos_in_list, tmpe1_ec);
    }

    xcom::remove(&VERTEX_in_list(v2), tmpe2_ec);
    if (v1pos_in_list == nullptr) {
        xcom::append_head(&VERTEX_in_list(v2), tmpe2_ec);
    } else {
        xcom::insertafter_one(&v1pos_in_list, tmpe2_ec);
    }
}


//Insert 'newv' between 'v1' and 'v2'.
//e.g: given edge v1->v2, the result is v1->newv->v2.
//Return edge v1->newv, newv->v2.
//
//NOTICE: newv must be node in graph.
void Graph::insertVertexBetween(UINT v1, UINT v2, UINT newv, OUT Edge ** e1,
                                OUT Edge ** e2, bool sort)
{
    Vertex * pv1 = getVertex(v1);
    Vertex * pv2 = getVertex(v2);
    Vertex * pnewv = getVertex(newv);
    ASSERT0(pv1 && pv2 && pnewv);
    insertVertexBetween(pv1, pv2, pnewv, e1, e2, sort);
}


//pos_in_outlist: optional, record the position in outlist of 'from' of 'e'
//pos_in_inlist: optional, record the position in inlist of 'to' of 'e'
Edge * Graph::removeEdge(Edge * e, OUT UINT * pos_in_outlist,
                         OUT UINT * pos_in_inlist)
{
    ASSERTN(m_ec_pool != nullptr, ("not yet initialized."));
    if (e == nullptr) { return nullptr; }
    Vertex * from = EDGE_from(e);
    Vertex * to = EDGE_to(e);
    //remove out of out-list of 'from'
    EdgeC * el;
    UINT j = 0;
    for (el = from->getOutList(); el != nullptr; j++) {
        if (el->getEdge() == e) {
            if (pos_in_outlist != nullptr) {
                *pos_in_outlist = j;
            }
            break;
        }
        el = el->get_next();
    }
    ASSERTN(el, ("can not find out-edge, it is illegal graph"));
    xcom::remove(&VERTEX_out_list(from), el);
    m_el_free_list.add_free_elem(el);

    //remove out of in-list of 'to'
    UINT i = 0;
    for (el = VERTEX_in_list(to); el != nullptr; i++) {
        if (el->getEdge() == e) {
            if (pos_in_inlist != nullptr) {
                *pos_in_inlist = i;
            }
            break;
        }
        el = el->get_next();
    }
    ASSERTN(el, ("can not find in-edge, it is illegal graph"));
    xcom::remove(&VERTEX_in_list(to), el);
    m_el_free_list.add_free_elem(el);

    //remove edge out of edge-tab
    m_edgetab.remove(e);
    m_e_free_list.add_free_elem(e);
    return e;
}


Vertex * Graph::removeVertex(Vertex * vex)
{
    ASSERTN(m_ec_pool != nullptr, ("not yet initialized."));
    if (vex == nullptr) { return nullptr; }
    for (EdgeC * el = vex->getOutList(); el != nullptr;) {
        EdgeC * tmp = el;
        el = el->get_next();
        removeEdge(tmp->getEdge());
    }
    for (EdgeC * el = vex->getInList(); el != nullptr;) {
        EdgeC * tmp = el;
        el = el->get_next();
        removeEdge(tmp->getEdge());
    }
    if (is_dense()) {
        m_dense_vertex->set(vex->id(), nullptr);
        m_dense_vex_num--;
    } else {
        vex = m_sparse_vertex->remove(vex);
    }
    m_v_free_list.add_free_elem(vex);
    return vex;
}


//Return all neighbors of 'vid' on graph.
//Return false if 'vid' is not on graph.
//
//'ni_list': record the neighbours of 'vid'.
//    Note that this function ensure each neighbours in ni_list is unique.
bool Graph::getNeighborList(OUT List<UINT> & ni_list, UINT vid) const
{
    ASSERTN(m_ec_pool != nullptr, ("not yet initialized."));

    //Ensure VertexHash::find is readonly.
    Graph * pthis = const_cast<Graph*>(this);
    Vertex * vex  = pthis->getVertex(vid);
    if (vex == nullptr) { return false; }

    EdgeC * el = VERTEX_in_list(vex);
    while (el != nullptr) {
        UINT v = el->getFromId();
        if (!ni_list.find(v)) {
            ni_list.append_tail(v);
        }
        el = EC_next(el);
    }

    el = VERTEX_out_list(vex);
    while (el != nullptr) {
        UINT v = el->getToId();
        if (!ni_list.find(v)) {
            ni_list.append_tail(v);
        }
        el = EC_next(el);
    }
    return true;
}


//Return all neighbors of 'vid' on graph.
//Return false if 'vid' is not on graph.
//niset: record the neighbours of 'vid'.
//       Note that this function ensure each neighbours in niset is unique.
//       Using sparse bitset is faster than list in most cases.
bool Graph::getNeighborSet(OUT DefSBitSet & niset, UINT vid) const
{
    ASSERTN(m_ec_pool != nullptr, ("not yet initialized."));
    //Ensure VertexHash::find is readonly.
    Graph * pthis = const_cast<Graph*>(this);
    Vertex * vex  = pthis->getVertex(vid);
    if (vex == nullptr) { return false; }

    EdgeC * el = VERTEX_in_list(vex);
    while (el != nullptr) {
        UINT v = el->getFromId();
        niset.bunion(v);
        el = EC_next(el);
    }

    el = VERTEX_out_list(vex);
    while (el != nullptr) {
        niset.bunion(el->getToId());
        el = EC_next(el);
    }
    return true;
}


UINT Graph::getDegree(Vertex const* vex) const
{
    ASSERTN(m_ec_pool != nullptr, ("not yet initialized."));
    if (vex == nullptr) { return 0; }
    return vex->getInDegree() + vex->getOutDegree();
}


bool Graph::isInDegreeEqualTo(Vertex const* vex, UINT num) const
{
    ASSERTN(m_ec_pool != nullptr, ("not yet initialized."));
    if (vex == nullptr) { return 0; }
    UINT degree = 0;
    for (EdgeC * el = VERTEX_in_list(vex); el != nullptr; el = EC_next(el)) {
        degree++;
        if (degree == num) { return true; }
    }
    return degree == num; //Both degree and num may be 0.
}


bool Graph::isOutDegreeEqualTo(Vertex const* vex, UINT num) const
{
    ASSERTN(m_ec_pool != nullptr, ("not yet initialized."));
    if (vex == nullptr) { return 0; }
    UINT degree = 0;
    for (EdgeC * el = VERTEX_out_list(vex); el != nullptr; el = EC_next(el)) {
        degree++;
        if (degree == num) { return true; }
    }
    return degree == num; //Both degree and num may be 0.
}


Edge * Graph::getEdge(Vertex const* from, Vertex const* to) const
{
    ASSERTN(m_ec_pool != nullptr, ("not yet initialized."));
    if (from == nullptr || to == nullptr) { return nullptr; }

    EdgeC * el = VERTEX_out_list(from);
    while (el != nullptr) {
        Edge * e = EC_edge(el);
        if (EDGE_from(e) == from && EDGE_to(e) == to) {
            return e;
        }
        if (!m_is_direction &&
            (EDGE_from(e) == to && EDGE_to(e) == from)) {
            return e;
        }
        el = EC_next(el);
    }

    if (!m_is_direction) {
        EdgeC * el2 = VERTEX_out_list(to);
        while (el2 != nullptr) {
            Edge * e = EC_edge(el2);
            if (EDGE_from(e) == to && EDGE_to(e) == from) {
                return e;
            }
            el2 = EC_next(el2);
        }
    }
    return nullptr;
}


Edge * Graph::getEdge(UINT from, UINT to) const
{
    ASSERTN(m_ec_pool != nullptr, ("not yet initialized."));
    Vertex * fp = getVertex(from);
    Vertex * tp = getVertex(to);
    return getEdge(fp, tp);
}


bool Graph::is_equal(Graph & g) const
{
    if (getVertexNum() != g.getVertexNum() ||
        getEdgeNum() != g.getEdgeNum()) {
        return false;
    }

    BitSet vs;
    VertexIter c = VERTEX_UNDEF;
    for (Vertex * v1 = get_first_vertex(c);
         v1 != nullptr; v1 = get_next_vertex(c)) {
        Vertex * v2 = g.getVertex(VERTEX_id(v1));
        if (v2 == nullptr) {
            return false;
        }

        vs.clean();
        EdgeC * el = VERTEX_out_list(v1);
        Edge * e = nullptr;
        UINT v1_succ_n = 0;
        if (el == nullptr) {
            if (VERTEX_out_list(v2) != nullptr) {
                return false;
            }
            continue;
        }

        for (e = EC_edge(el); e != nullptr; el = EC_next(el),
             e = el ? EC_edge(el) : nullptr) {
            vs.bunion(VERTEX_id(EDGE_to(e)));
            v1_succ_n++;
        }

        UINT v2_succ_n = 0;
        el = VERTEX_out_list(v2);
        for (e = EC_edge(el); e != nullptr; el = EC_next(el),
             e = el ? EC_edge(el) : nullptr) {
            v2_succ_n++;
            if (!vs.is_contain(VERTEX_id(EDGE_to(e)))) {
                return false;
            }
        }

        if (v1_succ_n != v2_succ_n) {
            return false;
        }
    }
    return true;
}


bool Graph::is_livein_from(Vertex const* v, Vertex const* pred,
                           Vertex const* start) const
{
    class Visited {
    public:
        BitSet * m_bs;
        TTab<UINT> * m_tab;
    public:
        Visited(bool is_dense)
        {
            m_bs = nullptr;
            m_tab = nullptr;
            if (is_dense) { m_bs = new BitSet(); }
            else { m_tab = new TTab<UINT>(); }
        }
        ~Visited()
        {
            if (m_bs != nullptr) { delete m_bs; }
            if (m_tab != nullptr) { delete m_tab; }
        }
        void add(UINT v)
        {
            if (m_bs != nullptr) { m_bs->bunion(v); }
            else { m_tab->append(v); }
        }
        bool is_visited(UINT v) const
        {
            if (m_bs != nullptr) { return m_bs->is_contain(v); }
            else { return m_tab->find(v); }
        }
    };
    Visited visit(is_dense());
    ASSERT0(is_pred(v, pred));
    List<Vertex const*> wl;
    wl.append_tail(pred);
    visit.add(v->id());
    Vertex const* t = nullptr;
    while ((t = wl.remove_head()) != nullptr) {
        if (t == start) { return true; }
        visit.add(t->id());
        for (EdgeC const* ec = t->getInList(); ec != nullptr;
             ec = ec->get_next()) {
            Vertex const* from = ec->getFrom();
            if (!visit.is_visited(from->id())) {
                wl.append_tail(from);
            }
        }
    }
    return false;
}


//Sort graph vertices in topological order.
//vex_vec: record vertics in topological order.
//Return true if sorting success, otherwise there exist cycles in graph.
//Note you should NOT retrieve vertex in 'vex_vec' via vertex's index because
//they are stored in dense manner.
bool Graph::sortInTopologOrder(OUT Vector<Vertex*> & vex_vec)
{
    ASSERTN(m_ec_pool != nullptr, ("Graph still not yet initialize."));
    if (getVertexNum() == 0) {
        return true;
    }
    List<Vertex*> ready_list;
    DefMiscBitSetMgr sm;
    DefSBitSet is_removed(sm.getSegMgr());
    VertexIter c = VERTEX_UNDEF;
    for (xcom::Vertex * vex = get_first_vertex(c);
         vex != nullptr; vex = get_next_vertex(c)) {
        if (vex->getInDegree() == 0) {
            ready_list.append_tail(vex);
        }
    }
    vex_vec.set(getVertexNum() - 1, nullptr);
    UINT pos = 0;
    for (; ready_list.get_elem_count() != 0;) {
        Vertex * ready = ready_list.remove_head();
        is_removed.bunion(ready->id());
        vex_vec.set(pos, ready);
        pos++;
        for (xcom::EdgeC const* el = ready->getOutList();
             el != nullptr; el = el->get_next()) {
            Vertex * ready_succ = el->getTo();

            //Determine if in-degree is not equal to 0.
            UINT in_degree = 0;
            for (xcom::EdgeC const* el2 = ready_succ->getInList();
                 el2 != nullptr; el2 = el2->get_next()) {
                Vertex const* ready_succ_pred = el2->getFrom();
                if (is_removed.is_contain(ready_succ_pred->id())) {
                    continue;
                }
                in_degree++;
            }
            if (in_degree == 0) {
                ready_list.append_tail(ready_succ);
            }
        }
    }
    return pos == getVertexNum() - 1;
}


//Remove all edges between v1 and v2.
void Graph::removeEdgeBetween(Vertex * v1, Vertex * v2)
{
    EdgeC * ec = VERTEX_out_list(v1);
    while (ec != nullptr) {
        EdgeC * next = EC_next(ec);
        Edge * e = EC_edge(ec);
        if ((EDGE_from(e) == v1 && EDGE_to(e) == v2) ||
            (EDGE_from(e) == v2 && EDGE_to(e) == v1)) {
            removeEdge(e);
        }
        ec = next;
    }

    ec = VERTEX_in_list(v1);
    while (ec != nullptr) {
        EdgeC * next = EC_next(ec);
        Edge * e = EC_edge(ec);
        if ((EDGE_from(e) == v1 && EDGE_to(e) == v2) ||
            (EDGE_from(e) == v2 && EDGE_to(e) == v1)) {
            removeEdge(e);
        }
        ec = next;
    }
}


//Remove transitive edge.
//e.g: Given edges of G, there are v0->v1->v2->v3, v0->v3, then v0->v3 named
//transitive edge.
//ALGO:
//    INPUT: Graph with N vertices.
//    1. Sort vertices in topological order.
//    2. Associate each edges with indicator respective,
//       and recording them in one matrix(N*N)
//       e.g: e1:v0->v2, e2:v1->v2, e3:v0->v1
//              0   1    2
//            0 --  e3   e1
//            1 --  --   e2
//            2 --  --   --
//
//    3. Scan vertices according to toplogical order,
//       remove all edges which the target-node has been
//       marked at else rows.
//       e.g: There are dependence edges: v0->v1, v0->v2.
//       If v1->v2 has been marked, we said v0->v2 is removable,
//       and the same goes for the rest of edges.
//e.g: E->D, A->D are transitive edges.
//           E   A
//         __|   |_
//        |   \ /  |
//        |    V   |
//        |    B   |
//        |    |   |
//        |    |   |
//        |    V   |
//        |    C   |
//        |___ | __|
//            \|/
//             V
//             D
void Graph::removeTransitiveEdge()
{
    Vector<Vertex*> vex_vec;
    sortInTopologOrder(vex_vec);
    DefMiscBitSetMgr bs_mgr;
    Vector<DefSBitSetCore*> reachset_vec;
    TMap<UINT, DefSBitSetCore*> reachset_map;
    BitSet is_visited;
    //Scanning vertices in topological order.
    for (VecIdx i = 0; i <= vex_vec.get_last_idx(); i++) {
        Vertex const* fromvex = vex_vec.get(i);
        ASSERT0(fromvex);
        if (is_dense()) {
            removeTransitiveEdgeHelper(fromvex, &reachset_vec,
                                       is_visited, bs_mgr);
        } else {
            removeTransitiveEdgeHelper(fromvex,
                (Vector<DefSBitSetCore*>*)&reachset_map, is_visited, bs_mgr);
        }
    }

    if (is_dense()) {
        for (VecIdx i = 0; i <= reachset_vec.get_last_idx(); i++) {
            DefSBitSetCore * bs = reachset_vec.get(i);
            if (bs != nullptr) {
                bs->clean(bs_mgr);
            }
        }
        return;
    }

    TMapIter<UINT, DefSBitSetCore*> iter;
    DefSBitSetCore * bs = nullptr;
    for (reachset_map.get_first(iter, &bs);
         bs != nullptr; reachset_map.get_next(iter, &bs)) {
        bs->clean(bs_mgr);
    }
}


void Graph::removeTransitiveEdgeHelper(Vertex const* fromvex,
                                       Vector<DefSBitSetCore*> * reachset,
                                       BitSet & is_visited,
                                       DefMiscBitSetMgr & bs_mgr)
{
    ASSERT0(reachset);
    if (is_visited.is_contain(fromvex->id())) { return; }
    is_visited.bunion(fromvex->id());

    ASSERT0(fromvex);
    if (fromvex->getOutList() == nullptr) { return; }

    //Reachset defines the set of vertex that fromvex is able to reach.
    DefSBitSetCore * from_reachset = is_dense() ?
        reachset->get(fromvex->id()) :
        ((TMap<UINT, DefSBitSetCore*>*)reachset)->get(fromvex->id());
    if (from_reachset == nullptr) {
        from_reachset = bs_mgr.allocSBitSetCore();
        if (is_dense()) {
            reachset->set(fromvex->id(), from_reachset);
        } else {
            ((TMap<UINT, DefSBitSetCore*>*)reachset)->set(fromvex->id(),
                                                          from_reachset);
        }
    }

    //Position in bitset has been sorted in topological order.
    EdgeC const* next_ec = nullptr;
    for (EdgeC const* ec = fromvex->getOutList(); ec != nullptr; ec = next_ec) {
        next_ec = ec->get_next();
        Vertex const* tovex = ec->getTo();

        from_reachset->bunion(tovex->id(), bs_mgr);
        removeTransitiveEdgeHelper(tovex, reachset, is_visited, bs_mgr);

        //Reachset defines the set of vertex that tovex is able to reach.
        DefSBitSetCore * to_reachset = is_dense() ?
            reachset->get(tovex->id()) :
            ((TMap<UINT, DefSBitSetCore*>*)reachset)->get(tovex->id());
        if (to_reachset == nullptr) { continue; }
        from_reachset->bunion(*to_reachset, bs_mgr);

        //Get successor set correspond to to_dense.
        if (tovex->getOutList() == nullptr) { continue; }

        //Iterate other successors except 'to'.
        EdgeC const* next_ec2 = nullptr;
        for (EdgeC const* ec2 = fromvex->getOutList();
             ec2 != nullptr; ec2 = next_ec2) {
            next_ec2 = ec2->get_next();
            Vertex const* othervex = ec2->getTo();
            if (othervex == tovex || !to_reachset->is_contain(othervex->id())) {
                continue;
            }
            if (next_ec == ec2) {
                next_ec = ec->get_next();
            }
            removeEdge(ec2->getEdge());
        }
    }
}


void Graph::dumpVexVector(Vector<Vertex*> const& vec, FILE * h)
{
    if (h == nullptr) { return; }
    fprintf(h, "\n");
    for (VecIdx i = 0; i <= vec.get_last_idx(); i++) {
        Vertex const* x = vec.get(i);
        if (x != nullptr) {
            if (i != 0) { fprintf(h, ","); }
            fprintf(h, "V%d", x->id());
        }
    }
    fflush(h);
}


void Graph::dumpVertex(FILE * h, Vertex const* v) const
{
    fprintf(h, "\nnode%d [shape = Mrecord, label=\"{V%d}\"];",
            VERTEX_id(v), VERTEX_id(v));
}


void Graph::dumpEdge(FILE * h, Edge const* e) const
{
    fprintf(h, "\nnode%d->node%d", e->from()->id(), e->to()->id());
    fprintf(h, "[");
    bool prt_comma = false;
    if (!is_direction()) {
        if (prt_comma) { fprintf(h, ","); }
        fprintf(h, "dir=none");
        prt_comma = true;
    }
    {
        //Print label.
        if (prt_comma) { fprintf(h, ","); }
        fprintf(h, "label=\"%s\"", "");
        prt_comma = true;
    }
    fprintf(h, "]");
}


void Graph::dumpDOT(CHAR const* name) const
{
    if (name == nullptr) {
        name = "graph.dot";
    }
    UNLINK(name);
    FILE * h = fopen(name, "a+");
    ASSERTN(h, ("%s create failed!!!", name));

    fprintf(h, "digraph G {\n");
    //Print node
    VertexIter itv = VERTEX_UNDEF;
    for (Vertex const* v = get_first_vertex(itv);
         v != nullptr; v = get_next_vertex(itv)) {
        dumpVertex(h, v);
    }

    //Print edge
    EdgeIter ite;
    for (Edge const* e = get_first_edge(ite);
         e != nullptr; e = get_next_edge(ite)) {
        dumpEdge(h, e);
    }
    fprintf(h, "\n}\n");
    fclose(h);
}


void Graph::dumpVCG(CHAR const* name) const
{
    ASSERTN(m_ec_pool != nullptr, ("not yet initialized."));
    if (name == nullptr) {
        name = "graph.vcg";
    }
    UNLINK(name);
    FILE * h = fopen(name, "a+");
    ASSERTN(h, ("%s create failed!!!",name));
    fprintf(h, "graph: {"
              "title: \"Graph\"\n"
              "shrink:  15\n"
              "stretch: 27\n"
              "layout_downfactor: 1\n"
              "layout_upfactor: 1\n"
              "layout_nearfactor: 1\n"
              "layout_splinefactor: 70\n"
              "spreadlevel: 1\n"
              "treefactor: 0.500000\n"
              "node_alignment: center\n"
              "orientation: top_to_bottom\n"
              "late_edge_labels: no\n"
              "display_edge_labels: yes\n"
              "dirty_edge_labels: no\n"
              "finetuning: no\n"
              "nearedges: no\n"
              "splines: yes\n"
              "ignoresingles: no\n"
              "straight_phase: no\n"
              "priority_phase: no\n"
              "manhatten_edges: no\n"
              "smanhatten_edges: no\n"
              "port_sharing: no\n"
              "crossingphase2: yes\n"
              "crossingoptimization: yes\n"
              "crossingweight: bary\n"
              "arrow_mode: free\n"
              "layoutalgorithm: mindepthslow\n"
              "node.borderwidth: 2\n"
              "node.color: lightcyan\n"
              "node.textcolor: black\n"
              "node.bordercolor: blue\n"
              "edge.color: darkgreen\n");

    //Print node
    VertexIter itv = VERTEX_UNDEF;
    for (Vertex const* v = get_first_vertex(itv);
         v != nullptr; v = get_next_vertex(itv)) {
        fprintf(h, "\nnode: { title:\"%d\" label:\"%d\" "
                "shape:circle fontname:\"courB\" color:gold}",
                VERTEX_id(v), VERTEX_id(v));
    }

    //Print edge
    EdgeIter ite;
    for (Edge const* e = get_first_edge(ite);
         e != nullptr; e = get_next_edge(ite)) {
        fprintf(h, "\nedge: { sourcename:\"%d\" targetname:\"%d\" %s}",
                VERTEX_id(EDGE_from(e)),
                VERTEX_id(EDGE_to(e)),
                m_is_direction ? "" : "arrowstyle:none" );
    }
    fprintf(h, "\n}\n");
    fclose(h);
}


//Return true if find an order of RPO for 'v' that less than order of 'ref'.
bool Graph::tryFindLessRPO(Vertex * v, Vertex const* ref)
{
    ASSERT0(v && ref);
    INT rpo = ref->rpo() - 1;
    ASSERT0(rpo >= RPO_INIT_VAL);
    if (isUsableRPO(rpo)) {
        VERTEX_rpo(v) = rpo;
        return true;
    }
    return false;
}
//END Graph


//
//START DGraph
//
DGraph::DGraph(UINT edge_hash_size, UINT vex_hash_size) :
    Graph(edge_hash_size, vex_hash_size)
{
    set_dense(true);
    m_bs_mgr = nullptr;
}


DGraph::DGraph(DGraph const& g) : Graph(g)
{
    ASSERTN(g.is_dense(), ("Dominate Graph have to be dense graph"));
    m_bs_mgr = g.m_bs_mgr;
    if (m_bs_mgr != nullptr) {
        cloneDomAndPdom(g);
    }
}


bool DGraph::cloneDomAndPdom(DGraph const& src)
{
    ASSERT0(m_bs_mgr != nullptr);
    ASSERTN(src.is_dense(), ("Dominate Graph have to be dense graph"));
    VertexIter c = VERTEX_UNDEF;
    for (Vertex * srcv = src.get_first_vertex(c);
         srcv != nullptr; srcv = src.get_next_vertex(c)) {
        UINT src_vid = VERTEX_id(srcv);
        Vertex * tgtv = getVertex(src_vid);
        ASSERT0(tgtv != nullptr);

        DomSet const* set = src.read_dom_set(VERTEX_id(srcv));
        if (set != nullptr) {
            get_dom_set(tgtv)->copy(*set);
        }

        set = src.read_pdom_set(VERTEX_id(srcv));
        if (set != nullptr) {
            get_pdom_set(tgtv)->copy(*set);
        }
    }
    m_idom_set.copy(src.m_idom_set);
    m_ipdom_set.copy(src.m_ipdom_set);
    return true;
}


size_t DGraph::count_mem() const
{
    size_t count = m_dom_set.count_mem();
    count += m_pdom_set.count_mem(); //record post-dominator-set of each vertex.
    count += m_idom_set.count_mem(); //immediate dominator.
    count += m_ipdom_set.count_mem(); //immediate post dominator.
    count += sizeof(m_bs_mgr); //Do NOT count up the bitset in BS_MGR.
    return count;
}


//Vertices should have been sorted in topological order.
//And we access them by reverse-topological order.
//'vlst': compute dominator for vertices in vlst if it
//    is not empty or else compute all graph.
//'uni': universe.
bool DGraph::computeDom(List<Vertex const*> const* vlst, DomSet const* uni)
{
    List<Vertex const*> tmpvlst;
    List<Vertex const*> * pvlst = &tmpvlst;
    if (vlst != nullptr) {
        //Here one must guarantee pvlst would not be modified.
        pvlst = const_cast<List<Vertex const*>*>(vlst);
    } else {
        VertexIter c = VERTEX_UNDEF;
        for (Vertex const* u = get_first_vertex(c);
             u != nullptr; u = get_next_vertex(c)) {
            pvlst->append_tail(u);
        }
    }

    DomSet const* luni = nullptr;
    if (uni != nullptr) {
        luni = uni;
    } else {
        DomSet * x = new DomSet();
        C<Vertex const*> * ct = nullptr;
        for (pvlst->get_head(&ct);
             ct != pvlst->end(); ct = pvlst->get_next(ct)) {
            Vertex const* u = ct->val();
            ASSERT0(u);
            x->bunion(VERTEX_id(u));
        }
        luni = x;
    }

    //Initialize dom-set for each BB.
    C<Vertex const*> * ct;
    for (pvlst->get_head(&ct); ct != pvlst->end(); ct = pvlst->get_next(ct)) {
        Vertex const* v = ct->val();
        ASSERT0(v);
        if (is_graph_entry(v)) {
            DomSet * dom = get_dom_set(v);
            dom->clean();
            dom->bunion(v->id());
        } else {
            get_dom_set(v)->copy(*luni);
        }
    }

    //DOM[entry] = {entry}
    //DOM[n] = {n} �� { ��(DOM[pred] of predecessor of 'n') }
    bool change = true;
    DomSet tmp;
    UINT count = 0;
    while (change && count < 10) {
        count++;
        change = false;
        C<Vertex const*> * ct2;
        for (pvlst->get_head(&ct2); ct2 != pvlst->end();
             ct2 = pvlst->get_next(ct2)) {
            Vertex const* v = ct2->val();
            ASSERT0(v);
            UINT vid = VERTEX_id(v);
            if (is_graph_entry(v)) {
                continue;
            }

            //Access each preds
            EdgeC * ec = VERTEX_in_list(v);
            while (ec != nullptr) {
                Vertex * pred = ec->getFrom();
                if (ec == VERTEX_in_list(v)) {
                    tmp.copy(*m_dom_set.get(VERTEX_id(pred)));
                } else {
                    tmp.intersect(*m_dom_set.get(VERTEX_id(pred)));
                }
                ec = EC_next(ec);
            }
            tmp.bunion(vid);

            DomSet * dom = m_dom_set.get(VERTEX_id(v));
            if (!dom->is_equal(tmp)) {
                dom->copy(tmp);
                change = true;
            }
        } //end for
    } //end while
    ASSERT0(!change);

    if (uni == nullptr && luni != nullptr) {
        delete luni;
    }
    return true;
}


//Vertices should have been sorted in topological order.
//And we access them by reverse-topological order.
//'vlst': compute dominator for vertices in vlst if it
//    is not empty or else compute all graph.
//'uni': universe.
bool DGraph::computeDom3(List<Vertex const*> const* vlst, DomSet const* uni)
{
    DUMMYUSE(uni);
    List<Vertex const*> tmpvlst;
    List<Vertex const*> * pvlst = &tmpvlst;
    if (vlst != nullptr) {
        //Here one must guarantee pvlst would not be modified.
        pvlst = const_cast<List<Vertex const*>*>(vlst);
    } else {
        VertexIter c = VERTEX_UNDEF;
        for (Vertex const* u = get_first_vertex(c);
             u != nullptr; u = get_next_vertex(c)) {
            pvlst->append_tail(u);
        }
    }

    //Initialize dom-set for each BB.
    C<Vertex const*> * ct;
    for (pvlst->get_head(&ct); ct != pvlst->end(); ct = pvlst->get_next(ct)) {
        Vertex const* v = ct->val();
        ASSERT0(v);
        if (is_graph_entry(v)) {
            DomSet * dom = get_dom_set(v);
            dom->clean();
            dom->bunion(VERTEX_id(v));
        } else {
            get_dom_set(v)->clean();
        }
    }

    //DOM[entry] = {entry}
    //DOM[n] = {n} �� { ��(DOM[pred] of predecessor of 'n') }
    bool change = true;
    DomSet tmp;
    UINT count = 0;
    while (change && count < 10) {
        count++;
        change = false;
        C<Vertex const*> * ct2;
        for (pvlst->get_head(&ct2);
             ct2 != pvlst->end(); ct2 = pvlst->get_next(ct2)) {
            Vertex const* v = ct2->val();
            ASSERT0(v);
            UINT vid = VERTEX_id(v);
            if (is_graph_entry(v)) {
                continue;
            }

            //Access each preds
            EdgeC * ec = VERTEX_in_list(v);
            UINT meet = 0;
            while (ec != nullptr) {
                Vertex * pred = ec->getFrom();
                DomSet * ds = m_dom_set.get(VERTEX_id(pred));
                if (ds->is_empty()) {
                    ec = EC_next(ec);
                    continue;
                }

                if (meet == 0) {
                    tmp.copy(*ds);
                } else {
                    tmp.intersect(*ds);
                }
                meet++;
                ec = EC_next(ec);
            }

            if (meet == 0) { tmp.clean(); }
            tmp.bunion(vid);

            DomSet * dom = m_dom_set.get(VERTEX_id(v));
            if (!dom->is_equal(tmp)) {
                dom->copy(tmp);
                change = true;
            }
        } //end for
    } //end while
    ASSERT0(!change);
    return true;
}


//Compute post-dominator according to rpo.
//root: root node of graph.
//uni: universe.
//Note you should use this function carefully, it may be expensive, because that
//the function does not check if RPO is available, namely, it will always
//compute the RPO.
bool DGraph::computePdomByRPO(Vertex * root, DomSet const* uni)
{
    RPOVexList vlst;
    computeRPONoRecursive(root, vlst);
    vlst.reverse();

    bool res = false;
    if (uni == nullptr) {
        res = computePdom(vlst);
    } else {
        res = computePdom(vlst, uni);
    }
    CHECK0_DUMMYUSE(res);
    return true;
}


//Vertices should have been sorted in topological order.
//We access them by reverse-topological order.
//Note the graph may NOT have an exit vertex.
bool DGraph::computePdom(List<Vertex const*> const& vlst)
{
    DomSet uni;
    C<Vertex const*> * ct;
    for (vlst.get_head(&ct); ct != vlst.end(); ct = vlst.get_next(ct)) {
        Vertex const* u = ct->val();
        ASSERT0(u);
        uni.bunion(VERTEX_id(u));
    }
    return computePdom(vlst, &uni);
}


//Vertices should have been sorted in topological order.
//And we access them by reverse-topological order.
//vlst: vertex list.
//uni: universe.
//Note the graph may NOT have an exit vertex.
bool DGraph::computePdom(List<Vertex const*> const& vlst, DomSet const* uni)
{
    ASSERT0(uni);

    //Initialize pdom for each bb
    C<Vertex const*> * ct;
    bool find_exit = false;
    for (vlst.get_head(&ct); ct != vlst.end(); ct = vlst.get_next(ct)) {
        Vertex const* v = ct->val();
        ASSERT0(v);
        get_pdom_set(v)->clean();
        if (is_graph_exit(v)) {
            find_exit = true;
            break;
        }
    }
    if (!find_exit) { return true; }
    for (vlst.get_head(&ct); ct != vlst.end(); ct = vlst.get_next(ct)) {
        Vertex const* v = ct->val();
        ASSERT0(v);
        if (is_graph_exit(v)) {
            //Note the graph may NOT have an exit vertex.
            DomSet * pdom = get_pdom_set(v);
            pdom->clean();
            pdom->bunion(VERTEX_id(v));
            find_exit = true;
        } else {
            get_pdom_set(v)->copy(*uni);
        }
    }

    //PDOM[exit] = {exit}
    //PDOM[n] = {n} U {��(PDOM[succ] of each succ of n)}
    bool change = true;
    DomSet tmp;
    UINT count = 0;
    while (change && count < 10) {
        count++;
        change = false;
        C<Vertex const*> * ct2;
        for (vlst.get_head(&ct2); ct2 != vlst.end();
             ct2 = vlst.get_next(ct2)) {
            Vertex const* v = ct2->val();
            ASSERT0(v);
            UINT vid = VERTEX_id(v);
            if (is_graph_exit(v)) {
                //Note the graph may NOT have an exit vertex.
                continue;
            }

            tmp.clean();
            //Access each succs
            EdgeC * ec = VERTEX_out_list(v);
            while (ec != nullptr) {
                Vertex * succ = ec->getTo();
                if (ec == VERTEX_out_list(v)) {
                    tmp.copy(*m_pdom_set.get(VERTEX_id(succ)));
                } else {
                    tmp.intersect(*m_pdom_set.get(VERTEX_id(succ)));
                }
                ec = EC_next(ec);
            }
            tmp.bunion(vid);

            DomSet * pdom = m_pdom_set.get(VERTEX_id(v));
            if (!pdom->is_equal(tmp)) {
                pdom->copy(tmp);
                change = true;
            }
        } //end for
    } // end while

    ASSERT0(!change);
    return true;
}


//This function need idom to be avaiable.
//NOTE: set does NOT include node itself.
bool DGraph::computeDom2(List<Vertex const*> const& vlst)
{
    C<Vertex const*> * ct;
    DomSet avail;
    for (vlst.get_head(&ct); ct != vlst.end(); ct = vlst.get_next(ct)) {
        Vertex const* v = ct->val();
        ASSERT0(v);
        DomSet * doms = get_dom_set(VERTEX_id(v));
        doms->clean();
        ASSERT0(doms);
        for (UINT idom = get_idom(VERTEX_id(v));
             idom != VERTEX_UNDEF; idom = get_idom(idom)) {
            if (avail.is_contain(idom)) {
                DomSet const* idom_doms = get_dom_set(idom);
                doms->copy(*idom_doms);
                doms->bunion(idom);
                break;
            }
            doms->bunion(idom);
        }
        avail.bunion(VERTEX_id(v));
    }
    return true;
}


//Vertices should have been sorted in rpo.
//'vlst': a list of vertex which sort in rpo order.
//
//NOTE:
//    1. The root node has better to be the first one in 'vlst'.
//    2. Do not use '0' as vertex id, it is used as Undefined.
//    3. Entry does not have idom.
bool DGraph::computeIdom2(List<Vertex const*> const& vlst)
{
    bool change = true;

    //Initialize idom-set for each BB.
    m_idom_set.clean();
    UINT nentry = 0;
    while (change) {
        change = false;
        //Access with topological order.
        C<Vertex const*> * ct;
        for (vlst.get_head(&ct); ct != vlst.end(); ct = vlst.get_next(ct)) {
            Vertex const* v = ct->val();
            ASSERT0(v);
            if (is_graph_entry(v)) {
                m_idom_set.set(VERTEX_id(v), (INT)VERTEX_id(v));
                nentry++;
                continue;
            }

            //Access each preds
            EdgeC const* ec = VERTEX_in_list(v);
            Vertex const* idom = nullptr;
            while (ec != nullptr) {
                Vertex const* pred = ec->getFrom();
                UINT pid = VERTEX_id(pred);

                if (m_idom_set.get(pid) == VERTEX_UNDEF) {
                    ec = EC_next(ec);
                    continue;
                }

                if (idom == nullptr) {
                    idom = pred;
                    ec = EC_next(ec);
                    continue;
                }

                Vertex const* j = pred;
                Vertex const* k = idom;
                while (j != k) {
                    while (VERTEX_rpo(j) > VERTEX_rpo(k)) {
                        j = getVertex((UINT)m_idom_set.get(VERTEX_id(j)));
                        ASSERT0(j);
                        if (is_graph_entry(j)) {
                            break;
                        }
                    }
                    while (VERTEX_rpo(j) < VERTEX_rpo(k)) {
                        k = getVertex((UINT)m_idom_set.get(VERTEX_id(k)));
                        ASSERT0(k);
                        if (is_graph_entry(k)) {
                            break;
                        }
                    }
                    if (is_graph_entry(j) && is_graph_entry(k)) {
                        break;
                    }
                 }

                if (j != k) {
                    //Multi entries.
                    ASSERT0(is_graph_entry(j) && is_graph_entry(k));
                    idom = nullptr;
                    break;
                }

                idom = j;
                ec = EC_next(ec);
            }

            if (idom == nullptr) {
                if (m_idom_set.get(v->id()) != VERTEX_UNDEF) {
                    m_idom_set.set(v->id(), VERTEX_UNDEF);
                    change = true;
                }
            } else if ((UINT)m_idom_set.get(v->id()) != idom->id()) {
                m_idom_set.set(v->id(), (INT)idom->id());
                change = true;
            }
        } //end for
    }

    C<Vertex const*> * ct;
    for (vlst.get_head(&ct); ct != vlst.end(); ct = vlst.get_next(ct)) {
        Vertex const* v = ct->val();
        ASSERT0(v);
        if (is_graph_entry(v)) {
            m_idom_set.set(v->id(), VERTEX_UNDEF);
            nentry--;
            if (nentry == 0) { break; }
        }
    }
    return true;
}


bool DGraph::verifyPdom(DGraph & g,
                        List<Vertex const*> const& rpovlst) const
{
    List<xcom::Vertex const*> vlst;
    C<Vertex const*> * ct;
    for (Vertex const* v = rpovlst.get_tail(&ct); v != nullptr;
         v = rpovlst.get_prev(&ct)) {
        ASSERT0(v->id() != VERTEX_UNDEF);
        vlst.append_tail(v);
    }
    bool f1 = g.computePdom(vlst);
    CHECK0_DUMMYUSE(f1);
    bool f2 = g.computeIpdom();
    CHECK0_DUMMYUSE(f2);
    for (VecIdx i = 0; i <= m_idom_set.get_last_idx(); i++) {
        UINT cur = m_ipdom_set.get(i);
        UINT anti = g.m_ipdom_set.get(i);
        ASSERTN(cur == anti, ("unmatch ipdom"));
    }
    return true;
}


bool DGraph::verifyDom(DGraph & g,
                       List<Vertex const*> const& rpovlst) const
{
    g.computeIdom2(rpovlst);
    for (VecIdx i = 0; i <= m_idom_set.get_last_idx(); i++) {
        UINT cur = m_idom_set.get(i);
        UINT anti = g.m_idom_set.get(i);
        ASSERTN(cur == anti, ("unmatch idom"));
    }
    return true;
}


bool DGraph::verifyDom() const
{
    DGraph g;
    g.clone(*this, false, false);
    Vertex * entry = nullptr;
    VertexIter it = VERTEX_UNDEF;
    for (Vertex * v = g.get_first_vertex(it); v != nullptr;
         v = g.get_next_vertex(it)) {
        if (g.is_graph_entry(v)) {
            entry = v;
            break;
        }
    }
    if (entry == nullptr) { return true; }
    RPOVexList rpovlst;
    g.computeRPONoRecursive(entry, rpovlst);
    verifyDom(g, rpovlst);
    return true;
}


bool DGraph::verifyPdom() const
{
    DGraph g;
    g.clone(*this, false, false);
    Vertex * entry = nullptr;
    VertexIter it = VERTEX_UNDEF;
    for (Vertex * v = g.get_first_vertex(it); v != nullptr;
         v = g.get_next_vertex(it)) {
        if (g.is_graph_entry(v)) {
            entry = v;
            break;
        }
    }
    if (entry == nullptr) { return true; }
    RPOVexList rpovlst;
    g.computeRPONoRecursive(entry, rpovlst);
    verifyPdom(g, rpovlst);
    return true;
}


bool DGraph::verifyDomAndPdom() const
{
    DGraph g;
    g.clone(*this, false, false);
    Vertex * entry = nullptr;
    VertexIter it = VERTEX_UNDEF;
    for (Vertex * v = g.get_first_vertex(it); v != nullptr;
         v = g.get_next_vertex(it)) {
        if (g.is_graph_entry(v)) {
            entry = v;
            break;
        }
    }
    if (entry == nullptr) { return true; }
    RPOVexList rpovlst;
    g.computeRPONoRecursive(entry, rpovlst);
    verifyDom(g, rpovlst);
    verifyPdom(g, rpovlst);
    return true;
}


//NOTE: Entry does not have idom.
bool DGraph::computeIdom()
{
    //Initialize idom-set for each BB.
    m_idom_set.clean();

    //Access with topological order.
    VertexIter c = VERTEX_UNDEF;
    for (Vertex * v = get_first_vertex(c);
         v != nullptr; v = get_next_vertex(c)) {
        UINT cur_id = VERTEX_id(v);
        if (is_graph_entry(v)) {
            continue;
        }
        if (m_dom_set.get(cur_id)->get_elem_count() >= 2) {
            BitSet * p = m_dom_set.get(cur_id);
            ASSERTN(p != nullptr, ("should compute dom first"));
            if (p->get_elem_count() == 1) {
                //There is no idom if 'dom' set only contain itself.
                ASSERT0(m_idom_set.get(cur_id) == VERTEX_UNDEF);
                continue;
            }
            p->diff(cur_id);

            #ifdef MAGIC_METHOD
            BSIdx i;
            for (i = p->get_first(); i != BS_UNDEF; i = p->get_next((UINT)i)) {
                if (m_dom_set.get((UINT)i)->is_equal(*p)) {
                    ASSERT0(m_idom_set.get(cur_id) == VERTEX_UNDEF);
                    m_idom_set.set(cur_id, i);
                    break;
                }
            }
            ASSERTN(i != BS_UNDEF, ("not find idom?"));
            #else
            BSIdx i;
            for (i = tmp.get_first(); i != BS_UNDEF; i = tmp.get_next(i)) {
                for (BSIdx j = tmp.get_first(); j != BS_UNDEF;
                     j = tmp.get_next(j)) {
                    if (i == j) {
                        continue;
                    }
                    if (m_dom_set.get(j)->is_contain(i)) {
                        tmp.diff(i);
                        //Search 'tmp' over again.
                        i = tmp.get_first();
                        j = i;
                    }
                }
            }
            i = tmp.get_first();
            ASSERTN(i != BS_UNDEF, ("cannot find idom of BB:%d", cur_id));
            ASSERTN(m_idom_set.get(cur_id) == VERTEX_UNDEF,
                    ("recompute idom for BB:%d", cur_id));
            m_idom_set.set(cur_id, i);
            #endif
            p->bunion(cur_id);
        } //end else if
    } //end for
    return true;
}


Vertex * Graph::get_last_vertex(VertexIter & cur) const
{
    ASSERTN(m_ec_pool != nullptr, ("not yet initialized."));
    if (is_dense()) {
        for (VecIdx i = m_dense_vertex->get_last_idx(); !IS_VECUNDEF(i); i--) {
            Vertex * vex = m_dense_vertex->get(i);
            if (vex != nullptr) {
                cur = i;
                return vex;
            }
        }
        return nullptr;
    }
    return m_sparse_vertex->get_last(cur);
}


Vertex * Graph::get_prev_vertex(VertexIter & cur) const
{
    ASSERTN(m_ec_pool != nullptr, ("not yet initialized."));
    if (is_dense()) {
        for (VecIdx i = cur - 1; !IS_VECUNDEF(i); i--) {
            Vertex * vex = m_dense_vertex->get(i);
            if (vex != nullptr) {
                cur = i;
                return vex;
            }
        }
        return nullptr;
    }
    return m_sparse_vertex->get_prev(cur);
}


Vertex * Graph::get_first_vertex(VertexIter & cur) const
{
    ASSERTN(m_ec_pool != nullptr, ("not yet initialized."));
    if (is_dense()) {
        for (VecIdx i = VERTEX_UNDEF + 1;
             i <= m_dense_vertex->get_last_idx(); i++) {
            Vertex * vex = m_dense_vertex->get(i);
            if (vex != nullptr) {
                cur = i;
                return vex;
            }
        }
        return nullptr;
    }
    return m_sparse_vertex->get_first(cur);
}


Vertex * Graph::get_next_vertex(VertexIter & cur) const
{
    ASSERTN(m_ec_pool != nullptr, ("not yet initialized."));
    if (is_dense()) {
        for (VecIdx i = cur + 1; i <= m_dense_vertex->get_last_idx(); i++) {
            Vertex * vex = m_dense_vertex->get(i);
            if (vex != nullptr) {
                cur = i;
                return vex;
            }
        }
        return nullptr;
    }
    return m_sparse_vertex->get_next(cur);
}


//NOTE: graph exit vertex does not have ipdom.
bool DGraph::computeIpdom()
{
    //Initialize ipdom-set for each BB.
    m_ipdom_set.clean();

    //Processing in reverse-topological order.
    VertexIter c = VERTEX_UNDEF;
    for (Vertex const* v = get_last_vertex(c);
         v != nullptr; v = get_prev_vertex(c)) {
        UINT cur_id = VERTEX_id(v);
        if (is_graph_exit(v) ||
            m_pdom_set.get(cur_id)->get_elem_count() <= 1) {
            continue;
        }

        BitSet * p = m_pdom_set.get(cur_id);
        ASSERTN(p != nullptr, ("should compute pdom first"));
        if (p->get_elem_count() == 1) {
            //There is no ipdom if 'pdom' set only contain itself.
            ASSERT0(m_ipdom_set.get(cur_id) == VERTEX_UNDEF);
            continue;
        }

        p->diff(cur_id);
        BSIdx i;
        for (i = p->get_first(); i != BS_UNDEF; i = p->get_next(i)) {
            if (m_pdom_set.get(i)->is_equal(*p)) {
                ASSERT0(m_ipdom_set.get(cur_id) == VERTEX_UNDEF);
                m_ipdom_set.set(cur_id, i);
                break;
            }
        }
        //ASSERTN(i != BS_UNDEF, ("not find ipdom")); //Not find.
        p->bunion(cur_id);
    }
    return true;
}


//dt: generate dominator tree and record in it.
void DGraph::genDomTree(OUT DomTree & dt) const
{
    dt.set_dense(is_dense());
    VertexIter c = VERTEX_UNDEF;
    for (Vertex * v = get_first_vertex(c);
         v != nullptr; v = get_next_vertex(c)) {
        UINT vid = VERTEX_id(v);
        dt.addVertex(vid);
        if (m_idom_set.get(vid) != VERTEX_UNDEF) {
            dt.addEdge((UINT)m_idom_set.get(vid), vid);
        }
    }
}


//pdom: generate post-dominator tree and record in it.
void DGraph::genPDomTree(OUT DomTree & pdt) const
{
    VertexIter c = VERTEX_UNDEF;
    pdt.set_dense(is_dense());
    for (Vertex * v = get_first_vertex(c);
         v != nullptr; v = get_next_vertex(c)) {
        UINT vid = v->id();
        pdt.addVertex(vid);
        if (m_ipdom_set.get(vid) != VERTEX_UNDEF) { //id of bb starting at 1.
            pdt.addEdge((UINT)m_ipdom_set.get(vid), vid);
        }
    }
}


void DGraph::dumpDom(CHAR const* name, bool dump_dom_tree) const
{
    if (name == nullptr) {
        name = "graph_dom.txt";
    }
    UNLINK(name);
    FILE * h = fopen(name, "a+");
    ASSERTN(h, ("%s create failed!!!",name));
    dumpDom(h, dump_dom_tree);
    fclose(h);
}


//Dump dom set, pdom set, idom, ipdom.
//'dump_dom_tree': set to be true to dump dominate
//  tree, and post dominate Tree.
void DGraph::dumpDom(FILE * h, bool dump_dom_tree) const
{
    if (h == nullptr) { return; }
    fprintf(h, "\n==---- DUMP DOM/PDOM/IDOM/IPDOM ----==");
    VertexIter c = VERTEX_UNDEF;
    for (Vertex * v = get_first_vertex(c);
         v != nullptr; v = get_next_vertex(c)) {
        UINT vid = VERTEX_id(v);
        BitSet * bs;
        fprintf(h, "\nVERTEX(%d)", vid);
        fprintf(h, "\n  domset:");
        if ((bs = m_dom_set.get(vid)) != nullptr) {
            for (BSIdx id = bs->get_first();
                 id != BS_UNDEF ; id = bs->get_next((UINT)id)) {
                if ((UINT)id != vid) {
                    fprintf(h, "%u ", id);
                }
            }
        }

        fprintf(h, "\n  pdomset:");
        if ((bs = m_pdom_set.get(vid)) != nullptr) {
            for (BSIdx id = bs->get_first();
                 id != BS_UNDEF; id = bs->get_next((UINT)id)) {
                if ((UINT)id != vid) {
                    fprintf(h, "%u ", id);
                }
            }
        }

        if (m_idom_set.get(vid) != VERTEX_UNDEF) {
            fprintf(h, "\n  idom:%u", m_idom_set.get(vid));
        } else {
            fprintf(h, "\n");
        }

        if (m_ipdom_set.get(vid) != VERTEX_UNDEF) {
            fprintf(h, "\n  ipdom:%u", m_ipdom_set.get(vid));
        } else {
            fprintf(h, "\n");
        }
    }
    fprintf(h, "\n");
    fflush(h);
    if (dump_dom_tree) {
        DomTree dom;
        genDomTree(dom);
        dom.dumpDOT("graph_dom_tree.dot");
        dom.erase();
        genPDomTree(dom);
        dom.dumpDOT("graph_pdom_tree.dot");
    }
}


//Sort node in dominator-tree in preorder.
void DGraph::sortDomTreeInPreorder(IN Vertex * root, OUT List<Vertex*> & lst)
{
    ASSERT0(root);
    BitSet is_visited;
    is_visited.bunion(VERTEX_id(root));
    lst.append_tail(root);

    Vertex * v;
    Stack<Vertex*> stk;
    stk.push(root);
    while ((v = stk.pop()) != nullptr) {
        if (!is_visited.is_contain(VERTEX_id(v))) {
            is_visited.bunion(VERTEX_id(v));
            stk.push(v);
            //The only place to process vertex.
            lst.append_tail(v);
        }

        //Visit children.
        EdgeC * el = VERTEX_out_list(v);
        Vertex * succ;
        while (el != nullptr) {
            succ = el->getTo();
            if (!is_visited.is_contain(VERTEX_id(succ))) {
                stk.push(v);
                stk.push(succ);
                break;
            }
            el = EC_next(el);
        }
    }
}


//Sort node on graph in bfs-order.
//'order_buf': record the bfs-order for each vertex.
//NOTE: BFS does NOT keep the sequence if you are going to
//access vertex in lexicographic order.
void DGraph::sortInBfsOrder(Vector<UINT> & order_buf, Vertex * root,
                            BitSet & visit)
{
    List<Vertex*> worklst;
    worklst.append_tail(root);
    UINT order = 1;
    while (worklst.get_elem_count() > 0) {
        Vertex * sv = worklst.remove_head();
        order_buf.set(VERTEX_id(sv), order);
        order++;
        visit.bunion(VERTEX_id(sv));
        EdgeC * el = VERTEX_out_list(sv);
        while (el != nullptr) {
            Vertex * to = el->getTo();
            if (visit.is_contain(VERTEX_id(to))) {
                el = EC_next(el);
                continue;
            }
            worklst.append_tail(to);
            el = EC_next(el);
        }
    }
}


//Sort in-edge of vex in given order.
//order: record the given order of each predecessor. Note the number
//       of elements have to equal to the number of predecessor of vex.
void DGraph::sortPred(MOD Vertex * vex, Vector<UINT> const& order)
{
    UINT pos = 0;
    EdgeC * next_ec = nullptr;
    for (EdgeC * ec = vex->getInList(); ec != nullptr; ec = next_ec, pos++) {
        next_ec = ec->get_next();
        ASSERT0(ec->getToId() == vex->id());
        UINT anti_pred = order.get(pos);
        if (ec->getFromId() == anti_pred) { continue; }

        //Find the anticipated pred.
        EdgeC * ec2 = next_ec;
        for (; ec2 != nullptr; ec2 = ec2->get_next()) {
            if (ec2->getFromId() == anti_pred) {
                break;
            }
        }
        ASSERTN(ec2, ("not found anti-pred"));
        xcom::swap(&VERTEX_in_list(vex), ec, ec2);
    }
}


void DGraph::sortDomTreeInPostrder(IN Vertex * root, OUT List<Vertex*> & lst)
{
    ASSERT0(root);
    BitSet is_visited;

    //Find the leaf node.
    Vertex * v;
    Stack<Vertex*> stk;
    stk.push(root);
    while ((v = stk.pop()) != nullptr) {
        //Visit children first.
        EdgeC * el = VERTEX_out_list(v);
        bool find = false; //find unvisited kid.
        Vertex * succ;
        while (el != nullptr) {
            succ = el->getTo();
            if (!is_visited.is_contain(VERTEX_id(succ))) {
                stk.push(v);
                stk.push(succ);
                find = true;
                break;
            }
            el = EC_next(el);
        }
        if (!find) {
            is_visited.bunion(VERTEX_id(v));
            //The only place to process vertex.
            lst.append_tail(v);
        }
    }
}


void DGraph::_removeUnreachNode(UINT id, BitSet & visited)
{
    visited.bunion(id);
    Vertex * vex = getVertex(id);
    EdgeC * el = VERTEX_out_list(vex);
    while (el != nullptr) {
        UINT succ = el->getToId();
        if (!visited.is_contain(succ)) {
            _removeUnreachNode(succ, visited);
        }
        el = EC_next(el);
    }
}


void DGraph::freeDomPdomSet(UINT vid)
{
    DomSet * domset = get_dom_set(vid);
    if (domset != nullptr) {
        m_bs_mgr->free(domset);
        m_dom_set.set(vid, nullptr);
    }
    DomSet * pdomset = get_pdom_set(vid);
    if (pdomset != nullptr) {
        m_bs_mgr->free(pdomset);
        m_pdom_set.set(vid, nullptr);
    }
}


//Add vertex to domset and pdomset for both livein and liveout paths.
static void removeVexFromDomAndPdomSet(DGraph * g, Vertex const* marker,
                                       UINT vexid)
{
    DomSet visited;
    List<Vertex const*> wl;
    wl.append_tail(marker);
    for (Vertex const* v = wl.remove_head(); v != nullptr;
         v = wl.remove_head()) {
        visited.bunion(v->id());
        if (v->id() != vexid) {
            DomSet * domset = g->get_dom_set(v->id());
            domset->diff(vexid);
            DomSet * pdomset = g->get_pdom_set(v->id());
            pdomset->diff(vexid);
        }
        for (EdgeC const* ec = v->getInList(); ec != nullptr;
             ec = ec->get_next()) {
            Vertex const* v = ec->getFrom();
            if (!visited.is_contain(v->id())) {
                visited.bunion(v->id());
                wl.append_tail(v);
            }
        }
    }

    wl.append_tail(marker);
    for (Vertex const* v = wl.remove_head(); v != nullptr;
         v = wl.remove_head()) {
        visited.bunion(v->id());
        if (v->id() != vexid) {
            DomSet * domset = g->get_dom_set(v->id());
            domset->diff(vexid);
            DomSet * pdomset = g->get_pdom_set(v->id());
            pdomset->diff(vexid);
        }
        for (EdgeC const* ec = v->getOutList(); ec != nullptr;
             ec = ec->get_next()) {
            Vertex const* s = ec->getTo();
            if (!visited.is_contain(s->id())) {
                visited.bunion(s->id());
                wl.append_tail(s);
            }
        }
    }
}


//The function try to change the DOM info when given vertex has been bypassed.
//vex: the vertex id that will be bypassed.
//e.g:
//  pred->vex->succ
//  where vex's idom is pred, vex's ipdom is succ, succ's idom is vex.
//after bypassing,
//  pred->vex->succ
//   \         ^
//    \_______/
//  succ's dom become pred.
bool DGraph::changeDomInfoByAddBypassEdge(UINT vex)
{
    UINT pred = get_idom(vex);
    UINT succ = get_ipdom(vex);
    UINT ipdom_pred = get_ipdom(pred);
    if (ipdom_pred != VERTEX_UNDEF && ipdom_pred != vex) { return false; }
    UINT idom_succ = get_idom(succ);
    if (idom_succ != VERTEX_UNDEF && idom_succ != vex) { return false; }
    if (ipdom_pred != VERTEX_UNDEF) {
        //PDOM may not yet be computed by PassMgr.
        set_ipdom(pred, succ);
    }
    if (idom_succ != VERTEX_UNDEF) {
        //DOM may not yet be computed by PassMgr.
        set_idom(succ, pred);
    }
    removeVexFromDomAndPdomSet(this, getVertex(vex), vex);
    return true;
}


//Add vertex newid to domset and pdomset for both livein and liveout paths
//of marker.
//is_dom: true if newid is new dom of marker.
//CASE:
//    Ventry
//  ___| |__
// |        |
// v        v
// V5       V6
// |        |
// ----   ---
//     || ___________
//     vvv           |
//     V3            |
//      | \         V8
//      |  \________^
//      v
//    Vexit
//after adding IDom V13 of V3
//    Ventry
//  ___| |__
// |        |
// v        v
// V5       V6
// |        |
// ----  ---
//     ||
//     vv
//     V13
//      |
//      |
//      | ___________
//      vv           |
//     V3            |
//      | \         V8
//      |  \________^
//      v
//    Vexit
//CASE2:
//          Ventry
//           |
//           v
//    -----> V3
//   |      / |
//   |     v  |
//    --- V8  |
//          \ |
//           vv
//           Vexit
//after adding IPDom V13 of V3
//          Ventry
//           |
//           v
//    -----> V3
//   |       |
//   |       v
//   |       V13
//   |      / |
//   |     v  |
//    --- V8  |
//          \ |
//           vv
//           Vexit
static void addVexToDomAndPdomSet(DGraph * g, Vertex const* vex, UINT newid,
                                  bool is_dom)
{
    UINT vexid = vex->id();
    DomSet visited;
    List<Vertex const*> wl;
    wl.append_tail(vex);
    for (Vertex const* v = wl.remove_head(); v != nullptr;
         v = wl.remove_head()) {
        visited.bunion(v->id());
        if (v->id() != vexid && v->id() != newid) {
            DomSet * domset = g->get_dom_set(v->id());
            DomSet * pdomset = g->get_pdom_set(v->id());
            if (is_dom) {
                if (domset->is_contain(vexid)) {
                    domset->bunion(newid);
                } else if (g->get_ipdom(v->id()) == vexid) {
                    g->set_ipdom(v->id(), newid);
                    pdomset->bunion(newid);
                }
            } else {
                if (pdomset->is_contain(vexid)) {
                    pdomset->bunion(newid);
                } else if (g->get_idom(v->id()) == vexid) {
                    g->set_idom(v->id(), newid);
                    domset->bunion(newid);
                }
            }
        }
        for (EdgeC const* ec = v->getInList(); ec != nullptr;
             ec = ec->get_next()) {
            Vertex const* v = ec->getFrom();
            if (!visited.is_contain(v->id())) {
                visited.bunion(v->id());
                wl.append_tail(v);
            }
        }
    }

    //If the succ of vex is also the pred, then it has been mark visited.
    //Unmark the succ in order to handle more subsequent successors.
    for (EdgeC const* ec = vex->getInList(); ec != nullptr;
         ec = ec->get_next()) {
        Vertex const* v = ec->getFrom();
        visited.diff(v->id());
    }
    wl.append_tail(vex);
    for (Vertex const* v = wl.remove_head(); v != nullptr;
         v = wl.remove_head()) {
        visited.bunion(v->id());
        if (v->id() != vexid && v->id() != newid) {
            DomSet * domset = g->get_dom_set(v->id());
            DomSet * pdomset = g->get_pdom_set(v->id());
            if (is_dom) {
                if (domset->is_contain(vexid)) {
                    domset->bunion(newid);
                } else if (g->get_ipdom(v->id()) == vexid) {
                    g->set_ipdom(v->id(), newid);
                    pdomset->bunion(newid);
                }
            } else {
                if (pdomset->is_contain(vexid)) {
                    pdomset->bunion(newid);
                } else if (g->get_idom(v->id()) == vexid) {
                    g->set_idom(v->id(), newid);
                    domset->bunion(newid);
                }
            }
        }
        for (EdgeC const* ec = v->getOutList(); ec != nullptr;
             ec = ec->get_next()) {
            Vertex const* s = ec->getTo();
            if (!visited.is_contain(s->id())) {
                visited.bunion(s->id());
                wl.append_tail(s);
            }
        }
    }
}


//The function adds Dom, Pdom, IDom, IPDom information for newipdom, whereas
//update the related info for 'vex'.
//vex: a marker vertex.
//newipdom: the vertex that must be ipdom of 'vex'.
void DGraph::addDomInfoByNewIPDom(Vertex const* vex, Vertex const* newipdom)
{
    UINT vexid = vex->id();
    UINT newipdomid = newipdom->id();
    UINT ipdom = get_ipdom(vexid);
    set_ipdom(vexid, newipdomid);
    if (ipdom != VERTEX_UNDEF) {
        set_ipdom(newipdomid, ipdom);
        UINT idomipdom = get_idom(ipdom);
        if (idomipdom == vexid) {
            set_idom(ipdom, newipdomid);
        }
    }
    if (!get_dom_set(vexid)->is_empty()) {
        //idom is meanlingless if there is no entry in graph.
        set_idom(newipdomid, vexid);
    }
    if (read_dom_set(newipdomid) == nullptr) {
        //Copy Dom, PDom info from vex to newidom.
        DomSet const* ds = read_dom_set(vexid);
        ASSERT0(ds);
        get_dom_set(newipdomid)->bunion(*ds);
        DomSet const* pds = read_pdom_set(vexid);
        if (pds != nullptr) {
            get_pdom_set(newipdomid)->bunion(*pds);
        }
    }
    //Set domset, pdomset.
    addVexToDomAndPdomSet(this, vex, newipdomid, false);
    get_pdom_set(vexid)->bunion(newipdomid);
    get_dom_set(newipdomid)->bunion(vexid);
}


//The function adds Dom, Pdom, IDom, IPDom information for newidom, whereas
//update the related info for 'vex'.
//vex: a marker vertex.
//newidom: the vertex that must be idom of 'vex'.
void DGraph::addDomInfoByNewIDom(Vertex const* vex, Vertex const* newidom)
{
    UINT vexid = vex->id();
    UINT newidomid = newidom->id();
    UINT idom = get_idom(vexid);
    set_idom(vexid, newidomid);
    if (idom != VERTEX_UNDEF) {
        set_idom(newidomid, idom);
        UINT ipdomidom = get_ipdom(idom);
        if (ipdomidom == vexid) {
            set_ipdom(idom, newidomid);
        }
    }
    if (!get_pdom_set(vexid)->is_empty()) {
        //ipdom is meanlingless if there is no exit in graph.
        set_ipdom(newidomid, vexid);
    }
    if (read_dom_set(newidomid) == nullptr) {
        //Copy Dom, PDom info from vex to newidom.
        DomSet const* ds = read_dom_set(vexid);
        ASSERT0(ds);
        get_dom_set(newidomid)->bunion(*ds);
        DomSet const* pds = read_pdom_set(vexid);
        if (pds != nullptr) {
            get_pdom_set(newidomid)->bunion(*pds);
        }
    }
    //Set domset, pdomset.
    addVexToDomAndPdomSet(this, vex, newidomid, true);
    get_dom_set(vexid)->bunion(newidomid);
    get_pdom_set(newidomid)->bunion(vexid);
}


//The function removes all Dom, Pdom, IDom, IPDom information about vex.
void DGraph::removeDomInfo(Vertex const* vex)
{
    ASSERT0(vex && vex->id() != VERTEX_UNDEF);
    UINT vexid = vex->id();
    freeDomPdomSet(vexid);
    UINT vexidom = get_idom(vexid);
    UINT vexipdom = get_ipdom(vexid);
    set_idom(vexid, VERTEX_UNDEF);
    set_ipdom(vexid, VERTEX_UNDEF);
    //Update domset, pdomset.
    DomSet visited;
    List<Vertex const*> wl;
    wl.append_tail(vex);
    for (Vertex const* v = wl.remove_head(); v != nullptr;
         v = wl.remove_head()) {
        visited.bunion(v->id());
        get_dom_set(v->id())->diff(vexid);
        get_pdom_set(v->id())->diff(vexid);
        if (get_idom(v->id()) == vexid) {
            set_idom(v->id(), vexidom);
        }
        if (get_ipdom(v->id()) == vexid) {
            set_ipdom(v->id(), vexipdom);
        }
        for (EdgeC const* ec = v->getInList(); ec != nullptr;
             ec = ec->get_next()) {
            Vertex const* v = ec->getFrom();
            if (!visited.is_contain(v->id())) {
                visited.bunion(v->id());
                wl.append_tail(v);
            }
        }
    }

    wl.append_tail(vex);
    for (Vertex const* v = wl.remove_head(); v != nullptr;
         v = wl.remove_head()) {
        visited.bunion(v->id());
        get_dom_set(v->id())->diff(vexid);
        get_pdom_set(v->id())->diff(vexid);
        if (get_idom(v->id()) == vexid) {
            set_idom(v->id(), vexidom);
        }
        if (get_ipdom(v->id()) == vexid) {
            set_ipdom(v->id(), vexipdom);
        }
        for (EdgeC const* ec = v->getOutList(); ec != nullptr;
             ec = ec->get_next()) {
            Vertex const* s = ec->getTo();
            if (!visited.is_contain(s->id())) {
                visited.bunion(s->id());
                wl.append_tail(s);
            }
        }
    }
}


//Perform DFS to seek for unreachable node.
//Return true if some nodes removed.
bool DGraph::removeUnreachNode(UINT entry_id)
{
    if (getVertexNum() == 0) { return false; }
    bool removed = false;
    BitSet visited;
    _removeUnreachNode(entry_id, visited);
    VertexIter c = VERTEX_UNDEF;
    for (Vertex * v = get_first_vertex(c);
         v != nullptr; v = get_next_vertex(c)) {
        if (!visited.is_contain(VERTEX_id(v))) {
            removeVertex(v);
            removed = true;
        }
    }
    return removed;
}
//END DGraph

} //namespace xcom
