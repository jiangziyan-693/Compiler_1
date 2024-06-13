#include <iostream>
#include "../include/util/IList.h"
#include "frontend/Lexer.h"
#include "frontend/Parser.h"
#include "../include/frontend/Visitor.h"
#include <fstream>
#include "sstream"
#include "iostream"
#include "Init.h"
#include "FrontendInit.h"
#include "MidEndRunner.h"
#include "MergeBB.h"
#include "RemovePhi.h"
#include "CodeGen.h"
#include "RegAllocator.h"
#include "MergeBlock.h"
#include <chrono>
#include <string>
#include <cstring>
#include <cstdlib>
#include "BKConstFold.h"
#include "BlockRelationDealer.h"
#include "MergeMI.h"

/* Argument Parse begin */

std::string input_file, ir_file, target_file;
int optimize_level;

//用于解析命令行参数，并将相应的参数值赋给全局变量 input_file、ir_file、target_file 和 optimize_level
void parse_args(int argc, char *argv[]) {
    // 1. ./compiler -S -o <target_file> <input_file>
    // 2. ./compiler -S -o <target_file> <input_file> -O1
    // 3. ./compiler -emit-ir <ir_file> <input_file> [-O1]
    // 4. ./compiler -emit-ir <ir_file> -S -o <target_file> <input_file> [-O1]

    for (int i = 1; i < argc; i++) {
        if (i + 2 < argc && strcmp(argv[i], "-S") == 0 && strcmp(argv[i + 1], "-o") == 0) {
            target_file.assign(argv[i + 2]);
            i += 2;
            continue;
        }
        if (i + 1 < argc && strcmp(argv[i], "-emit-ir") == 0) {
            ir_file.assign(argv[i + 1]);
            i += 1;
            continue;
        }
        if (strncmp(argv[i], "-O", 2) == 0) {
            optimize_level = atoi(argv[i] + 2);
            i += 1;
            continue;
        }
        input_file.assign(argv[i]);
    }
    if (input_file.empty()) {
        std::cerr << "error: need input file." << std::endl;
        exit(1);
    }
    if (target_file.empty() && ir_file.empty()) {
        std::cerr << "error: need either ir or backend target file." << std::endl;
        exit(1);
    }
}

/* Argument Parse end */

//backend函数是编译器后端的主要入口，它开始记录代码生成的开始时间，然后调用CodeGen::CODEGEN->gen()来生成目标代码，之后，它会记录代码生成所用的时间并输出
//接下来，会根据优化级别（optimize_level)执行一系列优化，包括常量折叠，无用指令删除，寄存器分配，指令合并等。
void backend() {
    // CenterControl::_FAST_REG_ALLOCATE = true;
    std::cerr << "code gen begin" << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    //Manager.MANAGER.outputLLVM();
    CodeGen::CODEGEN->gen();
    auto end = std::chrono::high_resolution_clock::now();
    std::cerr << "code gen end, Use Time: " << std::chrono::duration<double>(end - start).count() << "s" << std::endl;

    McProgram::MC_PROGRAM->output("arm-before-const_fold.s");
    McProgram *p = McProgram::MC_PROGRAM;

    if (CenterControl::_O2) {
        if (CenterControl::_GEP_CONST_BROADCAST) {
            BKConstFold bkConstFold;
            bkConstFold.run(p);
        }
        FOR_ITER_MF(mf, p) {
            FOR_ITER_MB(mb, mf) {
                for (MachineInst *mi = dynamic_cast<MachineInst *>(mb->beginMI->next);
                     mi->next != nullptr;) {
                    MachineInst *nextMI = dynamic_cast<MachineInst *>(mi->next);
#ifndef _BACKEND_COMMENT_OUTPUT
                    if (mi->isComment()) mi->remove();
#endif
                    mi = nextMI;
                }
            }
        }
        McProgram::MC_PROGRAM->output("arm-before-mergeMI-invr.s");
        MergeMI::mergeAll(p);
        if (CenterControl::ACROSS_BLOCK_MERGE_MI) {
            FOR_ITER_MF(mf, p){
                mf->remove_phi();
            }
        }
    } else {

        FOR_ITER_MF(mf, p) {
            FOR_ITER_MB(mb, mf) {
                for (MachineInst *mi = dynamic_cast<MachineInst *>(mb->beginMI->next);
                     mi->next != nullptr;) {
                    MachineInst *nextMI = dynamic_cast<MachineInst *>(mi->next);
#ifndef _BACKEND_COMMENT_OUTPUT
                    if (mi->isComment()) mi->remove();
#endif
                    mi = nextMI;
                }
            }
        }
    }
    // std::cerr << p << std::endl;
    McProgram::MC_PROGRAM->output("arm-before-alloc.s");
    std::cerr << "Reg Alloc begin" << std::endl;
    start = std::chrono::high_resolution_clock::now();
    if (CenterControl::_FAST_REG_ALLOCATE && !CenterControl::_O2) {
        NaiveRegAllocator regAllocator;
        regAllocator.AllocateRegister(p);
    } else {
        if (CodeGen::needFPU) {
            FPRegAllocator fpRegAllocator;
            fpRegAllocator.AllocateRegister(p);
        }
        std::cerr << "FPR reg alloc end" << std::endl;

        GPRegAllocator gpRegAllocator;
        gpRegAllocator.AllocateRegister(p);
    }
    end = std::chrono::high_resolution_clock::now();
    std::cerr << "Reg Alloc end, Use Time: " << std::chrono::duration<double>(end - start).count() << "s" << std::endl;

    if (CenterControl::_GLOBAL_BSS) {
        p->bssInit();
    }
    // if (CenterControl::_GLOBAL_BSS)
    //     McProgram::MC_PROGRAM->bssInit();

    // bool _O2 = true;
    if (CenterControl::_O2) {
        CenterControl::_TAG = "O2_2";
        if (CenterControl::_FUNC_PARALLEL) {
            CenterControl::_TAG = "O2_func";
        }
        std::cerr << "PeepHole begin" << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        PeepHole peepHole(p);
        peepHole.run();
        auto end = std::chrono::high_resolution_clock::now();
        std::cerr << "PeepHole end, Use Time: " << std::chrono::duration<double>(end - start).count() << "s"
                  << std::endl;
        McProgram::MC_PROGRAM->output("arm-before-merge.s");
        std::cerr << "MergeBlock (O2) begin" << std::endl;
        start = std::chrono::high_resolution_clock::now();
        MergeBlock *mergeBlock = new MergeBlock(p);
        int i = 0;
        while (i++ < 0) {
            mergeBlock->run(true);
        }
        end = std::chrono::high_resolution_clock::now();
        std::cerr << "MergeBlock (O2) end, Use Time: " << std::chrono::duration<double>(end - start).count() << "s"
                  << std::endl;

        if (CenterControl::_OPEN_PARALLEL) {
            Parallel::PARALLEL->gen();
        }
    } else {
        // std::cerr << "PeepHole begin" << std::endl;
        // auto start = std::chrono::high_resolution_clock::now();
        // PeepHole peepHole(p);
        // peepHole.run();
        // auto end = std::chrono::high_resolution_clock::now();
        // std::cerr << "PeepHole end, Use Time: " << std::chrono::duration<double>(end - start).count() << "s"
        //           << std::endl;

        // McProgram::MC_PROGRAM->output("arm-before-mergemb.s");
        // std::cerr << "MergeBlock begin" << std::endl;
        // start = std::chrono::high_resolution_clock::now();
        // MergeBlock *mergeBlock = new MergeBlock(p);
        // int i = 0;
        // while (i++ < 2) {
        //     mergeBlock->run(true);
        // }
        // end = std::chrono::high_resolution_clock::now();
        // std::cerr << "MergeBlock end, Use Time: " << std::chrono::duration<double>(end - start).count() << "s"
        //           << std::endl;
    }
}

