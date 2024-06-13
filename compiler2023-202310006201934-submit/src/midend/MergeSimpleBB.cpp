#include "../../include/midend/MergeSimpleBB.h"
#include "../../include/util/CenterControl.h"



	MergeSimpleBB::MergeSimpleBB(std::vector<Function*>* functions) {
        this->functions = functions;
    }

    void MergeSimpleBB::Run() {
        for (Function* function: *functions) {
            for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
                if ((bb->getBeginInstr() == bb->getEndInstr()) && dynamic_cast<INSTR::Jump*>(bb->getBeginInstr()) != nullptr) {
                    BasicBlock* suc = (*bb->succBBs)[0];
                    suc->precBBs->erase(
                            std::remove(suc->precBBs->begin(), suc->precBBs->end(), bb),
                            suc->precBBs->end());
                    for (BasicBlock* pre: *bb->precBBs) {
                        pre->modifyBrAToB(bb, suc);
                        pre->modifySuc(bb, suc);
                        suc->addPre(pre);
                    }
                    bb->getBeginInstr()->remove();
                    bb->remove();
                }
            }
        }
        RemoveDeadBB();
        MakeCFG();
    }

    void MergeSimpleBB::RemoveDeadBB() {
        //TODO:优化删除基本块的算法
        for (Function* function: *functions) {
            removeFuncDeadBB(function);
        }
    }

    void MergeSimpleBB::MakeCFG() {
        for (Function* function: *functions) {
            makeSingleFuncCFG(function);
        }
    }

    void MergeSimpleBB::removeFuncDeadBB(Function* function) {
        BasicBlock* beginBB = function->getBeginBB();
        BasicBlock* end = function->end;

        preMap = new std::unordered_map<BasicBlock*, std::vector<BasicBlock*>*>();
        sucMap = new std::unordered_map<BasicBlock*, std::vector<BasicBlock*>*>();
        std::set<BasicBlock*>* BBs = new std::set<BasicBlock*>();

        //初始化前驱后继图
        BasicBlock* pos = beginBB;
        while (!(pos == end)) {
            (*preMap)[pos] = new std::vector<BasicBlock*>();
            (*sucMap)[pos] = new std::vector<BasicBlock*>();
            BBs->insert(pos);
            pos = (BasicBlock*) pos->next;

            //remove useless br
            Instr* instr = pos->getEndInstr();
            while (dynamic_cast<INSTR::Branch*>(instr->prev) != nullptr || dynamic_cast<INSTR::Jump*>(instr->prev) != nullptr) {
                Instr* temp = instr;
                temp->remove();
                instr = (Instr*) instr->prev;
            }
        }

        //添加前驱和后继
        pos = beginBB;
        while (!(pos == end)) {
            Instr* lastInstr = pos->getEndInstr();
            if (dynamic_cast<INSTR::Branch*>(lastInstr) != nullptr) {
                BasicBlock* elseTarget = ((INSTR::Branch*) lastInstr)->getElseTarget();
                BasicBlock* thenTarget = ((INSTR::Branch*) lastInstr)->getThenTarget();
                (*sucMap)[pos]->push_back(thenTarget);
                (*sucMap)[pos]->push_back(elseTarget);
                (*preMap)[thenTarget]->push_back(pos);
                (*preMap)[elseTarget]->push_back(pos);
            } else if (dynamic_cast<INSTR::Jump*>(lastInstr) != nullptr) {
                BasicBlock* target = ((INSTR::Jump*) lastInstr)->getTarget();
                (*sucMap)[pos]->push_back(target);
                (*preMap)[target]->push_back(pos);
            }
            pos = (BasicBlock*) pos->next;
        }

        //回写基本块和函数
        std::set<BasicBlock*>* needRemove = new std::set<BasicBlock*>();
        std::set<BasicBlock*>* know = new std::set<BasicBlock*>();
        DFS(function->getBeginBB(), know);

        pos = beginBB;
        while (!(pos == end)) {
            if (!(know->find(pos) != know->end())) {
                needRemove->insert(pos);
            }
            pos = (BasicBlock*) pos->next;
        }


        for (BasicBlock* bb: *needRemove) {
            //System->err.println("remove:" + bb->getLabel());
            bb->remove();
            Instr* instr = bb->getBeginInstr();
            while (instr->next != nullptr) {
                instr->remove();
                instr = (Instr*) instr->next;
            }
        }
    }

    void MergeSimpleBB::DFS(BasicBlock* bb, std::set<BasicBlock*>* know) {
        if ((know->find(bb) != know->end())) {
            return;
        }
        know->insert(bb);
        for (BasicBlock* next: *(*sucMap)[bb]) {
            DFS(next, know);
        }
    }

    //计算单个函数的控制流图
    void MergeSimpleBB::makeSingleFuncCFG(Function* function) {
        BasicBlock* beginBB = function->getBeginBB();
        BasicBlock* end = function->end;

        preMap = new std::unordered_map<BasicBlock*, std::vector<BasicBlock*>*>();
        sucMap = new std::unordered_map<BasicBlock*, std::vector<BasicBlock*>*>();
        std::set<BasicBlock*>* BBs = new std::set<BasicBlock*>();

        //初始化前驱后继图
        BasicBlock* pos = beginBB;
        while (!(pos == end)) {
            (*preMap)[pos] = new std::vector<BasicBlock*>();
            (*sucMap)[pos] = new std::vector<BasicBlock*>();
            BBs->insert(pos);
            pos = (BasicBlock*) pos->next;
        }

        //添加前驱和后继
        pos = beginBB;
        while (!(pos == end)) {
            Instr* lastInstr = pos->getEndInstr();
            if (dynamic_cast<INSTR::Branch*>(lastInstr) != nullptr) {
                BasicBlock* elseTarget = ((INSTR::Branch*) lastInstr)->getElseTarget();
                BasicBlock* thenTarget = ((INSTR::Branch*) lastInstr)->getThenTarget();
                (*sucMap)[pos]->push_back(thenTarget);
                (*sucMap)[pos]->push_back(elseTarget);
                (*preMap)[thenTarget]->push_back(pos);
                (*preMap)[elseTarget]->push_back(pos);
            } else if (dynamic_cast<INSTR::Jump*>(lastInstr) != nullptr) {
                BasicBlock* target = ((INSTR::Jump*) lastInstr)->getTarget();
                (*sucMap)[pos]->push_back(target);
                (*preMap)[target]->push_back(pos);
            }
            pos = (BasicBlock*) pos->next;
        }

        //回写基本块和函数
        pos = beginBB;
        while (!(pos == end)) {
            pos->modifyPres((*preMap)[pos]);
            pos->modifySucs((*sucMap)[pos]);
            pos = (BasicBlock*) pos->next;
        }
        function->preMap = (preMap);
        function->sucMap = (sucMap);
        function->BBs = (BBs);
    }
