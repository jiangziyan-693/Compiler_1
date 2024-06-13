#include "../../include/midend/AggressiveFuncGVN.h"
#include "../../include/util/CenterControl.h"
#include "HashMap.h"

#include "../../include/util/stl_op.h"

//激进的GVN认为没有自定义函数调用的函数都可以GVN,
// 即使它的传参有数组

AggressiveFuncGVN::AggressiveFuncGVN(std::vector<Function *> *functions,
                                     std::unordered_map<GlobalValue *, Initial *> *globalValues) {
    this->functions = functions;
    this->globalValues = globalValues;
}

void AggressiveFuncGVN::Run() {
    for (Function *function: *functions) {
        (*use)[function] = new std::set<int>();
        (*def)[function] = new std::set<int>();
    }
    for (Value *value: KeySet(*globalValues)) {
        (*callMap)[value] = new std::set<Instr *>();
    }
    for (Function *function: *functions) {
        for (Function::Param *param: *function->params) {
            (*callMap)[param] = new std::set<Instr *>();
        }
        for (BasicBlock *bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock *) bb->next) {
            for (Instr *instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr *) instr->next) {
                if (dynamic_cast<INSTR::Alloc *>(instr) != nullptr) {
                    (*callMap)[instr] = new std::set<Instr *>();
                }
            }
        }
    }
    init();
    initCallMap();
    GVN();
    for (Instr *instr: *removes) {
        instr->remove();
    }
}

void AggressiveFuncGVN::init() {
    //clear
    for (Function *function: *functions) {
        for (BasicBlock *bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock *) bb->next) {
            for (Instr *instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr *) instr->next) {
                if (dynamic_cast<INSTR::Alloc *>(instr) != nullptr) {
                    ((INSTR::Alloc *) instr)->clearLoads();
                } else if (dynamic_cast<INSTR::Load *>(instr) != nullptr) {
                    ((INSTR::Load *) instr)->clear();
                } else if (dynamic_cast<INSTR::Store *>(instr) != nullptr) {
                    ((INSTR::Store *) instr)->clear();
                }
            }
        }
        for (Value *param: *function->params) {
            if (dynamic_cast<PointerType *>(param->type) != nullptr) {
                //TODO:check clearLoads
                ((Function::Param *) param)->loads->clear();
            }
        }
    }

    for (Function *function: *functions) {
        for (BasicBlock *bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock *) bb->next) {
            for (Instr *instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr *) instr->next) {
                if (dynamic_cast<INSTR::Alloc *>(instr) != nullptr) {
                    initAlloc(instr);
                }
            }
        }
        for (Value *param: *function->params) {
            if (dynamic_cast<PointerType *>(param->type) != nullptr) {
                initAlloc(param);
            }
        }
    }

    for (GlobalValue *val: KeySet(*globalValues)) {
        if (dynamic_cast<ArrayType *>(((PointerType *) val->type)->inner_type) != nullptr) {
            for (Use *use = val->getBeginUse(); use->next != nullptr; use = (Use *) use->next) {
                globalArrayDFS(val, use->user);
            }
        }
    }


    for (Function *function: *functions) {
        if (check(function)) {
            canGVN->insert(function);
        }
        for (BasicBlock *bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock *) bb->next) {
            for (Instr *instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr *) instr->next) {
                if (dynamic_cast<INSTR::Store *>(instr) != nullptr) {
                    Value *val = ((INSTR::Store *) instr)->getPointer();
                    while (dynamic_cast<INSTR::GetElementPtr *>(val) != nullptr) {
                        val = ((INSTR::GetElementPtr *) val)->getPtr();
                    }
                    if (dynamic_cast<Function::Param *>(val) != nullptr) {
                        (*def)[function]->insert(_index_of(function->params, val));
                    }
                } else if (dynamic_cast<INSTR::Load *>(instr) != nullptr) {
                    Value *val = ((INSTR::Load *) instr)->getPointer();
                    while (dynamic_cast<INSTR::GetElementPtr *>(val) != nullptr) {
                        val = ((INSTR::GetElementPtr *) val)->getPtr();
                    }
                    if (dynamic_cast<Function::Param *>(val) != nullptr) {
                        (*use)[function]->insert(_index_of(function->params, val));
                    }
                }
            }
        }
    }

    for (Function *function: *functions) {
        for (BasicBlock *bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock *) bb->next) {
            for (Instr *instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr *) instr->next) {
                if (dynamic_cast<INSTR::Call *>(instr) != nullptr) {
                    Function *callFunc = ((INSTR::Call *) instr)->getFunc();
                    for (Value *val: *((INSTR::Call *) instr)->getParamList()) {
                        if (dynamic_cast<Function::Param *>(val) != nullptr) {
                            int thisFuncIndex = _index_of(function->params, val);
                            int callFuncIndex = _index_of(((INSTR::Call *) instr)->getParamList(), val);

                            //fixme:has-bug
                            if (callFunc->isExternal) {
                                (*use)[function]->insert(thisFuncIndex);
                            } else {
                                if (_contains_((*use)[callFunc], callFuncIndex)) {
                                    (*use)[function]->insert(thisFuncIndex);
                                }
                                if (_contains_((*def)[callFunc], callFuncIndex)) {
                                    (*def)[function]->insert(thisFuncIndex);
                                }
                            }


                        }
                    }
                }
            }
        }
    }
}

