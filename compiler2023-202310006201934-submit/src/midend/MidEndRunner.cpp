//
// Created by start_0916 on 23-7-20.
//
#include "midend/MidEndRunner.h"
#include "MakeDFG.h"
#include "util.h"
#include "Mem2Reg.h"
#include "GVNAndGCM.h"
#include "ConstFold.h"
//#include "Manager.h"
#include "DeadCodeDelete.h"
#include "InstrComb.h"
#include "BranchOptimize.h"
#include "MergeSimpleBB.h"
#include "MathOptimize.h"
#include "FuncInline.h"
#include "LoopInfo.h"
#include "LoopIdcVarInfo.h"
#include "FuncInfo.h"
#include "GlobalValueLocalize.h"
#include "LCSSA.h"
#include "BranchLift.h"
#include "LoopUnRoll.h"
#include "GepFuse.h"
#include "GepSplit.h"
#include "LoopStrengthReduction.h"
#include "LoopFold.h"
#include "GlobalArrayGVN.h"
#include "LocalArrayGVN.h"
#include "LoopFuse.h"
#include "RemoveUselessStore.h"
#include "MidPeepHole.h"
#include "LoopInVarCodeLift.h"
#include "AggressiveFuncGVN.h"
#include "AggressiveFuncGCM.h"
#include "stl_op.h"
#include "Rem2DivMulSub.h"
#include "IfComb.h"
#include "SimpleCalc.h"
#include "pow_2_array_opt.h"
#include "math_opt_v2.h"
#include "arm_func_opt.h"
#include "loop_swap.h"
#include "if_comb_v2.h"
#include "BlockRelationDealer.h"
#include "AggressiveMarkParallel.h"
#include "MarkParallel.h"
#include "AllocaMove.h"
#include "ConstTransFold.h"
#include "store_opt.h"
#include "arm_func_opt_v2.h"

template<typename T>
void pass(std::vector<Function *>* functions) {
    T* t = new T(functions);
    t->Run();
    delete t;
}

template<typename T>
void pass(std::vector<Function *>* functions,
          std::unordered_map<GlobalValue *, Initial *>* globals) {
    T* t = new T(functions, globals);
    t->Run();
}




MidEndRunner::MidEndRunner(std::vector<Function *>* functions) {
    this->functions = functions;
}

