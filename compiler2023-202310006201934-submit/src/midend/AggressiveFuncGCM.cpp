#include "../../include/midend/AggressiveFuncGCM.h"
#include "../../include/util/CenterControl.h"


#include "../../include/util/stl_op.h"





AggressiveFuncGCM::AggressiveFuncGCM(std::vector<Function *> *functions) {
    this->functions = functions;
}

void AggressiveFuncGCM::Run() {
    init();
    GCM();
}

void AggressiveFuncGCM::init() {
    for (Function *function: *functions) {
        (*def)[function] = new std::set<int>();
        (*defGlobals)[function] = new std::set<Value *>();
        (*use)[function] = new std::set<int>();
        (*useGlobals)[function] = new std::set<Value *>();
    }

    for (Function *function: *functions) {
        for (BasicBlock *bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock *) bb->next) {
            for (Instr *instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr *) instr->next) {
                if (dynamic_cast<INSTR::Store *>(instr) != nullptr) {
                    Value *val = ((INSTR::Store *) instr)->getPointer();
                    while (dynamic_cast<INSTR::GetElementPtr *>(val) != nullptr) {
                        val = ((INSTR::GetElementPtr *) val)->getPtr();
                    }
                    if (dynamic_cast<Function::Param *>(val) != nullptr) {
                        (*def)[function]->insert(_index_of(function->params, val));
                    } else if (dynamic_cast<GlobalValue *>(val) != nullptr) {
                        (*defGlobals)[function]->insert(val);
                    }
                } else if (dynamic_cast<INSTR::Load *>(instr) != nullptr) {
                    Value *val = ((INSTR::Load *) instr)->getPointer();
                    while (dynamic_cast<INSTR::GetElementPtr *>(val) != nullptr) {
                        val = ((INSTR::GetElementPtr *) val)->getPtr();
                    }
                    if (dynamic_cast<Function::Param *>(val) != nullptr) {
                        (*use)[function]->insert(_index_of(function->params, val));
                    }
                    if (dynamic_cast<GlobalValue *>(val) != nullptr) {
                        (*useGlobals)[function]->insert(val);
                    }
                }
            }
        }
    }


    for (Function *function: *functions) {
        for (BasicBlock *bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock *) bb->next) {
            for (Instr *instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr *) instr->next) {
                if (dynamic_cast<INSTR::Call *>(instr) != nullptr) {
                    Function *callFunc = ((INSTR::Call *) instr)->getFunc();
                    for (Value *val: *((INSTR::Call *) instr)->getParamList()) {
                        if (dynamic_cast<Function::Param *>(val) != nullptr) {
                            int thisFuncIndex = _index_of(function->params, val);
                            int callFuncIndex = _index_of(((INSTR::Call *) instr)->getParamList(), val);
                            if (callFunc->isExternal) {
                                (*use)[function]->insert(thisFuncIndex);
                            } else {
                                if (_contains_((*use)[callFunc], callFuncIndex)) {
                                    (*use)[function]->insert(thisFuncIndex);
                                }
                                if (_contains_((*def)[callFunc], callFuncIndex)) {
                                    (*def)[function]->insert(thisFuncIndex);
                                }
                                _set_add_all((*useGlobals)[callFunc], (*useGlobals)[function]);
//                                    (*defGlobals)[function]->addAll((*defGlobals)[callFunc]);
                                _set_add_all((*defGlobals)[callFunc], (*defGlobals)[function])
                            }
                        }
                    }
                }
            }
        }
    }

    bool change = true;
    while (change) {
        change = false;
        for (Function *function: *functions) {
            if (!(canGCMFunc->find(function) != canGCMFunc->end()) && check(function)) {
                canGCMFunc->insert(function);
                change = true;
            }
        }
    }

}

