#include <cmath>
#include "../../include/midend/GVNAndGCM.h"
#include "../../include/util/CenterControl.h"
#include "stl_op.h"






GVNAndGCM::GVNAndGCM(std::vector<Function*>* functions) {
    this->functions = functions;
}

void GVNAndGCM::Run() {
    Init();
    GVN();
    GCM();
}

void GVNAndGCM::Init() {
    for (Function* function: *functions) {
        std::set<Instr*>* pinnedInstr = new std::set<Instr*>();
        for_bb_(function) {
            for_instr_(bb) {
                if (isPinned(instr)) {
                    pinnedInstr->insert(instr);
                }
            }
            (*insertPos)[bb] = (Instr *) bb->end->prev->prev;
        }
        (*pinnedInstrMap)[function] = pinnedInstr;
    }
}

void GVNAndGCM::GCM() {
    schedule_early();
    schedule_late();
    //printBeforeMove();
    move();
}

void GVNAndGCM::GVN() {
    GvnMap->clear();
    GvnCnt->clear();
    do_gvn();
}

void GVNAndGCM::do_gvn() {
    for (Function* function: *functions) {
//        BasicBlock* bb = first_bb_(function);
        BasicBlock* bb = _first<Function, BasicBlock>(function);
        RPOSearch(bb);
    }
}

void GVNAndGCM::RPOSearch(BasicBlock* bb) {
    Instr* alu = bb->getBeginInstr();
    while (alu->next != nullptr) {
        if (dynamic_cast<INSTR::Alu*>(alu) != nullptr && ((INSTR::Alu*) alu)->hasTwoConst()) {
            Constant* value = calc((INSTR::Alu*) alu);

            alu->modifyAllUseThisToUseA(value);
            alu->remove();
        } else if (dynamic_cast<INSTR::FPtosi*>(alu) != nullptr && dynamic_cast<ConstantFloat*>(((INSTR::FPtosi*) alu)->getRVal1()) != nullptr) {
            float val = (float) ((ConstantFloat*) ((INSTR::FPtosi*) alu)->getRVal1())->get_const_val();
            int ret = (int) val;

            alu->modifyAllUseThisToUseA(new ConstantInt(ret));
            alu->remove();
        } else if (dynamic_cast<INSTR::SItofp*>(alu) != nullptr && dynamic_cast<ConstantInt*>(((INSTR::SItofp*) alu)->getRVal1()) != nullptr) {
            int val = (int) ((ConstantInt*) ((INSTR::SItofp*) alu)->getRVal1())->get_const_val();
            float ret = (float) val;

            alu->modifyAllUseThisToUseA(new ConstantFloat(ret));
            alu->remove();
        }
        alu = (Instr*) alu->next;
    }

    std::set<Instr*>* instrs = new std::set<Instr*>();
    for_instr_(bb) {
        if (canGVN(instr)) {
            if (!addInstrToGVN(instr)) {
                instrs->insert(instr);
            }
        }
    }

    for (BasicBlock* next: (*bb->idoms)) {
        RPOSearch(next);
    }

    for (Instr* instr1: *instrs) {
        removeInstrFromGVN(instr1);
    }
}

bool GVNAndGCM::canGVN(Instr* instr) {
//    if (dynamic_cast<INSTR::FPtosi*>(instr) != nullptr || dynamic_cast<INSTR::SItofp*>(instr) != nullptr) {
//        return CenterControl::_OPEN_FTOI_ITOF_GVN;
//    }
    if (_type_in_types<INSTR::FPtosi, INSTR::SItofp>(instr)) {
        return CenterControl::_OPEN_FTOI_ITOF_GVN;
    }
    if (dynamic_cast<INSTR::Call*>(instr) != nullptr) {
        return !((INSTR::Call*) instr)->getFunc()->isExternal && ((INSTR::Call*) instr)->getFunc()->isCanGVN();
    }
    //fptosi sitofp fcmp待加强
//    return dynamic_cast<INSTR::Alu*>(instr) != nullptr ||
//           dynamic_cast<INSTR::Icmp*>(instr) != nullptr ||
//           dynamic_cast<INSTR::GetElementPtr*>(instr) != nullptr;

    return _type_in_types<INSTR::Alu, INSTR::Icmp, INSTR::GetElementPtr>(instr);
}




