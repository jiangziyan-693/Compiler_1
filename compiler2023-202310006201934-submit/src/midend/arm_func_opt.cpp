//
// Created by start on 23-8-12.
//
#include "arm_func_opt.h"
#include "stl_op.h"
#include "cmath"
#include "Manager.h"
#include "Loop.h"

arm_func_opt::arm_func_opt(std::vector<Function *> *functions, std::unordered_map<GlobalValue *, Initial *>* globals) {
    this->functions = functions;
    this->globals = globals;
}

void arm_func_opt::Run() {
    ustat();
    llmmod();
    memorize();
    patten_A();
    patten_B();
    patten_C();
}

void arm_func_opt::memorize() {
    const bool use_double_hash = false;
    const int hsh1_base = 53, hsh1_mod = 1007;
    const int hsh2_base = 1973, hsh2_mod = 165523;
    for (Function *function: *functions) {
        if (can_memorize(function)) {
            // std::cerr << "[func_opt: memorize] " << function->getName() << std::endl;
            assert(!function->retType->is_void_type());
            if (use_double_hash) {
                // 1: create global array (data, key2) for this function
                ArrayType *ty_glob_data = new ArrayType(hsh1_mod, function->retType),
                          *ty_glob_key2 = new ArrayType(hsh1_mod, new BasicType(DataType::I32));
                GlobalValue *glob_data = new GlobalValue(ty_glob_data, "m0000001_data_" + function->getName(), new ZeroInit(ty_glob_data)),
                            *glob_key2 = new GlobalValue(ty_glob_key2, "m0000001_key2_" + function->getName(), new ZeroInit(ty_glob_key2));
                assert(globals->count(glob_data) == 0 && globals->count(glob_key2) == 0);
                (*globals)[glob_data] = glob_data->initial;
                (*globals)[glob_key2] = glob_key2->initial;
                // 2: insert hash bbs
                BasicBlock *bb_hash0 = new BasicBlock(function, function->getBeginBB()->loop, false),
                           *bb_hash1 = new BasicBlock(function, function->getBeginBB()->loop, false),
                           *bb_hash2 = new BasicBlock(function, function->getBeginBB()->loop, false),
                           *bb_hash3 = new BasicBlock(function, function->getBeginBB()->loop, false),
                           *bb_hash4 = new BasicBlock(function, function->getBeginBB()->loop, false);
                ConstantInt *v_base1 = new ConstantInt(hsh1_base), *v_mod1 = new ConstantInt(hsh1_mod);
                ConstantInt *v_base2 = new ConstantInt(hsh2_base), *v_mod2 = new ConstantInt(hsh2_mod);
                // 2.1: bitcast all arguments to i32
                std::vector<Value *> cast_args;
                for (Function::Param *param: *function->params) {
                    if (param->type->is_int32_type()) {
                        cast_args.push_back(param);
                    } else {
                        INSTR::BitCast *cast = new INSTR::BitCast(param, new BasicType(DataType::I32), bb_hash0);
                        cast_args.push_back(cast);
                    }
                }
                assert(!cast_args.empty());
                // 2.2: calculate hash1 and hash2 value
                Value *v_hash1 = cast_args[0], *v_hash2 = cast_args[0];
                if (cast_args.size() == 1) {
                    v_hash1 = new INSTR::Alu(new BasicType(DataType::I32), INSTR::Alu::Op::REM, v_hash1, v_mod1, bb_hash0);
                    v_hash2 = new INSTR::Alu(new BasicType(DataType::I32), INSTR::Alu::Op::REM, v_hash2, v_mod2, bb_hash0);
                } else {
                    for (int i = 1; i < cast_args.size(); i++) {
                        std::vector<Value *> params1 {v_hash1, v_base1, cast_args[i], v_mod1};
                        v_hash1 = new INSTR::Call(ExternFunction::LLMAMOD, &params1, bb_hash0);
                        std::vector<Value *> params2 {v_hash2, v_base2, cast_args[i], v_mod2};
                        v_hash2 = new INSTR::Call(ExternFunction::LLMAMOD, &params2, bb_hash0);
                    }
                }
                v_hash2 = new INSTR::Alu(new BasicType(DataType::I32), INSTR::Alu::Op::ADD, v_hash2, ConstantInt::CONST_1, bb_hash0);
                new INSTR::Jump(bb_hash1, bb_hash0);
                // 2.3: eval data and used pointer
                std::vector<Value *> hashes {};
                INSTR::Phi *v_hash = new INSTR::Phi(new BasicType(DataType::I32), &hashes, bb_hash1);

                std::vector<Value *> idxList { ConstantInt::CONST_0, v_hash };
                INSTR::GetElementPtr *ptr_key2 = new INSTR::GetElementPtr(new BasicType(DataType::I32), glob_key2, &idxList, bb_hash1),
                                     *ptr_data = new INSTR::GetElementPtr(function->retType, glob_data, &idxList, bb_hash1);
                // 2.4: check key2 and hash2
                Value *v_key2 = new INSTR::Load(ptr_key2, bb_hash1);
                // eq 0
                Value *cond_key2_eq_0 = new INSTR::Icmp(INSTR::Icmp::Op::EQ, v_key2, ConstantInt::CONST_0, bb_hash1);
                new INSTR::Branch(cond_key2_eq_0, function->getBeginBB(), bb_hash2, bb_hash1);
                // eq hash2
                Value *cond_key2_eq_hash2 = new INSTR::Icmp(INSTR::Icmp::Op::EQ, v_key2, v_hash2, bb_hash2);
                new INSTR::Branch(cond_key2_eq_hash2, bb_hash4, bb_hash3, bb_hash2);
                // 2.5: add hash by one
                Value *v_hash_add1 = new INSTR::Alu(new BasicType(DataType::I32), INSTR::Alu::Op::ADD, v_hash, ConstantInt::CONST_1, bb_hash3);
                v_hash_add1 = new INSTR::Alu(new BasicType(DataType::I32), INSTR::Alu::Op::REM, v_hash_add1, v_mod1, bb_hash3);
                v_hash->addOptionalValue(v_hash1);
                v_hash->addOptionalValue(v_hash_add1);
                new INSTR::Jump(bb_hash1, bb_hash3);

                // 2.6: load memorized data and return
                Value *v_data = new INSTR::Load(ptr_data, bb_hash4);
                new INSTR::Return(v_data, bb_hash4);
                // 3: insert store inst before every returns
                for_bb_(function) {
                    for_instr_(bb) {
                        if (instr->isReturn()) {
                            INSTR::Return *i_ret = ((INSTR::Return *) instr);
                            assert(i_ret->hasValue());
                            Value *v_ret = i_ret->getRetValue();
                            new INSTR::Store(v_ret, ptr_data, i_ret);
                            new INSTR::Store(v_hash2, ptr_key2, i_ret);
                        }
                    }
                }
                function->insertAtBegin(bb_hash4);
                function->insertAtBegin(bb_hash3);
                function->insertAtBegin(bb_hash2);
                function->insertAtBegin(bb_hash1);
                function->insertAtBegin(bb_hash0);
                function->entry = bb_hash0;
            } else {
                // 1: create global array (data, used) for this function
                ArrayType *ty_glob_data = new ArrayType(hsh1_mod, function->retType),
                        *ty_glob_used = new ArrayType(hsh1_mod, new BasicType(DataType::I32));
                GlobalValue *glob_data = new GlobalValue(ty_glob_data, "m000000data_" + function->getName(), new ZeroInit(ty_glob_data)),
                            *glob_used = new GlobalValue(ty_glob_used, "m000000used_" + function->getName(), new ZeroInit(ty_glob_used));
                assert(globals->count(glob_data) == 0 && globals->count(glob_used) == 0);
                (*globals)[glob_data] = glob_data->initial;
                (*globals)[glob_used] = glob_used->initial;
                // 2: insert bbhash0, bbret0
                BasicBlock *bb_hash = new BasicBlock(function, function->getBeginBB()->loop, false);
                BasicBlock *bb_ret = new BasicBlock(function, function->getBeginBB()->loop, false);

                ConstantInt *v_base = new ConstantInt(hsh1_base), *v_mod = new ConstantInt(hsh1_mod);
                // 2.1: bitcast all arguments to i32
                std::vector<Value *> cast_args;
                for (Function::Param *param: *function->params) {
                    if (param->type->is_int32_type()) {
                        cast_args.push_back(param);
                    } else {
                        INSTR::BitCast *cast = new INSTR::BitCast(param, new BasicType(DataType::I32), bb_hash);
                        cast_args.push_back(cast);
                    }
                }
                assert(!cast_args.empty());
                // 2.2: calculate hash value
                Value *v_hash = cast_args[0];
                if (cast_args.size() == 1) {
                    v_hash = new INSTR::Alu(new BasicType(DataType::I32), INSTR::Alu::Op::REM, v_hash, v_mod, bb_hash);
                } else {
                    for (int i = 1; i < cast_args.size(); i++) {
                        std::vector<Value *> params {v_hash, v_base, cast_args[i], v_mod};
                        v_hash = new INSTR::Call(ExternFunction::LLMAMOD, &params, bb_hash);
                    }
                }
                // 2.3: eval data and used pointer
                std::vector<Value *> idxList { ConstantInt::CONST_0, v_hash };
                INSTR::GetElementPtr *ptr_used = new INSTR::GetElementPtr(new BasicType(DataType::I32), glob_used, &idxList, bb_hash),
                                    *ptr_data = new INSTR::GetElementPtr(function->retType, glob_data, &idxList, bb_hash);
                // 2.4: load used and check it
                Value *v_used = new INSTR::Load(ptr_used, bb_hash);
                Value *cond_used = new INSTR::Icmp(INSTR::Icmp::Op::NE, v_used, ConstantInt::CONST_0, bb_hash);
                new INSTR::Branch(cond_used, bb_ret, function->getBeginBB(), bb_hash);
                // 2.5: load memrized data and return
                Value *v_data = new INSTR::Load(ptr_data, bb_ret);
                new INSTR::Return(v_data, bb_ret);
                // 3: insert store inst before every returns
                for_bb_(function) {
                    for_instr_(bb) {
                        if (instr->isReturn()) {
                            INSTR::Return *i_ret = ((INSTR::Return *) instr);
                            assert(i_ret->hasValue());
                            Value *v_ret = i_ret->getRetValue();
                            new INSTR::Store(v_ret, ptr_data, i_ret);
                            new INSTR::Store(ConstantInt::CONST_1, ptr_used, i_ret);
                        }
                    }
                }
                function->insertAtBegin(bb_ret);
                function->insertAtBegin(bb_hash);
                function->entry = bb_hash;
            }
        }
    }
}

