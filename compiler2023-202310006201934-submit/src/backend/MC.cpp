//
// Created by XuRuiyuan on 2023/7/22.
//

#include "backend/MC.h"
#include "lir/Arm.h"
#include "backend/CodeGen.h"
#include "lir/MachineInst.h"
#include "mir/Type.h"
#include "frontend/Initial.h"
#include "util/util.h"
#include "../../include/mir/Function.h"
#include "lir/Operand.h"
#include "mir/BasicBlock.h"
#include "lir/I.h"
#include "iostream"
#include "Instr.h"
#include "utility"

#include <fstream>
#include "RegAllocator.h"
#include "Parallel.h"
#include "V.h"

McProgram::McProgram() {
    beginMF = new McFunction;
    endMF = new McFunction;
    beginMF->next = endMF;
    endMF->prev = beginMF;
}

// void McProgram::globalDataStbHelper(std::ostream &stb, std::vector<Arm::Glob *> *globData) {
//
// }

// void McProgram::bssInit() {
//     for (Arm::Glob *glob: globList) {
//         Flatten *flatten = glob->init->flatten();
//         int countNonZeros = 0;
//         for (Flatten::Entry entry: flatten) {
//             Value *value = entry->value;
//             if (value->opdType == BasicType::F32_TYPE) {
//                 int bin = ((ConstantFloat *) value)->get_int_bits();
//                 if (bin != 0) {
//                     countNonZeros += entry->count;
//                 }
//             } else {
//                 if (value != ConstantInt::CONST_0) {
//                     countNonZeros += entry->count;
//                 }
//             }
//         }
//         if (countNonZeros * 3 >= flatten->sizeInWords()) {
//             globData.push_back(glob);
//         } else {
//             globBss.push_back(glob);
//         }
//     }
//     for (Arm::Glob *glob: globBss) {
//         Operand *rBase = getRSReg(r3), rOffset = getRSReg(r2), rData = getRSReg(r1);
//         specialMI = new I::Mov(specialMI, rBase, glob);
//         Flatten *flatten = glob->init->flatten();
//         int offset = 0;
//         for (Flatten::Entry entry: flatten) {
//             int end = offset + entry->count;
//             while (offset < end) {
//                 Value *value = entry->value;
//                 if (value->opdType == BasicType::F32_TYPE) {
//                     int bin = ((ConstantFloat *) value)->getIntBits();
//                     if (bin != 0) {
//                         if (LdrStrImmEncode(offset * 4)) {
//                             specialMI = new I::Mov(specialMI, rData, new Operand(DataType::I32, bin));
//                             specialMI = new I::Str(specialMI, rData, rBase, new Operand(DataType::I32, offset * 4));
//                         } else {
//                             specialMI = new I::Mov(specialMI, rData, new Operand(DataType::I32, bin));
//                             specialMI = new I::Mov(specialMI, rOffset, new Operand(DataType::I32, offset));
//                             specialMI = new I::Str(specialMI, rData, rBase, rOffset,
//                                                    new Arm::Shift(Arm::ShiftType::Lsl, 2));
//                         }
//                     }
//                 } else {
//                     if (!value == CONST_0) {
//                         if (LdrStrImmEncode(offset * 4)) {
//                             specialMI = new I::Mov(specialMI, rData, new Operand(DataType::I32,
//                                                                                  (int) ((Constant) value)->get_const_val()));
//                             specialMI = new I::Str(specialMI, rData, rBase, new Operand(DataType::I32, offset * 4));
//                         } else {
//                             specialMI = new I::Mov(specialMI, rData, new Operand(DataType::I32,
//                                                                                  (int) ((Constant) value)->get_const_val()));
//                             specialMI = new I::Mov(specialMI, rOffset, new Operand(DataType::I32, offset));
//                             specialMI = new I::Str(specialMI, rData, rBase, rOffset,
//                                                    new Arm::Shift(Arm::ShiftType::Lsl, 2));
//                         }
//                     }
//                 }
//                 offset++;
//             }
//         }
//     }
// }

