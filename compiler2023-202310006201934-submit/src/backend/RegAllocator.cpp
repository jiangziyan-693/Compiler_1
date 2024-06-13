//
// Created by Administrator on 2023/7/26.
//

#include "backend/RegAllocator.h"
#include "backend/PeepHole.h"
#include "backend/MC.h"
#include "backend/CodeGen.h"
#include "lir/MachineInst.h"
#include "lir/Arm.h"
#include "lir/I.h"
#include "lir/V.h"
#include "lir/Operand.h"
#include "lir/StackCtrl.h"
#include "util/util.h"
#include "mir/BasicBlock.h"
#include "math.h"
// #include "Init.h"
// #include "FrontendInit.h"
// #define LOG

void RegAllocator::addEdge(Operand *u, Operand *v) {
    // todo :
    if (/*(u->hasReg() && v->hasReg())
        ||*/ u == rSP
        || v == rSP)
        return;
    AdjPair *adjPair = new AdjPair(u, v);

    if (!(adjSet->find(*adjPair) != adjSet->end() || *u == *v)) {
        adjSet->insert(*adjPair);
        adjSet->insert(*(new AdjPair(v, u)));
        if (!u->isPreColored(dataType)) {
            u->addAdj(v);
            u->degree++;
        }
        if (!v->isPreColored(dataType)) {
            v->addAdj(u);
            v->degree++;
        }
    }
}

void RegAllocator::fixStack(std::vector<IMI *> *needFixList) {
    for (IMI *mi: *needFixList) {
        // MachineInst *mi = dynamic_cast<MachineInst *>(it);
        McFunction *mf = mi->mb->mf;
        if (dynamic_cast<I::Binary *>(mi) != nullptr) {
            I::Binary *binary = dynamic_cast<I::Binary *>(mi);
            Operand *off;
            int newOff = 0;
            switch (binary->getFixType()) {
                case ENUM::VAR_STACK: {
                    newOff = mf->getVarStack();
                    break;
                }
                case ENUM::ONLY_PARAM: {
                    newOff = binary->callee->paramStack;
                    break;
                }
                default: {
                    exit(110);
                }
            }
            if (newOff == 0 && binary->getFixType() != ENUM::VAR_STACK) {
                // if (newOff == 0) {
                binary->remove();
            } else {
                if (CodeGen::immCanCode(newOff)) {
                    // todo
                    off = new Operand(DataType::I32, newOff);
                } else {
                    // 当前函数栈空间大, 切参数大于三个的时候会用到r4, 但是寄存器如果从r12开始分就需要当前函数加r4的使用, 否则会用r4但是没有push
                    //
                    if (!CenterControl::_FAST_REG_ALLOCATE)
                        binary->mb->mf->addUsedGPRs(Arm::GPR::r4);
                    off = Arm::Reg::getR(Arm::GPR::r4);
                    new I::Mov(off, new Operand(DataType::I32, newOff), binary);
                }
                binary->setROpd(off);
            }
        } else if (mi->isIMov()) {
            I::Mov *mv = dynamic_cast<I::Mov *>(mi);
            Operand *off = mv->getSrc();
            int newOff = mf->getTotalStackSize() + off->get_I_Imm();
            if (mv->getFixType() == ENUM::FLOAT_TOTAL_STACK) {
                if (CenterControl::_FixStackWithPeepHole && CodeGen::vLdrStrImmEncode(newOff) &&
                    __IS__(mv->next, I::Ldr)) {
                    V::Ldr *vldr = dynamic_cast<V::Ldr *>(mv->next->next);
                    vldr->setUse(0, Arm::Reg::getRreg(Arm::GPR::sp));
                    vldr->setOffSet(new Operand(DataType::I32, newOff));
                    mv->next->remove();
                    mv->clearNeedFix();
                    mv->remove();
                } else if (CenterControl::_FixStackWithPeepHole && CodeGen::immCanCode(newOff) &&
                           __IS__(mv->next, I::Binary)) {
                    I::Binary *binary = dynamic_cast<I::Binary *>(mv->next);
                    mv->clearNeedFix();
                    mv->remove();
                    binary->setROpd(new Operand(DataType::I32, newOff));
                } else {
                    mv->setSrc(new Operand(DataType::I32, newOff));
                    mv->clearNeedFix();
                }
            } else if (mv->getFixType() == ENUM::INT_TOTAL_STACK) {
                if (CenterControl::_FixStackWithPeepHole && CodeGen::LdrStrImmEncode(newOff) &&
                    __IS__(mv->next, I::Ldr)) {
                    I::Ldr *ldr = dynamic_cast<I::Ldr *>(mv->next);
                    ldr->setOffSet(new Operand(DataType::I32, newOff));
                    mv->clearNeedFix();
                    mv->remove();
                } else {
                    mv->setSrc(new Operand(DataType::I32, newOff));
                    mv->clearNeedFix();
                }
            } else {
                exit(120);
            }
        } else {
            exit(120);
        }
    }
}

void RegAllocator::liveInOutAnalysis(McFunction *mf) {
    bool changed = true;
    while (changed) {
        changed = false;
        FOR_ITER_REVERSE_MB(mb, mf) {
            std::vector<Operand *> newLiveOut;
            for (McBlock *succMB: mb->succMBs) {
                for (Operand *liveIn: *succMB->liveInSet) {
                    if (mb->liveOutSet->find(liveIn) == mb->liveOutSet->end()) {
                        mb->liveOutSet->insert(liveIn);
                        newLiveOut.push_back(liveIn);
                    }
                }
            }
            if (newLiveOut.size() > 0) {
                changed = true;
            }
            if (changed) {
                for (Operand *o: *mb->liveOutSet) {
                    if (mb->liveDefSet->find(o) == mb->liveDefSet->end()) {
                        mb->liveInSet->insert(o);
                    }
                }
            }
        }
    }
}

void RegAllocator::dealDefUse(std::set<Operand *, CompareOperand> *live, MachineInst *mi, McBlock *mb) {
    std::vector<Operand *> &defs = mi->defOpds;
    std::vector<Operand *> &uses = mi->useOpds;
    int loopDepth = (mb->bb->getLoopDep());
    if (defs.size() == 1) {
        Operand *def = defs[0];
        if (def->needColor(dataType)) {
            live->insert(def);
            for (Operand *l: *live) {
                addEdge(l, def);
            }
            def->loopCounter += loopDepth;
        }
        if (live->find(def) != live->end()) {
            live->erase(def);
        }
    } else if (defs.size() > 1) {
        for (Operand *def: defs) {
            if (def->needColor(dataType)) {
                live->insert(def);
            }
        }
        for (Operand *def: defs) {
            if (def->needColor(dataType)) {
                for (Operand *l: *live) {
                    addEdge(l, def);
                }
            }
        }

        for (Operand *def: defs) {
            if (def->needColor(dataType)) {
                live->erase(def);
                def->loopCounter += loopDepth;
            }
        }
    }
    for (Operand *use: uses) {
        if (use->needColor(dataType)) {
            live->insert(use);
            use->loopCounter += loopDepth;
        }
    }
    int num = std::pow(8, loopDepth);
    for (Operand *def: defs) {
        def->storeWeight = num;
    }
    for (Operand *use: uses) {
        use->loadWeight = num;
    }
}