bool arm_func_opt::can_memorize(Function* function) {
    if (!function->hasRet())
        return false;
    if (function->params->empty())
        return false;
    std::set<Function *> disabled_external_functions {
        ExternFunction::MEM_SET,  
    };
    std::set<Function *> allowed_external_functions {
        ExternFunction::LLMMOD,
        ExternFunction::LLMD2MOD,
        ExternFunction::LLMAMOD,
        ExternFunction::USAT,
    };
    // no pointer param
    for (Function::Param *param: *function->params) {
        if (param->type->is_pointer_type())
            return false;
    }
    // no sub no-pure calls and recursive call itself
    int recursive_calls = 0;
    for_bb_(function) {
        for_instr_(bb) {
            if (instr->isCall()) {
                INSTR::Call *call = (INSTR::Call *) instr;
                // std::cerr << "[memorize] " << function->getName() << " calls " << call->getFunc()->getName() << std::endl;
                if (call->getFunc() == function) {
                    recursive_calls++;
                    continue;
                }
                if (call->getFunc()->isExternal) {
                    if (!allowed_external_functions.count(call->getFunc()))
                        return false;
                } else {
                    if (!call->getFunc()->isPure)
                        return false;
                }
            }
        }
    }
    if (recursive_calls < 2)
        return false;
    // restrict: no load
    for_bb_(function) {
        for_instr_(bb) {
            if (instr->isLoad())
                return false;
        }
    }
    return true;
}

