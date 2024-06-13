//
// Created by start_0916 on 23-7-20.
//

#ifndef MAIN_MAKEDFG_H
#define MAIN_MAKEDFG_H

#include "../mir/Function.h"
#include "../mir/BasicBlock.h"
#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <functional>

typedef std::function<void (BasicBlock *)> DfsSingleOp;
typedef std::pair<DfsSingleOp, DfsSingleOp> DfsOp;  // pre-order-visit, post-order-visit
extern DfsSingleOp emptyDfsSingleOp;
extern DfsOp emptyDfsOp;

class MakeDFG {
public:
    std::vector<Function*>* functions;
    std::unordered_map<BasicBlock*, std::vector<BasicBlock*>*>* preMap = new std::unordered_map<BasicBlock*, std::vector<BasicBlock*>*>();
    std::unordered_map<BasicBlock*, std::vector<BasicBlock*>*>* sucMap = new std::unordered_map<BasicBlock*, std::vector<BasicBlock*>*>();
public:
    MakeDFG(std::vector<Function*>* functions);
    void Run();
    void RemoveDeadBB();
    void MakeCFG();
    void MakeDom();
    void MakeIDom();
    void MakeDF();
    void MakeDomTreeDeep();
    void MakeReversePostOrder();
    void MakeRDom();
    void MakeIRDom();

    void removeFuncDeadBB(Function* function);
    void DFS(BasicBlock* bb, std::set<BasicBlock*>* know);
    void makeSingleFuncCFG(Function* function);
    void makeSingleFuncDom(Function* function);
    void dfs(BasicBlock* bb, BasicBlock* no, std::set<BasicBlock*>* know, const DfsOp &op = emptyDfsOp);
    void rdfs(BasicBlock* bb, BasicBlock* no, std::set<BasicBlock*>* know, const DfsOp &op = emptyDfsOp);
    void makeSingleFuncIDom(Function* function);
    bool AIDomB(BasicBlock* A, BasicBlock* B);
    bool AIRDomB(BasicBlock *A, BasicBlock* B);
    void makeSingleFuncDF(Function* function);
    bool DFXHasY(BasicBlock* X, BasicBlock* Y);
    void makeDomTreeDeepForFunc(Function* function);
    void DFSForDomTreeDeep(BasicBlock* bb, int deep);
    void DFSForRDomTree(BasicBlock* bb);

    void makeReversePostOrder(Function *function);
    void makeSingleFuncRDom(Function *function);
    void makeSingleFuncIRDom(Function *function);
};


#endif //MAIN_MAKEDFG_H