std::set<Operand *, CompareOperand> *RegAllocator::adjacent(Operand *x) {
    std::set<Operand *, CompareOperand> *validConflictOpdSet = new std::set<Operand *, CompareOperand>();
    for (Operand *r: x->adjOpdSet) {
        if (std::find(selectStack->begin(), selectStack->end(), r) == selectStack->end()
            && coalescedNodeSet->find(r) == coalescedNodeSet->end()) {
            validConflictOpdSet->insert(r);
        }
    }
    return validConflictOpdSet;
}

Operand *RegAllocator::getAlias(Operand *x) {
    while (coalescedNodeSet->find(x) != coalescedNodeSet->end()) {
        x = x->alias;
    }
    return x;
}

void RegAllocator::turnInit(McFunction *mf) {
    livenessAnalysis(mf);
    adjSet = new std::set<AdjPair>();
    simplifyWorkSet = new std::set<Operand *, CompareOperand>();
    freezeWorkSet = new std::set<Operand *, CompareOperand>();
    spillWorkSet = new std::set<Operand *, CompareOperand>();
    spilledNodeSet = new std::set<Operand *, CompareOperand>();
    coloredNodeList = new std::vector<Operand *>();
    selectStack = new std::vector<Operand *>();
    coalescedNodeSet = new std::set<Operand *, CompareOperand>();
    coalescedMoveSet = new std::set<MachineInst *, CompareMI>();
    constrainedMoveSet = new std::set<MachineInst *, CompareMI>();
    frozenMoveSet = new std::set<MachineInst *, CompareMI>();
    workListMoveSet = new std::set<MachineInst *, CompareMI>();
    activeMoveSet = new std::set<MachineInst *, CompareMI>();
}


void RegAllocator::livenessAnalysis(McFunction *mf) {
    FOR_ITER_MB(mb, mf) {
        mb->liveUseSet = new std::unordered_set<Operand *>();
        mb->liveDefSet = new std::unordered_set<Operand *>();
        mb->liveInSet = new std::unordered_set<Operand *>();
        FOR_ITER_MI(mi, mb) {
            if (mi->tag == MachineInst::Tag::Comment
                // TODO:
                || (dataType == DataType::F32 && (dynamic_cast<VMI *>(mi) == nullptr))) {
                continue;
            }
            for (Operand *use: mi->useOpds) {
                if (use->needRegOf(dataType) && mb->liveDefSet->find(use) == mb->liveDefSet->end()) {
                    mb->liveUseSet->insert(use);
                    mb->liveInSet->insert(use);
                }
            }
            for (Operand *def: mi->defOpds) {
                if (def->needRegOf(dataType)) {
                    mb->liveDefSet->insert(def);
                }
            }
        }
#ifdef LOG
        std::cerr << mb->getLabel() << "\tdefSet:\t";
        for (Operand *o: *mb->liveDefSet) {
            std::cerr << std::string(*o) << ", ";
        }
        std::cerr << std::endl;

        std::cerr << mb->getLabel() << "\tuseSet:\t";
        for (Operand *o: *mb->liveUseSet) {
            std::cerr << std::string(*o) << ", ";
        }
        std::cerr << std::endl;
#endif
        mb->liveOutSet = new std::unordered_set<Operand *>();
    }
    liveInOutAnalysis(mf);
}

void RegAllocator::build() {
    FOR_ITER_REVERSE_MB(mb, curMF) {
        std::set<Operand *, CompareOperand> *live = new std::set<Operand *, CompareOperand>();
        for (Operand *o: *mb->liveOutSet) {
            live->insert(o);
        }
        FOR_ITER_REVERSE_MI(mi, mb) {
            if (mi->tag != MachineInst::Tag::Comment) {
                if (mi->isMovOfDataType(dataType)) {
                    MachineInst *mv = dynamic_cast<MachineInst *>(mi);
                    if (mv->directColor()) {
                        // 没有cond, 没有shift, src和dst都是虚拟寄存器的mov指令
                        // move 的 dst 和 src 不应是直接冲突的关系, 而是潜在的可合并的关系
                        // move a, b --> move rx, rx 需要a 和 b 不是冲突关系
                        live->erase(mv->getSrc());
                        mv->getDst()->movSet.insert(mv);
                        mv->getSrc()->movSet.insert(mv);
                        workListMoveSet->insert(mv);
                    }
                }
                dealDefUse(live, mi, mb);
            }
        }
#ifdef LOG
        std::cerr << mb->getLabel() << "\tadjSet:\t";
        for (const AdjPair &a: *adjSet) {
            std::cerr << "(" << std::string(*a.u) << ", " << std::string(*a.v) << "),";
        }
        std::cerr << std::endl;
#endif
    }
}

void RegAllocator::regAllocIteration() {
    auto TYPE1 = [&]() {
        if (!simplifyWorkSet->empty()) {
            simplify();
        } else if (!workListMoveSet->empty()) {
            coalesce();
        } else if (!freezeWorkSet->empty()) {
            freeze();
        } else if (!spillWorkSet->empty()) {
            select_spill();
        }
    };

    auto TYPE2 = [&]() {
        if (!simplifyWorkSet->empty()) {
            simplify();
        }
        if (!workListMoveSet->empty()) {
            coalesce();
        }
        if (!freezeWorkSet->empty()) {
            freeze();
        }
        if (!spillWorkSet->empty()) {
            select_spill();
        }
    };

    auto TYPE3 = [&]() {
        if (!workListMoveSet->empty()) {
            coalesce();
        } else if (!simplifyWorkSet->empty()) {
            simplify();
        } else if (!freezeWorkSet->empty()) {
            freeze();
        } else if (!spillWorkSet->empty()) {
            select_spill();
        }
    };
    while (simplifyWorkSet->size() + workListMoveSet->size() + freezeWorkSet->size() + spillWorkSet->size() > 0) {
        // CenterControl::_PREC_MB_TOO_MUCH = false;
        TYPE1();
        // TYPE2();
        // TYPE3();
    }
}

void RegAllocator::simplify() {
    Operand *x = *simplifyWorkSet->begin();
    simplifyWorkSet->erase(x);
    selectStack->push_back(x);
    for (Operand *adj: x->adjOpdSet) {
        if (std::find(selectStack->begin(), selectStack->end(), adj) == selectStack->end()
            && coalescedNodeSet->count(x) != 0) {
            decrementDegree(adj);
        }
    }
}

