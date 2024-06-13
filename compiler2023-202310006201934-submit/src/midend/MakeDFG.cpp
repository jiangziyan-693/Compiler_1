//
// Created by start_0916 on 23-7-20.
//
#include "midend/MakeDFG.h"
#include "../../include/util/HashMap.h"
#include "../../include/mir/Instr.h"

#include <algorithm>

MakeDFG::MakeDFG(std::vector<Function *>* functions) {
    this->functions = functions;
}

void MakeDFG::Run() {
    RemoveDeadBB();
    MakeCFG();
    MakeDom();
    MakeIDom();
    MakeDF();
    MakeDomTreeDeep();
    MakeReversePostOrder();
    MakeRDom();
    MakeIRDom();

    // for (Function *f: *functions) {
    //     f->print_dominator_tree();
    //     f->print_reversed_dominator_tree();
    // }
}

void MakeDFG::RemoveDeadBB() {
    for (auto function: (*functions)) {
        removeFuncDeadBB(function);
    }
}

void MakeDFG::MakeCFG() {
    for (auto function: (*functions)) {
        makeSingleFuncCFG(function);
    }
}

void MakeDFG::MakeDom() {
    for (Function* function: (*functions)) {
        makeSingleFuncDom(function);
    }
}

void MakeDFG::MakeIDom() {
    for (Function* function: (*functions)) {
        makeSingleFuncIDom(function);
    }
}

void MakeDFG::MakeRDom() {
    for (Function* function: (*functions)) {
        makeSingleFuncRDom(function);
    }
}

void MakeDFG::MakeIRDom() {
    for (Function* function: (*functions)) {
        makeSingleFuncIRDom(function);
        DFSForRDomTree(function->vExit);
    }
}

void MakeDFG::MakeDF() {
    for (Function* function: (*functions)) {
        makeSingleFuncDF(function);
    }
}

void MakeDFG::MakeDomTreeDeep() {
    for (Function* function: (*functions)) {
        makeDomTreeDeepForFunc(function);
    }
}

void MakeDFG::makeReversePostOrder(Function *f) {
    f->reversedPostOrder.clear();
    BasicBlock *entry = f->getBeginBB();
    std::set<BasicBlock*> know;
    std::function<void (BasicBlock *)> postVisit = [&] (BasicBlock *bb) {
        f->reversedPostOrder.push_back(bb);
    };
    dfs(entry, nullptr, &know, {emptyDfsSingleOp, postVisit});
    std::reverse(f->reversedPostOrder.begin(), f->reversedPostOrder.end());
}

void MakeDFG::MakeReversePostOrder() {
    for (Function* function: (*functions)) {
        makeReversePostOrder(function);
    }
}

void MakeDFG::removeFuncDeadBB(Function *function) {
    BasicBlock* beginBB = function->getBeginBB();
    BasicBlock* endBB = function->getEndBB();


    for (auto v: ValueSet(*preMap)) {
        delete v;
    }
    for (auto v: ValueSet(*sucMap)) {
        delete v;
    }

    preMap->clear();
    sucMap->clear();
    std::set<BasicBlock*> BBs;

    for (BasicBlock* pos = beginBB; pos->next != nullptr; pos = (BasicBlock*) pos->next) {
        (*preMap)[pos] = new std::vector<BasicBlock*>();
        (*sucMap)[pos] = new std::vector<BasicBlock*>();
        BBs.insert(pos);

        Instr* instr = pos->getEndInstr();
        while (dynamic_cast<INSTR::Branch*> (instr->prev) != nullptr
                || dynamic_cast<INSTR::Jump*> (instr->prev) != nullptr) {
            instr->remove();
            instr = (Instr*) instr->prev;
        }
    }

    for (BasicBlock* pos = beginBB; pos->next != nullptr; pos = (BasicBlock*) pos->next) {
        Instr* lastInstr = pos->getEndInstr();
        if (lastInstr->isBranch()) {
            BasicBlock* ele = ((INSTR::Branch*) lastInstr)->getElseTarget();
            BasicBlock* the = ((INSTR::Branch*) lastInstr)->getThenTarget();
            (*sucMap)[pos]->push_back(ele);
            (*sucMap)[pos]->push_back(the);
            (*preMap)[the]->push_back(pos);
            (*preMap)[ele]->push_back(pos);
        } else if (lastInstr->isJump()) {
            BasicBlock* ne = ((INSTR::Jump*) lastInstr)->getTarget();
            (*sucMap)[pos]->push_back(ne);
            (*preMap)[ne]->push_back(pos);
        }
    }

    std::set<BasicBlock*>* needRemove = new std::set<BasicBlock*>();
    std::set<BasicBlock*>* know = new std::set<BasicBlock*>();

    DFS(function->getBeginBB(), know);
    for (BasicBlock* pos = beginBB; pos->next != nullptr; pos = (BasicBlock*) pos->next) {
        if (know->find(pos) == know->end()) {
            needRemove->insert(pos);
        }
    }

    for (BasicBlock* bb: (*needRemove)) {
        bb->remove();
        for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
            instr->remove();
        }
    }

}

