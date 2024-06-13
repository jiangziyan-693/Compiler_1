//
// Created by XuRuiyuan on 2023/8/15.
//

#include "BKConstFold.h"
#include "MC.h"
#include "MachineInst.h"
#include "map"
#include "I.h"
#include "V.h"
#include "Arm.h"
#include "CodeGen.h"
#include <utility>
#include <functional>
#include "limits"


#define I32 DataType::I32
#define F32 DataType::F32
#define I1 DataType::I1
typedef MachineInst::Tag MITag;
typedef Arm::ShiftType ShiftType;
typedef Arm::Shift Shift;
#define LSL ShiftType::Lsl
#define ASR ShiftType::Asr
#define LSR ShiftType::Lsr
#define IS_POWER2(x) (((x) & ((x) - 1)) == 0)

// inline int countl_zero(unsigned const x) { return __builtin_clz(x); }

inline int bit_width(unsigned const x) {
    return std::numeric_limits<decltype(x)>::digits - __builtin_clz(x);
}

std::optional<int> BKConstFold::eval_operand(Operand *o, Arm::Shift *shift) {
    std::optional<int> x = std::nullopt;
    if (shift->hasShift() && !shift->shiftIsReg()) {
        auto sh = shift->getShiftOpd();
        if (!curMF->get_imm(o)) {
            return std::nullopt;
        }
        if (shift->shiftType == Arm::ShiftType::Lsl) {
            x = curMF->constMap[o].iv << sh->value;
        } else if (shift->shiftType == Arm::ShiftType::Lsr) {
            x = unsigned(curMF->constMap[o].iv) >> sh->value;
        } else if (shift->shiftType == Arm::ShiftType::Asr) {
            x = curMF->constMap[o].iv >> sh->value;
        } else if (shift->shiftType == Arm::ShiftType::Ror) {
            x = (unsigned(curMF->constMap[o].iv) >> sh->value) | (curMF->constMap[o].iv << (32 - sh->value));
        } else {
            exit(59);
        }
    } else if (shift->hasShift() && shift->shiftIsReg()) {
        auto lVr = curMF->get_imm(o);
        auto rVr = curMF->get_imm(shift->shiftOpd);
        bool lnotnull = lVr != std::nullopt;
        bool rnotnull = rVr != std::nullopt;
        if (lnotnull && rnotnull) {
            int li = *lVr;
            int ri = *rVr;
            if (shift->shiftType == Arm::ShiftType::Lsl) {
                x = li << ri;
            } else if (shift->shiftType == Arm::ShiftType::Lsr) {
                x = unsigned(li) >> ri;
            } else if (shift->shiftType == Arm::ShiftType::Asr) {
                x = li >> ri;
            } else if (shift->shiftType == Arm::ShiftType::Ror) {
                x = (unsigned(li) >> ri) | (li << (32 - ri));
            } else {
                exit(59);
            }
        } else if (lnotnull && *lVr == 0) {
            x = 0;
        } else if (rnotnull) {
            if (shift->shiftType == Arm::ShiftType::Lsr || shift->shiftType == Arm::ShiftType::Lsl) {
                if (*rVr >= 32) {
                    x = 0;
                }
            }
        }
    } else {
        x = curMF->get_imm(o);
    }
    return x;
}