void RegAllocator::freeze() {
    Operand *x = *freezeWorkSet->begin();
    freezeWorkSet->erase(x);
    simplifyWorkSet->insert(x);
    freezeMoves(x);
}

void RegAllocator::select_spill() {
    Operand *x = nullptr;
    bool reSpill = (dataType == DataType::I32 && CenterControl::_GPR_NOT_RESPILL)
                   || (dataType == DataType::F32 && CenterControl::_FPR_NOT_RESPILL);
    double max = 0;//= x->heuristicVal();
    for (Operand *o: *spillWorkSet) {
        double h = o->heuristicVal();
        if (x == nullptr) x = o;
        if (reSpill && o->isVirtual(dataType)) {
            if (newVRLiveLength->find(o) != newVRLiveLength->end()) {
                if (newVRLiveLength->operator[](o) < 5) {
                    h = 0;
                }
            }
        }
        if (h > max) {
            x = o;
            max = h;
        }
    }
    simplifyWorkSet->insert(x);
    freezeMoves(x);
    spillWorkSet->erase(x);
};

std::set<MachineInst *, CompareMI> *RegAllocator::nodeMoves(Operand *x) {
    std::set<MachineInst *, CompareMI> *canCoalesceSet = new std::set<MachineInst *, CompareMI>();
    for (MachineInst *r: x->movSet) {
        if (activeMoveSet->find(r) != activeMoveSet->end()
            || workListMoveSet->find(r) != workListMoveSet->end()) {
            canCoalesceSet->insert(r);
        }
    }
    return canCoalesceSet;
}

void RegAllocator::decrementDegree(Operand *x) {
    x->degree--;
    if (x->degree == K - 1) {
        for (MachineInst *mv: *nodeMoves(x)) {
            if (activeMoveSet->find(mv) != activeMoveSet->end()) {
                activeMoveSet->erase(mv);
                workListMoveSet->insert(mv);
            }
        }
        for (Operand *adj: *adjacent(x)) {
            for (MachineInst *mv: *nodeMoves(adj)) {
                if (activeMoveSet->find(mv) != activeMoveSet->end()) {
                    activeMoveSet->erase(mv);
                    workListMoveSet->insert(mv);
                }
            }
        }
        // todo: erase
        spillWorkSet->insert(x);
        if (!nodeMoves(x)->empty()) {
            freezeWorkSet->insert(x);
        } else {
            simplifyWorkSet->insert(x);
        }
    }
}

void RegAllocator::addWorkList(Operand *x) {
    if (!x->isPreColored(dataType) && (nodeMoves(x)->empty()) && x->degree < K) {
        assert(x->needColor(dataType));
        freezeWorkSet->erase(x);
        simplifyWorkSet->insert(x);
    }
}

void RegAllocator::combine(Operand *u, Operand *v) {
    if (freezeWorkSet->find(v) != freezeWorkSet->end()) {
        freezeWorkSet->erase(v);
    } else {
        spillWorkSet->erase(v);
    }
    coalescedNodeSet->insert(v);
    v->setAlias(u);
    for (MachineInst *mv: v->movSet) {
        u->movSet.insert(mv);
    }
    for (Operand *adj: v->adjOpdSet) {
        if (std::find(selectStack->begin(), selectStack->end(), adj) == selectStack->end()
            && coalescedNodeSet->find(adj) == coalescedNodeSet->end()) {
            addEdge(adj, u);
            decrementDegree(adj);
        }
    }
    if (u->degree >= K && freezeWorkSet->find(u) != freezeWorkSet->end()) {
        freezeWorkSet->erase(u);
        spillWorkSet->insert(u);
    }
}

void RegAllocator::coalesce() {
    MachineInst *mv = *workListMoveSet->begin();
    Operand *u = getAlias(mv->getDst());
    Operand *v = getAlias(mv->getSrc());
    if (v->isPreColored(dataType)) {
        Operand *tmp = u;
        u = v;
        v = tmp;
    }
    if (u->isPreColored(dataType) && u->value == 0
        && v->value == 0 && v->isVirtual(dataType)) {
        int a = 0;
    }
    workListMoveSet->erase(mv);
    if (*u == *v) {
        assert(u == v);
        coalescedMoveSet->insert(mv);
        addWorkList(u);
    } else if (v->isPreColored(dataType) || adjSet->find(*(new AdjPair(u, v))) != adjSet->end()) {
        constrainedMoveSet->insert(mv);
        addWorkList(u);
        addWorkList(v);
    } else {
        if (u->isPreColored(dataType)) {
            bool flag = true;
            for (Operand *adj: *adjacent(v)) {
                if (adj->degree >= K && !adj->isPreColored(dataType) &&
                    adjSet->find(*(new AdjPair(adj, u))) == adjSet->end()) {
                    // sp跟所有寄存器都冲突, 所以其实这里如果adj是sp的话一定不会进这个分支,不太影响
                    flag = false;
                }
            }
            if (flag) {
                coalescedMoveSet->insert(mv);
                combine(u, v);
                addWorkList(u);
            } else {
                activeMoveSet->insert(mv);
            }
        } else {
            std::set<Operand *, CompareOperand> unionSet;
            int cnt = 0;
            for (Operand *o: u->adjOpdSet) {
                unionSet.insert(o);
            }
            for (Operand *o: v->adjOpdSet) {
                unionSet.insert(o);
            }
            for (Operand *o: unionSet) {
                if (std::find(selectStack->begin(), selectStack->end(), o) == selectStack->end()
                    && coalescedNodeSet->find(o) == coalescedNodeSet->end()
                    && o->degree >= K) {
                    cnt++;
                }
            }
            if (cnt < K) {
                coalescedMoveSet->insert(mv);
                combine(u, v);
                addWorkList(u);
            } else {
                activeMoveSet->insert(mv);
            }
        }
    }
}

void RegAllocator::freezeMoves(Operand *u) {
    for (MachineInst *mv: *nodeMoves(u)) {
        if (activeMoveSet->find(mv) != activeMoveSet->end()) {
            activeMoveSet->erase(mv);
        } else {
            workListMoveSet->erase(mv);
        }
        frozenMoveSet->insert(mv);
        Operand *v;
        // todo
        if (*mv->getDst() == *u) {
            assert(mv->getDst() == u);
            v = mv->getSrc();
        } else {
            assert(mv->getDst() != u);
            v = mv->getDst();
        }
        // Operand *v = *mv->getDst() == *u ? mv->getSrc() : mv->getDst();
        if (nodeMoves(v)->empty() && v->degree < K) {
            freezeWorkSet->erase(v);
            simplifyWorkSet->insert(v);
        }
    }
}

struct EnumClassComparer {
    bool operator()(const Arm::GPR &lhs, const Arm::GPR &rhs) const {
        return static_cast<int>(lhs) < static_cast<int>(rhs);
    }
};

