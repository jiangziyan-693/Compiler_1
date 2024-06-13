#include "../../include/midend/MathOptimize.h"
#include "../../include/util/CenterControl.h"
#include "stl_op.h"





//%1 = mul %2, 100
// key ==> %1
// value ==> 100

MathOptimize::MathOptimize(std::vector<Function *> *functions) {
    this->functions = functions;
}

void MathOptimize::Run() {
    continueAddToMul();
    continueFAddToMul();
    addZeroFold();
    faddZeroFold();
    mulDivFold();
    fmulFDivFold();
}

void MathOptimize::continueAddToMul() {
    for (Function *function: *functions) {
        BasicBlock *bb = function->getBeginBB();
        while (bb->next != nullptr) {
            Instr *instr = bb->getBeginInstr();

            while (instr->next != nullptr) {
                continueAddToMulForInstr(instr);
                instr = (Instr *) instr->next;
            }

            bb = (BasicBlock *) bb->next;
        }
    }
}

void MathOptimize::continueFAddToMul() {
    for (Function *function: *functions) {
        for (BasicBlock *bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock *) bb->next) {
            for (Instr *instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr *) instr->next) {
                continueFAddToMulForInstr(instr);
            }
        }
    }
}

void MathOptimize::continueFAddToMulForInstr(Instr *instr) {
//    if (instr->name == ("%v300086")) {
//        int a = 0;
//    }
    int cnt = 0;
    Instr *beginInstr = instr;
    if (!(dynamic_cast<INSTR::Alu *>(instr) != nullptr)) {
        return;
    }
    //连加的启动,能否启动
    Value *rhs1 = instr->useValueList[0];
    Value *rhs2 = instr->useValueList[1];
    Value *base = nullptr;
    Value *addValue = nullptr;
    if (faddCanTransToMul(instr, rhs1)) {
        base = rhs1;
        addValue = rhs2;
    } else if (faddCanTransToMul(instr, rhs2)) {
        base = rhs2;
        addValue = rhs1;
    } else {
        return;
    }

    std::vector<Instr *> *adds = new std::vector<Instr *>();
    adds->push_back(instr);
    cnt = 1;
    while (faddCanTransToMul(instr, base)) {
        adds->push_back(getOneUser(instr));
        instr = (Instr *) instr->next;
        cnt++;
    }

    BasicBlock *bb = instr->bb;
    if (cnt > adds_to_mul_boundary) {
        //TODO:do trans *adds -> mul
        if ((addValue == base)) {
            cnt++;
            ((INSTR::Alu *) instr)->op = (INSTR::Alu::Op::FMUL);
            instr->modifyUse(base, 0);
            instr->modifyUse(new ConstantFloat(cnt), 1);
            for (int i = 0; i < adds->size() - 1; i++) {
                (*adds)[i]->remove();
            }
            beginInstr->next = (instr->next);
        } else {
            INSTR::Alu *mul = new INSTR::Alu(instr->type, INSTR::Alu::Op::FMUL, base, new ConstantFloat(cnt), bb);
            instr->insert_before(mul);
            instr->modifyUse(mul, 0);
            instr->modifyUse(addValue, 1);

            for (int i = 0; i < adds->size() - 1; i++) {
                (*adds)[i]->remove();
            }
            beginInstr->next = (instr->next);
        }
    }
}

bool MathOptimize::faddCanTransToMul(Instr *instr, Value *value) {
    if (!(dynamic_cast<INSTR::Alu *>(instr) != nullptr && (((INSTR::Alu *) instr)->op == INSTR::Alu::Op::FADD))) {
        return false;
    }
    //TODO:似乎不是必要,可以考虑一下如何找到可以归越成多个乘法的多个连加
    if (!instr->onlyOneUser()) {
        return false;
    }
    Use *use = instr->getBeginUse();
    Instr *user = use->user;
    if (!(dynamic_cast<INSTR::Alu *>(user) != nullptr && (((INSTR::Alu *) user)->op == INSTR::Alu::Op::FADD))) {
        return false;
    }
    if (dynamic_cast<ConstantFloat *>(value) != nullptr) {
        int index = 1 - _index_of((&user->useValueList), instr);
        float val1 = (float) ((ConstantFloat *) value)->get_const_val();
        Value *value2 = user->useValueList[index];
        if (dynamic_cast<ConstantFloat *>(value2) != nullptr) {
            float val2 = (float) ((ConstantFloat *) value2)->get_const_val();
            return val1 == val2;
        } else {
            return false;
        }
    }
    return (std::find(user->useValueList.begin(), user->useValueList.end(), value) != user->useValueList.end());
}

