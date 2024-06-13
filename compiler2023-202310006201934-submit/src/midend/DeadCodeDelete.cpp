#include "../../include/midend/DeadCodeDelete.h"
#include "../../include/util/CenterControl.h"
#include "HashMap.h"



//TODO:naive:只被store,但是没有被load的数组/全局变量可以删除


DeadCodeDelete::DeadCodeDelete(std::vector<Function*>* functions, std::unordered_map<GlobalValue*, Initial*>* globalValues) {
    this->functions = functions;
    this->globalValues = globalValues;
}

void DeadCodeDelete::Run(){
    removeUselessRet();
    noUserCodeDelete();
    deadCodeElimination();
    if (CenterControl::_O2) {
        strongDCE();
    }
    removeUselessGlobalVal();
    removeUselessLocalArray();
    noUserCodeDelete();
    removeUselessLoop();
    //TODO:删除所有来源值都相等的PHI
    //removeUselessPhi();
}


void DeadCodeDelete::removeUselessRet() {
    for (Function* function: *functions) {
        if (function->hasRet() && retCanRemove(function)) {
            for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
                for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
                    if (dynamic_cast<INSTR::Return*>(instr) != nullptr) {
                        if (function->retType->is_int32_type()) {
                            instr->modifyUse(new ConstantInt(0), 0);
                        } else if (function->retType->is_float_type()) {
                            instr->modifyUse(new ConstantFloat(0), 0);
                        } else {
                            //assert false;
                        }
                    }
                }
            }
        }
    }
}

bool DeadCodeDelete::retCanRemove(Function* function) {
    if (function->name == ("main")) {
        return false;
    }
    for (Use* use = function->getBeginUse(); use->next != nullptr; use = (Use*) use->next) {
        Instr* call = use->user;
        for (Use* callUse = call->getBeginUse(); callUse->next != nullptr; callUse = (Use*) callUse->next) {
            Instr* instr = callUse->user;
            if (!(dynamic_cast<INSTR::Return*>(instr) != nullptr && instr->bb->function == function)) {
                return false;
            }
        }
    }
    return true;
}


void DeadCodeDelete::check_instr() {
    for (Function* function: *functions) {
        std::set<Instr*>* instrs = new std::set<Instr*>();
        for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
            for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
                instrs->insert(instr);
            }
        }
        //System->err.println(function->name);
    }
}

//TODO:对于指令闭包
// 指令闭包的算法: 必须被保留的指令  函数的return,函数调用,对全局变量的store,数组  从这些指令dfs use
//  其余全部删除
//  wait memorySSA for 数组级别数据流分析
void DeadCodeDelete::noUserCodeDelete() {
    bool changed = true;
    while(changed)
    {
        changed = false;
        for (Function* function : *functions) {
            BasicBlock* beginBB = function->getBeginBB();
            BasicBlock* end = function->end;

            BasicBlock* pos = beginBB;
            while (!(pos == end)) {

                Instr* instr = pos->getBeginInstr();
                //Instr endInst = pos->end;
                // 一个基本块至少有一条跳转指令
                try {
                    while (instr->next != nullptr) {
                        if (!(instr->isTerminator()) && dynamic_cast<INSTR::Call *>(instr) == nullptr && !(instr->type->is_void_type()) &&
                            instr->isNoUse()) {
                            instr->remove();
                            changed = true;
                        }
                        instr = (Instr*) instr->next;
                    }
                } catch (int a) {
                    //System->out.println(instr->toString());
                }

                pos = (BasicBlock*) pos->next;
            }
        }
    }
}

//fixme:对于SSA格式,这个算法貌似是多余的?
void DeadCodeDelete::deadCodeElimination() {
    for (Function* function: *functions) {
        deadCodeEliminationForFunc(function);
    }
}



void DeadCodeDelete::strongDCE() {
    for (Function* function: *functions) {
        strongDCEForFunc(function);
    }
}


//缩点,删除入度为0的点
void DeadCodeDelete::strongDCEForFunc(Function* function) {
    //check_instr();
    all_instr->clear();
    edge->clear();
//    stack->clear();
    std::stack<Instr*>().swap(*stack);
    for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
        for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
            (*edge)[instr] = new std::set<Instr*>();
            (*low)[instr] = 0;
            (*dfn)[instr] = 0;
            (*inStack)[instr] = false;
            all_instr->insert(instr);
        }
    }

    for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
        for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
            for (Value* value: instr->useValueList) {
                if (dynamic_cast<Instr*>(value) != nullptr) {
                    (*edge)[instr]->insert((Instr*) value);
                }
            }
        }
    }

    for (Instr* instr: *all_instr) {
        if ((*dfn)[instr] == 0) {
            tarjan(instr);
        }
    }

    //缩点后重新建图
    for (Instr* x: KeySet(*edge)) {
        for (Instr* y: *(*edge)[x]) {
            int color_x = (*color)[x];
            int color_y = (*color)[y];
            if (color_x != color_y) {
                (*color_in_deep)[color_y] = (*color_in_deep)[color_y] + 1;
            }
        }
    }

    for (int key: KeySet(*color_in_deep)) {
        if ((*color_in_deep)[key] == 0) {
            bool canRemove = true;
            for (Instr* instr: *(*instr_in_color)[key]) {
                if (hasEffect(instr)) {
                    canRemove = false;
                    break;
                }
            }
            if (canRemove) {
                for (Instr* instr : *(*instr_in_color)[key]) {
                    instr->remove();
                }
            }
        }
    }

}