#define SET_BIT(num, bit) num &= ~((unsigned int) (1u << (bit)))

void RegAllocator::preAssignColors() {
    colorMap.clear();
    while (!selectStack->empty()) {
        Operand *toBeColored = selectStack->back();
        selectStack->pop_back();
        unsigned okColor = 0;
#ifdef ASSERT
        assert(K <= 32);
#endif
        if (K == 32) okColor = -1;
        else {
            for (int i = 0; i <= 12; i++) {
                okColor = okColor | (1u << i);
            }
            okColor = okColor | (1u << 14);
        }
        // unsigned int okColor = ((int) (1u << K)) - 1;
        // if (dataType == DataType::I32) SET_BIT(okColor, (int) Arm::GPR::sp);
        for (Operand *adj: toBeColored->adjOpdSet) {
            Operand *a = getAlias(adj);
            if (a->hasReg() && a->isDataType(dataType)) {
                SET_BIT(okColor, a->getReg()->value);
            } else if (a->isVirtual(dataType)) {
                if (colorMap.find(a) != colorMap.end()) {
                    Operand *r = colorMap[a];
                    SET_BIT(okColor, r->reg->value);
                }
            }
        }
        if (okColor == 0) {
            spilledNodeSet->insert(toBeColored);
        } else {
            // unsigned int i = 0;
            unsigned int color = 0;
            while (color < K) {
                if ((okColor & (1llu << color)) != 0) break;
                color++;
            }
            // unsigned int color = K - 1;
            // while (color > 0) {
            //     if ((okColor & (1llu << color)) != 0) break;
            //     color--;
            // }
            if (dataType == DataType::I32) {
                colorMap[toBeColored] = Arm::Reg::getRreg((int) color);
            } else {
                colorMap[toBeColored] = Arm::Reg::getSreg((int) color);
            }
        }
    }
}

void RegAllocator::mixedLivenessAnalysis(McFunction *mf) {

    FOR_ITER_MB(mb, mf) {
        mb->liveUseSet = new std::unordered_set<Operand *>();
        mb->liveDefSet = new std::unordered_set<Operand *>();
        mb->liveInSet = new std::unordered_set<Operand *>();
        FOR_ITER_MI(mi, mb) {
            if (mi->tag == MachineInst::Tag::Comment) {
                continue;
            }
            for (Operand *use: mi->useOpds) {
                if (use->opdType != Operand::OpdType::Immediate
                    && use->opdType != Operand::OpdType::FConst
                    && mb->liveDefSet->find(use) == mb->liveDefSet->end()) {
                    mb->liveUseSet->insert(use);
                    mb->liveInSet->insert(use);
                }
            }
            for (Operand *def: mi->defOpds) {
                if (def->opdType != Operand::OpdType::Immediate
                    && def->opdType != Operand::OpdType::FConst) {
                    mb->liveDefSet->insert(def);
                }
            }
        }
#ifdef LOG
        std::cerr << mb->getLabel() << "\tdefSet:\t";
        for (Operand *o: *mb->liveDefSet) {
            std::cerr << std::string(*o) << ", ";
        }
        std::cerr << std::endl;

        std::cerr << mb->getLabel() << "\tuseSet:\t";
        for (Operand *o: *mb->liveUseSet) {
            std::cerr << std::string(*o) << ", ";
        }
        std::cerr << std::endl;
#endif
        mb->liveOutSet = new std::unordered_set<Operand *>();
    }
    liveInOutAnalysis(mf);
}

void GPRegAllocator::AllocateRegister(McProgram *mcProgram) {
    FOR_ITER_MF(mf, mcProgram) {
        newVRLiveLength = new std::map<Operand *, int>();
        curMF = mf;
        while (true) {
            turnInit(mf);
            for (int i = 0; i < 13; i++) {
                Arm::Reg::getR(i)->degree = MAX_DEGREE;
            }
            Arm::Reg::getR(Arm::GPR::lr)->degree = MAX_DEGREE;
            for (Arm::Reg *reg: Arm::Reg::gprPool) {
                reg->loopCounter = 0;
                reg->degree = MAX_DEGREE;
                reg->adjOpdSet.clear();
                reg->movSet.clear();
                reg->setAlias(nullptr);
                reg->old = nullptr;
            }
            for (Operand *o: curMF->vrList) {
                o->loopCounter = 0;
                o->degree = 0;
                o->adjOpdSet.clear();
                o->movSet.clear();
                o->setAlias(nullptr);
                o->old = nullptr;
            }
            build();


            for (Operand *vr: curMF->vrList) {
                if (vr->degree >= K) {
                    spillWorkSet->insert(vr);
                } else if (!nodeMoves(vr)->empty()) {
                    freezeWorkSet->insert(vr);
                } else {
                    simplifyWorkSet->insert(vr);
                }
            }
            regAllocIteration();
            assignColors();
            if (spilledNodeSet->empty()) {
                break;
            }
            for (Operand *operand: *spilledNodeSet) {
                dealSpillNode(operand);
            }
        }
        int a = 0;
    }
}


void GPRegAllocator::dealSpillNode(Operand *x) {
    dealSpillTimes++;
    FOR_ITER_MB(mb, curMF) {
        toStack = !(CenterControl::_GEP_CONST_BROADCAST && CenterControl::_O2 && curMF->reg2val.count(x) != 0)/* ||
                  CenterControl::_GPRMustToStack || x->cost > 4 || x->getDefMI() == nullptr*/;
        curMB = mb;
        offImm = new Operand(DataType::I32, curMF->getVarStack());
        firstUse = nullptr;
        lastDef = nullptr;
        vr = nullptr;
        int checkCount = 0;
        FOR_ITER_MI(srcMI, mb) {
            if (srcMI->isCall() || srcMI->isComment()) continue;
            if (!srcMI->defOpds.empty()) {
                Operand *def = srcMI->defOpds[0];
                if (def == x) {

                    if (vr == nullptr) {
                        vr = vrCopy(x);
                    }
                    srcMI->setDef(vr);
                    lastDef = srcMI;
                }
            }
            for (int idx = 0; idx < srcMI->useOpds.size(); idx++) {
                Operand *use = srcMI->useOpds[idx];
                if (use == x) {
                    if (vr == nullptr) {
                        vr = vrCopy(x);
                    }
                    srcMI->setUse(idx, vr);
                    if (firstUse == nullptr && (CenterControl::_cutLiveNessShortest || lastDef == nullptr)) {
                        firstUse = srcMI;
                    }
                }
            }
            if (CenterControl::_cutLiveNessShortest || checkCount++ > SPILL_MAX_LIVE_INTERVAL) {
                checkpoint(x);
            }
        }
        checkpoint(x);
    }
    if (toStack) {
        curMF->addVarStack(4);
    }
}