void MidEndRunner::Run(int opt) {
//    MakeDFG* makeDfg = new MakeDFG(functions);
//    makeDfg->Run();
    
    if (!opt) {
//        Mem2Reg* mem2Reg = new Mem2Reg(functions);
//        mem2Reg->Run();
        pass<AllocaMove> (functions);
        // pass<Mem2Reg>(functions);
        return;
    } else {
//        pass<Mem2Reg>(functions);
//        pass<FuncInline>(functions);
//        pass<MakeDFG>(functions);
////        re_calc();
////        pass<GlobalValueLocalize>(functions, globalValues);
////        pass<Mem2Reg>(functions);
////        base_opt();
//
//        return;
        pass<MakeDFG>(functions);
        re_calc();
        pass<GlobalValueLocalize>(functions, globalValues);
        merge_simple_bb();
        pass<Mem2Reg>(functions);
        if (CenterControl::_OPEN_CONST_TRANS_FOLD) {
            pass<ConstTransFold>(functions);
        }
        pass<MathOptimize>(functions);
        base_opt();

//        output("why.ll");

        pass<MathOptimize>(functions);
        gep_fuse();
        base_opt();

        //array func gvn
        local_array_gvn();
        //func gvn gcm
        func_gvn();
        func_gcm();
        mark_parallel();


        re_calc();
        pass<arm_func_opt>(functions, globalValues);
        // output("debug_inline_2.ll");
        re_calc();
    //    output("func_opt.ll");
        base_opt();

//        output("before\n");
        pass<arm_func_opt_v2>(functions, globalValues);
//        output("after\n");
        re_calc();
        base_opt();

        if (CenterControl::_OPEN_LOOP_SWAP) {
//            output("before_swap.ll");
            pass<loop_swap>(functions);
            re_calc();
//            output("after_swap.ll");
        }


        pass<FuncInline>(functions);
        re_calc();
        pass<GlobalValueLocalize>(functions, globalValues);
        pass<Mem2Reg>(functions);
        pass<MathOptimize>(functions);
        gep_fuse();
        base_opt();

        local_array_gvn();

        pass<MathOptimize>(functions);
        pass<BranchOptimize>(functions);
        re_calc();
        base_opt();

        br_opt();


//        output("before_swap.ll");
//        pass<loop_swap>(functions);
//        re_calc();
//        output("after_swap.ll");
        if (CenterControl::_OPEN_LOOP_SWAP) {
//            output("before_swap.ll");
            pass<loop_swap>(functions);
            re_calc();
//            output("after_swap.ll");
        }


        loop_opt();
        local_array_gvn();
        global_array_gvn();
        //memset_opt
        re_calc();

        br_opt();
        br_opt();
        br_opt();
        br_opt();
        remove_use_same_phi();



        loop_fold();
        loop_fold();
        pass<MathOptimize>(functions);
        base_opt();
//        output("base.ll");
        pass<store_opt>(functions);
        base_opt();
//        output("after.ll");
        loop_strength_reduction();
        re_calc();

        pass<MathOptimize>(functions);
        pass<MathOptimize>(functions);
        base_opt();
//        output("v2_before.ll");
        pass<math_opt_v2>(functions);
        base_opt();
        // output("debug_before.ll");
//        local_array_gvn();
        global_array_gvn();
        base_opt();


        //math math pass
        //invarcode lift
        re_calc();
//        output("before_lift.ll");
        loop_in_var_code_lift();
        loop_fuse();


        //lift
        remove_use_same_phi();
        br_opt();
        rem_2_div_mul_sub();
        if_comb();
        simple_calc();

        do_parallel();
//

        gep_split();
        shorter_lift_time();

        br_opt_use_at_end();

//        output("before_opt.ll");
        pass<pow_2_array_opt>(functions);
        base_opt();
        // output("before_if_comb_v2.ll");
        pass<if_comb_v2>(functions);
        base_opt();
        base_opt();
        simplify_phi();
        re_calc();
        base_opt();
        dlnce();
        re_calc();
        base_opt();
//        pass<store_opt>(functions);

//        load_2_shift();

//        test_peephole();


////        merge_simple_bb();
//        pass<MergeSimpleBB>(functions);
//        pass<MakeDFG>(functions);
//        loop_opt();
//        re_calc();
//        output("after_loop_opt");
//        br_opt();
////        br_opt();
//        pass<BranchOptimize>(functions);
//        re_calc();
////        base_opt();
//        pass<DeadCodeDelete>(functions, globalValues);
//        pass<InstrComb>(functions);
//        pass<ConstFold>(functions, globalValues);
//        pass<DeadCodeDelete>(functions, globalValues);
//        pass<MakeDFG>(functions);
//        pass<GVNAndGCM>(functions);
//

//        br_opt();
//
//        pass<MathOptimize>(functions);

//        pass<ConstFold>(functions, Manager::MANAGER->globals);
//        Mem2Reg* mem2Reg = new Mem2Reg(functions);
//        mem2Reg->Run();

//        ConstFold* constFold = new ConstFold(functions, Manager::MANAGER->globals);
//        constFold->Run();

        re_calc();
        return;
    }
}

//void pass() {
//
//}
void MidEndRunner::base_opt() {
    if (CenterControl::_OPEN_CONST_TRANS_FOLD) {
        pass<ConstTransFold>(functions);
    }
    pass<DeadCodeDelete>(functions, globalValues);
    pass<InstrComb>(functions);
    pass<ConstFold>(functions, globalValues);
    pass<DeadCodeDelete>(functions, globalValues);
    pass<GVNAndGCM>(functions);
}

void MidEndRunner::merge_simple_bb() {
    pass<MergeSimpleBB>(functions);
    pass<MakeDFG>(functions);
}

void MidEndRunner::re_calc() {
    pass<MakeDFG>(functions);
    pass<LoopInfo>(functions);
    pass<LoopIdcVarInfo>(functions);
    pass<FuncInfo>(functions);
}

void MidEndRunner::merge_bb() {

}

void MidEndRunner::if_comb() {
    pass<IfComb>(functions);
    br_opt();
    base_opt();
}

void MidEndRunner::simple_calc() {
    pass<SimpleCalc>(functions);
    br_opt();
    remove_use_same_phi();
    base_opt();
}

void MidEndRunner::loop_opt() {
    pass<LoopInfo>(functions);
    pass<LCSSA>(functions);
    pass<BranchLift>(functions);
    re_calc();
    base_opt();

    pass<LoopIdcVarInfo>(functions);
    pass<LoopUnRoll>(functions);
    re_calc();
    base_opt();
}

void MidEndRunner::br_opt() {
    pass<BranchOptimize>(functions);
    re_calc();
    base_opt();
}

void MidEndRunner::output(std::string filename) {
    Manager::MANAGER->outputLLVM(filename);
}


void MidEndRunner::gep_fuse() {
    pass<GepFuse>(functions);
    pass<DeadCodeDelete>(functions, globalValues);
}

void MidEndRunner::gep_split() {
    pass<GepSplit>(functions);
    base_opt();
}