void arm_func_opt::ustat() {
    std::set<Function*>* removes = new std::set<Function*>();
    for (Function *function: *functions) {
        if (is_ustat(function)) {
            for_use_(function) {
                Instr* call = use->user;
                Instr* modify_call = new INSTR::Call(ExternFunction::USAT,
                                                     new std::vector<Value*> {
                                                            call->useValueList[1],
                                                            new ConstantInt((*log_num)[function])
                                                        },
                                                        call->bb);
                call->modifyAllUseThisToUseA(modify_call);
                call->insert_after(modify_call);
                call->remove();
            }
            removes->insert(function);
        }
    }

    for (Function* function: *removes) {
        functions->erase(
                std::remove(functions->begin(), functions->end(), function),
                functions->end());

        for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
            for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
                instr->remove();
            }
//            bb->remove();
        }
        function->is_deleted = true;
    }
}

bool arm_func_opt::is_ustat(Function *function) {
    if (function->params->size() != 1) {
        return false;
    }
    Function::Param *param = *(function->params->begin());
    if (!param->type->is_int32_type()) {
        return false;
    }
    BasicBlock *A = function->entry;
    if (bb_instr_num(A) != 2) {
        return false;
    }
    Instr *A_icmp = A->getBeginInstr();
    Instr *A_br = A->getEndInstr();
    if (!is_type(INSTR::Icmp, A_icmp) || !is_type(INSTR::Branch, A_br)) {
        return false;
    }
    if (((INSTR::Icmp*) A_icmp)->op != INSTR::Icmp::Op::SLT) {
        return false;
    }
    if (A_icmp->useValueList[0] != param || !is_zero(A_icmp->useValueList[1]))
        return false;
    if (A_br->useValueList[0] != A_icmp) {
        return false;
    }
    BasicBlock* C = ((INSTR::Branch*) A_br)->getElseTarget();
    BasicBlock* B = ((INSTR::Branch*) A_br)->getThenTarget();
    if (bb_instr_num(B) != 1) {
        return false;
    }
    Instr* B_jump = B->getEndInstr();
    if (!is_type(INSTR::Jump, B_jump)) {
        return false;
    }
    BasicBlock* D = ((INSTR::Jump*) B_jump)->getTarget();
    if (bb_instr_num(C) != 2) {
        return false;
    }
    Instr* C_icmp = C->getBeginInstr();
    Instr* C_br = C->getEndInstr();
    if (!is_type(INSTR::Icmp, C_icmp) || !is_type(INSTR::Branch, C_br)) {
        return false;
    }
    if (((INSTR::Icmp*) C_icmp)->op != INSTR::Icmp::Op::SGT) {
        return false;
    }
    if (C_icmp->useValueList[0] != param || !is_2_pow_sub_1(C_icmp->useValueList[1]))
        return false;
    int ret_tag = get_log(C_icmp->useValueList[1]);
    if (C_br->useValueList[0] != C_icmp) {
        return false;
    }
    if (((INSTR::Branch*) C_br)->getElseTarget() != D) {
        return false;
    }
    BasicBlock* E = ((INSTR::Branch*) C_br)->getThenTarget();
    if (bb_instr_num(E) != 1) {
        return false;
    }
    Instr* E_jump = B->getEndInstr();
    if (!is_type(INSTR::Jump, E_jump)) {
        return false;
    }
    if (((INSTR::Jump*) E_jump)->getTarget() != D) {
        return false;
    }

    if (bb_instr_num(D) != 2) {
        return false;
    }
    Instr* phi = D->getBeginInstr();
    Instr* ret = D->getEndInstr();
    if (!is_type(INSTR::Phi, phi) || !is_type(INSTR::Return, ret)) {
        return false;
    }
    if (phi->useValueList.size() != 3 ||
            !is_zero(phi->useValueList[0]) ||
            phi->useValueList[1] != param ||
            !is_type(ConstantInt, phi->useValueList[2]) ||
            ((ConstantInt*) phi->useValueList[2])->get_const_val() != ((ConstantInt*) C_icmp->useValueList[1])->get_const_val()) {
        return false;
    }
    if (ret->useValueList.size() != 1 ||
        ret->useValueList[0] != phi) {
        return false;
    }
    (*log_num)[function] = ret_tag;
    return true;
}

int arm_func_opt::bb_instr_num(BasicBlock *bb) {
    int ret = 0;
    for_instr_(bb) {
        ret++;
    }
    return ret;
}

bool arm_func_opt::is_zero(Value *value) {
    if (!is_type(ConstantInt, value)) {
        return false;
    }
    return ((ConstantInt *) value)->get_const_val() == 0;
}

