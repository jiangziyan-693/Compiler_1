//
// Created by XuRuiyuan on 2023/8/16.
//

#include "util.h"
#include "BlockRelationDealer.h"
#include "Function.h"
#include "BasicBlock.h"
#include "Loop.h"
#include "Manager.h"
#include "stl_op.h"
#include <fstream>
#include <functional>
#include <cmath>

#define FOR_ITER_CHAIN(it, l) for(Chain *it = l.begin->next ; it->next != nullptr; it = it->next)

// sort basic blocks according to Pettis-Hansen Heuristic
void sort_basicblock(Function *f) {
    ChainList chainList;
    BasicBlock *entry = f->getBeginBB();
    FOR_ITER_BB(bb, f) {
        chainList.insert_at_end(new Chain(bb));
    }
    // std::set<BasicBlock *> bbset;
    // for(Chain *c = chainList.begin->next ; c->next != nullptr; c = c->next) {
    //     bbset.insert(c->bbs.front());
    // }
    // std::cerr << "[start]" << bbset.size() << std::endl;
    // std::vector<BasicBlock *> newBBs;
    auto &freqs = f->branch_freqs;
    bool changed = true;
    while (changed) {
        changed = false;
        Chain *head, *tail;
        double maxFreq = 0;
        FOR_ITER_CHAIN(ch1, chainList) {
            FOR_ITER_CHAIN(ch2, chainList) {
                if (ch1 == ch2)
                    continue;
                auto e = std::make_pair(ch1->bbs.back(), ch2->bbs.front());
                if (freqs.find(e) != freqs.end()) {
                    double freq = freqs[e];
                    if (maxFreq < freq) {
                        maxFreq = freq;
                        head = ch1, tail = ch2;
                    }
                }
            }
        }
        if (maxFreq > 0) {
            // merge basic blocks of `tail` into `head`
            for (auto bb: tail->bbs) {
                head->bbs.push_back(bb);
            }
            tail->remove();
            changed = true;
        }
    }
    // bbset.clear();
    // for(Chain *c = chainList.begin->next ; c->next != nullptr; c = c->next) {
    //     for(BasicBlock *bb : c->bbs) {
    //         bbset.insert(bb);
    //     }
    //     // bbset.insert(c->bbs.front());
    // }
    // std::cerr << "[mid]" << bbset.size() << std::endl;

    Chain *entry_chain = nullptr;
    FOR_ITER_CHAIN(it, chainList) {
        if (it->bbs.front() == entry) {
            for (BasicBlock *bb: it->bbs) {
                // f->newBegin->insert_after(bb);
                f->newBBs.push_back(bb);
            }
            entry_chain = it;
            break;
        }
    }

    assert(entry_chain != nullptr);
    entry_chain->remove();
    // if (entry_chain != nullptr) {
    //     entry_chain->remove();
    // }

    FOR_ITER_CHAIN(it1, chainList) {
        it1->nr_edges = 0;
        FOR_ITER_CHAIN(it2, chainList) {
            if (it1 != it2) {
                for (auto b1: it1->bbs) {
                    for (auto b2: it2->bbs) {
                        if (std::find(b1->succBBs->begin(), b1->succBBs->end(), b2) != b1->succBBs->end())
                            it1->nr_edges++;
                    }
                }
            }
        }
    }

    std::multiset<Chain *, CompareChain> chain_set;
    FOR_ITER_CHAIN(ch, chainList) {
        chain_set.insert(ch);
    }
    for (Chain *c: chain_set) {
        for (BasicBlock *bb: c->bbs) {
            f->newBBs.push_back(bb);
        }
    }
}

struct Ctx {
    Function *f;
    std::set<Value *> likely_bool, unlikely_bool;
    std::set<BasicBlock *> call_bbs, store_bbs, ret_bbs;
};