void AggressiveFuncGVN::initCallMap() {
    for (Function *function: *functions) {
        for (BasicBlock *bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock *) bb->next) {
            for (Instr *instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr *) instr->next) {
                if (dynamic_cast<INSTR::Call *>(instr) != nullptr) {
                    for (Value *val: *((INSTR::Call *) instr)->getParamList()) {
                        if (val->type->is_pointer_type()) {
                            while (dynamic_cast<INSTR::GetElementPtr *>(val) != nullptr) {
                                val = ((INSTR::GetElementPtr *) val)->getPtr();
                            }
                            if (!(callMap->find(val) != callMap->end())) {
                                (*callMap)[val] = new std::set<Instr *>();
                            }
                            (*callMap)[val]->insert(instr);
                        }
                    }
                }
            }
        }
    }
}

void AggressiveFuncGVN::globalArrayDFS(GlobalValue *array, Instr *instr) {
    if (dynamic_cast<INSTR::GetElementPtr *>(instr) != nullptr) {
        for (Use *use = instr->getBeginUse(); use->next != nullptr; use = (Use *) use->next) {
            Instr *user = use->user;
            globalArrayDFS(array, user);
        }
    } else if (dynamic_cast<INSTR::Load *>(instr) != nullptr) {
        ((INSTR::Load *) instr)->alloc = (array);
        if (!(loads->find(array) != loads->end())) {
            (*loads)[array] = new std::set<Instr *>();
        }
        (*loads)[array]->insert(instr);
    } else if (dynamic_cast<INSTR::Store *>(instr) != nullptr) {
        ((INSTR::Store *) instr)->alloc = (array);
    } else {
        //assert false;
    }
}

void AggressiveFuncGVN::initAlloc(Value *alloc) {
    for (Use *use = alloc->getBeginUse(); use->next != nullptr; use = (Use *) use->next) {
        Instr *user = use->user;
        allocDFS(alloc, user);
    }
}

void AggressiveFuncGVN::allocDFS(Value *alloc, Instr *instr) {
    if (dynamic_cast<INSTR::GetElementPtr *>(instr) != nullptr) {
        for (Use *use = instr->getBeginUse(); use->next != nullptr; use = (Use *) use->next) {
            Instr *user = use->user;
            allocDFS(alloc, user);
        }
    } else if (dynamic_cast<INSTR::Load *>(instr) != nullptr) {
        ((INSTR::Load *) instr)->alloc = (alloc);
        if (dynamic_cast<INSTR::Alloc *>(alloc) != nullptr) {
            ((INSTR::Alloc *) alloc)->addLoad(instr);
        } else if (dynamic_cast<Function::Param *>(alloc) != nullptr) {
            ((Function::Param *) alloc)->loads->insert(instr);
        }
    } else if (dynamic_cast<INSTR::Store *>(instr) != nullptr) {
        ((INSTR::Store *) instr)->alloc = (alloc);
    } else {
        //assert false;
        //memset已经特判,不需要考虑bitcast?
    }
}


bool AggressiveFuncGVN::check(Function *function) {
    for (BasicBlock *bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock *) bb->next) {
        for (Instr *instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr *) instr->next) {
            if (dynamic_cast<INSTR::Call *>(instr) != nullptr) {
                if (!(((INSTR::Call *) instr)->getFunc() == function)) {
                    return false;
                }
                //return false;
            }
            for (Value *value: instr->useValueList) {
                if (dynamic_cast<GlobalValue *>(value) != nullptr) {
                    return false;
                }
            }
        }
    }
    return true;
}

void AggressiveFuncGVN::GVN() {
    for (Function *function: *functions) {
        BasicBlock *bb = function->getBeginBB();
        RPOSearch(bb);
    }
}