bool arm_func_opt::is_2_pow_sub_1(Value *value) {
    if (!is_type(ConstantInt, value)) {
        return false;
    }
    int val = ((ConstantInt *) value)->get_const_val() + 1;
    return pow(2, log(val) / log(2)) == val;
}

int arm_func_opt::get_log(Value *value) {
    int val = ((ConstantInt *) value)->get_const_val() + 1;
    return log(val) / log(2);
}


void arm_func_opt::llmmod() {
    std::set<Function*>* removes = new std::set<Function*>();
    for_func_(functions) {
        if (is_llmmod(function)) {
            printf("find_llmod\n");
            for_use_(function) {
                Instr* call = use->user;
                Instr* modify_call = new INSTR::Call(ExternFunction::LLMMOD,
                                                     new std::vector<Value*> {
                                                             call->useValueList[1],
                                                             call->useValueList[2],
                                                             new ConstantInt(mod)
                                                     },
                                                     call->bb);
                call->modifyAllUseThisToUseA(modify_call);
                call->insert_after(modify_call);
                call->remove();
            }
            removes->insert(function);
        }
    }
    for (Function* function: *removes) {
        functions->erase(
                std::remove(functions->begin(), functions->end(), function),
                functions->end());

        for (BasicBlock* bb = function->getBeginBB(); bb->next != nullptr; bb = (BasicBlock*) bb->next) {
            for (Instr* instr = bb->getBeginInstr(); instr->next != nullptr; instr = (Instr*) instr->next) {
                instr->remove();
            }
//            bb->remove();
        }
        function->is_deleted = true;
    }
}

bool arm_func_opt::is_llmmod(Function *function) {
    if (function->params->size() != 2) return false;
    Value *param_0 = (*function->params)[0],
            *param_1 = (*function->params)[1];
    BasicBlock* A = function->entry;
    //A
    check_bb_instr_num(A, 2);
    init_check_trans(A, 1, INSTR::Icmp);
    init_check_trans(A, 2, INSTR::Branch);
    check_icmp_val_op_const(A_1, param_1, INSTR::Icmp::Op::EQ, 0);
    check_use_at(A_2, A_1, 0);
    BasicBlock* B = A_2->getThenTarget(),
    *C = A_2->getElseTarget();
    // B
    check_bb_instr_num(B, 1);
    init_check_trans(B, 1, INSTR::Return);
    check_ret_const(B_1, 0);
    //C
    check_bb_instr_num(C, 2);
    init_check_trans(C, 1, INSTR::Icmp);
    init_check_trans(C, 2, INSTR::Branch);
    check_icmp_val_op_const(C_1, param_1, INSTR::Icmp::Op::EQ, 1);
    check_use_at(C_2, C_1, 0);
    BasicBlock *D = C_2->getThenTarget(),
    *E = C_2->getElseTarget();
    //D
    check_bb_instr_num(D, 2);
    init_check_trans(D, 1, INSTR::Alu);
    init_check_trans(D, 2, INSTR::Return);
    check_val_mod_const(D_1, param_0);
    check_ret_val(D_2, D_1);
    mod = ((ConstantInt*) D_1->useValueList[1])->get_const_val();
    //E
    check_bb_instr_num(E, 7);
    init_check_trans(E, 1, INSTR::Alu);
    init_check_trans(E, 2, INSTR::Call);
    init_check_trans(E, 3, INSTR::Alu);
    init_check_trans(E, 4, INSTR::Alu);
    init_check_trans(E, 5, INSTR::Alu);
    init_check_trans(E, 6, INSTR::Icmp);
    init_check_trans(E, 7, INSTR::Branch);
    check_val_div_const(E_1, param_1, 2);
    check_call_func_p1_p2(E_2, function, param_0, E_1);
    check_v1_add_v2(E_3, E_2, E_2);
    check_val_mod_const(E_4, E_3);
    check_val_mod_2(E_5, param_1);
    check_icmp_val_op_const(E_6, E_5, INSTR::Icmp::Op::EQ, 1);
    check_use_at(E_7, E_6, 0);
    BasicBlock *F = E_7->getThenTarget(),
    *G = E_7->getElseTarget();
    //F
    check_bb_instr_num(F, 3);
    init_check_trans(F, 1, INSTR::Alu);
    init_check_trans(F, 2, INSTR::Alu);
    init_check_trans(F, 3, INSTR::Return);
    check_v1_add_v2(F_1, E_4, param_0);
    check_val_mod_const(F_2, F_1);
    check_ret_val(F_3, F_2);
    //G
    check_bb_instr_num(G, 1);
    init_check_trans(G, 1, INSTR::Return);
    check_ret_val(G_1, E_4);
    return true;
}

bool arm_func_opt::is_rem_const(Instr *instr) {
    return false;
}

Instr *arm_func_opt::get_instr_at(BasicBlock *bb, int index) {
    for_instr_(bb) {
        index--;
        if (!index) {
            return instr;
        }
    }
    return nullptr;
}

