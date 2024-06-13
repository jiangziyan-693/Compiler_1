#include "../../include/backend/CodeGen.h"
#include "../../include/util/Manager.h"
#include "../../include/mir/Function.h"
#include "backend/MC.h"
#include "lir/MachineInst.h"
#include "lir/Arm.h"
#include "lir/I.h"
#include "lir/V.h"
#include "lir/StackCtrl.h"
#include "util/util.h"
#include "mir/Use.h"
#include "mir/GlobalVal.h"
#include "mir/Value.h"
#include "mir/Instr.h"
#include "mir/Type.h"
#include "mir/BasicBlock.h"
#include "BlockRelationDealer.h"

#include <functional>
// #include <limits>
#include <ostream>
// #include <utility>

CodeGen::CodeGen() {
    curFunc = nullptr;
    curMF = nullptr;
    // fMap = Manager::MANAGER->functions;
    // globalMap = Manager::MANAGER->globals;
    // fMap = new std::map<std::string, Function *>();
    // for (const auto &it: *Manager::MANAGER->functions) {
    //     (*fMap)[it.first] = it.second;
    // }
    // globalMap = new std::map<GlobalValue *, Initial *, CompareGlobalValue>();
    // for (const auto &it: *Manager::MANAGER->globals) {
    //     (*globalMap)[it.first] = it.second;
    // }
    // curMF->value2Opd.clear();
    f2mf.clear();
    bb2mb.clear();
    // _DEBUG_OUTPUT_MIR_INTO_COMMENT = true;
}

// CodeGen *CodeGen::CODEGEN = new CodeGen();

void CodeGen::gen() {
    needFPU = Lexer::detect_float;
    fMap = new std::map<std::string, Function *>();
    for (const auto &it: *Manager::MANAGER->functions) {
        (*fMap)[it.first] = it.second;
    }
    globalMap = new std::map<GlobalValue *, Initial *, CompareGlobalValue>();
    for (const auto &it: *Manager::MANAGER->globals) {
        (*globalMap)[it.first] = it.second;
    }
    genGlobal();

    for (const auto &it: *fMap) {
        Function *func = it.second;
        if (func->is_deleted) continue;
        McFunction *mcFunc = new McFunction(func);
        f2mf[func] = mcFunc;
        if (func == ExternFunction::MEM_SET) {
            assert(func->params->size() == 3);
            func->params->erase(func->params->begin() + 1);
        }
        for (Function::Param *p: *func->params) {
            if (p->type->is_float_type()) {
                mcFunc->floatParamCount++;
            } else {
                mcFunc->intParamCount++;
            }
        }
    }
    if (CenterControl::USE_ESTIMATE_BASE_SORT && CenterControl::_O2) {
        BBSort::EstimateBaseSort();
    } else {
        BBSort::DefaultSort();
    }
    for (const auto &it: *fMap) {
        Function *func = it.second;
        if (func->is_deleted) continue;
        if (func->isExternal) {
            continue;
        }
        curMF = f2mf[func];
        curMF->setUseLr();
        allocMap.clear();
        curMF->phi2tmpMap.clear();
        McProgram::MC_PROGRAM->endMF->insert_before(curMF);
        curFunc = func;
        bool isMain = false;
        if (curFunc->name == "main") {
            isMain = true;
            McProgram::MC_PROGRAM->mainMcFunc = curMF;
            curMF->isMain = true;
        }
        curMF->clearVRCount();
        curMF->clearSVRCount();
        curMF->value2Opd.clear();
        cmpInst2MICmpMap.clear();
        BasicBlock *bb = func->getBeginBB();
        BasicBlock *endBB = func->end;
        // int cnt = 0;
        while (bb->next != nullptr) {
            // std::cerr << bb->label << std::endl;
            McBlock *mb = new McBlock(bb);
            bb->mb = mb;
            bb2mb[bb] = mb;
            bb = (BasicBlock *) bb->next;
        }
        bb = func->getBeginBB();
        curMB = bb->mb;
        Push(curMF);
        I::Binary *bino = new I::Binary(MachineInst::Tag::Sub, Arm::Reg::getR(Arm::GPR::sp),
                                        Arm::Reg::getR(Arm::GPR::sp),
                                        new Operand(DataType::I32, 0),
                                        curMB);
        bino->setNeedFix(ENUM::VAR_STACK);
        curMF->functionInsertedMI = bino;
        if (!isMain) {
            dealParam();
        } else {
            // if (curMF->isMain && CenterControl::_GLOBAL_BSS) McProgram::specialMI = bino;
        }
        // std::cerr << func->getName() << ":[newBBs size]\t" << func->newBBs.size() << std::endl;
        for (auto it1 = func->newBBs.begin(); it1 != func->newBBs.end(); it1++) {
            // bb2mb.erase(*it1);
            genBB(it1);
        }
        // for(auto &it : bb2mb) {
        //     std::cerr << it.first->label << std::endl;
        // }
        // if (CenterControl::USE_ESTIMATE_BASE_SORT) {
        //     for (BasicBlock *visitBB: func->newBBs) {
        //         genBB(visitBB);
        //     }
        // } else {
        //     nextBBList.push(bb);
        //     while (!nextBBList.empty()) {
        //         BasicBlock *visitBB = nextBBList.top();
        //         nextBBList.pop();
        //         genBB(visitBB);
        //     }
        // }
        FOR_ITER_MB(mb, curMF) {
            FOR_ITER_MI(mi, mb) {
                if (dynamic_cast<I::Binary *>(mi) != nullptr || mi->isOf(MachineInst::Tag::Ldr) ||
                    mi->isOf(MachineInst::Tag::LongMul) || mi->isIMov() || mi->isOf(MachineInst::Tag::FMA)) {
                    mi->calCost();
                }
            }
        }
        if (CenterControl::_GEP_CONST_BROADCAST && CenterControl::_O2) {
            // MachineInst *insertAfter = curMF->functionInsertedMI;
            for (auto &iter: curMF->reg2val) {
                Operand *vr = iter.first;
                RegValue &regVal = iter.second;
                if (regVal.index() == Immediate) {
                    int val = std::get<int>(regVal);
                    curMF->functionInsertedMI = new I::Mov(curMF->functionInsertedMI, vr,
                                                           new Operand(DataType::I32, val));
                } else if (regVal.index() == GlobBase) {
                    Arm::Glob *glob = std::get<Arm::Glob *>(regVal);
                    curMF->functionInsertedMI = new I::Mov(curMF->functionInsertedMI, vr, glob);
                    curMF->functionInsertedMI->setGlobAddr();
                } else if (regVal.index() == StackAddr) {
                    SpBasedOffAndSize sa = std::get<SpBasedOffAndSize>(regVal);
                    Operand *tmp = nullptr;
                    if (!CodeGen::immCanCode(sa.offset)) {
                        tmp = curMF->newVR();
                        curMF->functionInsertedMI = new I::Mov(curMF->functionInsertedMI, tmp,
                                                               new Operand(DataType::I32, sa.offset));
                    } else {
                        tmp = new Operand(DataType::I32, sa.offset);
                    }
                    curMF->functionInsertedMI = new I::Binary(curMF->functionInsertedMI, MachineInst::Tag::Add, vr,
                                                              Arm::Reg::getRreg(Arm::GPR::sp),
                                                              tmp);
                }
            }
        }
    }
}

void CodeGen::Push(McFunction *mf) {
    if (needFPU) new StackCtl::VPush(mf, curMB);
    MachineInst *tmp = new StackCtl::Push(mf, curMB);
    if (curMF->isMain && CenterControl::_GLOBAL_BSS) McProgram::specialMI = tmp;
}

void CodeGen::Pop(McFunction *mf) {
    new StackCtl::Pop(mf, curMB);
    if (needFPU) new StackCtl::VPop(mf, curMB);
}