void McProgram::bssInit() {
    // split glob to .data and .bss
    for (Arm::Glob *glob: globList) {
        // put to .data: over 1/3 non-zero values
        Flatten *flatten = glob->init->flatten();
        int countNonZeros = 0;
        for (Flatten::Entry *entry = (Flatten::Entry *) flatten->begin->next; entry->has_next(); entry = (Flatten::Entry *) entry->next) {
            Value *value = entry->value;
            if (value->type->is_float_type()) {
                float f = ((ConstantFloat *) value)->get_const_val();
                if (f != 0) {
                    countNonZeros += entry->count;
                }
            } else {
                assert(value->type->is_int32_type());
                if (((ConstantInt *) value)->get_const_val() != 0) {
                    countNonZeros += entry->count;
                }
            }
        }
        std::cerr << "glob: " << glob->name << " non-zeros: " << countNonZeros;
        if (countNonZeros * 3 >= flatten->sizeInWords()) {
            globData.push_back(glob);
            std::cerr << " -> .data" << std::endl;
        } else {
            globBss.push_back(glob);
            std::cerr << " -> .bss" << std::endl;
        }
    }

    assert(specialMI != nullptr);

    for (Arm::Glob *glob: globBss) {
        Operand *rBase = Arm::Reg::getRreg(Arm::GPR::r3);
        Operand *rOff = Arm::Reg::getRreg(Arm::GPR::r2);
        Operand *rData = Arm::Reg::getRreg(Arm::GPR::r1);
        specialMI = new I::Mov(specialMI, rBase, glob);
        int offset = 0;
        Flatten *flatten = glob->init->flatten();
        for (Flatten::Entry *entry = (Flatten::Entry *) flatten->begin->next; entry->has_next(); entry = (Flatten::Entry *) entry->next) {
            int end = offset + entry->count;
            while (offset < end) {
                Value *value = entry->value;
                if (value->type->is_float_type()) {
                    float f = ((ConstantFloat *) value)->get_const_val();
                    int bin = *(int *) (&f);
                    if (bin != 0) {
                        if (CodeGen::LdrStrImmEncode(offset * 4)) {
                            specialMI = new I::Mov(specialMI, rData, new Operand(DataType::I32, bin));
                            specialMI = new I::Str(specialMI, rData, rBase, new Operand(DataType::I32, offset * 4));
                        } else {
                            specialMI = new I::Mov(specialMI, rData, new Operand(DataType::I32, bin));
                            specialMI = new I::Mov(specialMI, rOff, new Operand(DataType::I32, offset));
                            specialMI = new I::Str(specialMI, rData, rBase, rOff,
                                                   new Arm::Shift(Arm::ShiftType::Lsl, 2));
                        }
                    }
                } else {
                    assert(value->type->is_int32_type());
                    int val = ((ConstantInt *) value)->get_const_val();
                    if (val != 0) {
                        if (CodeGen::LdrStrImmEncode(offset * 4)) {
                            specialMI = new I::Mov(specialMI, rData, new Operand(DataType::I32, val));
                            specialMI = new I::Str(specialMI, rData, rBase, new Operand(DataType::I32, offset * 4));
                        } else {
                            specialMI = new I::Mov(specialMI, rData, new Operand(DataType::I32, val));
                            specialMI = new I::Mov(specialMI, rOff, new Operand(DataType::I32, offset));
                            specialMI = new I::Str(specialMI, rData, rBase, rOff,
                                                   new Arm::Shift(Arm::ShiftType::Lsl, 2));
                        }
                    }
                }
                offset++;
            }
        }
    }
    if (CenterControl::_GEP_CONST_BROADCAST && CenterControl::_O2) {
        specialMI->mb->mf->functionInsertedMI = specialMI;
    }
}