void GPRegAllocator::checkpoint(Operand *x) {
    if (CenterControl::_GEP_CONST_BROADCAST && CenterControl::_O2 && curMF->reg2val.count(x) != 0) {
        // Operand *vr = iter.first;
        // if(lastDef != nullptr) {
        //     std::cerr << std::string(*lastDef) << std::endl;
        // }
        if (firstUse != nullptr) {
            RegValue &regVal = curMF->reg2val[x];
            if (regVal.index() == Immediate) {
                int val = std::get<int>(regVal);
                new I::Mov(vr, new Operand(DataType::I32, val), firstUse);
            } else if (regVal.index() == GlobBase) {
                Arm::Glob *glob = std::get<Arm::Glob *>(regVal);
                MachineInst *mv = new I::Mov(vr, glob, firstUse);
                mv->setGlobAddr();
            } else if (regVal.index() == StackAddr) {
                SpBasedOffAndSize sa = std::get<SpBasedOffAndSize>(regVal);
                Operand *tmp = nullptr;
                if (!CodeGen::immCanCode(sa.offset)) {
                    tmp = curMF->newVR();
                    new I::Mov(tmp, new Operand(DataType::I32, sa.offset), firstUse);
                } else {
                    tmp = new Operand(DataType::I32, sa.offset);
                }
                new I::Binary(MachineInst::Tag::Add, vr, Arm::Reg::getRreg(Arm::GPR::sp), tmp, firstUse);
            }
        }
    } else {
        toStack = true;
        if (firstUse != nullptr) {
            Operand *offset = offImm;
            MachineInst *mi;
            if (offImm->value >= 4096) {
                Operand *dst = curMF->newVR();
                new I::Mov(dst, new Operand(DataType::I32, offImm->value / 4), firstUse);

                new I::Ldr(vr, rSP, dst, new Arm::Shift(Arm::ShiftType::Lsl, 2), firstUse);
            } else {
                new I::Ldr(vr, rSP, offset, firstUse);
            }

        }
        if (lastDef != nullptr) {
            MachineInst *insertAfter = lastDef;
            Operand *offset = offImm;
            MachineInst *mi;
            if (offImm->value >= 4096) {
                Operand *dst = curMF->newVR();
                insertAfter = new I::Mov(lastDef, dst, new Operand(DataType::I32, offImm->value / 4));

                new I::Str(insertAfter, vr, rSP, dst, new Arm::Shift(Arm::ShiftType::Lsl, 2));
            } else {
                new I::Str(insertAfter, vr, rSP, offset);
            }

        }
    } /* else {
        if (firstUse != nullptr) {
            MachineInst *defMI = x->getDefMI();
            toInsertMIList.clear();
            regMap.clear();
            MachineInst *prevMI = firstUse;
            toInsertMIList.insert(defMI);
            if (defMI->defOpds.size() != 1) {
                exit(102);
            }
            Operand *def = defMI->defOpds[0];
            if (def->old != nullptr) {
                def = def->old;
            }
            regMap[def] = vr;
            while (!toInsertMIList.empty()) {
                MachineInst *mi = *toInsertMIList.begin();
                toInsertMIList.erase(mi);
                if (mi == nullptr) continue;
                if (dynamic_cast<I::Binary *>(mi) != nullptr) {
                    I::Binary *binary = dynamic_cast<I::Binary *>(mi);
                    Operand *l = binary->getLOpd();
                    Operand *r = binary->getROpd();
                    addDefiners(l, r);
                } else {
                    switch (mi->getTag()) {
                        case MachineInst::Tag::IMov: {
                            addDefiners((dynamic_cast<I::Mov *>(mi))->getSrc());
                            break;
                        }
                        case MachineInst::Tag::Ldr: {
                            addDefiners((dynamic_cast<I::Ldr *>(mi))->getAddr(),
                                        (dynamic_cast<I::Ldr *>(mi))->getOffset());
                            break;
                        }
                    }
                }
                if (dynamic_cast<I::Binary *>(mi) != nullptr) {
                    I::Binary *binary = dynamic_cast<I::Binary *>(mi);
                    Operand *dst = binary->getDst();
                    Operand *l = binary->getLOpd();
                    Operand *r = binary->getROpd();
                    Arm::Shift *shift = binary->getShift();
                    if (shift->shiftOpd->isVirtual(dataType)) {
                        shift = new Arm::Shift(shift->shiftType, genOpd(shift->shiftOpd));
                    }
                    prevMI = new I::Binary(binary->getTag(), genOpd(dst), genOpd(l), genOpd(r), shift, prevMI);
                    prevMI->setCond(mi->getCond());
                } else {
                    switch (mi->getTag()) {
                        case MachineInst::Tag::IMov: {
                            I::Mov *iMov = dynamic_cast<I::Mov *>(mi);
                            Arm::Shift *shift = iMov->getShift();
                            if (shift->shiftOpd->isVirtual(dataType)) {
                                shift = new Arm::Shift(shift->shiftType, genOpd(shift->shiftOpd));
                            }
                            prevMI = new I::Mov(genOpd(iMov->getDst()), genOpd(iMov->getSrc()), shift, prevMI);
                            prevMI->setCond(iMov->getCond());
                            break;
                        }
                        case MachineInst::Tag::Ldr: {
                            I::Ldr *ldr = dynamic_cast<I::Ldr *>(mi);
                            Operand *data = ldr->getData();
                            Operand *addr = ldr->getAddr();
                            Operand *offset = ldr->getOffset();
                            Arm::Shift *shift = ldr->MachineInst::getShift();
                            if (shift->shiftOpd->isVirtual(dataType)) {
                                shift = new Arm::Shift(shift->shiftType, genOpd(shift->shiftOpd));
                            }
                            prevMI = new I::Ldr(genOpd(data), genOpd(addr), genOpd(offset), shift, prevMI);
                            prevMI->setCond(mi->getCond());
                            break;
                        }
                    }
                }
                if (mi->isNeedFix()) {
                    if (mi->getCallee() != nullptr) {
                        prevMI->setNeedFix(mi->getCallee(), mi->getFixType());
                    } else {
                        prevMI->setNeedFix(mi->getFixType());
                    }
                }
            }
        }
    }*/
    if (vr != nullptr) {
        int firstPos = -1;
        int lastPos = -1;
        int pos = 0;
        FOR_ITER_MI(mi, curMB) {
            ++pos;
            if (std::find(mi->defOpds.begin(), mi->defOpds.end(), vr) != mi->defOpds.end()
                || std::find(mi->useOpds.begin(), mi->useOpds.end(), vr) != mi->useOpds.end()) {
                if (firstPos == -1) {
                    firstPos = pos;
                }
                lastPos = pos;
            }
        }
        if (firstPos >= 0) {
            (*newVRLiveLength)[vr] = lastPos - firstPos + 1;
        }
    }
    firstUse = nullptr;
    lastDef = nullptr;
    vr = nullptr;
}