void CodeGen::dealParam() {
    int rIdx = 0;
    int sIdx = 0;
    int rTop = rIdx;
    int sTop = sIdx;
    for (Function::Param *param: *curFunc->params) {
        if (param->type->is_float_type()) {
            Operand *opd = newSVR();
            curMF->value2Opd[(Value *) param] = opd;
            if (sIdx < sParamCnt) {
                new V::Mov(opd, Arm::Reg::getS(sIdx), curMB);
                sTop = sIdx + 1;
            } else {
                Operand *offImm = new Operand(DataType::I32, 4 * (rTop + sTop - (rIdx + sIdx) - 1));
                Operand *imm = newVR();
                I::Mov *mv = new I::Mov(imm, offImm, curMB);
                mv->setNeedFix(ENUM::FLOAT_TOTAL_STACK);
                Operand *dstAddr = newVR();
                new I::Binary(MachineInst::Tag::Add, dstAddr, Arm::Reg::getR(Arm::GPR::sp), imm, curMB);
                new V::Ldr(opd, dstAddr, curMB);
                curMF->addParamStack(4);
            }
            sIdx++;
        } else {
            Operand *opd = newVR();
            curMF->value2Opd[(Value *) param] = opd;
            if (rIdx < rParamCnt) {
                new I::Mov(opd, Arm::Reg::getR(rIdx), curMB);
                rTop = rIdx + 1;
            } else {
                Operand *offImm = new Operand(DataType::I32, 4 * (rTop + sTop - (rIdx + sIdx) - 1));
                Operand *dst = newVR();
                I::Mov *mv = new I::Mov(dst, offImm, curMB);
                mv->setNeedFix(ENUM::INT_TOTAL_STACK);
                new I::Ldr(opd, Arm::Reg::getR(Arm::GPR::sp), dst, curMB);
                curMF->addParamStack(4);
            }
            rIdx++;
        }
    }
    curMF->alignParamStack();
}

int numberOfTrailingZeros(int i) {
    // HD, Count trailing 0's
    i = ~i & (i - 1);
    if (i <= 0) return i & 32;
    int n = 1;
    if (i > 1 << 16) {
        n += 16;
        i >>= 16;
    }
    if (i > 1 << 8) {
        n += 8;
        i >>= 8;
    }
    if (i > 1 << 4) {
        n += 4;
        i >>= 4;
    }
    if (i > 1 << 2) {
        n += 2;
        i >>= 2;
    }
    return n + (i >> 1);
}

