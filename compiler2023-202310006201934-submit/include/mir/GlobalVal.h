// 该代码定义了一系列用于表示全局值的类，包括 GlobalVal 及其派生类 GlobalValue、UndefValue 和 VirtualValue。
// 这些类用于表示编译器中的各种全局变量及其属性和行为，并提供了相应的字符串转换操作。
#ifndef FASTER_MEOW_GLOBALVAL_H
#define FASTER_MEOW_GLOBALVAL_H

#include "string"
#include "../frontend/AST.h"
#include "../frontend/Initial.h"

class Value;

class Type;

class GlobalVal : public Value {// 表示该类及其派生类必须实现字符串转换操作。还有一个带有类型参数的构造函数和一个默认构造函数
public:
    explicit GlobalVal(Type *type);

    GlobalVal() = default;

    ~GlobalVal() override = default;

    virtual explicit operator std::string() const override = 0;

};

class GlobalValue : public GlobalVal {// 用于表示具体的全局变量
public:
    ~GlobalValue() override = default;

    GlobalValue(Type *pointeeType, AST::Def *def, Initial *initial);

    GlobalValue(Type *pointeeType, std::string name, Initial *initial);

public:

    AST::Def *def = nullptr;
    Initial *initial = nullptr;
    bool local = false;

    void setCanLocal();

    bool canLocal() const;

    virtual explicit operator std::string() const override;
};

struct CompareGlobalValue {// 是一个函数对象，用于比较两个 GlobalValue 指针。它重载了 operator()，以便在需要排序或比较 GlobalValue 对象时使用。
    bool operator()(const GlobalValue *lhs, const GlobalValue *rhs) const;
};

class UndefValue : public GlobalVal {// UndefValue 类继承自 GlobalVal 类，用于表示未定义的全局值。
public:
    ~UndefValue() override = default;

    UndefValue() = default;

    explicit UndefValue(Type *type) : GlobalVal(type) {}

public:

    static int undefValueCnt;
    std::string label = "";
    std::string name = "";

    std::string getLabel() const;
    virtual std::string getName() const override;

    virtual explicit operator std::string() const override;
};

class VirtualValue : public GlobalVal {// VirtualValue 类继承自 GlobalVal 类，用于表示虚拟的全局值。
public:
    ~VirtualValue() override = default;

public:

    static int virtual_value_cnt;

    explicit VirtualValue(Type *type);

    virtual explicit operator std::string() const override;
};

#endif //FASTER_MEOW_GLOBALVAL_H
