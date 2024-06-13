#include "../../include/midend/MemSetOptimize.h"
#include "../../include/util/CenterControl.h"
#include "stl_op.h"
#include "ArrayGVNGCM.h"

#include "Manager.h"



//只针对全局数组
//局部数组使用ArraySSA
//    std::unordered_map<std::string, Instr*>* GvnMap = new std::unordered_map<std::string, Instr*>();
//    std::unordered_map<std::string, int>* GvnCnt = new std::unordered_map<std::string, int>();
//    std::set<Loop*>* allLoop = new std::set<Loop*>();
//HashSet<Loop*> oneDimLoop = new std::set<>();

MemSetOptimize::MemSetOptimize(std::vector<Function *> *functions,
                               std::unordered_map<GlobalValue *, Initial *> *globalValues) {
    this->functions = functions;
//        this->idcPhis = new std::set<>();
    this->globalValues = globalValues;
}

void MemSetOptimize::Run() {
    init();
    globalArrayFold();
    //initLoopToMemSet();
}

void MemSetOptimize::init() {
    for (Function *function: *functions) {
        std::set<Loop *> *loops = new std::set<Loop *>();
        getLoops(function->getBeginBB()->loop, loops);
        for (Loop *loop: *loops) {
            if (loop->getLoopDepth() == 1) {
                //
                idcPhis->clear();
                if (checkArrayInit(loop)) {
                    if (dynamic_cast<GlobalVal *>(loop->getInitArray()) != nullptr) {
                        globalArrayInitLoops->insert(loop);
                    } else {
                        localArrayInitLoops->insert(loop);
                    }
                }
            }
        }
    }
}

//优化mm,mv
void MemSetOptimize::globalArrayFold() {
    for (Loop *loop: *globalArrayInitLoops) {
        Value *initValue = loop->getInitValue();
        Value *initArray = loop->getInitArray();
        std::set<Function *> *userFuncs = new std::set<Function *>();
        for (Use *use = initArray->getBeginUse(); use->next != nullptr; use = (Use *) use->next) {
            userFuncs->insert(use->user->bb->function);
        }
        bool useInOneFunc = userFuncs->size() == 1;
        Function *function = nullptr;
        for (Function *function1: *userFuncs) {
            function = function1;
        }
        //TODO:下面条件限制的非常死
        // 待优化
        if (useInOneFunc && loop->getEnterings()->size() == 1 &&
            _contains_(loop->getEnterings(), function->getBeginBB())) {
            canGVN->clear();
            bool ret = check(initArray);
            if (ret) {
                //globalArrayGVN(initArray, function);
                ArrayGVNGCM *arrayGVNGCM = new ArrayGVNGCM(function, canGVN);
                arrayGVNGCM->Run();
            }
        }
    }
}

