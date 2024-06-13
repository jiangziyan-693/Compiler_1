//
// Created by XuRuiyuan on 2023/7/26.
//
#include "unordered_set"
#include "backend/PeepHole.h"
#include "lir/I.h"
#include "lir/V.h"
#include "lir/Operand.h"
#include "lir/Arm.h"
#include "backend/CodeGen.h"
#include "lir/StackCtrl.h"
#include "backend/RegAllocator.h"
#include "lir/I.h"
#include "iostream"

// TODO!:
//  add	r1,	r2,	#imm
// 	mov	r0,	#0
// 	str	r0,	[r1]

void PeepHole::run() {
    FOR_ITER_MF(mf, p) {
        bool unDone = true;
        while (unDone) {
            unDone = oneStage(mf);
            if (twoStage(mf))
                unDone = true;
        }
        threeStage(mf);
    }
}

void PeepHole::runOneStage() {
    FOR_ITER_MF(mf, p) {
        bool unDone = true;
        while (unDone) {
            unDone = oneStage(mf);
        }
    }
}

void PeepHole::threeStage(McFunction *mf) {
    FOR_ITER_MB(mb, mf) {
        curMB = mb;
        // Operand *G = new Operand*[16];
        Operand *GPRConstPool[GPR_NUM] = {};
        std::vector<MachineInst *> removeList;
        FOR_ITER_MI(mi, mb) {
            if (mi->isIMov() && mi->noShiftAndCond()) {
                I::Mov *iMov = dynamic_cast<I::Mov *>(mi);
                if (iMov->getSrc()->isImm()) {
                    Operand *gpr = GPRConstPool[iMov->getDst()->value];
                    if (gpr == iMov->getDst()) {
                        removeList.push_back(mi);
                    } else {
                        GPRConstPool[iMov->getDst()->value] = iMov->getSrc();
                    }
                }
            } else {
                for (Operand *def: mi->defOpds) {
                    if (def->isDataType(DataType::I32)) {
                        GPRConstPool[def->value] = nullptr;
                    }
                }
            }
        }
        for (MachineInst *mi: removeList) {
            mi->remove();
        }
    }
}

bool PeepHole::oneStage(McFunction *mf) {
    bool unDone = false;

    FOR_ITER_MB(mb, mf) {
        curMB = mb;
        FOR_ITER_MI(mi, mb) {
            // MachineInst *mi = dynamic_cast<MachineInst *>() i;
            MachineInst *prevInst =
                    mi->prev->prev == nullptr ? MachineInst::emptyInst : dynamic_cast<MachineInst *>(mi->prev);
            MachineInst *nextInst =
                    mi->next->next == nullptr ? MachineInst::emptyInst : dynamic_cast<MachineInst *>(mi->next);
            switch (mi->tag) {
                case MachineInst::Tag::Add:
                case MachineInst::Tag::Sub: {
                    I::Binary *bino = dynamic_cast<I::Binary *>(mi);
                    if (*(bino->getROpd()) == *(Operand::I_ZERO)) {
                        unDone = true;
                        if (*(bino->getDst()) == *(bino->getLOpd())) {
                            bino->remove();
                        } else {
                            new I::Mov(bino->getDst(), bino->getLOpd(), bino);
                            bino->remove();
                        }
                    }
                    break;
                }
                case MachineInst::Tag::Jump: {
                    if (mi->isNoCond() && (dynamic_cast<GDJump *>(mi))->getTarget() == curMB->next) {
                        unDone = true;
                        mi->remove();
                    }
                    break;
                }
                case MachineInst::Tag::Ldr: {
                    if (oneStageLdr(MachineInst::Tag::Str, mi, prevInst)) unDone = true;
                    break;
                }
                case MachineInst::Tag::VLdr: {
                    if (oneStageLdr(MachineInst::Tag::VStr, mi, prevInst)) unDone = true;
                    break;
                }
                case MachineInst::Tag::IMov:
                case MachineInst::Tag::VMov: {
                    if (oneStageMov(mi, nextInst)) unDone = true;
                    break;
                }
            }
        }
    }
    return unDone;
}

