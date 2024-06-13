//
// Created by start on 23-7-29.
//

#include "../../include/midend/LCSSA.h"
#include "../../include/util/CenterControl.h"





LCSSA::LCSSA(std::vector<Function*>* functions) {
    this->functions = functions;
}

void LCSSA::Run() {
    addPhi();
}

void LCSSA::addPhi() {
    for (Function* function: *functions) {
        addPhiForFunc(function);
    }
}

void LCSSA::addPhiForFunc(Function* function) {
    for (BasicBlock* bb: *function->loopHeads) {
        addPhiForLoop(bb->loop);
    }
}

void LCSSA::addPhiForLoop(Loop* loop) {
    std::set<BasicBlock*>* exits = loop->getExits();
    for (BasicBlock* bb: *loop->getNowLevelBB()) {
        Instr* instr = bb->getBeginInstr();
        while (instr->next != nullptr) {
//            if (instr->getName() == "%v648") {
//                printf("test\n");
//            }
            if (usedOutLoop(instr, loop)) {
                for (BasicBlock* exit: *exits) {
                    addPhiAtExitBB(instr, exit, loop);
                }
            }
            instr = (Instr*) instr->next;
        }
    }
}

void LCSSA::addDef(std::set<Instr*>* defs, Instr* instr) {
    if ((defs->find(instr) != defs->end())) {
        return;
    }
    defs->insert(instr);
    if (dynamic_cast<INSTR::Phi*>(instr) != nullptr) {
        for (Value* value: instr->useValueList) {
            if (dynamic_cast<Instr*>(value) != nullptr) {
                defs->insert((Instr*) value);
            }
        }
    }
}

void LCSSA::addPhiAtExitBB(Value* value, BasicBlock* bb, Loop* loop) {
    std::vector<Value*>* optionalValues = new std::vector<Value*>();
    for (int i = 0; i < bb->precBBs->size(); i++) {
        optionalValues->push_back(value);
    }
    INSTR::Phi* phi = new INSTR::Phi(value->type, optionalValues, bb);
    //Instr->Phi phi = new INSTR::Phi(value->getType(), optionalValues, bb, true);
    //TODO:ReName
    std::unordered_map<Instr*, int>* useInstrMap = new std::unordered_map<Instr*, int>();
    std::set<Instr*>* defInstrs = new std::set<Instr*>();
    std::stack<Value*>* S = new std::stack<Value*>();
    //defInstrs->insert(phi);
    //defInstrs->insert((Instr*) value);
    addDef(defInstrs, phi);
    addDef(defInstrs, (Instr*) value);
    Use* use = value->getBeginUse();
    while (use->next != nullptr) {
        Instr* user = use->user;
        BasicBlock* userBB = user->bb;
        //fixme:time 07-18-00:15 考虑正确性
        //PHI对其的使用其实是在PHI的前驱对它的使用
        //与GCM的scheduleLate采用同一思想
        //对于正常的PHI不能重新计算到达定义,因为有些定义已经没有了
        //初始化S?

        if (dynamic_cast<INSTR::Phi*>(user) != nullptr) {
            if ((userBB->loop == loop)) {
                use = (Use*) use->next;
                continue;
            }
        }

        if (dynamic_cast<INSTR::Phi*>(user) != nullptr) {
            int index = use->idx;
            userBB = (*userBB->precBBs)[index];
        }
        if (userBB->loop != loop) {
            (*useInstrMap)[use->user] = use->idx;
        }

        //(*useInstrMap)[use->user] = use->idx;
        use = (Use*) use->next;
    }
    (*useInstrMap)[phi] = -1;
    RenameDFS(S, bb->function->getBeginBB(), useInstrMap, defInstrs);
    //RenameDFS(S, bb->loop->getHeader(), useInstrMap, *defInstrs);
}

void LCSSA::RenameDFS(std::stack<Value*>* S, BasicBlock* X, std::unordered_map<Instr*, int>* useInstrMap, std::set<Instr*>* defInstrs) {
    int cnt = 0;
    Instr* A = X->getBeginInstr();
    while (A->next != nullptr) {
//        if (A->getName() == "%v636") {
//            printf("log\n");
//        }
        if (!(dynamic_cast<INSTR::Phi*>(A) != nullptr) && (useInstrMap->find(A) != useInstrMap->end())) {
            A->modifyUse(getStackTopValue(S), (*useInstrMap)[A]);
        }
        if ((defInstrs->find(A) != defInstrs->end())) {
            S->push(A);
            cnt++;
        }
        A = (Instr*) A->next;
    }

    std::vector<BasicBlock*>* Succ = X->succBBs;
    for (int i = 0; i < Succ->size(); i++) {
        BasicBlock* bb = (*Succ)[i];
        Instr* instr = (*Succ)[i]->getBeginInstr();
        while (instr->next != nullptr) {
            if (!(dynamic_cast<INSTR::Phi*>(instr) != nullptr)) {
                break;
            }
            if ((useInstrMap->find(instr) != useInstrMap->end())) {
                if ((*useInstrMap)[instr] == -1) {
                    instr->modifyUse(getStackTopValue(S), std::find(bb->precBBs->begin(), bb->precBBs->end(), X) - bb->precBBs->begin());
                } else if (std::find(bb->precBBs->begin(), bb->precBBs->end(), X) - bb->precBBs->begin() == (*useInstrMap)[instr]) {
                    instr->modifyUse(getStackTopValue(S), (*useInstrMap)[instr]);
                }
                //instr->modifyUse(getStackTopValue(S), bb->precBBs->indexOf(X));
            }
            instr = (Instr*) instr->next;
        }
    }

    for (BasicBlock* next: *X->idoms) {
        RenameDFS(S, next, useInstrMap, defInstrs);
    }

    for (int i = 0; i < cnt; i++) {
        S->pop();
    }
}

Value* LCSSA::getStackTopValue(std::stack<Value*>* S) {
    if (S->empty()) {
        return new UndefValue();
    }
    return S->top();
}


//判断value是否在loop外被使用
bool LCSSA::usedOutLoop(Value* value, Loop* loop) {
    Use* use = value->getBeginUse();
    while (use->next != nullptr) {
        Instr* user = use->user;
        BasicBlock* userBB = user->bb;
        //PHI对其的使用其实是在PHI的前驱对它的使用
        //与GCM的scheduleLate采用同一思想
        if (dynamic_cast<INSTR::Phi*>(user) != nullptr) {
            int index = use->idx;
            userBB = (*userBB->precBBs)[index];
        }
        if (userBB->loop != loop) {
            return true;
        }
        use = (Use*) use->next;
    }
    return false;
}


