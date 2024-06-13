//
// Created by Administrator on 2023/7/15.
//

#include "../../include/util/Manager.h"
#include "../../include/mir/Function.h"
#include "../../include/mir/GlobalVal.h"
#include "../../include/mir/Type.h"
#include "../../include/frontend/Initial.h"

#include <fstream>

Manager::Manager() {
    external = true;
    addFunction(ExternFunction::GET_INT);
    addFunction(ExternFunction::GET_CH);
    addFunction(ExternFunction::GET_FLOAT);
    addFunction(ExternFunction::GET_ARR);
    addFunction(ExternFunction::GET_FARR);
    addFunction(ExternFunction::PUT_INT);
    addFunction(ExternFunction::PUT_CH);
    addFunction(ExternFunction::PUT_FLOAT);
    addFunction(ExternFunction::PUT_ARR);
    addFunction(ExternFunction::PUT_FARR);
    addFunction(ExternFunction::MEM_SET);
    // addFunction(ExternFunction::MEM_SET_ZERO);
    addFunction(ExternFunction::START_TIME);
    ExternFunction::START_TIME->isTimeFunc = true;
    addFunction(ExternFunction::STOP_TIME);
    ExternFunction::STOP_TIME->isTimeFunc = true;
    addFunction(ExternFunction::PARALLEL_START);
    addFunction(ExternFunction::PARALLEL_END);
    addFunction(ExternFunction::LLMMOD);
    addFunction(ExternFunction::LLMD2MOD);
    addFunction(ExternFunction::LLMAMOD);
    addFunction(ExternFunction::USAT);
    external = false;
}

std::vector<Function *> *Manager::getFunctionList() {
    std::vector<Function *> *list = new std::vector<Function *>;
    for (const auto &it: *functions) {
        Function *function = it.second;
        if (function->is_deleted) continue;
        if (function->hasBody()) {
            list->push_back(function);
        }
    }
    return list;
}

void Manager::addGlobal(Symbol *symbol) {
    (*globals)[(GlobalValue *) symbol->value] = symbol->initial;
}

void Manager::addFunction(Function *function) {
    (*functions)[function->name] = function;
    externalFuncList.push_back(function);
}

bool Manager::hasMainFunction() {
    return functions->find(MAIN_FUNCTION) != functions->end();
}

// std::unordered_map<GlobalValue *, Initial *> Manager::getGlobals() {
//     return this->globals;
// }

// std::unordered_map<std::string, Function *> *Manager::getFunctions() {
//     std::unordered_map<std::string, Function *> *ret = new std::unordered_map<std::string, Function *>();
//     for (const auto &it: functions) {
//         std::string str = it.first;
//         if (!it.second->is_deleted) {
//             (*ret)[str] = it.second;
//         }
//     }
//     return ret;
// }

std::ostream &Manager::outputLLVM(std::ostream &stream) {
    for (const auto &it: *globals) {
        stream << it.first->getName() + " = dso_local global " + std::string(*it.second) << "\n";
    }
    for (const auto &it: *functions) {
        Function *f = it.second;
        if (f->is_deleted) continue;
        // if (f == ExternFunction::MEM_SET)continue;
        if (!f->hasBody()) {
            stream << f->getDeclare() << "\n";
        }
    }
    // stream << "declare void @memset(i32*, i32, i32)\n";
    // stream << "\ndefine dso_local void @_our_memset(i32 * %f0, i32 %f1) {\n"
    //        << "\tcall void memset(i32 *%f0, i32 0,i32 %f1)\n"
    //        << "}\n\n";
    for (const auto &it: *functions) {
        Function *f = it.second;
        if (f->is_deleted) continue;
        if (f->getDeleted()) {
            continue;
        }
        if (f->hasBody()) {
            stream << f << "\n";
        }
    }
    return stream;
}

void Manager::outputLLVM(std::string filename) {
    std::ofstream fout(filename);
    this->outputLLVM(fout);
    fout.close();
}