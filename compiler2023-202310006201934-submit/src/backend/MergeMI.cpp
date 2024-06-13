//
// Created by XuRuiyuan on 2023/8/18.
//

#include "MergeMI.h"
#include "util.h"
#include "MachineInst.h"
#include "Arm.h"
#include "CodeGen.h"

#define I32 DataType::I32

static void insert_def_use(McFunction *mf, MachineInst *mi, Location const &loc) {
    for (auto const r: mi->defOpds) {
        mf->vrDefs[r].insert(loc);
    }
    for (auto const r: mi->useOpds) {
        mf->vrUses[r].insert(loc);
    }
}

void MergeMI::gen_def_use_pos(McFunction *mf) {
    mf->vrDefs.clear();
    mf->vrUses.clear();
    FOR_ITER_MB(mb, mf) {
        int idx = 0;
        FOR_ITER_MI(mi, mb) {
            insert_def_use(mf, mi, Location{.mb = mb, .locatedMI = mi, .index = idx++});
        }
    }
}

void MergeMI::mergeAll(McProgram *mp) {
    FOR_ITER_MF(mf, mp) {
        // ssa_merge_shift_to_binary(mf);
        ssa_merge_add_to_load_store(mf);
        // ssa_merge_add_sub_to_mul(mf);
    }
}

void MergeMI::ssa_merge_shift_to_binary(McFunction *mf) {
    gen_def_use_pos(mf);
}

void MergeMI::ssa_merge_add_to_load_store(McFunction *mf) {
    gen_def_use_pos(mf);
    for (auto const &it: mf->vrDefs) {
        Operand *defVr = it.first;
        if (it.second.size() == 1) {
            const Location &defLoc = *it.second.cbegin();
            MachineInst *defMI = defLoc.locatedMI;
            auto check = [&]() {
                int max_use_idx = -1;
                for (const Location &useLoc: mf->vrUses[defVr]) {
                    if (useLoc.mb == defMI->mb) {
                        max_use_idx = std::max(max_use_idx, useLoc.index);
                    }
                }
                bool badDef = false;
                for (Operand *useOpd: defMI->useOpds) {
                    for (const Location &locOfUseOfDefMI: mf->vrDefs[useOpd]) {
                        if (locOfUseOfDefMI.mb == defMI->mb
                            && locOfUseOfDefMI.index >= defLoc.index
                            && locOfUseOfDefMI.index <= max_use_idx) {
                            badDef = true;
                            break;
                        }
                    }
                }
                return badDef;
            };
            if (!CenterControl::ACROSS_BLOCK_MERGE_MI && check()) continue;
            if (defMI->isNoCond() && !defMI->isNeedFix()) {
                if ((defMI->tag == MachineInst::Tag::Add || defMI->tag == MachineInst::Tag::Sub)
                    && (defMI->noShift() || !defMI->shift->shiftIsReg())) {
                    std::vector<MachineMemInst *> memToReplace;
                    if (defMI->useOpds[1]->is_I_Imm() && defMI->noShift()) {
                        // add addr, a, #imm
                        int imm = defMI->useOpds[1]->value;
                        bool cannotMerge = false;
                        for (const Location &useLoc: mf->vrUses[defVr]) {
                            MachineInst *useMI = useLoc.locatedMI;
                            if ((useMI->tag == MachineInst::Tag::Ldr ||
                                 useMI->tag == MachineInst::Tag::Str)
                                && (CenterControl::ACROSS_BLOCK_MERGE_MI || defMI->mb == useMI->mb)
                                && useMI->noShift()) {
                                // ldr/str d [addr, off]
                                MachineMemInst *mem = dynamic_cast<MachineMemInst *>(useMI);
                                if (mem->getAddr() == defVr
                                    && mem->getOffset()->is_I_Imm()
                                    && CodeGen::LdrStrImmEncode(mem->getOffset()->value + imm)) {
                                    memToReplace.push_back(mem);
                                } else {
                                    cannotMerge = true;
                                }
                            } else {
                                cannotMerge = true;
                            }
                        }
                        if (cannotMerge) continue;
                        for (MachineMemInst *memMI: memToReplace) {
                            // MachineInst *toReplace = dynamic_cast<MachineInst *>(memMI);
                            memMI->setAddr(defMI->useOpds[0]);
                            memMI->setOffSet(new Operand(I32, memMI->getOffset()->value + defMI->useOpds[1]->value));
                        }
                        defMI->remove();
                    } else if (defMI->useOpds.size() > 1 && defMI->shift->hasShift() && !defMI->shift->shiftIsReg()) {
                        // add addr, lv, rv, [type] #shiftImm
                        Operand *lv = defMI->useOpds[0];
                        Operand *rv = defMI->useOpds[1];
                        bool cannotMerge = false;
                        for (const Location &useLoc: mf->vrUses[defVr]) {
                            MachineInst *useMI = useLoc.locatedMI;
                            if ((useMI->tag == MachineInst::Tag::Ldr ||
                                 useMI->tag == MachineInst::Tag::Str)
                                && (CenterControl::ACROSS_BLOCK_MERGE_MI || defMI->mb == useMI->mb)
                                && useMI->noShift()) {
                                // ldr/str d [addr, #0]
                                // assert(defMI->defOpds[0] == useMI->useOpds[0]);
                                MachineMemInst *mem = dynamic_cast<MachineMemInst *>(useMI);
                                if (mem->getAddr() == defVr
                                    && *mem->getOffset() == *Operand::I_ZERO) {
                                    memToReplace.push_back(mem);
                                } else {
                                    cannotMerge = true;
                                }
                            } else {
                                cannotMerge = true;
                            }
                        }
                        if (cannotMerge) continue;
                        for (MachineMemInst *memMI: memToReplace) {
                            // ldr/str d [lv, rv, [type] #shiftImm]
                            memMI->setAddr(lv);
                            memMI->setOffSet(rv);
                            memMI->addShift(defMI->shift);
                        }
                        defMI->remove();
                    }
                    // }
                    // if(manyMbDef) continue;
                    // bool badDef = false;
                    // for(Operand *useOpd : defMI->useOpds) {
                    //     for(const Location &locOfUseOfDefMI: mf->vrDefs[useOpd]) {
                    //         if(locOfUseOfDefMI.mb == defMI->mb
                    //         && locOfUseOfDefMI.index >= defLoc.index
                    //         && locOfUseOfDefMI.index <= max_use_idx) {
                    //             badDef = true;
                    //             break;
                    //         }
                    //     }
                    // }
                    // if(!badDef) {
                    //     for (const Location &useLoc: mf->vrUses[defVr]) {
                    //         MachineInst *useMI = useLoc.locatedMI;
                    //
                    //         useMI->remove();
                    //     }
                    // }
                }
            }
        }
    }
}

void MergeMI::ssa_merge_add_sub_to_mul(McFunction *mf) {
    gen_def_use_pos(mf);
    FOR_ITER_MB(mb, mf) {

    }
}