void GPRegAllocator::addDefiners(Operand *o) {
    if (o->isVirtual(dataType) && !o->isGlobAddr()) {
        MachineInst *definer = o->getDefMI();
        toInsertMIList.insert(definer);
    }
}

void GPRegAllocator::addDefiners(Operand *o1, Operand *o2) {
    if (o1->isVirtual(dataType) && !o1->isGlobAddr()) {
        MachineInst *definer = o1->getDefMI();
        toInsertMIList.insert(definer);
    }
    if (o2->isVirtual(dataType) && !o2->isGlobAddr()) {
        MachineInst *definer = o2->getDefMI();
        toInsertMIList.insert(definer);
    }
}

Operand *GPRegAllocator::genOpd(Operand *dst) {
    if (dst->isVirtual(dataType) && !dst->isGlobAddr()) {
        if (dst->old != nullptr) dst = dst->old;
        Operand *put = regMap[dst];
        if (put == nullptr) {
            put = vrCopy(dst);
            regMap[dst] = put;
        }
        return put;
    } else {
        return dst;
    }
}

Operand *GPRegAllocator::vrCopy(Operand *oldVR) {
    Operand *copy = curMF->newVR();
    if (oldVR != nullptr && oldVR->isVirtual(dataType) && !oldVR->isGlobAddr()) {
        while (oldVR->old != nullptr) {
            oldVR = oldVR->old;
        }
        copy->old = oldVR;
        copy->setDefCost(oldVR);
    }
    return copy;
}

void GPRegAllocator::assignColors() {
    preAssignColors();
    if (spilledNodeSet->size() > 0) {
        return;
    }
    for (Operand *v: *coalescedNodeSet) {
        Operand *a = getAlias(v);
        colorMap[v] = a->isPreColored(dataType) ? Arm::Reg::getRSReg(a->reg) : colorMap[a];
    }
    std::vector<IMI *> *needFixList = new std::vector<IMI *>();
    FOR_ITER_MB(mb, curMF) {
        FOR_ITER_MI(mi, mb) {
            if (mi->isNeedFix()) {
                needFixList->push_back(dynamic_cast<IMI *>(mi));
            }
            if (mi->isComment()) continue;
            if (mi->isCall()) {
                curMF->setUseLr();
                int idx = 0;
                // std::vector<Operand *> defs = mi->defOpds;
                for (Operand *def: mi->defOpds) {
                    mi->defOpds[idx++] = Arm::Reg::getRSReg(def->getReg());
                }
                idx = 0;
                // std::vector<Operand *> uses = mi->useOpds;
                for (Operand *use: mi->useOpds) {
                    mi->useOpds[idx++] = Arm::Reg::getRSReg(use->getReg());
                }
                continue;
            }
            // std::vector<Operand *> defs = mi->defOpds;
            // std::vector<Operand *> uses = mi->useOpds;
            if (mi->defOpds.size() > 0) {
                Operand *def = mi->defOpds[0];
                if (def->isPreColored(dataType)) {
                    mi->defOpds[0] = Arm::Reg::getRSReg(def->getReg());
                } else {
                    Operand *set = colorMap[def];
                    if (set != nullptr) {
                        curMF->addUsedGPRs(set->reg);

                        mi->defOpds[0] = set;
                    }
                }
            }
            for (int i = 0; i < mi->useOpds.size(); i++) {
                Operand *use = mi->useOpds[i];
                if (use->isPreColored(dataType)) {
                    mi->useOpds[i] = Arm::Reg::getRSReg(use->getReg());
                } else {
                    Operand *set = colorMap[use];
                    if (set != nullptr) {
                        curMF->addUsedGPRs(set->reg);

                        mi->setUse(i, set);
                    }
                }
            }
        }
    }
    curMF->alignTotalStackSize();
    fixStack(needFixList);
}

GPRegAllocator::GPRegAllocator() {
    coalescedMoveSet = new std::set<MachineInst *, CompareMI>();
    constrainedMoveSet = new std::set<MachineInst *, CompareMI>();
    frozenMoveSet = new std::set<MachineInst *, CompareMI>();
    workListMoveSet = new std::set<MachineInst *, CompareMI>();
    activeMoveSet = new std::set<MachineInst *, CompareMI>();
    dataType = DataType::I32;
    K = RK;
    SPILL_MAX_LIVE_INTERVAL = RK * 2;
}

FPRegAllocator::FPRegAllocator() {
    coalescedMoveSet = new std::set<MachineInst *, CompareMI>();
    constrainedMoveSet = new std::set<MachineInst *, CompareMI>();
    frozenMoveSet = new std::set<MachineInst *, CompareMI>();
    workListMoveSet = new std::set<MachineInst *, CompareMI>();
    activeMoveSet = new std::set<MachineInst *, CompareMI>();
    dataType = DataType::F32;
    K = SK;
    if (!CenterControl::_FAST_REG_ALLOCATE) {
        SPILL_MAX_LIVE_INTERVAL = SK;
    }
}

void FPRegAllocator::AllocateRegister(McProgram *mcProgram) {
    FOR_ITER_MF(mf, mcProgram) {
#ifdef LOG
        std::cerr << "[Log] start FPRegAllocator::AllocateRegister:\t" << mf->getName() << std::endl;
#endif
        newVRLiveLength = new std::map<Operand *, int>();
        curMF = mf;
        while (true) {
            turnInit(mf);
            for (int i = 0; i < K; i++) {
                Arm::Reg::getS(i)->degree = MAX_DEGREE;
            }

            for (Arm::Reg *reg: Arm::Reg::fprPool) {
                reg->loopCounter = 0;
                reg->degree = MAX_DEGREE;
                reg->adjOpdSet.clear();
                reg->movSet.clear();
                reg->setAlias(nullptr);
                reg->old = nullptr;
            }
            for (Operand *o: curMF->sVrList) {
                o->loopCounter = 0;
                o->degree = 0;
                o->adjOpdSet.clear();
                o->movSet.clear();
                o->setAlias(nullptr);
                o->old = nullptr;
            }
            build();
#ifdef LOG
            std::cerr << "[Log] after build:\t" << mf->getName() << std::endl;
            std::cerr << mcProgram << std::endl;
#endif
            for (Operand *vr: curMF->sVrList) {
                if (vr->degree >= K) {
                    spillWorkSet->insert(vr);
                } else if (nodeMoves(vr)->size() > 0) {
                    freezeWorkSet->insert(vr);
                } else {
                    simplifyWorkSet->insert(vr);
                }
            }
            regAllocIteration();
#ifdef LOG
            std::cerr << "[Log] after regAllocIteration:\t" << mf->getName() << std::endl;
#endif
            assignColors();
#ifdef LOG
            std::cerr << "[Log] after assignColors:\t" << mf->getName() << std::endl;
#endif
            if (spilledNodeSet->empty()) {
                break;
            }

#ifdef LOG
            std::cerr << "[Log] need spill:\t" << mf->getName() << std::endl;
#endif
            spillMap.clear();
            for (Operand *operand: *spilledNodeSet) {
                dealSpillNode(operand);
            }
            // std::cerr << "[Log] after dealSpillNode:\t" << mf->getName() << std::endl;

        }

    }
}

