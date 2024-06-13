#include "../../include/midend/LocalArrayGVN.h"
#include "../../include/util/CenterControl.h"
#include "HashMap.h"
#include "stl_op.h"




//TODO:GCM更新phi,删除无用phi,添加数组相关分析,
    // 把load,store,get_element_ptr也纳入GCM考虑之中






	LocalArrayGVN::LocalArrayGVN(std::vector<Function*>* functions, std::string label) {
        this->functions = functions;
        this->label = label;

    }

    void LocalArrayGVN::Run() {
        //GCM();

        if (label == ("GVN")) {
            Init();
            GVN();
        } else if (label == ("GCM")) {
            Init();
            GCM();
        }
    }

    void LocalArrayGVN::Init() {
        for (Function* function: *functions) {
            for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
                for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
                    if (dynamic_cast<INSTR::Alloc*>(instr) != nullptr) {
                        ((INSTR::Alloc*) instr)->clearLoads();
                    } else if (dynamic_cast<INSTR::Load*>(instr) != nullptr) {
                        ((INSTR::Load*) instr)->clear();
                    } else if (dynamic_cast<INSTR::Store*>(instr) != nullptr) {
                        ((INSTR::Store*) instr)->clear();
                    }
                }
            }
            for (Value* param: *function->params) {
                if (dynamic_cast<PointerType*>(param->type) != nullptr) {
                    ((Function::Param*) param)->loads->clear();
                }
            }
        }
        for (Function* function: *functions) {
            for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
                for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
                    if (dynamic_cast<INSTR::Alloc*>(instr) != nullptr) {
                        initAlloc(instr);
                    }
                }
            }
            for (Value* param: *function->params) {
                if (dynamic_cast<PointerType*>(param->type) != nullptr) {
                    initAlloc(param);
                }
            }
        }


    }

    void LocalArrayGVN::initAlloc(Value* alloc) {
        for (Use* use = alloc->getBeginUse(); use->next != nullptr; use = (Use*) use->next) {
            Instr* user = use->user;
            allocDFS(alloc, user);
        }
    }

    void LocalArrayGVN::allocDFS(Value* alloc, Instr* instr) {
        if (dynamic_cast<INSTR::GetElementPtr*>(instr) != nullptr) {
            for (Use* use = instr->getBeginUse(); use->next != nullptr; use = (Use*) use->next) {
                Instr* user = use->user;
                allocDFS(alloc, user);
            }
        } else if (dynamic_cast<INSTR::Load*>(instr) != nullptr) {
            ((INSTR::Load*) instr)->alloc = (alloc);
            if (dynamic_cast<INSTR::Alloc*>(alloc) != nullptr) {
                ((INSTR::Alloc*) alloc)->addLoad(instr);
            } else if (dynamic_cast<Function::Param*>(alloc) != nullptr) {
                ((Function::Param*) alloc)->loads->insert(instr);
            }
            //TODO:待强化
            if (dynamic_cast<INSTR::Alloc*>(alloc) != nullptr) {
                instrCanGCM->insert(instr);
            }
        } else if (dynamic_cast<INSTR::Store*>(instr) != nullptr) {
            ((INSTR::Store*) instr)->alloc = (alloc);

            if (dynamic_cast<INSTR::Alloc*>(alloc) != nullptr) {
                instrCanGCM->insert(instr);
            }
        } else {
            //assert false;
        }
    }

    void LocalArrayGVN::GVN() {
        GvnMap->clear();
        GvnCnt->clear();
        GVNInit();
        for (Function* function: *functions) {
            localArrayGVN(function);
        }
    }


    void LocalArrayGVN::localArrayGVN(Function* function) {
        BasicBlock* bb = function->getBeginBB();
        RPOSearch(bb);
    }



    void LocalArrayGVN::GVNInit() {
        for (Function* function: *functions) {
            if (check_good_func(function)) {
                goodFuncs->insert(function);
            }
        }
    }

    bool LocalArrayGVN::check_good_func(Function* function) {
        for (Value* value: *function->params) {
            if (value->type->is_pointer_type()) {
                return false;
            }
        }
        for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
            for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
                for (Value* value: instr->useValueList) {
                    if (dynamic_cast<GlobalValue*>(value) != nullptr) {
                        return false;
                    }
                }
            }
        }
        return true;
    }


    void LocalArrayGVN::RPOSearch(BasicBlock* bb) {
        if (_STRONG_CHECK_) {
            std::unordered_map<std::string, int>* tempGvnCnt = new std::unordered_map<std::string, int>();
            std::unordered_map<std::string, Instr*>* tempGvnMap = new std::unordered_map<std::string, Instr*>();
            std::unordered_map<std::string, int>* backGvnCnt = new std::unordered_map<std::string, int>();
            std::unordered_map<std::string, Instr*>* backGvnMap = new std::unordered_map<std::string, Instr*>();
            for (std::string key : KeySet(*GvnCnt)) {
                (*tempGvnCnt)[key] = (*GvnCnt)[key];
                (*backGvnCnt)[key] = (*GvnCnt)[key];
            }
            for (std::string key : KeySet(*GvnMap)) {
                (*tempGvnMap)[key] = (*GvnMap)[key];
                (*backGvnCnt)[key] = (*GvnCnt)[key];
            }
            //(*GvnCntByBB)[bb] = tempGvnCnt;


            if (bb->precBBs->size() > 1) {
                tempGvnCnt->clear();
                tempGvnMap->clear();
            }
            if (bb->precBBs->size() == 1 && bb->iDominator != ((*bb->precBBs)[0])) {
                tempGvnCnt->clear();
                tempGvnMap->clear();
            }


            Instr* instr = bb->getBeginInstr();
            while (instr->next != nullptr) {
                if (dynamic_cast<INSTR::Load*>(instr) != nullptr && ((INSTR::Load*) instr)->alloc != nullptr) {
                    addLoadToGVNStrong(instr, tempGvnMap);
                } else if (dynamic_cast<INSTR::Store*>(instr) != nullptr && ((INSTR::Store*) instr)->alloc != nullptr) {
                    Value* alloc = ((INSTR::Store*) instr)->alloc;
                    if (dynamic_cast<INSTR::Alloc*>(alloc) != nullptr) {
                        for (Instr* load : *((INSTR::Alloc*) alloc)->loads) {
                            //assert dynamic_cast<INSTR::Load*>(load) != nullptr;
                            try {
                                removeLoadFromGVNStrong(load, tempGvnMap);
                            } catch (int) {
                                //System->err.println("err");
                            }
                        }
                    } else if (dynamic_cast<Function::Param*>(alloc) != nullptr) {
                        for (Instr* load : *((Function::Param*) alloc)->loads) {
                            //assert dynamic_cast<INSTR::Load*>(load) != nullptr;
                            removeLoadFromGVNStrong(load, tempGvnMap);
                        }
                    } else {
                        //assert false;
                    }

                } else if (dynamic_cast<INSTR::Call*>(instr) != nullptr) {
                    //TODO:待强化,根据函数传入的指针,判断修改了哪个Alloc/参数
                    if (_contains_(goodFuncs, ((INSTR::Call*) instr)->getFunc())) {

                    } else {
                        GvnMap->clear();
                        GvnCnt->clear();
                        tempGvnCnt->clear();
                        tempGvnMap->clear();
                    }

                }
                instr = (Instr*) instr->next;
            }

            for (BasicBlock* next : *bb->idoms) {
                RPOSearch(next);
            }

            GvnMap->clear();
            GvnCnt->clear();
//            GvnMap->putAll(backGvnMap);
//            GvnCnt->putAll(backGvnCnt);
            _put_all(backGvnCnt, GvnCnt);
            _put_all(backGvnMap, GvnMap);
        } else {
            std::unordered_map<std::string, int>* tempGvnCnt = new std::unordered_map<std::string, int>();
            std::unordered_map<std::string, Instr*>* tempGvnMap = new std::unordered_map<std::string, Instr*>();
            for (std::string key : KeySet(*GvnCnt)) {
                (*tempGvnCnt)[key] = (*GvnCnt)[key];
            }
            for (std::string key : KeySet(*GvnMap)) {
                (*tempGvnMap)[key] = (*GvnMap)[key];
            }
            //(*GvnCntByBB)[bb] = tempGvnCnt;
            Instr* instr = bb->getBeginInstr();
            while (instr->next != nullptr) {
                if (dynamic_cast<INSTR::Load*>(instr) != nullptr && ((INSTR::Load*) instr)->alloc != nullptr) {
                    addLoadToGVN(instr);
                } else if (dynamic_cast<INSTR::Store*>(instr) != nullptr && ((INSTR::Store*) instr)->alloc != nullptr) {
                    Value* alloc = ((INSTR::Store*) instr)->alloc;
                    if (dynamic_cast<INSTR::Alloc*>(alloc) != nullptr) {
                        for (Instr* load : *((INSTR::Alloc*) alloc)->loads) {
                            //assert dynamic_cast<INSTR::Load*>(load) != nullptr;
                            try {
                                removeLoadFromGVN(load);
                            } catch (int) {
                                //System->err.println("err");
                            }
                        }
                    } else if (dynamic_cast<Function::Param*>(alloc) != nullptr) {
                        for (Instr* load : *((Function::Param*) alloc)->loads) {
                            //assert dynamic_cast<INSTR::Load*>(load) != nullptr;
                            removeLoadFromGVN(load);
                        }
                    } else {
                        //assert false;
                    }

                } else if (dynamic_cast<INSTR::Call*>(instr) != nullptr) {
                    //TODO:待强化,根据函数传入的指针,判断修改了哪个Alloc/参数
                    if (_contains_(goodFuncs, ((INSTR::Call*) instr)->getFunc())) {

                    } else {
                        GvnMap->clear();
                        GvnCnt->clear();
                    }

                }
                instr = (Instr*) instr->next;
            }

            for (BasicBlock* next : *bb->idoms) {
                RPOSearch(next);
            }

            GvnMap->clear();
            GvnCnt->clear();
//            GvnMap->putAll(tempGvnMap);
//            GvnCnt->putAll(tempGvnCnt);
            _put_all(tempGvnMap, GvnMap);
            _put_all(tempGvnCnt, GvnCnt);
        }
    }

    void LocalArrayGVN::add(std::string str, Instr* instr) {
        if (!(GvnCnt->find(str) != GvnCnt->end())) {
            (*GvnCnt)[str] = 1;
        } else {
            (*GvnCnt)[str] = (*GvnCnt)[str] + 1;
        }
        if (!(GvnMap->find(str) != GvnMap->end())) {
            (*GvnMap)[str] = instr;
        }
    }

    void LocalArrayGVN::remove(std::string str) {
        if (!(GvnCnt->find(str) != GvnCnt->end()) || (*GvnCnt)[str] == 0) {
            return;
        }
        (*GvnCnt)[str] = (*GvnCnt)[str] - 1;
        if ((*GvnCnt)[str] == 0) {
            GvnMap->erase(str);
        }
    }

    bool LocalArrayGVN::addLoadToGVNStrong(Instr* load, std::unordered_map<std::string, Instr*>* tempGvnMap) {
        //进行替换
        //assert dynamic_cast<INSTR::Load*>(load) != nullptr;
        std::string hash = ((INSTR::Load*) load)->getPointer()->name;
        if ((GvnMap->find(hash) != GvnMap->end()) && (tempGvnMap->find(hash) != tempGvnMap->end())) {
            load->modifyAllUseThisToUseA((*GvnMap)[hash]);
            //load->remove();
            return true;
        }
        add(hash, load);
        if (!(tempGvnMap->find(hash) != tempGvnMap->end())) {
            (*tempGvnMap)[hash] = load;
        }
        return false;
    }

    void LocalArrayGVN::removeLoadFromGVNStrong(Instr* load, std::unordered_map<std::string, Instr*>* tempGvnMap) {
        //assert dynamic_cast<INSTR::Load*>(load) != nullptr;
        std::string hash = ((INSTR::Load*) load)->getPointer()->name;
        remove(hash);
        tempGvnMap->erase(hash);
    }


    bool LocalArrayGVN::addLoadToGVN(Instr* load) {
        //进行替换
        //assert dynamic_cast<INSTR::Load*>(load) != nullptr;
        std::string hash = ((INSTR::Load*) load)->getPointer()->name;
        if ((GvnMap->find(hash) != GvnMap->end())) {
            load->modifyAllUseThisToUseA((*GvnMap)[hash]);
            //load->remove();
            return true;
        }
        add(hash, load);
        return false;
    }

    void LocalArrayGVN::removeLoadFromGVN(Instr* load) {
        //assert dynamic_cast<INSTR::Load*>(load) != nullptr;
        std::string hash = ((INSTR::Load*) load)->getPointer()->name;
        remove(hash);
    }



    //TODO:GVM添加store
    //      存在一条A(store)到B(load)的路径,认为存在user/use关系
    //      考虑循环的数据流
    void LocalArrayGVN::GCM() {
        GCMInit();
        for (Function* function: *functions) {
            scheduleEarlyForFunc(function);
        }

        for (Function* function: *functions) {
            scheduleLateForFunc(function);
        }
    }

    void LocalArrayGVN::GCMInit() {
        for (Function* function: *functions) {
            (*pinnedInstrMap)[function] = new std::set<Instr*>();
            reachDef->clear();
            for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
                for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
                    if (!(instrCanGCM->find(instr) != instrCanGCM->end())) {
                        (*pinnedInstrMap)[function]->insert(instr);
                    }
                    if (dynamic_cast<INSTR::Alloc*>(instr) != nullptr) {
                        (*reachDef)[instr] = instr;
                    }
                }
            }
            DFS(function->getBeginBB());
        }

    }

    void LocalArrayGVN::DFS(BasicBlock* bb) {
        for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
            if (dynamic_cast<INSTR::Store*>(instr) != nullptr && ((INSTR::Store*) instr)->alloc != nullptr) {
                (*reachDef)[((INSTR::Store*) instr)->alloc] = instr;
            } else if (dynamic_cast<INSTR::Load*>(instr) != nullptr && ((INSTR::Load*) instr)->alloc != nullptr) {
                ((INSTR::Load*) instr)->useStore = (reachDef->find(((INSTR::Load*) instr)->alloc)->second);
                Instr* def = reachDef->find(((INSTR::Load*) instr)->alloc)->second;
                if (dynamic_cast<INSTR::Store*>(def) != nullptr) {
                    ((INSTR::Store*) def)->addUser(instr);
                }
            } else if (dynamic_cast<INSTR::Call*>(instr) != nullptr) {
                for (Value* value: KeySet(*reachDef)) {
                    (*reachDef)[value] = instr;
                }
            }
        }
        for (BasicBlock* next: *bb->idoms) {
            DFS(next);
        }
    }


    void LocalArrayGVN::scheduleEarlyForFunc(Function* function) {
        std::set<Instr*>* pinnedInstr = (*pinnedInstrMap)[function];
        know = new std::set<Instr*>();
        root = function->getBeginBB();
        for (Instr* instr: *pinnedInstr) {
            instr->earliestBB = instr->bb;
            know->insert(instr);
        }
        for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
           for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
               if (!(know->find(instr) != know->end())) {
                   scheduleEarly(instr);
               } else if ((pinnedInstr->find(instr) != pinnedInstr->end())) {
                   for (Value* value: instr->useValueList) {
                       if (!(dynamic_cast<Instr*>(value) != nullptr)) {
                           continue;
                       }
                       scheduleEarly((Instr*) value);
                   }
               }
           }
        }
    }

    void LocalArrayGVN::scheduleEarly(Instr* instr) {
        if ((know->find(instr) != know->end())) {
            return;
        }
        know->insert(instr);
        instr->earliestBB = root;
        for (Value* X: instr->useValueList) {
            if (dynamic_cast<Instr*>(X) != nullptr) {
                scheduleEarly((Instr*) X);
                if (instr->earliestBB->domTreeDeep < ((Instr*) X)->earliestBB->domTreeDeep) {
                    instr->earliestBB = ((Instr*) X)->earliestBB;
                }
            }
        }
        if (dynamic_cast<INSTR::Load*>(instr) != nullptr) {
            if (((INSTR::Load*) instr)->useStore == nullptr) {
                //System->err.println("err");
            }
            Value* X = ((INSTR::Load*) instr)->useStore;
            scheduleEarly((Instr*) X);
            if (instr->earliestBB->domTreeDeep < ((Instr*) X)->earliestBB->domTreeDeep) {
                instr->earliestBB = ((Instr*) X)->earliestBB;
            }
        }
    }


    void LocalArrayGVN::scheduleLateForFunc(Function* function) {
        std::set<Instr*>* pinnedInstr = (*pinnedInstrMap)[function];
        know = new std::set<Instr*>();
        for (Instr* instr: *pinnedInstr) {
            instr->latestBB = instr->bb;
            know->insert(instr);
        }
        for (Instr* instr: *pinnedInstr) {
            Use* use = instr->getBeginUse();
            while (use->next != nullptr) {
                scheduleLate(use->user);
                use = (Use*) use->next;
            }
        }
        for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
            std::vector<Instr*>* instrs = new std::vector<Instr*>();
            Instr* instr = bb->getEndInstr();
            while (instr->prev != nullptr) {
                instrs->push_back(instr);
                instr = (Instr*) instr->prev;
            }
            for (Instr* instr1: *instrs) {
                if (!(know->find(instr1) != know->end())) {
                    scheduleLate(instr1);
                }
            }
        }
    }

    void LocalArrayGVN::scheduleLate(Instr* instr) {
//        if (instr->toString()->equals("store i32 %v242, i32* %v239")) {
//            //System->err.println("err");
//        }

        if ((know->find(instr) != know->end())) {
            return;
        }
        know->insert(instr);
        BasicBlock* lca = nullptr;
        Use* usePos = instr->getBeginUse();
        while (usePos->next != nullptr) {
            Instr* y = usePos->user;

            scheduleLate(y);
            BasicBlock* use = y->latestBB;
            if (dynamic_cast<INSTR::Phi*>(y) != nullptr) {
                int j = usePos->idx;
                use = (*y->latestBB->precBBs)[j];
            }
            lca = findLCA(lca, use);
            usePos = (Use*) usePos->next;
        }
        if (dynamic_cast<INSTR::Store*>(instr) != nullptr) {
            for (Instr* y : *((INSTR::Store*) instr)->users) {
                scheduleLate(y);
                BasicBlock* use = y->latestBB;
                if (dynamic_cast<INSTR::Phi*>(y) != nullptr) {
                    int j = usePos->idx;
                    use = (*y->latestBB->precBBs)[j];
                }
                lca = findLCA(lca, use);
            }
        }
        // use the latest and earliest blocks to pick final positing
        // now latest is lca
        BasicBlock* best = lca;
        if (lca == nullptr) {
            instr->latestBB = instr->bb;
            //System->err.println("err_GCM " + instr->toString());
        } else {
            while (!(lca == instr->earliestBB)) {
                if (lca->getLoopDep() < best->getLoopDep()) {
                    best = lca;
                }
                lca = lca->iDominator;
                if (lca == nullptr) {
                    //System->err.println("err_GCM " + instr->toString());
                }
            }
            if (lca->getLoopDep() < best->getLoopDep()) {
                best = lca;
            }
            instr->latestBB = best;
        }

        if (instr->latestBB != (instr->bb)) {
            //System->err.println("Array GCM Move");
            instr->delFromNowBB();
            //TODO:检查 insert 位置 是在头部还是尾部
            Instr* pos = findInsertPos(instr, instr->latestBB);
            pos->insert_before(instr);
            instr->bb = instr->latestBB;
        }
    }

    BasicBlock* LocalArrayGVN::findLCA(BasicBlock* a, BasicBlock* b) {
        if (a == nullptr) {
            return b;
        }
        while (a->domTreeDeep > b->domTreeDeep) {
            a = a->iDominator;
        }
        while (b->domTreeDeep > a->domTreeDeep) {
            b = b->iDominator;
        }
        while (!(a == b)) {
            a = a->iDominator;
            b = b->iDominator;
        }
        return a;
    }

    Instr* LocalArrayGVN::findInsertPos(Instr* instr, BasicBlock* bb) {
        std::set<Value*>* users = new std::set<Value*>();
        Use* use = instr->getBeginUse();
        while (use->next != nullptr) {
            users->insert(use->user);
            use = (Use*) use->next;
        }
        if (dynamic_cast<INSTR::Store*>(instr) != nullptr) {
            _set_add_all(((INSTR::Store*) instr)->users, users);
        }
        Instr* later = nullptr;
        Instr* pos = bb->getBeginInstr();
        while (pos->next != nullptr) {
            if (dynamic_cast<INSTR::Phi*>(pos) != nullptr) {
                pos = (Instr*) pos->next;
                continue;
            }
            if ((users->find(pos) != users->end())) {
                later = pos;
                break;
            }
            pos = (Instr*) pos->next;
        }

        if (later != nullptr) {
            return later;
        }

        return bb->getEndInstr();
    }

    //只移动Load,Store
    //  store的user认为是store的数组的所有被store支配的load
    //  load的use除了基本的use还有 所有支配它的store
    bool LocalArrayGVN::isPinned(Instr* instr) {
        return !(dynamic_cast<INSTR::Store*>(instr) != nullptr) && !(dynamic_cast<INSTR::Load*>(instr) != nullptr);
    }
