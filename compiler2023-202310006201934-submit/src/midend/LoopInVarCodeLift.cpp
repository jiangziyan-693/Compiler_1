//
// Created by start on 23-7-29.
//

#include "../../include/midend/LoopInVarCodeLift.h"
#include "../../include/util/CenterControl.h"
#include "HashMap.h"
//



//TODO:load外提 -- spmv




LoopInVarCodeLift::LoopInVarCodeLift(std::vector<Function*>* functions, std::unordered_map<GlobalValue*, Initial*>* globalValues) {
    this->functions = functions;
    this->globalValues = globalValues;
}

void LoopInVarCodeLift::Run() {
    init();
    arrayConstDefLift();
    init();
    loopInVarLoadLift();

}

void LoopInVarCodeLift::init() {
    loadCanGCM->clear();
    allocDefs->clear();
    allocUsers->clear();
    defs->clear();
    users->clear();
    defLoops->clear();
    useLoops->clear();
}

void LoopInVarCodeLift::arrayConstDefLift() {
    for (Function* function: *functions) {
        arrayConstDefLiftForFunc(function);
    }
}

//alloc的所有def都在一个块,且为常数,且该块中的load均在store之后
void LoopInVarCodeLift::arrayConstDefLiftForFunc(Function* function) {
    //init
    for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
        for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
            if (dynamic_cast<INSTR::Alloc*>(instr) != nullptr) {
                //TODO:待加强,当前只考虑int数组
                for (Use* use = instr->getBeginUse(); use->next != nullptr; use = (Use*) use->next) {
                    DFS((INSTR::Alloc*) instr, use->user);
                }
            }
        }
    }
    //
    for (INSTR::Alloc* alloc: KeySet(*allocDefs)) {
        tryLift(alloc);
    }
}

void LoopInVarCodeLift::tryLift(INSTR::Alloc* alloc) {
    std::set<Instr*>* defForThisAlloc = (*allocDefs)[alloc];
    BasicBlock* bb = nullptr;
    for (Instr* def: *defForThisAlloc) {
        if (bb == nullptr) {
            bb = def->bb;
        } else {
            if (bb != (def->bb)) {
                return;
            }
        }
    }

    //所有def都在一个bb内
    bool tag = false;
    for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
        if ((*allocUsers)[alloc]->find(instr) != (*allocUsers)[alloc]->end()) {
            tag = true;
        }
        //在use之后def
        if (tag && (defForThisAlloc->find(instr) != defForThisAlloc->end())) {
            return;
        }
    }

    //def 都是常数
    for (Instr* def: *defForThisAlloc) {
        if (dynamic_cast<INSTR::Call*>(def) != nullptr) {
            if (((INSTR::Call*) def)->getFunc()->getName() == ("memset")) {
                if (dynamic_cast<ConstantInt*>((*((INSTR::Call*) def)->getParamList())[1]) != nullptr &&
                        dynamic_cast<ConstantInt*>((*((INSTR::Call*) def)->getParamList())[2]) != nullptr ) {
                    int val = (int) ((ConstantInt*) (*((INSTR::Call*) def)->getParamList())[1])->get_const_val();
                    if (val != 0) {
                        return;
                    }
                } else {
                    return;
                }
            } else {
                return;
            }
        } else {
            //assert dynamic_cast<INSTR::Store*>(def) != nullptr;
            Value* ptr = ((INSTR::Store*) def)->getPointer();
            if (dynamic_cast<ConstantInt *>(((INSTR::Store *) def)->getValue()) == nullptr) {
                return;
            }
            if (dynamic_cast<INSTR::GetElementPtr*>(ptr) != nullptr) {
                for (Value* index: *((INSTR::GetElementPtr*) ptr)->getIdxList()) {
                    if (dynamic_cast<ConstantInt *>(index) == nullptr) {
                        return;
                    }
                }
            } else {
                return;
            }
        }
    }

    //do lift
    if (bb->loop->getLoopDepth() == 0) {
        return;
    }
    Loop* loop = bb->loop;
    //TODO:多入口也可以?
    if (loop->getEnterings()->size() > 1) {
        return;
    }
    std::vector<Instr*>* move = new std::vector<Instr*>();
    for (BasicBlock* entering: *loop->getEnterings()) {
        move->clear();
        for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
            if ((defForThisAlloc->find(instr) != defForThisAlloc->end())) {
                move->push_back(instr);
            }
        }
        for (Instr* instr: *move) {
            instr->delFromNowBB();
            entering->getEndInstr()->insert_before(instr);
            instr->bb = entering;
            //instr->latestBB = entering;
        }
    }

}