void GVNAndGCM::schedule_early() {
    for (Function* function: *functions) {
        std::set<Instr *> *pinnedInstr = (*pinnedInstrMap)[function];
        know = new std::set<Instr *>();
//        root = first_bb_(function);
        root = _first<Function, BasicBlock>(function);
        for (Instr *instr: *pinnedInstr) {
            instr->earliestBB = instr->bb;
            know->insert(instr);
        }
        for_bb_(function) {
            for_instr_(bb) {
                if (!(know->find(instr) != know->end())) {
                    schedule_early_instr(instr);
                } else if ((pinnedInstr->find(instr) != pinnedInstr->end())) {
                    for (Value *value: instr->useValueList) {
//                        if (dynamic_cast<Constant *>(value) != nullptr ||
//                            dynamic_cast<BasicBlock *>(value) != nullptr ||
//                            dynamic_cast<GlobalVal *>(value) != nullptr || dynamic_cast<Function *>(value) != nullptr ||
//                            dynamic_cast<Function::Param *>(value) != nullptr) {
//                            continue;
//                        }
                        if (_type_in_types<Constant, BasicBlock,
                                GlobalVal, Function,
                                Function::Param>(value)) {
                            continue;
                        }
                        schedule_early_instr((Instr *) value);
                    }
                }
            }
        }
    }
}

void GVNAndGCM::schedule_early_instr(Instr* instr) {
    if ((know->find(instr) != know->end())) {
        return;
    }
    know->insert(instr);
    instr->earliestBB = root;
    for (Value* X: instr->useValueList) {
        if (dynamic_cast<Instr*>(X) != nullptr) {
            schedule_early_instr((Instr *) X);
            if (instr->earliestBB->domTreeDeep < ((Instr*) X)->earliestBB->domTreeDeep) {
                instr->earliestBB = ((Instr*) X)->earliestBB;
            }
        }
    }
}


void GVNAndGCM::schedule_late() {
    for (Function* function: *functions) {
        std::set<Instr *> *pinnedInstr = (*pinnedInstrMap)[function];
        know = new std::set<Instr *>();
        for (Instr *instr: *pinnedInstr) {
            instr->latestBB = instr->bb;
            know->insert(instr);
        }
        for (Instr *instr: *pinnedInstr) {
            Use *use = instr->getBeginUse();
            while (use->next != nullptr) {
                schedule_late_instr(use->user);
                use = (Use *) use->next;
            }
        }
        for_bb_(function) {
            std::vector<Instr *> *instrs = new std::vector<Instr *>();
//            Instr *instr = last_instr_(bb);
            Instr* instr = _last<BasicBlock, Instr>(bb);
            while (instr->prev != nullptr) {
                instrs->push_back(instr);
                instr = (Instr *) instr->prev;
            }
            for (Instr *instr1: *instrs) {
                if (know->find(instr1) == know->end()) {
                    schedule_late_instr(instr1);
                }
            }
        }
    }
}

void GVNAndGCM::schedule_late_instr(Instr* instr) {
    if ((know->find(instr) != know->end())) {
        return;
    }
    know->insert(instr);
    BasicBlock* lca = nullptr;
    Use* usePos = instr->getBeginUse();
    while (usePos->next != nullptr) {
        Instr* y = usePos->user;
        schedule_late_instr(y);
        BasicBlock* use = y->latestBB;
        if (dynamic_cast<INSTR::Phi*>(y) != nullptr) {
            int j = usePos->idx;
            use = (*y->latestBB->precBBs)[j];
        }
        lca = find_lca(lca, use);
        usePos = (Use*) usePos->next;
    }
    BasicBlock* best = lca;

    while (lca != instr->earliestBB) {
        if (lca->getLoopDep() < best->getLoopDep()) {
            best = lca;
        }
        lca = lca->iDominator;
    }
    if (lca->getLoopDep() < best->getLoopDep()) {
        best = lca;
    }
    instr->latestBB = best;

    if (instr->latestBB != (instr->bb)) {
        instr->delFromNowBB();
        Instr* pos = nullptr;
        pos = findInsertPos(instr, instr->latestBB);
        pos->insert_before(instr);
        instr->bb = instr->latestBB;
    }
}

void GVNAndGCM::move() {
    for (Function* function: *functions) {
        for_bb_(function) {
            std::vector<Instr*>* instrs = new std::vector<Instr*>();
            for_instr_(bb) {
                instrs->push_back(instr);
            }
            for (Instr* i: *instrs) {
                if (i->latestBB != (bb)) {
                    i->delFromNowBB();
                    Instr* pos = findInsertPos(i, i->latestBB);
                    pos->insert_before(i);
                    i->bb = i->latestBB;
                }
            }
        }
    }
}