void arm_func_opt::patten_A() {

    for_func_(functions) {
        for_bb_(function) {
            if (is_patten_A(bb)) {
                bool tag = false;
                for (Function* function1: *functions) {
                    if (function1 != function) {
                        if (function1->params->size() != 4) continue;
                        param_0 = (*function1->params)[0];
                        param_1 = (*function1->params)[1];
                        param_2 = (*function1->params)[2];
                        param_3 = (*function1->params)[3];
                        for (BasicBlock* bb1 = function1->getBeginBB(); bb1->next != nullptr; bb1 = (BasicBlock*) bb1->next) {
                            if (try_simplify_loop_nest(bb1)) {
                                tag = true;
                                break;
                            }
                        }
                    }
                }
                if (!tag) return;
                is_patten_A(bb);
                printf("arm func find patten A\n");
                std::vector<GlobalValue*>* add_global_val = new std::vector<GlobalValue*>();
                for (int i = 0; i < pow_n; ++i) {
                    Type *type = ((PointerType *) v_A->type)->inner_type;
                    ZeroInit *init = new ZeroInit(type);
                    std::string name = "g_add_" + std::to_string(i);
                    GlobalValue *val = new GlobalValue(type, name , init);
                    (*globals)[val] = init;
                    add_global_val->push_back(val);
                }

//                Type *type_D = ((PointerType *) v_A->type)->inner_type;
//                ZeroInit *init_D = new ZeroInit(type_D);
//                GlobalValue *val_D = new GlobalValue(type_D, "g_D" , init_D);
//                (*globals)[val_D] = init_D;
//
//                Type *type_E = ((PointerType *) v_A->type)->inner_type;
//                ZeroInit *init_E = new ZeroInit(type_E);
//                GlobalValue *val_E = new GlobalValue(type_E, "g_E" , init_E);
//                (*globals)[val_E] = init_E;
//
//                Type *type_F = ((PointerType *) v_A->type)->inner_type;
//                ZeroInit *init_F = new ZeroInit(type_F);
//                GlobalValue *val_F = new GlobalValue(type_F, "g_F" , init_F);
//                (*globals)[val_F] = init_F;

                Type* gep_type = ((ArrayType*) ((PointerType *) v_A->type)->inner_type)->base_type;

                std::vector<Instr*>* add_geps = new std::vector<Instr*>();
                for (int i = 0; i < pow_n; ++i) {
                    std::vector<Value*>* index = new std::vector<Value*>();
                    index->push_back(new ConstantInt(0));
                    index->push_back(new ConstantInt(0));
                    Instr* gep = new INSTR::GetElementPtr(gep_type, (*add_global_val)[i], index, A);
                    add_geps->push_back(gep);
                }
//                std::vector<Value*>* index_D = new std::vector<Value*>();
//                index_D->push_back(new ConstantInt(0));
//                index_D->push_back(new ConstantInt(0));
//                Instr* gep_D_0_0 = new INSTR::GetElementPtr(gep_type, val_D, index_D, A);
//
//                std::vector<Value*>* index_E = new std::vector<Value*>();
//                index_E->push_back(new ConstantInt(0));
//                index_E->push_back(new ConstantInt(0));
//                Instr* gep_E_0_0 = new INSTR::GetElementPtr(gep_type, val_E, index_E, A);
//
//                std::vector<Value*>* index_F = new std::vector<Value*>();
//                index_F->push_back(new ConstantInt(0));
//                index_F->push_back(new ConstantInt(0));
//                Instr* gep_F_0_0 = new INSTR::GetElementPtr(gep_type, val_F, index_F, A);

//                A->getEndInstr()->insert_before(gep_D_0_0);
//                A->getEndInstr()->insert_before(gep_E_0_0);
//                A->getEndInstr()->insert_before(gep_F_0_0);
                for (int i = 0; i < pow_n; ++i) {
                    A->getEndInstr()->insert_before((*add_geps)[i]);
                }

                Instr *gep_A_0_0 = get_instr_at(A, 4),
                        *gep_B_0_0 = get_instr_at(A, 3),
                        *gep_C_0_0 = get_instr_at(A, 2);
                Function* func = (Function*) C->getBeginInstr()->useValueList[0];

//                std::vector<Value*>* param_A_A_C = new std::vector<Value*>();
//                param_A_A_C->push_back(gep_A_0_0);
//                param_A_A_C->push_back(gep_A_0_0);
//                param_A_A_C->push_back(gep_C_0_0);

                std::vector<Instr*>* add_calls = new std::vector<Instr*>();
                INSTR::Call* A_2 = new INSTR::Call(func, new std::vector<Value*>{N, gep_A_0_0, gep_A_0_0, gep_C_0_0}, A);
                INSTR::Call* A_4 = new INSTR::Call(func, new std::vector<Value*>{N, gep_C_0_0, gep_C_0_0, (*add_geps)[0]}, A);

                add_calls->push_back(A_2);
                add_calls->push_back(A_4);
                for (int i = 1; i < pow_n - 1; ++i) {
                    INSTR::Call* t = new INSTR::Call(func, new std::vector<Value*>{N, (*add_geps)[i - 1], (*add_geps)[i - 1], (*add_geps)[i]}, A);
                    add_calls->push_back(t);
                }
                INSTR::Call* pow_n_mul = new INSTR::Call(func, new std::vector<Value*>{N, (*add_geps)[pow_n - 2], gep_B_0_0, (*add_geps)[pow_n - 1]}, A);
                INSTR::Call* pow_n_add_2_mul = new INSTR::Call(func, new std::vector<Value*>{N, gep_C_0_0, (*add_geps)[pow_n - 1], gep_B_0_0}, A);
                add_calls->push_back(pow_n_mul);
                add_calls->push_back(pow_n_add_2_mul);

//                INSTR::Call* A_A_C = new INSTR::Call(func, new std::vector<Value*>{N, gep_A_0_0, gep_A_0_0, gep_C_0_0}, A);
//                INSTR::Call* C_C_D = new INSTR::Call(func, new std::vector<Value*>{N, gep_C_0_0, gep_C_0_0, gep_D_0_0}, A);
//                INSTR::Call* D_D_E = new INSTR::Call(func, new std::vector<Value*>{N, gep_D_0_0, gep_D_0_0, gep_E_0_0}, A);
//                INSTR::Call* E_B_F = new INSTR::Call(func, new std::vector<Value*>{N, gep_E_0_0, gep_B_0_0, gep_F_0_0}, A);
//                INSTR::Call* C_F_B = new INSTR::Call(func, new std::vector<Value*>{N, gep_C_0_0, gep_F_0_0, gep_B_0_0}, A);

                for (Instr* instr: *add_calls) {
                    A->getEndInstr()->insert_before(instr);
                }
//                A->getEndInstr()->insert_before(A_A_C);
//                A->getEndInstr()->insert_before(C_C_D);
//                A->getEndInstr()->insert_before(D_D_E);
//                A->getEndInstr()->insert_before(E_B_F);
//                A->getEndInstr()->insert_before(C_F_B);

                A->getEndInstr()->modifyUse(D, 0);

                A->modifySuc(B, D);
                D->modifyPre(B, A);

                B->remove();
                for_instr_(B) {
                    instr->remove();
                }
                C->remove();
                for_instr_(C) {
                    instr->remove();
                }
            }
        }
    }
}

