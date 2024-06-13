#include "../../include/midend/ArrayGVNGCM.h"
#include "../../include/util/CenterControl.h"





//TODO:GCM更新phi,删除无用phi,添加数组相关分析,
// 把load,store,get_element_ptr也纳入GCM考虑之中



ArrayGVNGCM::ArrayGVNGCM(Function *function, std::set<Instr *> *canGVNGCM) {
    this->function = function;
    this->canGVNGCM = canGVNGCM;
}

void ArrayGVNGCM::Run() {
    Init();
    GVN();
    GCM();
}

void ArrayGVNGCM::Init() {
    BasicBlock *bb = function->getBeginBB();
    while (bb->next != nullptr) {
        Instr *instr = bb->getBeginInstr();
        while (instr->next != nullptr) {
            if (isPinned(instr)) {
                pinnedInstr->insert(instr);
            }
            instr = (Instr *) instr->next;
        }
        (*insertPos)[bb] = (Instr *) bb->getEndInstr()->prev;
        bb = (BasicBlock *) bb->next;
    }
}

void ArrayGVNGCM::GCM() {
    scheduleEarlyForFunc(function);
    scheduleLateForFunc(function);
    move();
}

void ArrayGVNGCM::GVN() {
    GvnMap->clear();
    GvnCnt->clear();
    globalArrayGVN(array, function);
}

void ArrayGVNGCM::globalArrayGVN(Value *array, Function *function) {
    BasicBlock *bb = function->getBeginBB();
    RPOSearch(bb);
}

void ArrayGVNGCM::RPOSearch(BasicBlock *bb) {
    std::set<Instr *> *loads = new std::set<Instr *>();
    Instr *load = bb->getBeginInstr();
    while (load->next != nullptr) {
        if ((canGVNGCM->find(load) != canGVNGCM->end())) {
            if (!addLoadToGVN(load)) {
                loads->insert(load);
            }
        }
        load = (Instr *) load->next;
    }

    for (BasicBlock *next: *bb->idoms) {
        RPOSearch(next);
    }

    for (Instr *temp: *loads) {
        removeLoadFromGVN(temp);
    }
}

void ArrayGVNGCM::add(std::string str, Instr *instr) {
    if (!(GvnCnt->find(str) != GvnCnt->end())) {
        (*GvnCnt)[str] = 1;
    } else {
        (*GvnCnt)[str] = (*GvnCnt)[str] + 1;
    }
    if (!(GvnMap->find(str) != GvnMap->end())) {
        (*GvnMap)[str] = instr;
    }
}

void ArrayGVNGCM::remove(std::string str) {
    (*GvnCnt)[str] = (*GvnCnt)[str] - 1;
    if ((*GvnCnt)[str] == 0) {
        GvnMap->erase(str);
    }
}

bool ArrayGVNGCM::addLoadToGVN(Instr *load) {
    //进行替换
    //assert dynamic_cast<INSTR::Load*>(load) != nullptr;
    std::string hash = ((INSTR::Load *) load)->getPointer()->name;
    if ((GvnMap->find(hash) != GvnMap->end())) {
        load->modifyAllUseThisToUseA((*GvnMap)[hash]);
        load->remove();
        return true;
    }
    add(hash, load);
    return false;
}

void ArrayGVNGCM::removeLoadFromGVN(Instr *load) {
    //assert dynamic_cast<INSTR::Load*>(load) != nullptr;
    std::string hash = ((INSTR::Load *) load)->getPointer()->name;
    remove(hash);
}


void ArrayGVNGCM::scheduleEarlyForFunc(Function *function) {
    know = new std::set<Instr *>();
    root = function->getBeginBB();
    for (Instr *instr: *pinnedInstr) {
        instr->earliestBB = instr->bb;
        know->insert(instr);
    }
    BasicBlock *bb = function->getBeginBB();
    while (bb->next != nullptr) {
        Instr *instr = bb->getBeginInstr();
        while (instr->next != nullptr) {
            if (!(know->find(instr) != know->end())) {
                scheduleEarly(instr);
            } else if ((pinnedInstr->find(instr) != pinnedInstr->end())) {
                for (Value *value: instr->useValueList) {
                    if (dynamic_cast<Constant *>(value) != nullptr || dynamic_cast<BasicBlock *>(value) != nullptr ||
                        dynamic_cast<GlobalVal *>(value) != nullptr || dynamic_cast<Function *>(value) != nullptr ||
                        dynamic_cast<Function::Param *>(value) != nullptr) {
                        continue;
                    }
                    //assert dynamic_cast<Instr*>(value) != nullptr;
                    scheduleEarly((Instr *) value);
                }
            }
            instr = (Instr *) instr->next;
        }
        bb = (BasicBlock *) bb->next;
    }
}

void ArrayGVNGCM::scheduleEarly(Instr *instr) {
    if ((know->find(instr) != know->end())) {
        return;
    }
    know->insert(instr);
    instr->earliestBB = root;
    for (Value *X: instr->useValueList) {
        if (dynamic_cast<Instr *>(X) != nullptr) {
            scheduleEarly((Instr *) X);
            if (instr->earliestBB->domTreeDeep < ((Instr *) X)->earliestBB->domTreeDeep) {
                instr->earliestBB = ((Instr *) X)->earliestBB;
            }
        }
    }
}