void FPRegAllocator::dealSpillNode(Operand *x) {
    dealSpillTimes++;
    toStack = CenterControl::_FPRMustToStack;
    FOR_ITER_MB(mb, curMF) {
        curMB = mb;
        offImm = new Operand(DataType::I32, curMF->getVarStack());
        firstUse = nullptr;
        lastDef = nullptr;
        sVr = nullptr;
        int checkCount = 0;
        FOR_ITER_MI(srcMI, mb) {
            if (srcMI->isCall() || srcMI->isComment() || (__IS_NOT__(srcMI, VMI))) continue;
            // std::vector<Operand *> defs = srcMI->defOpds;
            // std::vector<Operand *> uses = srcMI->useOpds;
            if (!srcMI->defOpds.empty()) {
                Operand *def = srcMI->defOpds[0];
                if (def == x) {

                    if (sVr == nullptr) {
                        sVr = vrCopy(x);
                    }
                    srcMI->setDef(sVr);
                    lastDef = srcMI;
                }
            }
            for (int idx = 0; idx < srcMI->useOpds.size(); idx++) {
                Operand *use = srcMI->useOpds[idx];
                if (use == x) {
                    if (sVr == nullptr) {
                        sVr = vrCopy(x);
                    }
                    srcMI->setUse(idx, sVr);
                    if (firstUse == nullptr && (CenterControl::_cutLiveNessShortest || lastDef == nullptr)) {
                        firstUse = srcMI;
                    }
                }
            }
            if (CenterControl::_cutLiveNessShortest || checkCount++ > SPILL_MAX_LIVE_INTERVAL) {
                checkpoint(x);
            }
        }
        checkpoint(x);
    }
    if (toStack) {
        curMF->addVarStack(4);
    }
}

void FPRegAllocator::checkpoint(Operand *x) {
    if (toStack) {
        if (firstUse != nullptr) {
            // V::Ldr *mi = nullptr;
            if (offImm->value < 1024) {
                new V::Ldr(sVr, rSP, offImm, firstUse);
            } else {
                if (CodeGen::immCanCode(offImm->get_I_Imm())) {
                    Operand *dstAddr = curMF->newVR();
                    new I::Binary(MachineInst::Tag::Add, dstAddr, rSP, offImm, firstUse);
                    new V::Ldr(sVr, dstAddr, firstUse);
                } else {
                    Operand *dstAddr = curMF->newVR();
                    new I::Mov(dstAddr, offImm, firstUse);
                    Operand *finalAddr = curMF->newVR();
                    new I::Binary(MachineInst::Tag::Add, finalAddr, rSP, dstAddr, firstUse);
                    new V::Ldr(sVr, finalAddr, firstUse);
                }
            }
        }
        if (lastDef != nullptr) {
            if (offImm->value < 1024) {
                new V::Str(lastDef, sVr, rSP, offImm);
            } else {
                if (CodeGen::immCanCode(offImm->get_I_Imm())) {
                    Operand *dstAddr = curMF->newVR();
                    I::Binary *bino = new I::Binary(lastDef, MachineInst::Tag::Add, dstAddr, rSP, offImm);
                    new V::Str(bino, sVr, dstAddr);
                } else {
                    Operand *dstAddr = curMF->newVR();
                    I::Mov *mv = new I::Mov(lastDef, dstAddr, offImm);
                    Operand *finalAddr = curMF->newVR();
                    I::Binary *bino = new I::Binary(mv, MachineInst::Tag::Add, finalAddr, rSP, dstAddr);
                    new V::Str(bino, sVr, finalAddr);
                }
            }
        }
    } else {
        Operand *gprSpilledTo;
        if (spillMap.count(x)) {
            gprSpilledTo = spillMap[x];
        } else {
            gprSpilledTo = curMF->newVR();
            spillMap[x] = gprSpilledTo;
        }
        if (firstUse != nullptr) {
            // V::Ldr *mi = nullptr;
            new V::Mov(sVr, gprSpilledTo, firstUse);
        }
        if (lastDef != nullptr) {
            new V::Mov(gprSpilledTo, sVr, firstUse);
        }
    }
    if (sVr != nullptr) {
        int firstPos = -1;
        int lastPos = -1;
        int pos = 0;
        FOR_ITER_MI(mi, curMB) {
            ++pos;
            if (std::find(mi->defOpds.begin(), mi->defOpds.end(), sVr) != mi->defOpds.end()
                || std::find(mi->useOpds.begin(), mi->useOpds.end(), sVr) != mi->useOpds.end()) {
                if (firstPos == -1) {
                    firstPos = pos;
                }
                lastPos = pos;
            }
        }
        if (firstPos >= 0) {
            (*newVRLiveLength)[sVr] = lastPos - firstPos + 1;
        }
    }
    firstUse = nullptr;
    lastDef = nullptr;
    sVr = nullptr;
}

Operand *FPRegAllocator::vrCopy(Operand *oldVR) {
    Operand *copy = curMF->newSVR();
    return copy;
}

void FPRegAllocator::assignColors() {
    preAssignColors();
    if (!spilledNodeSet->empty()) {
        return;
    }
    for (Operand *v: *coalescedNodeSet) {
        Operand *a = getAlias(v);
        colorMap[v] = a->isPreColored(dataType) ? Arm::Reg::getRSReg(a->reg) : colorMap[a];
    }
    FOR_ITER_MB(mb, curMF) {
        FOR_ITER_MI(mi, mb) {
            if (mi->isComment()) continue;
            if (mi->isCall()) {
                curMF->setUseLr();
                continue;
            }
            if (__IS_NOT__(mi, VMI)) {
                continue;
            }

            // std::vector<Operand *> defs = mi->defOpds;
            // std::vector<Operand *> uses = mi->useOpds;
            if (!mi->defOpds.empty()) {
                Operand *def = mi->defOpds[0];
                if (def->isPreColored(dataType)) {
                    mi->defOpds[0] = Arm::Reg::getRSReg(def->getReg());
                } else {
                    Operand *set = colorMap[mi->defOpds[0]];
                    if (set != nullptr) {
                        curMF->addUsedFRPs(set->reg);
                        mi->defOpds[0] = set;
                    }
                }
            }
            for (int i = 0; i < mi->useOpds.size(); i++) {
                Operand *use = mi->useOpds[i];
                if (use->isPreColored(dataType)) {
                    mi->useOpds[i] = Arm::Reg::getRSReg(use->getReg());
                } else {
                    Operand *set = colorMap[use];
                    if (set != nullptr) {
                        curMF->addUsedFRPs(set->reg);

                        mi->setUse(i, set);
                    }
                }
            }
        }
    }
}