void MidEndRunner::loop_strength_reduction() {
    pass<LoopStrengthReduction>(functions);
//    output("after.ll");
    re_calc();
    base_opt();
}

void MidEndRunner::loop_fold() {
    pass<LoopFold>(functions);
    re_calc();
    br_opt();
}

void MidEndRunner::loop_fuse() {
//    output("before_fuse.ll");
//    pass<MakeDFG>(functions);
//    output("before_fuse1.ll");
//    pass<LoopInfo>(functions);
//    pass<LoopIdcVarInfo>(functions);
//    pass<FuncInfo>(functions);


    pass<LoopFuse>(functions);
    global_array_gvn();
    local_array_gvn();
    pass<RemoveUselessStore>(functions);
    pass<MidPeepHole>(functions);
//    output("after_fuse.ll");
    base_opt();
}

void MidEndRunner::global_array_gvn() {
    base_opt();
    pass<GlobalArrayGVN>(functions, globalValues);
    base_opt();
}

void MidEndRunner::local_array_gvn() {
    base_opt();
    LocalArrayGVN* localArrayGvn = new LocalArrayGVN(functions, "GVN");
    localArrayGvn->Run();
    base_opt();
}

void MidEndRunner::loop_in_var_code_lift() {
    pass<LoopInVarCodeLift>(functions, globalValues);
    base_opt();

}

void MidEndRunner::func_gvn() {
    pass<AggressiveFuncGVN>(functions, globalValues);
    base_opt();
}

void MidEndRunner::func_gcm() {
    pass<AggressiveFuncGCM>(functions);
    base_opt();
}

void MidEndRunner::remove_use_same_phi() {
    for (Function* function: *functions) {
        for_bb_(function) {
            for_instr_(bb) {
                if (dynamic_cast<INSTR::Phi*>(instr) != nullptr) {
                    bool can_delete = true;
                    Value* base = instr->useValueList[0];
                    for (Value* value: instr->useValueList) {
                        if (base != value) {
                            can_delete = false;
                            break;
                        }
                    }
                    if (can_delete) {
                        instr->modifyAllUseThisToUseA(base);
                        instr->remove();
                    }
                }
            }
        }
    }
}

void MidEndRunner::shorter_lift_time() {
    std::set<Instr*>* can_move = new std::set<Instr*>();
    for (Function* function: *functions) {
        can_move->clear();
        for_bb_(function) {
            for_instr_(bb) {
                if (instr->getBeginUse() == instr->getEndUse()) {
                    Instr* user = instr->getBeginUse()->user;
                    if (dynamic_cast<INSTR::Store*>(user) != nullptr &&
                            dynamic_cast<INSTR::GetElementPtr*>(instr) != nullptr &&
                            user->bb == bb) {
                        can_move->insert(instr);
                    }
//                    if (user->bb == bb) {
//                        user->prev->next = user->next;
//                        user->next->prev = user->prev;
//                        instr->insert_after(user);
//                    }
                }
            }
        }
        for (Instr* instr: *can_move) {
            Instr* user = instr->getBeginUse()->user;
            instr->prev->next = instr->next;
            instr->next->prev = instr->prev;
            user->insert_before(instr);
        }
    }
}

void MidEndRunner::rem_2_div_mul_sub() {
    pass<Rem2DivMulSub>(functions);
    base_opt();
}

void MidEndRunner::br_opt_use_at_end() {
    BranchOptimize* t = new BranchOptimize(functions);
    t->peep_hole();
    re_calc();
}

void MidEndRunner::test_peephole() {
    for (Function* function: *functions) {
        for_bb_(function) {
            if (bb->label != "b30") {
                continue;
            }
            for_instr_(bb) {
                if (instr->operator std::string() ==
                "%v267 = getelementptr inbounds [31 x i32], [31 x i32]* %v224, i32 0, i32 %v532") {
                    INSTR::Alu* shl = new INSTR::Alu(BasicType::I32_TYPE, INSTR::Alu::Op::SHL,
                                                     new ConstantInt(1), instr->useValueList[2], bb);
                    ((Instr*) instr->next)->modifyAllUseThisToUseA(shl);
                    instr->next->insert_before(shl);

                }
            }
        }
    }
//    output("has.ll");
    base_opt();
}

