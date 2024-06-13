#include "../../include/midend/Rem2DivMulSub.h"
#include "../../include/util/CenterControl.h"

#include <cmath>




	Rem2DivMulSub::Rem2DivMulSub(std::vector<Function*>* functions) {
        this->functions = functions;
    }

    void Rem2DivMulSub::Run() {
        init();
        transRemToDivMulSub();
    }

    void Rem2DivMulSub::init() {
        for (Function* function: *functions) {
            for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
                for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
                    if (dynamic_cast<INSTR::Alu*>(instr) != nullptr) {
                        if ((((INSTR::Alu*) instr)->op == INSTR::Alu::Op::REM)) {
                            rems->insert((INSTR::Alu*) instr);
                        } else if ((((INSTR::Alu*) instr)->op == INSTR::Alu::Op::FREM)) {
                            frems->insert((INSTR::Alu*) instr);
                        }
                    }
                }
            }
        }
    }

    void Rem2DivMulSub::transRemToDivMulSub() {
        for (INSTR::Alu* rem: *rems) {
            Value* A = rem->getRVal1();
            Value* B = rem->getRVal2();
            if (dynamic_cast<ConstantInt*>(B) != nullptr) {
                int val = (int) ((ConstantInt*) B)->get_const_val();
                if (val > 0 && ((int) pow(2, ((int) (log(val) / log(2))))) == val) {
                    continue;
                }
            }
            Instr* div = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::DIV, A, B, rem->bb);
            rem->insert_before(div);
            Instr* mul = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::MUL, div, B, rem->bb);
            rem->insert_before(mul);
            Instr* mod = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::SUB, A, mul, rem->bb);
            rem->insert_before(mod);

            rem->modifyAllUseThisToUseA(mod);
        }

        for (INSTR::Alu* frem: *frems) {
            Value* A = frem->getRVal1();
            Value* B = frem->getRVal2();
            Instr* div = new INSTR::Alu(BasicType::F32_TYPE, INSTR::Alu::Op::FDIV, A, B, frem->bb);
            frem->insert_before(div);
            Instr* mul = new INSTR::Alu(BasicType::F32_TYPE, INSTR::Alu::Op::FMUL, div, B, frem->bb);
            frem->insert_before(mul);
            Instr* mod = new INSTR::Alu(BasicType::F32_TYPE, INSTR::Alu::Op::FSUB, A, mul, frem->bb);
            frem->insert_before(mod);

            frem->modifyAllUseThisToUseA(mod);
        }
    }