std::ostream &operator<<(std::ostream &stream, const McProgram *p) {
    // std::ostream stream = new std::ostream();
    stream << "@ " << CenterControl::_TAG << "\n";
    stream << ".arch armv7ve\n.arm\n";
    if (CodeGen::needFPU) {
        stream << ".fpu vfpv3-d16\n";
    }
    stream << ".section .text\n";
    FOR_ITER_MF(function, p) {
        stream << "\n\n.global\t" << function->getName() << std::endl;
#ifdef _BACKEND_COMMENT_OUTPUT
        stream << "@ regStackSize =\t" << function->regStack << " ;\n@ varStackSize =\t" << function->varStack
               << " ;\n@ paramStackSize =\t" << function->paramStack << " ;" << std::endl;
        stream << "@ usedCalleeSavedGPRs =\t[";
        unsigned int num = function->usedCalleeSavedGPRs;
        int i = 0;
        bool first = true;
        while (num) {
            if ((num & 1u) != 0) {
                if (!first) {
                    stream << ", ";
                } else {
                    first = false;
                }
                stream << Arm::gprStrList[i];
            }
            i++;
            num >>= 1;
        }
        stream << "];\n@ usedCalleeSavedFPRs =\t[";
        i = 0;
        first = true;
        num = function->usedCalleeSavedFPRs;
        while (num) {
            if ((num & 1u) != 0) {
                if (!first) {
                    stream << ", ";
                } else {
                    first = false;
                }
                stream << "s" << i;
            }
            i++;
            num >>= 1;
        }
        stream << "];" << std::endl;
        // print machine blocks
        stream << "@ MBs: [";
        FOR_ITER_MB(mb, function) {
            stream << mb->getLabel();
            if (mb->next->next != nullptr) stream << ", ";
        }
        stream << "];" << std::endl;
#endif
        stream << function->getName() << ":\n.p2align 4" << std::endl;
#ifdef LIVENESS_OUT
        RegAllocator::mixedLivenessAnalysis(function);
#endif
        FOR_ITER_MB(mb, function) {
            int num = 0;
            FOR_ITER_MI(mi, mb) {
                if (mi->tag == MachineInst::Tag::Push) {
                    num = 10000;
                    break;
                }
                num++;
            }
            if (num == 1 && !dynamic_cast<MachineInst *>(mb->beginMI->next)->sideEff()) {
                stream << ".p2align 4" << std::endl;
            }
            if (num > 8)
                stream << ".p2align 4" << std::endl;
            stream << mb->getLabel() << ":" << std::endl;
#ifdef _BACKEND_COMMENT_OUTPUT
            stream << "@ pred:\t[";
            for (McBlock *it: mb->predMBs) {
                stream << it->getLabel() << ", ";
            }
            stream << "]\n@ succ:\t[";
            for (McBlock *it: mb->succMBs) {
                stream << it->getLabel() << ", ";
            }
            stream << "]\n";
#endif
#ifdef LIVENESS_OUT
            std::set<Operand *, CompareOperand> visitSet;
            if (mb->liveUseSet) {
                stream << "@ LiveUse:\t[";
                visitSet.clear();
                for(Operand *o: *mb->liveUseSet) {
                    visitSet.insert(o);
                }
                for (const Operand *o: visitSet) {
                    stream << std::string(*o) << ", ";
                }
                stream << "]" << std::endl;
            }
            if (mb->liveDefSet) {
                stream << "@ LiveDef:\t[";
                visitSet.clear();
                for(Operand *o: *mb->liveDefSet) {
                    visitSet.insert(o);
                }
                for (const Operand *o: visitSet) {
                    stream << std::string(*o) << ", ";
                }
                stream << "]" << std::endl;
            }
            if (mb->liveInSet) {
                stream << "@ LiveIn:\t[";
                visitSet.clear();
                for(Operand *o: *mb->liveInSet) {
                    visitSet.insert(o);
                }
                for (const Operand *o: visitSet) {
                    stream << std::string(*o) << ", ";
                }
                stream << "]" << std::endl;
            }
            if (mb->liveOutSet) {
                stream << "@ LiveOut:\t[";
                visitSet.clear();
                for(Operand *o: *mb->liveOutSet) {
                    visitSet.insert(o);
                }
                for (const Operand *o: visitSet) {
                    stream << std::string(*o) << ", ";
                }
                stream << "]" << std::endl;
            }
#endif
            FOR_ITER_MI(inst, mb) {
                if (!inst->isComment()) {
                    stream << "\t";
                }
                // std::cerr << std::string(*inst) << std::endl;
                // if(inst->tag == MachineInst::Tag::Add) {
                //     int a=0;
                // }
                stream << std::string(*inst) << std::endl;
            }

            stream << std::endl;
        }
    }
    stream << CenterControl::OUR_MEMSET_TAG << ":\n"
                                               "    add r1, r0, r1\n"
                                               "    mov r2, #0\n"
                                               "1:\n"
                                               "    str r2, [r0]\n"
                                               "    add r0, r0, #4\n"
                                               "    cmp r0, r1\n"
                                               "    blt 1b\n"
                                               "    bx  lr\n";
    stream << std::endl;
    for (const auto &entry: CodeGen::name2constFOpd) {
        std::string name = entry.first;
        ConstantFloat *constF = entry.second->constF;
        int i = constF->getIntBits();
        stream << name << ":" << std::endl;
        stream << "\t.word\t" << i << std::endl;
    }
    if (CenterControl::_GLOBAL_BSS && CenterControl::_O2) {
        stream << ".section .bss\n";
        stream << ".align 2\n";
        for (Arm::Glob *glob: p->globBss) {
            stream << "\n.global\t" << glob->getGlob() << std::endl;
            stream << glob->getGlob() << ":\n";
            if (glob->init != nullptr) {
                Flatten *flatten = glob->init->flatten();
                stream << ".zero " << flatten->sizeInBytes() << std::endl;
            } else {
                stream << ".zero 4\n";
            }
        }
        McProgram::globalDataStbHelper(stream, &(p->globData));
// #ifdef _OPEN_PARALLEL_BACKEND
        if (CenterControl::_OPEN_PARALLEL) {
            for (const auto &it: Parallel::PARALLEL->tmpGlob) {
                Arm::Glob *glob = it.second;
                stream << "\n.global\t" << glob->getGlob() << std::endl;
                stream << glob->getGlob() << ":\n";
                stream << "\t.word\t0\n";
            }
        }
// #endif
    } else {
        McProgram::globalDataStbHelper(stream, &(p->globList));
    }
    return stream;
}

