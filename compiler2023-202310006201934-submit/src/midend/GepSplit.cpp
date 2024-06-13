//
// Created by start on 23-7-29.
//


#include "../../include/midend/GepSplit.h"
#include "../../include/util/CenterControl.h"



GepSplit::GepSplit(std::vector<Function*>* functions) {
    this->functions = functions;
}

void GepSplit::Run() {
    _GepSplit();
    RemoveUselessGep();
}

void GepSplit::_GepSplit() {
    for (Function* function: *functions) {
        gepSplitForFunc(function);
    }
}

//TODO:GEP最终的格式?
//  %v30 = getelementptr inbounds [3 x [4 x [5 x i32]]], [3 x [4 x [5 x i32]]]* %f1, i32 1, i32 2
// ----------------
// 是否需要保证index[0] == 0 ==> choose
// %v30 = getelementptr inbounds [3 x [4 x [5 x i32]]], [3 x [4 x [5 x i32]]]* %f1, i32 %1
// %v31 = getelementptr inbounds [3 x [4 x [5 x i32]]], [3 x [4 x [5 x i32]]]* %30, i32 0, i32 %2
// 不允许强制类型转换
// %v30 = getelementptr inbounds [3 x [4 x [5 x i32]]], [3 x [4 x [5 x i32]]]* %f1, i32 1
// %v31 = getelementptr inbounds [4 x [5 x i32]], [4 x [5 x i32]]* %30, i32 2
void GepSplit::gepSplitForFunc(Function* function) {
    for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
        for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
            if (dynamic_cast<INSTR::GetElementPtr*>(instr) != nullptr && ((INSTR::GetElementPtr*) instr)->getIdxList()->size() >= 2) {
                split((INSTR::GetElementPtr*) instr);
            }
        }
    }
}

void GepSplit::split(INSTR::GetElementPtr* gep) {
    BasicBlock* bb = gep->bb;
//    ArrayList<Value> indexs = new ArrayList<>(gep.getIdxList().subList(1, gep.getIdxList().size()));
    std::vector<Value*>* indexs = new std::vector<Value*>();
    auto idxList = gep->getIdxList();
    for (int i = 1; i < idxList->size(); i++) {
        indexs->push_back((*idxList)[i]);
    }

    Value* ptr = gep->getPtr();
    Instr* pos = gep;
    Value* pre = (*gep->getIdxList())[0];
    std::vector<Value*>* preIndexs = new std::vector<Value*>();
    preIndexs->push_back(pre);
    if (gep->getIdxList()->size() == 2) {
        if (dynamic_cast<Constant*>(pre) != nullptr && ((int) ((ConstantInt*) pre)->get_const_val()) == 0) {
            return;
        }
    }
    Instr* preOffset = new INSTR::GetElementPtr(((PointerType*) ptr->type)->inner_type, ptr, preIndexs, bb);
    pos->insert_after(preOffset);
    pos = preOffset;
    ptr = preOffset;
    for (Value* index: *indexs) {
        std::vector<Value*>* tempIndexs = new std::vector<Value*>();
        tempIndexs->push_back(new ConstantInt(0));
        tempIndexs->push_back(index);
        INSTR::GetElementPtr* temp = new INSTR::GetElementPtr(((ArrayType*) ((PointerType*) ptr->type)->inner_type)->base_type, ptr, tempIndexs, bb);
        pos->insert_after(temp);
        pos = temp;
        ptr = temp;
    }
    gep->modifyAllUseThisToUseA(pos);
}


void GepSplit::RemoveUselessGep() {
    for (Function* function: *functions) {
        //删除冗余GEP
        for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
            for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
                if (dynamic_cast<INSTR::GetElementPtr*>(instr) != nullptr && isZeroOffsetGep((INSTR::GetElementPtr*) instr)) {
                    instr->modifyAllUseThisToUseA(((INSTR::GetElementPtr*) instr)->getPtr());
                }
            }
        }
    }
}

bool GepSplit::isZeroOffsetGep(INSTR::GetElementPtr* gep) {
    std::vector<Value*>* values = gep->getIdxList();
    if (values->size() > 1) {
        return false;
    }
    for (Value* value: *values) {
        if (!(dynamic_cast<Constant*>(value) != nullptr)) {
            return false;
        }
        int val = (int) ((ConstantInt*) value)->get_const_val();
        if (val != 0) {
            return false;
        }
    }
    return true;
}