bool PeepHole::oneStageMov(MachineInst *mi, MachineInst *nextInst) {
    if (!mi->getShift()->hasShift()) {
        // auto *p = dynamic_cast<MachineMove *>(mi);
        // todo: unknown bug
        // CAST_IF(curMov, mi, MachineMove);
        MachineInst *curMov = mi;
        Operand *dst = curMov->getDst();
        Operand *src = curMov->getSrc();
        if (*(dst) == *(src)) {
            curMov->remove();
            return true;
        } else if (curMov->isNoCond() && nextInst->isOf(mi->tag)) {
            // CAST_IF(nextMov, nextInst, MachineMove);
            MachineInst *nextMov = nextInst;
            // MachineMove *nextMov = (MachineMove *) nextInst;
            if (*(nextMov->getDst()) == *(nextMov->getSrc())) {
                nextMov->remove();
                return true;
            } else if (*(nextMov->getDst()) == *(curMov->getDst())) {
                curMov->remove();
                return true;
            }
        }
    }
    return false;
}

bool PeepHole::oneStageLdr(MachineInst::Tag tag, MachineInst *mi, MachineInst *prevInst) {
    CAST_IF(ldr, mi, MachineMemInst);
    if (prevInst->isOf(tag)) {
        CAST_IF(str, prevInst, MachineMemInst);
        if (!(ldr->isNoCond() && str->isNoCond()))
            return false;
        if (*ldr->getAddr() == *(str->getAddr())
            && *ldr->getOffset() == *(str->getOffset())
            && *ldr->getShift() == *(str->getShift())) {
            if (tag == MachineInst::Tag::Str) new I::Mov(ldr->getData(), str->getData(), mi);
            else if (tag == MachineInst::Tag::VStr) new V::Mov(ldr->getData(), str->getData(), mi);
            else exit(151);
            ldr->remove();
            return true;
        }
    }
    return false;
}

MachineInst *PeepHole::getLastDefiner(Operand *opd) {
    if (dynamic_cast<Arm::Reg *>(opd) == nullptr) return nullptr;
    if (CodeGen::needFPU && opd->isF32()) {
        return lastFPRsDefMI[opd->value];
    }
    return lastGPRsDefMI[opd->value];
}

void PeepHole::putLastDefiner(Operand *opd, MachineInst *mi) {
    if (!(dynamic_cast<Arm::Reg *>(opd))) return;
    if (CodeGen::needFPU && opd->isF32()) lastFPRsDefMI[opd->value] = mi;
    else lastGPRsDefMI[opd->value] = mi;
}