void CodeGen::genBB(std::vector<BasicBlock *>::iterator it) {
    BasicBlock *bb = *it;
    // std::cerr << bb->label << std::endl;
    curMB = bb->mb;
    visitBBSet.insert(curMB);
    curMB->setMf(curMF);
    Instr *instr = bb->getBeginInstr();
    while (instr->next != nullptr) {
        // if (_DEBUG_OUTPUT_MIR_INTO_COMMENT) {
        new MIComment(std::string(*instr), curMB);
        // }
        if (instr->tag == ValueTag::value
            || instr->tag == ValueTag::func
            || instr->tag == ValueTag::bb) {
            exit(107);
        } else if (instr->tag == ValueTag::bino) {
            genBinaryInst((INSTR::Alu *) instr);
        } else if (instr->tag == ValueTag::jump) {
            McBlock *mb = ((INSTR::Jump *) instr)->getTarget()->mb;
            curMB->succMBs.push_back(mb);
            mb->predMBs.insert(curMB);
            if (mb->predMBs.size() > 8) CenterControl::_PREC_MB_TOO_MUCH = true;
            if (visitBBSet.find(mb) == visitBBSet.end()) {
                nextBBList.push(mb->bb);
                visitBBSet.insert(mb);
            }
            new GDJump(mb, curMB);
        } else if (instr->tag == ValueTag::icmp
                   || instr->tag == ValueTag::fcmp) {
            genCmp(instr);
        } else if (instr->tag == ValueTag::branch) {
            Arm::Cond cond = Arm::Cond::Eq;
            INSTR::Branch *brInst = (INSTR::Branch *) instr;
            McBlock *mb;
            if (ConstantBool *p = dynamic_cast<ConstantBool *>(brInst->getCond())) {
                if ((int) p->get_const_val() == 0) {
                    mb = brInst->getElseTarget()->mb;
                } else {
                    mb = brInst->getThenTarget()->mb;
                }
                curMB->succMBs.push_back(mb);
                mb->predMBs.insert(curMB);
                if (mb->predMBs.size() > 8) CenterControl::_PREC_MB_TOO_MUCH = true;
                if (visitBBSet.find(mb) == visitBBSet.end()) {
                    visitBBSet.insert(mb);
                    nextBBList.push(mb->bb);
                }
                new GDJump(mb, curMB);
            } else {
                Value *condValue = brInst->getCond();
                McBlock *trueMcBlock = brInst->getThenTarget()->mb;
                McBlock *falseMcBlock = brInst->getElseTarget()->mb;
                falseMcBlock->predMBs.insert(curMB);
                if (falseMcBlock->predMBs.size() > 8) CenterControl::_PREC_MB_TOO_MUCH = true;
                trueMcBlock->predMBs.insert(curMB);
                if (trueMcBlock->predMBs.size() > 8) CenterControl::_PREC_MB_TOO_MUCH = true;
                if (CenterControl::USE_ESTIMATE_BASE_SORT && CenterControl::_O2) {
                    bool exchanged = false;
                    if (trueMcBlock->bb == *(it + 1)) {
                        exchanged = true;
                    } else if (falseMcBlock->bb == *(it + 1)) {
                        exchanged = false;
                    } /*else if (trueMcBlock->bb->getLoopDep() > falseMcBlock->bb->getLoopDep()) { exchanged = true; }*/
                    bool isIcmp = !condValue->isFcmp();
                    CMPAndArmCond *t = cmpInst2MICmpMap[condValue];
                    if (t != nullptr) {
                        cond = t->ArmCond;
                    } else {
                        Operand *condVR = getVR_may_imm(condValue);
                        cond = Arm::Cond::Ne;
                        new I::Cmp(cond, condVR, new Operand(DataType::I32, 0), curMB);
                    }
                    if (exchanged) {
                        McBlock *tmp = falseMcBlock;
                        falseMcBlock = trueMcBlock;
                        trueMcBlock = tmp;
                        cond = isIcmp ? getIcmpOppoCond(cond) : getFcmpOppoCond(cond);
                    }
                    curMB->succMBs.push_back(trueMcBlock);
                    curMB->succMBs.push_back(falseMcBlock);
                    new GDBranch(!isIcmp, cond, trueMcBlock, curMB);
                    new GDJump(falseMcBlock, curMB);
                    instr = (Instr *) instr->next;
                    continue;
                }
                if (visitBBSet.find(falseMcBlock) == visitBBSet.end()) {
                    nextBBList.push(falseMcBlock->bb);
                    visitBBSet.insert(falseMcBlock);
                }
                bool exchanged = false;
                if (visitBBSet.find(trueMcBlock) == visitBBSet.end()) {
                    visitBBSet.insert(trueMcBlock);
                    exchanged = true;
                    nextBBList.push(trueMcBlock->bb);
                }
                if (trueMcBlock->bb->getLoopDep() > falseMcBlock->bb->getLoopDep()) exchanged = true;
                bool isIcmp = !condValue->isFcmp();
                CMPAndArmCond *t = cmpInst2MICmpMap[condValue];
                if (t != nullptr) {
                    cond = t->ArmCond;
                } else {
                    Operand *condVR = getVR_may_imm(condValue);
                    cond = Arm::Cond::Ne;
                    new I::Cmp(cond, condVR, new Operand(DataType::I32, 0), curMB);
                }
                if (exchanged) {
                    McBlock *tmp = falseMcBlock;
                    falseMcBlock = trueMcBlock;
                    trueMcBlock = tmp;
                    cond = isIcmp ? getIcmpOppoCond(cond) : getFcmpOppoCond(cond);
                }
                curMB->succMBs.push_back(trueMcBlock);
                curMB->succMBs.push_back(falseMcBlock);
                new GDBranch(!isIcmp, cond, trueMcBlock, curMB);
                new GDJump(falseMcBlock, curMB);
            }
        }
            // else if (instr->tag ==  ValueTag::fneg) {
            //     INSTR::Fneg *fnegInst = (INSTR::Fneg *) instr;
            //     Operand *src = getVR_may_imm(fnegInst->getRVal1());
            //     Operand *dst = getVR_no_imm(fnegInst);
            //     new V::Neg(dst, src, curMB);
            // }
        else if (instr->tag == ValueTag::ret) {
            INSTR::Return *returnInst = (INSTR::Return *) instr;
            DataType retDataType = DataType::I1;
            if (returnInst->hasValue()) {
                if (returnInst->getRetValue()->type->is_int32_type()) {
                    retDataType = DataType::I32;
                    Operand *retOpd = getVR_may_imm(returnInst->getRetValue());
                    new I::Mov(Arm::Reg::getR(Arm::GPR::r0), retOpd, curMB);
                } else if (returnInst->getRetValue()->type->is_float_type()) {
                    retDataType = DataType::F32;
                    Operand *retOpd = getVR_may_imm(returnInst->getRetValue());
                    new V::Mov(Arm::Reg::getS(Arm::FPR::s0), retOpd, curMB);
                }
            }
            I::Binary *bino = new I::Binary(MachineInst::Tag::Add, Arm::Reg::getR(Arm::GPR::sp),
                                            Arm::Reg::getR(Arm::GPR::sp),
                                            new Operand(DataType::I32, 0), curMB);
            bino->setNeedFix(ENUM::VAR_STACK);
            if (retDataType == DataType::I32) {
                new I::Ret(curMF, Arm::Reg::getR(Arm::GPR::r0), curMB);
            } else if (retDataType == DataType::F32) {
                new V::Ret(curMF, Arm::Reg::getS(Arm::FPR::s0), curMB);
            } else {
                new I::Ret(curMF, curMB);
            }
        } else if (instr->tag == ValueTag::zext) {
            Operand *dst = getVR_no_imm(instr);
            Operand *src = getVR_may_imm(((INSTR::Zext *) instr)->getRVal1());
            new I::Mov(dst, src, curMB);
        } else if (instr->tag == ValueTag::fptosi) {
            Operand *src = getVR_may_imm(((INSTR::FPtosi *) instr)->getRVal1());
            Operand *tmp = newSVR();
            new V::Cvt(V::CvtType::f2i, tmp, src, curMB);
            Operand *dst = getVR_no_imm(instr);
            new V::Mov(dst, tmp, curMB);
        } else if (instr->tag == ValueTag::sitofp) {
            Operand *src = getVR_may_imm(((INSTR::SItofp *) instr)->getRVal1());
            Operand *tmp = newSVR();
            new V::Mov(tmp, src, curMB);
            Operand *dst = getVR_no_imm(instr);
            new V::Cvt(V::CvtType::i2f, dst, tmp, curMB);
        } else if (instr->tag == ValueTag::alloc) {
            // todo 常量下标访问频次多的可以放前面
            INSTR::Alloc *allocInst = (INSTR::Alloc *) instr;
            // std::cerr << "[genBB] allocInst: " << std::string(*allocInst) << std::endl;
            Type *contentType = allocInst->contentType;
            // std::cerr << "[genBB] contentType: " << std::string(*contentType) << std::endl;
            if (!contentType->is_pointer_type()) {
                if (contentType->is_basic_type()) {
                    allocMap[allocInst] = contentType->is_float_type() ? curMF->newSVR() : curMF->newVR();
                    instr = (Instr *) instr->next;
                    continue;
                }
                Operand *addr = getVR_no_imm(allocInst);
                Operand *spReg = Arm::Reg::getR(Arm::GPR::sp);
                if (!CenterControl::_GEP_CONST_BROADCAST && CenterControl::_O2) {
                    Operand *offset = new Operand(DataType::I32, curMF->getVarStack());
                    Operand *mvDst;
                    if (!AddSubImmEncode(curMF->getVarStack())) {
                        // todo 只有数组申请的太大才会涉及到此处优化, 如本来需要movw和movt, 但是除以4后正好只需要一条
                        mvDst = newVR();
                        new I::Mov(mvDst, offset, curMB);
                    } else {
                        mvDst = offset;
                    }
                    new I::Binary(MachineInst::Tag::Add, addr, spReg, mvDst, curMB);
                    curMF->addVarStack(((ArrayType *) contentType)->getFlattenSize() * 4);
                } else {
                    int curBaseOff = curMF->getVarStack();
                    // Operand *offset = new Operand(DataType::I32, curMF->getVarStack());
                    if (curBaseOff == 0) {
                        new I::Mov(addr, spReg, curMB);
                    } else {
                        Operand *mvDst;
                        if (!AddSubImmEncode(curMF->getVarStack())) {
                            mvDst = newVR();
                            if (CodeGen::immCanCode(curBaseOff / 4)) {
                                curBaseOff = curBaseOff / 4;
                                new I::Mov(mvDst, new Operand(DataType::I32, curBaseOff),
                                           new Arm::Shift(Arm::ShiftType::Lsl, 2), curMB);
                            } else {
                                new I::Mov(mvDst, new Operand(DataType::I32, curBaseOff), curMB);
                            }
                        } else {
                            mvDst = new Operand(DataType::I32, curBaseOff);
                        }
                        new I::Binary(MachineInst::Tag::Add, addr, spReg, mvDst, curMB);
                    }
                    int size = ((ArrayType *) contentType)->getFlattenSize() * 4;
                    curMF->reg2val[addr] = SpBasedOffAndSize{size, curBaseOff};
                    curMF->addVarStack(size);
                }
            } else {
                // Type *innerType = ((PointerType *) contentType)->inner_type;
                allocMap[allocInst] = curMF->newVR();
            }
        } else if (instr->tag == ValueTag::load) {
            INSTR::Load *loadInst = (INSTR::Load *) instr;
            Value *addrValue = loadInst->getPointer();
            if (INSTR::Alloc *allocInst = dynamic_cast<INSTR::Alloc *>(addrValue)) {
                Operand *o = allocMap[allocInst];
                assert(o != nullptr);
                curMF->value2Opd[instr] = o;
                instr = (Instr *) instr->next;
                continue;
            }
            Operand *addrOpd = getVR_from_ptr(addrValue);
            Operand *data = getVR_no_imm(instr);
            if (loadInst->type->is_float_type()) {
                new V::Ldr(data, addrOpd, curMB);
            } else {
                new I::Ldr(data, addrOpd, new Operand(DataType::I32, 0), curMB);
            }
        } else if (instr->tag == ValueTag::store) {
            INSTR::Store *storeInst = (INSTR::Store *) instr;
            if (INSTR::Alloc *allocInst = dynamic_cast<INSTR::Alloc *>(storeInst->getPointer())) {
                Value *dataValue = storeInst->getValue();
                Operand *data;
                if (INSTR::Alloc *dataAllocInst = dynamic_cast<INSTR::Alloc *>(storeInst->getValue())) {
                    data = allocMap[dataAllocInst];
                } else {
                    data = getVR_may_imm(storeInst->getValue());
                }
                Operand *o = allocMap[allocInst];
                Type *contentType = allocInst->contentType;
                if (data->dataType == DataType::F32) {
                    new V::Mov(o, data, curMB);
                } else {
                    new I::Mov(o, data, curMB);
                }
                instr = (Instr *) instr->next;
                continue;
            }
            Operand *data = getVR_may_imm(storeInst->getValue());
            Operand *addr = getVR_from_ptr(storeInst->getPointer());
            if (storeInst->getValue()->type->is_float_type()) {
                new V::Str(data, addr, curMB);
            } else {
                new I::Str(data, addr, curMB);
            }
        } else if (instr->tag == ValueTag::gep) {
            INSTR::GetElementPtr *gep = (INSTR::GetElementPtr *) instr;
            Value *ptrValue = gep->getPtr();
            int offsetCount = gep->getOffsetCount();
            Type *curBaseType = ((PointerType *) ptrValue->type)->inner_type;
            Operand *curAddrVR = getVR_from_ptr(ptrValue);
            if (CenterControl::_O2) {
                // %v30 = getelementptr inbounds [3 x [4 x [5 x i32]]], [3 x [4 x [5 x i32]]]* %f1, i32 %1
                // %v31 = getelementptr inbounds [3 x [4 x [5 x i32]]], [3 x [4 x [5 x i32]]]* %30, i32 0, i32 %2
                assert(offsetCount > 0 && offsetCount <= 2);
                if (offsetCount == 1) {
                    Value *curIdxValue = gep->useValueList[1];
                    int offSet =
                            4 * (curBaseType->is_basic_type() ? 1 : ((ArrayType *) curBaseType)->getFlattenSize());
                    if (curIdxValue->isConstantInt()) {
                        int curIdx = (int) ((ConstantInt *) curIdxValue)->get_const_val();
                        if (curIdx == 0) {
                            new I::Mov(getVR_no_imm(gep), curAddrVR, curMB);
                            // curMF->value2Opd.put(gep, curAddrVR);
                            // curMF->value2Opd.put(ptrValue, curAddrVR);
                        } else {
                            int totalOff = offSet * curIdx;
                            if (immCanCode(totalOff)) {
                                new I::Binary(MachineInst::Tag::Add, getVR_no_imm(gep), curAddrVR,
                                              new Operand(DataType::I32, totalOff), curMB);
                            } else {
                                singleTotalOff(gep, curAddrVR, totalOff);
                            }
                        }
                    } else {
                        if (curBaseType->is_basic_type()) {
                            new I::Binary(MachineInst::Tag::Add, getVR_no_imm(gep), curAddrVR,
                                          getVR_no_imm(curIdxValue),
                                          new Arm::Shift(Arm::ShiftType::Lsl, 2), curMB);
                        } else {
                            int baseSize = ((ArrayType *) curBaseType)->getFlattenSize();
                            int baseOffSet = 4 * baseSize;
                            if ((baseOffSet & (baseOffSet - 1)) == 0) {
                                new I::Binary(MachineInst::Tag::Add, getVR_no_imm(gep), curAddrVR,
                                              getVR_no_imm(curIdxValue),
                                              new Arm::Shift(Arm::ShiftType::Lsl,
                                                             numberOfTrailingZeros(baseOffSet)), curMB);
                            } else {
                                new I::Fma(true, false, getVR_no_imm(gep), getVR_no_imm(curIdxValue),
                                           getImmVR(baseOffSet), curAddrVR, curMB);
                            }
                        }
                    }
                } else {
                    Value *firstIdx = gep->useValueList[1];
                    Value *secondIdx = gep->useValueList[2];
                    Type *innerType = ((ArrayType *) curBaseType)->base_type;
                    if (innerType->is_basic_type()) {
                        if (secondIdx->isConstantInt()) {
                            int secondIdxNum = (int) ((ConstantInt *) secondIdx)->get_const_val();
                            int totalOff = 4 * secondIdxNum;
                            /*if (totalOff == 0) {
                                new I::Mov(getVR_no_imm(gep), curAddrVR, curMB);
                                // curMF->value2Opd.put(gep, curAddrVR);
                                // curMF->value2Opd.put(ptrValue, curAddrVR);
                            } else */
                            if (immCanCode(totalOff)) {
                                new I::Binary(MachineInst::Tag::Add, getVR_no_imm(gep), curAddrVR,
                                              new Operand(DataType::I32, totalOff), curMB);
                            } else {
                                int i = 0;
                                while ((secondIdxNum % 2) == 0) {
                                    i++;
                                    secondIdxNum = secondIdxNum / 2;
                                }
                                new I::Binary(MachineInst::Tag::Add, getVR_no_imm(gep), curAddrVR,
                                              getImmVR(secondIdxNum), new Arm::Shift(Arm::ShiftType::Lsl, i + 2),
                                              curMB);
                            }
                        } else {
                            new I::Binary(MachineInst::Tag::Add, getVR_no_imm(gep), curAddrVR, getVR_no_imm(secondIdx),
                                          new Arm::Shift(Arm::ShiftType::Lsl, 2), curMB);
                        }
                    } else {
                        // dst = curAddrVR + 4 * (int) baseSize * (may imm) secondIdx;
                        // baseOffSet = 4 * baseSize
                        int baseSize = ((ArrayType *) innerType)->getFlattenSize();
                        int baseOffSet = 4 * baseSize;
                        bool baseIs2Power = (baseSize & (baseSize - 1)) == 0;
                        if (secondIdx->isConstantInt()) {
                            // offset 是常数
                            int secondIdxNum = (int) ((ConstantInt *) secondIdx)->get_const_val();
                            int totalOffSet = 4 * baseSize * secondIdxNum;
                            if (immCanCode(totalOffSet)) {
                                new I::Binary(MachineInst::Tag::Add, getVR_no_imm(gep), curAddrVR,
                                              new Operand(DataType::I32, totalOffSet), curMB);
                            } else {
                                bool secondIdxIs2Power = (secondIdxNum & (secondIdxNum - 1)) == 0;
                                if (baseIs2Power) {
                                    genGepAdd(gep, curAddrVR, baseOffSet, secondIdxNum);
                                } else if (secondIdxIs2Power) {
                                    genGepAdd(gep, curAddrVR, secondIdxNum, baseOffSet);
                                } else {
                                    singleTotalOff(gep, curAddrVR, totalOffSet);
                                }
                            }
                        } else {
                            if ((baseOffSet & (baseOffSet - 1)) == 0) {
                                new I::Binary(MachineInst::Tag::Add, getVR_no_imm(gep), curAddrVR,
                                              getVR_no_imm(secondIdx), new Arm::Shift(Arm::ShiftType::Lsl,
                                                                                      numberOfTrailingZeros(
                                                                                              baseOffSet)), curMB);
                            } else {
                                new I::Fma(true, false, getVR_no_imm(gep), getVR_no_imm(secondIdx),
                                           getImmVR(baseOffSet), curAddrVR, curMB);
                            }
                        }
                    }
                }
            } else {
                Operand *dstVR = getVR_no_imm(gep);
                // Machine.Operand *curAddrVR = getVR_no_imm(ptrValue);
                // Operand *curAddrVR = getVR_from_ptr(ptrValue);
                // Operand *curAddrVR = newVR();
                // new I::Mov(curAddrVR, basePtrVR, curMB);
                int totalConstOff = 0;
                for (int i = 0; i < offsetCount; i++) {
                    Value *curIdxValue = gep->getIdxValueOf(i);
                    int offUnit = 4;
                    if (curBaseType->is_array_type()) {
                        offUnit = 4 * ((ArrayType *) curBaseType)->getFlattenSize();
                        curBaseType = ((ArrayType *) curBaseType)->base_type;
                    }
                    if (auto *p = dynamic_cast<ConstantInt *>(curIdxValue)) {
                        // 每一个常数下标都会累加到 totalConstOff 中
                        totalConstOff += offUnit * (int) ((ConstantInt *) curIdxValue)->get_const_val();
                        if (i == offsetCount - 1) {
                            // 这里的设计比较微妙
                            if (totalConstOff == 0) {
                                // TODO
                                curMF->value2Opd[gep] = curAddrVR;
                                // new I::Mov(dstVR, curAddrVR, curMB);
                            } else {
                                Operand *imm;
                                if (immCanCode(totalConstOff)) {
                                    imm = new Operand(DataType::I32, totalConstOff);
                                } else {
                                    imm = getImmVR(totalConstOff);
                                }
                                new I::Binary(MachineInst::Tag::Add, dstVR, curAddrVR, imm, curMB);
                            }
                        }
                    } else {
                        // TODO 是否需要避免寄存器分配时出现use的VR与def的VR相同的情况
                        Operand *curIdxVR = getVR_no_imm(curIdxValue);
                        Operand *offUnitImmVR = getImmVR(offUnit);
                        /*
                         * Fma Rd, Rm, Rs, Rn
                         * smmla:Rn + (Rm * Rs)[63:32] or smmls:Rd := Rn – (Rm * Rs)[63:32]
                         * mla:Rn + (Rm * Rs)[31:0] or mls:Rd := Rn – (Rm * Rs)[31:0]
                         * dst = acc +(-) lhs * rhs
                         */
                        // Machine.Operand *tmpDst = newVR();
                        // new MIFma(true,false, tmpDst, curAddrVR, curIdxVR ,offUnitImmVR, curMB);
                        // curAddrVR = tmpDst;
                        if (i == offsetCount - 1) {
                            if (totalConstOff != 0) {
                                Operand *immVR = getImmVR(totalConstOff);
                                Operand *dst = newVR();
                                new I::Binary(MachineInst::Tag::Add, dst, curAddrVR, immVR, curMB);
                                curAddrVR = dst;
                            }
                            new I::Fma(true, false, dstVR, curIdxVR, offUnitImmVR, curAddrVR, curMB);
                        } else {
                            Operand *dst = newVR();
                            new I::Fma(true, false, dst, curIdxVR, offUnitImmVR, curAddrVR, curMB);
                            curAddrVR = dst;
                        }
                    }
                }
            }
        } else if (instr->tag == ValueTag::bitcast) {
            INSTR::BitCast *bitcast = (INSTR::BitCast *) instr;
            Operand *src = getVR_no_imm(bitcast->getSrcValue());
            // float to non-float, non-float to float
            bool src_is_float = bitcast->getSrcValue()->type->is_float_type();
            bool dst_is_float = bitcast->type->is_float_type();
            if (src_is_float != dst_is_float) {
                Operand *dst = getVR_no_imm(bitcast);
                new V::Mov(dst, src, curMB);
                curMF->value2Opd[bitcast] = dst;
            } else {
                curMF->value2Opd[bitcast] = src;
            }
        } else if (instr->tag == ValueTag::call) {
            INSTR::Call *call_inst = dynamic_cast<INSTR::Call *>(instr);
            if (call_inst->getFunc() == ExternFunction::USAT) {
                std::vector<Value *> *param_list = call_inst->getParamList();
                Value *bitnum = (*param_list)[1];
                Value *src = (*param_list)[0];
                assert(bitnum->isConstantInt());
                int bit_count = dynamic_cast<ConstantInt *>(bitnum)->get_const_val();
                if (src->isConstantInt()) {
                    int src_num = dynamic_cast<ConstantInt *>(src)->get_const_val();
                    if (src_num < 0) src_num = 0;
                    if (src_num > ((1 << bit_count) - 1)) src_num = (1 << bit_count) - 1;
                    new I::Mov(getVR_no_imm(instr), new Operand(DataType::I32, src_num), curMB);
                } else {
                    new I::Binary(MachineInst::Tag::USAT, getVR_no_imm(instr), new Operand(DataType::I32, bit_count),
                                  getVR_may_imm(src),
                                  curMB);
                }
            } else
                dealCall((INSTR::Call *) instr);
        } else if (instr->tag == ValueTag::phi) {
            INSTR::Phi *phi = (INSTR::Phi *) instr;
            Operand *vr = getVR_no_imm(phi);
            Operand *tmp;
            if (phi->type->is_float_type()) {
                tmp = curMF->newSVR();
            } else {
                tmp = curMF->newVR();
            }
            curMF->phi2tmpMap[phi] = tmp;
        } else if (instr->tag == ValueTag::pcopy) {
        } else if (instr->tag == ValueTag::move) {
            Value *src = ((INSTR::Move *) instr)->getSrc();
            Value *dst = ((INSTR::Move *) instr)->getDst();
            if (src->isConstantFloat()) {
                ConstantFloat *constF = (ConstantFloat *) src;
                float imm = (float) constF->get_const_val();
                if (floatCanCode(imm)) {
                    Operand *source = new Operand(DataType::F32, imm);
                    Operand *target = getVR_no_imm(dst);
                    new V::Mov(target, source, curMB);
                } else {
                    std::string name = constF->asm_name;
                    Operand *addr = name2constFOpd[name];
                    if (addr == nullptr) {
                        addr = new Operand(DataType::F32, constF);
                        name2constFOpd[name] = addr;
                    }
                    Arm::Glob *glob = new Arm::Glob(name);
                    Operand *labelAddr = newVR();
                    new I::Mov(labelAddr, glob, curMB);
                    Operand *target = getVR_no_imm(dst);
                    new V::Ldr(target, labelAddr, curMB);
                }
            } else {
                Operand *source = getOp_may_imm(src);
                Operand *target = getVR_no_imm(dst);
                if (source->isF32() || target->isF32()) {
                    new V::Mov(target, source, curMB);
                } else {
                    new I::Mov(target, source, curMB);
                }
            }
        }
        instr = (Instr *) instr->next;
    }

}