bool arm_func_opt::is_patten_A(BasicBlock *bb) {
//    if (bb->label == "b32") {
//        printf("debug\n");
//    }
    if (!bb->isLoopHeader) return false;
    if (bb->precBBs->size() != 2) return false;
    A = (*bb->precBBs)[0];
    B = bb;
    C = (*bb->precBBs)[1];
    //B
    check_bb_instr_num(B, 3);
    init_check_trans(B, 1, INSTR::Phi);
    init_check_trans(B, 2, INSTR::Icmp);
    init_check_trans(B, 3, INSTR::Branch);
    check_use_const_at(B_1, 0, 0);
    //need check phi use value index 1
    check_use_at(B_2, B_1, 0);
//    check_use_const_at(B_2, 5, 1);
    if (!is_type(ConstantInt, B_2->useValueList[1])) return false;
    if (B_2->op != INSTR::Icmp::Op::SLT) return false;
    pow_n = get_n(2 * ((ConstantInt*) B_2->useValueList[1])->get_const_val());

    if (pow_n == -1) return false;
//    printf("pow_n %d\n", pow_n);
//    std::cout << bb->label << std::endl;
//    check_icmp_val_op_const(B_2, B_1, INSTR::Icmp::Op::SLT, 5);
    check_use_at(B_3, B_2, 0);
    check_use_at(B_3, C, 1);
    D = B_3->getElseTarget();

    //A
    check_bb_instr_num(A, 5);
    init_check_trans(A, 1, INSTR::Call);
    init_check_trans(A, 2, INSTR::GetElementPtr);
    init_check_trans(A, 3, INSTR::GetElementPtr);
    init_check_trans(A, 4, INSTR::GetElementPtr);
    init_check_trans(A, 5, INSTR::Jump);
    check_use_at(A_1, ExternFunction::START_TIME, 0);
    check_use_const_at(A_1, 0, 1);
    v_C = A_2->useValueList[0];
    check_use_const_at(A_2, 0, 1);
    check_use_const_at(A_2, 0, 2);
    v_B = A_3->useValueList[0];
    check_use_const_at(A_3, 0, 1);
    check_use_const_at(A_3, 0, 2);
    v_A = A_4->useValueList[0];
    check_use_const_at(A_4, 0, 1);
    check_use_const_at(A_4, 0, 2);
    check_use_at(A_5, B, 0);

    check_square_len(v_A);
    check_square_len(v_B);
    check_square_len(v_C);


    //C

    check_bb_instr_num(C, 4);
    init_check_trans(C, 1, INSTR::Call);
    init_check_trans(C, 2, INSTR::Call);
    init_check_trans(C, 3, INSTR::Alu);
    init_check_trans(C, 4, INSTR::Jump);
    check_use_at(C_1, C_2->useValueList[0], 0);
    Value* n = C_1->useValueList[1];
    check_use_at(C_1, A_4, 2);
    check_use_at(C_1, A_3, 3);
    check_use_at(C_1, A_2, 4);

    check_use_at(C_2, C_1->useValueList[0], 0);
    check_use_at(C_2, n, 1);
    check_use_at(C_2, A_4, 2);
    check_use_at(C_2, A_2, 3);
    check_use_at(C_2, A_3, 4);

    if (dynamic_cast<INSTR::Call*>(n) == nullptr) return false;
    if (((INSTR::Call*) n)->useValueList[0] != ExternFunction::GET_INT) return false;
    N = n;

    check_alu_v1_op_const(C_3, B_1, INSTR::Alu::Op::ADD, 1);
    //re check B phi
    check_use_at(B_1, C_3, 1);
    check_use_at(C_4, B, 0);

    return true;
}

