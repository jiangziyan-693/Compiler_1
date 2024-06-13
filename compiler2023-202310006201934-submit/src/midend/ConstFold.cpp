//
// Created by start on 23-7-29.
//

#include "../../include/midend/ConstFold.h"
#include "../../include/util/CenterControl.h"
#include "util/HashMap.h"
#include "stl_op.h"


ConstFold::ConstFold(std::vector<Function*>* functions, std::unordered_map<GlobalValue*, Initial*>* globalValues) {
    this->functions = functions;
    this->globalValues = globalValues;
}

void ConstFold::Run() {
    condConstFold();
    arrayConstFold();
    singleBBMemoryFold();
}

void ConstFold::condConstFold() {
    for (Function* function: *functions) {
        condConstFoldForFunc(function);
    }
}

void ConstFold::condConstFoldForFunc(Function* function) {
    for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
        for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
            if (dynamic_cast<INSTR::Icmp*>(instr) != nullptr) {
                Value* lValue = instr->useValueList[0];
                Value* rValue = instr->useValueList[1];
                if (dynamic_cast<Constant*>(lValue) != nullptr && dynamic_cast<Constant*>(rValue) != nullptr) {
                    bool tag = true;
                    int lInt = (int) ((ConstantInt*) lValue)->get_const_val();
                    int rInt = (int) ((ConstantInt*) rValue)->get_const_val();
                    switch (((INSTR::Icmp*) instr)->op) {
                        case INSTR::Icmp::Op::SLT:
                            tag = lInt < rInt;
                            break;
                        case INSTR::Icmp::Op::SLE:
                            tag = lInt <= rInt;
                            break;
                        case INSTR::Icmp::Op::SGT:
                            tag = lInt > rInt;
                            break;
                        case INSTR::Icmp::Op::SGE:
                            tag = lInt >= rInt;
                            break;
                        case INSTR::Icmp::Op::NE:
                            tag = lInt != rInt;
                            break;
                        case INSTR::Icmp::Op::EQ:
                            tag = lInt == rInt;
                            break;
                        default:
                            break;
                    }
                    int val = tag? 1:0;
                    ConstantBool* _bool = new ConstantBool(val);
                    instr->modifyAllUseThisToUseA(_bool);
                }
            } else if (dynamic_cast<INSTR::Fcmp*>(instr) != nullptr) {
                Value* lValue = instr->useValueList[0];
                Value* rValue = instr->useValueList[1];
                if (dynamic_cast<Constant*>(lValue) != nullptr && dynamic_cast<Constant*>(rValue) != nullptr) {
                    bool tag = true;
                    float lFloat = (float) ((ConstantFloat*) lValue)->get_const_val();
                    float rFloat = (float) ((ConstantFloat*) rValue)->get_const_val();
                    switch (((INSTR::Fcmp*) instr)->op) {
                        case INSTR::Fcmp::Op::OLT:
                            tag = lFloat < rFloat;
                            break;
                        case INSTR::Fcmp::Op::OLE:
                            tag = lFloat <= rFloat;
                            break;
                        case INSTR::Fcmp::Op::OGT:
                            tag = lFloat > rFloat;
                            break;
                        case INSTR::Fcmp::Op::OGE:
                            tag = lFloat >= rFloat;
                            break;
                        case INSTR::Fcmp::Op::ONE:
                            tag = lFloat != rFloat;
                            break;
                        case INSTR::Fcmp::Op::OEQ:
                            tag = lFloat == rFloat;
                            break;
                        default:
                            break;
                    }
                    int val = tag? 1:0;
                    ConstantBool* _bool = new ConstantBool(val);
                    instr->modifyAllUseThisToUseA(_bool);
                }
            } else if (dynamic_cast<INSTR::Zext*>(instr) != nullptr) {
                Value* value = ((INSTR::Zext*) instr)->getRVal1();
                if (dynamic_cast<ConstantBool*>(value) != nullptr) {
                    int val = (int) ((ConstantBool*) value)->get_const_val();
                    if (instr->type->is_int32_type()) {
                        Value* afterExt = new ConstantInt(val);
                        instr->modifyAllUseThisToUseA(afterExt);
                    }
                }
            }
        }
    }
}


void ConstFold::globalConstPtrInit() {
    for (Value* value: KeySet(*globalValues)) {
        if (value->type->is_pointer_type() && globalArrayIsConst(value) &&
                !(((PointerType*) value->type)->inner_type)->is_basic_type()) {
            if (((ArrayType*) ((PointerType*) value->type)->inner_type)->dims.size() == 1 &&
                    *(((ArrayType*) ((PointerType*) value->type)->inner_type)->dims.begin()) == 3) {
                continue;
            }
            constGlobalPtrs->insert(value);
        }
    }
}

//找到没有store过的数组
bool ConstFold::globalArrayIsConst(Value* value) {
    for (Use* use = value->getBeginUse(); use->next != nullptr; use = (Use*) use->next) {
        bool ret = check(use->user);
        if (!ret) {
            return false;
        }
    }
    return true;
}

bool ConstFold::check(Instr* instr) {
    for (Use* use = instr->getBeginUse(); use->next != nullptr; use = (Use*) use->next) {
        Instr* user = use->user;
        if (dynamic_cast<INSTR::GetElementPtr*>(user) != nullptr) {
            bool ret = check(user);
            if (!ret) {
                return false;
            }
        } else if (dynamic_cast<INSTR::Store*>(user) != nullptr || dynamic_cast<INSTR::Call*>(user) != nullptr) {
            return false;
        } else if (dynamic_cast<INSTR::Load*>(user) != nullptr) {

        } else {
            //assert true;
        }
    }
    return true;
}