void CodeGen::singleTotalOff(INSTR::GetElementPtr *gep, Operand *curAddrVR, int totalOff) {
    int i = 0;
    while ((totalOff % 2) == 0) {
        i++;
        totalOff = totalOff / 2;
    }
    new I::Binary(MachineInst::Tag::Add, getVR_no_imm(gep), curAddrVR, getImmVR(totalOff),
                  new Arm::Shift(Arm::ShiftType::Lsl, i), curMB);
}

void CodeGen::genGepAdd(INSTR::GetElementPtr *gep, Operand *curAddrVR, int offBase1, int offBase2) {
    int i = 0;
    while ((offBase2 % 2) == 0) {
        i++;
        offBase2 = offBase2 / 2;
    }
    new I::Binary(MachineInst::Tag::Add, getVR_no_imm(gep), curAddrVR, getImmVR(offBase2),
                  new Arm::Shift(Arm::ShiftType::Lsl, numberOfTrailingZeros(offBase1) + i), curMB);
}

void CodeGen::dealCall(INSTR::Call *call_inst) {
    std::vector<Value *> *param_list = call_inst->getParamList();
    if (call_inst->getFunc() == ExternFunction::MEM_SET) {
        assert(param_list->size() == 3);
        param_list->erase(param_list->begin() + 1);
    }
    // std::vector<Operand *> paramSVRList;
    int rIdx = 0;
    int sIdx = 0;
    int rParamTop = rIdx;
    int sParamTop = sIdx;
    for (Value *p: *param_list) {
        if (p->type->is_float_type()) {
            if (sIdx < sParamCnt) {
                Operand *fpr = Arm::Reg::getS(sIdx);
                new V::Mov(fpr, getOp_may_imm(p), curMB);
                sParamTop = sIdx + 1;
            } else {
                int offset_imm = (sParamTop + rParamTop - (rIdx + sIdx) - 1) * 4;
                Operand *data = getVR_may_imm(p);
                Operand *off = new Operand(DataType::I32, offset_imm);
                if (vLdrStrImmEncode(offset_imm)) {
                    new V::Str(data, Arm::Reg::getR(Arm::GPR::sp), off, curMB);
                } else if (immCanCode(offset_imm)) {
                    Operand *dstAddr = newVR();
                    new I::Binary(MachineInst::Tag::Sub, dstAddr, Arm::Reg::getR(Arm::GPR::sp),
                                  new Operand(DataType::I32, offset_imm), curMB);
                    new V::Str(data, dstAddr, curMB);
                } else if (immCanCode(-offset_imm)) {
                    Operand *dstAddr = newVR();
                    new I::Binary(MachineInst::Tag::Sub, dstAddr, Arm::Reg::getR(Arm::GPR::sp),
                                  new Operand(DataType::I32, -offset_imm),
                                  curMB);
                    new V::Str(data, dstAddr, curMB);
                } else {
                    Operand *imm = newVR();
                    new I::Mov(imm, new Operand(DataType::I32, -offset_imm), curMB);
                    Operand *dstAddr = newVR();
                    new I::Binary(MachineInst::Tag::Sub, dstAddr, Arm::Reg::getR(Arm::GPR::sp), imm, curMB);
                    new V::Str(data, dstAddr, curMB);
                }
            }
            sIdx++;
        } else {
            if (rIdx < rParamCnt) {
                Operand *gpr = Arm::Reg::getR(rIdx);
                new I::Mov(gpr, getOp_may_imm(p), curMB);
                rParamTop = rIdx + 1;
            } else {
                int offset_imm = (sParamTop + rParamTop - (rIdx + sIdx) - 1) * 4;
                Operand *data = getVR_may_imm(p);
                Operand *addr = Arm::Reg::getR(Arm::GPR::sp);
                Operand *off = new Operand(DataType::I32, offset_imm);
                if (!LdrStrImmEncode(offset_imm)) {
                    Operand *immDst = newVR();
                    new I::Mov(immDst, off, curMB);
                    off = immDst;
                }
                new I::Str(data, addr, off, curMB);
            }
            rIdx++;
        }
    }
    if (needFPU) {
    }
    Function *callFunc = call_inst->getFunc();
    McFunction *callMcFunc = f2mf[callFunc];
    if (callFunc->isExternal) {
        McFunction *mf = f2mf[callFunc];
        new MICall(mf, curMB);
    } else {
        if (callMcFunc == nullptr) {
        }
        I::Binary *bino = new I::Binary(MachineInst::Tag::Sub, Arm::Reg::getR(Arm::GPR::sp),
                                        Arm::Reg::getR(Arm::GPR::sp), new Operand(DataType::I32, 0),
                                        curMB);
        bino->setNeedFix(callMcFunc, ENUM::STACK_FIX::ONLY_PARAM);
        new MICall(callMcFunc, curMB);
        I::Binary *bino2 = new I::Binary(MachineInst::Tag::Add, Arm::Reg::getR(Arm::GPR::sp),
                                         Arm::Reg::getR(Arm::GPR::sp), new Operand(DataType::I32, 0),
                                         curMB);
        bino2->setNeedFix(callMcFunc, ENUM::STACK_FIX::ONLY_PARAM);
    }
    if (call_inst->type->is_int32_type()) {
        new I::Mov(getVR_no_imm(call_inst), Arm::Reg::getR(Arm::GPR::r0), curMB);
    } else if (call_inst->type->is_float_type()) {
        new V::Mov(getVR_no_imm(call_inst), Arm::Reg::getS(Arm::FPR::s0), curMB);
    } else if (!call_inst->type->is_void_type()) {
        exit(113);
    }
}