void McProgram::output(std::string filename) const {
    std::ofstream fout(filename);
    fout << this << std::endl;
    fout.close();
}

void McProgram::globalDataStbHelper(std::ostream &stb, const std::vector<Arm::Glob *> *globData) {
    stb << ".section .data" << std::endl;
    stb << ".align 2" << std::endl;
    for (Arm::Glob *glob: *globData) {
        stb << "\n.global\t" << glob->getGlob() << std::endl;
        stb << glob->getGlob() << ":" << std::endl;
        if (glob->init != nullptr) {
            Flatten *flatten = glob->init->flatten();
            stb << std::string(*flatten);
        } else {
            stb << "\t.word\t0" << std::endl;
        }
    }
}


McBlock *McFunction::getBeginMB() {
    return (McBlock *) beginMB->prev;
}

void McFunction::insertAtEnd(McBlock *mb) {
    endMB->insert_before(mb);
}

McBlock *McFunction::getEndMB() {
    return (McBlock *) endMB;
}

// std::vector<Arm::GPRs *> McFunction::getUsedRegList() {
//     return new std::vector<Arm::GPRs *>{*usedCalleeSavedGPRs};
// }

McFunction::McFunction() {
    beginMB = new McBlock();
    endMB = new McBlock();
    beginMB->next = endMB;
    endMB->prev = beginMB;
    // McProgram::MC_PROGRAM->endMF->insert_before(this);
}

McFunction::McFunction(Function *function) {
    beginMB = new McBlock();
    endMB = new McBlock();
    beginMB->next = endMB;
    endMB->prev = beginMB;
    this->mFunc = function;
    // McProgram::MC_PROGRAM->endMF->insert_before(this);
}

McFunction::McFunction(std::string name) {
    beginMB = new McBlock();
    endMB = new McBlock();
    beginMB->next = endMB;
    endMB->prev = beginMB;
    this->name = name;
    // McProgram::MC_PROGRAM->endMF->insert_before(this);
}

std::string McFunction::getName() {
    if (mFunc != nullptr) return mFunc->name;
    return name;
}

