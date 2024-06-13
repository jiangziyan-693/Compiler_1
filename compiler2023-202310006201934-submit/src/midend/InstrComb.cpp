#include "../../include/midend/InstrComb.h"
#include "../../include/util/CenterControl.h"




    //TODO:把乘法 除法 取模也纳入考虑

	InstrComb::InstrComb(std::vector<Function*>* functions) {
        this->functions = functions;
    }

    void InstrComb::Run() {
        for (Function* function: *functions) {
            InstrCombForFunc(function);
        }
    }

    void InstrComb::InstrCombForFunc(Function* function) {
        BasicBlock* bb = function->getBeginBB();
        while (bb->next != nullptr) {
//            if (bb->label == "b10") {
//                printf("in b 10\n");
//            }
            Instr* instr = bb->getBeginInstr();
            while (instr->next != nullptr) {
                if (instr->canComb())  {
                    std::vector<INSTR::Alu*>* tags = new std::vector<INSTR::Alu*>();
                    Use* use = instr->getBeginUse();
                    while (use->next != nullptr) {
// dynamic_cast<INSTR::Alu*>(assert use->user) != nullptr;
                        tags->push_back((INSTR::Alu*) use->user);
                        use = (Use*) use->next;
                    }
                    //assert dynamic_cast<INSTR::Alu*>(instr) != nullptr;
                    combSrcToTags((INSTR::Alu*) instr, tags);
                    instr->remove();
                } else if (instr->canCombFloat()) {
                    std::vector<INSTR::Alu*>* tags = new std::vector<INSTR::Alu*>();
                    Use* use = instr->getBeginUse();
                    while (use->next != nullptr) {
// dynamic_cast<INSTR::Alu*>(assert use->user) != nullptr;
                        tags->push_back((INSTR::Alu*) use->user);
                        use = (Use*) use->next;
                    }
                    //assert dynamic_cast<INSTR::Alu*>(instr) != nullptr;
                    combSrcToTagsFloat((INSTR::Alu*) instr, tags);
                    instr->remove();
                }
                instr = (Instr*) instr->next;
            }
            bb = (BasicBlock*) bb->next;
        }
    }

    void InstrComb::combSrcToTags(INSTR::Alu* src, std::vector<INSTR::Alu*>* tags) {
        for (INSTR::Alu* alu: *tags) {
            combAToB(src, alu);
        }
    }

    void InstrComb::combSrcToTagsFloat(INSTR::Alu* src, std::vector<INSTR::Alu*>* tags) {
        for (INSTR::Alu* alu: *tags) {
            combAToBFloat(src, alu);
        }
    }

    void InstrComb::combSrcToTagsMul(INSTR::Alu* src, std::vector<INSTR::Alu*>* tags) {
        for (INSTR::Alu* alu: *tags) {
            combAToBMul(src, alu);
        }
    }


    void InstrComb::combSrcToTagsMulFloat(INSTR::Alu* src, std::vector<INSTR::Alu*>* tags) {
        for (INSTR::Alu* alu: *tags) {
            combAToBMulFloat(src, alu);
        }
    }

    void InstrComb::combAToB(INSTR::Alu* A, INSTR::Alu* B) {
        std::vector<Value*>* AUseList = &A->useValueList;
        std::vector<Value*>* BUseList = &B->useValueList;
        int ConstInA = 0, ConstInB = 0;
        bool ConstInAIs0 = false, ConstInBIs0 = false;
        if (dynamic_cast<Constant*>((*AUseList)[0]) != nullptr) {
            ConstInA = (int) ((ConstantInt*) (*AUseList)[0])->get_const_val();
            ConstInAIs0 = true;
        } else {
            ConstInA = (int) ((ConstantInt*) (*AUseList)[1])->get_const_val();
        }

        if (dynamic_cast<Constant*>((*BUseList)[0]) != nullptr) {
            ConstInB = (int) ((ConstantInt*) (*BUseList)[0])->get_const_val();
            ConstInBIs0 = true;
        } else {
            ConstInB = (int) ((ConstantInt*) (*BUseList)[1])->get_const_val();
        }
        if (ConstInAIs0 && ConstInBIs0 && A->isAdd() && B->isAdd()) {
            ConstantInt* constantInt = new ConstantInt(ConstInA + ConstInB);
            B->modifyUse(constantInt, 0);
            B->modifyUse((*AUseList)[1], 1);
            //A->remove();
        }
        else if (ConstInAIs0 && ConstInBIs0 && A->isAdd() && !B->isAdd()) {
            ConstantInt* constantInt = new ConstantInt(ConstInB - ConstInA);
            B->modifyUse(constantInt, 0);
            B->modifyUse((*AUseList)[1], 1);
            //A->remove();
        }
        else if (ConstInAIs0 && ConstInBIs0 && !A->isAdd() && B->isAdd()) {
            ConstantInt* constantInt = new ConstantInt(ConstInB + ConstInA);
            B->modifyUse(constantInt, 0);
            B->modifyUse((*AUseList)[1], 1);
            B->op = (INSTR::Alu::Op::SUB);
            //A->remove();
        }
        else if (ConstInAIs0 && ConstInBIs0 && !A->isAdd() && !B->isAdd()) {
            ConstantInt* constantInt = new ConstantInt(ConstInB - ConstInA);
            B->modifyUse(constantInt, 0);
            B->modifyUse((*AUseList)[1], 1);
            B->op = (INSTR::Alu::Op::ADD);
            //A->remove();
        }


        else if (ConstInAIs0 && !ConstInBIs0 && A->isAdd() && B->isAdd()) {
            ConstantInt* constantInt = new ConstantInt(ConstInA + ConstInB);
            B->modifyUse(constantInt, 1);
            B->modifyUse((*AUseList)[1], 0);
            //A->remove();
        }
        else if (ConstInAIs0 && !ConstInBIs0 && A->isAdd() && !B->isAdd()) {
            ConstantInt* constantInt = new ConstantInt(ConstInA - ConstInB);
            B->modifyUse(constantInt, 1);
            B->modifyUse((*AUseList)[1], 0);
            B->op = (INSTR::Alu::Op::ADD);
            //A->remove();
        }
        //b = x - a; c = b + y; c = (x+y) - a
        else if (ConstInAIs0 && !ConstInBIs0 && !A->isAdd() && B->isAdd()) {
            ConstantInt* constantInt = new ConstantInt(ConstInA + ConstInB);
            B->modifyUse(constantInt, 0);
            B->modifyUse((*AUseList)[1], 1);
            B->op = (INSTR::Alu::Op::SUB);
            //A->remove();
        }
        //b = x - a; c = b - y ==> c = (x - y) - a
        else if (ConstInAIs0 && !ConstInBIs0 && !A->isAdd() && !B->isAdd()) {
            ConstantInt* constantInt = new ConstantInt(ConstInA - ConstInB);
            B->modifyUse(constantInt, 0);
            B->modifyUse((*AUseList)[1], 1);
            //A->remove();
        }



        else if (!ConstInAIs0 && ConstInBIs0 && A->isAdd() && B->isAdd()) {
            ConstantInt* constantInt = new ConstantInt(ConstInA + ConstInB);
            B->modifyUse(constantInt, 0);
            B->modifyUse((*AUseList)[0], 1);
            //A->remove();
        }
        else if (!ConstInAIs0 && ConstInBIs0 && A->isAdd() && !B->isAdd()) {
            ConstantInt* constantInt = new ConstantInt(ConstInB - ConstInA);
            B->modifyUse(constantInt, 0);
            B->modifyUse((*AUseList)[0], 1);
            //A->remove();
        }
        else if (!ConstInAIs0 && ConstInBIs0 && !A->isAdd() && B->isAdd()) {
            ConstantInt* constantInt = new ConstantInt(ConstInB - ConstInA);
            B->modifyUse(constantInt, 0);
            B->modifyUse((*AUseList)[0], 1);
            //A->remove();
        }
        else if (!ConstInAIs0 && ConstInBIs0 && !A->isAdd() && !B->isAdd()) {
            ConstantInt* constantInt = new ConstantInt(ConstInB + ConstInA);
            B->modifyUse(constantInt, 0);
            B->modifyUse((*AUseList)[0], 1);
            //A->remove();
        }

        else if (!ConstInAIs0 && !ConstInBIs0 && A->isAdd() && B->isAdd()) {
            ConstantInt* constantInt = new ConstantInt(ConstInA + ConstInB);
            B->modifyUse(constantInt, 0);
            B->modifyUse((*AUseList)[0], 1);
            //A->remove();
        }
        else if (!ConstInAIs0 && !ConstInBIs0 && A->isAdd() && !B->isAdd()) {
            ConstantInt* constantInt = new ConstantInt(ConstInA - ConstInB);
            B->modifyUse(constantInt, 0);
            B->modifyUse((*AUseList)[0], 1);
            B->op = (INSTR::Alu::Op::ADD);
            //A->remove();
        }
        else if (!ConstInAIs0 && !ConstInBIs0 && !A->isAdd() && B->isAdd()) {
            ConstantInt* constantInt = new ConstantInt(ConstInB - ConstInA);
            B->modifyUse(constantInt, 0);
            B->modifyUse((*AUseList)[0], 1);
            //A->remove();
        }

        //b = a - x; c = b - y; c = a - (x + y)
        else if (!ConstInAIs0 && !ConstInBIs0 && !A->isAdd() && !B->isAdd()) {
            ConstantInt* constantInt = new ConstantInt(-ConstInB - ConstInA);
            B->modifyUse(constantInt, 0);
            B->modifyUse((*AUseList)[0], 1);
            B->op = (INSTR::Alu::Op::ADD);
            //A->remove();
        }
    }

    void InstrComb::combAToBFloat(INSTR::Alu* A, INSTR::Alu* B) {
        std::vector<Value*>* AUseList = &A->useValueList;
        std::vector<Value*>* BUseList = &B->useValueList;
        float ConstInA = 0, ConstInB = 0;
        bool ConstInAIs0 = false, ConstInBIs0 = false;
        if (dynamic_cast<Constant*>((*AUseList)[0]) != nullptr) {
            ConstInA = (float) ((ConstantFloat*) (*AUseList)[0])->get_const_val();
            ConstInAIs0 = true;
        } else {
            ConstInA = (float) ((ConstantFloat*) (*AUseList)[1])->get_const_val();
        }

        if (dynamic_cast<Constant*>((*BUseList)[0]) != nullptr) {
            ConstInB = (float) ((ConstantFloat*) (*BUseList)[0])->get_const_val();
            ConstInBIs0 = true;
        } else {
            ConstInB = (float) ((ConstantFloat*) (*BUseList)[1])->get_const_val();
        }
        if (ConstInAIs0 && ConstInBIs0 && A->isFAdd() && B->isFAdd()) {
            ConstantFloat* constantFloat = new ConstantFloat(ConstInA + ConstInB);
            B->modifyUse(constantFloat, 0);
            B->modifyUse((*AUseList)[1], 1);
            //A->remove();
        }
        else if (ConstInAIs0 && ConstInBIs0 && A->isFAdd() && !B->isFAdd()) {
            ConstantFloat* constantFloat = new ConstantFloat(ConstInB - ConstInA);
            B->modifyUse(constantFloat, 0);
            B->modifyUse((*AUseList)[1], 1);
            //A->remove();
        }
        else if (ConstInAIs0 && ConstInBIs0 && !A->isFAdd() && B->isFAdd()) {
            ConstantFloat* constantFloat = new ConstantFloat(ConstInB + ConstInA);
            B->modifyUse(constantFloat, 0);
            B->modifyUse((*AUseList)[1], 1);
            B->op = (INSTR::Alu::Op::FSUB);
            //A->remove();
        }
        else if (ConstInAIs0 && ConstInBIs0 && !A->isFAdd() && !B->isFAdd()) {
            ConstantFloat* constantFloat = new ConstantFloat(ConstInB - ConstInA);
            B->modifyUse(constantFloat, 0);
            B->modifyUse((*AUseList)[1], 1);
            B->op = (INSTR::Alu::Op::FADD);
            //A->remove();
        }


        else if (ConstInAIs0 && !ConstInBIs0 && A->isFAdd() && B->isFAdd()) {
            ConstantFloat* constantFloat = new ConstantFloat(ConstInA + ConstInB);
            B->modifyUse(constantFloat, 1);
            B->modifyUse((*AUseList)[1], 0);
            //A->remove();
        }
        else if (ConstInAIs0 && !ConstInBIs0 && A->isFAdd() && !B->isFAdd()) {
            ConstantFloat* constantFloat = new ConstantFloat(ConstInA - ConstInB);
            B->modifyUse(constantFloat, 1);
            B->modifyUse((*AUseList)[1], 0);
            B->op = (INSTR::Alu::Op::FADD);
            //A->remove();
        }
        //b = x - a; c = b + y; c = (x+y) - a
        else if (ConstInAIs0 && !ConstInBIs0 && !A->isFAdd() && B->isFAdd()) {
            ConstantFloat* constantFloat = new ConstantFloat(ConstInA + ConstInB);
            B->modifyUse(constantFloat, 0);
            B->modifyUse((*AUseList)[1], 1);
            B->op = (INSTR::Alu::Op::FSUB);
            //A->remove();
        }
        //b = x - a; c = b - y ==> c = (x - y) - a
        else if (ConstInAIs0 && !ConstInBIs0 && !A->isFAdd() && !B->isFAdd()) {
            ConstantFloat* constantFloat = new ConstantFloat(ConstInA - ConstInB);
            B->modifyUse(constantFloat, 0);
            B->modifyUse((*AUseList)[1], 1);
            //A->remove();
        }



        else if (!ConstInAIs0 && ConstInBIs0 && A->isFAdd() && B->isFAdd()) {
            ConstantFloat* constantFloat = new ConstantFloat(ConstInA + ConstInB);
            B->modifyUse(constantFloat, 0);
            B->modifyUse((*AUseList)[0], 1);
            //A->remove();
        }
        else if (!ConstInAIs0 && ConstInBIs0 && A->isFAdd() && !B->isFAdd()) {
            ConstantFloat* constantFloat = new ConstantFloat(ConstInB - ConstInA);
            B->modifyUse(constantFloat, 0);
            B->modifyUse((*AUseList)[0], 1);
            //A->remove();
        }
        else if (!ConstInAIs0 && ConstInBIs0 && !A->isFAdd() && B->isFAdd()) {
            ConstantFloat* constantFloat = new ConstantFloat(ConstInB - ConstInA);
            B->modifyUse(constantFloat, 0);
            B->modifyUse((*AUseList)[0], 1);
            //A->remove();
        }
        else if (!ConstInAIs0 && ConstInBIs0 && !A->isFAdd() && !B->isFAdd()) {
            ConstantFloat* constantFloat = new ConstantFloat(ConstInB + ConstInA);
            B->modifyUse(constantFloat, 0);
            B->modifyUse((*AUseList)[0], 1);
            //A->remove();
        }

        else if (!ConstInAIs0 && !ConstInBIs0 && A->isFAdd() && B->isFAdd()) {
            ConstantFloat* constantFloat = new ConstantFloat(ConstInA + ConstInB);
            B->modifyUse(constantFloat, 0);
            B->modifyUse((*AUseList)[0], 1);
            //A->remove();
        }
        else if (!ConstInAIs0 && !ConstInBIs0 && A->isFAdd() && !B->isFAdd()) {
            ConstantFloat* constantFloat = new ConstantFloat(ConstInA - ConstInB);
            B->modifyUse(constantFloat, 0);
            B->modifyUse((*AUseList)[0], 1);
            B->op = (INSTR::Alu::Op::FADD);
            //A->remove();
        }
        else if (!ConstInAIs0 && !ConstInBIs0 && !A->isFAdd() && B->isFAdd()) {
            ConstantFloat* constantFloat = new ConstantFloat(ConstInB - ConstInA);
            B->modifyUse(constantFloat, 0);
            B->modifyUse((*AUseList)[0], 1);
            //A->remove();
        }

        //b = a - x; c = b - y; c = a - (x + y)
        else if (!ConstInAIs0 && !ConstInBIs0 && !A->isFAdd() && !B->isFAdd()) {
            ConstantFloat* constantFloat = new ConstantFloat(-ConstInB - ConstInA);
            B->modifyUse(constantFloat, 0);
            B->modifyUse((*AUseList)[0], 1);
            B->op = (INSTR::Alu::Op::FADD);
            //A->remove();
        }
    }


    void InstrComb::combAToBMul(INSTR::Alu* A, INSTR::Alu* B) {
        std::vector<Value*>* AUseList = &A->useValueList;
        std::vector<Value*>* BUseList = &B->useValueList;
        int ConstInA = 0, ConstInB = 0;
        bool ConstInAIs0 = false, ConstInBIs0 = false;
        if (dynamic_cast<Constant*>((*AUseList)[0]) != nullptr) {
            ConstInA = (int) ((ConstantInt*) (*AUseList)[0])->get_const_val();
            ConstInAIs0 = true;
        } else {
            ConstInA = (int) ((ConstantInt*) (*AUseList)[1])->get_const_val();
        }

        if (dynamic_cast<Constant*>((*BUseList)[0]) != nullptr) {
            ConstInB = (int) ((ConstantInt*) (*BUseList)[0])->get_const_val();
            ConstInBIs0 = true;
        } else {
            ConstInB = (int) ((ConstantInt*) (*BUseList)[1])->get_const_val();
        }

        ConstantInt* constantInt = new ConstantInt(ConstInA * ConstInB);
        if (ConstInAIs0 && ConstInBIs0) {
            B->modifyUse(constantInt, 0);
            B->modifyUse((*AUseList)[1], 1);
            //A->remove();
        }
        else if (ConstInAIs0 && !ConstInBIs0) {
            B->modifyUse(constantInt, 1);
            B->modifyUse((*AUseList)[1], 0);
            //A->remove();
        }
        else if (!ConstInAIs0 && ConstInBIs0) {
            B->modifyUse(constantInt, 0);
            B->modifyUse((*AUseList)[1], 1);
            //A->remove();
        }
        else if (!ConstInAIs0 && !ConstInBIs0) {
            B->modifyUse(constantInt, 1);
            B->modifyUse((*AUseList)[1], 0);
            //A->remove();
        }
    }



    void InstrComb::combAToBMulFloat(INSTR::Alu* A, INSTR::Alu* B) {
        std::vector<Value*>* AUseList = &A->useValueList;
        std::vector<Value*>* BUseList = &B->useValueList;
        float ConstInA = 0, ConstInB = 0;
        bool ConstInAIs0 = false, ConstInBIs0 = false;
        if (dynamic_cast<Constant*>((*AUseList)[0]) != nullptr) {
            ConstInA = (float) ((ConstantFloat*) (*AUseList)[0])->get_const_val();
            ConstInAIs0 = true;
        } else {
            ConstInA = (float) ((ConstantFloat*) (*AUseList)[1])->get_const_val();
        }

        if (dynamic_cast<Constant*>((*BUseList)[0]) != nullptr) {
            ConstInB = (float) ((ConstantFloat*) (*BUseList)[0])->get_const_val();
            ConstInBIs0 = true;
        } else {
            ConstInB = (float) ((ConstantFloat*) (*BUseList)[1])->get_const_val();
        }

        ConstantFloat* constantFloat = new ConstantFloat(ConstInA * ConstInB);
        if (ConstInAIs0 && ConstInBIs0) {
            B->modifyUse(constantFloat, 0);
            B->modifyUse((*AUseList)[1], 1);
            //A->remove();
        }
        else if (ConstInAIs0 && !ConstInBIs0) {
            B->modifyUse(constantFloat, 1);
            B->modifyUse((*AUseList)[1], 0);
            //A->remove();
        }
        else if (!ConstInAIs0 && ConstInBIs0) {
            B->modifyUse(constantFloat, 0);
            B->modifyUse((*AUseList)[1], 1);
            //A->remove();
        }
        else if (!ConstInAIs0 && !ConstInBIs0) {
            B->modifyUse(constantFloat, 1);
            B->modifyUse((*AUseList)[1], 0);
            //A->remove();
        }
    }
