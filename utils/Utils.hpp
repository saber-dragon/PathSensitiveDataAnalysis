#ifndef PATHSENSITIVEANALYSIS_UTILS_HPP
#define PATHSENSITIVEANALYSIS_UTILS_HPP

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/PointerIntPair.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/GraphTraits.h"  // for GraphTraits

#include "llvm/IR/Instruction.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Instructions.h" // for PHINode
#include "llvm/Analysis/CFG.h"
#include "llvm/Support/raw_ostream.h"

#include "llvm/Transforms/Utils/ValueMapper.h"

#include <string>
//#include <bits/unordered_map.h>

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

    std::string toString(const std::pair<Instruction*, Instruction*>& Edge){
        return toString(Edge.first) + " --> " + toString(Edge.second);
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
    void replaceAllUsesOfWithIn(Value *I, Value *J, const std::set<BasicBlock *>& BBSet) {
        for (auto UI = I->user_begin(), UE = I->user_end(); UI != UE;) {
            Use &TheUse = UI.getUse();
            ++UI;
            if (auto *II = dyn_cast<Instruction>(TheUse.getUser()))
                if (BBSet.count(II->getParent()))
                    II->replaceUsesOfWith(I, J);
        }
    }
    void replaceAllUsesOfOutside(Value *I, Value *J, const std::set<BasicBlock *>& BBSet) {
        for (auto UI = I->user_begin(), UE = I->user_end(); UI != UE;) {
            Use &TheUse = UI.getUse();
            ++UI;
            if (auto *II = dyn_cast<Instruction>(TheUse.getUser()))
                if (!BBSet.count(II->getParent()))
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

    template <typename T>
    T  get(const DenseMap<T, T>& TMap, const T& Key){
        if (TMap.count(Key)) return TMap.lookup(Key);
        return Key;
    }

    Value *get(const ValueToValueMapTy& VMap, Value* OldValue){
        if (VMap.count(OldValue)) return VMap.lookup(OldValue);
        return OldValue;
    }

//    template <typename T>
//    T get(const std::unordered_map<T, T>& TMap, const T& Key){
//        if (TMap.count(Key)) return TMap.at(Key);
//        else return Key;
//    }
}
#endif //PATHSENSITIVEANALYSIS_UTILS_HPP
