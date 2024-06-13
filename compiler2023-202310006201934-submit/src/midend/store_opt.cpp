//
// Created by start on 23-8-20.
//
#include "store_opt.h"
#include "stl_op.h"
store_opt::store_opt(std::vector<Function *> *functions) {
    this->functions = functions;
}

void store_opt::Run() {
    patten_A();
}

void store_opt::patten_A() {
    //init
    if (functions->size() != 1) return;
    for_func_(functions) {
        for_bb_(function) {
            for_instr_(bb) {
                if  (is_patten_A(instr)) {
                    atomic_array->insert(instr);
                }
            }
        }
    }
    printf("atomic_array num %d\n", atomic_array->size());
    std::set<Instr*>* can_remove = new std::set<Instr*>();
    for (Instr* alloc: *atomic_array) {
        can_remove->clear();
        geps->clear();
        check_A(alloc);
        for (Instr* gep: *geps) {
//            std::cout << gep->operator std::string() << std::endl;
            if (store_once_at_head(gep)) {
                for_use_(gep) {
                    Instr* user = use->user;
                    if (is_type(INSTR::Load, user)) {
                        user->modifyAllUseThisToUseA(st_val);
                    }
                }

                continue;
            }

            for_use_(gep) {
                Instr* user = use->user;
                if (is_type(INSTR::Store, user)) {
                    if (!is_type(ConstantInt, ((INSTR::Store*) user)->getValue())) continue;
                    int val = ((ConstantInt*) ((INSTR::Store*) user)->getValue())->get_const_val();
                    Instr* pos = (Instr*) user->next;
                    for (; pos->next != nullptr; pos = (Instr*) pos->next) {
                        if (is_type(INSTR::Store, pos) && ((INSTR::Store*) pos)->getPointer() == gep) {
                            break;
                        }
                        if (is_type(INSTR::Load, pos) && ((INSTR::Load*) pos)->getPointer() == gep) {
                            pos->modifyAllUseThisToUseA(new ConstantInt(val));
                        }
                    }
                    if (pos->next == nullptr) {
                        bool tag = true;
                        for_use_(gep) {
                            Instr* t = use->user;
                            if (!is_type(INSTR::Load, t)) continue;
                            if (t->bb != user->bb || (t->bb == user->bb && A_after_B(user, t))) {
                                tag = false;
                            }
                        }
                        if (tag) can_remove->insert(user);
                    }
                }
            }
        }
        for (Instr* instr: *can_remove) {
            instr->remove();
        }
    }

}

bool store_opt::is_patten_A(Instr* instr) {
    if (!is_type(INSTR::Alloc, instr)) return false;
    if (!((PointerType*) instr->type)->inner_type->is_array_type()) return false;
//    if (((ArrayType*) ((PointerType*) instr->type)->inner_type)->dims.size() != 1) return false;
//    for ()
    return check_A(instr);
}



bool store_opt::check_A(Instr *in) {
    for_use_(in) {
        Instr* instr = use->user;
        if (is_type(INSTR::GetElementPtr, instr)) {
            geps->insert(instr);
            int len = instr->useValueList.size();
            for (int i = 1; i < len; i++) {
                if (!is_type(ConstantInt, instr->useValueList[i])) return false;
            }
            if (!check_A(instr)) return false;
        } else if (is_type(INSTR::Load, instr)) {

        } else if (is_type(INSTR::Store, instr)) {

        } else if (is_type(INSTR::Call, instr)) {
            return false;
        }
    }
    return true;
}

bool store_opt::A_after_B(Instr *A, Instr *B) {
    if (A->bb != B->bb) {
        return false;
    }
    int tag = 0;
    for_instr_(A->bb) {
        if (tag) {
            break;
        }
        if (instr == B) {
            tag = 1;
        }
        if (!tag && instr == A) {
            return false;
        }
    }
    return true;
}

bool store_opt::store_once_at_head(Instr *instr) {
    int store_num = 0;
    INSTR::Store* st = nullptr;
    for_use_(instr) {
        Instr* user = use->user;
        if (is_type(INSTR::Store, user)) {
            st = ((INSTR::Store*) user);
            store_num++;
        }
    }
    if (!(store_num == 1 && st->bb == st->bb->function->entry)) return false;
    for_use_(instr) {
        Instr* user = use->user;
        if (!is_type(INSTR::Store, user) && user->bb == user->bb->function->entry) {
            return false;
        }
    }
    st_val = st->getValue();
    return true;
//    return false;
}

