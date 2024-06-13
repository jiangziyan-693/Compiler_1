//
// Created by start on 23-8-13.
//

#include "if_comb_v2.h"
#include "stl_op.h"
#include "HashMap.h"

if_comb_v2::if_comb_v2(std::vector<Function *> *functions) {
    this->functions = functions;

}

void if_comb_v2::Run() {
    patten_A();
    patten_B();
    patten_C();
    patten_D();
}

void if_comb_v2::patten_A() {
    if (functions->size() != 1) {
        return;
    }
    for_func_(functions) {
        for_bb_(function) {
            if (is_patten_A_bb(bb)) {
                printf("find_patten_A\n");
                Instr* bb_1 = get_instr_at(bb, 1);
                Instr* bb_2 = get_instr_at(bb, 2);
                Instr* shl_1 = get_instr_end_at(bb, 6);
                Instr* div = get_instr_end_at(bb, 5);
                Instr* ashr = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::ASHR, div->useValueList[0], shl_1->useValueList[1], bb);
                div->insert_after(ashr);
                div->modifyAllUseThisToUseA(ashr);
                Instr* xx = get_instr_end_at(bb, 4);
                Instr* d = get_instr_end_at(bb, 3);

                Instr* icmp = get_instr_end_at(bb, 2);
                Instr* br = get_instr_end_at(bb, 1);
                Instr* d_sub_xx = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::SUB, d, xx, bb);
                Instr* shl_2 = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::SHL, d_sub_xx, shl_1->useValueList[1], bb);
                Instr* add = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::ADD, bb_2, shl_2, bb);
                Instr* store = new INSTR::Store(add, bb_1, bb);
                Instr* jump = new INSTR::Jump(head, bb);
                br->insert_before(d_sub_xx);
                br->insert_before(shl_2);
                br->insert_before(add);
                br->insert_before(store);
                br->insert_before(jump);
                bb->succBBs->clear();
                bb->succBBs->push_back(head);
                head->modifyPre(B, bb);
                br->remove();
                icmp->remove();

                remove_bb_and_instr(A);
                remove_bb_and_instr(B);
                remove_bb_and_instr(C);
                remove_bb_and_instr(D);
                remove_bb_and_instr(E);
                remove_bb_and_instr(F);
                remove_bb_and_instr(G);

                break;
            }
        }
    }
}

