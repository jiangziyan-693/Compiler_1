//
// Created by XuRuiyuan on 2023/7/15.
//

// 该代码定义了一个 BasicBlock 类，用于表示编译器中的基本块，包含了一系列数据成员和方法，支持基本块的各种操作和属性管理
#ifndef FASTER_MEOW_BASICBLOCK_H
#define FASTER_MEOW_BASICBLOCK_H

#include "Value.h"
#include "../util/IList.h"
// #include "Loop.h"
#include <vector>
#include <set>
// #include "Instr.h"
// 这些是 BasicBlock 类的成员变量，用于存储基本块的各种属性：
class Instr;
class Loop;

class Function;// 指向所属的函数
class Loop;// 指向所属循环
class McBlock;
class BasicBlock : public Value {
public:
    // std::shared_ptr<BasicBlock> sp;
    Function *function;
    // 指向基本块的开始和结束指令
    Instr *begin;
    Instr *end;
    Loop *loop;
    // Coll::IList<Instr> instr_list;
    std::string label;

    std::vector<BasicBlock*>* precBBs = new std::vector<BasicBlock*>();//前驱和后继基本块集合
    std::vector<BasicBlock*>* succBBs = new std::vector<BasicBlock*>();

    std::set<BasicBlock*>* doms = new std::set<BasicBlock*>();// 支配关系集合
    std::set<BasicBlock*>* idoms = new std::set<BasicBlock*>();
    std::set<BasicBlock*>* rdoms = new std::set<BasicBlock*>();             // reversed dom tree
    std::set<BasicBlock*>* irdoms = new std::set<BasicBlock*>();
    std::set<BasicBlock*>* DF = new std::set<BasicBlock*>();

    bool isLoopHeader = false;
    bool isLoopEntering = false;
    bool isLoopExiting = false;
    bool isLoopLatch = false;
    bool isExit = false;

    int domTreeDeep;// 支配树的深度
    BasicBlock* iDominator;// 支配和逆支配基本块
    BasicBlock* iRDominator;    // reversed dom tree

    static int bb_count;// 用于统计基本块数量
    static int empty_bb_count;
    McBlock *mb;// 指向 McBlock 的指针
public:
    ~BasicBlock() override = default;
    BasicBlock();
    BasicBlock(Function *function, Loop *loop, bool insertAtEnd = true);

    //TODO:Loop
    //这些是 BasicBlock 类的方法，用于基本块的各种操作，例如插入指令、设置前驱后继基本块、修改支配关系、克隆基本块、删除基本块、设置循环相关属性等
    int getLoopDep();

    void setFunction(Function *function, Loop *loop);
    void init();
    Instr* getEntry();
    Instr* getLast();
    void insertAtEnd(Instr* in);
    void insertAtHead(Instr* in);
    bool isTerminated();

    Instr* getBeginInstr();
    Instr* getEndInstr();
    void setPrecBBs(std::vector<BasicBlock*>* precBBs);
    void setSuccBBs(std::vector<BasicBlock*>* succBBs);
    void modifyLoop(Loop* loop);
//    void setDoms(std::set<BasicBlock*> doms);
//    void setIdoms(std::set<BasicBlock*> idoms);
//    void setDF(std::set<BasicBlock*> DF);
//    void setDomTreeDeep(int doomTreeDeep);
//    void setIDominator(BasicBlock* iDominator);
//
//    std::vector<BasicBlock*> getPrecBBs();
//    std::vector<BasicBlock*> getSuccBBs();
//    std::set<BasicBlock*> getDoms();
//    std::set<BasicBlock*> getIdoms();
//    std::set<BasicBlock*> getDF();
//    int getDomTreeDeep();
//    BasicBlock* getIDominator();

    BasicBlock* cloneToFunc(Function* function);
    BasicBlock* cloneToFunc(Function* function, Loop* loop);
    BasicBlock* cloneToFunc_LUR(Function* function, Loop* loop);

    void fixPreSuc(BasicBlock* oldBB);
    void fix();
    void modifyPre(BasicBlock* old, BasicBlock* now);
    void modifySuc(BasicBlock* old, BasicBlock* now);
    void modifyPres(std::vector<BasicBlock*>* precBBs);
    void modifySucs(std::vector<BasicBlock*>* succBBs);
    void addPre(BasicBlock* add);
    void addSuc(BasicBlock* add);
    void simplyPhi(std::vector<BasicBlock*>* oldPres, std::vector<BasicBlock*>* newPres);
    bool arrayEq(std::vector<BasicBlock*>* oldPres, std::vector<BasicBlock*>* newPres);



    void modifySucAToB(BasicBlock* A, BasicBlock* B);
    void modifyBrAToB(BasicBlock* A, BasicBlock* B);
    void modifyBrToJump(BasicBlock* next);
    void remove() override;
    McBlock *getMb() const {return mb;}

    void setLoopStart();
    void setLoopHeader();
    void setLoopEntering();
    void setLoopExiting();
    void setLoopLatch();
    void setExit();
    void setNotHead();
    void setNotLatch();


    virtual explicit operator std::string() const override;

    double freq;

    // std::shared_ptr<BasicBlock> get_this();
};


#endif //FASTER_MEOW_BASICBLOCK_H