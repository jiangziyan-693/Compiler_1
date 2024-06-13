//
// Created by Administrator on 2023/7/15.
//

#ifndef FASTER_MEOW_MANAGER_H
#define FASTER_MEOW_MANAGER_H


#include <string>
#include <map>
#include <unordered_map>
#include "vector"
#include "memory"
#include "frontend/Symbol.h"

class Function;

class GlobalValue;

class Initial;

class Manager {
public:

    static Manager *MANAGER;
    static std::string MAIN_FUNCTION;
    static bool external;
    std::unordered_map<GlobalValue *, Initial *> *globals = new std::unordered_map<GlobalValue *, Initial *>();
    static std::map<std::string, Function *> *functions;
    std::vector<Function *> externalFuncList;
    static int outputLLVMCnt;
    // public void outputLLVM() throws FileNotFoundException {
    //             outputLLVM("llvmOf-" + outputLLVMCnt++);
    //     }
    // public void outputLLVM(String llvmFilename) throws FileNotFoundException {
    //             outputLLVM(new FileOutputStream(llvmFilename + ".ll"));
    //     }
    static int outputMIcnt;

    // public void outputMI(String miFilename) throws FileNotFoundException {
    //             outputMI(new FileOutputStream(miFilename + ".txt"));
    //     }

    Manager();

    std::vector<Function *>* getFunctionList();

    void addGlobal(Symbol *symbol);

    void addFunction(Function *function);

    bool hasMainFunction();

    std::ostream &outputLLVM(std::ostream &stream);
    void outputLLVM(std::string filename);
    // void outputMI();
    // void outputMI(bool flag);
    // void outputMI(OutputStream out);
    // std::unordered_map<GlobalValue *, Initial *> getGlobals();

    // std::unordered_map<std::string, Function *> *getFunctions();

};


namespace ExternFunction {
    extern Function *GET_INT;
    extern Function *GET_CH;
    extern Function *GET_FLOAT;
    extern Function *GET_ARR;
    extern Function *GET_FARR;
    extern Function *PUT_INT;
    extern Function *PUT_CH;
    extern Function *PUT_FLOAT;
    extern Function *PUT_ARR;
    extern Function *PUT_FARR;
    extern Function *MEM_SET;
    // extern Function *MEM_SET_ZERO;
    extern Function *START_TIME;
    extern Function *STOP_TIME;
    extern Function *PARALLEL_START;
    extern Function *PARALLEL_END;
    extern Function *LLMMOD;
    extern Function *LLMD2MOD;
    extern Function *LLMAMOD;
    extern Function *USAT;
};

#endif //FASTER_MEOW_MANAGER_H