bool PeepHole::twoStage(McFunction *mf) {
    bool unDone = false;
    FOR_ITER_MB(mb, mf) {
        mb->miNum = 0;
        curMB = mb;
        mb->liveUseSet = new std::unordered_set<Operand *>();
        mb->liveDefSet = new std::unordered_set<Operand *>();
        mb->liveInSet = new std::unordered_set<Operand *>();
        FOR_ITER_MI(mi, mb) {
            for (Operand *use: mi->useOpds)
                if (dynamic_cast<Arm::Reg *>(use) != nullptr && mb->liveDefSet->find(use) == mb->liveDefSet->end()) {
                    mb->liveUseSet->insert(use);
                    mb->liveInSet->insert(use);
                }
            for (Operand *def: mi->defOpds)
                if (dynamic_cast<Arm::Reg *>(def) != nullptr)
                    mb->liveDefSet->insert(def);
        }
        mb->liveOutSet = new std::unordered_set<Operand *>();
    }
    RegAllocator::liveInOutAnalysis(mf);
    FOR_ITER_MB(mb, mf) {
        for (int i = 0; i < GPR_NUM; i++) {
            lastGPRsDefMI[i] = nullptr;
        }
        for (int i = 0; i < FPR_NUM; i++) {
            lastFPRsDefMI[i] = nullptr;
        }
        FOR_ITER_MI(mi, mb) {
            if (mi->isComment()) continue;
            mb->miNum++;
            mi->theLastUserOfDef = nullptr;
            // std::vector<Operand *> uses = mi->useOpds;
            // std::vector<Operand *> defs = mi->defOpds;
            if (mi->isOf(MachineInst::Tag::ICmp) || mi->isOf(MachineInst::Tag::VCmp)) {
                mi->defOpds.push_back(Arm::Reg::getRreg(Arm::GPR::cspr));
            }
            if (mi->isCall()) {
                mi->defOpds.push_back(Arm::Reg::getRreg(Arm::GPR::cspr));
                mi->useOpds.push_back(Arm::Reg::getRreg(Arm::GPR::sp));
            }
            if (mi->hasCond()) {
                mi->useOpds.push_back(Arm::Reg::getRreg(Arm::GPR::cspr));
            }
            for (Operand *use: mi->useOpds) {
                if (__IS__(use, Arm::Reg)) {
                    MachineInst *lastDefMI = getLastDefiner(use);
                    if (lastDefMI != nullptr) {
                        lastDefMI->theLastUserOfDef = mi;
                    }
                }
            }
            for (Operand *def: mi->defOpds) {
                if (__IS__(def, Arm::Reg)) {
                    putLastDefiner(def, mi);
                }
            }
            if (mi->sideEff()) mi->theLastUserOfDef = mi;
            else mi->theLastUserOfDef = nullptr;
        }
        FOR_ITER_MI(mi, mb) {
            bool isLastDefMI = true;
            bool defRegInLiveOut = false;
            bool defNoSp = true;
            for (Operand *def: mi->defOpds) {
                // todo
                if (*mi != *(getLastDefiner(def))) isLastDefMI = false;
                if (mb->liveOutSet->find(def) != mb->liveOutSet->end()) defRegInLiveOut = true;
                if (Arm::Reg::getRreg(Arm::GPR::sp) == def) defNoSp = false;
            }
            if (!(isLastDefMI && defRegInLiveOut) && mi->isNoCond()) {
                if (__IS__(mi, StackCtl::StackCtrl)) continue;
                if (mi->theLastUserOfDef == nullptr && mi->noShift() && defNoSp) {
                    mi->remove();
                    unDone = true;
                    mb->miNum--;
                    continue;
                }
                // if (!mi->isNotLastInst()) continue;
                MachineInst *nextMI = dynamic_cast<MachineInst *>(mi->next);
                if (nextMI == nullptr) continue;
                if (mi->isIMov()) {
                    if (!CodeGen::immCanCode((dynamic_cast<I::Mov *>(mi))->getSrc()->value)) {
                        continue;
                    }
                }
                // TODO sub x, a, a
                // TODO reluctantStr ldr - mla - ldr - str
                if (mi->noShift()) {
                    // add/sub a c #i
                    // ldr b [a, #x]
                    // =>
                    // ldr b [c, #x+i]
                    // ---------------
                    // add/sub a c #i
                    // move b #x
                    // str b [a, #x]
                    // =>
                    // move b x
                    // str b [c, #x+i]
                    if (mi->isOf(MachineInst::Tag::Add) || mi->isOf(MachineInst::Tag::Sub)) {
                        I::Binary *binary = dynamic_cast<I::Binary *>(mi);
                        if (binary->getROpd()->isPureImmWithOutGlob(DataType::I32)) {
                            int imm = binary->getROpd()->value;
                            if (mi->lastUserIsNext()) {
                                switch (nextMI->tag) {
                                    case MachineInst::Tag::Ldr: {
                                        // add/sub a c #i
                                        // ldr b [a, #x]
                                        // =>
                                        // ldr b [c, #x+i]
                                        I::Ldr *ldr = dynamic_cast<I::Ldr *>(nextMI);
                                        if (*ldr->getAddr() == *(binary->getDst())
                                            && ldr->getOffset()->isPureImmWithOutGlob(DataType::I32)) {
                                            if (mi->isOf(MachineInst::Tag::Add)) {
                                                imm += ldr->getOffset()->get_I_Imm();
                                            } else if (mi->isOf(MachineInst::Tag::Sub)) {
                                                imm -= ldr->getOffset()->get_I_Imm();
                                            } else {
                                                exit(181);
                                            }
                                            if (CodeGen::LdrStrImmEncode(imm)) {
                                                unDone = true;
                                                ldr->setAddr(binary->getLOpd());
                                                ldr->setOffSet(new Operand(DataType::I32, imm));
                                                binary->remove();
                                            }
                                        }
                                        break;
                                    }
                                    case MachineInst::Tag::Str: {
                                        I::Str *str = dynamic_cast<I::Str *>(nextMI);
                                        if (*str->getAddr() == *(binary->getDst())
                                            && str->getOffset()->isPureImmWithOutGlob(DataType::I32)) {
                                            assert(!str->getShift()->hasShift());
                                            if (mi->isOf(MachineInst::Tag::Add)) {
                                                imm += str->getOffset()->get_I_Imm();
                                            } else if (mi->isOf(MachineInst::Tag::Sub)) {
                                                imm -= str->getOffset()->get_I_Imm();
                                            } else {
                                                exit(182);
                                            }
                                            if (CodeGen::LdrStrImmEncode(imm)) {
                                                unDone = true;
                                                str->setAddr(binary->getLOpd());
                                                str->setOffSet(new Operand(DataType::I32, imm));
                                                binary->remove();
                                            }
                                        }
                                        break;
                                    }
                                    case MachineInst::Tag::IMov:
                                        if (mi->lastUserIsNext()
                                            && nextMI->getCond() == mi->getCond()
                                            && nextMI->noShift()) {
                                            I::Mov *iMov = dynamic_cast<I::Mov *>(nextMI);
                                            // add a, b, c
                                            // mov d, a
                                            // ->
                                            // add d, b, c
                                            if (*binary->getDst() == *iMov->getSrc()) {
                                                binary->setDst(iMov->getDst());
                                                iMov->remove();
                                                unDone = true;
                                                if (lastGPRsDefMI[iMov->getDst()->value] == nextMI) {
                                                    lastGPRsDefMI[iMov->getDst()->value] = mi;
                                                    // exit(112);
                                                }
                                            }
                                        }
                                        break;
                                }
                            } else if (nextMI->isIMov()) {
                                // add/sub a c #i
                                // move b #x
                                // str b [a, #x]
                                // =>
                                // move b #x
                                // str b [c, #x+i]
                                // TODO 这个原来会跳过, 现在慎用
                                // 怀疑已经被fixStack的时候消除了
                                MachineInst *secondNextMI = dynamic_cast<MachineInst *>(mi->next);
                                if (mi->theLastUserOfDef != nullptr
                                    && mi->theLastUserOfDef == secondNextMI) {
                                    I::Mov *iMov = dynamic_cast<I::Mov *>(nextMI);
                                    if (secondNextMI->isOf(MachineInst::Tag::Str)) {
                                        I::Str *str = dynamic_cast<I::Str *>(secondNextMI);
                                        if (*iMov->getDst() == *(str->getData())
                                            && *binary->getDst() == *(str->getAddr())
                                            && *str->getData() != *(binary->getLOpd())
                                            && str->getOffset()->isPureImmWithOutGlob(DataType::I32)) {
                                            if (CodeGen::LdrStrImmEncode(imm)) {
                                                unDone = true;
                                                str->setAddr(binary->getLOpd());
                                                str->setOffSet(new Operand(DataType::I32, imm));
                                                binary->remove();
                                            }
                                        }
                                    }
                                }
                            }
                        } else if (mi->isOf(MachineInst::Tag::Sub) && nextMI->isOf(MachineInst::Tag::Sub)
                                   && mi->lastUserIsNext()
                                   && nextMI->getCond() == mi->getCond()
                                   && nextMI->noShift()) {
                            // sub a, b, a
                            // sub b, b, a
                            // a2 = b - a1
                            // b = b - a2 = b - (b - a1) = a1
                            I::Binary *sub1 = binary;
                            I::Binary *sub2 = dynamic_cast<I::Binary *>(nextMI);
                            std::unordered_set<Operand *> aSet;
                            aSet.insert(sub1->getDst());
                            aSet.insert(sub1->getROpd());
                            aSet.insert(sub2->getROpd());
                            std::unordered_set<Operand *> bSet;
                            bSet.insert(sub1->getLOpd());
                            bSet.insert(sub2->getDst());
                            bSet.insert(sub2->getLOpd());
                            if (aSet.size() + bSet.size() <= 2) {
                                unDone = true;
                                I::Mov *iMov = new I::Mov(sub2->getDst(), sub1->getDst(), sub2);
                                sub1->remove();
                                sub2->remove();
                                mb->miNum--;
                                iMov->theLastUserOfDef = sub2->theLastUserOfDef;
                                if (lastGPRsDefMI[iMov->getDst()->value] != dynamic_cast<MachineInst *>(sub2)) {
                                    exit(111);
                                }
                                lastGPRsDefMI[iMov->getDst()->value] = dynamic_cast<MachineInst *>(iMov);
                            }
                        } else if (nextMI->isOf(MachineInst::Tag::IMov)
                                   && mi->lastUserIsNext()
                                   && nextMI->getCond() == mi->getCond()
                                   && nextMI->noShift()) {
                            I::Mov *iMov = dynamic_cast<I::Mov *>(nextMI);
                            // add a, b, c
                            // mov d, a
                            // ->
                            // add d, b, c
                            if (*binary->getDst() == *iMov->getSrc()) {
                                binary->setDst(iMov->getDst());
                                iMov->remove();
                                unDone = true;
                                if (lastGPRsDefMI[iMov->getDst()->value] != nextMI) {
                                    exit(115);
                                }
                                lastGPRsDefMI[iMov->getDst()->value] = mi;
                            }


                        }
                    } else if (mi->isOf(MachineInst::Tag::IMov)) {
                        // mov a, b
                        // anything
                        // =>
                        // anything (replaced)
                        I::Mov *iMov = dynamic_cast<I::Mov *>(mi);
                        if (!iMov->getSrc()->isImm()
                            && __IS__(nextMI, IMI)
                            && mi->lastUserIsNext()
                            && !nextMI->isOf(MachineInst::Tag::Call)
                            && !nextMI->isOf(MachineInst::Tag::IRet)
                            && !nextMI->isOf(MachineInst::Tag::VRet)) {
                            bool isDeleted = false;
                            for (int i = 0; i < nextMI->useOpds.size(); i++) {
                                if (*nextMI->useOpds[i] == *(iMov->getDst())) {
                                    nextMI->useOpds[i] = iMov->getSrc();
                                    isDeleted = true;
                                }
                            }
                            if (isDeleted) {
                                unDone = true;
                                mi->remove();
                            }
                        } else if (iMov->getSrc()->isPureImmWithOutGlob(DataType::I32)) {
                            int imm = iMov->getSrc()->value;
                            if (iMov->lastUserIsNext() && nextMI->noShift()) {
                                switch (nextMI->tag) {
                                    case MachineInst::Tag::ICmp: {
                                        I::Cmp *icmp = dynamic_cast<I::Cmp *>(nextMI);
                                        Operand *dst = iMov->getDst();
                                        if (*dst == *(icmp->getROpd()) && *dst != *(icmp->getLOpd())) {
                                            if (CodeGen::immCanCode(imm)) {
                                                icmp->setROpd(iMov->getSrc());
                                                unDone = true;
                                                iMov->remove();
                                            } else if (CodeGen::immCanCode(-imm)) {
                                                icmp->setROpd(new Operand(DataType::I32, -imm));
                                                icmp->cmn = true;
                                                unDone = true;
                                                iMov->remove();
                                            }
                                        }
                                        break;
                                    }
                                    case MachineInst::Tag::Add:
                                    case MachineInst::Tag::Sub: {
                                        I::Binary *binary = (dynamic_cast<I::Binary *>(nextMI));
                                        Operand *l = binary->getLOpd();
                                        Operand *r = binary->getROpd();
                                        if (CodeGen::immCanCode(imm) && *r == *(iMov->getDst()) &&
                                            *l != *(iMov->getDst())) {
                                            binary->setROpd(iMov->getSrc());
                                            iMov->remove();
                                            unDone = true;
                                        }
                                        break;
                                    }
                                }
                            } else if (nextMI->isNotLastInst() && nextMI->noShiftAndCond()) {
                                MachineInst *next2MI = dynamic_cast<MachineInst *>(nextMI->next);
                                if (mi->theLastUserOfDef == next2MI && next2MI->isOf(MachineInst::Tag::ICmp) &&
                                    next2MI->noShift()) {
                                    I::Cmp *icmp = dynamic_cast<I::Cmp *>(next2MI);
                                    bool flag = true;
                                    // std::vector<Operand *> uses = nextMI->useOpds;
                                    // std::vector<Operand *> defs = nextMI->defOpds;
                                    if (nextMI->isOf(MachineInst::Tag::ICmp)
                                        || nextMI->isOf(MachineInst::Tag::VCmp)) {
                                        nextMI->defOpds.push_back(Arm::Reg::getRreg(Arm::GPR::cspr));
                                    }
                                    if (nextMI->isCall()) {
                                        nextMI->defOpds.push_back(Arm::Reg::getRreg(Arm::GPR::cspr));
                                        nextMI->useOpds.push_back(Arm::Reg::getRreg(Arm::GPR::sp));
                                    }
                                    for (Operand *def: nextMI->defOpds) {
                                        if (*def == *iMov->getDst() && *def != *iMov->getSrc()) {
                                            flag = false;
                                        }
                                    }
                                    for (Operand *use: nextMI->useOpds) {
                                        if (use == iMov->getDst()) {
                                            flag = false;
                                        }
                                    }
                                    if (flag && icmp->getROpd() == iMov->getDst()) {
                                        unDone = true;
                                        icmp->setROpd(iMov->getSrc());
                                        iMov->remove();
                                    }
                                }
                            }
                        }
                    } else if (__IS__(mi, ActualDefMI)) {
                        if (mi->lastUserIsNext()) {
                            if (isSimpleIMov(nextMI)) {
                                Operand *def = (dynamic_cast<ActualDefMI *>(mi)->getDef());
                                I::Mov *iMov = dynamic_cast<I::Mov *>(nextMI);
                                if (*def == *(iMov->getSrc())) {
                                    mi->setDef(iMov->getDst());
                                    iMov->remove();
                                    unDone = true;
                                }
                            } else if (mi->isOf(MachineInst::Tag::Mul)
                                       && (nextMI->isOf(MachineInst::Tag::Add)
                                           || nextMI->isOf(MachineInst::Tag::Sub))
                                       && mi->getCond() == nextMI->getCond()) {
                                I::Binary *mul = dynamic_cast<I::Binary *>(mi);
                                I::Binary *addOrSub = dynamic_cast<I::Binary *>(nextMI);
                                Operand *mulDst = mul->getDst();
                                Operand *mulLOpd = mul->getLOpd();
                                Operand *mulROpd = mul->getROpd();
                                Operand *asDst = addOrSub->getDst();
                                Operand *asLOpd = addOrSub->getLOpd();
                                Operand *asROpd = addOrSub->getROpd();
                                bool flag1 = *mulDst == *asLOpd;
                                bool flag2 = *mulDst == *asROpd;
                                I::Fma *fma = nullptr;
                                if (flag1 && !flag2 && !asROpd->isImm()) {
                                    fma = new I::Fma(addOrSub, nextMI->isOf(MachineInst::Tag::Add), false,
                                                     asDst, mulLOpd, mulROpd, asROpd);
                                } else if (flag2 && !flag1) {
                                    fma = new I::Fma(addOrSub, nextMI->isOf(MachineInst::Tag::Add), false,
                                                     asDst, mulLOpd, mulROpd, asLOpd);
                                }
                                if (fma != nullptr) {
                                    unDone = true;
                                    fma->theLastUserOfDef = addOrSub->theLastUserOfDef;
                                    if (lastGPRsDefMI[asDst->value] == addOrSub) {
                                        lastGPRsDefMI[asDst->value] = fma;
                                    }
                                    mul->remove();
                                    addOrSub->remove();
                                    mb->miNum--;
                                }
                            }
                        }
                    }
                } else {
                    if (mi->isOf(MachineInst::Tag::Add)) {
                        I::Binary *addMI = dynamic_cast<I::Binary *>(mi);
                        if (!(addMI->getROpd()->isImm() && CodeGen::LdrStrImmEncode(addMI->getROpd()->value))) {
                            switch (nextMI->getTag()) {
                                case MachineInst::Tag::Ldr:
                                case MachineInst::Tag::Str: {
                                    // add a, b, c, shift
                                    // ldr/str x, [a, #0]
                                    // =>
                                    // ldr/str x, [b, c, shift]
                                    if (mi->lastUserIsNext()) {
                                        MachineMemInst *mem = dynamic_cast<MachineMemInst *>(nextMI);
                                        if (*addMI->getDst() == *(mem->getAddr())
                                            && *mem->getOffset() == *(Operand::I_ZERO)
                                            && mem->isNoCond()) {
                                            unDone = true;
                                            mem->setAddr(addMI->getLOpd());
                                            mem->setOffSet(addMI->getROpd());
                                            mem->addShift(addMI->getShift());
                                            addMI->remove();
                                        }
                                    }
                                    break;
                                }
                                case MachineInst::Tag::IMov: {
                                    if (nextMI->isNotLastInst() && nextMI->isNoCond()) {
                                        MachineInst *secondNextMI = dynamic_cast<MachineInst *>(nextMI->next);;
                                        // todo secondNextMI->isNotLastInst() && no need
                                        if (secondNextMI->isNotLastInst() && secondNextMI->isNoCond()
                                            && mi->theLastUserOfDef != nullptr
                                            && mi->theLastUserOfDef == secondNextMI
                                            && secondNextMI->isOf(MachineInst::Tag::Str)) {
                                            I::Str *str = dynamic_cast<I::Str *>(secondNextMI);
                                            I::Mov *mov = dynamic_cast<I::Mov *>(nextMI);
                                            if (*mov->getDst() == *(str->getData())
                                                && *str->getAddr() == *(addMI->getDst())
                                                && *str->getData() != *(addMI->getLOpd())
                                                && *str->getData() != *(addMI->getROpd())
                                                && *str->getOffset() == *(Operand::I_ZERO)) {
                                                // add a, b, c, shift
                                                // move d y
                                                // str d [a, #0]
                                                // =>
                                                // move d y
                                                // str d [b, c shift]
                                                // ----------------------------------
                                                // this situation should be avoided:
                                                // add a, d, c, shift
                                                // move d y
                                                // str d [d, c shift]
                                                str->setAddr(addMI->getLOpd());
                                                str->setOffSet(addMI->getROpd());
                                                str->MachineInst::addShift(addMI->getShift());
                                                addMI->remove();
                                                unDone = true;
                                            }
                                        }
                                    }
                                    break;
                                }
                            }
                        }
                    } else if (mi->isOf(MachineInst::Tag::IMov)
                               && mi->lastUserIsNext()
                               && nextMI->getCond() == mi->getCond()
                               && nextMI->noShift()) {
                        I::Mov *iMov = dynamic_cast<I::Mov *>(mi);
                        switch (nextMI->getTag()) {
                            case MachineInst::Tag::Add:
                            case MachineInst::Tag::Sub: {
                                I::Binary *binary = dynamic_cast<I::Binary *>(nextMI);
                                Operand *lOpd = binary->getLOpd();
                                Operand *rOpd = binary->getROpd();
                                if (*rOpd == *(iMov->getDst()) && *lOpd != *rOpd) {
                                    binary->setROpd(iMov->getSrc());
                                    binary->addShift(iMov->getShift());
                                    unDone = true;
                                    iMov->remove();
                                }
                                break;
                            }
                            case MachineInst::Tag::Ldr:
                            case MachineInst::Tag::Str: {
                                MachineMemInst *memLdrStr = dynamic_cast<MachineMemInst *>(nextMI);
                                if (*memLdrStr->getOffset() == *(iMov->getDst())
                                    && memLdrStr->getShift()->noShift()
                                    && *memLdrStr->getAddr() != *(iMov->getDst())
                                    &&
                                    ((nextMI->isOf(MachineInst::Tag::Ldr) ||
                                      *memLdrStr->getData() == *(iMov->getDst())))) {
                                    unDone = true;
                                    memLdrStr->setOffSet(iMov->getDst());
                                    memLdrStr->addShift(iMov->getShift());
                                    iMov->remove();
                                }
                                break;
                            }
                            case MachineInst::Tag::IMov: {
                                I::Mov *nextMov = dynamic_cast<I::Mov *>(nextMI);
                                if (*nextMov->getSrc() == *(iMov->getDst())) {
                                    unDone = true;
                                    nextMov->setSrc(iMov->getSrc());
                                    nextMov->addShift(iMov->getShift());
                                    iMov->remove();
                                }
                                break;
                            }
                        }
                    }
                }
            }
            if (unDone) mb->miNum--;
        }
    }
    return unDone;
}

bool PeepHole::isSimpleIMov(MachineInst *mi) {
    return mi->isIMov() && mi->noShiftAndCond();
}
