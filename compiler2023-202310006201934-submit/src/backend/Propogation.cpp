//
// Created by XuRuiyuan on 2023/8/10.
//
#include "Propogation.h"
#include "util.h"
#include "MachineInst.h"
#include "MC.h"
#include "I.h"
#include "set"
// #include "util.h"
#include <algorithm>
#include "utility"

void PtrPorpogation::ptr_propogation() {
    FOR_ITER_MF(mf, p) {
        sp_base_ptr_propogation(mf);
    }
}

void PtrPorpogation::sp_base_ptr_propogation(McFunction *mf) {
    std::map<Operand *, int> sp_based_addr2off;
    FOR_ITER_MB(mb, mf) {
        FOR_ITER_MI(mi, mb) {
            if (auto *bino = dynamic_cast<I::Binary *>(mi)) {
                if (bino->isNeedFix()) continue;
                if (*bino->getLOpd() == *Arm::Reg::getsp() && bino->getROpd()->is_I_Imm()) {
                    sp_based_addr2off[bino->getDst()] = bino->getROpd()->value;
                } else if (*bino->getROpd() == *Operand::I_ZERO) {

                }

            }
        }
    }
}

void PtrPorpogation::glob_base_ptr_propogation(McFunction *mf){

}