bool AggressiveFuncGCM::check(Function *function) {
    if (!CenterControl::_STRONG_FUNC_GCM) {
        for (Function::Param *param: *function->params) {
            if (param->type->is_pointer_type()) {
                return false;
            }
        }
        for (BasicBlock *bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock *) bb->next) {
            for (Instr *instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr *) instr->next) {
                if (dynamic_cast<INSTR::Call *>(instr) != nullptr) {
                    if (!(((INSTR::Call *) instr)->getFunc() == function) &&
                        !_contains_(canGCMFunc, ((INSTR::Call *) instr)->getFunc())) {
                        return false;
                    }
                }
                for (Value *value: instr->useValueList) {
                    if (dynamic_cast<GlobalValue *>(value) != nullptr) {
                        return false;
                    }
                }
            }
        }
        return true;
    }
    if ((*defGlobals)[function]->size() > 0) {
        return false;
    }
    for (Function::Param *param: *function->params) {
        if (param->type->is_pointer_type() && _contains_((*def)[function], _index_of(function->params, param))) {
            return false;
        }
    }
    for (BasicBlock *bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock *) bb->next) {
        for (Instr *instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr *) instr->next) {
            if (dynamic_cast<INSTR::Call *>(instr) != nullptr) {
                if (!(((INSTR::Call *) instr)->getFunc() == function) &&
                    !_contains_(canGCMFunc, ((INSTR::Call *) instr)->getFunc())) {
                    return false;
                }
            }
        }
    }
    return true;
}

void AggressiveFuncGCM::GCM() {
    noUserCallDelete();
    for (Function *function: *functions) {
        (*pinnedInstrMap)[function] = new std::set<Instr *>();
        for (BasicBlock *bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock *) bb->next) {
            for (Instr *instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr *) instr->next) {
                if (isPinned(instr)) {
                    (*pinnedInstrMap)[function]->insert(instr);
                }
            }
        }
    }
    for (Function *function: *functions) {
        scheduleEarlyForFunc(function);
    }
    for (Function *function: *functions) {
        scheduleLateForFunc(function);
    }
    //printBeforeMove();
    move();
}


void AggressiveFuncGCM::noUserCallDelete() {
    bool changed = true;
    while (changed) {
        changed = false;
        for (Function *function: *functions) {
            BasicBlock *beginBB = function->getBeginBB();
            BasicBlock *end = function->end;

            BasicBlock *pos = beginBB;
            while (!(pos == end)) {

                Instr *instr = pos->getBeginInstr();
                try {
                    while (instr->next != nullptr) {
                        if ((dynamic_cast<INSTR::Call *>(instr) != nullptr) && instr->isNoUse() && canGCM(instr)) {
                            instr->remove();
                            changed = true;
                        }
                        instr = (Instr *) instr->next;
                    }
                } catch (int) {
                    //System->out.println(instr->toString());
                }

                pos = (BasicBlock *) pos->next;
            }
        }
    }
}

void AggressiveFuncGCM::scheduleEarlyForFunc(Function *function) {
    std::set<Instr *> *pinnedInstr = (*pinnedInstrMap)[function];
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
                    if (!(dynamic_cast<Instr *>(value) != nullptr)) {
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

void AggressiveFuncGCM::scheduleEarly(Instr *instr) {
    if ((know->find(instr) != know->end())) {
        return;
    }
    know->insert(instr);
    instr->earliestBB = root;
    for (Value *X: instr->useValueList) {
        if (dynamic_cast<Instr *>(X) != nullptr) {
            //assert dynamic_cast<Instr*>(X) != nullptr;
            scheduleEarly((Instr *) X);
            if (instr->earliestBB->domTreeDeep < ((Instr *) X)->earliestBB->domTreeDeep) {
                instr->earliestBB = ((Instr *) X)->earliestBB;
            }
        }
    }
}


void AggressiveFuncGCM::scheduleLateForFunc(Function *function) {
    std::set<Instr *> *pinnedInstr = (*pinnedInstrMap)[function];
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

void AggressiveFuncGCM::scheduleLate(Instr *instr) {
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
    // *use the latest and earliest blocks to pick final positing
    // now latest is lca
    BasicBlock *best = lca;
    if (lca == nullptr) {
        //System->err.println("err_GCM");
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

void AggressiveFuncGCM::move() {
    for (Function *function: *functions) {
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
}

BasicBlock *AggressiveFuncGCM::findLCA(BasicBlock *a, BasicBlock *b) {
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

Instr *AggressiveFuncGCM::findInsertPos(Instr *instr, BasicBlock *bb) {
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
        if (dynamic_cast<INSTR::Call *>(pos) != nullptr &&
            ((INSTR::Call *) pos)->getFunc()->name == ("_sysy_stoptime")) {
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
bool AggressiveFuncGCM::canGCM(Instr *instr) {
    if (dynamic_cast<INSTR::Call *>(instr) != nullptr) {
        return _contains_(canGCMFunc, ((INSTR::Call *) instr)->getFunc());
    } else {
        return false;
    }
}

bool AggressiveFuncGCM::isPinned(Instr *instr) {
    return !canGCM(instr);
}