void McFunction::addVarStack(int i) {
    varStack += i;
}

void McFunction::addParamStack(int i) {
    paramStack += i;
}

void McFunction::alignParamStack() {
    int b = paramStack % SP_ALIGN;
    if (b != 0) {
        paramStack += SP_ALIGN - b;
    }
}

void McFunction::addRegStack(int i) {
    regStack += i;
}

int McFunction::getTotalStackSize() {
    return varStack + paramStack + regStack;
}

int McFunction::getParamStack() {
    return paramStack;
}

void McFunction::setAllocStack() {
    allocStack = varStack;
}

int McFunction::getAllocStack() {
    return allocStack;
}

int McFunction::getVarStack() {
    return varStack;
}

int McFunction::getVRSize() {
    return vrCount;
}

int McFunction::getSVRSize() {
    return sVrCount;
}

Operand *McFunction::getVR(int idx) {
    return vrList[idx];
}

Operand *McFunction::getSVR(int idx) {
    return sVrList[idx];
}

Operand *McFunction::newVR() {
    Operand *vr = new Operand(vrCount++, DataType::I32);
    vrList.push_back(vr);
    return vr;
}

Operand *McFunction::newSVR() {
    Operand *sVr = new Operand(sVrCount++, DataType::F32);
    sVrList.push_back(sVr);
    return sVr;
}

void McFunction::clearVRCount() {
    vrCount = 0;
    this->vrList.clear();
}

void McFunction::clearSVRCount() {
    sVrCount = 0;
    this->sVrList.clear();
}

void McFunction::addUsedGPRs(Arm::Regs *reg) {
    if (reg->isGPR) {
        if (reg->value == (int) Arm::GPR::sp || reg->value < MIN(intParamCount, CodeGen::rParamCnt) ||
            (this->mFunc->retType->is_int32_type() && reg->value == 0)) {
            return;
        }
        if (NUM_AND_BIT(usedCalleeSavedGPRs, reg->value) == 0) {
            NUM_SET_BIT(usedCalleeSavedGPRs, reg->value);
            addRegStack(4);
        }
    }
}

void McFunction::addUsedGPRs(Arm::GPR gid) {
    if (gid == Arm::GPR::sp || ((int) gid) < MIN(intParamCount, CodeGen::rParamCnt)
        || (this->mFunc->retType->is_int32_type() && ((int) gid) == 0)) {
        return;
    }
    if (NUM_AND_BIT(usedCalleeSavedGPRs, ((unsigned int) gid)) == 0) {
        NUM_SET_BIT(usedCalleeSavedGPRs, ((unsigned int) gid));
        addRegStack(4);
    }
}

void McFunction::addUsedFRPs(Arm::Regs *reg) {
    if (reg->isFPR) {
        if (reg->value < MIN(floatParamCount, CodeGen::sParamCnt) ||
            (this->mFunc->retType->is_float_type() && reg->value == 0)) {
            return;
        }
        if (reg->value < 2) return;
        if (NUM_NO_BIT(usedCalleeSavedFPRs, reg->value)) {
            NUM_SET_BIT(usedCalleeSavedFPRs, (reg->value));
            addRegStack(4);
            int idx = reg->value;
            if (idx % 2 == 0) {
                if (NUM_NO_BIT(usedCalleeSavedFPRs, (idx + 1))) {
                    NUM_SET_BIT(usedCalleeSavedFPRs, (idx + 1));
                    addRegStack(4);
                }
            } else {
                if (NUM_NO_BIT(usedCalleeSavedFPRs, (idx - 1))) {
                    NUM_SET_BIT(usedCalleeSavedFPRs, (idx - 1));
                    addRegStack(4);
                }
            }
        }
    }
}

void McFunction::setUseLr() {
    useLr = true;
    addUsedGPRs(Arm::GPR::lr);
}

int McFunction::getRegStack() const {
    return regStack;
}

void McFunction::alignTotalStackSize() {
    int totalSize = getTotalStackSize();
    int b = totalSize % SP_ALIGN;
    if (b != 0) {
        varStack += SP_ALIGN - b;
    }
}