void MakeDFG::DFS(BasicBlock *bb, std::set<BasicBlock *>* know) {
    if (know->find(bb) != know->end()) {
        return;
    }
    know->insert(bb);
    for (auto next: *(sucMap->find(bb)->second)) {
        DFS(next, know);
    }
}

void MakeDFG::makeSingleFuncCFG(Function *function) {
    BasicBlock* beginBB = function->getBeginBB();
    BasicBlock* end = function->end;

    preMap = new std::unordered_map<BasicBlock*, std::vector<BasicBlock*>*>();
    sucMap = new std::unordered_map<BasicBlock*, std::vector<BasicBlock*>*>();
    auto* BBs = new std::set<BasicBlock*>();
    BasicBlock* pos = beginBB;
    for (; pos != end; pos = (BasicBlock*) pos->next) {
        (*preMap)[pos] = new std::vector<BasicBlock*>();
        (*sucMap)[pos] = new std::vector<BasicBlock*>();
        BBs->insert(pos);
    }

    //添加前驱和后继
    pos = beginBB;
    while (!(pos == end)) {
        Instr* lastInstr = pos->getEndInstr();
        if (dynamic_cast<INSTR::Branch*>(lastInstr) != nullptr) {
            BasicBlock* elseTarget = ((INSTR::Branch*) lastInstr)->getElseTarget();
            BasicBlock* thenTarget = ((INSTR::Branch*) lastInstr)->getThenTarget();
            (*sucMap)[pos]->push_back(thenTarget);
            (*sucMap)[pos]->push_back(elseTarget);
            (*preMap)[thenTarget]->push_back(pos);
            (*preMap)[elseTarget]->push_back(pos);
        } else if (dynamic_cast<INSTR::Jump*>(lastInstr) != nullptr) {
            BasicBlock* target = ((INSTR::Jump*) lastInstr)->getTarget();
            (*sucMap)[pos]->push_back(target);
            (*preMap)[target]->push_back(pos);
        }
        pos = (BasicBlock*) pos->next;
    }

    //回写基本块和函数
    pos = beginBB;
    while (!(pos == end)) {
        pos->setPrecBBs((*preMap)[pos]);
        pos->setSuccBBs((*sucMap)[pos]);
        pos = (BasicBlock*) pos->next;
    }
    function->preMap = preMap;
    function->sucMap = (sucMap);
    function->BBs = (BBs);
}

void MakeDFG::makeSingleFuncDom(Function *function) {
    BasicBlock* enter = function->getBeginBB();
    std::set<BasicBlock*>* BBs = function->BBs;
    for (BasicBlock* bb: *BBs) {
        std::set<BasicBlock*>* doms = new std::set<BasicBlock*>();
        std::set<BasicBlock*>* know = new std::set<BasicBlock*>();
        dfs(enter, bb, know);

        for (BasicBlock* temp: *BBs) {
            if (!(know->find(temp) != know->end())) {
                doms->insert(temp);
            }
        }

        bb->doms = (doms);
    }
}

void MakeDFG::makeSingleFuncRDom(Function *function) {
    BasicBlock* vExit = new BasicBlock;
    function->vExit = vExit;
    std::set<BasicBlock*>* BBs = function->BBs;
    // setup vExit's precBBs
    for (BasicBlock* bb: *BBs) {
        if (bb->succBBs->empty()) {
            vExit->precBBs->push_back(bb);
        }
    }

    for (BasicBlock* bb: *BBs) {
        std::set<BasicBlock*>* rdoms = new std::set<BasicBlock*>();
        std::set<BasicBlock*>* know = new std::set<BasicBlock*>();
        rdfs(vExit, bb, know);
        for (BasicBlock *temp: *BBs) {
            if (know->find(temp) == know->end()) {
                rdoms->insert(temp);
            }
        }
        bb->rdoms = rdoms;
        // std::cerr << "bb(" << bb->label << ") rdoms: ";
        // for (BasicBlock *rb: *bb->rdoms) {
        //     std::cerr << rb->label << " ";
        // }
        // std::cerr << std::endl;
    }
    {
        BasicBlock* bb = vExit;
        std::set<BasicBlock*>* rdoms = new std::set<BasicBlock*>();
        std::set<BasicBlock*>* know = new std::set<BasicBlock*>();
        rdfs(vExit, bb, know);
        for (BasicBlock *temp: *BBs) {
            if (know->find(temp) == know->end()) {
                rdoms->insert(temp);
            }
        }
        bb->rdoms = rdoms;
        // std::cerr << "vExit(" << bb->label << ") rdoms: ";
        // for (BasicBlock *rb: *bb->rdoms) {
        //     std::cerr << rb->label << " ";
        // }
        // std::cerr << std::endl;
    }
}