void collect_info(Ctx *ctx) {
    for_bb_(ctx->f) {
        for_instr_(bb) {
            if (instr->tag == ValueTag::call) {
                std::cerr << "[collect_info] bb " << bb->label << " has call " << instr->getName() << std::endl;
                ctx->call_bbs.insert(bb);
            }
            else if (instr->tag == ValueTag::store) {
                std::cerr << "[collect_info] bb " << bb->label << " has store " << instr->getName() << std::endl;
                ctx->store_bbs.insert(bb);
            }
            else if (instr->tag == ValueTag::ret) {
                std::cerr << "[collect_info] bb " << bb->label << " has ret " << instr->getName() << std::endl;
                ctx->ret_bbs.insert(bb);
            }
            else if (instr->tag == ValueTag::icmp) {
                INSTR::Icmp *icmp = (INSTR::Icmp *) instr;
                if (icmp->op == INSTR::Icmp::Op::EQ) {
                    std::cerr << "[collect_info] " << icmp->getName() << "@" << bb->label << " is eq (unlikely)" << std::endl;
                    ctx->unlikely_bool.insert(icmp);
                }
                else if (icmp->op == INSTR::Icmp::Op::NE) {
                    std::cerr << "[collect_info] " << icmp->getName() << "@" << bb->label << " is ne (likely)" << std::endl;
                    ctx->likely_bool.insert(icmp);
                }
                else {
                    if (icmp->getRVal2()->isConstantInt() && ((ConstantInt *)(icmp->getRVal2()))->get_const_val() == 0) {
                        if (icmp->op == INSTR::Icmp::Op::SLT || icmp->op == INSTR::Icmp::Op::SLE) {
                            std::cerr << "[collect_info] " << icmp->getName() << "@" << bb->label << " is <=0 (unlikely)" << std::endl;
                            ctx->unlikely_bool.insert(icmp);
                        }
                        else if (icmp->op == INSTR::Icmp::Op::SGT || icmp->op == INSTR::Icmp::Op::SGE) {
                            std::cerr << "[collect_info] " << icmp->getName() << "@" << bb->label << " is >=0 (likely)" << std::endl;
                            ctx->likely_bool.insert(icmp);
                        }
                    }
                }
            }
        }
    }
}

typedef std::function<bool (BasicBlock *)> BranchPredicator;

inline static double logistic(const double x, const double k = 1.0, const double x0 = 0.0) {
    return 1.0 / (1.0 + exp(-k * (x - x0)));
}

bool common_prob_fix(Ctx *ctx, const BranchPredicator& predict, const INSTR::Branch *br, double *prob, const double p0) {
    if (predict(br->thenTarget)) {
        *prob = p0;
        return true;
    }
    if (predict(br->elseTarget)) {
        *prob = 1 - p0;
        return true;
    }
    return false;
}

bool likely_branch(Ctx *ctx, BasicBlock *bb, const INSTR::Branch *br, double *prob) {
    const double p0 = 0.84;
    if (ctx->likely_bool.count(br->getCond())) {
        *prob = p0;
        return true;
    }
    if (ctx->unlikely_bool.count(br->getCond())) {
        *prob = 1 - p0;
        return true;
    }
    return false;
}

bool loop_inside_to_header(Ctx *ctx, BasicBlock *bb, const INSTR::Branch *br, double *prob) {
    // bb should inside loop
    if (!bb->loop || (bb->loop->getLoopDepth() <= 0))
        return false;
    Loop *loop = bb->loop;
    std::cerr << "[BlockRelation::loop_inside_to_header] bb(" << bb->label << ")@loop(depth=" << loop->loopDepth << ", header=" << loop->getHeader()->label << ")" << std::endl; 
    double p0 = logistic(loop->loopDepth, 1.0, -1.2);
    if (br->getThenTarget() == loop->getHeader() && br->getElseTarget()->loop == loop) {
        *prob = p0;
        std::cerr << "[BlockRelation::loop_inside_to_header] br(" << std::string(*br) << ")@bb(" << bb->label << ")"
            << ".thenTarget=" << br->getThenTarget()->label << ", p=" << *prob << std::endl;
        return true;
    }
    if (br->getElseTarget() == loop->getHeader() && br->getThenTarget()->loop == loop) {
        *prob = 1 - p0;
        std::cerr << "[BlockRelation::loop_inside_to_header] br(" << std::string(*br) << ")@bb(" << bb->label << ")"
            << ".elseTarget=" << br->getThenTarget()->label << ", p=" << *prob << std::endl;
        return true;
    }
    return false;
}