bool CodeGen::LdrStrImmEncode(int off) {
    return off < 4096 && off > -4096;
}

bool CodeGen::vLdrStrImmEncode(int off) {
    return off <= 1020 && off >= -1020;
}

bool CodeGen::AddSubImmEncode(int off) {
    return immCanCode(off);
}

void CodeGen::genGlobal() {
    for (const auto &entry: *globalMap) {
        GlobalValue *globalValue = entry.first;
        Arm::Glob *glob = new Arm::Glob(globalValue);
        McProgram::MC_PROGRAM->globList.push_back(glob);
        globPtr2globNameOpd[globalValue] = glob;
    }
}

bool CodeGen::isFBino(INSTR::Alu::Op op) {
    return op == INSTR::Alu::Op::FSUB || op == INSTR::Alu::Op::FADD || op == INSTR::Alu::Op::FDIV ||
           op == INSTR::Alu::Op::FMUL || op == INSTR::Alu::Op::FREM;
}

int numberOfLeadingZeros(int i) {
    // HD, Count leading 0's
    if (i <= 0)
        return i == 0 ? 32 : 0;
    int n = 31;
    if (i >= 1 << 16) {
        n -= 16;
        i >>= 16;
    }
    if (i >= 1 << 8) {
        n -= 8;
        i >>= 8;
    }
    if (i >= 1 << 4) {
        n -= 4;
        i >>= 4;
    }
    if (i >= 1 << 2) {
        n -= 2;
        i >>= 2;
    }
    return n - (i >> 1);
}

