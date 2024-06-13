//
// Created by start on 23-7-29.
//
#include "../../include/midend/LoopFuse.h"
#include "../../include/util/CenterControl.h"

#include "stl_op.h"




LoopFuse::LoopFuse(std::vector<Function*>* functions) {
    this->functions = functions;
}

void LoopFuse::Run() {
    init();
    loopFuse();
}

void LoopFuse::init() {
    for (Function* function: *functions) {
        for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
            if (bb->isLoopHeader) {
                loops->insert(bb->loop);
            }
        }
    }
}

void LoopFuse::loopFuse() {
    for (Loop* loop: *loops) {
        tryFuseLoop(loop);
    }
}

void LoopFuse::tryFuseLoop(Loop* preLoop) {
    if (!preLoop->isSimpleLoop() || !preLoop->isIdcSet()) {
        return;
    }
    BasicBlock* preExit = nullptr;
    for (BasicBlock* bb: *preLoop->getExits()) {
        preExit = bb;
    }
    if (preExit->succBBs->size() != 1) {
        return;
    }
    if (!(*preExit->succBBs)[0]->isLoopHeader) {
        return;
    }
    Loop* sucLoop = (*preExit->succBBs)[0]->loop;
    if (!sucLoop->isSimpleLoop() || !sucLoop->isIdcSet()) {
        return;
    }
    if (!(preLoop->getIdcInit() == sucLoop->getIdcInit()) ||
        !(preLoop->getIdcEnd() == sucLoop->getIdcEnd())) {
        return;
    }
    if (!(preLoop->getIdcStep() == sucLoop->getIdcStep())) {
        Value* preStep = preLoop->getIdcStep();
        Value* sucStep = sucLoop->getIdcStep();
        if (dynamic_cast<ConstantInt*>(preStep) != nullptr && dynamic_cast<ConstantInt*>(sucStep) != nullptr) {
            int preStepVal = (int) ((ConstantInt*) preStep)->get_const_val();
            int sucStepVal = (int) ((ConstantInt*) sucStep)->get_const_val();
            if (preStepVal != sucStepVal) {
                return;
            }
        } else {
            return;
        }
    }

    if (preLoop->getNowLevelBB()->size() > 2 || sucLoop->getNowLevelBB()->size() > 2) {
        return;
    }
    if (preLoop->getEnterings()->size() != 1 || sucLoop->getEnterings()->size() != 1) {
        return;
    }

    BasicBlock *preLatch =  *preLoop->getLatchs()->begin(),
            *sucLatch = *sucLoop->getLatchs()->begin(),
            *preHead = preLoop->getHeader(),
            *sucHead = sucLoop->getHeader(),
            *preEntering = *preLoop->getEnterings()->begin(),
            *sucEntering = *sucLoop->getEnterings()->begin();

    std::set<Instr*>* preIdcInstrs = new std::set<Instr*>(), *sucIdcInstrs = new std::set<Instr*>();
    std::unordered_map<Value*, Value*>* map = new std::unordered_map<Value*, Value*>();
    preIdcInstrs->insert((Instr*) preLoop->getIdcPHI());
    preIdcInstrs->insert((Instr*) preLoop->getIdcAlu());
    preIdcInstrs->insert((Instr*) preLoop->getIdcCmp());
    preIdcInstrs->insert(preHead->getEndInstr());
    preIdcInstrs->insert(preLatch->getEndInstr());

    sucIdcInstrs->insert((Instr*) sucLoop->getIdcPHI());
    sucIdcInstrs->insert((Instr*) sucLoop->getIdcAlu());
    sucIdcInstrs->insert((Instr*) sucLoop->getIdcCmp());
    sucIdcInstrs->insert(sucHead->getEndInstr());
    sucIdcInstrs->insert(sucLatch->getEndInstr());

    if (((INSTR::Icmp*) preLoop->getIdcCmp())->op != (((INSTR::Icmp*) sucLoop->getIdcCmp())->op)) {
        return;
    }
    if (preLatch->getEndInstr()->prev != preLoop->getIdcAlu()) {
        return;
    }


    for (Instr* instr: *preIdcInstrs) {
        if (useOutLoop(instr, preLoop)) {
            return;
        }
    }
    for (Instr* instr: *sucIdcInstrs) {
        if (useOutLoop(instr, sucLoop)) {
            return;
        }
    }

    (*map)[preLoop->getIdcPHI()] = sucLoop->getIdcPHI();
    (*map)[preLoop->getIdcAlu()] = sucLoop->getIdcAlu();
    (*map)[preLoop->getIdcCmp()] = sucLoop->getIdcCmp();
    (*map)[preHead] = sucHead;
    (*map)[preLatch] = sucLatch;


    for (Instr* instr = preHead->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
        if (!(preIdcInstrs->find(instr) != preIdcInstrs->end())) {
            return;
        }
    }
    for (Instr* instr = sucHead->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
        if (!(sucIdcInstrs->find(instr) != sucIdcInstrs->end())) {
            return;
        }
    }
    std::set<Value*> *preLoad = new std::set<Value*>(), *preStore = new std::set<Value*>();
    std::unordered_map<Value*, Instr*> *preLoadGep = new std::unordered_map<Value*, Instr*>(), *preStoreGep = new std::unordered_map<Value*, Instr*>();
    std::set<Value*> *sucLoad = new std::set<Value*>(), *sucStore = new std::set<Value*>();
    std::unordered_map<Value*, Instr*> *sucLoadGep = new std::unordered_map<Value*, Instr*>(), *sucStoreGep = new std::unordered_map<Value*, Instr*>();
    std::set<Instr*>* preLatchInstrs = new std::set<Instr*>();
    std::set<Instr*>* sucLatchInstrs = new std::set<Instr*>();
    for (Instr* instr = preLatch->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
        preLatchInstrs->insert(instr);
    }
    for (Instr* instr = sucLatch->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
        sucLatchInstrs->insert(instr);
    }

    preLatchInstrs->erase((Instr*) preLoop->getIdcAlu());
    sucLatchInstrs->erase((Instr*) sucLoop->getIdcAlu());

    //TODO:检查指令对应,判断读写数组
    for (Instr* instr: *preLatchInstrs) {
        if (useOutLoop(instr, preLoop)) {
            return;
        }
        if (dynamic_cast<INSTR::Jump*>(instr) != nullptr) {
            //nothing
        } else if (dynamic_cast<INSTR::Load*>(instr) != nullptr) {
            Value* array = ((INSTR::Load*) instr)->getPointer();
            while (dynamic_cast<INSTR::GetElementPtr*>(array) != nullptr) {
                array = ((INSTR::GetElementPtr*) array)->getPtr();
            }
            preLoad->insert(array);
        } else if (dynamic_cast<INSTR::Store*>(instr) != nullptr) {
            Value* storeValue = ((INSTR::Store*) instr)->getValue();
            if (dynamic_cast<Instr*>(storeValue) != nullptr && !(((Instr*) storeValue)->bb == preLatch)) {
                return;
            }
            Value* array = ((INSTR::Store*) instr)->getPointer();
            while (dynamic_cast<INSTR::GetElementPtr*>(array) != nullptr) {
                array = ((INSTR::GetElementPtr*) array)->getPtr();
            }
            preStore->insert(array);
        } else if (dynamic_cast<INSTR::Alu*>(instr) != nullptr) {
            Value* val1 = ((INSTR::Alu*) instr)->getRVal1();
            Value* val2 = ((INSTR::Alu*) instr)->getRVal2();
            if (dynamic_cast<Instr*>(val1) != nullptr && !(((Instr*) val1)->bb == preLatch)) {
                return;
            }
            if (dynamic_cast<Instr*>(val2) != nullptr && !(((Instr*) val2)->bb == preLatch)) {
                return;
            }
        } else if (dynamic_cast<INSTR::GetElementPtr*>(instr) != nullptr) {
            Value* array = ((INSTR::GetElementPtr*) instr)->getPtr();
            while (dynamic_cast<INSTR::GetElementPtr*>(array) != nullptr) {
                array = ((INSTR::GetElementPtr*) array)->getPtr();
            }
            for (Use* use = instr->getBeginUse(); use->next != nullptr; use = (Use*) use->next) {
                if (dynamic_cast<INSTR::Load*>(use->user) != nullptr) {
                    if ((preLoadGep->find(array) != preLoadGep->end())) {
                        return;
                    }
                    (*preLoadGep)[array] = instr;
                } else if (dynamic_cast<INSTR::Store*>(use->user) != nullptr) {
                    if ((preStoreGep->find(array) != preStoreGep->end())) {
                        return;
                    }
                    (*preStoreGep)[array] = instr;
                } else {
                    return;
                }
            }
        } else {
            return;
        }
    }
    for (Instr* instr: *sucLatchInstrs) {
        if (useOutLoop(instr, sucLoop)) {
            return;
        }
        if (dynamic_cast<INSTR::Jump*>(instr) != nullptr) {
            //nothing
        } else if (dynamic_cast<INSTR::Load*>(instr) != nullptr) {
            Value* array = ((INSTR::Load*) instr)->getPointer();
            while (dynamic_cast<INSTR::GetElementPtr*>(array) != nullptr) {
                array = ((INSTR::GetElementPtr*) array)->getPtr();
            }
            sucLoad->insert(array);
        } else if (dynamic_cast<INSTR::Store*>(instr) != nullptr) {
            Value* storeValue = ((INSTR::Store*) instr)->getValue();
            if (dynamic_cast<Instr*>(storeValue) != nullptr && !(((Instr*) storeValue)->bb == sucLatch)) {
                return;
            }
            Value* array = ((INSTR::Store*) instr)->getPointer();
            while (dynamic_cast<INSTR::GetElementPtr*>(array) != nullptr) {
                array = ((INSTR::GetElementPtr*) array)->getPtr();
            }
            sucStore->insert(array);
        } else if (dynamic_cast<INSTR::Alu*>(instr) != nullptr) {
            Value* val1 = ((INSTR::Alu*) instr)->getRVal1();
            Value* val2 = ((INSTR::Alu*) instr)->getRVal2();
            if (dynamic_cast<Instr*>(val1) != nullptr &&
                (((Instr*) val1)->bb->loop == preLoop)) {
                return;
            }
            if (dynamic_cast<Instr*>(val2) != nullptr &&
                (((Instr*) val2)->bb->loop == preLoop)) {
                return;
            }
        } else if (dynamic_cast<INSTR::GetElementPtr*>(instr) != nullptr) {
            Value* array = ((INSTR::GetElementPtr*) instr)->getPtr();
            while (dynamic_cast<INSTR::GetElementPtr*>(array) != nullptr) {
                array = ((INSTR::GetElementPtr*) array)->getPtr();
            }
            for (Use* use = instr->getBeginUse(); use->next != nullptr; use = (Use*) use->next) {
                if (dynamic_cast<INSTR::Load*>(use->user) != nullptr) {
                    if ((sucLoadGep->find(array) != sucLoadGep->end())) {
                        return;
                    }
                    (*sucLoadGep)[array] = instr;
                } else if (dynamic_cast<INSTR::Store*>(use->user) != nullptr) {
                    if ((sucStoreGep->find(array) != sucStoreGep->end())) {
                        return;
                    }
                    (*sucStoreGep)[array] = instr;
                } else {
                    return;
                }
            }
        } else {
            return;
        }
    }

    //!(preLoad == sucLoad)这个条件并不需要?
//    preStore != (sucStore)
    for (Value* v: *preStore) {
        if (!_contains_(sucStore, v)) {
            return;
        }
    }
    for (Value* v: *sucStore) {
        if (!_contains_(preStore, v)) {
            return;
        }
    }
    if (preStore->size() != 1 || sucStore->size() != 1) {
        return;
    }
    Value* preStoreArray = *preStore->begin();
    Value* sucStoreArray = *sucStore->begin();
    Value* preStoreIndex = (*preStoreGep)[preStoreArray];
    Value* sucStoreIndex = (*sucStoreGep)[sucStoreArray];

    //fixme:仔细考虑并不是一个数组的时候
    if (preStoreArray != (sucStoreArray)) {
        return;
    }

    if ((preLoadGep->find(preStoreArray) != preLoadGep->end()) && preStoreIndex != ((*preLoadGep)[preStoreArray])) {
        return;
    }

    if ((sucLoadGep->find(sucStoreArray) != sucLoadGep->end()) && sucStoreIndex != ((*sucLoadGep)[sucStoreArray])) {
        return;
    }
    bool change = true;
    while (change) {
        change = false;
        int size = map->size();
        for (Instr* instr: *preLatchInstrs) {
            hasReflectInstr(map, instr, sucLatchInstrs);
        }
        if (map->size() > size) {
            change = true;
        }
    }



    //sucEntering 没有对preStoreArray 的读写
    for (Instr* instr = sucEntering->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
        if (dynamic_cast<INSTR::Load*>(instr) != nullptr) {
            Value* array = ((INSTR::Load*) instr)->getPointer();
            while (dynamic_cast<INSTR::GetElementPtr*>(array) != nullptr) {
                array = ((INSTR::GetElementPtr*) array)->getPtr();
            }
            if ((array == preStoreArray)) {
                return;
            }
        } else if (dynamic_cast<INSTR::Store*>(instr) != nullptr) {
            Value* array = ((INSTR::Store*) instr)->getPointer();
            while (dynamic_cast<INSTR::GetElementPtr*>(array) != nullptr) {
                array = ((INSTR::GetElementPtr*) array)->getPtr();
            }
            if ((array == preStoreArray)) {
                return;
            }
        }
    }


    if ((sucLoad->find(preStoreArray) != sucLoad->end())) {
        if (getReflectValue(preStoreIndex, map) != ((*sucLoadGep)[preStoreArray])) {
            return;
        }
    }

    std::vector<Instr*>* temp = new std::vector<Instr*>();
    for (Instr* instr = sucEntering->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
        if (!(dynamic_cast<INSTR::Branch*>(instr) != nullptr) && !(dynamic_cast<INSTR::Jump*>(instr) != nullptr)) {
            temp->push_back(instr);
        }
    }
    for (int i = 0; i < temp->size(); i++) {
        Instr* instr = (*temp)[i];
        instr->delFromNowBB();
        preEntering->getEndInstr()->insert_before(instr);
        instr->bb = preEntering;
    }
    temp->clear();
    for (Instr* instr = sucLatch->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
        if (!(dynamic_cast<INSTR::Branch*>(instr) != nullptr) && !(dynamic_cast<INSTR::Jump*>(instr) != nullptr) && instr != (sucLoop->getIdcAlu())) {
            temp->push_back(instr);
        }
    }
    for (int i = 0; i < temp->size(); i++) {
        Instr* instr = (*temp)[i];
        instr->delFromNowBB();
        preLatch->getEndInstr()->prev->insert_before(instr);
        instr->bb = preLatch;

        for (int index = 0; index < instr->useValueList.size(); index++) {
            Value* value = instr->useValueList[index];
            if ((value == sucLoop->getIdcPHI())) {
                instr->modifyUse(preLoop->getIdcPHI(), index);
            }
        }
    }
}