void DeadCodeDelete::tarjan(Instr* x) {

    index++;
    (*dfn)[x] = index;
    (*low)[x] = index;
    stack->push(x);
    (*inStack)[x] = true;



    for (Instr* next: *(*edge)[x]) {
        if ((*dfn)[next] == 0) {
            tarjan(next);
            (*low)[x] = std::min(low->at(x), (*low)[next]);
        } else if ((*inStack)[next]) {
            (*low)[x] = std::min(low->at(x), (*dfn)[next]);
        }
    }
    if ((*dfn)[x] == (*low)[x]) {
        color_num++;
        (*instr_in_color)[color_num] = new std::set<Instr*>();
        (*color_in_deep)[color_num] = 0;
        while (!stack->empty()) {
            (*color)[stack->top()] = color_num;
            (*inStack)[stack->top()] = false;
            Instr* top = stack->top();
            stack->pop();
            (*instr_in_color)[color_num]->insert(top);
            if ((top == x)) {
                break;
            }
        }
    }
}

void DeadCodeDelete::deadCodeEliminationForFunc(Function* function) {
    std::set<Instr*>* know = new std::set<Instr*>();
    std::unordered_map<Instr*, std::set<Instr*>*>* closureMap = new std::unordered_map<Instr*, std::set<Instr*>*>();
    root = new std::unordered_map<Instr*, Instr*>();
    deep = new std::unordered_map<Instr*, int>();
    for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
        for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
            (*deep)[instr] = -1;
            (*root)[instr] = instr;
        }
    }

    for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
        for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
            for (Value* value: instr->useValueList) {
                if (dynamic_cast<Instr*>(value) != nullptr) {
                    Instr* x = find(instr);
                    Instr* y = find((Instr*) value);
                    if (x != (y)) {
                        union_(x, y);
                    }
                }
            }
            know->insert(instr);
        }
    }

    //构建引用闭包
    for (Instr* instr: *know) {
        Instr* root = find(instr);
        if (closureMap->find(root) == closureMap->end()) {
            (*closureMap)[root] = new std::set<Instr*>();
        }
        (*closureMap)[root]->insert(instr);
    }

    for (Instr* instr: KeySet(*closureMap)) {
        std::set<Instr*>* instrs = (*closureMap)[instr];
        bool canRemove = true;
        for (Instr* i: *instrs) {
            if (hasEffect(i)) {
                canRemove = false;
                break;
            }
        }
        if (canRemove) {
            for (Instr* i: *instrs) {
                i->remove();
            }
        }
    }

}

Instr* DeadCodeDelete::find(Instr* x) {
    if (((*root)[x] == x)) {
        return x;
    } else {
        (*root)[x] = find((*root)[x]);
        return (*root)[x];
    }
}

void DeadCodeDelete::union_(Instr* a, Instr* b) {
    if ((*deep)[b] < (*deep)[a]) {
        (*root)[a] = b;
    } else {
        if ((*deep)[a] == (*deep)[b]) {
            (*deep)[a] = (*deep)[a] - 1;
        }
        (*root)[b] = a;
    }
}

bool DeadCodeDelete::hasEffect(Instr* instr) {
    //TODO:GEP?
    return dynamic_cast<INSTR::Jump*>(instr) != nullptr || dynamic_cast<INSTR::Branch*>(instr) != nullptr
           || dynamic_cast<INSTR::Return*>(instr) != nullptr || dynamic_cast<INSTR::Call*>(instr) != nullptr
           || dynamic_cast<INSTR::Store*>(instr) != nullptr || dynamic_cast<INSTR::Load*>(instr) != nullptr;
}

//for i=1:n
//  a[i] = getint()
//return 0

void DeadCodeDelete::removeUselessGlobalVal() {
    std::set<GlobalVal*>* remove = new std::set<GlobalVal*>();
    for (GlobalVal* val: KeySet(*globalValues)) {
        if (((PointerType*) val->type)->inner_type->is_array_type()) {
            bool ret = tryRemoveUselessArray(val);
            if (ret) {
                remove->insert(val);
            }
            continue;
        }
        bool canRemove = true;
        Use* use = val->getBeginUse();
        while (use->next != nullptr) {
            if (dynamic_cast<INSTR::Store *>(use->user) == nullptr) {
                canRemove = false;
            }
            use = (Use*) use->next;
        }

        if (canRemove) {
            use = val->getBeginUse();
            while (use->next != nullptr) {
                Instr* user = use->user;
                //assert dynamic_cast<INSTR::Store*>(user) != nullptr;
                deleteInstr(user, false);
                use = (Use*) use->next;
            }
            remove->insert(val);
        }
    }
    for (Value* value: *remove) {
        globalValues->erase((GlobalValue*) value);
    }
}