void arm_func_opt::patten_B() {
    if (functions->size() != 2) return;
    for_func_(functions) {
        if (function->params->size() != 4) continue;
        param_0 = (*function->params)[0];
        param_1 = (*function->params)[1];
        param_2 = (*function->params)[2];
        param_3 = (*function->params)[3];
        for_bb_(function) {
            if (try_simplify_loop_nest(bb)) {
                //remove bb and swap loop
                printf("arm func find patten B\n");
                for_instr_(H) {
                    instrs->push_back(instr);
                }
                for (Instr* instr: *instrs) {
                    instr->delFromNowBB();
                    instr->bb = E;
                    E->getEndInstr()->insert_before(instr);
                }
                E->getEndInstr()->remove();
                ((Instr*) E->getBeginInstr()->next->next)->remove();
                H->remove();
                //remove G
                G->remove();
                for_instr_(G) {
                    instr->remove();
                }
                D->precBBs->clear();
                D->precBBs->push_back(B);
                D->precBBs->push_back(K);
                Instr* D_1_phi_old = D->getBeginInstr();
                Instr* D_1_phi_new = new INSTR::Phi(BasicType::I32_TYPE, new std::vector<Value*> {new ConstantInt(0), D_1_phi_old->useValueList[2]}, D);
                D_1_phi_old->modifyAllUseThisToUseA(D_1_phi_new);
                D_1_phi_old->remove();
//                D->insertAtHead(D_1_phi_new);
                I->modifyPre(H, E);

                //swap K i

//                INSTR::Phi *idc_phi_A = (INSTR::Phi *) A->getBeginInstr(),
//                        *idc_phi_B = (INSTR::Phi *) D->getBeginInstr();
//
//
//                INSTR::Icmp *idc_cmp_A = (INSTR::Icmp *) A->getBeginInstr()->next,
//                        *idc_cmp_B = (INSTR::Icmp *) D->getBeginInstr()->next;
//
//                INSTR::Alu *idc_alu_A = (INSTR::Alu *) F->getBeginInstr(),
//                        *idc_alu_B = (INSTR::Alu *) K->getBeginInstr();
//
//                INSTR::Alu* tmp = (INSTR::Alu*) idc_alu_B->cloneToBB(idc_alu_B->bb);
//                idc_alu_B->insert_after(tmp);
//                idc_alu_B = tmp;
//
//                BasicBlock *head_A = A,
//                        *head_B = D;
//
//
//                idc_phi_A->delFromNowBB();
//                idc_cmp_A->delFromNowBB();
//                idc_phi_B->delFromNowBB();
//                idc_cmp_B->delFromNowBB();
//                head_A->getEndInstr()->insert_before(idc_phi_B);
//                head_A->getEndInstr()->insert_before(idc_cmp_B);
//                idc_phi_B->bb = head_A;
//                idc_cmp_B->bb = head_A;
//
//
//                head_B->getEndInstr()->insert_before(idc_phi_A);
//                head_B->getEndInstr()->insert_before(idc_cmp_A);
//                idc_phi_A->bb = head_B;
//                idc_cmp_A->bb = head_B;
//
//                idc_alu_A->modifyUse(idc_phi_B, 0);
//                idc_alu_B->modifyUse(idc_phi_A, 0);
//
//                idc_phi_A->modifyUse(idc_alu_B, 1);
//                idc_phi_B->modifyUse(idc_alu_A, 1);
//
//                head_A->getEndInstr()->modifyUse(idc_cmp_B, 0);
//                head_B->getEndInstr()->modifyUse(idc_cmp_A, 0);


            }
        }
    }
}

bool arm_func_opt::try_simplify_loop_nest(BasicBlock *bb) {
    if (bb->label == "b8") {
        printf("debug b8\n");
    }
    A = bb;
    //A
    check_bb_instr_num(A, 3);
    init_check_trans(A, 1, INSTR::Phi);
    init_check_trans(A, 2, INSTR::Icmp);
    init_check_trans(A, 3, INSTR::Branch);
    check_use_const_at(A_1, 0, 0);
    //need check A_1 phi index 1
    check_icmp_v1_op_v2(A_2, A_1, INSTR::Icmp::Op::SLT, param_0);
    check_use_at(A_3, A_2, 0);
    B = A_3->getThenTarget(), C = A_3->getElseTarget();
    //B
    check_bb_instr_num(B, 1);
    init_check_trans(B, 1, INSTR::Jump);
    D = B_1->getTarget();
    //C
    check_bb_instr_num(C, 1);
    init_check_trans(C, 1, INSTR::Return);
    if (C_1->hasValue()) return false;
    //D
    check_bb_instr_num(D, 3);
    init_check_trans(D, 1, INSTR::Phi);
    init_check_trans(D, 2, INSTR::Icmp);
    init_check_trans(D, 3, INSTR::Branch);
    check_use_const_at(D_1, 0, 0);
    //need check D_1 phi index 1, 2
    check_icmp_v1_op_v2(D_2, D_1, INSTR::Icmp::Op::SLT, param_0);
    check_use_at(D_3, D_2, 0);
    E = D_3->getThenTarget(), F = D_3->getElseTarget();
    //E
    check_bb_instr_num(E, 4);
    init_check_trans(E, 1, INSTR::GetElementPtr);
    init_check_trans(E, 2, INSTR::Load);
    init_check_trans(E, 3, INSTR::Icmp);
    init_check_trans(E, 4, INSTR::Branch);
    check_gep_a_i_j(E_1, param_1, D_1, A_1);
    check_use_at(E_2, E_1, 0);
    check_icmp_val_op_const(E_3, E_2, INSTR::Icmp::Op::EQ, 0);
    check_use_at(E_4, E_3, 0);
    G = E_4->getThenTarget(), H = E_4->getElseTarget();
    //F
    check_bb_instr_num(F, 2);
    init_check_trans(F, 1, INSTR::Alu);
    init_check_trans(F, 2, INSTR::Jump);
    check_alu_v1_op_const(F_1, A_1, INSTR::Alu::Op::ADD, 1);
    check_use_at(F_2, A, 0);
    //G
    check_bb_instr_num(G, 2);
    init_check_trans(G, 1, INSTR::Alu);
    init_check_trans(G, 2, INSTR::Jump);
    check_alu_v1_op_const(G_1, D_1, INSTR::Alu::Op::ADD, 1);
    check_use_at(G_2, D, 0);
    //H
    check_bb_instr_num(H, 1);
    init_check_trans(H, 1, INSTR::Jump);
    I = H_1->getTarget();
    //I
    check_bb_instr_num(I, 3);
    init_check_trans(I, 1, INSTR::Phi);
    init_check_trans(I, 2, INSTR::Icmp);
    init_check_trans(I, 3, INSTR::Branch);
    check_use_const_at(I_1, 0, 0);
    //need check I_1 PHI index 1
    check_icmp_v1_op_v2(I_2, I_1, INSTR::Icmp::Op::SLT, param_0);
    check_use_at(I_3, I_2, 0);
    J = I_3->getThenTarget(), K = I_3->getElseTarget();
    //J
    check_bb_instr_num(J, 9);
    init_check_trans(J, 1, INSTR::GetElementPtr);
    init_check_trans(J, 2, INSTR::Load);
    init_check_trans(J, 3, INSTR::GetElementPtr);
    init_check_trans(J, 4, INSTR::Load);
    init_check_trans(J, 5, INSTR::Alu);
    init_check_trans(J, 6, INSTR::Alu);
    init_check_trans(J, 7, INSTR::Store);
    init_check_trans(J, 8, INSTR::Alu);
    init_check_trans(J, 9, INSTR::Jump);
    check_gep_a_i_j(J_1, param_3, D_1, I_1);
    check_use_at(J_2, J_1, 0);
    check_gep_a_i_j(J_3, param_2, A_1, I_1);
    check_use_at(J_4, J_3, 0);
    check_alu_v1_op_v2(J_5, E_2, INSTR::Alu::Op::MUL, J_4);
    check_alu_v1_op_v2(J_6, J_2, INSTR::Alu::Op::ADD, J_5);
    check_store_val_to_ptr(J_7, J_6, J_1);
    check_alu_v1_op_const(J_8, I_1, INSTR::Alu::Op::ADD, 1);
    check_use_at(J_9, I, 0);
    //K
    check_bb_instr_num(K, 2);
    init_check_trans(K, 1, INSTR::Alu);
    init_check_trans(K, 2, INSTR::Jump);
    check_alu_v1_op_const(K_1, D_1, INSTR::Alu::Op::ADD, 1);
    check_use_at(K_2, D, 0);
    //recheck PHI
    // A_1 index 1
    check_use_at(A_1, F_1, 1);
    // D_1 index 1, 2
    check_use_at(D_1, G_1, 1);
    check_use_at(D_1, K_1, 2);
    // I_1 index 1
    check_use_at(I_1, J_8, 1);
    return true;
}

