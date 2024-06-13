#include "../../include/midend/FuncInline.h"
#include "../../include/util/CenterControl.h"
#include "HashMap.h"
#include <algorithm>
#include <cassert>



//TODO: 不能递归
//TODO: 寻找函数调用链
//TODO: 递归的内联


//fixme:考虑库函数如何维持正确性
//      因为建立反向图的时候,只考虑了自定义的函数

//TODO:如果存在调用:
// A -> A
// B -> A
// C -> B
// 在当前判断中 A B C 入度均为1,无法内联
// 但是,其实可以把B内联到C里,
// 所以可以内联的条件可以加强为:对于一个函数,如果入度为0/入度不为0,但是所有的入边对应的函数,均只存在自调用


//A调用B则存在B->A
//记录反图的入度
//A调用B则存在A->B


FuncInline::FuncInline(std::vector<Function*>* functions) {
    this->functions = functions;
}

void FuncInline::Run() {
    GetFuncCanInline();
    for (Function* function: *funcCanInline) {
        if ((funcCallSelf->find(function) != funcCallSelf->end())) {
            continue;
        }
//        if (function->getName() != "hash") {
//            continue;
//        }
//        if (function->getName() == "insert") {
//            continue;
//        }
//        if (function->getName() == "reduce") {
//            printf("inline reduce");
//        }
        inlineFunc(function);
        functions->erase(
                std::remove(functions->begin(), functions->end(), function),
                functions->end());
        for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
            for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
                instr->remove();
            }
        }
        function->is_deleted = true;
    }
    //System->err.println("fun_inline_end");
}

void FuncInline::GetFuncCanInline() {
    makeReserveMap();
    if (CenterControl::_STRONG_FUNC_INLINE) {
        topologySortStrong();
    } else {
        topologySort();
    }
}

//f1调用f2 添加一条f2到f1的边
void FuncInline::makeReserveMap() {
    for (Function* function: *functions) {
        (*Map)[function] = new std::set<Function*>();
    }
    for (Function* function: *functions) {
        (*reserveMap)[function] = new std::set<Function*>();
        if (!(inNum->find(function) != inNum->end())) {
            (*inNum)[function] = 0;
        }
        Use* use = function->getBeginUse();
        while (use->next != nullptr) {
            Function* userFunc = use->user->bb->function;
            bool ret = ((*reserveMap)[function]->find(userFunc) == (*reserveMap)[function]->end());
            (*reserveMap)[function]->insert(userFunc);
            (*Map)[userFunc]->insert(function);
            if (!(inNum->find(userFunc) != inNum->end())) {
                (*inNum)[userFunc] = 0;
            }
            if (ret) {
                (*inNum)[userFunc] = (*inNum)[userFunc] + 1;
            }
            use = (Use*) use->next;
        }
    }
}

void FuncInline::topologySortStrong() {
    for (Function* function: KeySet(*inNum)) {
        if ((*inNum)[function] == 0 && function->name != ("main")) {
            queue->push(function);
        } else if ((*inNum)[function] == 1 && (*(*Map)[function]->begin() == function)) {
            queue->push(function);
            funcCallSelf->insert(function);
        }
    }
    while (!queue->empty()) {
        Function* pos = queue->front();
        funcCanInline->push_back(pos);
        for (Function* next: *(*reserveMap)[pos]) {
            if ((std::find(funcCanInline->begin(), funcCanInline->end(), next) != funcCanInline->end())) {
                continue;
            }
            (*inNum)[next] = (*inNum)[next] - 1;
            //assert (*Map)[next]->contains(pos);
            (*Map)[next]->erase(pos);
            if ((*inNum)[next] == 0 && next->name != ("main")) {
                queue->push(next);
            } else if ((*inNum)[next] == 1 && (*(*Map)[next]->begin() == next)) {
                queue->push(next);
                funcCallSelf->insert(next);
            }
        }
        queue->pop();
    }
}

void FuncInline::topologySort() {
    for (Function* function: KeySet(*inNum)) {
        if ((*inNum)[function] == 0 && function->name != ("main")) {
            queue->push(function);
        }
    }
    while (!queue->empty()) {
        Function* pos = queue->front();
        funcCanInline->push_back(pos);
        for (Function* next: *(*reserveMap)[pos]) {
            (*inNum)[next] = (*inNum)[next] - 1;
            if ((*inNum)[next] == 0 && next->name != ("main")) {
                queue->push(next);
            }
        }
        queue->pop();
    }
}