// std::optional<bool> BKConstFold::const_bool_deal(Arm::Cond cond, MachineInst *cmp) {
//     auto left_num = curMF->get_imm(cmp->useOpds[0]);
//     auto right_num = curMF->get_imm(cmp->useOpds[1]);
//     bool lf = left_num != std::nullopt;
//     bool rf = right_num != std::nullopt;
//     std::optional<bool> res = std::nullopt;
//     if (lf && rf) {
//         int l = *left_num;
//         int r = *right_num;
//         if (cond == Arm::Cond::Eq) {
//             res = l == r;
//         } else if (cond == Arm::Cond::Ne) {
//             res = l != r;
//         } else if (cond == Arm::Cond::Gt) {
//             res = l > r;
//         } else if (cond == Arm::Cond::Ge) {
//             res = l >= r;
//         } else if (cond == Arm::Cond::Lt) {
//             res = l < r;
//         } else if (cond == Arm::Cond::Le) {
//             res = l <= r;
//             // todo
//             // } else if (cond == Arm::Cond::Hi) {
//             //     res = l > r;
//             // } else if (cond == Arm::Cond::Pl) {
//             //     res = l >= r;
//         }
//     } else if (lf) {
//         int l = *left_num;
//         if (CodeGen::immCanCode(l)) {
//             // todo
//             // assert(false);
//         }
//     } else if (rf) {
//         int r = *right_num;
//         if (CodeGen::immCanCode(r)) {
//             // todo
//         }
//     }
//     return res;
// }

