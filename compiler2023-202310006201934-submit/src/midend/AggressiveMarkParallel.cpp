#include "../../include/midend/AggressiveMarkParallel.h"
#include "../../include/util/CenterControl.h"


#include "stl_op.h"





	AggressiveMarkParallel::AggressiveMarkParallel(std::vector<Function*>* functions) {
        this->functions = functions;
    }

    void AggressiveMarkParallel::Run() {
        init();
        for (Function* function: *functions) {
            know->clear();
            for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
                if (!(know->find(bb) != know->end()) && bb->isLoopHeader) {
                    markLoop(bb->loop);
                }
            }
        }
    }

    void AggressiveMarkParallel::init() {
        for (Function* function: *functions) {
            (*funcLoadArrays)[function] = new std::set<Value*>();
            if (check(function)) {
                goodFunc->insert(function);
            }
        }
    }

    bool AggressiveMarkParallel::check(Function* function) {
        for (Value* param: *function->params) {
            if (param->type->is_pointer_type()) {
                return false;
            }
        }
        for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
            for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
                if (dynamic_cast<INSTR::Call*>(instr) != nullptr) {
                    return false;
                }
                if (dynamic_cast<INSTR::Store*>(instr) != nullptr) {
                    return false;
                } else if (dynamic_cast<INSTR::Load*>(instr) != nullptr) {
                    Value* array = ((INSTR::Load*) instr)->getPointer();
                    while (dynamic_cast<INSTR::GetElementPtr*>(array) != nullptr) {
                        array = ((INSTR::GetElementPtr*) array)->getPtr();
                    }
                    (*funcLoadArrays)[function]->insert(array);
                }
            }
        }
        return true;
    }

    bool AggressiveMarkParallel::isPureLoop(Loop* loop) {
        if (!loop->hasChildLoop()) {
            return true;
        }
        if (loop->getChildrenLoops()->size() > 1) {
            return false;
        }
        if (!loop->isSimpleLoop() || !loop->isIdcSet()) {
            return false;
        }
        return isPureLoop(*loop->getChildrenLoops()->begin());
    }

    void AggressiveMarkParallel::markLoop(Loop* loop) {
        //当前只考虑main函数内的循环
        if (!isPureLoop(loop)) {
            return;
        }
        if (loop->hasChildLoop()) {
            return;
        }
        std::set<BasicBlock*>* bbs = new std::set<BasicBlock*>();
        std::set<Value*>* idcVars = new std::set<Value*>();
        std::set<Loop*>* loops = new std::set<Loop*>();
//        bbs->addAll(loop->getNowLevelBB());
        _set_add_all(loop->getNowLevelBB(), bbs);
        idcVars->insert(loop->getIdcPHI());
        loops->insert(loop);

        std::set<Value*>* idcInstrs = new std::set<Value*>();
        idcInstrs->insert(loop->getIdcPHI());
        idcInstrs->insert(loop->getIdcCmp());
        idcInstrs->insert(loop->getIdcAlu());
        idcInstrs->insert(loop->getHeader()->getEndInstr());
        idcInstrs->insert((*loop->getLatchs()->begin())->getEndInstr());

        for (Instr* instr = loop->getHeader()->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
            if (dynamic_cast<INSTR::Phi*>(instr) != nullptr && instr != (loop->getIdcPHI())) {
                return;
            }
        }

        //只有一次调用函数, 读一个数组, 写一个数组
        Value *loadArray = nullptr, *storeArray = nullptr;
        Function* func = nullptr;

        for (BasicBlock* bb: *bbs) {
            for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
                if (useOutLoops(instr, loops)) {
                    return;
                }
                if ((idcInstrs->find(instr) != idcInstrs->end())) {

                } else if (dynamic_cast<INSTR::Call*>(instr) != nullptr) {
                    if (((INSTR::Call*) instr)->getFunc()->isExternal) {
                        continue;
                    }
                    if (func != nullptr) {
                        return;
                    }
                    func = ((INSTR::Call*) instr)->getFunc();
                } else if (dynamic_cast<INSTR::GetElementPtr*>(instr) != nullptr) {
                    //trivel写法
                    if (!(dynamic_cast<GlobalValue*>(((INSTR::GetElementPtr*) instr)->getPtr()) != nullptr)) {
                        return;
                    }
                    for (Value* idc: *((INSTR::GetElementPtr*) instr)->getIdxList()) {
                        if (dynamic_cast<Constant*>(idc) != nullptr && (int) ((ConstantInt*) idc)->get_const_val() == 0) {
                            continue;
                        }
                        if (!(idcVars->find(idc) != idcVars->end())) {
                            return;
                        }
                    }
                } else if (dynamic_cast<INSTR::Load*>(instr) != nullptr) {
                    if (loadArray != nullptr) {
                        return;
                    }
                    Value* array = ((INSTR::Load*) instr)->getPointer();
                    while (dynamic_cast<INSTR::GetElementPtr*>(array) != nullptr) {
                        array = ((INSTR::GetElementPtr*) array)->getPtr();
                    }
                    loadArray = array;
                } else if (dynamic_cast<INSTR::Store*>(instr) != nullptr) {
                    if (storeArray != nullptr) {
                        return;
                    }
                    Value* array = ((INSTR::Store*) instr)->getPointer();
                    while (dynamic_cast<INSTR::GetElementPtr*>(array) != nullptr) {
                        array = ((INSTR::GetElementPtr*) array)->getPtr();
                    }
                    storeArray = array;
                } else {
                    return;
                }
            }
        }

        if (func == nullptr || loadArray == nullptr || storeArray == nullptr) {
            return;
        }

        if (dynamic_cast<GlobalValue *>(loadArray) == nullptr || dynamic_cast<GlobalValue *>(storeArray) == nullptr) {
            return;
        }

        //很强的限制,可以考虑删除

        for (Value* array: *(*funcLoadArrays)[func]) {
            if (!(dynamic_cast<GlobalValue*>(array) != nullptr)) {
                return;
            }
        }
        if (_contains_((*funcLoadArrays)[func], storeArray) || loadArray == (storeArray)) {
            return;
        }

        loop->setCanAggressiveParallel(true);
        CenterControl::_FUNC_PARALLEL = true;
    }


    bool AggressiveMarkParallel::useOutLoops(Value* value, std::set<Loop*>* loops) {
        for (Use* use = value->getBeginUse(); use->next != nullptr; use = (Use*) use->next) {
            Instr* user = use->user;
            if (!_contains_(loops, user->bb->loop)) {
                return true;
            }
        }
        return false;
    }
