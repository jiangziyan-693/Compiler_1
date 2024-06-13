//
// Created by start on 23-7-26.
//

#include "../../include/midend/Mem2Reg.h"
#include "../../include/util/CenterControl.h"





Mem2Reg::Mem2Reg(std::vector<Function*>* functions) {
    this->functions = functions;
}

void Mem2Reg::Run() {
    removeAlloc();
}

void Mem2Reg::removeAlloc() {
    for (Function* function: *functions) {
        removeFuncAlloc(function);
    }
}

void Mem2Reg::removeFuncAlloc(Function* function) {
    for (BasicBlock* bb: *function->BBs) {
        removeBBAlloc(bb);
    }
}

void Mem2Reg::removeBBAlloc(BasicBlock* basicBlock) {
    Instr* temp = basicBlock->getBeginInstr();
    while (!temp->isEnd()) {
        if (dynamic_cast<INSTR::Alloc*>(temp) != nullptr && !((INSTR::Alloc*) temp)->isArrayAlloc()) {
            remove(temp);
        }
        temp = (Instr*) temp->next;
    }
}

void Mem2Reg::remove(Instr* instr) {
    //def 和 use 是指 定义/使用了 alloc出的那块内存地址
    std::set<BasicBlock*>* defBBs = new std::set<BasicBlock*>();
    std::set<BasicBlock*>* useBBs = new std::set<BasicBlock*>();
    std::set<Instr*>* defInstrs = new std::set<Instr*>();
    std::set<Instr*>* useInstrs = new std::set<Instr*>();
    Use* pos = instr->getBeginUse();
    Type* InnerType = ((INSTR::Alloc*) instr)->contentType;
    while (pos->next != nullptr) {
        Instr* userInstr = pos->user;
        if (dynamic_cast<INSTR::Store*>(userInstr) != nullptr) {
            defBBs->insert(userInstr->bb);
            defInstrs->insert(userInstr);
        } else if (dynamic_cast<INSTR::Load*>(userInstr) != nullptr) {
            useBBs->insert(userInstr->bb);
            useInstrs->insert(userInstr);
        } else {
            exit(10);
        }
        pos = (Use*) pos->next;
    }

    if (useBBs->empty()) {
        for (Instr* temp: *defInstrs) {
            temp->remove();
        }
    } else if (defBBs->size() == 1 && check(defBBs, useBBs)) {
        //fixme:本来我认为如果只有一个定义指令 就可以直接进行替换,即如下代码
        //          但是 实际上这样替换的前提还需要定义指令支配所有使用指令,
        //          当存在未定义的使用的时候会直接生成不符合llvm的语法,所以直接删除这一分支
        //if (defInstrs->size() == 1) {
        //                Instr* def = nullptr;
        //                for (Instr* temp: *defInstrs) {
        //                    def = temp;
        //                }
        //                for (Instr* use: *useInstrs) {
        //                    use->modifyAllUseThisToUseA(((INSTR::Store*) def)->getValue());
        //                }
        //            }

        BasicBlock* defBB = nullptr;
        for (BasicBlock* bb: *defBBs) {
            defBB = bb;
        }

        Instr* reachDef = nullptr;
        Instr* BB_pos = defBB->getBeginInstr();
        while (BB_pos->next != nullptr) {
            if ((defInstrs->find(BB_pos) != defInstrs->end())) {
                reachDef = BB_pos;
            } else if ((useInstrs->find(BB_pos) != useInstrs->end())) {
                if (reachDef == nullptr) {
                    BB_pos->modifyAllUseThisToUseA(new UndefValue(InnerType));
                } else {
                    BB_pos->modifyAllUseThisToUseA(((INSTR::Store*) reachDef)->getValue());
                }
            }
            BB_pos = (Instr*) BB_pos->next;
        }

        //TODO:对于未定义的使用,是否不必要进行定义,当前实现方法为所有其他BB的use认为使用了唯一的reachDef
        for (Instr* userInstr: *useInstrs) {
            if (userInstr->bb != (defBB)) {
                //assert reachDef != nullptr;
                userInstr->modifyAllUseThisToUseA(((INSTR::Store*) reachDef)->getValue());
            }
        }

    } else {
        //TODO:多个块store 此Alloc指令申请的空间
        std::set<BasicBlock*>* F = new std::set<BasicBlock*>();
        std::set<BasicBlock*>* W = new std::set<BasicBlock*>();


        // W->insertAll(defBBs);
        for (auto t: (*defBBs)) {
            W->insert(t);
        }

        while (!W->empty()) {
            BasicBlock* X = getRandFromHashSet(W);
            W->erase(X);
            for (BasicBlock* Y: (*X->DF)) {
                if (!(F->find(Y) != F->end())) {
                    F->insert(Y);
                    if (!(defBBs->find(Y) != defBBs->end())) {
                        W->insert(Y);
                    }
                }
            }
        }

        for (BasicBlock* bb: *F) {
            //TODO:添加phi指令
            //new phi
            //useInstrs->insert(phi);
            //defInstrs->insert(phi);
            //bb->insertAtEnd();
            //dynamic_cast<Type->PointerType>(assert instr->getType()) != nullptr;
            Instr* PHI = nullptr;
            std::vector<Value*>* optionalValues = new std::vector<Value*>();
            for (int i = 0; i < bb->precBBs->size(); i++) {
                //空指令
                optionalValues->push_back(new Instr());
            }
            if (((PointerType *) instr->type)->inner_type->is_float_type()) {
                PHI = new INSTR::Phi(BasicType::F32_TYPE, optionalValues, bb);
            } else {
                PHI = new INSTR::Phi(BasicType::I32_TYPE, optionalValues, bb);
            }
            useInstrs->insert(PHI);
            defInstrs->insert(PHI);
            //bb->insertAtHead(PHI);
        }

        //TODO:Rename
        auto *S = new std::stack<Value*>();
        Type *type = ((PointerType *) instr->type)->inner_type;
        RenameDFS(S, instr->bb->function->getBeginBB(), useInstrs, defInstrs);
    }



    //
    instr->remove();
    if (!(useBBs->empty())) {
        for (Instr* instr1 : *defInstrs) {
            if (!(dynamic_cast<INSTR::Phi*>(instr1) != nullptr)) {
                instr1->remove();
            }
        }
        for (Instr* instr1 : *useInstrs) {
            if (!(dynamic_cast<INSTR::Phi*>(instr1) != nullptr)) {
                instr1->remove();
            }
        }
    }
}