//程序的入口，它首先调用 parse_args 解析命令行参数，然后设置编译器的优化级别。
//它调用 FileDealer::inputDealer 处理输入文件，并执行词法分析（lexer.lex()）和语法分析（parser.parseAst()）。
//解析完语法树后，它会调用 Visitor 来访问 AST 并生成中间表示。
int main(int argc, char *argv[]) {
    // std::cerr << CodeGen::floatCanCode(0.0f) << std::endl;
    // return 0;

    //解析命令行参数
    parse_args(argc, argv);
    if (optimize_level > 0) {
        CenterControl::_O2 = true;
    }

    //处理输入文件
    FileDealer::inputDealer(input_file.c_str());

    //创建词法分析器并进行词法分析
    Lexer lexer = Lexer();
    lexer.lex();

    //创建语法分析器并进行语法分析
    Parser parser = Parser(lexer.tokenList);
    //记录开始时间
    std::cerr << "Parser & Visitor begin" << std::endl;
    auto start = std::chrono::high_resolution_clock::now();

    //生成抽象语法树
    AST::Ast *ast = parser.parseAst();
    //访问抽象语法树并生成中间表示
    Visitor visitor = Visitor();
    visitor.visitAst(ast);
    //记录并输出解析和访问抽象语法树所用时间
    auto end = std::chrono::high_resolution_clock::now();
    std::cerr << "Parser & Visitor end, Use Time: " << std::chrono::duration<double>(end - start).count() << "s"<< std::endl;

    //输出LLVM中间表示（IR)
    Manager::MANAGER->outputLLVM("llvm-0.ll");

    //开始中间代码运行和优化
    std::cerr << "MidEndRunner begin" << std::endl;
    start = std::chrono::high_resolution_clock::now();
    MidEndRunner *midEndRunner = new MidEndRunner((Manager::MANAGER->getFunctionList()));
    midEndRunner->Run(optimize_level);
    end = std::chrono::high_resolution_clock::now();
    //记录并输出中间代码运行和优化时间
    std::cerr << "MidEndRunner end, Use Time: " << std::chrono::duration<double>(end - start).count() << "s"
              << std::endl;

    //再次输出LLVM中间表示（优化后）
    Manager::MANAGER->outputLLVM(ir_file.empty() ? "llvm-1.ll" : ir_file);

    //如果没有指定目标文件，则只输出前端结果
    if (target_file.empty()) {
        std::cerr << "Frontend Only" << std::endl;
        return 0;
    }
    //如果有优化选项，则执行额外优化步骤
    if (CenterControl::_O2) {
        RemovePhi *removePhi = new RemovePhi(midEndRunner->functions);
        removePhi->Run();

        Manager::MANAGER->outputLLVM("llvm-2.ll");

        MergeBB *mergeBB = new MergeBB(midEndRunner->functions);
        mergeBB->Run();

        midEndRunner->re_calc();
        BBSort::InitializeInfo();
        Manager::MANAGER->outputLLVM("llvm-3.ll");
    }

    //执行后端处理，生成目标代码
    backend();
    //输出最终的目标文件
    McProgram::MC_PROGRAM->output(target_file);

    //程序结束
    return 0;
}