void AggressiveFuncGVN::RPOSearch(BasicBlock *bb) {
    std::unordered_map<std::string, int> *tempGvnCnt = new std::unordered_map<std::string, int>();
    std::unordered_map<std::string, Instr *> *tempGvnMap = new std::unordered_map<std::string, Instr *>();
//        tempGvnCnt->putAll(GvnCnt);
//        tempGvnMap->putAll(GvnMap);

    _put_all(GvnCnt, tempGvnCnt);
    _put_all(GvnMap, tempGvnMap);

    for (Instr *instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr *) instr->next) {
        if (dynamic_cast<INSTR::Call *>(instr) != nullptr && ((INSTR::Call *) instr)->getFunc()->isExternal) {
            if (((INSTR::Call *) instr)->getFunc()->name == ("memset")) {
                Value *array = (*((INSTR::Call *) instr)->getParamList())[0];
                while (dynamic_cast<INSTR::GetElementPtr *>(array) != nullptr) {
                    array = ((INSTR::GetElementPtr *) array)->getPtr();
                }
                //assert (callMap->find(array) != callMap->end());
                for (Instr *instr1: *(*callMap)[array]) {
                    removeCallFromGVN(instr1);
                }
            } else {
                return;
            }
        }

        if (dynamic_cast<INSTR::Call *>(instr) != nullptr && !((INSTR::Call *) instr)->getFunc()->isExternal) {
            if (!_contains_(canGVN, ((INSTR::Call *) instr)->getFunc())) {
                continue;
            }
            addCallToGVN(instr);
            Function *func = ((INSTR::Call *) instr)->getFunc();
            for (int index: *(*def)[func]) {
                Value *array = (*((INSTR::Call *) instr)->getParamList())[index];
                while (dynamic_cast<INSTR::GetElementPtr *>(array) != nullptr) {
                    array = ((INSTR::GetElementPtr *) array)->getPtr();
                }
                //assert (callMap->find(array) != callMap->end());
                for (Instr *instr1: *(*callMap)[array]) {
                    removeCallFromGVN(instr1);
                }
            }
        } else if (dynamic_cast<INSTR::Store *>(instr) != nullptr) {
            if (!((PointerType *) ((INSTR::Store *) instr)->getPointer()->type)->inner_type->is_basic_type()) {
                Value *array = ((INSTR::Store *) instr)->alloc;
                for (Instr *instr1: *(*callMap)[array]) {
                    removeCallFromGVN(instr1);
                }
            }
        }
    }

    for (BasicBlock *next: *bb->idoms) {
        RPOSearch(next);
    }

    GvnMap->clear();
    GvnCnt->clear();
//        GvnMap->putAll(tempGvnMap);
//        GvnCnt->putAll(tempGvnCnt);
    _put_all(tempGvnMap, GvnMap);
    _put_all(tempGvnCnt, GvnCnt);

}

void AggressiveFuncGVN::add(std::string str, Instr *instr) {
    if (!(GvnCnt->find(str) != GvnCnt->end())) {
        (*GvnCnt)[str] = 1;
    } else {
        (*GvnCnt)[str] = (*GvnCnt)[str] + 1;
    }
    if (!(GvnMap->find(str) != GvnMap->end())) {
        (*GvnMap)[str] = instr;
    }
}

void AggressiveFuncGVN::remove(std::string str) {
    if (!(GvnCnt->find(str) != GvnCnt->end()) || (*GvnCnt)[str] == 0) {
        return;
    }
    (*GvnCnt)[str] = (*GvnCnt)[str] - 1;
    if ((*GvnCnt)[str] == 0) {
        GvnMap->erase(str);
    }
}

bool AggressiveFuncGVN::addCallToGVN(Instr *call) {
    //进行替换
    //assert dynamic_cast<INSTR::Call*>(call) != nullptr;
    std::string hash = ((INSTR::Call *) call)->getFunc()->name + "(";
    for (Value *value: *((INSTR::Call *) call)->getParamList()) {
        hash += value->name + ", ";
    }
    hash += ")";
    if ((GvnMap->find(hash) != GvnMap->end())) {
        call->modifyAllUseThisToUseA((*GvnMap)[hash]);
        //call->remove();
        removes->insert(call);
        return true;
    }
    add(hash, call);
    return false;
}

void AggressiveFuncGVN::removeCallFromGVN(Instr *call) {
    //assert dynamic_cast<INSTR::Call*>(call) != nullptr;
    std::string hash = ((INSTR::Call *) call)->getFunc()->name + "(";
    for (Value *value: *((INSTR::Call *) call)->getParamList()) {
        hash += value->name + ", ";
    }
    hash += ")";
    remove(hash);
}