std::optional<int> McFunction::get_imm(Operand *o) {
    if (o->is_I_Imm()) return o->value;
    auto const it = constMap.find(o);
    if (it != constMap.end()) {
        return it->second.iv;
    } else {
        return {};
    }
}

std::optional<float> McFunction::get_fconst(Operand *pOperand) {
    // if (pOperand->isFConst) return pOperand->value;
    auto const it = constMap.find(pOperand);
    if (it != constMap.end()) {
        return it->second.iv;
    } else {
        return {};
    }
}

void McFunction::remove_phi() {
    for (auto &it: phi2tmpMap) {
        INSTR::Phi *phiInst = it.first;
        McBlock *phiMB = CodeGen::CODEGEN->bb2mb[phiInst->bb];
        // Operand *vr = it.second.first;
        Operand *phiVR = value2Opd[phiInst];
        Operand *tmp = it.second;
        int idx = 0;
        for (BasicBlock *predBB: *phiInst->bb->precBBs) {
            McBlock *predMB = CodeGen::CODEGEN->bb2mb[predBB];
            MachineInst *mi = dynamic_cast<MachineInst *>(predMB->endMI->prev);
            for (; mi->prev != nullptr; mi = dynamic_cast<MachineInst *>(mi->prev)) {
                if (!(mi->isBranch() || mi->isJump())) {
                    break;
                }
            }
            mi = dynamic_cast<MachineInst *>(mi->next);
            assert(mi->prev != nullptr);
            if (phiVR->dataType == DataType::F32) {
                new V::Mov(tmp, value2Opd[(phiInst->useValueList)[idx]], mi);
            } else {
                new I::Mov(tmp, value2Opd[(phiInst->useValueList)[idx]], mi);
            }
            idx++;
        }
    }
}

MachineInst *McBlock::getBeginMI() const {
    return (MachineInst *) beginMI->next;
}

MachineInst *McBlock::getEndMI() const {
    return (MachineInst *) endMI->prev;
}

void McBlock::insertAtEnd(MachineInst *in) {
    endMI->insert_before(in);
}

void McBlock::insertAtHead(MachineInst *in) {
    beginMI->insert_after(in);
}

McBlock::McBlock() {
    beginMI = new MachineInst();
    endMI = new MachineInst();
    beginMI->next = endMI;
    endMI->prev = beginMI;
}

McBlock::McBlock(McBlock *curMB, McBlock *pred) {
    beginMI = new MachineInst();
    endMI = new MachineInst();
    beginMI->next = endMI;
    endMI->prev = beginMI;
    this->mf = pred->mf;
    mb_idx = globIndex++;
    label = MB_Prefix + std::to_string(mb_idx) + curMB->getLabel();
    pred->insert_after(this);
}

McBlock::McBlock(BasicBlock *bb) {
    beginMI = new MachineInst();
    endMI = new MachineInst();
    beginMI->next = endMI;
    endMI->prev = beginMI;
    this->bb = bb;
    mb_idx = globIndex++;
}

McBlock::McBlock(std::string label, McFunction *insertAtEnd) {
    beginMI = new MachineInst();
    endMI = new MachineInst();
    beginMI->next = endMI;
    endMI->prev = beginMI;
    this->bb = nullptr;
    this->label = std::move(label);
    mf = insertAtEnd;
    mf->insertAtEnd(this);
}

std::string McBlock::getLabel() const {
    return (bb == nullptr ? label : MB_Prefix + std::to_string(mb_idx) + "_" + bb->label);
}

void McBlock::setMf(McFunction *mf) {
    this->mf = mf;
    mf->insertAtEnd(this);
}

McBlock *McBlock::falseSucc() {
    if (succMBs.size() < 2) return nullptr;
    return succMBs[1];
}

McBlock *McBlock::trueSucc() {
    return succMBs[0];
}

void McBlock::setFalseSucc(McBlock *onlySuccMB) {
    succMBs[1] = onlySuccMB;
}

void McBlock::setTrueSucc(McBlock *onlySuccMB) {
    succMBs[0] = onlySuccMB;
}