bool FuncInline::canInline(Function* function) {
    BasicBlock* bb = function->getBeginBB();
    while (bb->next != nullptr) {
        Instr* instr = bb->getBeginInstr();
        while (instr->next != nullptr) {
            if (dynamic_cast<INSTR::Call*>(instr) != nullptr) {
                return false;
            }
            instr = (Instr*) instr->next;
        }
        bb = (BasicBlock*) bb->next;
    }
    return true;
}

void FuncInline::inlineFunc(Function* function) {
    Use* use = function->getBeginUse();
    while (use->next != nullptr) {
        //dynamic_cast<INSTR::Call*>(assert use->user) != nullptr;
        transCallToFunc(function, (INSTR::Call*) use->user);
        use = (Use*) use->next;
    }
}

void FuncInline::transCallToFunc(Function* function, INSTR::Call* call) {
    if (dynamic_cast<INSTR::Phi*>(function->getBeginBB()->getBeginInstr()) != nullptr) {
        //System->err.println("phi_in_func_begin_BB");
    }
    CloneInfoMap::clear();
    assert(CloneInfoMap::valueMap.empty());
    assert(CloneInfoMap::loopMap.empty());
    assert(CloneInfoMap::loopCondCntMap.empty());

    Function* inFunction = call->bb->function;
    BasicBlock* beforeCallBB = call->bb;

    std::vector<BasicBlock*>* sucs_afterCall = beforeCallBB->succBBs;

    Instr* instr = beforeCallBB->getBeginInstr();
    //原BB中需要保留的instr
    while (instr->next != nullptr) {
        if ((instr == call)) {
            break;
        }
        instr = (Instr*) instr->next;
    }


    BasicBlock* retBB = new BasicBlock(inFunction, beforeCallBB->loop);
    if (!function->retType->is_void_type()) {
        //Instr retPhi = new INSTR::Phi(call->getType(), new std::vector<>(), retBB);
        Instr* retPhi = new INSTR::Phi(function->retType, new std::vector<Value*>(), retBB);
        instr->modifyAllUseThisToUseA(retPhi);
    }

    function->inlineToFunc(inFunction, retBB, call, beforeCallBB->loop);

    //instr->cloneToBB(callBB);
    instr->remove();
    instr = (Instr*) instr->next;
    instr->prev->set_next(beforeCallBB->end);
    beforeCallBB->end->set_prev(instr->prev);




    BasicBlock* afterCallBB = new BasicBlock(inFunction, beforeCallBB->loop);
    std::vector<Instr*>* instrs = new std::vector<Instr*>();
    while (instr->next != nullptr) {
        instrs->push_back(instr);
        instr = (Instr*) instr->next;
    }
    for (Instr* instr1: *instrs) {
        instr1->bb = afterCallBB;
        afterCallBB->insertAtEnd(instr1);
    }


    BasicBlock* funcBegin = (BasicBlock*) CloneInfoMap::getReflectedValue(function->getBeginBB());


    std::vector<BasicBlock*>* sucs_beforeCall = new std::vector<BasicBlock*>();
    sucs_beforeCall->push_back(funcBegin);
    beforeCallBB->modifySucs(sucs_beforeCall);

    std::vector<BasicBlock*>* pres_funcBegin = new std::vector<BasicBlock*>();
    pres_funcBegin->push_back(beforeCallBB);
    funcBegin->modifyPres(pres_funcBegin);

    Instr* jumpToCallBB = new INSTR::Jump((BasicBlock*) CloneInfoMap::getReflectedValue(function->getBeginBB()), beforeCallBB);
    Instr* jumpToAfterCallBB = new INSTR::Jump(afterCallBB, retBB);

    std::vector<BasicBlock*>* succ_retBB = new std::vector<BasicBlock*>();
    succ_retBB->push_back(afterCallBB);
    retBB->modifySucs(succ_retBB);

    std::vector<BasicBlock*>* pres_afterCall = new std::vector<BasicBlock*>();
    pres_afterCall->push_back(retBB);
    afterCallBB->modifySucs(sucs_afterCall);
    afterCallBB->modifyPres(pres_afterCall);

    for (BasicBlock* bb: *sucs_afterCall) {
        bb->modifyPre(beforeCallBB, afterCallBB);
    }
}

