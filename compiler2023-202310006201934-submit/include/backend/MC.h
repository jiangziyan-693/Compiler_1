//
// Created by XuRuiyuan on 2023/7/22.
//

#ifndef FASTER_MEOW_MC_H
#define FASTER_MEOW_MC_H

#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include "../mir/Value.h"
#include "../mir/Instr.h"
#include "../mir/Type.h"
#include "../mir/GlobalVal.h"
#include "../lir/Operand.h"

namespace Arm {

    // enum class Cond;
    // enum class ShiftType;

    enum class GPR;

    class Regs;

    class Reg;

    class Glob;

    class Shift;

    class GPRs;

    class FPRs;
}
class MachineInst;

class McFunction;

class IMI;

class McBlock;

class Operand;

class MachineMove;

class Function;

class McProgram {
public:
    // ~McProgram() override = default;
    McProgram();

public:

    static McProgram *MC_PROGRAM;
    // Ilist <McFunction> funcList = new Ilist<>();
    McFunction *beginMF;
    McFunction *endMF;
    std::vector<Arm::Glob *> globList;
    McFunction *mainMcFunc = nullptr;
    std::vector<IMI *> needFixList;
    static MachineInst *specialMI;
    std::vector<Arm::Glob *> globData;
    std::vector<Arm::Glob *> globBss;

    void bssInit();

    friend std::ostream &operator<<(std::ostream &stream, const McProgram *p);

    static void globalDataStbHelper(std::ostream &stb, const std::vector<Arm::Glob *> *globData);

    void output(std::string filename) const;
};

struct Location {
    McBlock *mb = nullptr;
    MachineInst *locatedMI = nullptr;
    int index = -1;

    bool operator<(Location const &other) const {
        return mb == other.mb ? index < other.index : mb < other.mb;
    }
};

class McFunction : public ILinkNode {
public:
    McFunction();

    explicit McFunction(Function *function);

    explicit McFunction(std::string name);

    ~McFunction() override = default;

public:

    // Ilist <McBlock> mbList = new Ilist<>();
    McBlock *beginMB;
    McBlock *endMB;
    int floatParamCount = 0;
    int intParamCount = 0;
    bool isMain = false;
    int vrCount = 0;
    int sVrCount = 0;
    int varStack = 0;
    int paramStack = 0;
    int regStack = 0;
    Function *mFunc = nullptr;
    unsigned int usedCalleeSavedGPRs = 0;
    unsigned int usedCalleeSavedFPRs = 0;
    bool useLr = false;
    std::string name = "";
    int allocStack = -1;
    std::vector<Operand *> vrList;
    std::vector<Operand *> sVrList;

    std::deque<SpBasedOffAndSize *> param_objs;
    std::deque<SpBasedOffAndSize *> normal_objs;
    std::map<Operand *, RegValue, CompareOperand> reg2val; // 记录一些单赋值虚拟寄存器的取值
    std::map<int, Operand *> intVal2vr;
    std::map<Arm::Glob *, Operand *> glob2vr;
    MachineInst *functionInsertedMI;

    std::map<Operand *, BKConstVal, CompareOperand> constMap;
    std::map<INSTR::Phi *, Operand *> phi2tmpMap;

    std::optional<int> get_imm(Operand *o);

    McBlock *getBeginMB();

    void insertAtEnd(McBlock *mb);

    McBlock *getEndMB();

    // std::vector<Arm::GPRs *> getUsedRegList();

    std::string getName();

    void addVarStack(int i);

    void addParamStack(int i);

    void alignParamStack();

    void addRegStack(int i);

    int getTotalStackSize();

    int getParamStack();

    void setAllocStack();

    int getAllocStack();

    int getVarStack();

    int getVRSize();

    int getSVRSize();

    Operand *getVR(int idx);

    Operand *getSVR(int idx);

    Operand *newVR();

    Operand *newSVR();

    void clearVRCount();

    void clearSVRCount();

    void addUsedGPRs(Arm::Regs *reg);

    void addUsedGPRs(Arm::GPR gid);

    void addUsedFRPs(Arm::Regs *reg);

    void setUseLr();

    int getRegStack() const;

    void alignTotalStackSize();

    void buildDefUse();

    std::optional<float> get_fconst(Operand *pOperand);

    std::unordered_map<Operand *, std::set<Location>> vrDefs;
    std::unordered_map<Operand *, std::set<Location>> vrUses;

    void remove_phi();

    std::map<Value *, Operand *> value2Opd;
};

class McBlock : public ILinkNode {
public:
    ~McBlock() override = default;

    McBlock();

    McBlock(McBlock *curMB, McBlock *pred);

    McBlock(BasicBlock *bb);

    McBlock(std::string label, McFunction *insertAtEnd);

public:

    static std::string MB_Prefix;
    BasicBlock *bb = nullptr;
    McFunction *mf = nullptr;
    // Ilist <MachineInst> miList = new Ilist<>();
    MachineInst *beginMI{};
    MachineInst *endMI{};
    static int globIndex;
    int miNum = 0;
    int mb_idx = 0;
    std::string label = "";
    std::unordered_set<McBlock *> predMBs;
    std::vector<McBlock *> succMBs;
    std::unordered_set<Operand *> *liveUseSet{};
    std::unordered_set<Operand *> *liveDefSet{};
    std::unordered_set<Operand *> *liveInSet{};
    std::unordered_set<Operand *> *liveOutSet{};

    MachineInst *getBeginMI() const;

    MachineInst *getEndMI() const;

    void insertAtEnd(MachineInst *in);

    void insertAtHead(MachineInst *in);

    std::string getLabel() const;

    void setMf(McFunction *mf);

    McBlock *falseSucc();

    McBlock *trueSucc();

    void setFalseSucc(McBlock *onlySuccMB);

    void setTrueSucc(McBlock *onlySuccMB);

    bool operator==(const McBlock &other) const {
        return this->id == other.id;
    }

    bool operator!=(const McBlock &other) const {
        return this->id != other.id;
    }

    bool operator<(const McBlock &other) const {
        return this->id < other.id;
    }


};


#endif //FASTER_MEOW_MC_H
