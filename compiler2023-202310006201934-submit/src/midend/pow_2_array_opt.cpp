#include "pow_2_array_opt.h"
#include "stl_op.h"
#include "HashMap.h"
//
// Created by start on 23-8-11.
//
pow_2_array_opt::pow_2_array_opt(std::vector<Function *> *functions) {
    this->functions = functions;
}

void pow_2_array_opt::Run() {
    init();
}

bool pow_2_array_opt::is_pow_2_array_alloc(Instr* alloc) {
    if (dynamic_cast<INSTR::Alloc*>(alloc) == nullptr) {
        return false;
    }
    if (!((PointerType*) alloc->type)->inner_type->is_array_type()) {
        return false;
    }
    ArrayType* t = (ArrayType*) ((PointerType*) alloc->type)->inner_type;
    if (t->dims.size() != 1) {
        return false;
    }
    int len = *(t->dims.begin());
    if (len > 100) {
        return false;
    }
    int tag[105] = {0};
    for (Use* use = alloc->getBeginUse(); use->next != nullptr; use = (Use*) use->next) {
        Instr* user = use->user;
        if (dynamic_cast<INSTR::GetElementPtr*>(user) == nullptr) {
            return false;
        }
        if (!user->onlyOneUser()) {
            if (user->getBeginUse()->next != user->getEndUse()) {
                return false;
            }
            Instr* ins1 = user->getBeginUse()->user;
            Instr* ins2 = user->getEndUse()->user;
            if (!_type_in_types<INSTR::Store, INSTR::Load, INSTR::Call>(ins1)) {
                return false;
            }
            if (!_type_in_types<INSTR::Store, INSTR::Load, INSTR::Call>(ins2)) {
                return false;
            }
            if (dynamic_cast<INSTR::Call*>(ins1) != nullptr && ((INSTR::Call*) ins1)->getFunc()->getName() != "memset") {
                return false;
            }
            if (dynamic_cast<INSTR::Call*>(ins2) != nullptr && ((INSTR::Call*) ins2)->getFunc()->getName() != "memset") {
                return false;
            }
        }
        if (dynamic_cast<Constant*>(user->useValueList[2]) == nullptr) {
            for (Use* use1 = user->getBeginUse(); use1->next != nullptr; use1 = (Use*) use1->next) {
                Instr* ins = use1->user;
                if (dynamic_cast<INSTR::Load*>(ins) == nullptr) {
                    return false;
                }
            }
        }
        int index = ((ConstantInt*) (user->useValueList[2]))->get_const_val();
        Instr* user_user = user->getBeginUse()->user;
        if (dynamic_cast<INSTR::Call*>(user_user) != nullptr && !user->onlyOneUser()) {
            user_user = user->getEndUse()->user;
        }
        if (dynamic_cast<INSTR::Load*>(user_user) != nullptr) {
            continue;
        }
        if (dynamic_cast<INSTR::Store*>(user_user) == nullptr ||
            dynamic_cast<ConstantInt*>(user_user->useValueList[0]) == nullptr) {
            return false;
        }
        int val = ((ConstantInt*) (user_user->useValueList[0]))->get_const_val();
        if (val != (1 << index)) {
            return false;
        }
        tag[index] = 1;
    }
    for (int i = 0; i < len; i++) {
        if (!tag[i]) {
            return false;
        }
    }
    return true;
}

void pow_2_array_opt::init() {
    for (Function* function: *functions) {
        for_bb_(function) {
            for_instr_(bb) {
//                if (instr->operator std::string() == "%v224 = alloca [31 x i32]") {
//                    printf("debug\n");
//                }
                if (is_pow_2_array_alloc(instr)) {
                    std::cout << instr->operator std::string() << std::endl;
                    ((INSTR::Alloc*) instr)->clearLoads();
                    for (Use *use = instr->getBeginUse(); use->next != nullptr; use = (Use *) use->next) {
                        Instr *user = use->user;
                        alloc_load_DFS(instr,  user);
                    }
                    for (Instr* load: *((INSTR::Alloc*) instr)->loads) {
                        Value* gep = ((INSTR::Load*) load)->getPointer();
                        Value* index = ((Instr*) gep)->useValueList[2];
                        INSTR::Alu* shl = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::SHL,
                                                         new ConstantInt(1), index, load->bb);
                        load->insert_after(shl);
                        load->modifyAllUseThisToUseA(shl);
                    }
                }
            }
        }
    }
}

void pow_2_array_opt::alloc_load_DFS(Value *alloc, Instr *instr) {
    if (dynamic_cast<INSTR::GetElementPtr *>(instr) != nullptr) {
        for (Use *use = instr->getBeginUse(); use->next != nullptr; use = (Use *) use->next) {
            Instr *user = use->user;
            alloc_load_DFS(alloc, user);
        }
    } else if (dynamic_cast<INSTR::Load *>(instr) != nullptr) {
        ((INSTR::Load *) instr)->alloc = (alloc);
        ((INSTR::Alloc *) alloc)->addLoad(instr);
    } else if (dynamic_cast<INSTR::Store *>(instr) != nullptr) {
//        ((INSTR::Store *) instr)->alloc = (alloc);
    } else {
        //assert false;
        //memset已经特判,不需要考虑bitcast?
    }
}
