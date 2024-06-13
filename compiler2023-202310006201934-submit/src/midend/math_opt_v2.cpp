//
// Created by start on 23-8-11.
//
#include "math_opt_v2.h"
#include "stl_op.h"
#include "HashMap.h"

math_opt_v2::math_opt_v2(std::vector<Function *> *functions) {
    this->functions = functions;
}

void math_opt_v2::Run() {
    init();
    alu_fold();
    alu_fold_v2();
    alu_fold();
}

void math_opt_v2::init() {
    for (Function *function: *functions) {
        for_bb_(function) {
            for_instr_(bb) {
                if (is_type(INSTR::Alu, instr)) {
                    INSTR::Alu *alu = (INSTR::Alu *) instr;
                    if (alu->hasOneConst() && alu->op == INSTR::Alu::Op::ADD) {
                        one_const_add->insert(alu);
                    }
                }
            }
        }
    }
}

void math_opt_v2::alu_fold() {
    for (Instr *add: *one_const_add) {
        for_use_(add) {
            Instr *user = use->user;
            if (is_type(INSTR::Alu, user)) {
                INSTR::Alu *sub = (INSTR::Alu *) user;
                if (sub->bb != add->bb) {
                    continue;
                }
                if (dynamic_cast<ConstantInt *>(sub->useValueList[1]) != nullptr &&
                    sub->op == INSTR::Alu::Op::SUB &&
                    get_const_value(add) == get_const_value(sub)) {
                    sub->modifyAllUseThisToUseA(get_none_const_value(add));
                }
            }
        }
    }
}

int math_opt_v2::get_const_value(Instr *instr) {
    if (dynamic_cast<ConstantInt *>(instr->useValueList[0]) != nullptr) {
        return ((ConstantInt *) instr->useValueList[0])->get_const_val();
    }
    return ((ConstantInt *) instr->useValueList[1])->get_const_val();
}

Value *math_opt_v2::get_none_const_value(Instr *instr) {
    if (dynamic_cast<ConstantInt *>(instr->useValueList[0]) != nullptr) {
        return instr->useValueList[1];
    }
    return instr->useValueList[0];
}


void math_opt_v2::alu_fold_v2() {
    for_func_(functions) {
        for_bb_(function) {
            for_instr_(bb) {
                patten_1(instr);
                patten_2(instr);
            }
        }
    }
}

bool math_opt_v2::is_A_add_B(Instr *instr, Value *A, Value *B) {
    if (!is_type(INSTR::Alu, instr)) {
        return false;
    }
    if (((INSTR::Alu *) instr)->op != INSTR::Alu::Op::ADD) {
        return false;
    }
    return instr->useValueList[0] == A && instr->useValueList[1] == B;
}

bool math_opt_v2::is_A_add_1(Instr *instr, Value *A) {
    if (!is_type(INSTR::Alu, instr)) {
        return false;
    }
    if (((INSTR::Alu *) instr)->op != INSTR::Alu::Op::ADD) {
        return false;
    }
//    INSTR::Alu* alu = (INSTR::Alu*) instr;
    return instr->useValueList[0] == A && is_const_1(instr->useValueList[1]);
}

bool math_opt_v2::is_A_sub_1(Instr *instr, Value *A) {
    if (!is_type(INSTR::Alu, instr)) {
        return false;
    }
    if (((INSTR::Alu *) instr)->op != INSTR::Alu::Op::SUB) {
        return false;
    }
//    INSTR::Alu* alu = (INSTR::Alu*) instr;
    return instr->useValueList[0] == A && is_const_1(instr->useValueList[1]);
}

bool math_opt_v2::is_add_none_const(Instr *instr) {
    if (!is_type(INSTR::Alu, instr)) {
        return false;
    }
    if (((INSTR::Alu *) instr)->op != INSTR::Alu::Op::ADD) {
        return false;
    }
    return !((INSTR::Alu *) instr)->hasOneConst() &&
           !((INSTR::Alu *) instr)->hasOneConst();
}

bool math_opt_v2::is_const_1(Value *A) {
    if (!is_type(ConstantInt, A)) {
        return false;
    }
    return ((ConstantInt*) A)->get_const_val() == 1;
}

bool math_opt_v2::A_after_B(Instr *A, Instr *B) {
    if (A->bb != B->bb) {
        return false;
    }
    int tag = 0;
    for_instr_(A->bb) {
        if (tag) {
            break;
        }
        if (instr == B) {
            tag = 1;
        }
        if (!tag && instr == A) {
            return false;
        }
    }
    return true;
}

void math_opt_v2::patten_1(Instr *instr) {
    if (!is_add_none_const(instr)) {
        return;
    }
    Instr* C = instr;
    Instr* A = (Instr*) C->useValueList[0];
    Instr* B = (Instr*) C->useValueList[1];
    Instr *D = nullptr, *E = nullptr, *F = nullptr;
    for_use_(C) {
        if (use->user->bb != C->bb) {
            continue;
        }
        if (is_A_add_1(use->user, C)) {
            D = use->user;
        }
    }
    for_use_(B) {
        if (use->user->bb != C->bb) {
            continue;
        }
        if (is_A_add_1(use->user, B)) {
            E = use->user;
        }
    }
    if (D == nullptr || E == nullptr) {
        return;
    }
    for_use_(E) {
        if (use->user->bb != E->bb) {
            continue;
        }
        if (is_A_add_B(use->user, A, E)) {
            F = use->user;
        }
    }
    if (F && A_after_B(F, D)) {
        F->modifyAllUseThisToUseA(D);
    }
    return;
}

void math_opt_v2::patten_2(Instr *instr) {
    if (!is_add_none_const(instr)) {
        return;
    }
    Instr* C = instr;
    Instr* A = (Instr*) C->useValueList[0];
    Instr* B = (Instr*) C->useValueList[1];
    Instr *D = nullptr, *E = nullptr, *F = nullptr;
    for_use_(B) {
        if (use->user->bb != C->bb) {
            continue;
        }
        if (is_A_add_1(use->user, B)) {
            D = use->user;
        }
    }
    if (!D) {
        return;
    }
    for_use_(D) {
        if (use->user->bb != D->bb) {
            continue;
        }
        if (is_A_add_B(use->user, A, D)) {
            E = use->user;
        }
    }
    if (!E) {
        return;
    }
    for_use_(E) {
        if (use->user->bb != E->bb) {
            continue;
        }
        if (is_A_sub_1(use->user, E)) {
            F = use->user;
        }
    }
    if (F && A_after_B(F, C)) {
        F->modifyAllUseThisToUseA(C);
    }
    return;
}