void BKConstFold::const_fold(McFunction *mf) {
    // return;
    // curMF->constMap.clear();
    std::set<Operand *, CompareOperand> singleDefVR;
    std::map<Operand *, std::set<MachineInst *>, CompareOperand> ssaVR2MI;
    FOR_ITER_MB(mb, mf) {
        FOR_ITER_MI(mi, mb) {
            mi->singleDef = false;
            for (Operand *def: mi->defOpds) {
                ssaVR2MI[def].insert(mi);
            }
        }
    }
    for (auto const &it: ssaVR2MI) {
        if (it.second.size() == 1) {
            singleDefVR.insert(it.first);
            (*it.second.begin())->singleDef = true;
        }
    }
    // curMF->constMap;
    FOR_ITER_MB(mb, mf) {
        curMF = mf;
        for (MachineInst *mi = dynamic_cast<MachineInst *>(mb->beginMI->next),
                     *nextMI = dynamic_cast<MachineInst *>(mi->next);
             mi->next != nullptr; mi = nextMI) {
            nextMI = dynamic_cast<MachineInst *>(mi->next);
            bool success_imm = false;
            if(mi->isNeedFix()) continue;
            if (CAST_IF(iMov, mi, I::Mov)) {
                if(iMov->getSrc()->value == 4) {
                    int a = 0;
                }
                if (!mi->singleDef) {
                    std::optional<int> may_imm_src = eval_operand(iMov->getSrc(), iMov->shift);
                    if (may_imm_src != std::nullopt) {
                        int imm = *may_imm_src;
                        iMov->shift = Shift::NONE_SHIFT;
                        iMov->setUse(0, new Operand(I32, imm));
                    }
                    continue;
                }
                if (iMov->getSrc()->is_I_Imm()
                    && iMov->getDst() != Arm::Reg::getR(Arm::GPR::sp)
                    /*&& !iMov->isNeedFix()*/) {
                    int imm = iMov->getSrc()->value;
                    if (iMov->isMvn) {
                        imm = ~imm;
                    }
                    if (iMov->getDst()->isVirtual(I32)) {
                        curMF->constMap[iMov->getDst()] = imm;
                    } else {
                        iMov->shift = Shift::NONE_SHIFT;
                        iMov->setUse(0, new Operand(I32, imm));
                    }
                    success_imm = true;
                } else {
                    if(iMov->getSrc()->value == 4) {
                        int a = 0;
                    }
                    std::optional<int> may_imm_src = eval_operand(iMov->getSrc(), iMov->shift);
                    if (may_imm_src != std::nullopt) {
                        int imm = *may_imm_src;
                        if (iMov->getDst()->isVirtual(I32)) {
                            curMF->constMap[iMov->getDst()] = imm;
                        } else {
                            iMov->shift = Shift::NONE_SHIFT;
                            iMov->setUse(0, new Operand(I32, imm));
                        }
                        success_imm = true;
                    }
                }
            } else if (CAST_IF(vMov, mi, V::Mov)) {
                continue;
                if (!mi->singleDef) {
                    if(mi->noShift()) {
                        std::optional<float> may_imm_src = curMF->get_fconst(vMov->getSrc());
                        if (may_imm_src != std::nullopt) {
                            int imm = *may_imm_src;
                            iMov->shift = Shift::NONE_SHIFT;
                            iMov->setUse(0, new Operand(I32, imm));
                        }
                    }
                    continue;
                }
                if (vMov->noShift() && vMov->getDst()->isVirtual(F32)) {
                    curMF->constMap[vMov->getDst()] = vMov->getDst()->fConst;
                }
                success_imm = true;
            }
            if (success_imm) continue;
            if (/*!mi->isNeedFix() &&*/
                mi->cond == Arm::Cond::Any
                && (mi->tag == MITag::Add
                    || mi->tag == MITag::Sub
                    || mi->tag == MITag::Rsb
                    || mi->tag == MITag::Mul
                    || mi->tag == MITag::Div
                    || mi->tag == MITag::LongMul
                    || mi->tag == MITag::And
                    || mi->tag == MITag::Or
                    || mi->tag == MITag::Xor
                    || mi->tag == MITag::USAT
                    || mi->tag == MITag::Bic
                        // || mi->tag == MITag::Bfc
                )) {
                if (mi->tag == MachineInst::Tag::Add && mi->shift->hasShift()) {
                    int a = 0;
                }
                std::optional<int> may_imm1 = curMF->get_imm(mi->useOpds[0]);
                std::optional<int> may_imm2 = std::nullopt;
                if (mi->noShift()) {
                    may_imm2 = curMF->get_imm(mi->useOpds[1]);
                } else {
                    may_imm2 = eval_operand(mi->useOpds[1], mi->shift);
                }
                // std::optional<int> res = std::nullopt;
                if (may_imm1 != std::nullopt && may_imm2 != std::nullopt) {
                    int imm1 = *may_imm1;
                    int imm2 = *may_imm2;
                    int res = 0;
                    bool flag = true;
                    if (mi->tag == MITag::Add) {
                        res = imm1 + imm2;
                    } else if (mi->tag == MITag::Sub) {
                        res = imm1 - imm2;
                    } else if (mi->tag == MITag::Rsb) {
                        res = imm2 - imm1;
                    } else if (mi->tag == MITag::Mul) {
                        res = imm1 * imm2;
                    } else if (mi->tag == MITag::Div) {
                        res = imm1 / imm2;
                    } else if (mi->tag == MITag::LongMul) {
                        res = (int) (((long long) imm1) * imm2 >> 32);
                    } else if (mi->tag == MITag::And) {
                        res = imm1 & imm2;
                    } else if (mi->tag == MITag::Or) {
                        res = imm1 | imm2;
                    } else if (mi->tag == MITag::Xor) {
                        res = imm1 ^ imm2;
                    } else if (mi->tag == MITag::USAT) {
                        res = imm2 & ((1u << imm1) - 1);
                    } else if (mi->tag == MITag::Bic) {
                        res = imm1 & ~imm2;
                        // } else if (mi->tag == MITag::Bfc) {
                        //     res = imm1
                    } else {
                        flag = false;
                        // exit(60);
                    }
                    if (flag) {
                        nextMI = dynamic_cast<MachineInst *>(new I::Mov(mi, mi->defOpds[0], new Operand(I32, res)));
                        mi->remove();
                    }
                } else if ((mi->noShift() || may_imm2 != std::nullopt) && mi->tag == MITag::Mul) {
                    int imm2;
                    Operand *src;
                    if (may_imm2 != std::nullopt) {
                        imm2 = *may_imm2;
                        src = mi->useOpds[0];
                    } else if (may_imm1 != std::nullopt) {
                        imm2 = *may_imm1;
                        src = mi->useOpds[1];
                    } else {
                        continue;
                    }
                    Operand *dst = mi->defOpds[0];
                    bool flag = false;
                    if (imm2 == 0) {
                        nextMI = new I::Mov(mi, dst, new Operand(I32, 0));
                        flag = true;
                    } else if (imm2 == 1) {
                        new I::Mov(mi, dst, src);
                        flag = true;
                    } else if (imm2 == -1) {
                        new I::Binary(mi, MITag::Rsb, dst, src,
                                      new Operand(I32, 0));
                        flag = true;
                    } else if (IS_POWER2(imm2)) {
                        new I::Mov(mi, dst, src, new Shift(LSL, bit_width(imm2) - 1));
                        flag = true;
                    } else if (IS_POWER2(imm2 + 1)) {
                        new I::Binary(mi, MITag::Rsb, dst, src, src,
                                      new Shift(LSL, bit_width(imm2 + 1) - 1));
                        flag = true;
                    } else if (IS_POWER2(imm2 - 1)) {
                        new I::Binary(mi, MITag::Add, dst, src, src,
                                      new Shift(LSL, bit_width(imm2 - 1) - 1));
                        flag = true;
                    } else if (IS_POWER2(-imm2 + 1)) {
                        // mul ra, rb, -3
                        // sub ra, rb, rb #2
                        new I::Binary(mi, MITag::Sub, dst, src, src,
                                      new Shift(LSL, bit_width(-imm2 + 1) - 1));
                        flag = true;
                    } else if (IS_POWER2(-imm2 - 1)) {
                        // mul ra, rb, -3
                        // add tmp, rb, rb # 1
                        // rsb ra, tmp, #0
                        Operand *tmp = curMF->newVR();
                        MachineInst *pos = new I::Binary(mi, MITag::Add, tmp, src, src,
                                                         new Shift(LSL, bit_width(-imm2 - 1) - 1));
                        new I::Binary(pos, MITag::Rsb, dst, tmp, new Operand(I32, 0));
                        flag = true;
                    } else if (IS_POWER2(-imm2)) {
                        // mul ra, rb, -4
                        // mov tmp, rb, rb, #2
                        // rsb ra, tmp, #0
                        // todo
                        Operand *tmp = curMF->newVR();
                        MachineInst *pos = new I::Mov(mi, tmp, src, new Shift(LSL, bit_width(-imm2) - 1));
                        new I::Binary(pos, MITag::Rsb, dst, tmp, new Operand(I32, 0));
                        flag = true;
                    } else {
                        flag = false;
                    }
                    if (flag) {
                        mi->remove();
                    }
                } else if ((mi->noShift() || may_imm2 != std::nullopt) && mi->tag == MITag::LongMul) {
                    int imm2;
                    Operand *src;
                    if (may_imm2 != std::nullopt) {
                        imm2 = *may_imm2;
                        src = mi->useOpds[0];
                    } else if (may_imm1 != std::nullopt) {
                        imm2 = *may_imm1;
                        src = mi->useOpds[1];
                    } else {
                        continue;
                    }
                    bool flag = false;
                    if (imm2 == 0) {
                        nextMI = new I::Mov(mi, mi->defOpds[0], new Operand(I32, 0));
                        flag = true;
                    } else if (imm2 == 1) {
                        new I::Mov(mi, mi->defOpds[0], src, new Shift(ASR, new Operand(I32, 31)));
                        flag = true;
                    } else if (imm2 == -1) {
                        Operand *tmp = curMF->newVR();
                        MachineInst *pos = new I::Binary(mi, MITag::Rsb, tmp, src, new Operand(I32, 0));
                        new I::Mov(pos, mi->defOpds[0], tmp, new Shift(ASR, new Operand(I32, 31)));
                        flag = true;
                    } else if (IS_POWER2(imm2)) {
                        // auto const log_imm = bit_width(imm2) - 1;
                        new I::Mov(mi, mi->defOpds[0], src, new Shift(ASR, new Operand(I32, 33 - bit_width(imm2))));
                        flag = true;
                    } else {
                        flag = false;
                    }
                    if (flag) {
                        mi->remove();
                    }
                } else if (may_imm2 != std::nullopt) {
                    int imm2 = *may_imm2;
                    if (mi->tag == MITag::Add) {
                        if (imm2 == 0) {
                            new I::Mov(mi, mi->defOpds[0], mi->useOpds[0]);
                            mi->remove();
                        } else if (CodeGen::immCanCode(imm2)) {
                            mi->setUse(1, new Operand(I32, imm2));
                            mi->shift = Shift::NONE_SHIFT;
                        } else if (CodeGen::immCanCode(-imm2)) {
                            mi->setUse(1, new Operand(I32, -imm2));
                            mi->tag = MITag::Sub;
                            mi->shift = Shift::NONE_SHIFT;
                        }
                    } else if (mi->tag == MITag::Sub) {
                        if (imm2 == 0) {
                            new I::Mov(mi, mi->defOpds[0], mi->useOpds[0]);
                            mi->remove();
                        } else if (CodeGen::immCanCode(imm2)) {
                            mi->setUse(1, new Operand(I32, imm2));
                            mi->shift = Shift::NONE_SHIFT;
                        } else if (CodeGen::immCanCode(-imm2)) {
                            mi->setUse(1, new Operand(I32, -imm2));
                            mi->tag = MITag::Add;
                            mi->shift = Shift::NONE_SHIFT;
                        }
                    } else if (mi->tag == MITag::Rsb) {
                        // if(imm2 == 0)
                    } else if (mi->tag == MITag::Xor) {
                        if (imm2 == 0) {
                            new I::Mov(mi, mi->defOpds[0], mi->useOpds[0]);
                            mi->remove();
                        }
                    } else if (mi->tag == MITag::And) {
                        if (imm2 == 0) {
                            nextMI = new I::Mov(mi, mi->defOpds[0], new Operand(I32, 0));
                            mi->remove();
                        } else if (imm2 == 0xffffffff) {
                            new I::Mov(mi, mi->defOpds[0], mi->useOpds[0]);
                            mi->remove();
                        }
                    } else if (mi->tag == MITag::Or) {
                        if (imm2 == 0) {
                            new I::Mov(mi, mi->defOpds[0], mi->useOpds[0]);
                            mi->remove();
                        } else if (imm2 == 0xffff'ffff) {
                            nextMI = new I::Mov(mi, mi->defOpds[0], new Operand(I32, -1));
                            mi->remove();
                        }
                    }
                } else if (mi->noShift() && may_imm1 != std::nullopt) {
                    int imm1 = *may_imm1;
                    Operand *src = mi->useOpds[1];
                    Operand *dst = mi->defOpds[0];
                    if (mi->tag == MITag::Add) {
                        if (imm1 == 0) {
                            new I::Mov(mi, mi->defOpds[0], src);
                            mi->remove();
                        } else if (CodeGen::immCanCode(imm1)) {
                            new I::Binary(mi, MITag::Add, dst, src, new Operand(I32, imm1));
                            mi->remove();
                        } else if (CodeGen::immCanCode(-imm1)) {
                            new I::Binary(mi, MITag::Sub, dst, src, new Operand(I32, -imm1));
                            mi->remove();
                        }
                    } else if (mi->tag == MITag::Sub) {
                        if (imm1 == 0) {
                            new I::Mov(mi, mi->defOpds[0], src);
                            mi->remove();
                        } else if (CodeGen::immCanCode(imm1)) {
                            new I::Binary(mi, MITag::Rsb, dst, src, new Operand(I32, imm1));
                            mi->remove();
                        }
                    } else if (mi->tag == MITag::Div) {
                        if (imm1 == 0) {
                            nextMI = new I::Mov(mi, dst, new Operand(I32, 0));
                            mi->remove();
                        }
                    }
                } else if (!mi->noShift() && may_imm1 != std::nullopt) {
                    if (*mi->shift->shiftOpd == *Operand::I_ZERO) {
                        exit(72);
                    }
                    int imm1 = *may_imm1;
                    if (CodeGen::immCanCode(imm1) || CodeGen::immCanCode(-imm1)) {
                        if (mi->tag == MITag::Add) {
                            // add dst, imm1, src, xxx, #x/shift
                            Operand *tmp = curMF->newVR();
                            // assert(*mi->shift->shiftOpd != *Operand::I_ZERO);
                            MachineInst *pos = new I::Mov(mi, tmp, mi->useOpds[1], mi->shift);
                            MITag tag = MachineInst::Tag::Add;
                            if (!CodeGen::immCanCode(imm1)) {
                                tag = MachineInst::Tag::Sub;
                                imm1 = -imm1;
                            }
                            new I::Binary(pos, tag, mi->defOpds[0], tmp, new Operand(I32, imm1));
                            mi->remove();
                        } else if (mi->tag == MITag::Rsb) {
                            // nouse
                            // rsb dst, imm1, src, xxx, #x/shift
                            Operand *tmp = curMF->newVR();
                            // assert(*mi->shift->shiftOpd != *Operand::I_ZERO);
                            MachineInst *pos = new I::Mov(mi, tmp, mi->useOpds[1], mi->shift);
                            MITag tag = MachineInst::Tag::Sub;
                            if (!CodeGen::immCanCode(imm1)) {
                                tag = MachineInst::Tag::Add;
                                imm1 = -imm1;
                            }
                            new I::Binary(pos, tag, mi->defOpds[0], tmp, new Operand(I32, imm1));
                            mi->remove();
                        } else if (mi->tag == MITag::Sub && CodeGen::immCanCode(imm1)) {
                            // sub dst, imm1, src, xxx, #x/shift
                            Operand *tmp = curMF->newVR();
                            // assert(*mi->shift->shiftOpd != *Operand::I_ZERO);
                            MachineInst *pos = new I::Mov(mi, tmp, mi->useOpds[1], mi->shift);
                            new I::Binary(pos, MITag::Rsb, mi->defOpds[0], tmp, new Operand(I32, imm1));
                            mi->remove();
                        }
                    }
                }
            }
        }
    }
}