void LoopInVarCodeLift::DFS(INSTR::Alloc* alloc, Instr* instr) {
    if (dynamic_cast<INSTR::GetElementPtr*>(instr) != nullptr) {
        for (Use* use = instr->getBeginUse(); use->next != nullptr; use = (Use*) use->next) {
            Instr* user = use->user;
            DFS(alloc, user);
        }
    } else if (dynamic_cast<INSTR::Store*>(instr) != nullptr) {
        if (!(allocDefs->find(alloc) != allocDefs->end())) {
            (*allocDefs)[alloc] = new std::set<Instr*>();
        }
        (*allocDefs)[alloc]->insert(instr);
    } else if (dynamic_cast<INSTR::Load*>(instr) != nullptr) {
        if (!(allocUsers->find(alloc) != allocUsers->end())) {
            (*allocUsers)[alloc] = new std::set<Instr*>();
        }
        (*allocUsers)[alloc]->insert(instr);
    } else if (dynamic_cast<INSTR::Call*>(instr) != nullptr) {
        //这两个函数的正确执行,不依赖于数组的原始值,所以只被认为是def
        if (((INSTR::Call*) instr)->getFunc()->getName() == ("memset") ||
            ((INSTR::Call*) instr)->getFunc()->getName() == ("getarray")) {
            if (!(allocDefs->find(alloc) != allocDefs->end())) {
                (*allocDefs)[alloc] = new std::set<Instr*>();
            }
            (*allocDefs)[alloc]->insert(instr);
        } else {
            if (!(allocUsers->find(alloc) != allocUsers->end())) {
                (*allocUsers)[alloc] = new std::set<Instr*>();
            }
            if (!(allocDefs->find(alloc) != allocDefs->end())) {
                (*allocDefs)[alloc] = new std::set<Instr*>();
            }
            (*allocUsers)[alloc]->insert(instr);
            (*allocDefs)[alloc]->insert(instr);
        }
    } else if (dynamic_cast<INSTR::BitCast*>(instr) != nullptr) {
        //bitcast
        for (Use* use = instr->getBeginUse(); use->next != nullptr; use = (Use*) use->next) {
            Instr* user = use->user;
            DFS(alloc, user);
        }
    } else {
        //assert false;
    }
}

void LoopInVarCodeLift::loopInVarLoadLift() {
    for (Function* function: *functions) {
        for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
            for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
                if (dynamic_cast<INSTR::Alloc*>(instr) != nullptr) {
                    //TODO:待加强,当前只考虑int数组
                    for (Use* use = instr->getBeginUse(); use->next != nullptr; use = (Use*) use->next) {
                        DFSArray(instr, use->user);
                    }
                }
            }
        }
    }
    for (GlobalValue* globalVal: KeySet(*globalValues)) {
        (*users)[globalVal] = new std::set<Instr*>();
        (*defs)[globalVal] = new std::set<Instr*>();
    }

    for (GlobalValue* globalVal: KeySet(*globalValues)) {
        //(*useGlobalFuncs)[globalVal] = new std::set<>();
        //(*defGlobalFuncs)[globalVal] = new std::set<>();
        for (Use* use = globalVal->getBeginUse(); use->next != nullptr; use = (Use*) use->next) {
            DFSArray(globalVal, use->user);
        }
    }

    for (Function* function: *functions) {
        (*funcUseGlobals)[function] = new std::set<Value*>();
        (*funcDefGlobals)[function] = new std::set<Value*>();
        (*callMap)[function] = new std::set<Function*>();
        for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
            for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
                if (dynamic_cast<INSTR::Call*>(instr) != nullptr && !((INSTR::Call*) instr)->getFunc()->isExternal) {
                    (*callMap)[function]->insert(((INSTR::Call*) instr)->getFunc());
                }
            }
        }
    }

    for (GlobalValue* globalVal: KeySet(*globalValues)) {
        for (Instr* instr: *(*users)[globalVal]) {
            (*funcUseGlobals)[instr->bb->function]->insert(globalVal);
        }
        for (Instr* instr: *(*defs)[globalVal]) {
//            funcDefGlobals->get(instr->bb->getFunction())->add(globalVal);
            (*funcDefGlobals)[instr->bb->function]->insert(globalVal);
        }
    }

    bool change = true;
    while (change) {
        change = false;
        for (Function* function: *functions) {
            for (Function* called: *(*callMap)[function]) {
                for (Value* value: *(*funcUseGlobals)[called]) {
                    bool ret = (*funcUseGlobals)[function]->find(value) == (*funcUseGlobals)[function]->end();
                    (*funcUseGlobals)[function]->insert(value);
                    if (ret) {
                        change = true;
                    }
                }

                for (Value* value: *(*funcDefGlobals)[called]) {
//                    bool ret = (*funcDefGlobals)[function]->add(value);
                    bool ret = (*funcUseGlobals)[function]->find(value) == (*funcUseGlobals)[function]->end();
                    (*funcUseGlobals)[function]->insert(value);
                    if (ret) {
                        change = true;
                    }
                }
            }
        }
    }

    for (Function* function: KeySet(*funcUseGlobals)) {
        for (Value* value: *(*funcUseGlobals)[function]) {
            for (Use* use = function->getBeginUse(); use->next != nullptr; use = (Use*) use->next) {
                Instr* user = use->user;
                (*users)[value]->insert(user);
            }
        }
    }

    for (Function* function: KeySet(*funcDefGlobals)) {
        for (Value* value: *(*funcDefGlobals)[function]) {
            for (Use* use = function->getBeginUse(); use->next != nullptr; use = (Use*) use->next) {
                Instr* user = use->user;
                (*defs)[value]->insert(user);
            }
        }
    }

    for (Value* value: KeySet(*users)) {
        (*useLoops)[value] = new std::set<Loop*>();
        (*defLoops)[value] = new std::set<Loop*>();
        for (Instr* instr: *(*users)[value]) {
            (*useLoops)[value]->insert(instr->bb->loop);
        }
    }

    for (Value* value: KeySet(*defs)) {
        (*defLoops)[value] = new std::set<Loop*>();
        for (Instr* instr: *(*defs)[value]) {
            (*defLoops)[value]->insert(instr->bb->loop);
        }
    }



    for (Value* array: KeySet(*users)) {
        loopInVarLoadLiftForArray(array);
    }


}