bool LoopFuse::hasReflectInstr(std::unordered_map<Value*, Value*>* map, Instr* instr, std::set<Instr*>* instrs) {
    for (Instr* instr1: *instrs) {
        if (check(instr, instr1, map)) {
            return true;
        }
    }
    return false;
}

bool LoopFuse::check(Instr* A, Instr* B, std::unordered_map<Value*, Value*>* map) {
    if (A->tag != B->tag) {
        return false;
    }
    if (dynamic_cast<INSTR::Icmp*>(A) != nullptr) {
        INSTR::Icmp* icmpA = (INSTR::Icmp*) A;
        INSTR::Icmp* icmpB = (INSTR::Icmp*) B;
        if (icmpB->op != (icmpA->op) ||
            !(icmpB->getRVal1() == getReflectValue(icmpA->getRVal1(), map)) ||
            !(icmpB->getRVal2() == getReflectValue(icmpA->getRVal2(), map))) {
            return false;
        }
    } else if (dynamic_cast<INSTR::Load*>(A) != nullptr) {
        INSTR::Load* loadA = (INSTR::Load*) A;
        INSTR::Load* loadB = (INSTR::Load*) B;
        if (!(loadB->getPointer() == getReflectValue(loadA->getPointer(), map))) {
            return false;
        }
    } else if (dynamic_cast<INSTR::Store*>(A) != nullptr) {
        INSTR::Store* storeA = (INSTR::Store*) A;
        INSTR::Store* storeB = (INSTR::Store*) B;
        if (!(storeB->getPointer() == getReflectValue(storeA->getPointer(), map)) ||
            !(storeB->getValue() == getReflectValue(storeA->getValue(), map))) {
            return false;
        }
    } else if (dynamic_cast<INSTR::GetElementPtr*>(A) != nullptr) {
        if (A->useValueList.size() != 3 || B->useValueList.size() != 3) {
            return false;
        }
        INSTR::GetElementPtr* gepA = (INSTR::GetElementPtr*) A;
        INSTR::GetElementPtr* gepB = (INSTR::GetElementPtr*) B;
        if (gepB->useValueList[0] != (getReflectValue(gepA->useValueList[0], map)) ||
            gepB->useValueList[1] != (getReflectValue(gepA->useValueList[1], map)) ||
            gepB->useValueList[2] != (getReflectValue(gepA->useValueList[2], map))) {
            return false;
        }
    } else if (dynamic_cast<INSTR::Alu*>(A) != nullptr) {
        INSTR::Alu* aluA = (INSTR::Alu*) A;
        INSTR::Alu* aluB = (INSTR::Alu*) B;
        if (aluB->op != (aluA->op) ||
            !(aluB->getRVal1() == getReflectValue(aluA->getRVal1(), map)) ||
            !(aluB->getRVal2() == getReflectValue(aluA->getRVal2(), map))) {
            return false;
        }
    } else {
        return false;
    }
    //添加映射
    (*map)[A] = B;
    return true;
}

Value* LoopFuse::getReflectValue(Value* value, std::unordered_map<Value*, Value*>* map) {
    if (!(map->find(value) != map->end())) {
        return value;
    }
    return (*map)[value];
}

bool LoopFuse::useOutLoop(Instr* instr, Loop* loop) {
    for (Use* use = instr->getBeginUse(); use->next != nullptr; use = (Use*) use->next) {
        Instr* user = use->user;
        if (!(user->bb->loop == loop)) {
            return true;
        }
    }
    return false;
}