int bitCount(int i1) {
    unsigned int i = i1;
    // HD, Figure 5-2
    i = i - ((i >> 1) & 0x55555555);
    i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
    i = (i + (i >> 4)) & 0x0f0f0f0f;
    i = i + (i >> 8);
    i = i + (i >> 16);
    return i & 0x3f;
}
//
// inline int countl_zero(unsigned const x) { return __builtin_clz(x); }
//
// inline int bit_width(unsigned const x) {
//     return std::numeric_limits<decltype(x)>::digits - countl_zero(x);
// }

void CodeGen::genBinaryInst(INSTR::Alu *instr) {
    int bitsOfInt = 32;
    MachineInst::Tag tag = tagMap[(int) instr->op];
    Value *lhs = instr->getRVal1();
    Value *rhs = instr->getRVal2();
    Operand *dVR = getVR_no_imm(instr);
    if (tag == MachineInst::Tag::Mod) {
        bool isPowerOf2 = false;
        int imm = 1, abs = 1;
        if (rhs->isConstantInt()) {
            imm = (int) ((ConstantInt *) rhs)->get_const_val();
            abs = (imm < 0) ? (-imm) : imm;
            if ((abs & (abs - 1)) == 0) {
                isPowerOf2 = true;
            }
        }
        if (optMulDiv && isPowerOf2) {
            if (lhs->isConstantInt()) {
                int vlhs = (int) ((ConstantInt *) lhs)->get_const_val();
                new I::Mov(dVR, new Operand(DataType::I32, vlhs % imm), curMB);
            } else if (abs == 1 || abs == -1) {
                new I::Mov(dVR, new Operand(DataType::I32, 0), curMB);
                // } else if (imm == 2) {
                //     Operand *tmp1 = newVR();
                //     Operand *lVR = getVR_may_imm(lhs);
                //     I::Binary *addlsr = new I::Binary(MachineInst::Tag::Add, tmp1, lVR, lVR, curMB);
                //     addlsr->shift = new Arm::Shift(Arm::ShiftType::Lsr, 31);
                //     Operand *tmp2 = newVR();
                //     new I::Binary(MachineInst::Tag::Bic, tmp2, tmp1, new Operand(DataType::I32, 1), curMB);
                //     new I::Binary(MachineInst::Tag::Sub, dVR, lVR, tmp2, curMB);
                // } else if ((imm & (imm - 1)) == 0) {
                //     int log_imm = bit_width(imm) - 1;
                //     assert(log_imm > 0);
                //     Operand *tmp1 = newVR();
                //     Operand *lVR = getVR_may_imm(lhs);
                //     I::Mov *mvasr = new I::Mov(tmp1, lVR, curMB);
                //     mvasr->shift = new Arm::Shift(Arm::ShiftType::Asr, 31);
                //     Operand *tmp2 = newVR();
                //     I::Binary *addlsr = new I::Binary(MachineInst::Tag::Add, tmp2, lVR, tmp1, curMB);
                //     addlsr->shift = new Arm::Shift(Arm::ShiftType::Lsr, 32 - log_imm);
                //     Operand *tmp3;
                //     if (log_imm <= 8) {
                //         tmp3 = newVR();
                //         new I::Binary(MachineInst::Tag::Bic, tmp3, tmp2, new Operand(DataType::I32, imm - 1), curMB);
                //     } else if (log_imm >= 24) {
                //         tmp3 = newVR();
                //         new I::Binary(MachineInst::Tag::And, tmp3, tmp2, new Operand(DataType::I32, ~(imm - 1)), curMB);
                //     } else {
                //         new I::Binary(MachineInst::Tag::Bfc, tmp2, new Operand(DataType::I32, 0),
                //                       new Operand(DataType::I32, log_imm), curMB);
                //         tmp3 = tmp2;
                //     }
                //     if (imm > 0)
                //         new I::Binary(MachineInst::Tag::Sub, dVR, lVR, tmp3, curMB);
                //     else
                //         new I::Binary(MachineInst::Tag::Add, dVR, lVR, tmp3, curMB);
            } else {
                int lsh = numberOfTrailingZeros(abs), rsh = bitsOfInt - lsh;
                Operand *lVR = getVR_may_imm(lhs);
                Operand *sgn = newVR();
                new I::Mov(sgn, lVR, new Arm::Shift(Arm::ShiftType::Asr, bitsOfInt - 1), curMB);
                Operand *tmp = newVR();
                new I::Binary(MachineInst::Tag::Add, tmp, lVR, sgn, new Arm::Shift(Arm::ShiftType::Lsr, rsh), curMB);
                Operand *quo = newVR();
                new I::Mov(quo, tmp, new Arm::Shift(Arm::ShiftType::Asr, lsh), curMB);
                new I::Binary(MachineInst::Tag::Sub, dVR, lVR, quo, new Arm::Shift(Arm::ShiftType::Lsl, lsh), curMB);
            }
        } else {
            Operand *lVR = getVR_may_imm(lhs);
            Operand *rVR = getVR_may_imm(rhs);
            Operand *dst1 = newVR();
            new I::Binary(MachineInst::Tag::Div, dst1, lVR, rVR, curMB);
            Operand *dst2 = newVR();
            new I::Binary(MachineInst::Tag::Mul, dst2, dst1, rVR, curMB);
            new I::Binary(MachineInst::Tag::Sub, dVR, lVR, dst2, curMB);
        }
    } else if (optMulDiv && tag == MachineInst::Tag::Mul && (lhs->isConstantInt() || rhs->isConstantInt())) {
        if (lhs->isConstantInt() && rhs->isConstantInt()) {
            int vlhs = ((ConstantInt *) lhs)->get_const_val();
            int vrhs = ((ConstantInt *) rhs)->get_const_val();
            new I::Mov(dVR, new Operand(DataType::I32, vlhs * vrhs), curMB);
        } else {
            Operand *srcOp;
            int imm;
            if (lhs->isConstantInt()) {
                srcOp = getVR_may_imm(rhs);
                imm = (int) ((ConstantInt *) lhs)->get_const_val();
            } else {
                srcOp = getVR_may_imm(lhs);
                imm = (int) ((ConstantInt *) rhs)->get_const_val();
            }
            int abs = (imm < 0) ? (-imm) : imm;
            if (abs == 0) {
                new I::Mov(dVR, new Operand(DataType::I32, 0), curMB);
            } else if ((abs & (abs - 1)) == 0) {
                int sh = bitsOfInt - 1 - numberOfLeadingZeros(abs);
                if (sh > 0) {
                    new I::Mov(dVR, srcOp, new Arm::Shift(Arm::ShiftType::Lsl, sh), curMB);
                } else {
                    new I::Mov(dVR, srcOp, curMB);
                }
                if (imm < 0) {
                    new I::Binary(MachineInst::Tag::Rsb, dVR, dVR, new Operand(DataType::I32, 0), curMB);
                }
            } else if (bitCount(abs) == 2) {
                int hi = bitsOfInt - 1 - numberOfLeadingZeros(abs);
                int lo = numberOfTrailingZeros(abs);
                Operand *shiftHi = newVR();
                new I::Mov(shiftHi, srcOp, new Arm::Shift(Arm::ShiftType::Lsl, hi), curMB);
                new I::Binary(MachineInst::Tag::Add, dVR, shiftHi, srcOp, new Arm::Shift(Arm::ShiftType::Lsl, lo),
                              curMB);
                if (imm < 0) {
                    new I::Binary(MachineInst::Tag::Rsb, dVR, dVR, new Operand(DataType::I32, 0), curMB);
                }
            } else if (((abs + 1) & (abs)) == 0) {
                int sh = bitsOfInt - 1 - numberOfLeadingZeros(abs + 1);
                new I::Binary(MachineInst::Tag::Rsb, dVR, srcOp, srcOp, new Arm::Shift(Arm::ShiftType::Lsl, sh), curMB);
                if (imm < 0) {
                    new I::Binary(MachineInst::Tag::Rsb, dVR, dVR, new Operand(DataType::I32, 0), curMB);
                }
            } else {
                new I::Binary(tag, dVR, srcOp, getImmVR(imm), curMB);
            }
        }
    } else if (optMulDiv && tag == MachineInst::Tag::Div && rhs->isConstantInt()) {
        if (lhs->isConstantInt()) {
            int vlhs = ((ConstantInt *) lhs)->get_const_val();
            int vrhs = ((ConstantInt *) rhs)->get_const_val();
            new I::Mov(dVR, new Operand(DataType::I32, vlhs / vrhs), curMB);
        }
        Operand *lVR = getVR_may_imm(lhs);
        int imm = (int) ((ConstantInt *) rhs)->get_const_val();
        int abs = (imm < 0) ? (-imm) : imm;
        if (abs == 0) {
            new I::Mov(dVR, lVR, curMB);
        } else if (imm == 1) {
            new I::Mov(dVR, lVR, curMB);
        } else if (imm == -1) {
            new I::Binary(MachineInst::Tag::Rsb, dVR, lVR, new Operand(DataType::I32, 0), curMB);
        } else if ((abs & (abs - 1)) == 0) {
            int sh = bitsOfInt - 1 - numberOfLeadingZeros(abs);
            Operand *sgn = newVR();
            new I::Mov(sgn, lVR, new Arm::Shift(Arm::ShiftType::Asr, bitsOfInt - 1), curMB);
            Operand *tmp = newVR();
            new I::Binary(MachineInst::Tag::Add, tmp, lVR, sgn, new Arm::Shift(Arm::ShiftType::Lsr, bitsOfInt - sh),
                          curMB);
            new I::Mov(dVR, tmp, new Arm::Shift(Arm::ShiftType::Asr, sh), curMB);
            if (imm < 0) {
                new I::Binary(MachineInst::Tag::Rsb, dVR, dVR, new Operand(DataType::I32, 0), curMB);
            }
        } else {
            int magic, more;
            std::bitset<32> bits((unsigned int) abs);
            int log2d = bitsOfInt - 1 - numberOfLeadingZeros(abs);
            int negativeDivisor = 128;
            int addMarker = 64;
            int s32ShiftMask = 31;
            long uint32Mask = 0xFFFFFFFFL;
            int uint8Mask = 0xFF;
            if ((abs & (abs - 1)) == 0) {
                magic = 0;
                more = (imm < 0 ? (log2d | negativeDivisor) : log2d) & uint8Mask;
            } else {
                unsigned int rem, proposed;
                unsigned long long n = ((unsigned long long) (1u << (log2d - 1)) << 32);
                proposed = (unsigned int) (n / abs);
                rem = (unsigned int) (n - proposed * (unsigned long long) abs);
                proposed += proposed;
                int twiceRem = rem + rem;
                if ((twiceRem & uint32Mask) >= (abs & uint32Mask) || (twiceRem & uint32Mask) < (rem & uint32Mask)) {
                    proposed += 1;
                }
                more = (log2d | addMarker) & uint8Mask;
                proposed += 1;
                magic = proposed;
                if (imm < 0) {
                    more |= negativeDivisor;
                }
            }
            int sh = more & s32ShiftMask;
            int mask = (1 << sh), sign = ((more & (0x80)) != 0) ? -1 : 0, isPower2 = (magic == 0) ? 1 : 0;
            Operand *q = newVR();
            new I::Binary(MachineInst::Tag::LongMul, q, lVR, getImmVR(magic), curMB);
            new I::Binary(MachineInst::Tag::Add, q, q, lVR, curMB);
            Operand *q1 = newVR();
            new I::Binary(MachineInst::Tag::And, q1, getImmVR(mask - isPower2), q,
                          new Arm::Shift(Arm::ShiftType::Asr, 31), curMB);
            new I::Binary(MachineInst::Tag::Add, q, q, q1, curMB);
            new I::Mov(q, q, new Arm::Shift(Arm::ShiftType::Asr, sh), curMB);
            if (sign < 0) {
                new I::Binary(MachineInst::Tag::Rsb, q, q, new Operand(DataType::I32, 0), curMB);
            }
            new I::Mov(dVR, q, curMB);
        }
    } else if (isFBino(instr->op)) {
        if (tag == MachineInst::Tag::FMod) {
            exit(199);
        }
        Operand *lVR = getVR_may_imm(lhs);
        Operand *rVR = getVR_may_imm(rhs);
        new V::Binary(tag, dVR, lVR, rVR, curMB);
    } else {
        if (lhs->isConstantInt()) {
            switch (tag) {
                case MachineInst::Tag::Add: {
                    Value *tmp = lhs;
                    lhs = rhs;
                    rhs = tmp;
                    break;
                }
                case MachineInst::Tag::Sub: {
                    tag = MachineInst::Tag::Rsb;
                    Value *tmp = lhs;
                    lhs = rhs;
                    rhs = tmp;
                    break;
                }
            }
        }
        Operand *lVR = getVR_may_imm(lhs);
        Operand *rVR = (tag == MachineInst::Tag::Div || tag == MachineInst::Tag::Mul) ? getVR_may_imm(rhs)
                                                                                      : getOp_may_imm(rhs);
        new I::Binary(tag, dVR, lVR, rVR, curMB);
    }
}

