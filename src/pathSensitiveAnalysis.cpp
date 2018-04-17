/*
 * Filename : pathSentitiveAnalysis.cpp
 *
 * Description :
 *
 * Author : Long Gong (saber.fate.dragon@gmail.com)
 *
 * Start Date : 14 Mar 2018
 *
 */
#include "llvm/Transforms/IPO/SCCP.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/PointerIntPair.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/Analysis/ConstantFolding.h"
#include "llvm/Analysis/GlobalsModRef.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
//#include "llvm/Analysis/Utils/Local.h"
//#include "llvm/Analysis/ValueLattice.h"
//#include "llvm/Analysis/ValueLatticeUtils.h"

#include "llvm/ADT/GraphTraits.h"  // for GraphTraits
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"  // TerminatorInst
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"  // for ModulePass
#include "llvm/IR/User.h"
#include "llvm/IR/Value.h"
#include "llvm/Pass.h"
#include "llvm/Support/Format.h"  // for format()
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Analysis/ConstantFolding.h"
//#include "llvm/Analysis/TargetLibraryInfo.h"

#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/SCCP.h"


#include <set>
#include <unordered_map>
#include "PathSensitiveAnalysisConfig.h"

#include "SCCP.hpp"

using namespace llvm;

namespace {

    template <typename Type>
    std::string toString(const Type* val) {
        std::string vStr;
        raw_string_ostream os(vStr);
        val->print(os);

        return vStr;
    }

    bool isConstant(SCCPSolver& Solver, Value *V){
        Constant *Const = nullptr;
        if (V->getType()->isStructTy()) {
            std::vector<LatticeVal> IVs = Solver.getStructLatticeValueFor(V);
            if (any_of(IVs, [](const LatticeVal &LV) { return LV.isOverdefined(); }))
                return false;
            std::vector<Constant *> ConstVals;
            auto *ST = dyn_cast<StructType>(V->getType());
            for (unsigned i = 0, e = ST->getNumElements(); i != e; ++i) {
                LatticeVal V = IVs[i];
                ConstVals.push_back(V.isConstant()
                                    ? V.getConstant()
                                    : UndefValue::get(ST->getElementType(i)));
            }
            Const = ConstantStruct::get(ST, ConstVals);
        } else {
            LatticeVal IV = Solver.getLatticeValueFor(V);
            if (IV.isOverdefined())
                return false;
            Const = IV.isConstant() ? IV.getConstant() : UndefValue::get(V->getType());
        }
        return true;
    }


    struct PathSensitiveAnalysis : FunctionPass {
        static char ID;

        PathSensitiveAnalysis() : FunctionPass(ID) {}

        bool runOnFunction(Function &F) override {
            if (skipFunction(F)) return false;
            bool Change = false;
            const DataLayout &DL = F.getParent()->getDataLayout();
            TargetLibraryInfo *TLI =
                    &getAnalysis<TargetLibraryInfoWrapperPass>().getTLI();

            SCCPSolver Solver(DL, TLI);

            // Mark the first block of the function as being executable.
            Solver.MarkBlockExecutable(&F.front());

            // Mark all arguments to the function as being overdefined.
            for (Argument &AI : F.args())
                Solver.markAnythingOverdefined(&AI);

            // Solve for constants.
            bool ResolvedUndefs = true;
            while (ResolvedUndefs) {
                Solver.Solve();
                DEBUG(dbgs() << "RESOLVING UNDEFs\n");
                ResolvedUndefs = Solver.ResolvedUndefsIn(F);
            }

            for (BasicBlock &BB : F) {
                if (!Solver.isBlockExecutable(&BB)) continue;
                // Iterate over all of the instructions in a function, replacing them with
                // constants if we have found them to be of constant values.
                //
                for (BasicBlock::iterator BI = BB.begin(), E = BB.end(); BI != E;) {
                    Instruction *Inst = &*BI++;
                    if (Inst->getType()->isVoidTy() || isa<TerminatorInst>(Inst))
                        continue;
                    if (auto* PHI = dyn_cast<PHINode>(Inst)){
                        if (Solver.isDestructiveMerge(PHI)){
                            errs() << toString(PHI)
                                   << " is a destructive merge.\n"
                                   << "its region of influence is ";
                            for (auto UI = PHI->use_begin(), UE = PHI->use_end();UI != UE;){
                                Use *U = &*UI++;
                                errs() << toString(U) << "\n";
                            }
                            errs() << "--------------------------------------------\n";
                        }

                    }
                }
            }


            return Change;
        }

        void getAnalysisUsage(AnalysisUsage &AU) const override {
            AU.setPreservesCFG();
            AU.addRequired<TargetLibraryInfoWrapperPass>();
        }

        void print(raw_ostream &os, const Module *) const override {}
    };
    char PathSensitiveAnalysis::ID = 0;
    static RegisterPass<PathSensitiveAnalysis> X(PASS_NAME,
                                                     "Comprehensive Path-sensitive Data-flow Analysis",
                                                     false,
                                                     false);
}  // namespace