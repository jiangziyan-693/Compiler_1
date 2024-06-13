//
// Created by start on 23-8-11.
//
#include "math_cast.h"
#include "stl_op.h"
#include "HashMap.h"

math_cast::math_cast(std::vector<Function *> *functions) {
    this->functions = functions;
}

void math_cast::init() {
    for (Function* function: *functions) {
        for_bb_(function) {
            for_instr_(bb) {
                if (dynamic_cast<INSTR::Alu*>(instr) != nullptr) {
                    if (((INSTR::Alu*) instr)->op == INSTR::Alu::Op::REM &&
                            dynamic_cast<ConstantInt*>(instr->useValueList[1]) != nullptr &&
                            true) {

                    }
                }
            }
        }
    }
}