bool if_comb_v2::is_patten_A_bb(BasicBlock *bb) {
    //bb
    int bb_ins_num = bb_instr_num(bb);
    if (bb_ins_num < 6) return false;
    init_check_trans(bb, 1, INSTR::GetElementPtr);
    init_check_trans(bb, 2, INSTR::Load);
    init_check_trans_end(bb, 1, INSTR::Branch);
    init_check_trans_end(bb, 2, INSTR::Icmp);
    init_check_trans_end(bb, 3, INSTR::Alu);
    init_check_trans_end(bb, 4, INSTR::Alu);
    init_check_trans_end(bb, 5, INSTR::Alu);
    init_check_trans_end(bb, 6, INSTR::Alu);
    check_const_shl_val(bb_end_6, 1, bb_end_6->useValueList[1]);
    check_val_mod_2(bb_end_4, bb_end_5);
    check_val_mod_2(bb_end_3, bb_end_3->useValueList[0]);
    check_icmp_v1_op_v2(bb_end_2, bb_end_4, INSTR::Icmp::Op::NE, bb_end_3);
    check_alu_v1_op_v2(bb_end_5, bb_2, INSTR::Alu::Op::DIV, bb_end_6);
    check_use_at(bb_end_1, bb_end_2, 0);
    check_use_at(bb_2, bb_1, 0);
    A = bb_end_1->getThenTarget();
    B = bb_end_1->getElseTarget();
    //A
    check_bb_instr_num(A, 2);
    init_check_trans(A, 1, INSTR::Icmp);
    init_check_trans(A, 2, INSTR::Branch);
    check_icmp_val_op_const(A_1, bb_end_4, INSTR::Icmp::Op::EQ, 0);
    check_use_at(A_2, A_1, 0);
    C = A_2->getThenTarget();
    D = A_2->getElseTarget();
    //C
    check_bb_instr_num(C, 2);
    init_check_trans(C, 1, INSTR::Icmp);
    init_check_trans(C, 2, INSTR::Branch);
    check_icmp_val_op_const(C_1, bb_end_3, INSTR::Icmp::Op::EQ, 1);
    check_use_at(C_2, C_1, 0);
    check_use_at(C_2, D, 2);
    E = C_2->getThenTarget();
    //D
    check_bb_instr_num(D, 3);
    //Do Phi Check later
    init_check_trans(D, 1, INSTR::Phi);
    init_check_trans(D, 2, INSTR::Icmp);
    init_check_trans(D, 3, INSTR::Branch);
    check_icmp_val_op_const(D_2, bb_end_4, INSTR::Icmp::Op::EQ, 1);
    check_use_at(D_3, D_2, 0);
    check_use_at(D_3, B, 2);
    F = D_3->getThenTarget();
    //E
    check_bb_instr_num(E, 1);
    init_check_trans(E, 1, INSTR::Jump);
    check_use_at(E_1, D, 0);
    //F
    check_bb_instr_num(F, 2);
    init_check_trans(F, 1, INSTR::Icmp);
    init_check_trans(F, 2, INSTR::Branch);
    check_icmp_val_op_const(F_1, bb_end_3, INSTR::Icmp::Op::EQ, 0);
    check_use_at(F_2, F_1, 0);
    check_use_at(F_2, B, 2);
    G = F_2->getThenTarget();
    //G
    check_bb_instr_num(G, 2);
    init_check_trans(G, 1, INSTR::Alu);
    init_check_trans(G, 2, INSTR::Jump);
    check_alu_v1_op_v2(G_1, D_1, INSTR::Alu::Op::SUB, bb_end_6);
    check_use_at(G_2, B, 0);

    //check D phi
    check_use_const_at(D_1, 0, 0);
    check_use_const_at(D_1, 0, 1);
    check_use_at(D_1, bb_end_6, 2);
    //B
    //check phi
    check_bb_instr_num(B, 4);
    init_check_trans(B, 1, INSTR::Phi);
    init_check_trans(B, 2, INSTR::Alu);
    init_check_trans(B, 3, INSTR::Store);
    init_check_trans(B, 4, INSTR::Jump);

    check_use_const_at(B_1, 0, 0);
    check_use_at(B_1, D_1, 1);
    check_use_at(B_1, D_1, 2);
    check_use_at(B_1, G_1, 3);
    check_alu_v1_op_v2(B_2, bb_2, INSTR::Alu::Op::ADD, B_1);
    check_store_val_to_ptr(B_3, B_2, bb_1);
    head = B_4->getTarget();

    //check_bb_instr_num(bb, )
    return true;
}

int if_comb_v2::bb_instr_num(BasicBlock *bb) {
    int ret = 0;
    for_instr_(bb) {
        ret++;
    }
    return ret;
}

Instr *if_comb_v2::get_instr_at(BasicBlock *bb, int index) {
    for_instr_(bb) {
        index--;
        if (!index) {
            return instr;
        }
    }
}

Instr *if_comb_v2::get_instr_end_at(BasicBlock *bb, int index) {
    for (Instr* instr = bb->getEndInstr(); bb->prev != nullptr; instr = (Instr*) instr->prev) {
        index--;
        if (!index) {
            return instr;
        }
    }
}

void if_comb_v2::patten_B() {
    if (functions->size() != 1) {
        return;
    }
    for_func_(functions) {
        for_bb_(function) {
            if (is_patten_B_bb(bb)) {

                printf("find patten B\n");
                Instr* jump = new INSTR::Jump(B, bb);
                bb->getEndInstr()->insert_before(jump);
                bb->getEndInstr()->remove();
                A->precBBs->erase(
                        std::remove(A->precBBs->begin(), A->precBBs->end(), bb),
                        A->precBBs->end()
                        );
                break;
            }
        }
    }
}

