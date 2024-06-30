//
// Created by XuRuiyuan on 2023/7/15.
//
// 该代码定义了一个 Function 类，用于表示编译器中的函数，
// 包含了一系列数据成员和方法，支持函数的各种操作和属性管理。同时还定义了一个内部类 Param，用于表示函数参数。通过这些数据成员和方法，可以实现对函数的详细管理和操作。
#ifndef FASTER_MEOW_FUNCTION_H
#define FASTER_MEOW_FUNCTION_H


#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include "Value.h"
#include "Instr.h"
#include "Type.h"

class BasicBlock;
class Loop;
class Function : public Value {
public:
// 这些是 Function 类的成员变量，用于存储函数的各种属性：
    std::string name;
    bool is_caller = true;
    bool is_deleted = false;
    Type* retType = nullptr;
    bool isPure = false;
    BasicBlock *entry = nullptr;
    BasicBlock *vExit = nullptr; // only for reversed dom tree

public:
// Param 类用于表示函数的参数，包含参数的索引、父函数、加载指令等信息。
    class Param : public Value{
    public:
        // Param *sp;
        static int FPARAM_COUNT;
        int idx;
        Function *parentFunc;
        std::set<Instr *>* loads = new std::set<Instr*>();
    public:
        explicit Param(Type *type, int idx);
        ~Param() override = default;
        virtual explicit operator std::string() const override;
        // static std::vector<Param *> wrapParam(int size, Type * types[]);
//        bool hasBody();
    };


    std::vector<Param *>* params;// 函数参数列表
    BasicBlock *begin;
    BasicBlock *end;// 函数的起始和结束基本块
    std::unordered_map<BasicBlock*, std::vector<BasicBlock*>*>* preMap;
    std::unordered_map<BasicBlock*, std::vector<BasicBlock*>*>* sucMap;// 前驱和后继基本块的映射
    std::set<BasicBlock*>* BBs = new std::set<BasicBlock*>();// 函数包含的基本块和循环头基本块集合
    std::set<BasicBlock*>* loopHeads = new std::set<BasicBlock*>();
    bool isExternal = false;// 标识函数是否是外部函数
public:
    explicit Function(std::string string, std::vector<Param *>* vector, Type *pType);
    explicit Function(bool isExternal, std::string string, std::vector<Param *>* vector, Type *pType);

    ~Function() override = default;
// 用于函数的各种操作，例如插入基本块、获取起始和结束基本块、检查函数是否为空、添加循环头、函数内联、获取函数声明和名称、打印支配树等
    bool hasBody();
    bool hasRet();

    void insertAtBegin(BasicBlock *bb);
    void insertAtEnd(BasicBlock* in);
    BasicBlock* getBeginBB() const;
    BasicBlock* getEndBB() const;
    bool isEmpty();
    void addLoopHead(BasicBlock* bb);
    void inlineToFunc(Function*  tagFUnc,
                      BasicBlock* retBB,
                      INSTR::Call* call,
                      Loop* loop);
    // Function *get_this();
    bool isTimeFunc = false;
    bool getDeleted() const {return is_deleted;}
    bool isCanGVN();

    std::string getDeclare();
    std::string getName() const override;

    friend std::ostream &operator<<(std::ostream &stream, const Function *f);

    std::map<std::pair<BasicBlock*, BasicBlock*>, double> branch_probs;
    std::map<std::pair<BasicBlock*, BasicBlock*>, double> branch_freqs;
    std::vector<BasicBlock *> newBBs;
    std::vector<BasicBlock *> reversedPostOrder;

    void print_dominator_tree() const;
    void print_reversed_dominator_tree() const;
};



#endif //FASTER_MEOW_FUNCTION_H