void MidEndRunner::simplify_phi() {
    std::vector<std::pair<int, int>> pairs;
    for_func_(functions) {
        for_bb_(function) {
            for_instr_(bb) {
                if (!is_type(INSTR::Phi, instr)) break;
                int len = instr->useValueList.size();
                pairs.clear();
                for (int i = 0; i < len; ++i) {
                    for (int j = i + 1; j < len; ++j) {
                        if (instr->useValueList[i] == instr->useValueList[j]) {
                            pairs.push_back(std::make_pair(i, j));
                        }
                    }
                }
                if (pairs.size() != 1) {
                    continue;
                }
                int a = pairs.begin()->first, b = pairs.begin()->second;
                BasicBlock *A = (*bb->precBBs)[a], *B = (*bb->precBBs)[b];
                if (B->succBBs->size() != 1) continue;
                if (!B->getBeginInstr()->isBJ()) continue;
                std::vector<Value*>* phi_val = new std::vector<Value*>();
                for (int i = 0; i < len; ++i) {
                    if (i != b) {
                        phi_val->push_back(instr->useValueList[i]);
                    }
                }
                Instr* phi = new INSTR::Phi(instr->type, phi_val, bb);
//                instr->insert_after(phi);
                instr->modifyAllUseThisToUseA(phi);
                instr->remove();
                bb->precBBs->erase(std::remove(bb->precBBs->begin(), bb->precBBs->end(), B),
                                   bb->precBBs->end());
                Instr* jump = B->getBeginInstr();
                jump->delFromNowBB();
                jump->bb = A;
                A->getEndInstr()->insert_before(jump);
                A->getEndInstr()->remove();
                A->succBBs->clear();
                A->succBBs->push_back(bb);
                B->remove();
                return;
            }
        }
    }
}

void MidEndRunner::mark_parallel() {
    if (!CenterControl::_OPEN_PARALLEL) {
        return;
    }
    re_calc();
    pass<AggressiveMarkParallel>(functions);
}

void MidEndRunner::do_parallel() {
    if (!CenterControl::_OPEN_PARALLEL) {
        return;
    }
    re_calc();
    pass<MarkParallel>(functions);
    re_calc();
}

void MidEndRunner::dlnce() {
    for_func_(functions) {
        for_bb_(function) {
            if (!bb->isLoopHeader) continue;
            Loop* loop_A = bb->loop;
            if (!loop_A->isSimpleLoop() || !loop_A->isIdcSet()) continue;
            if (loop_A->childrenLoops->size() != 1) continue;
            Loop* loop_B = *loop_A->childrenLoops->begin();
            int num_A = 0, num_B = 0;
            for (BasicBlock* bb: *loop_A->nowLevelBB) {
                if (bb != loop_A->header && bb != *(loop_A->exitings->begin()) && bb != *(loop_A->latchs->begin())) {
                    num_A++;
                    if (std::find(bb->precBBs->begin(), bb->precBBs->end(), loop_A->header) == bb->precBBs->end()) {
                        num_A++;
                    }
                    if (!bb->getBeginInstr()->isBJ()) {
                        num_A++;
                    }
                }
            }
            for (BasicBlock* bb: *loop_B->nowLevelBB) {
                if (bb != loop_B->header && bb != *(loop_B->exitings->begin()) && bb != *(loop_B->latchs->begin())) {
                    num_B++;
                }
            }
            if (num_A > 1 || num_B > 0) continue;
            std::set<Instr*>* idc_instrs = new std::set<Instr*>();
            add_idc_instr(idc_instrs, loop_A);
            add_idc_instr(idc_instrs, loop_B);
            for (BasicBlock* bb: *loop_A->nowLevelBB) {
                for_instr_(bb) {
                    if (!_contains_(idc_instrs, instr) && !instr->isBJ()) {
                        return;
                    }
                }
            }

            for (BasicBlock* bb: *loop_B->nowLevelBB) {
                for_instr_(bb) {
                    if (!_contains_(idc_instrs, instr) && !instr->isBJ()) {
                        return;
                    }
                }
            }

            BasicBlock* exit = *loop_A->exits->begin();
            BasicBlock* entering = *loop_A->enterings->begin();
            if (entering->succBBs->size() != 1) continue;
            BasicBlock* head = loop_A->getHeader();
            entering->modifySuc(head, exit);
            exit->modifyPre(head, entering);
            entering->getEndInstr()->modifyUse(exit, 0);
            return;
        }
    }
}

void MidEndRunner::add_idc_instr(std::set<Instr *> *instrs, Loop *loop) {
    if (!loop->isSimpleLoop() || !loop->isIdcSet()) return;
    if (is_type(Instr, loop->idcAlu)) instrs->insert((Instr*) loop->idcAlu);
    if (is_type(Instr, loop->idcPHI)) instrs->insert((Instr*) loop->idcPHI);
    if (is_type(Instr, loop->idcCmp)) instrs->insert((Instr*) loop->idcCmp);
    if (is_type(Instr, loop->idcInit)) instrs->insert((Instr*) loop->idcInit);
    if (is_type(Instr, loop->idcEnd)) instrs->insert((Instr*) loop->idcEnd);
    if (is_type(Instr, loop->idcStep)) instrs->insert((Instr*) loop->idcStep);


}