bool if_comb_v2::is_patten_B_bb(BasicBlock *bb) {
    check_bb_instr_num(bb, 7);
    init_check_trans(bb, 1, INSTR::Phi);
    init_check_trans(bb, 2, INSTR::Alu);
    init_check_trans(bb, 3, INSTR::Alu);
    init_check_trans(bb, 4, INSTR::Alu);
    init_check_trans(bb, 5, INSTR::Alu);
    init_check_trans(bb, 6, INSTR::Icmp);
    init_check_trans(bb, 7, INSTR::Branch);
    Value* be_mod_ = bb_2->useValueList[0];

    check_alu_v1_op_not_special_const(bb_2, be_mod_, INSTR::Alu::Op::DIV);
    check_alu_v1_op_not_special_const(bb_3, bb_2, INSTR::Alu::Op::MUL);
    check_alu_v1_op_v2(bb_4, be_mod_, INSTR::Alu::Op::SUB, bb_3);
    check_alu_v1_op_not_special_const(bb_5, bb_4, INSTR::Alu::Op::DIV);
    int x_1 = get_const_v(bb_2->useValueList[1]),
    x_2 = get_const_v(bb_3->useValueList[1]),
    x_3 = get_const_v(bb_5->useValueList[1]);
    if (x_1 != x_2 || x_1 % x_3 != 0) return false;
    check_icmp_val_op_const(bb_6, bb_5, INSTR::Icmp::Op::SGE, x_1 / x_3);
    check_use_at(bb_7, bb_6, 0);
    A = bb_7->getThenTarget();
    B = bb_7->getElseTarget();
    return true;

}

void if_comb_v2::patten_C() {
    if (functions->size() != 1) return;
    for_func_(functions) {
        for_bb_(function) {
            if (is_patten_C_bb(bb)) {
                printf("find patten C\n");
//                Instr* jump = new INSTR::Jump(B, A);
                C->precBBs->clear();
                C->precBBs->push_back(B);
                instrs->clear();
                for_instr_(B) {
                    instrs->push_back(instr);
                }
                for (Instr* instr: *instrs) {
                    instr->delFromNowBB();
                    instr->bb = A;
                    A->getEndInstr()->insert_before(instr);
                }
                A->getEndInstr()->remove();
                B->remove();
//                break;
            }
        }
    }
}

bool if_comb_v2::is_patten_C_bb(BasicBlock *bb) {
    //TODO:限定A_1 的下标不在A B C 中定义 ==> if need
    //A
    A = bb;
    check_bb_instr_num(A, 4);
    init_check_trans(A, 1, INSTR::GetElementPtr);
    init_check_trans(A, 2, INSTR::Load);
    init_check_trans(A, 3, INSTR::Icmp);
    init_check_trans(A, 4, INSTR::Branch)
    check_use_at(A_2, A_1, 0);
    check_icmp_val_op_const(A_3, A_2, INSTR::Icmp::Op::EQ, 0);
    check_use_at(A_4, A_3, 0);
    B = A_4->getElseTarget();
    C = A_4->getThenTarget();
    //B
    check_bb_instr_num(B, 7);
    init_check_trans(B, 1, INSTR::Load);
    init_check_trans(B, 2, INSTR::GetElementPtr);
    init_check_trans(B, 3, INSTR::Load);
    init_check_trans(B, 4, INSTR::Alu);
    init_check_trans(B, 5, INSTR::Alu);
    init_check_trans(B, 6, INSTR::Store);
    init_check_trans(B, 7, INSTR::Jump);
    Value* B_1_load_pos = B_1->useValueList[0];
    check_gep_equal_index_diff_array(A_1, B_2);
    Value* A_1_B_2_gep_index = A_1->useValueList[2];
    check_use_at(B_3, B_2, 0);
    check_alu_v1_op_v2(B_4, A_2, INSTR::Alu::Op::MUL, B_3);
    check_alu_v1_op_v2(B_5, B_1, INSTR::Alu::Op::ADD, B_4);
    check_use_at(B_6, B_5, 0);
    check_use_at(B_6, B_1_load_pos, 1);
    check_use_at(B_7, C, 0);

    //C
    check_bb_instr_num(C, 2);
    init_check_trans(C, 1, INSTR::Alu);
    init_check_trans(C, 2, INSTR::Jump);
    check_alu_v1_op_const(C_1, A_1_B_2_gep_index, INSTR::Alu::Op::ADD, 1);


    return true;
}