bool Mem2Reg::check(std::set<BasicBlock*>* defBBs, std::set<BasicBlock*>* useBBs) {
    BasicBlock* defBB = nullptr;
    //assert defBBs->size() == 1;
    for (BasicBlock* bb: *defBBs) {
        defBB = bb;
    }
    for (BasicBlock* bb: *useBBs) {
        if (!((defBB->doms)->find(bb) != (defBB->doms)->end())) {
            return false;
        }
    }
    return true;
}

void Mem2Reg::RenameDFS(std::stack<Value*>* S, BasicBlock* X, std::set<Instr*>* useInstrs, std::set<Instr*>* defInstrs) {
    int cnt = 0;
    Instr* A = X->getBeginInstr();
    while (A->next != nullptr) {
        if (!(dynamic_cast<INSTR::Phi*>(A) != nullptr) && (useInstrs->find(A) != useInstrs->end())) {
            //assert dynamic_cast<INSTR::Load*>(A) != nullptr;
            //A->modifyAllUseThisToUseA(S->peek());
            A->modifyAllUseThisToUseA(getStackTopValue(S));
        }
        if ((defInstrs->find(A) != defInstrs->end())) {
            //assert dynamic_cast<INSTR::Store*>(A) != nullptr || dynamic_cast<INSTR::Phi*>(A) != nullptr;
            if (dynamic_cast<INSTR::Store*>(A) != nullptr) {
                S->push(((INSTR::Store*) A)->getValue());
                //A->remove();
            } else {
                S->push(A);
            }
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
            if ((useInstrs->find(instr) != useInstrs->end())) {
                //instr->modifyUse(S->peek(), bb->precBBs->indexOf(X));
                int index = std::find(bb->precBBs->begin(), bb->precBBs->end(), X) - bb->precBBs->begin();
                // from bb->precBBs->indexOf(X)
                instr->modifyUse(getStackTopValue(S), index);

                //instr->remove();
            }
            instr = (Instr*) instr->next;
        }
    }

    for (BasicBlock* next: (*X->idoms)) {
        RenameDFS(S, next, useInstrs, defInstrs);
    }

    for (int i = 0; i < cnt; i++) {
        S->pop();
    }
}

BasicBlock* Mem2Reg::getRandFromHashSet(std::set<BasicBlock*>* hashSet) {
    for (BasicBlock* bb: (*hashSet)) {
        return bb;
    }
    return nullptr;
}

Value* Mem2Reg::getStackTopValue(std::stack<Value*>* S) {
    if (S->empty()) {
        return new UndefValue();
    }
    return S->top();
}