void CodeGen::genCmp(Instr *instr) {
    Operand *dst = getVR_may_imm(instr);
    if (instr->isIcmp()) {
        INSTR::Icmp *icmp = ((INSTR::Icmp *) instr);
        Value *lhs = icmp->getRVal1();
        Value *rhs = icmp->getRVal2();
        Operand *lVR = getVR_may_imm(lhs);
        Operand *rVR = getOp_may_imm(rhs);
        int condIdx = (int) icmp->op;
        Arm::Cond cond = Arm::Cond(condIdx);
        I::Cmp *cmp = new I::Cmp(cond, lVR, rVR, curMB);
        if (((Instr *) icmp->next)->isBranch()
            && icmp->onlyOneUser()
            && icmp->getBeginUse()->user->isBranch() && icmp->next == icmp->getBeginUse()->user) {
            cmpInst2MICmpMap[instr] = new CMPAndArmCond(cmp, cond);
        } else {
            new I::Mov(cond, dst, new Operand(DataType::I32, 1), curMB);
            new I::Mov(getIcmpOppoCond(cond), dst, new Operand(DataType::I32, 0), curMB);
        }
    } else if (instr->isFcmp()) {
        INSTR::Fcmp *fcmp = ((INSTR::Fcmp *) instr);
        Value *lhs = fcmp->getRVal1();
        Value *rhs = fcmp->getRVal2();
        Operand *lSVR = getVR_may_imm(lhs);
        Operand *rSVR;
        if (rhs->isConstantFloat() && ((ConstantFloat *) rhs)->get_const_val() == 0.0f) {
            ConstantFloat *constF = (ConstantFloat *) rhs;
            rSVR = new Operand(DataType::F32, constF->get_const_val());
        } else {
            rSVR = getVR_may_imm(rhs);
        }
        Arm::Cond cond = fcmp->op == INSTR::Fcmp::Op::OEQ ? Arm::Cond::Eq :
                         fcmp->op == INSTR::Fcmp::Op::ONE ? Arm::Cond::Ne :
                         fcmp->op == INSTR::Fcmp::Op::OGT ? Arm::Cond::Hi :
                         fcmp->op == INSTR::Fcmp::Op::OGE ? Arm::Cond::Pl :
                         fcmp->op == INSTR::Fcmp::Op::OLT ? Arm::Cond::Lt :
                         fcmp->op == INSTR::Fcmp::Op::OLE ? Arm::Cond::Le :
                         Arm::Cond::Any;
        V::Cmp *vcmp = new V::Cmp(cond, lSVR, rSVR, curMB);
        if (((Instr *) fcmp->next)->isBranch()
            && fcmp->onlyOneUser()
            && fcmp->getBeginUse()->user->isBranch() && fcmp->next == fcmp->getBeginUse()->user) {
            cmpInst2MICmpMap[instr] = new CMPAndArmCond(vcmp, cond);
        } else {
            new I::Mov(cond, dst, new Operand(DataType::I32, 1), curMB);
            new I::Mov(getFcmpOppoCond(cond), dst, new Operand(DataType::I32, 0), curMB);
        }
    }
}