void BKConstFold::gep_broadcast(McFunction *mf) {
    std::set<Operand *, CompareOperand> singleDefVR;
    std::map<Operand *, std::set<MachineInst *>, CompareOperand> ssaVR2MI;
    FOR_ITER_MB(mb, mf) {
        FOR_ITER_MI(mi, mb) {
            mi->singleDef = false;
            for (Operand *def: mi->defOpds) {
                ssaVR2MI[def].insert(mi);
            }
        }
    }
    for (auto const &it: ssaVR2MI) {
        if (it.second.size() == 1) {
            singleDefVR.insert(it.first);
            (*it.second.begin())->singleDef = true;
        }
    }
}

void propagate_constants(McFunction *mf) {
    FOR_ITER_MB(mb, mf) {
        std::map<Operand *, int> int_vals;
        FOR_ITER_MI(mi, mb) {
            if (CAST_IF(iMov, mi, I::Mov)) {
                if (mi->noShift()) {
                    Operand *dst = iMov->getDst();
                    Operand *src = iMov->getSrc();
                    std::optional<int> opt_imm;
                    auto const it = mf->reg2val.find(src);
                    if (it != mf->reg2val.end()) {
                        auto &val = mf->reg2val.at(src);
                        if (val.index() == RegValueType::Immediate)
                            opt_imm = std::get<Immediate>(val);
                    }
                    if (int_vals.count(src))
                        opt_imm = int_vals.at(src);

                    if (opt_imm) {
                        int imm = *opt_imm;
                        int_vals[dst] = imm;
                        if (CodeGen::immCanCode(imm))
                            iMov->setUse(0, new Operand(I32, imm));
                        // else if (CodeGen::immCanCode(~imm)) {
                        //     iMov->setUse(0,new Operand(I32, imm));
                        //     iMov->isMvn = !iMov->isMvn;
                    }
                }
            }
        }
    }
}

void BKConstFold::run(McProgram *p) {
    FOR_ITER_MF(mf, p) {
        const_fold(mf);
    }
}