void DeadCodeDelete::deleteInstr(Instr* instr, bool tag) {
    if (tag && hasEffect(instr)) {
        return;
    }
    for (Value* value: instr->useValueList) {
        if (dynamic_cast<Instr*>(value) != nullptr && value->onlyOneUser()) {
            deleteInstr((Instr*) value, true);
        }
    }
    instr->remove();
}

void DeadCodeDelete::removeUselessLocalArray() {
    for (Function* function: *functions) {
        for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
            for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
                if (dynamic_cast<INSTR::Alloc*>(instr) != nullptr) {
                    tryRemoveUselessArray(instr);
                }
            }
        }
    }
}


//删除无用的全局/局部数组 global-init local-alloc
bool DeadCodeDelete::tryRemoveUselessArray(Value* value) {
    std::set<Instr*>* instrs = new std::set<Instr*>();
    bool ret = check(value, instrs);
    if (ret) {
        for (Instr* instr: *instrs) {
            instr->remove();
        }
    }
    return ret;
}

bool DeadCodeDelete::check(Value* value, std::set<Instr*>* know) {
    if (dynamic_cast<Instr*>(value) != nullptr) {
        know->insert((Instr*) value);
    }
    for (Use* use = value->getBeginUse(); use->next != nullptr; use = (Use*) use->next) {
        Instr* user = use->user;
        if (dynamic_cast<INSTR::GetElementPtr*>(user) != nullptr) {
            bool ret = check(user, know);
            if (!ret) {
                return false;
            }
        } else {
            //只被store 而且store所用的值没有effect
            if (dynamic_cast<INSTR::Store*>(user) != nullptr) {
                Value* storeValue = user->useValueList[0];
                know->insert(user);
            } else if (dynamic_cast<INSTR::Call*>(user) != nullptr) {
                know->insert(user);
                Value* val = user->useValueList[0];
                if (dynamic_cast<Function*>(val) != nullptr) {
                    if (val->getName() != ("memset")) {
                        return false;
                    }
                }
            } else {
                return false;
            }
        }
    }
    return true;
}

void DeadCodeDelete::removeUselessLoop() {
    for (Function* function: *functions) {
        Loop* loop = function->getBeginBB()->loop;
        tryRemoveLoop(loop);
    }
}

bool DeadCodeDelete::tryRemoveLoop(Loop* loop) {
    //return false;
    std::set<Loop*>* removes = new std::set<Loop*>();
    for (Loop* next: (*loop->childrenLoops)) {
        bool ret = tryRemoveLoop(next);
        if (ret) {
            removes->insert(next);
        }
    }

    for (Loop* loop1: *removes) {
        loop->getChildrenLoops()->erase(loop1);
    }

    if (loopCanRemove(loop)) {
        //TODO:REMOVE
        std::set<BasicBlock*>* enterings = loop->getEnterings();
        std::set<BasicBlock*>* exits = loop->getExits();
        std::vector<BasicBlock*>* pres = new std::vector<BasicBlock*>();
        BasicBlock* head = loop->getHeader();
        BasicBlock* exit = nullptr;
        for (BasicBlock* bb: *exits) {
            exit = bb;
        }
        for (BasicBlock* entering: *enterings) {
            entering->modifyBrAToB(head, exit);
            entering->modifySuc(head, exit);
            pres->push_back(entering);
        }
        exit->modifyPres(pres);
        std::set<BasicBlock*>* bbRemove = new std::set<BasicBlock*>();
        for (BasicBlock* bb: *loop->getNowLevelBB()) {
            for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
                instr->remove();
            }
            bbRemove->insert(bb);
        }
        //fixme:是否需要删除bb里的instr
        for (BasicBlock* bb: *bbRemove) {
            bb->remove();
        }
        return true;
    }
    return false;
}

bool DeadCodeDelete::loopCanRemove(Loop* loop) {
    if (loop->getChildrenLoops()->size() != 0) {
        return false;
    }
    if (loop->getExits()->size() != 1) {
        return false;
    }
    BasicBlock* exit = nullptr;
    for (BasicBlock* bb: *loop->getExits()) {
        exit = bb;
    }
    if (dynamic_cast<INSTR::Phi*>(exit->getBeginInstr()) != nullptr) {
        return false;
    }
    //TODO:trivial 待强化
    if (loop->getExitings()->size() != exit->precBBs->size()) {
        return false;
    }
    for (BasicBlock* bb: *loop->getNowLevelBB()) {
        for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
            if (hasStrongEffect(instr)) {
                return false;
            }
            for (Use* use = instr->getBeginUse(); use->next != nullptr; use = (Use*) use->next) {
                Instr* user = use->user;
                if (user->bb->loop != loop || hasStrongEffect(user)) {
                    return false;
                }
            }
        }
    }
    return true;
}

bool DeadCodeDelete::hasStrongEffect(Instr* instr) {
    return dynamic_cast<INSTR::Return*>(instr) != nullptr || dynamic_cast<INSTR::Call*>(instr) != nullptr
           || dynamic_cast<INSTR::Store*>(instr) != nullptr || dynamic_cast<INSTR::Load*>(instr) != nullptr;
}