void MemSetOptimize::initLoopToMemSet() {
    if (functions->size() > 1) {
        return;
    }
    Function *function = (*functions)[0];
    for (BasicBlock *bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock *) bb->next) {
        allLoop->insert(bb->loop);
    }
    for (Loop *loop: *allLoop) {
        if (loop->getChildrenLoops()->size() == 0) {
            idcPhis->clear();
            checkArrayInit(loop);
        }
    }

    std::set<BasicBlock *> *removes = new std::set<BasicBlock *>();
    for (Loop *loop: *allLoop) {
        if (!loop->isSimpleLoop()) {
            continue;
        }
        if (!loop->isArrayInit) {
            continue;
        }
        Value *value = loop->getInitValue();
        Value *array = loop->getInitArray();
        if (!(dynamic_cast<Constant *>(value) != nullptr && ((int) ((ConstantInt *) value)->get_const_val()) == 0)) {
            continue;
        }
        if (dynamic_cast<ArrayType *>(((PointerType *) array->type)->inner_type) != nullptr &&
            ((ArrayType *) ((PointerType *) array->type)->inner_type)->getDimSize() == 1 &&
            loop->getEnterings()->size() == 1) {
            std::set<BasicBlock *> *enterings = loop->getEnterings();
            BasicBlock *entering = nullptr;
            for (BasicBlock *bb: *enterings) {
                entering = bb;
            }
            if (!(dynamic_cast<ConstantInt *>(loop->getIdcInit()) != nullptr)) {
                continue;
            }
            if (!(((int) ((ConstantInt *) loop->getIdcInit())->get_const_val()) == 0)) {
                continue;
            }
            if (((INSTR::Icmp *) loop->getIdcCmp())->op != (INSTR::Icmp::Op::SLT)) {
                continue;
            }
            if (!(dynamic_cast<ConstantInt *>(loop->getIdcStep()) != nullptr)) {
                continue;
            }
            if (!(((int) ((ConstantInt *) loop->getIdcStep())->get_const_val()) == 1)) {
                continue;
            }
            bool tag = true;
            for (Instr *instr: *loop->getExtras()) {
                for (Use *use = instr->getBeginUse(); use->next != nullptr; use = (Use *) use->next) {
                    if (use->user->bb->loop != (loop)) {
                        tag = false;
                        break;
                    }
                }
            }
            if (!tag) {
                continue;
            }
            Type *type = ((ArrayType *) ((PointerType *) array->type)->inner_type)->base_type;
            std::vector<Value *> *params = new std::vector<Value *>();
            std::vector<Value *> *indexs = new std::vector<Value *>();
            indexs->push_back(new ConstantInt(0));
            indexs->push_back(new ConstantInt(0));
            //只考虑从下标0开始,只考虑整数下标
            //fixme:计算长度,而不采用数组总长度
            Instr *enteringBr = entering->getEndInstr();
            INSTR::GetElementPtr *gep = new INSTR::GetElementPtr(type, array, indexs, entering);
            INSTR::Alu *offset = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::MUL, loop->getIdcEnd(),
                                                new ConstantInt(4), entering);
            params->push_back(gep);
            params->push_back(new ConstantInt(0));
            params->push_back(offset);
            INSTR::Call *memset = new INSTR::Call(ExternFunction::MEM_SET, params, entering);
            enteringBr->insert_before(gep);
            enteringBr->insert_before(offset);
            enteringBr->insert_before(memset);
            //fixme:直接采用数组总长度


            std::set<BasicBlock *> *exits = loop->getExits();
            BasicBlock *exit = nullptr;
            for (BasicBlock *bb: *exits) {
                exit = bb;
            }
            std::set<BasicBlock *> *exitings = loop->getExitings();
            BasicBlock *exiting = nullptr;
            for (BasicBlock *bb: *exitings) {
                exiting = bb;
            }
            entering->modifyBrAToB(loop->getHeader(), exit);
            entering->modifySuc(loop->getHeader(), exit);
            exit->modifyPre(exiting, entering);

            for (BasicBlock *bb: *loop->getNowLevelBB()) {
                removes->insert(bb);
            }
        }
    }
    for (BasicBlock *bb: *removes) {
        for (Instr *instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr *) instr->next) {
            instr->remove();
        }
        bb->remove();
    }
}


bool MemSetOptimize::check(Value *array) {
    for (Use *use = array->getBeginUse(); use->next != nullptr; use = (Use *) use->next) {
        if (dynamic_cast<INSTR::Store *>(use->user) != nullptr && !use->user->isArrayInit()) {
            return false;
        }
        if (dynamic_cast<INSTR::GetElementPtr *>(use->user) != nullptr) {
            bool ret = check(use->user);
            if (!ret) {
                return false;
            }
        }
        if (dynamic_cast<INSTR::Load *>(use->user) != nullptr) {
            canGVN->insert(use->user);
        }
    }
    return true;
}


void MemSetOptimize::getLoops(Loop *loop, std::set<Loop *> *loops) {
    loops->insert(loop);
    for (Loop *next: *loop->getChildrenLoops()) {
        getLoops(next, loops);
    }
}