void MathOptimize::continueAddToMulForInstr(Instr *instr) {
    int cnt = 0;
    Instr *beginInstr = instr;
    if (!(dynamic_cast<INSTR::Alu *>(instr) != nullptr)) {
        return;
    }
    //连加的启动,能否启动
    Value *rhs1 = instr->useValueList[0];
    Value *rhs2 = instr->useValueList[1];
    Value *base = nullptr;
    Value *addValue = nullptr;
    if (addCanTransToMul(instr, rhs1)) {
        base = rhs1;
        addValue = rhs2;
    } else if (addCanTransToMul(instr, rhs2)) {
        base = rhs2;
        addValue = rhs1;
    } else {
        return;
    }

    std::vector<Instr *> *adds = new std::vector<Instr *>();
    adds->push_back(instr);
    cnt = 1;
    while (addCanTransToMul(instr, base)) {
        adds->push_back(getOneUser(instr));
        instr = (Instr *) instr->next;
        cnt++;
    }

    BasicBlock *bb = instr->bb;
    if (cnt > adds_to_mul_boundary) {
        //TODO:do trans *adds -> mul
        if ((addValue == base)) {
            cnt++;

            ((INSTR::Alu *) instr)->op = (INSTR::Alu::Op::MUL);
            instr->modifyUse(base, 0);
            instr->modifyUse(new ConstantInt(cnt), 1);
            for (int i = 0; i < adds->size() - 1; i++) {
                (*adds)[i]->remove();
            }

            beginInstr->next = (instr->next);
            return;
        } else {
            INSTR::Alu *mul = new INSTR::Alu(instr->type, INSTR::Alu::Op::MUL, base, new ConstantInt(cnt), bb);
            instr->insert_before(mul);
            instr->modifyUse(mul, 0);
            instr->modifyUse(addValue, 1);

            for (int i = 0; i < adds->size() - 1; i++) {
                (*adds)[i]->remove();
            }

            beginInstr->next = (instr->next);
            return;
        }
    }
}

bool MathOptimize::addCanTransToMul(Instr *instr, Value *value) {
    if (!(dynamic_cast<INSTR::Alu *>(instr) != nullptr && (((INSTR::Alu *) instr)->op == INSTR::Alu::Op::ADD))) {
        return false;
    }
    //TODO:似乎不是必要,可以考虑一下如何找到可以归越成多个乘法的多个连加
    if (!instr->onlyOneUser()) {
        return false;
    }
    Use *use = instr->getBeginUse();
    Instr *user = use->user;
    if (!(dynamic_cast<INSTR::Alu *>(user) != nullptr && (((INSTR::Alu *) user)->op == INSTR::Alu::Op::ADD))) {
        return false;
    }
    if (dynamic_cast<ConstantInt *>(value) != nullptr) {
        int index = 1 - _index_of((&user->useValueList), instr);
        int val1 = (int) ((ConstantInt *) value)->get_const_val();
        Value *value2 = user->useValueList[index];
        if (dynamic_cast<ConstantInt *>(value2) != nullptr) {
            int val2 = (int) ((ConstantInt *) value2)->get_const_val();
            return val1 == val2;
        } else {
            return false;
        }
    }
    return (std::find(user->useValueList.begin(), user->useValueList.end(), value) != user->useValueList.end());
}

//只对 只有一个user的指令保证正确
Instr *MathOptimize::getOneUser(Instr *instr) {
    return instr->getBeginUse()->user;
}