// bool loop_header(Ctx *ctx, BasicBlock *bb, const INSTR::Branch *br, double *prob) {
//     double p0 = 0.79;
//     if (!bb->loop || (bb->loop->getLoopDepth() <= 0))
//         return false;
//     Loop *loop = bb->loop;
//     if (loop->getHeader() != bb)
//         return false;
//     if ()
    
// }


bool loop_break(Ctx *ctx, BasicBlock *bb, const INSTR::Branch *br, double *prob) {
    double p0 = 0.79;
    if (!bb->loop || (bb->loop->getLoopDepth() <= 0))
        return false;
    Loop *loop = bb->loop;
    if (br->getThenTarget() == loop->getHeader() || br->getElseTarget() == loop->getHeader())
        return false;
    if (br->getThenTarget()->loop == loop || br->getThenTarget()->loop == loop)
        return false;
    BranchPredicator predict = [&] (BasicBlock *bb2) {
        if (bb2->loop == loop) {
            std::cerr << "[loop_break] br(" << std::string(*br) << ")@bb(" << bb->label << ")"
                << "breaks loop with header (" << loop->getHeader()->label << ")" << std::endl;
            return true;
        }
        return false;
    };
    return common_prob_fix(ctx, predict, br, prob, p0);
}

bool loop_enter_deeper(Ctx *ctx, BasicBlock *bb, const INSTR::Branch *br, double *prob) {
    double p0 = 0.89;
    Loop *thenLoop = br->getThenTarget()->loop, *elseLoop = br->getElseTarget()->loop;
    std::cerr << "[loop_enter_deeper] br(" << std::string(*br) << ")@bb(" << bb->label << ")"
        << ".then.depth = " << thenLoop->getLoopDepth() << ", .else.depth = " << elseLoop->getLoopDepth() << std::endl;
    if (thenLoop->getLoopDepth() > elseLoop->getLoopDepth()) {
        *prob = p0;
        return true;
    }
    else if (thenLoop->getLoopDepth() < elseLoop->getLoopDepth()) {
        *prob = 1 - p0;
        return true;
    }
    return false;
}

bool instr_count(Ctx *ctx, BasicBlock *bb, const INSTR::Branch *br, double *prob) {
    const double p0 = 0.34;
    int count_then = 0, count_else = 0;
    for_instr_(br->getThenTarget()) {
        count_then++;
    }
    for_instr_(br->getElseTarget()) {
        count_else++;
    }
    if (count_then > count_else) {
        *prob = p0;
        return true;
    } else if (count_then < count_else) {
        *prob = 1 - p0;
        return true;
    }
    return false;
}

// bool loop_header(Ctx *ctx, BasicBlock *bb, const INSTR::Branch *br, double *prob) {
//     const double p0 = 0.75;
//     BranchPredicator is_loop_header = [](BasicBlock *bb) {
//         return bb->loop && bb == bb->loop->header;
//     };
//     BranchPredicator is_loop_preheader = [&] (BasicBlock *bb2) {
//         Instr *terminator = bb2->getEndInstr();
//         if (terminator->tag == ValueTag::jump)
//             return is_loop_header(((INSTR::Jump *)terminator)->getTarget());
//         return false;
//     };
//     BranchPredicator apply = [&] (BasicBlock *bb2) {
//         return (is_loop_header(bb2) || is_loop_preheader(bb2)) && (bb2->rdoms->count(bb));
//     };
//     return common_prob_fix(ctx, apply, br, prob, p0);
// }

