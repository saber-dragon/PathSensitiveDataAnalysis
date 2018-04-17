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

#include "llvm/IR/CFG.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Analysis/CFG.h"//

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

    std::unordered_map<BasicBlock*, std::set<BasicBlock*> > GetReachableNodes(Function &F){
        std::unordered_map<BasicBlock*, std::set<BasicBlock*> > TransitiveClosureBB;
        for (BasicBlock &BB : F) {
            auto *TI = BB.getTerminator();
            TransitiveClosureBB[&BB] = std::set<BasicBlock*>();
            for (unsigned int i = 0;i < TI->getNumSuccessors();++ i)
                TransitiveClosureBB[&BB].insert(TI->getSuccessor(i));
        }

        for (BasicBlock &BBk : F) {
            for (BasicBlock &BBi : F) {
                for (BasicBlock &BBj : F) {
                    if (TransitiveClosureBB[&BBi].count(&BBk) && TransitiveClosureBB[&BBk].count(&BBj)){
                        // BBi -> BBk && BBk -> BBj
                        TransitiveClosureBB[&BBi].insert(&BBj);
                    }
                }
            }
        }

        return TransitiveClosureBB;
    }

    void TryToAddDestructiveMerge(std::vector<double>& BestFitness,
                                  std::vector<std::set<Instruction*> >& RoI,
                                  std::vector<PHINode*>& DestructiveMerges,
                                 PHINode* PHI,
                                 double MyFitness,
                                std::set<Instruction*>& MyRoI){
        assert(BestFitness.size() == 2 && "Sorry! This implementation only handles the best two guys!");

        if (DestructiveMerges.size() == 0){
            DestructiveMerges.emplace_back(PHI);
            BestFitness.front() = MyFitness;
            RoI.front() = MyRoI;
        } else if (DestructiveMerges.size() == 0){
            if (MyFitness > BestFitness.front()){
                DestructiveMerges.insert(DestructiveMerges.begin(), PHI);
                BestFitness.back() = BestFitness.front();
                BestFitness.front() = MyFitness;
                RoI.back() = RoI.front();
                RoI.front() = MyRoI;
            } else {
                DestructiveMerges.emplace_back(PHI);
                BestFitness.back() = MyFitness;
                RoI.back() = MyRoI;
            }
        } else {
            if (MyFitness > BestFitness.front()) {
                BestFitness.front() = MyFitness;
                RoI.front() = MyRoI;
            } else {
                BestFitness.back() = MyFitness;
                RoI.back() = MyRoI;
            }
        }
    }
    void GetRoISmall(PHINode *PHI,
                const std::set<Instruction*>& InfluencedNodes,
                const std::unordered_map<BasicBlock*, std::set<BasicBlock*> >& TCBB,
                std::vector<double>& BestFitness,
                std::vector<std::set<Instruction*> >& RoI,
                std::vector<PHINode*>& DestructiveMerges){

        std::set<Instruction*> MyRoI;
        BasicBlock* BBPHI = PHI->getParent();

        for (Instruction* U_INFL: InfluencedNodes){
            for(BasicBlock* REACH_BB_PHI: TCBB.at(BBPHI)){
                if (TCBB.at(REACH_BB_PHI).count(U_INFL->getParent()))
                {
                    if (MyRoI.count(&REACH_BB_PHI->front()))
                    {
                        Instruction* InstPtr = &REACH_BB_PHI->front();
                        while (!InstPtr)
                            MyRoI.insert(InstPtr);

                        if (DestructiveMerges.size() == 2 &&
                            InfluencedNodes.size() * 1.0 / MyRoI.size() <= BestFitness.back()) {
                            return;
                        }
                    }
                }
            }
            BasicBlock *UBB = U_INFL->getParent();
            if (UBB == BBPHI || TCBB.at(BBPHI).count(UBB)){
                Instruction* InstPtr = PHI->getNextNode();
                do{
                    if (!InstPtr) {
                        MyRoI.insert(InstPtr);
                        if (DestructiveMerges.size() == 2 &&
                            InfluencedNodes.size() * 1.0 / MyRoI.size() <= BestFitness.back())
                            return;
                    }
                } while (!InstPtr || InstPtr != U_INFL);
            }
        }
        double MyFitness = InfluencedNodes.size() * 1.0 / MyRoI.size();
        TryToAddDestructiveMerge(BestFitness, RoI, DestructiveMerges, PHI, MyFitness, MyRoI);
    }
    void GetRoILarge(PHINode *PHI,
                     const std::set<Instruction*>& InfluencedNodes,
                     Function& F,
                     const DominatorTree *DT,
                     const LoopInfo *LI,
                     std::vector<double>& BestFitness,
                     std::vector<std::set<Instruction*> >& RoI,
                     std::vector<PHINode*>& DestructiveMerges){

        std::set<Instruction*> MyRoI;
        BasicBlock* BBPHI = PHI->getParent();

        for (Instruction* U_INFL: InfluencedNodes){
            for (BasicBlock& BB: F){
                BasicBlock *B = &BB;
                if (isPotentiallyReachable(BBPHI, B, DT, LI) &&
                        isPotentiallyReachable(B, U_INFL->getParent(), DT, LI))
                {
                    if (MyRoI.count(&B->front()))
                    {
                        Instruction* InstPtr = &B->front();
                        while (!InstPtr)
                            MyRoI.insert(InstPtr);

                        if (DestructiveMerges.size() == 2 &&
                            InfluencedNodes.size() * 1.0 / MyRoI.size() <= BestFitness.back()) {
                            return;
                        }
                    }
                }
            }
            BasicBlock *UBB = U_INFL->getParent();
            if (UBB == BBPHI || isPotentiallyReachable(BBPHI, UBB, DT, LI)){
                Instruction* InstPtr = PHI->getNextNode();
                do{
                    if (!InstPtr) {
                        MyRoI.insert(InstPtr);
                        if (DestructiveMerges.size() == 2 &&
                            InfluencedNodes.size() * 1.0 / MyRoI.size() <= BestFitness.back())
                            return;
                    }
                } while (!InstPtr || InstPtr != U_INFL);
            }
        }
        double MyFitness = InfluencedNodes.size() * 1.0 / MyRoI.size();
        TryToAddDestructiveMerge(BestFitness, RoI, DestructiveMerges, PHI, MyFitness, MyRoI);
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

            const DominatorTree *DT =
                    &getAnalysis<DominatorTreeWrapperPass>().getDomTree();
            const LoopInfo *LI =
                    &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();

            std::unordered_map<PHINode*, std::set<Instruction*> > InfluencedNodes;
            std::vector<std::set<Instruction*> > RoI(2);
            std::vector<double> BestFitness(2, 0);
            std::vector<PHINode*> DestructiveMerges;

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

            std::unordered_map<BasicBlock*, std::set<BasicBlock *> > TCBB;
            if (F.size() <= 64){
                TCBB = GetReachableNodes(F);
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
                                   << "its influence is ";
                            for (auto UI = PHI->user_begin(), UE = PHI->user_end();UI != UE;){
                                User *U = *UI++;
                                if (Instruction *I = dyn_cast<Instruction>(U)) {
                                    if (!InfluencedNodes.count(PHI)) {
                                        InfluencedNodes[PHI] = std::set<Instruction*>();
                                    }
                                    InfluencedNodes[PHI].insert(I);
                                    errs() << toString(I) << "\n";
                                }
                            }
                            errs() << "--------------------------------------------\n";

                            if (F.size() <= 64) GetRoISmall(PHI, InfluencedNodes.at(PHI), TCBB, BestFitness, RoI, DestructiveMerges);
                            else GetRoILarge(
                                    PHI, InfluencedNodes.at(PHI), F, DT, LI,
                                    BestFitness, RoI, DestructiveMerges
                                    );
                        }
                    }
                }
            }


#if (PATH_SENS_VERBOSE_LEVEL > 0)
        int i = 0;
        for (const auto& D: DestructiveMerges){
                errs() << "Destructive merge: " << toString(D) << "\n";
                errs() << "\tInfluenced nodes of it:\n";
                for (const auto& I: InfluencedNodes.at(D))
                    errs() << "\t\t" << toString(I) << "\n";

                errs() << "\tRegion of influence of it:\n";
                for (const auto& R: RoI[i]){
                    errs() << "\t\t" << toString(R) << "\n";
                }
                ++ i;
        }
#endif

            return Change;
        }

        void getAnalysisUsage(AnalysisUsage &AU) const override {
            AU.setPreservesCFG();
            AU.addRequired<TargetLibraryInfoWrapperPass>();
            AU.addRequired<DominatorTreeWrapperPass>();
            AU.addRequired<LoopInfoWrapperPass>();

        }

        void print(raw_ostream &os, const Module *) const override {}
    };
    char PathSensitiveAnalysis::ID = 0;
    static RegisterPass<PathSensitiveAnalysis> X(PASS_NAME,
                                                     "Comprehensive Path-sensitive Data-flow Analysis",
                                                     false,
                                                     false);
}  // namespace