void arm_func_opt::patten_C() {
    for_func_(functions) {
        for_bb_(function) {
            check_patten_C(bb);
        }
    }
}

void arm_func_opt::check_patten_C(BasicBlock *bb) {
    if (bb->label == "b19") {
        printf("debug\n");
    }
    if (!bb->isLoopHeader) return;
    Loop* loop = bb->loop;
    if (loop->hasChildLoop()) return;
    if (!loop->isSimpleLoop() || !loop->isIdcSet()) {
        return;
    }
    Instr* idc_phi = (Instr*) loop->getIdcPHI();
    Instr* idc_cmp = (Instr*) loop->getIdcCmp();
    Instr* idc_alu = (Instr*) loop->getIdcAlu();

    BasicBlock *head = loop->getHeader();
    if (loop->exits->size() != 1 ||
        loop->enterings->size() != 1 ||
        loop->latchs->size() != 1) {
        return;
    }
    BasicBlock *exit = *loop->exits->begin();
    BasicBlock *latch = *loop->latchs->begin();
    //    BasicBlock *exit = *loop->exits->begin();
    int num = 0;
    Instr* math_phi = nullptr;
    for_instr_(head) {
        if (instr != idc_phi && instr != idc_cmp && !instr->isBJ()) {
            num++;
            math_phi = instr;
        }
    }
    if (num != 1 || !is_type(INSTR::Phi, math_phi)) return;
    //    check_use_const_at(math_phi, 0, 0);
    if (!is_type(ConstantInt, math_phi->useValueList[0])) return;
    if (((ConstantInt*) math_phi->useValueList[0])->get_const_val() != 0) return;
    Instr* math_alu = (Instr*) math_phi->useValueList[1];
    if (math_alu->bb != latch) return;
    if (!is_type(INSTR::Alu, math_alu)) return;
    if (math_alu->next != idc_alu) return;
    Value* added_val = nullptr;
    if (math_alu->useValueList[0] == math_phi) {
        added_val = math_alu->useValueList[1];
    } else {
        added_val = math_alu->useValueList[0];
    }

    num = 0;
    Instr* math_store = nullptr;
    for_use_(math_phi) {
        Instr* user = use->user;
        if (user != math_alu) {
            num++;
            math_store = user;
        }
    }
    if (num != 1) return;
    if (math_store->bb != exit) return;
    if (!is_type(INSTR::Store, math_store)) return;
    if (((INSTR::Store*) math_store)->getPointer() != math_store->prev) return;
    Instr* gep = (Instr*) math_store->prev;
    gep->delFromNowBB();
    gep->bb = latch;
    math_alu->insert_before(gep);

    Instr* load = new INSTR::Load(gep, latch);
    Instr* add_ = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::ADD, load, added_val, latch);
    Instr* st = new INSTR::Store(add_, gep, latch);

    gep->insert_after(st);
    gep->insert_after(add_);
    gep->insert_after(load);

    math_alu->remove();
    math_phi->remove();
    math_store->remove();

    printf("find arm func patten C\n");


}

int arm_func_opt::get_n(int num) {
    int t = num - 2;
    if (t < 0) {
        return -1;
    }
    int p = (int) (log(t) / log(2));
    if ((int) pow(2, p) != t) {
        return -1;
    }
    return p;
}