DfsSingleOp emptyDfsSingleOp { [] (BasicBlock *) {} };
DfsOp emptyDfsOp {emptyDfsSingleOp, emptyDfsSingleOp};

void MakeDFG::dfs(BasicBlock *bb, BasicBlock *no, std::set<BasicBlock *>* know, const DfsOp &op) {
    if ((bb == no)) {
        return;
    }
    if ((know->find(bb) != know->end())) {
        return;
    }
    op.first(bb);
    know->insert(bb);
    for (BasicBlock* next: *(bb->succBBs)) {
        if (know->find(next) == know->end() && (next != no)) {
            dfs(next, no, know, op);
        }
    }
    op.second(bb);
}

void MakeDFG::rdfs(BasicBlock *bb, BasicBlock *no, std::set<BasicBlock *>* know, const DfsOp &op) {
    if ((bb == no)) {
        return;
    }
    if ((know->find(bb) != know->end())) {
        return;
    }
    op.first(bb);
    know->insert(bb);
    for (BasicBlock* next: *(bb->precBBs)) {
        if (know->find(next) == know->end() && (next != no)) {
            rdfs(next, no, know, op);
        }
    }
    op.second(bb);
}

void MakeDFG::makeSingleFuncIDom(Function *function) {
    std::set<BasicBlock*>* BBs = function->BBs;
    for (BasicBlock* A: *BBs) {
        std::set<BasicBlock*>* idoms = new std::set<BasicBlock*>();
        for (BasicBlock* B: (*A->doms)) {
            if (AIDomB(A, B)) {
                idoms->insert(B);
            }
        }

        A->idoms = (idoms);
    }
}

void MakeDFG::makeSingleFuncIRDom(Function *function) {
    std::set<BasicBlock*>* BBs = function->BBs;
    for (BasicBlock* A: *BBs) {
        std::set<BasicBlock*>* irdoms = new std::set<BasicBlock*>();
        for (BasicBlock* B: (*A->rdoms)) {
            if (AIRDomB(A, B)) {
                irdoms->insert(B);
            }
        }

        A->irdoms = (irdoms);
    }
    {
        BasicBlock *A = function->vExit;
        std::set<BasicBlock*>* irdoms = new std::set<BasicBlock*>();
        for (BasicBlock* B: (*A->rdoms)) {
            if (AIRDomB(A, B)) {
                irdoms->insert(B);
            }
        }
        A->irdoms = (irdoms);
    }
}

bool MakeDFG::AIDomB(BasicBlock *A, BasicBlock *B) {
    std::set<BasicBlock*>* ADoms = A->doms;
    if (!(ADoms->find(B) != ADoms->end())) {
        return false;
    }
    if ((A == B)) {
        return false;
    }
    for (BasicBlock* temp: *ADoms) {
        if (!(temp == A) && !(temp == B)) {
            if (temp->doms->find(B) != temp->doms->end()) {
                return false;
            }
        }
    }
    return true;
}

bool MakeDFG::AIRDomB(BasicBlock *A, BasicBlock *B) {
    std::set<BasicBlock*>* ARDoms = A->rdoms;
    if (!(ARDoms->find(B) != ARDoms->end())) {
        return false;
    }
    if ((A == B)) {
        return false;
    }
    for (BasicBlock* temp: *ARDoms) {
        if (!(temp == A) && !(temp == B)) {
            if (temp->rdoms->find(B) != temp->rdoms->end()) {
                return false;
            }
        }
    }
    return true;
}

void MakeDFG::makeSingleFuncDF(Function *function) {
    for (BasicBlock* X: (*function->BBs)) {
        std::set<BasicBlock*>* DF = new std::set<BasicBlock*>();
        for (BasicBlock* Y: (*function->BBs)) {
            if (DFXHasY(X, Y)) {
                DF->insert(Y);
            }
        }

        X->DF = (DF);
    }
}

bool MakeDFG::DFXHasY(BasicBlock *X, BasicBlock *Y) {
    for (BasicBlock* P: *(Y->precBBs)) {
        if (((X->doms)->find(P) != (X->doms)->end()) &&
        ((X == Y) || !((X->doms)->find(Y) != (X->doms)->end()))) {
            return true;
        }
    }
    return false;
}

void MakeDFG::makeDomTreeDeepForFunc(Function *function) {
    BasicBlock* entry = function->getBeginBB();
    int deep = 1;
    DFSForDomTreeDeep(entry, deep);
}

void MakeDFG::DFSForDomTreeDeep(BasicBlock *bb, int deep) {
    bb->domTreeDeep = (deep);
    for (BasicBlock* next: *(bb->idoms)) {
        next->iDominator = (bb);
        DFSForDomTreeDeep(next, deep+1);
    }
}

void MakeDFG::DFSForRDomTree(BasicBlock *bb) {
    for (BasicBlock* next: *(bb->irdoms)) {
        next->iRDominator = (bb);
        DFSForRDomTree(next);
    }
}