void MathOptimize::addZeroFold() {
    for (Function *function: *functions) {
        for (BasicBlock *bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock *) bb->next) {
            for (Instr *instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr *) instr->next) {
                if (dynamic_cast<INSTR::Alu *>(instr) != nullptr &&
                    (((INSTR::Alu *) instr)->op == INSTR::Alu::Op::ADD)) {
                    if (dynamic_cast<ConstantInt *>(((INSTR::Alu *) instr)->getRVal1()) != nullptr) {
                        int val = (int) ((ConstantInt *) ((INSTR::Alu *) instr)->getRVal1())->get_const_val();
                        if (val == 0) {
                            instr->modifyAllUseThisToUseA(((INSTR::Alu *) instr)->getRVal2());
                            instr->remove();
                        }
                    } else if (dynamic_cast<ConstantInt *>(((INSTR::Alu *) instr)->getRVal2()) != nullptr) {
                        int val = (int) ((ConstantInt *) ((INSTR::Alu *) instr)->getRVal2())->get_const_val();
                        if (val == 0) {
                            instr->modifyAllUseThisToUseA(((INSTR::Alu *) instr)->getRVal1());
                            instr->remove();
                        }
                    }
                }
            }
        }
    }
}

void MathOptimize::mulDivFold() {
    mulMap->clear();
    for (Function *function: *functions) {
        BasicBlock *bb = function->getBeginBB();
        RPOSearch(bb);
    }
}

void MathOptimize::RPOSearch(BasicBlock *bb) {
    std::set<Instr *> *adds = new std::set<Instr *>();

    for (Instr *instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr *) instr->next) {
        if (dynamic_cast<INSTR::Alu *>(instr) != nullptr) {
            INSTR::Alu::Op op = ((INSTR::Alu *) instr)->op;
            if ((op == INSTR::Alu::Op::MUL)) {
                Value *val1 = ((INSTR::Alu *) instr)->getRVal1();
                Value *val2 = ((INSTR::Alu *) instr)->getRVal2();
                if (dynamic_cast<ConstantInt *>(val1) != nullptr) {
                    int time = (int) ((ConstantInt *) val1)->get_const_val();
                    adds->insert(instr);
                    (*mulMap)[instr] = time;
                } else if (dynamic_cast<ConstantInt *>(val2) != nullptr) {
                    int time = (int) ((ConstantInt *) val2)->get_const_val();
                    adds->insert(instr);
                    (*mulMap)[instr] = time;
                }
            } else if ((op == INSTR::Alu::Op::DIV)) {
                if (dynamic_cast<ConstantInt *>(((INSTR::Alu *) instr)->getRVal2()) != nullptr) {
                    int divTime = (int) ((ConstantInt *) ((INSTR::Alu *) instr)->getRVal2())->get_const_val();
                    if (dynamic_cast<Instr *>(((INSTR::Alu *) instr)->getRVal1()) != nullptr) {
                        Instr *val = (Instr *) ((INSTR::Alu *) instr)->getRVal1();
                        if ((mulMap->find(val) != mulMap->end()) && (*mulMap)[val] == divTime) {
                            //assert dynamic_cast<INSTR::Alu*>(val) != nullptr;
                            if (dynamic_cast<ConstantInt *>(((INSTR::Alu *) val)->getRVal1()) != nullptr) {
                                instr->modifyAllUseThisToUseA(((INSTR::Alu *) val)->getRVal2());
                                instr->remove();
                            } else if (dynamic_cast<ConstantInt *>(((INSTR::Alu *) val)->getRVal2()) != nullptr) {
                                instr->modifyAllUseThisToUseA(((INSTR::Alu *) val)->getRVal1());
                                instr->remove();
                            } else {
                                //assert false;
                            }
                        }
                    }
                }
            }
        }
    }

    for (BasicBlock *next: *bb->idoms) {
        RPOSearch(next);
    }

    for (Instr *instr: *adds) {
        mulMap->erase(instr);
    }


}


