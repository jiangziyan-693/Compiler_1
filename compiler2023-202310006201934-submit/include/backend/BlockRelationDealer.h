//
// Created by XuRuiyuan on 2023/8/16.
//

#ifndef FASTER_MEOW_BLOCKRELATIONDEALER_H
#define FASTER_MEOW_BLOCKRELATIONDEALER_H

#include <unordered_set>
#include <stack>
#include "util.h"
#include "BasicBlock.h"
#include "algorithm"

class Chain {
public:
    std::vector<BasicBlock *> bbs;
    int nr_edges = 0;
    Chain *prev = nullptr;
    Chain *next = nullptr;

    Chain() = default;

    explicit Chain(BasicBlock *bb) {
        bbs.push_back(bb);
    }

    void remove() const {
        if (prev != nullptr) {
            prev->next = next;
        }
        if (next != nullptr) {
            next->prev = prev;
        }
    }

    void insert_after(Chain *node) {
        node->prev = this;
        node->next = this->next;
        if (next != nullptr) {
            next->prev = node;
        }
        this->next = node;
    }

// node -> this
    void insert_before(Chain *node) {
        node->next = this;
        node->prev = this->prev;
        if (prev != nullptr) {
            prev->next = node;
        }
        this->prev = node;
    }

    void set_prev(Chain *node) {
        this->prev = node;
    }

    void set_next(Chain *node) {
        this->next = node;
    }

    friend int count_edge(const Chain &c1, const Chain &c2) {
        int n = 0;
        for (auto b1: c1.bbs) {
            for (auto b2: c2.bbs) {
                if (std::find(b1->succBBs->begin(), b1->succBBs->end(), b2) != b1->succBBs->end())
                    n++;
            }
        }
        return n;
    }
};

class ChainList {
public:
    Chain *begin;
    Chain *end;

    ChainList() {
        begin = new Chain();
        end = new Chain();
        begin->next = end;
        end->prev = begin;
    }

    void insert_at_end(Chain *chain) const {
        chain->prev = end->prev;
        chain->next = end;
        end->prev->next = chain;
        end->prev = chain;
    }
};

struct CompareChain {
    bool operator()(const Chain *ch1, const Chain *ch2) const {
        return ch1->nr_edges > ch2->nr_edges || ch1->bbs.size() > ch2->bbs.size() || ch1->bbs.front()->id < ch2->bbs.front()->id;
    }
};



class BBSort {
public:
    static void InitializeInfo();
    static void DefaultSort();
    static void EstimateBaseSort();

    static void getBB(BasicBlock *curBB, std::unordered_set<BasicBlock *> &visitBBSet, std::stack<BasicBlock *> &nextBBList);
};


#endif //FASTER_MEOW_BLOCKRELATIONDEALER_H