bool MemSetOptimize::checkArrayInit(Loop *loop) {
    if (!loop->isSimpleLoop() || !loop->isIdcSet()) {
        return false;
    }
    if (loop->hasChildLoop() && loop->getChildrenLoops()->size() != 1) {
        //只有一个内部循环
        return false;
    }
    std::set<Instr *> *instrs = new std::set<Instr *>();
    instrs->insert(loop->getHeader()->getEndInstr());
    instrs->insert((Instr *) loop->getIdcPHI());
    instrs->insert((Instr *) loop->getIdcAlu());
    instrs->insert((Instr *) loop->getIdcCmp());

    idcPhis->insert((Instr *) loop->getIdcPHI());

    if (!loop->hasChildLoop()) {
        //没有子循环
        std::set<Instr *> *extras = new std::set<Instr *>();
        for (BasicBlock *bb: *loop->getNowLevelBB()) {
            for (Instr *instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr *) instr->next) {
                if (!(dynamic_cast<INSTR::Jump *>(instr) != nullptr) && !(instrs->find(instr) != instrs->end())) {
                    extras->insert(instr);
                }
            }
        }
        int getint_cnt = 0, gep_cnt = 0, store_cnt = 0;
        INSTR::Call *getint = nullptr;
        INSTR::GetElementPtr *gep = nullptr;
        INSTR::Store *store = nullptr;
        for (Instr *instr: *extras) {
            if (dynamic_cast<INSTR::Call *>(instr) != nullptr &&
                ((INSTR::Call *) instr)->getFunc()->name == ("getint")) {
                getint_cnt++;
                getint = (INSTR::Call *) instr;
            } else if (dynamic_cast<INSTR::GetElementPtr *>(instr) != nullptr) {
                gep_cnt++;
                gep = (INSTR::GetElementPtr *) instr;
            } else if (dynamic_cast<INSTR::Store *>(instr) != nullptr) {
                store_cnt++;
                store = (INSTR::Store *) instr;
            } else {
                return false;
            }
        }
        if (gep_cnt != 1) {
            return false;
        }
        std::vector<Value *> *indexs = gep->getIdxList();
        //TODO:下面条件限制的非常死,对于函数调用中传参数组的初始化不友好
        // 待优化
        if (idcPhis->size() + 1 != indexs->size()) {
            return false;
        }
        if (dynamic_cast<Constant *>((*indexs)[0]) != nullptr &&
            ((int) ((ConstantInt *) (*indexs)[0])->get_const_val()) == 0) {
//                for (Value* index: indexs->subList(1, indexs->size())) {
//                    if (!(idcPhis->find(index) != idcPhis->end())) {
//                        return false;
//                    }
//                }
            for (int i = 1; i < indexs->size(); ++i) {
                Value *index = (*indexs)[i];
                if (!(idcPhis->find((Instr *) index) != idcPhis->end())) {
                    return false;
                }
            }
        } else {
            return false;
        }
        if (store_cnt == 1) {
            Value *ptr = store->getPointer();
            if (ptr != (gep)) {
                return false;
            }
            Value *initArray = gep->getPtr();
            Value *initValue = store->getValue();
            if (getint_cnt == 1) {
                if (initValue != (getint)) {
                    return false;
                }
            }
            gep->setArrayInit(true);
            store->setArrayInit(true);
            loop->setArrayInitInfo(1, initArray, initValue, extras);
            return true;
        } else {
            return false;
        }

    } else {
        //有子循环且子循环数量为1
        for (BasicBlock *bb: *loop->getNowLevelBB()) {
            for (Instr *instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr *) instr->next) {
                if (!(dynamic_cast<INSTR::Jump *>(instr) != nullptr) && !(instrs->find(instr) != instrs->end())) {
                    return false;
                }
            }
        }
        Loop *next = nullptr;
        for (Loop *temp: *loop->getChildrenLoops()) {
            next = temp;
        }
        //assert next != nullptr;
        bool ret = checkArrayInit(next);
        if (ret) {
            loop->setArrayInitInfo(next->getArrayInitDims() + 1, next->getInitArray(), next->getInitValue(),
                                   next->getExtras());
            return true;
        }
        return false;
    }
}