BasicBlock* GVNAndGCM::find_lca(BasicBlock* a, BasicBlock* b) {
    if (a == nullptr) {
        return b;
    }
    while (a->domTreeDeep > b->domTreeDeep) {
        a = a->iDominator;
    }
    while (b->domTreeDeep > a->domTreeDeep) {
        b = b->iDominator;
    }
    while (a != b) {
        a = a->iDominator;
        b = b->iDominator;
    }
    return a;
}

Instr* GVNAndGCM::findInsertPos(Instr* instr, BasicBlock* bb) {
    auto* users = new std::vector<Value*>();

    for (Use* use = instr->getBeginUse(); use->next != nullptr; use = (Use*) use->next) {
        users->push_back(use->user);
    }
    Instr* later = nullptr;
    for (Instr* pos = bb->getBeginInstr(); pos->next != nullptr; pos = (Instr*) pos->next) {
        if (dynamic_cast<INSTR::Phi*>(pos) != nullptr) {
            continue;
        }
        if ((std::find(users->begin(), users->end(), pos) != users->end())) {
            later = pos;
            break;
        }
    }

    if (later != nullptr) {
        return later;
    }

//    return last_instr_(bb);
    return _last<BasicBlock, Instr>(bb);
}




// TODO:考虑数组变量读写的GCM 指针是SSA形式 但是内存不是
//  考虑移动load,store是否会产生影响
//  移动的上下限是对同一个数组的最近的load/store?
bool GVNAndGCM::isPinned(Instr* instr) {
//    return dynamic_cast<INSTR::Jump*>(instr) != nullptr || dynamic_cast<INSTR::Branch*>(instr) != nullptr ||
//           dynamic_cast<INSTR::Phi*>(instr) != nullptr || dynamic_cast<INSTR::Return*>(instr) != nullptr ||
//           dynamic_cast<INSTR::Store*>(instr) != nullptr || dynamic_cast<INSTR::Load*>(instr) != nullptr ||
//           dynamic_cast<INSTR::Call*>(instr) != nullptr;

    return _type_in_types<INSTR::Jump, INSTR::Branch,
                            INSTR::Phi, INSTR::Return,
                            INSTR::Store, INSTR::Load,
                            INSTR::Call>(instr);
}

void GVNAndGCM::add(std::string str, Instr* instr) {
    if (!(GvnCnt->find(str) != GvnCnt->end())) {
        (*GvnCnt)[str] = 1;
    } else {
        (*GvnCnt)[str] = (*GvnCnt)[str] + 1;
    }
    if (!(GvnMap->find(str) != GvnMap->end())) {
        (*GvnMap)[str] = instr;
    }
}

void GVNAndGCM::remove(std::string str) {
    (*GvnCnt)[str] = (*GvnCnt)[str] - 1;
    if ((*GvnCnt)[str] == 0) {
        GvnMap->erase(str);
    }
}