NaiveRegAllocator::NaiveRegAllocator() {
    initPool();
}

void NaiveRegAllocator::initPool() {
    rRegPool[0] = Arm::Reg::getR(4);
    rRegPool[1] = Arm::Reg::getR(5);
    rRegPool[2] = Arm::Reg::getR(6);
    rRegPool[3] = Arm::Reg::getR(7);
    rRegPool[4] = Arm::Reg::getR(8);
    rRegPool[5] = Arm::Reg::getR(9);
    rRegPool[6] = Arm::Reg::getR(10);
    rRegPool[7] = Arm::Reg::getR(11);
    for (int i = 24; i < 32; i++) {
        sRegPool[i - 24] = Arm::Reg::getS(i);
    }
}

Arm::Reg *NaiveRegAllocator::rRegPop() {
    if (rStackTop <= 0) {
    }
    return rRegPool[--rStackTop];
}

Arm::Reg *NaiveRegAllocator::sRegPop() {
    return sRegPool[--sStackTop];
}

void NaiveRegAllocator::reset() {
    rStackTop = rRegNum;
    sStackTop = sRegNum;
}

void NaiveRegAllocator::AllocateRegister(McProgram *program) {
    FOR_ITER_MF(mf, program) {
        mf->setAllocStack();
        mf->addVarStack(4 * mf->getVRSize());
        mf->addVarStack(4 * mf->getSVRSize());
        FOR_ITER_MB(mb, mf) {
            FOR_ITER_MI(mi, mb) {
                if (mi->isCall()) {
                    mf->setUseLr();
                }
            }
        }
        mf->alignTotalStackSize();
    }
    fixStack(&program->needFixList);
    FOR_ITER_MF(mf, program) {
        int iVrBase = mf->getAllocStack();
        int sVrBase = mf->getVRSize() * 4 + iVrBase;
        FOR_ITER_MB(mb, mf) {
            FOR_ITER_MI(mi, mb) {
                std::vector<Operand *> &defs = mi->defOpds;
                std::vector<Operand *> &uses = mi->useOpds;
                if (mi->isComment() || mi->isCall()) continue;
                /* Caution: It's buggy under the conditionally instruction. Example:
                 *  orrne opd, op1, op2     @ def opd, modify conditionally
                 * 
                 * After fast allocation, a `str` is inserted after the binary inst like:
                 *  orrne opd, op1, op2
                 *  str opd, [sp, #off]
                 * 
                 * Once the conditional flag of binary inst is false, the opd is not really modified, 
                 *  but the `str` inst is unconditionally, which force-updated the opd
                 *  and may cause dirty value written into the stack.
                 * 
                 * To fix it, the `str` following the conditional binary instruction should ALSO be CONDITIONAL!
                 * like:
                 *  orrne, opd, op1, op2    @ eval opd
                 *  strne opd               @ def opd conditionally
                 */
                int useIdx = 0;
                for (Operand *use: uses) {
                    if (use->isVirtual(DataType::I32)) {
                        int offset = iVrBase + 4 * use->value;
                        if (ABS(offset) < 4096) {
                            Arm::Reg *useReg = rRegPop();
                            new I::Ldr(useReg, Arm::Reg::getR(Arm::GPR::sp),
                                       new Operand(DataType::I32, offset), mi);
                            mi->setUse(useIdx, useReg);
                        } else {
                            Arm::Reg *offReg = rRegPop();
                            new I::Mov(offReg, new Operand(DataType::I32, offset), mi);
                            Arm::Reg *useReg = rRegPop();
                            new I::Ldr(useReg, Arm::Reg::getR(Arm::GPR::sp), offReg, mi);
                            mi->setUse(useIdx, useReg);
                        }
                    } else if (use->isVirtual(DataType::F32)) {
                        int offset = sVrBase + 4 * use->value;
                        if (ABS(offset) <= 1020) {
                            Arm::Reg *useReg = sRegPop();
                            new V::Ldr(useReg, Arm::Reg::getR(Arm::GPR::sp),
                                       new Operand(DataType::I32, offset), mi);
                            mi->setUse(useIdx, useReg);
                        } else {
                            Arm::Reg *offReg = rRegPop();
                            new I::Mov(offReg, new Operand(DataType::I32, offset), mi);
                            new I::Binary(MachineInst::Tag::Add, offReg, Arm::Reg::getR(Arm::GPR::sp), offReg,
                                          mi);
                            Arm::Reg *useReg = sRegPop();
                            new V::Ldr(useReg, offReg, mi);
                            mi->setUse(useIdx, useReg);
                        }
                    }
                    useIdx++;
                }
                if (defs.size() > 0) {
                    Operand *def = defs[0];
                    if (def->isVirtual(DataType::I32)) {
                        int offset = iVrBase + 4 * def->value;
                        if (ABS(offset) < 4096) {
                            Arm::Reg *useReg = rRegPop();
                            I::Str *str = new I::Str(mi, useReg, Arm::Reg::getR(Arm::GPR::sp),
                                                     new Operand(DataType::I32, offset));
                            str->setCond(mi->getCond());
                            mi->setDef(useReg);
                        } else {
                            Arm::Reg *offReg = rRegPop();
                            I::Mov *mv = new I::Mov(mi, offReg, new Operand(DataType::I32, offset));
                            Arm::Reg *defReg = rRegPop();
                            I::Str *str = new I::Str(mv, defReg, Arm::Reg::getR(Arm::GPR::sp), offReg);
                            str->setCond(mi->getCond());
                            mi->setDef(defReg);
                        }
                    } else if (def->isVirtual(DataType::F32)) {
                        int offset = sVrBase + 4 * def->value;
                        if (ABS(offset) <= 1020) {
                            Arm::Reg *defReg = sRegPop();
                            V::Str *str = new V::Str(mi, defReg, rSP, new Operand(DataType::I32, offset));
                            str->setCond(mi->getCond());
                            mi->setDef(defReg);
                        } else {
                            Arm::Reg *offReg = rRegPop();
                            I::Mov *mv = new I::Mov(mi, offReg, new Operand(DataType::I32, offset));
                            I::Binary *bino = new I::Binary(mv, MachineInst::Tag::Add, offReg,
                                                            Arm::Reg::getR(Arm::GPR::sp), offReg);
                            Arm::Reg *defReg = sRegPop();
                            V::Str *str = new V::Str(bino, defReg, offReg);
                            str->setCond(mi->getCond());
                            mi->setDef(defReg);
                        }
                    }
                }
                reset();
            }
        }
    }
}
