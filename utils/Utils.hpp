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
#include <vector>
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

    template <typename T>
    bool HasIntersection(const std::set<T>& A, const std::set<T>& B){
        if (A.empty() || B.empty()) return false;
        for (const auto& e : A){
            if (B.count(e)) return true;
        }
        return false;
    }

    template <typename T>
    std::set<T> SetUnion(const std::set<T>& A, const std::set<T>& B){
        std::set<T> C;
        C.insert(A.begin(), A.end());
        C.insert(B.begin(), B.end());

        return C;
    }

    std::vector<Instruction*> InstSuccessors(Instruction* Inst){
        std::vector<Instruction*> succ;
        if (Instruction * S = Inst->getNextNode()){
            succ.push_back(S);
        } else {
            TerminatorInst *TI = dyn_cast<TerminatorInst>(Inst);
            for (unsigned k = 0;k < TI->getNumSuccessors();++ k){
                succ.push_back(&(TI->getSuccessor(k)->front()));
            }
        }
        return succ;
    }

    std::vector<Instruction*> InstPredecessors(Instruction* Inst){
        std::vector<Instruction*> pred;
        if (Instruction * S = Inst->getPrevNode()){
            pred.push_back(S);
        } else {
            BasicBlock *BB = Inst->getParent();
            for (pred_iterator BI = pred_begin(BB), BE = pred_end(BB); BI != BE; ++ BI){
                BasicBlock *P = *BI;
                pred.push_back(&(P->back()));
            }
        }
        return pred;
    }
    void ReachbyMe(BasicBlock* BB, DenseSet<BasicBlock*>& Visited){
        if (Visited.count(BB)) return;
        Visited.insert(BB);

        auto* TI = BB->getTerminator();
        for (unsigned k = 0;k < TI->getNumSuccessors();++ k){
            auto* Next = TI->getSuccessor(k);
            if (!Visited.count(Next)) ReachbyMe(Next, Visited);
        }
    }

    void ReachToMe(BasicBlock* BB, DenseSet<BasicBlock*>& Visited){
        if (Visited.count(BB)) return ;

        Visited.insert(BB);

        for (pred_iterator PI = pred_begin(BB), PE = pred_end(BB);PI != PE;++ PI){
            BasicBlock *Pre = *PI;
            if (!Visited.count(Pre)) ReachToMe(Pre, Visited);
        }

    }
//    template <typename T>
//    T get(const std::unordered_map<T, T>& TMap, const T& Key){
//        if (TMap.count(Key)) return TMap.at(Key);
//        else return Key;
//    }
}
#endif //PATHSENSITIVEANALYSIS_UTILS_HPP