bool GVNAndGCM::addInstrToGVN(Instr* instr) {
    //进行替换
    bool tag = false;
    if (dynamic_cast<INSTR::FPtosi*>(instr) != nullptr) {
        std::string hash = "FPtosi " + ((INSTR::FPtosi*) instr)->getRVal1()->getName();
        if ((GvnMap->find(hash) != GvnMap->end())) {
            instr->modifyAllUseThisToUseA((*GvnMap)[hash]);
            instr->remove();
            //当进行替换的时候,(cmp相当于有多个Br
            // 需要维护condCnt),
            for (Use* use = (*GvnMap)[hash]->getBeginUse(); use->next != nullptr; use = (Use*) use->next) {
                Instr* user = use->user;
                if (dynamic_cast<INSTR::Branch*>(user) != nullptr) {
                    user->setCondCount((*GvnMap)[hash]->getCondCount());
                }
            }
            return true;
        }
        add(hash, instr);
    } else if (dynamic_cast<INSTR::SItofp*>(instr) != nullptr) {
        std::string hash = "SItofp " + ((INSTR::SItofp*) instr)->getRVal1()->getName();
        if ((GvnMap->find(hash) != GvnMap->end())) {
            instr->modifyAllUseThisToUseA((*GvnMap)[hash]);
            instr->remove();
            //当进行替换的时候,(cmp相当于有多个Br
            // 需要维护condCnt),
            for (Use* use = (*GvnMap)[hash]->getBeginUse(); use->next != nullptr; use = (Use*) use->next) {
                Instr* user = use->user;
                if (dynamic_cast<INSTR::Branch*>(user) != nullptr) {
                    user->setCondCount((*GvnMap)[hash]->getCondCount());
                }
            }
            return true;
        }
        add(hash, instr);
    } else if (dynamic_cast<INSTR::Icmp*>(instr) != nullptr) {
        std::string hash = INSTR::get_icmp_op_name(((INSTR::Icmp *) instr)->op);
        hash += " " + ((INSTR::Icmp*) instr)->getRVal1()->getName() + ", " + ((INSTR::Icmp*) instr)->getRVal2()->getName();
        if ((GvnMap->find(hash) != GvnMap->end())) {
            instr->modifyAllUseThisToUseA((*GvnMap)[hash]);
            instr->remove();
            //当进行替换的时候,(cmp相当于有多个Br
            // 需要维护condCnt),
            for (Use* use = (*GvnMap)[hash]->getBeginUse(); use->next != nullptr; use = (Use*) use->next) {
                Instr* user = use->user;
                if (dynamic_cast<INSTR::Branch*>(user) != nullptr) {
                    user->setCondCount((*GvnMap)[hash]->getCondCount());
                }
            }
            return true;
        }
        add(hash, instr);
    } else if (dynamic_cast<INSTR::Call*>(instr) != nullptr) {
        std::string hash = ((INSTR::Call*) instr)->getFunc()->name + "(";
        std::vector<Value*>* params = ((INSTR::Call*) instr)->getParamList();
        for (int i = 0; i < params->size(); i++) {
            hash += (*params)[i]->getName();
            if (i < params->size() - 1) {
                hash +=  ", ";
            }
        }
        hash += ")";
        if ((GvnMap->find(hash) != GvnMap->end())) {
            instr->modifyAllUseThisToUseA((*GvnMap)[hash]);
            instr->remove();
            return true;
        }
        add(hash, instr);
    } else if (dynamic_cast<INSTR::GetElementPtr*>(instr) != nullptr) {
        std::string hash = ((INSTR::GetElementPtr*) instr)->getPtr()->getName();
        std::vector<Value*>* indexs = ((INSTR::GetElementPtr*) instr)->getIdxList();
        for (int i = 0; i < indexs->size(); i++) {
            hash = hash + "[" + (*indexs)[i]->getName() + "]";
        }
        if ((GvnMap->find(hash) != GvnMap->end())) {
            instr->modifyAllUseThisToUseA((*GvnMap)[hash]);
            instr->remove();
            return true;
        }
        add(hash, instr);
    } else if (dynamic_cast<INSTR::Alu*>(instr) != nullptr) {
        std::string hash = instr->useValueList[0]->getName() +
                INSTR::get_alu_op_name(((INSTR::Alu *) instr)->op) + instr->useValueList[1]->getName();
        if ((GvnMap->find(hash) != GvnMap->end())) {
            instr->modifyAllUseThisToUseA((*GvnMap)[hash]);
            instr->remove();
            return true;
        }
        if ((((INSTR::Alu*) instr)->op == INSTR::Alu::Op::ADD) || (((INSTR::Alu*) instr)->op == INSTR::Alu::Op::MUL) ||
        (((INSTR::Alu*) instr)->op == INSTR::Alu::Op::FADD) ||( ((INSTR::Alu*) instr)->op == INSTR::Alu::Op::FMUL)) {
            std::string str = instr->useValueList[0]->getName() + INSTR::get_alu_op_name(((INSTR::Alu*) instr)->op) + instr->useValueList[1]->getName();
            add(str, instr);
            if (instr->useValueList[0]->getName() != instr->useValueList[1]->getName()) {
                str = instr->useValueList[1]->getName() + INSTR::get_alu_op_name(((INSTR::Alu*) instr)->op) + instr->useValueList[0]->getName();
                add(str, instr);
            }
        } else {
            std::string str = instr->useValueList[0]->getName() + INSTR::get_alu_op_name(((INSTR::Alu*) instr)->op) + instr->useValueList[1]->getName();
            add(str, instr);
        }
    }
    return tag;
}