bool call_block(Ctx *ctx, BasicBlock *bb, const INSTR::Branch *br, double *prob) {
    const double p0 = 1 - 0.78;
    BranchPredicator predict = [&] (BasicBlock *bb2) {
        if (ctx->call_bbs.count(bb2) /* && bb2->rdoms->count(bb) */) {
            return true;
        }
        return false;
    };
    return common_prob_fix(ctx, predict, br, prob, p0);
}

bool store_block(Ctx *ctx, BasicBlock *bb, const INSTR::Branch *br, double *prob) {
    const double p0 = 1 - 0.55;
    BranchPredicator predict = [&] (BasicBlock *bb2) {
        if (ctx->store_bbs.count(bb2) /* && bb2->rdoms->count(bb) */) {
            return true;
        }
        return false;
    };
    return common_prob_fix(ctx, predict, br, prob, p0);
}

bool return_block(Ctx *ctx, BasicBlock *bb, const INSTR::Branch *br, double *prob) {
    const double p0 = 1 - 0.82;
    BranchPredicator predict = [&] (BasicBlock *bb2) {
        if (ctx->ret_bbs.count(bb2) /* && bb2->rdoms->count(bb) */) {
            std::cerr << "[return_block] br(" << std::string(*br) << ") jump to return bb " << bb2->label << std::endl;
            return true;
        }
        return false;
    };
    return common_prob_fix(ctx, predict, br, prob, p0);
}

void calc_probs(Ctx *ctx) {
    for_bb_(ctx->f) {
        Instr *terminator = bb->getLast();
        if (terminator->isBranch()) {
            std::vector<double> vp;
            INSTR::Branch *br = (INSTR::Branch *) terminator;
            double p1 = 0.5, p2 = 0.5;
            double p;
            std::string sb = bb->label, st = br->getThenTarget()->label, sf = br->getElseTarget()->label;
            
            std::vector< std::function<bool (Ctx *, BasicBlock *, const INSTR::Branch *, double *)> > patterns = {
                loop_inside_to_header,
                loop_break,
                loop_enter_deeper,
                instr_count,
                call_block,
                store_block,
                return_block,
            };
            for (auto &patt: patterns) {
                if (patt(ctx, bb, br, &p)) {
                    vp.push_back(p);
                }
            }
            for (double p: vp) {
                double d = p1 * p + p2 * (1 - p);
                p1 = p1 * p / d;
                p2 = p2 * (1 - p) / d;
            }
            std::cerr << "[compute_branch_probabilities] (" << sb << " ? " << st << " : " << sf << "): ";
            for (double p: vp) {
                std::cerr << "p(" << p << ")" << "; ";
            }
            std::cerr << "p1=" << p1 << ",p2=" << p2 << std::endl;
            assert(std::abs(p1 + p2 - 1.0) < 1e-6);  // p1 + p2 = 1
            ctx->f->branch_probs[{bb, br->getThenTarget()}] = p1;
            ctx->f->branch_probs[{bb, br->getElseTarget()}] = p2;
        } else if (terminator->isJump()) {
            INSTR::Jump *jump = (INSTR::Jump *) terminator;
            ctx->f->branch_probs[{bb, jump->getTarget()}] = 1;
        }
    }
}

void calc_freqs(Ctx *ctx) {
    for (auto &entry : ctx->f->branch_probs) {
        BasicBlock *bb1 = entry.first.first, *bb2 = entry.first.second;
        Loop *loop1 = bb1->loop, *loop2 = bb2->loop;
        ctx->f->branch_freqs[{bb1, bb2}] = ctx->f->branch_probs[{bb1, bb2}] * ((loop1->getLoopDepth() + 1) * (loop2->getLoopDepth() + 1));
    }
}

