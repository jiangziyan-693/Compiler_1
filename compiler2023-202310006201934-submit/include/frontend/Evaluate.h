//
// Created by XuRuiyuan on 2023/7/18.
//

#ifndef FASTER_MEOW_EVALUATE_H
#define FASTER_MEOW_EVALUATE_H

#include "Symbol.h"
#include "AST.h"
#include "util/util.h"
//用于评估表达式
namespace Evaluate {

    extern IFV *evalUnaryConstExp(AST::UnaryExp *p);// 函数 evalUnaryConstExp 首先评估一元表达式中的基本表达式，然后依次应用一元操作符，返回最终的计算结果

    extern IFV *evalBinaryConstExp(AST::BinaryExp *p);//具体来说，函数 evalBinaryConstExp 首先评估表达式中的第一个操作数，然后依次应用二元操作符和后续的操作数，返回最终的计算结果。

    extern IFV *evalConstExp(AST::Exp *pExp);// 评估表达式的常数值并返回结果

    extern int evalConstIntExp(AST::Exp *exp);// 计算整数

    extern float evalConstFloatExp(AST::Exp *exp);// 计算浮点数

    extern IFV *evalPrimaryExp(AST::PrimaryExp *exp);// 计算基本表达式的值，根据表达式的具体类型（数字、子表达式或左值）调用相应的评估函数。如果传入的表达式不属于上述三种类型，则退出程序并返回一个错误代码

    extern IFV *evalNumber(AST::Number *number);//这个函数根据数字的具体类型（十六进制整数、八进制整数、十进制整数或浮点数）进行相应的转换，不同类型的数据进行计算

    extern IFV *evalLVal(AST::LVal *lVal);// 计算函数的左值

    extern IFV *binaryCalcHelper(Token *op, IFV *src1, IFV *src2);// 辅助函数 binaryCalcHelper，用于计算二元运算符（如加、减、乘、除、取模）的结果。

    extern IFV *unaryCalcHelper(Token *op, IFV *src);// 辅助函数，用于计算单元的结果
};

#endif //FASTER_MEOW_EVALUATE_H
