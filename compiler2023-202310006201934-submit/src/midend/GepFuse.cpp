//
// Created by start on 23-7-29.
//

#include "../../include/midend/GepFuse.h"
#include "../../include/util/CenterControl.h"




//考虑不能融合的情况?
//TODO:是否存在不能融合的情况?

GepFuse::GepFuse(std::vector<Function*>* functions) {
    this->functions = functions;
}

void GepFuse::Run() {
    gepFuse();
}

void GepFuse::gepFuse() {
    for (Function* function: *functions) {
        gepFuseForFunc(function);
    }
}

void GepFuse::gepFuseForFunc(Function* function) {
    //删除冗余GEP
    for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
        for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
            if (dynamic_cast<INSTR::GetElementPtr*>(instr) != nullptr && isZeroOffsetGep((INSTR::GetElementPtr*) instr)) {
                instr->modifyAllUseThisToUseA(((INSTR::GetElementPtr*) instr)->getPtr());
            }
        }
    }

    //GEP融合
    for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
        for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
            if (dynamic_cast<INSTR::GetElementPtr*>(instr) != nullptr &&
                dynamic_cast<INSTR::GetElementPtr *>(((INSTR::GetElementPtr *) instr)->getPtr()) == nullptr) {
                fuseDFS(instr);
            }
        }
    }

}

bool GepFuse::isZeroOffsetGep(INSTR::GetElementPtr* gep) {
    std::vector<Value*>* values = gep->getIdxList();
    if (values->size() > 1) {
        return false;
    }
    for (Value* value: *values) {
        if (!(dynamic_cast<Constant*>(value) != nullptr)) {
            return false;
        }
        int val = (int) ((ConstantInt*) value)->get_const_val();
        if (val != 0) {
            return false;
        }
    }
    return true;
}

void  GepFuse::fuseDFS(Instr* instr) {
    Geps->push_back((INSTR::GetElementPtr*) instr);
    std::set<Use*>* uses = new std::set<Use*>();
    for (Use* use = instr->getBeginUse(); use->next != nullptr; use = (Use*) use->next) {
        uses->insert(use);
    }
    bool tag = false;
    for (Use* use: *uses) {
        //此处user use并没有被维护
        if (dynamic_cast<INSTR::GetElementPtr*>(use->user) != nullptr) {
            fuseDFS(use->user);
        } else if (dynamic_cast<INSTR::Store*>(use->user) != nullptr ||
                dynamic_cast<INSTR::Load*>(use->user) != nullptr ||
                dynamic_cast<INSTR::Call*>(use->user) != nullptr) {
            if (!tag) {
                fuse();
                tag = true;
            }
        }
    }
    Geps->erase(Geps->begin() + (Geps->size() - 1));
}

void GepFuse::fuse() {
    if (Geps->size() == 1) {
        return;
    }
    int begin_index = Geps->size() - 1;
    while (begin_index > 0) {
        if ((*Geps)[begin_index]->getPtr() == (Geps->at(begin_index - 1))) {
            begin_index--;
        } else {
            break;
        }
    }
    INSTR::GetElementPtr* gep_0 = (*Geps)[begin_index];
    INSTR::GetElementPtr* gep_end = Geps->at(Geps->size() - 1);
    int dim = ((ArrayType*) ((PointerType*) (*Geps)[0]->getPtr()->type)->inner_type)->getDimSize();
    int sum = 0;
    for (int i = begin_index; i < Geps->size(); i++) {
        //TODO:在过程中发现偏移相等了
        INSTR::GetElementPtr* gep = (*Geps)[i];
        sum += gep->getIdxList()->size() - 1;
    }
    //assert sum <= dim;
    if (sum < dim) {
        return;
    }


    std::vector<Value*>* retIndexs = new std::vector<Value*>();
//    retIndexs->insertAll(gep_0->getIdxList());
    for (auto t: (*gep_0->getIdxList())) {
        retIndexs->push_back(t);
    }


    for (int i = begin_index + 1; i < Geps->size(); i++) {
        INSTR::GetElementPtr* gep = (*Geps)[i];

        int num = retIndexs->size();
        Value* nowEndIndex = retIndexs->at(num - 1);
        Value* baseOffset = (*gep->getIdxList())[0];

        if (dynamic_cast<Constant*>(nowEndIndex) != nullptr && dynamic_cast<Constant*>(baseOffset) != nullptr) {
            int A = (int) ((ConstantInt*) nowEndIndex)->get_const_val();
            int B = (int) ((ConstantInt*) baseOffset)->get_const_val();
            //gep_0->modifyUse(new ConstantInt(A + B), num);
            (*retIndexs)[num - 1] = new ConstantInt(A + B);
        } else {
            if (dynamic_cast<Constant*>(nowEndIndex) != nullptr) {
                int A = (int) ((ConstantInt*) nowEndIndex)->get_const_val();
                if (A != 0) {
                    return;
                }
                //gep_0->modifyUse(baseOffset, num);
                (*retIndexs)[num - 1] = baseOffset;
            } else if (dynamic_cast<Constant*>(baseOffset) != nullptr) {
                int B = (int) ((ConstantInt*) baseOffset)->get_const_val();
                if (B != 0) {
                    return;
                }
            } else {
                return;
            }
        }

        for (int j = 1; j < gep->getIdxList()->size(); j++) {
            //gep_0->insertIdx(gep->getIdxList()[j]);
            retIndexs->push_back((*gep->getIdxList())[j]);
        }
    }
    //Geps->get(Geps->size() - 1)->modifyAllUseThisToUseA(gep_0);
    //gep_0->delFromNowBB();
    //Geps->get(Geps->size() - 1)->insertAfter(gep_0);
    //Geps[0]->modifyType(Geps->get(Geps->size() - 1)->getType());
    gep_end->modifyPtr(gep_0->getPtr());
    gep_end->modifyIndexs(retIndexs);
}