Arm::Cond CodeGen::getIcmpOppoCond(Arm::Cond cond) {
    return cond == Arm::Cond::Eq ? Arm::Cond::Ne :
           cond == Arm::Cond::Ne ? Arm::Cond::Eq :
           cond == Arm::Cond::Ge ? Arm::Cond::Lt :
           cond == Arm::Cond::Gt ? Arm::Cond::Le :
           cond == Arm::Cond::Le ? Arm::Cond::Gt :
           cond == Arm::Cond::Lt ? Arm::Cond::Ge :
           Arm::Cond::Any;
}

Arm::Cond CodeGen::getFcmpOppoCond(Arm::Cond cond) {
    return cond == Arm::Cond::Eq ? Arm::Cond::Ne :
           cond == Arm::Cond::Ne ? Arm::Cond::Eq :
           cond == Arm::Cond::Hi ? Arm::Cond::Le :
           cond == Arm::Cond::Pl ? Arm::Cond::Lt :
           cond == Arm::Cond::Le ? Arm::Cond::Hi :
           cond == Arm::Cond::Lt ? Arm::Cond::Pl :
           Arm::Cond::Any;
}

bool CodeGen::immCanCode(unsigned int imm) {
    int i = 0;
    do {
        int n = (imm << (2 * i)) | (imm >> (32 - 2 * i));
        if ((n & ~0xFF) == 0) return true;
    } while (i++ < 16);
    return false;
}

bool CodeGen::floatCanCode(float imm) {
    IFU u;
    u.f = imm;
    int bits = u.i;
    int bitArray[32] = {0};
    for (int i = 0; i < 32; i++) {
        int base = (1 << i);
        bitArray[i] = (bits & base) != 0;
    }
    for (int i = 0; i < 19; i++) {
        if (bitArray[i]) {
            return false;
        }
    }
    for (int i = 25; i <= 29; i++) {
        if (bitArray[i] == bitArray[30]) {
            return false;
        }
    }
    return true;
}

Operand *CodeGen::newSVR() {
    return curMF->newSVR();
}

Operand *CodeGen::newVR() {
    return curMF->newVR();
}

Operand *CodeGen::newSVR(Value *value) {
    Operand *opd = curMF->newSVR();
    curMF->value2Opd[value] = opd;
    return opd;
}

Operand *CodeGen::newVR(Value *value) {
    Operand *opd = curMF->newVR();
    curMF->value2Opd[value] = opd;
    return opd;
}

Operand *CodeGen::getVR_no_imm(Value *value) {
    Operand *opd = curMF->value2Opd[value];
    return opd == nullptr ? (value->type->is_float_type() ? newSVR(value) : newVR(value)) : opd;
}

Operand *CodeGen::getImmVR(int imm) {
    if (CenterControl::_GEP_CONST_BROADCAST && CenterControl::_O2) {
        if (curMF->intVal2vr.count(imm) == 0) {
            Operand *dst = newVR();
            curMF->reg2val[dst] = imm;
            curMF->intVal2vr[imm] = dst;
            return dst;
        } else {
            return curMF->intVal2vr[imm];
        }
    }
    Operand *dst = newVR();
    Operand *immOpd = new Operand(DataType::I32, imm);
    new I::Mov(dst, immOpd, curMB);
    return dst;
}

Operand *CodeGen::getFConstVR(ConstantFloat *constF) {
    float imm = (float) constF->get_const_val();
    Operand *dst = newSVR();
    if (floatCanCode(imm)) {
        Operand *fopd = new Operand(DataType::F32, imm);
        new V::Mov(dst, fopd, curMB);
    } else {
        std::string name = constF->asm_name;
        Operand *addr = name2constFOpd[name];
        if (addr == nullptr) {
            addr = new Operand(DataType::F32, constF);
            name2constFOpd[name] = addr;
        }
        Arm::Glob *glob = new Arm::Glob(name);
        Operand *labelAddr = newVR();
        new I::Mov(labelAddr, glob, curMB);
        new V::Ldr(dst, labelAddr, curMB);
    }
    return dst;
}

Operand *CodeGen::getOp_may_imm(Value *value) {
    if (value->type->is_int32_type() && value->isConstant() || value->type->is_int1_type() && value->isConstant()) {
        int imm = (int) ((ConstantInt *) value)->get_const_val();
        if (immCanCode(imm))
            return new Operand(DataType::I32, imm);
    } else if (value->isConstantFloat()) {
        float imm = (float) ((ConstantFloat *) value)->get_const_val();
        if (floatCanCode(imm)) {
            return new Operand(DataType::F32, imm);
        }
    }
    return getVR_may_imm(value);
}

Operand *CodeGen::getVR_may_imm(Value *value) {
    if (value->type->is_int32_type() && value->isConstant() || value->type->is_int1_type() && value->isConstant()) {
        return getImmVR((int) ((ConstantInt *) value)->get_const_val());
    } else if (value->type->is_float_type() && value->isConstant()) {
        return getFConstVR((ConstantFloat *) value);
    } else {
        return getVR_no_imm(value);
    }
}

Operand *CodeGen::getVR_from_ptr(Value *value) {
    if (value->type->is_int1_type()) {
        std::cerr << "!panic at getVR_from_ptr!: int1 value " << std::string(*value);
        exit(99);
    }
    if (value->isGlobal) {
        Arm::Glob *glob = globPtr2globNameOpd[(GlobalValue *) value];

        if (CenterControl::_GEP_CONST_BROADCAST && CenterControl::_O2) {
            if (curMF->glob2vr.count(glob) == 0) {
                Operand *addr = newVR();
                curMF->glob2vr[glob] = addr;
                curMF->reg2val[addr] = glob;
                return addr;
            } else {
                return curMF->glob2vr[glob];
            }
        }
        Operand *addr = newVR();
        I::Mov *iMov = new I::Mov(addr, glob, curMB);
        iMov->setGlobAddr();
        return addr;
    } else {
        return getVR_no_imm(value);
    }
}

CMPAndArmCond::CMPAndArmCond(MachineInst *cmp, Arm::Cond cond) {
    CMP = cmp;
    ArmCond = cond;
}