void if_comb_v2::patten_D() {
    std::vector<Instr*>* adds = new std::vector<Instr*>();
    for_func_(functions) {
        for_bb_(function) {
            if (is_patten_D_bb(bb)) {
                printf("find patten D\n");
//                INSTR::Jump* jump = new INSTR::Jump(B, A);
//                A->getEndInstr()->remove();
//                A->getEndInstr()->insert_before(jump);
                Instr* br = A->getEndInstr();
                adds->clear();
                for_instr_(B) {
                    adds->push_back(instr);
                }
                for (Instr* instr: *adds) {
                    instr->delFromNowBB();
                    instr->bb = A;
                    A->getEndInstr()->insert_before(instr);
                }
                br->remove();
                B->remove();

                A->succBBs->clear();
                for_instr_(C) {
                    instr->remove();
                }
                C->remove();
                for_instr_(D) {
                    instr->remove();
                }
                D->remove();
            }
        }
    }
}

bool if_comb_v2::is_patten_D_bb(BasicBlock *bb) {
    A = bb;
    check_bb_instr_num(A, 4);
    init_check_trans(A, 1, INSTR::Call);
    init_check_trans(A, 2, INSTR::Alu);
    init_check_trans_t1_t2(A, 3, INSTR::Icmp, INSTR::Fcmp);
    init_check_trans(A, 4, INSTR::Branch);
    B = A_4->getThenTarget(), C = A_4->getElseTarget();
    //B

    //C
    check_bb_instr_num(C, 2);
    init_check_trans_t1_t2(C, 1, INSTR::Icmp, INSTR::Fcmp);
    init_check_trans(C, 2, INSTR::Branch);
    check_use_at(C_2, C_1, 0);
    check_use_at(C_2, B, 1);
    D = C_2->getElseTarget();

    //D
    if (is_type(INSTR::Branch, D->getEndInstr())) return false;
    if (!is_all_range(A_3, C_1)) return false;

    return true;
}

bool if_comb_v2::is_all_range(Instr *cmp1, Instr *cmp2) {
    if (is_type(INSTR::Icmp, cmp1)) {
        INSTR::Icmp* icmp1 = (INSTR::Icmp*) cmp1;
        INSTR::Icmp* icmp2 = (INSTR::Icmp*) cmp2;
        if (icmp1->getRVal1() != icmp2->getRVal1()) return false;
        if (icmp1->op == INSTR::Icmp::Op::SLE && icmp2->op == INSTR::Icmp::Op::SGE &&
            is_type(ConstantFloat, icmp1->getRVal2()) && is_type(ConstantFloat, icmp2->getRVal2())) {
            if (((ConstantFloat*) icmp1->getRVal2())->get_const_val() >
                ((ConstantFloat*) icmp1->getRVal1())->get_const_val()) {
                return true;
            }
        }
        if (icmp1->op == INSTR::Icmp::Op::SGE && icmp2->op == INSTR::Icmp::Op::SLE &&
            is_type(ConstantFloat, icmp1->getRVal2()) && is_type(ConstantFloat, icmp2->getRVal2())) {
            if (((ConstantFloat*) icmp1->getRVal2())->get_const_val() <
                ((ConstantFloat*) icmp1->getRVal1())->get_const_val()) {
                return true;
            }
        }
        return false;
    } else {
        INSTR::Fcmp* fcmp1 = (INSTR::Fcmp*) cmp1;
        INSTR::Fcmp* fcmp2 = (INSTR::Fcmp*) cmp2;
        if (fcmp1->getRVal1() != fcmp2->getRVal1()) return false;
        if (fcmp1->op == INSTR::Fcmp::Op::OLE && fcmp2->op == INSTR::Fcmp::Op::OGE &&
                is_type(ConstantFloat, fcmp1->getRVal2()) && is_type(ConstantFloat, fcmp2->getRVal2())) {
            if (((ConstantFloat*) fcmp1->getRVal2())->get_const_val() >
                    ((ConstantFloat*) fcmp1->getRVal1())->get_const_val()) {
                return true;
            }
        }
        if (fcmp1->op == INSTR::Fcmp::Op::OGE && fcmp2->op == INSTR::Fcmp::Op::OLE &&
            is_type(ConstantFloat, fcmp1->getRVal2()) && is_type(ConstantFloat, fcmp2->getRVal2())) {
            if (((ConstantFloat*) fcmp1->getRVal2())->get_const_val() <
                ((ConstantFloat*) fcmp1->getRVal1())->get_const_val()) {
                return true;
            }
        }
        return false;
    }
    return false;
}

