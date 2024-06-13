//
// Created by Administrator on 2023/7/26->
//

#ifndef FASTER_MEOW_REGALLOCATOR_H
#define FASTER_MEOW_REGALLOCATOR_H

#include <algorithm>
#include <map>
#include "lir/Arm.h"
#include "set"
#include "stack"
#include "vector"
#include "utility"
#include "MachineInst.h"

class McFunction;

class McProgram;

class McBlock;

class IMI;

enum class DataType;

class AdjPair {
public:
    Operand *u;
    Operand *v;
    std::pair<Operand *, Operand *> p;

    AdjPair(Operand *u, Operand *v) : u(u), v(v) { p = std::make_pair(u, v); }

    bool operator==(const AdjPair &other) const {
        return (u == other.u && v == other.v);
        // || (*u == *other.v && *v == *other.u);
    }


    bool operator!=(const AdjPair &other) const {
        return (u != other.u || v != other.v);
        // || (*u == *other.v && *v == *other.u);
    }

    bool operator<(const AdjPair &other) const {
        // if(*u < *other.u) return true;
        // if(*v < *other.v) return true;

        // if(u->value == 0 && v->value == 0) {
        //     int a = 0;
        // }
        // if (this->operator==(other)) return false;

        /*不满足如u,v分别为Reg,Operand, 切u = other.v, v = other.u*/
        // if((void *) u < (void *)other.u) return true;
        // if((void *) v < (void *)other.v) return true;
        return p < other.p;
    }
};

// namespace std {
//     template<>
//     struct hash<AdjPair> {
//         std::size_t operator()(AdjPair const &adjPair) const noexcept {
//             return hash<int>()(adjPair.u->value) ^ hash<int>()(adjPair.v->value);
//         }
//     };
// }
struct CompareOperand;
struct CompareMI;
class RegAllocator {
public:
    McFunction *curMF;
    // static int SP_ALIGN;
    // static bool DEBUG_STDIN_OUT;
    int K;
    int RK = 14;
    int SK = 32;
    DataType dataType;
    int SPILL_MAX_LIVE_INTERVAL = 1;
    Arm::Reg *rSP = Arm::Reg::getR(Arm::GPR::sp);
    int MAX_DEGREE = 0xfffffffe >> 2;
    std::set<Operand *, CompareOperand> *simplifyWorkSet;
    std::set<Operand *, CompareOperand> *freezeWorkSet;
    std::set<Operand *, CompareOperand> *spillWorkSet;
    std::set<Operand *, CompareOperand> *spilledNodeSet;
    std::set<Operand *, CompareOperand> *coalescedNodeSet;
    std::vector<Operand *> *coloredNodeList;
    std::vector<Operand *> *selectStack;
    std::set<AdjPair> *adjSet;
    std::set<MachineInst *, CompareMI> *coalescedMoveSet;

    std::set<MachineInst *, CompareMI> *constrainedMoveSet;

    std::set<MachineInst *, CompareMI> *frozenMoveSet;

    std::set<MachineInst *, CompareMI> *workListMoveSet;

    std::set<MachineInst *, CompareMI> *activeMoveSet;

    void addEdge(Operand *u, Operand *v);

    virtual void AllocateRegister(McProgram *mcProgram) = 0;

    virtual ~RegAllocator() = default;

    void fixStack(std::vector<IMI *> *needFixList);

    static void liveInOutAnalysis(McFunction *mf);

    void dealDefUse(std::set<Operand *, CompareOperand> *live, MachineInst *mi, McBlock *mb);

    std::set<Operand *, CompareOperand> *adjacent(Operand *x);

    Operand *getAlias(Operand *x);

    void turnInit(McFunction *mf);

    void livenessAnalysis(McFunction *mf);

    static void mixedLivenessAnalysis(McFunction *mf);

    void build();

    std::map<Operand *, int> *newVRLiveLength = new std::map<Operand *, int>();

    void regAllocIteration();

    std::set<MachineInst *, CompareMI> *nodeMoves(Operand *x);

    void decrementDegree(Operand *x);

    void addWorkList(Operand *x);

    void combine(Operand *u, Operand *v);

    void coalesce();

    void freezeMoves(Operand *u);

    std::map<Operand *, Operand *> colorMap;

    // virtual std::set<Arm::Regs> getOkColorSet();
    void preAssignColors();

    void select_spill();

    void simplify();

    void freeze();
};

class GPRegAllocator : public RegAllocator {
public:
    ~GPRegAllocator() override = default;

public:

    GPRegAllocator();

    Operand *vr = nullptr;
    MachineInst *firstUse = nullptr;
    MachineInst *lastDef = nullptr;
    Operand *offImm = nullptr;
    bool toStack = true;
    McBlock *curMB = nullptr;
    int dealSpillTimes = 0;
    std::set<MachineInst *, CompareMI> toInsertMIList;
    std::map<Operand *, Operand *> regMap;

    void AllocateRegister(McProgram *mcProgram) override;

    void dealSpillNode(Operand *x);

    void checkpoint(Operand *x);

    void addDefiners(Operand *o);

    void addDefiners(Operand *o1, Operand *o2);

    Operand *genOpd(Operand *dst);

    Operand *vrCopy(Operand *oldVR);

    void assignColors();

};

class FPRegAllocator : public RegAllocator {
public:
    ~FPRegAllocator() override = default;

public:

    Operand *sVr = nullptr;
    MachineInst *firstUse = nullptr;
    MachineInst *lastDef = nullptr;
    Operand *offImm = nullptr;
    bool toStack = true;
    McBlock *curMB = nullptr;
    int dealSpillTimes = 0;
    std::map<Operand *, Operand *, CompareOperand> spillMap;

    FPRegAllocator();

    void AllocateRegister(McProgram *mcProgram) override;

    void dealSpillNode(Operand *x);

    void checkpoint(Operand *x);

    Operand *vrCopy(Operand *oldVR);

    void assignColors();

};

class NaiveRegAllocator : public RegAllocator {
public:
    virtual ~NaiveRegAllocator() = default;

public:

    int rRegNum = 8;
    int sRegNum = 8;
    Arm::Reg *rRegPool[8] = {};
    Arm::Reg *sRegPool[8] = {};
    int rStackTop = rRegNum;
    int sStackTop = sRegNum;

    NaiveRegAllocator();

    void initPool();

    Arm::Reg *rRegPop();

    Arm::Reg *sRegPop();

    void reset();

    void AllocateRegister(McProgram *program) override;

};

#endif //FASTER_MEOW_REGALLOCATOR_H