void MathOptimize::faddZeroFold() {
    for (Function *function: *functions) {
        for (BasicBlock *bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock *) bb->next) {
            for (Instr *instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr *) instr->next) {
                if (dynamic_cast<INSTR::Alu *>(instr) != nullptr &&
                    (((INSTR::Alu *) instr)->op == INSTR::Alu::Op::FADD)) {
                    if (dynamic_cast<ConstantFloat *>(((INSTR::Alu *) instr)->getRVal1()) != nullptr) {
                        ConstantFloat *val = ((ConstantFloat *) ((INSTR::Alu *) instr)->getRVal1());
                        if (val->get_const_val() == 0.0) {
                            // TODO: val->isZero()
                            instr->modifyAllUseThisToUseA(((INSTR::Alu *) instr)->getRVal2());
                            instr->remove();
                        }
                    } else if (dynamic_cast<ConstantFloat *>(((INSTR::Alu *) instr)->getRVal2()) != nullptr) {
                        ConstantFloat *val = ((ConstantFloat *) ((INSTR::Alu *) instr)->getRVal2());
                        if (val->get_const_val() == 0.0) {
                            //TODO: val->isZero()
                            instr->modifyAllUseThisToUseA(((INSTR::Alu *) instr)->getRVal1());
                            instr->remove();
                        }
                    }
                }
            }
        }
    }
}

void MathOptimize::fmulFDivFold() {
    fmulMap->clear();
    for (Function *function: *functions) {
        BasicBlock *bb = function->getBeginBB();
        RPOSearchFloat(bb);
    }
}


void MathOptimize::RPOSearchFloat(BasicBlock *bb) {
    std::set<Instr *> *adds = new std::set<Instr *>();

    for (Instr *instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr *) instr->next) {
        if (dynamic_cast<INSTR::Alu *>(instr) != nullptr) {
            INSTR::Alu::Op op = ((INSTR::Alu *) instr)->op;
            if ((op == INSTR::Alu::Op::FMUL)) {
                Value *val1 = ((INSTR::Alu *) instr)->getRVal1();
                Value *val2 = ((INSTR::Alu *) instr)->getRVal2();
                if (dynamic_cast<ConstantFloat *>(val1) != nullptr) {
                    adds->insert(instr);
                    (*fmulMap)[instr] = val1->getName();
                } else if (dynamic_cast<ConstantFloat *>(val2) != nullptr) {
                    adds->insert(instr);
                    (*fmulMap)[instr] = val2->getName();
                }
            } else if ((op == INSTR::Alu::Op::FDIV)) {
                if (dynamic_cast<ConstantFloat *>(((INSTR::Alu *) instr)->getRVal2()) != nullptr) {
                    std::string divTime = ((INSTR::Alu *) instr)->getRVal2()->getName();
                    if (dynamic_cast<Instr *>(((INSTR::Alu *) instr)->getRVal1()) != nullptr) {
                        Instr *val = (Instr *) ((INSTR::Alu *) instr)->getRVal1();
                        if ((fmulMap->find(val) != fmulMap->end()) && ((*fmulMap)[val] == divTime)) {
                            //assert dynamic_cast<INSTR::Alu*>(val) != nullptr;
                            if (dynamic_cast<ConstantFloat *>(((INSTR::Alu *) val)->getRVal1()) != nullptr) {
                                instr->modifyAllUseThisToUseA(((INSTR::Alu *) val)->getRVal2());
                                instr->remove();
                            } else if (dynamic_cast<ConstantFloat *>(((INSTR::Alu *) val)->getRVal2()) != nullptr) {
                                instr->modifyAllUseThisToUseA(((INSTR::Alu *) val)->getRVal1());
                                instr->remove();
                            } else {
                                //assert false;
                            }
                        }
                    }
                }
            }
        }
    }

    for (BasicBlock *next: *bb->idoms) {
        RPOSearchFloat(next);
    }

    for (Instr *instr: *adds) {
        fmulMap->erase(instr);
    }


}