void GVNAndGCM::removeInstrFromGVN(Instr* instr) {
    if (dynamic_cast<INSTR::FPtosi*>(instr) != nullptr) {
        std::string hash = "FPtosi " + ((INSTR::FPtosi*) instr)->getRVal1()->getName();
        remove(hash);
    } else if (dynamic_cast<INSTR::SItofp*>(instr) != nullptr) {
        std::string hash = "SItofp " + ((INSTR::SItofp*) instr)->getRVal1()->getName();
        remove(hash);
    } else if (dynamic_cast<INSTR::Icmp*>(instr) != nullptr) {
        std::string hash = INSTR::get_icmp_op_name(((INSTR::Icmp*) instr)->op);
        hash += " " + ((INSTR::Icmp*) instr)->getRVal1()->getName() + ", " + ((INSTR::Icmp*) instr)->getRVal2()->getName();
        remove(hash);
    } else if (dynamic_cast<INSTR::Call*>(instr) != nullptr) {
        std::string hash = ((INSTR::Call*) instr)->getFunc()->getName() + "(";
        std::vector<Value*>* params = ((INSTR::Call*) instr)->getParamList();
        for (int i = 0; i < params->size(); i++) {
            hash += (*params)[i]->getName();
            if (i < params->size() - 1) {
                hash +=  ", ";
            }
        }
        hash += ")";
        remove(hash);
    } else if (dynamic_cast<INSTR::GetElementPtr*>(instr) != nullptr) {
        std::string hash = ((INSTR::GetElementPtr*) instr)->getPtr()->getName();
        std::vector<Value*>* indexs = ((INSTR::GetElementPtr*) instr)->getIdxList();
        for (int i = 0; i < indexs->size(); i++) {
            hash = hash + "[" + (*indexs)[i]->getName() + "]";
        }
        remove(hash);
    } else if (dynamic_cast<INSTR::Alu*>(instr) != nullptr) {
        if ((((INSTR::Alu*) instr)->op == INSTR::Alu::Op::ADD) || (((INSTR::Alu*) instr)->op == INSTR::Alu::Op::MUL) ||
            (((INSTR::Alu*) instr)->op == INSTR::Alu::Op::FADD) ||( ((INSTR::Alu*) instr)->op == INSTR::Alu::Op::FMUL)) {
            std::string str = instr->useValueList[0]->getName() + INSTR::get_alu_op_name(((INSTR::Alu*) instr)->op) + instr->useValueList[1]->getName();
            remove(str);
            if (instr->useValueList[0]->getName() != instr->useValueList[1]->getName()) {
                str = instr->useValueList[1]->getName() + INSTR::get_alu_op_name(((INSTR::Alu*) instr)->op) + instr->useValueList[0]->getName();
                remove(str);
            }
        } else {
            std::string str = instr->useValueList[0]->getName() + INSTR::get_alu_op_name(((INSTR::Alu*) instr)->op) + instr->useValueList[1]->getName();
            remove(str);
        }
    }
}

Constant* GVNAndGCM::calc(INSTR::Alu* alu) {
    //assert alu->hasTwoConst();
    if (alu->type->is_int32_type()) {
        INSTR::Alu::Op op = alu->op;
        int ConstA = (int) ((ConstantInt*) alu->useValueList[0])->get_const_val();
        int ConstB = (int) ((ConstantInt*) alu->useValueList[1])->get_const_val();
        ConstantInt* value = nullptr;
        if ((op == INSTR::Alu::Op::ADD)) {
            value = new ConstantInt(ConstA + ConstB);
        } else if ((op == INSTR::Alu::Op::SUB)) {
            value = new ConstantInt(ConstA - ConstB);
        } else if ((op == INSTR::Alu::Op::MUL)) {
            value = new ConstantInt(ConstA * ConstB);
        } else if ((op == INSTR::Alu::Op::DIV)) {
            if (ConstB == 0) {
                ConstB = 1;
            }
            value = new ConstantInt(ConstA / ConstB);
        } else if ((op == INSTR::Alu::Op::REM)) {
            if (ConstB == 0) {
                ConstB = 1;
            }
            value = new ConstantInt(ConstA % ConstB);
        } else {
            //System->err.println("err_1");
        }

        return value;

    } else {
        INSTR::Alu::Op op = alu->op;
        float ConstA = (float) ((ConstantFloat*) alu->useValueList[0])->get_const_val();
        float ConstB = (float) ((ConstantFloat*) alu->useValueList[1])->get_const_val();
        ConstantFloat* value = nullptr;
        if ((op == INSTR::Alu::Op::FADD)) {
            value = new ConstantFloat(ConstA + ConstB);
        } else if ((op == INSTR::Alu::Op::FSUB)) {
            value = new ConstantFloat(ConstA - ConstB);
        } else if ((op == INSTR::Alu::Op::FMUL)) {
            value = new ConstantFloat(ConstA * ConstB);
        } else if ((op == INSTR::Alu::Op::FDIV)) {
            value = new ConstantFloat(ConstA / ConstB);
        } else if ((op == INSTR::Alu::Op::FREM)) {
            value = new ConstantFloat(std::fmod(ConstA, ConstB));
        }

        return value;
    }
}