void ArrayGVNGCM::scheduleLateForFunc(Function *function) {
    know = new std::set<Instr *>();
    for (Instr *instr: *pinnedInstr) {
        instr->latestBB = instr->bb;
        know->insert(instr);
    }
    for (Instr *instr: *pinnedInstr) {
        Use *use = instr->getBeginUse();
        while (use->next != nullptr) {
            scheduleLate(use->user);
            use = (Use *) use->next;
        }
    }
    BasicBlock *bb = function->getBeginBB();
    while (bb->next != nullptr) {
        std::vector<Instr *> *instrs = new std::vector<Instr *>();
        Instr *instr = bb->getEndInstr();
        while (instr->prev != nullptr) {
            instrs->push_back(instr);
            instr = (Instr *) instr->prev;
        }
        for (Instr *instr1: *instrs) {
            if (!(know->find(instr1) != know->end())) {
                scheduleLate(instr1);
            }
        }
        bb = (BasicBlock *) bb->next;
    }
}

void ArrayGVNGCM::scheduleLate(Instr *instr) {
    if ((know->find(instr) != know->end())) {
        return;
    }
    know->insert(instr);
    BasicBlock *lca = nullptr;
    Use *usePos = instr->getBeginUse();
    while (usePos->next != nullptr) {
        Instr *y = usePos->user;

        scheduleLate(y);
        BasicBlock *use = y->latestBB;
        if (dynamic_cast<INSTR::Phi *>(y) != nullptr) {
            int j = usePos->idx;
            use = (*y->latestBB->precBBs)[j];
        }
        lca = findLCA(lca, use);
        usePos = (Use *) usePos->next;
    }
    BasicBlock *best = lca;
    if (lca == nullptr) {
        //System->err.println("err_GCM");
        //assert true;
    }
    while (!(lca == instr->earliestBB)) {
        if (lca->getLoopDep() < best->getLoopDep()) {
            best = lca;
        }
        lca = lca->iDominator;
    }
    if (lca->getLoopDep() < best->getLoopDep()) {
        best = lca;
    }
    instr->latestBB = best;

    if (instr->latestBB != (instr->bb)) {
        instr->delFromNowBB();
        //TODO:检查 insert 位置 是在头部还是尾部
        Instr *pos = nullptr;
        pos = findInsertPos(instr, instr->latestBB);
        pos->insert_before(instr);
        instr->bb = instr->latestBB;
    }
}

void ArrayGVNGCM::move() {
    BasicBlock *bb = function->getBeginBB();
    while (bb->next != nullptr) {
        Instr *instr = bb->getBeginInstr();
        std::vector<Instr *> *instrs = new std::vector<Instr *>();
        while (instr->next != nullptr) {
            instrs->push_back(instr);
            instr = (Instr *) instr->next;
        }
        for (Instr *i: *instrs) {
            if (i->latestBB != (bb)) {
                //assert false;
                i->delFromNowBB();
                //TODO:检查 insert 位置 是在头部还是尾部
                Instr *pos = findInsertPos(i, i->latestBB);
                pos->insert_before(i);
                i->bb = i->latestBB;
            }
        }
        bb = (BasicBlock *) bb->next;
    }
}

BasicBlock *ArrayGVNGCM::findLCA(BasicBlock *a, BasicBlock *b) {
    if (a == nullptr) {
        return b;
    }
    while (a->domTreeDeep > b->domTreeDeep) {
        a = a->iDominator;
    }
    while (b->domTreeDeep > a->domTreeDeep) {
        b = b->iDominator;
    }
    while (!(a == b)) {
        a = a->iDominator;
        b = b->iDominator;
    }
    return a;
}

Instr *ArrayGVNGCM::findInsertPos(Instr *instr, BasicBlock *bb) {
    std::vector<Value *> *users = new std::vector<Value *>();
    Use *use = instr->getBeginUse();
    while (use->next != nullptr) {
        users->push_back(use->user);
        use = (Use *) use->next;
    }
    Instr *later = nullptr;
    Instr *pos = bb->getBeginInstr();
    while (pos->next != nullptr) {
        if (dynamic_cast<INSTR::Phi *>(pos) != nullptr) {
            pos = (Instr *) pos->next;
            continue;
        }
        if ((std::find(users->begin(), users->end(), pos) != users->end())) {
            later = pos;
            break;
        }
        pos = (Instr *) pos->next;
    }

    if (later != nullptr) {
        return later;
    }

    return bb->getEndInstr();
}


// TODO:考虑数组变量读写的GCM 指针是SSA形式 但是内存不是
//  考虑移动load,store是否会产生影响
//  移动的上下限是对同一个数组的最近的load/store?
bool ArrayGVNGCM::isPinned(Instr *instr) {
    if ((canGVNGCM->find(instr) != canGVNGCM->end())) {
        return true;
    }
    return dynamic_cast<INSTR::Jump *>(instr) != nullptr || dynamic_cast<INSTR::Branch *>(instr) != nullptr ||
           dynamic_cast<INSTR::Phi *>(instr) != nullptr || dynamic_cast<INSTR::Return *>(instr) != nullptr ||
           dynamic_cast<INSTR::Store *>(instr) != nullptr || dynamic_cast<INSTR::Load *>(instr) != nullptr ||
           // dynamic_cast<INSTR::Icmp*>(instr) != nullptr || dynamic_cast<INSTR::Fcmp*>(instr) != nullptr ||
           //dynamic_cast<INSTR::GetElementPtr*>(instr) != nullptr ||
           dynamic_cast<INSTR::Call *>(instr) != nullptr;
}