// void calc_freqs(Ctx *ctx) {
//     for_bb_(ctx->f) {
//         bb->freq = 0;
//     }

//     const int T = 1;
//     BasicBlock *entry = ctx->f->getBeginBB();
//     for (int i = 0; i < T; i++) {
//         // std::cerr << "i = " << i << std::endl;
//         for (BasicBlock *bb: ctx->f->reversedPostOrder) {
//             // std::cerr << "[call freqs] visit " << bb->label << std::endl;
//             if (bb == entry)
//                 bb->freq = 1;
//             else {
//                 double freq = 0;
//                 for (BasicBlock *pb : *bb->precBBs) {
//                     freq += ctx->f->branch_freqs[{pb, bb}];
//                 }
//                 // std::cerr << "[call freqs] freq " << bb->label << " " << freq << std::endl;
//                 bb->freq = freq;
//             }

//             for (BasicBlock *sb : *bb->succBBs) {
//                 if (ctx->f->branch_probs.count({bb, sb})) {
//                     ctx->f->branch_freqs[{bb, sb}] = bb->freq * ctx->f->branch_probs[{bb, sb}];
//                 } else {
//                     assert(sb == ctx->f->vExit);
//                     assert(sb->succBBs->empty());
//                 }
//             }
//         }
//     }
// }

void block_relation_init(Function *f) {
    Ctx ctx;
    ctx.f = f;
    collect_info(&ctx);
    calc_probs(&ctx);
    calc_freqs(&ctx);
}

void BBSort::InitializeInfo() {
    for (Function *f: (*Manager::MANAGER->getFunctionList())) {
        block_relation_init(f);
    }
}

void BBSort::getBB(BasicBlock *curBB, std::unordered_set<BasicBlock *> &visitBBSet, std::stack<BasicBlock *> &nextBBList) {
    Instr *instr = curBB->getEndInstr();
    if (instr->tag == ValueTag::jump) {
        BasicBlock *bb = ((INSTR::Jump *) instr)->getTarget();
        if (visitBBSet.find(bb) == visitBBSet.end()) {
            nextBBList.push(bb);
            visitBBSet.insert(bb);
        }
    } else if (instr->tag == ValueTag::branch) {
        INSTR::Branch *brInst = (INSTR::Branch *) instr;
        BasicBlock *bb;
        if (ConstantBool *p = dynamic_cast<ConstantBool *>(brInst->getCond())) {
            if ((int) p->get_const_val() == 0) {
                bb = brInst->getElseTarget();
            } else {
                bb = brInst->getThenTarget();
            }
            if (visitBBSet.find(bb) == visitBBSet.end()) {
                visitBBSet.insert(bb);
                nextBBList.push(bb);
            }
        } else {
            BasicBlock *trueMcBlock = brInst->getThenTarget();
            BasicBlock *falseMcBlock = brInst->getElseTarget();
            if (visitBBSet.find(falseMcBlock) == visitBBSet.end()) {
                nextBBList.push(falseMcBlock);
                visitBBSet.insert(falseMcBlock);
            }
            if (visitBBSet.find(trueMcBlock) == visitBBSet.end()) {
                visitBBSet.insert(trueMcBlock);
                nextBBList.push(trueMcBlock);
            }
        }
    }
}
void BBSort::DefaultSort() {
    for (Function *f: (*Manager::MANAGER->getFunctionList())) {
        std::stack<BasicBlock *> nextBBList;
        std::unordered_set<BasicBlock *> visitBBSet;
        nextBBList.push(f->getBeginBB());
        while (!nextBBList.empty()) {
            BasicBlock *visitBB = nextBBList.top();
            f->newBBs.push_back(visitBB);
            nextBBList.pop();
            getBB(visitBB, visitBBSet, nextBBList);
        }
    }
}


void BBSort::EstimateBaseSort() {
    for (Function *f: (*Manager::MANAGER->getFunctionList())) {
        sort_basicblock(f);
    }
}