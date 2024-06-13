#include "../../include/midend/GlobalArrayGVN.h"
#include "../../include/util/CenterControl.h"
#include "HashMap.h"
#include "stl_op.h"

#include "Manager.h"

	GlobalArrayGVN::GlobalArrayGVN(std::vector<Function*>* functions, std::unordered_map<GlobalValue*, Initial*>* globalValues) {
        this->functions = functions;
        this->globalValues = globalValues;
    }

    void GlobalArrayGVN::Run() {
        init();
        GVN();
    }

    void GlobalArrayGVN::init() {
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

        for (GlobalValue* val: KeySet(*globalValues)) {
            if (dynamic_cast<ArrayType*>(((PointerType*) val->type)->inner_type) != nullptr) {
                for (Use* use = val->getBeginUse(); use->next != nullptr; use = (Use*) use->next) {
                    DFS(val, use->user);
                }
            }
        }


    }

    void GlobalArrayGVN::DFS(GlobalValue* array, Instr* instr) {
        if (dynamic_cast<INSTR::GetElementPtr*>(instr) != nullptr) {
            for (Use* use = instr->getBeginUse(); use->next != nullptr; use = (Use*) use->next) {
                Instr* user = use->user;
                DFS(array, user);
            }
        } else if (dynamic_cast<INSTR::Load*>(instr) != nullptr) {
            ((INSTR::Load*) instr)->alloc = (array);
            if (!(loads->find(array) != loads->end())) {
                (*loads)[array] = new std::set<Instr*>();
            }
            (*loads)[array]->insert(instr);
        } else if (dynamic_cast<INSTR::Store*>(instr) != nullptr) {
            ((INSTR::Store*) instr)->alloc = (array);
        } else {
            //assert false;
        }
    }

    void GlobalArrayGVN::GVN() {
        GvnMap->clear();
        GvnCnt->clear();
        for (Function* function: *functions) {
            globalArrayGVN(function);
        }
    }

    void GlobalArrayGVN::globalArrayGVN(Function* function) {
        BasicBlock* bb = function->getBeginBB();
        RPOSearch(bb);
    }

    void GlobalArrayGVN::RPOSearch(BasicBlock* bb) {
        std::unordered_map<std::string, int>* tempGvnCnt = new std::unordered_map<std::string, int>();
        std::unordered_map<std::string, Instr*>* tempGvnMap = new std::unordered_map<std::string, Instr*>();
        for (std::string key: KeySet(*GvnCnt)) {
            (*tempGvnCnt)[key] = (*GvnCnt)[key];
        }
        for (std::string key: KeySet(*GvnMap)) {
            (*tempGvnMap)[key] = (*GvnMap)[key];
        }


        Instr* instr = bb->getBeginInstr();
        while (instr->next != nullptr) {
            if (dynamic_cast<INSTR::Load*>(instr) != nullptr && ((INSTR::Load*) instr)->alloc != nullptr) {
                addLoadToGVN(instr);
            } else if (dynamic_cast<INSTR::Store*>(instr) != nullptr && ((INSTR::Store*) instr)->alloc != nullptr) {
                Value* array = ((INSTR::Store*) instr)->alloc;
                //assert dynamic_cast<GlobalValue*>(array) != nullptr;
                if ((loads->find((GlobalValue*) array) != loads->end())) {
                    for (Instr* load : *(*loads)[(GlobalValue*) array]) {
                        //assert dynamic_cast<INSTR::Load*>(load) != nullptr;
                        removeLoadFromGVN(load);
                    }
                } else {
                    //这是正常的,因为单一函数中可能没有load全局数组,只是store了它
                    //System->err.println("no_use");
                }

            } else if (dynamic_cast<INSTR::Call*>(instr) != nullptr &&
                    ((INSTR::Call*) instr)->getFunc() != ExternFunction::USAT) {
                //TODO:待强化,根据函数传入的指针,判断修改了哪个Alloc/参数
                GvnMap->clear();
                GvnCnt->clear();
            }
            instr = (Instr*) instr->next;
        }

        for (BasicBlock* next : *bb->idoms) {
            RPOSearch(next);
        }

        GvnMap->clear();
        GvnCnt->clear();
//        GvnMap->putAll(tempGvnMap);
//        GvnCnt->putAll(tempGvnCnt);
        _put_all(tempGvnCnt, GvnCnt);
        _put_all(tempGvnMap, GvnMap);

    }

    void GlobalArrayGVN::add(std::string str, Instr* instr) {
        if (!(GvnCnt->find(str) != GvnCnt->end())) {
            (*GvnCnt)[str] = 1;
        } else {
            (*GvnCnt)[str] = (*GvnCnt)[str] + 1;
        }
        if (!(GvnMap->find(str) != GvnMap->end())) {
            (*GvnMap)[str] = instr;
        }
    }

    void GlobalArrayGVN::remove(std::string str) {
        if (!(GvnCnt->find(str) != GvnCnt->end()) || (*GvnCnt)[str] == 0) {
            return;
        }
        (*GvnCnt)[str] = (*GvnCnt)[str] - 1;
        if ((*GvnCnt)[str] == 0) {
            GvnMap->erase(str);
        }
    }

    bool GlobalArrayGVN::addLoadToGVN(Instr* load) {
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

    void GlobalArrayGVN::removeLoadFromGVN(Instr* load) {
        //assert dynamic_cast<INSTR::Load*>(load) != nullptr;
        std::string hash = ((INSTR::Load*) load)->getPointer()->name;
        remove(hash);
    }

