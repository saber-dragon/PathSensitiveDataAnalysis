#ifndef PATHSENSITIVEANALYSIS_UTILS_HPP
#define PATHSENSITIVEANALYSIS_UTILS_HPP

#include "llvm/IR/Instruction.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h" // for PHINode
#include "llvm/Analysis/CFG.h"
#include "llvm/Support/raw_ostream.h"

#include <string>
using namespace llvm;

namespace saber{
    template <typename Type>
    std::string toString(const Type* val) {
        std::string vStr;
        raw_string_ostream os(vStr);
        val->print(os);
        return vStr;
    }

    template <>
    std::string toString(const Instruction* val) {
        std::string vStr;
        raw_string_ostream os(vStr);
        val->print(os);
        return (vStr + " (BasicBlock: " + val->getParent()->getName().str() + ")");
    }

    template <>
    std::string toString(const PHINode* val) {
        return toString(dyn_cast<Instruction>(val));
    }
    void replaceAllUsesOfWithIn(Value *I, Value *J, BasicBlock *BB) {
        for (auto UI = I->user_begin(), UE = I->user_end(); UI != UE;) {
            Use &TheUse = UI.getUse();
            ++UI;
            if (auto *II = dyn_cast<Instruction>(TheUse.getUser()))
                if (BB == II->getParent())
                    II->replaceUsesOfWith(I, J);
        }
    }

    void PrintBB(BasicBlock *BB) {
        errs() << BB->getName() << " :\n";
        for (BasicBlock::iterator BI = BB->begin(), E = BB->end(); BI != E;) {
            Instruction *Inst = &*BI++;
            errs() << "\n" << toString(Inst) << "\n";
        }
    }
}
#endif //PATHSENSITIVEANALYSIS_UTILS_HPP