//TODO:check 语句只执行一次<==>语句在Main中且不在循环内
//          数组==>只store一次 写入的是constValue / value

//fixme:目前只处理全局数组,因为局部数组没有init,考虑上述问题,可以优化
void ConstFold::arrayConstFold() {
    globalConstPtrInit();
    for (Function* function: *functions) {
        arrayConstFoldForFunc(function);
    }
}

void ConstFold::arrayConstFoldForFunc(Function* function) {
    for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
        for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
            if (dynamic_cast<INSTR::GetElementPtr*>(instr) != nullptr) {
                Value* ptr = ((INSTR::GetElementPtr*) instr)->getPtr();
                if ((constGlobalPtrs->find(ptr) != constGlobalPtrs->end())) {
                    std::vector<Value*>* indexs = ((INSTR::GetElementPtr*) instr)->getIdxList();
                    bool indexAllConstTag = true;
                    for (Value* value: *indexs) {
                        if (!(dynamic_cast<Constant*>(value) != nullptr)) {
                            indexAllConstTag = false;
                        }
                    }
                    if (!indexAllConstTag) {
                        continue;
                    }
                    //gep指令的数组没有被store过,且下标为常数
                    std::vector<Value*>* ret = new std::vector<Value*>();
                    for (int i = 1; i < indexs->size(); i++) {
                        ret->push_back((*indexs)[i]);
                    }
                    Value* arrayElementVal = getGlobalConstArrayValue(ptr, ret);
                    if (arrayElementVal == nullptr) {
                        continue;
                    }
                    //此时保证GEP是可以直接load的指针
                    for (Use* use = instr->getBeginUse(); use->next != nullptr; use = (Use*) use->next) {
                        Instr* user = use->user;
                        if (!(dynamic_cast<INSTR::Load*>(user) != nullptr)) {
                            continue;
                        }
                        user->modifyAllUseThisToUseA(arrayElementVal);
                    }
                }
            }
        }
    }
}

Value* ConstFold::getGlobalConstArrayValue(Value* ptr, std::vector<Value*>* indexs) {
    std::vector<int>* lens = new std::vector<int>();
    getArraySize(((PointerType*) ptr->type)->inner_type, lens);
    Type* type = ((ArrayType*) ((PointerType*) ptr->type)->inner_type)->getBaseEleType();
    std::vector<int>* offsetArray = new std::vector<int>();
    //offsetArray->insertAll(lens);
    _vector_add_all(lens, offsetArray);

    for (int i = offsetArray->size() - 2; i >= 0; i--) {
        (*offsetArray)[i] = offsetArray->at(i + 1) * (*offsetArray)[i];
    }

    Initial* init = (*globalValues)[(GlobalValue*) ptr];
    if (indexs->size() != offsetArray->size()) {
        return nullptr;
    }
    //assert indexs->size() == offsetArray->size();
    //TODO:修改取init值的方式
    //      适配a[2][3] --> load a[0][5]


    if (dynamic_cast<ZeroInit*>(init) != nullptr) {
        if (type->is_int32_type()) {
            return new ConstantInt(0);
        } else {
            return new ConstantFloat(0);
        }
    }
    for (Value* tmp: *indexs) {
        //assert dynamic_cast<ConstantInt*>(tmp) != nullptr;
        //assert dynamic_cast<Initial->ArrayInit>(init) != nullptr;
        int index = (int) ((ConstantInt*) tmp)->get_const_val();
        init = ((ArrayInit*) init)->get(index);
        if (dynamic_cast<ZeroInit*>(init) != nullptr) {
            if (type->is_int32_type()) {
                return new ConstantInt(0);
            } else {
                return new ConstantFloat(0);
            }
        }
    }
    //assert dynamic_cast<Initial->ValueInit>(init) != nullptr;

    return ((ValueInit*) init)->value;
}

void ConstFold::getArraySize(Type *type, std::vector<int>* ret) {
    if (dynamic_cast<ArrayType*>(type) != nullptr) {
        ret->push_back(((ArrayType*) type)->size);
        getArraySize(((ArrayType*) type)->base_type, ret);
    }
}

void ConstFold::singleBBMemoryFold() {
    for (Function* function: *functions) {
        for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
            singleBBMemoryFoldForBB(bb);
        }
    }
}

void ConstFold::singleBBMemoryFoldForBB(BasicBlock* bb) {
    for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
        if (dynamic_cast<INSTR::Store*>(instr) != nullptr) {
            Value* ptr = ((INSTR::Store*) instr)->getPointer();
            Value* value = ((INSTR::Store*) instr)->getValue();
            instr = (Instr*) instr->next;
            while (instr->next != nullptr &&
                   !(dynamic_cast<INSTR::Call*>(instr) != nullptr) &&
                   !(dynamic_cast<INSTR::Store*>(instr) != nullptr)) {
                if (dynamic_cast<INSTR::Load*>(instr) != nullptr && (((INSTR::Load*) instr)->getPointer() == ptr)) {
                    instr->modifyAllUseThisToUseA(value);
                }
                instr = (Instr*) instr->next;
            }
            if (dynamic_cast<INSTR::Store*>(instr) != nullptr) {
                instr = (Instr*) instr->prev;
            }
        }
        if (instr->next == nullptr) {
            break;
        }
    }
}