void LoopInVarCodeLift::DFSArray(Value* value, Instr* instr) {
    if (dynamic_cast<INSTR::GetElementPtr*>(instr) != nullptr) {
        for (Use* use = instr->getBeginUse(); use->next != nullptr; use = (Use*) use->next) {
            Instr* user = use->user;
            DFSArray(value, user);
        }
    } else if (dynamic_cast<INSTR::Store*>(instr) != nullptr) {
        if (!(defs->find(value) != defs->end())) {
            (*defs)[value] = new std::set<Instr*>();
        }
        (*defs)[value]->insert(instr);
    } else if (dynamic_cast<INSTR::Load*>(instr) != nullptr) {
        if (!(users->find(value) != users->end())) {
            (*users)[value] = new std::set<Instr*>();
        }
        (*users)[value]->insert(instr);
    } else if (dynamic_cast<INSTR::Call*>(instr) != nullptr) {
        //这两个函数的正确执行,不依赖于数组的原始值,所以只被认为是def
        if (((INSTR::Call*) instr)->getFunc()->getName() == ("memset") ||
            ((INSTR::Call*) instr)->getFunc()->getName() == ("getarray")) {
            if (!(defs->find(value) != defs->end())) {
                (*defs)[value] = new std::set<Instr*>();
            }
            (*defs)[value]->insert(instr);
        } else if (((INSTR::Call*) instr)->getFunc()->getName() == ("putarray")) {
            if (!(users->find(value) != users->end())) {
                (*users)[value] = new std::set<Instr*>();
            }
            (*users)[value]->insert(instr);
        } else {
            if (!(users->find(value) != users->end())) {
                (*users)[value] = new std::set<Instr*>();
            }
            if (!(defs->find(value) != defs->end())) {
                (*defs)[value] = new std::set<Instr*>();
            }
            (*users)[value]->insert(instr);
            (*defs)[value]->insert(instr);
        }
    } else if (dynamic_cast<INSTR::BitCast*>(instr) != nullptr) {
        //bitcast
        for (Use* use = instr->getBeginUse(); use->next != nullptr; use = (Use*) use->next) {
            Instr* user = use->user;
            DFSArray(value, user);
        }
    } else {
        //assert false;
    }
}

void LoopInVarCodeLift::loopInVarLoadLiftForArray(Value* array) {
    for (Instr* user: *(*users)[array]) {
        if (!(dynamic_cast<INSTR::Load*>(user) != nullptr)) {
            continue;
        }
        Loop* loop = user->bb->loop;
        if (loop->getEnterings()->size() > 1) {
            continue;
        }
        //处在0层循环的code不需要提升-->不能(entering为null)
        if (loop->getLoopDepth() == 0) {
            continue;
        }
        BasicBlock* entering = nullptr;
        for (BasicBlock* bb: *loop->getEnterings()) {
            entering = bb;
        }
        //强条件
        bool tag = false;
        for (Loop* defLoop: *(*defLoops)[array]) {
            if (check(loop, defLoop)) {
                tag = true;
            }
        }
        if (tag) {
            continue;
        }
        if ((*defLoops)[array]->find(loop) == (*defLoops)[array]->end()) {
            if ((dynamic_cast<INSTR::GetElementPtr*>(((INSTR::Load*) user)->getPointer()) != nullptr)  &&
                ((INSTR::GetElementPtr*) ((INSTR::Load*) user)->getPointer())->bb->loop == loop) {
                continue;
            }
            user->delFromNowBB();
            entering->getEndInstr()->insert_before(user);
            user->bb = entering;
        }
    }
}

bool LoopInVarCodeLift::check(Loop* useLoop, Loop* defLoop) {
    int useDeep = useLoop->getLoopDepth();
    int defDeep = defLoop->getLoopDepth();
    if (useDeep == defDeep) {
        return useLoop == defLoop;
    }
    if (useDeep > defDeep) {
        return false;
    } else {
        int time = defDeep - useDeep;
        for (int i = 0; i < time; i++) {
            defLoop = defLoop->getParentLoop();
        }
    }
    //assert useLoop->getLoopDepth() == defLoop->getLoopDepth();
    return useLoop == defLoop;
}


