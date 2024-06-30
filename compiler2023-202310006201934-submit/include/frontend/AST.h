//定义了一个抽象语法树（AST）相关的类结构，通常用于编译器或解释器中来表示源代码的结构
//文件头和预处理指令
// Created by Administrator on 2023/7/13.
//

#ifndef FASTER_MEOW_AST_H
#define FASTER_MEOW_AST_H

#include "vector"
#include "Token.h"
//宏__SS__(T)定义了一个快捷方式，用于定义默认构造函数和析构函数。
#define __SS__(T) public: T() = default; ~T() override = default;
//所有的类都在AST命名空间下，以避免命名冲突。
namespace AST {
    // class Decl;

    class Def;

    class CompUnit;
//Init和PrimaryExp是基础类，提供默认的构造函数和析构函数。
    class Init {
    public:
        Init() = default;

        virtual ~Init() = default;
    };

    class InitArray;

    class FuncDef;


    class Block;

    class BlockItem {
    public:
        virtual ~BlockItem() = default;
    };

    class Stmt : public BlockItem {
    public:
        Stmt() = default;

        ~Stmt() override = default;
    };
    //
    // class Assign;
    //
    // class ExpStmt;
    //
    // class IfStmt;
    //
    // class WhileStmt;
    //
    // class Continue;
    //
    // class Return;

    //Exp类继承自Init和PrimaryExp，表示一个通用的表达式。
    class PrimaryExp {
    public:
        PrimaryExp() = default;

        virtual ~PrimaryExp() = default;
    };

    class Exp : public Init, public PrimaryExp {
    public:
        Exp() = default;

        ~Exp() override = default;
    };

    // class BinaryExp;
    //
    // class UnaryExp;
    //
    //
    // class LVal;
    //
    // class Number;
    //
    // class Call;
    // Ast类包含一个CompUnit的向量，表示整个抽象语法树。
    class Ast {
    public:
        std::vector<CompUnit *> units;

        Ast() = default;

        virtual ~Ast() = default;
    };
    // CompUnit类是所有组件单元的基类。
    class CompUnit {
    public:
        CompUnit() = default;

        virtual ~CompUnit() = default;
    };
    
    // Decl类表示一个声明，包含是否为常量、类型和定义的向量
    class Decl : public CompUnit, public BlockItem {
    public:
        bool constant;
        Token *bType;
        std::vector<Def *> defs;

        Decl() = default;
        // Decl(bool constant, Token *bType) : constant(constant), bType(bType) {}

        ~Decl() override = default;
    };
    
    // Def类表示一个定义，包含类型、标识符和初始化列表
    class Def {
    public:
        Token::TokenType bType;
        Token *ident;
        std::vector<Exp *> indexes;
        Init *init;

        Def() = default;

        ~Def() = default;
    };

    class InitArray : public Init {
    public:
        std::vector<Init *> init;
        int nowIdx = 0;

        ~InitArray() override = default;

        Init *getNowInit() {
            return this->init[nowIdx];
        }
    };

    class FuncFParam;
    
    // FuncDef类表示一个函数定义，包含返回类型、标识符、参数列表和函数体
    class FuncDef : public CompUnit {
    public:
        Token *type;
        Token *ident;
        std::vector<FuncFParam *> fParams;
        Block *body;

        FuncDef() = default;

        ~FuncDef() override = default;
    };

    class FuncFParam {//参数的定义
    public:
        Token *bType;
        Token *ident;
        bool array;
        std::vector<Exp *> sizes;
    };
    
    //Block表示一个代码块的定义
    class Block : public Stmt {//Stmt表示语句的定义
    public:
        Block() = default;

        ~Block() override = default;

        std::vector<BlockItem *> items;
    };

    class LVal;
    //Stmt语句的子类，表示源代码中的赋值操作
    class Assign : public Stmt {
    public:
        LVal *left;
        Exp *right;
    };

    //表达式语句
    class ExpStmt : public Stmt {
    public:
        Exp *exp;

        ExpStmt() = default;

        ~ExpStmt() override = default;
    };

    // if语句的定义
    class IfStmt : public Stmt {
    public:
        Exp *cond;
        Stmt *thenTarget;
        Stmt *elseTarget;

        IfStmt() = default;

        ~IfStmt() override = default;
    };
    
    // while语句的定义
    class WhileStmt : public Stmt {
    public:
        Exp *cond;
        Stmt *body;

        WhileStmt() = default;

        ~WhileStmt() override = default;
    };
    
    //break语句的定义
    class Break : public Stmt {
    __SS__(Break)
    };
    
    //continue语句的定义
    class Continue : public Stmt {
    __SS__(Continue)
    };
    
    //return语句的定义
    class Return : public Stmt {
    __SS__(Return)

        Exp *value;
    };
    
    //复合语句的定义
    class BinaryExp : public Exp {
    __SS__(BinaryExp)

        Exp *first;
        std::vector<Token *> operators;
        std::vector<Exp *> follows;
    };
    
    //一元表达式定义
    class UnaryExp : public Exp {
    __SS__(UnaryExp)

        std::vector<Token *> unaryOps;
        PrimaryExp *primaryExp;
    };

    // 表达式左值的定义
    class LVal : public PrimaryExp {
    __SS__(LVal)

        Token *ident;
        std::vector<Exp *> indexes;
    };

    // Number用于表示常量
    class Number : public PrimaryExp {
    public:
        Number(Token *number) {
            this->number = number;

            //处理整数常量
            if (number->is_int_const()) {
                isIntConst = true;

                if (number->token_type == Token::HEX_INT)
                    intConstVal = std::stoi(number->content.substr(2), nullptr, 16);
                else if (number->token_type == Token::OCT_INT)
                    intConstVal = std::stoi(number->content.substr(1), nullptr, 8);
                else if (number->token_type == Token::DEC_INT)
                    intConstVal = std::stoi(number->content);
                // intConstVal = std::stoi(number->content);
                floatConstVal = (float) intConstVal;
            } else if (number->is_float_const()) {//处理浮点数常量
                isFloatConst = true;
                // todo : may bug !!!
                floatConstVal = std::stof(number->content);
                intConstVal = (int) floatConstVal;
            } else {//处理无效常量
                exit(5);
            }
        }

        ~Number() override = default;

        Token *number;
        bool isIntConst = false;
        bool isFloatConst = false;
        int intConstVal = 0;
        float floatConstVal = 0.0f;

        friend std::ostream &operator<<(std::ostream &stream, const Number *number1) {

            if (number1->isIntConst) {
                stream << "int " << number1->intConstVal;
            } else if (number1->isFloatConst) {
                stream << "float" << number1->floatConstVal;
            } else {
                stream << "???" << number1->number;
            }
            return stream;
        }
    };
    // Call 类表示一个函数调用，包含函数标识符和参数列表
    class Call : public PrimaryExp {
    __SS__(Call)

        Token *ident;
        std::vector<Exp *> params;
        int lineno = 0;
    };
}

#endif //FASTER_MEOW_AST_H
