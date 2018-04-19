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
//#include "llvm/Transforms/IPO/SCCP.h"

// Useful LLVM data structures
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/PointerIntPair.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/GraphTraits.h"  // for GraphTraits


#include "llvm/Analysis/ConstantFolding.h"
#include "llvm/Analysis/GlobalsModRef.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/Analysis/CFG.h"//
#include "llvm/Analysis/LoopInfo.h"
//#include "llvm/Analysis/Utils/Local.h"
//#include "llvm/Analysis/ValueLattice.h"
//#include "llvm/Analysis/ValueLatticeUtils.h"


#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"  // TerminatorInst
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Module.h"  // for ModulePass
#include "llvm/IR/User.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/BasicBlock.h"

#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
//#include "llvm/Transforms/Scalar.h"
//#include "llvm/Analysis/ConstantFolding.h"
//#include "llvm/Analysis/TargetLibraryInfo.h"

//#include "llvm/Transforms/IPO.h"
//#include "llvm/Transforms/Scalar.h"
//#include "llvm/Transforms/Scalar/SCCP.h"
#include <set>
#include <unordered_map>
#include "PathSensitiveAnalysisConfig.h"

#include "SCCP.hpp"
#include "Utils.hpp"

using namespace llvm;
using namespace saber;

namespace {

    void GetReachableNodes(Function &F,
                           const DenseMap<BasicBlock*, int>& ID,
                           std::vector<std::vector<bool> >& TransitiveClosureBB) {
        for (BasicBlock &BB : F) {
            auto *TI = BB.getTerminator();
            auto i = ID.lookup(&BB);

            for (unsigned int k = 0, e = TI->getNumSuccessors();k < e;++ k){
                BasicBlock *Succ = TI->getSuccessor(k);
                auto j = ID.lookup(Succ);
                TransitiveClosureBB[i][j] = true;
            }
        }


        for (BasicBlock &BBk : F) {
            auto k = ID.lookup(&BBk);
            for (BasicBlock &BBi : F) {
                auto i = ID.lookup(&BBi);
                for (BasicBlock &BBj : F) {
                    auto j = ID.lookup(&BBj);
                    if (TransitiveClosureBB[i][k] && TransitiveClosureBB[k][j]){
                        TransitiveClosureBB[i][j] = true;
                    }
                }
            }
        }
	
#if (PATH_SENS_VERBOSE_LEVEL > 1)
//    for (BasicBlock &BB : F)
//    {
//        auto i = ID.lookup(&BB);
//        errs() << BB.getName() << " : ";
//        for (BasicBlock &BBPrime : F)
//        {
//            auto j = ID.lookup(&BBPrime);
//            if (TransitiveClosureBB[i][j])
//                errs() << BBPrime.getName() << " ";
//        }
//        errs() << "\n";
//    }
#endif
        //return TransitiveClosureBB;
    }
    
    
    void BuildBasicBlockId(Function& F, DenseMap<BasicBlock*, int>& ID){
        int id = 0;
        for (BasicBlock &BB: F){
            ID[&BB] = id ++;
        }
#if PATH_SENS_VERBOSE_LEVEL > 1
//    for (const auto& p: ID){
//            errs() << p.first->getName() << " : " << p.second << "\n";
//        }
#endif
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
            RoI.front().clear();// delete
            RoI.front().insert(MyRoI.begin(), MyRoI.end());
        } else if (DestructiveMerges.size() == 1){
            if (MyFitness > BestFitness.front()){
                DestructiveMerges.insert(DestructiveMerges.begin(), PHI);
                BestFitness.back() = BestFitness.front();
                BestFitness.front() = MyFitness;
                RoI.back().clear();
                RoI.back().insert(RoI.front().begin(), RoI.front().end());
                RoI.front().clear();
                RoI.front().insert(MyRoI.begin(), MyRoI.end());
            } else {
                DestructiveMerges.emplace_back(PHI);
                BestFitness.back() = MyFitness;
                RoI.back().clear();
                RoI.back().insert(MyRoI.begin(), MyRoI.end());
            }
        } else {
            if (MyFitness > BestFitness.front()) {
                DestructiveMerges.front() = PHI;
                BestFitness.front() = MyFitness;
                RoI.front().clear();// delete
                RoI.front().insert(MyRoI.begin(), MyRoI.end());
            } else {
                DestructiveMerges.back() = PHI;
                BestFitness.back() = MyFitness;
                RoI.back().clear();
                RoI.back().insert(MyRoI.begin(), MyRoI.end());
            }
        }
    }

//    bool CloneInstTest(Instruction* Inst){
//        if (Inst == nullptr) return false; //
//        BasicBlock *Pb = Inst->getParent();
//        auto *newInst = Inst->clone();
//        Pb->getInstList().insert(Inst, newInst);
//        return true;
//    }
//
//    bool SplitBasicBlock(BasicBlock* B, Instruction* Inst){
//        static LLVMContext TheContext;
//        BasicBlock* NewBlock = BasicBlock::Create(TheContext, "Test", B->getParent(), B);
//        Instruction *InstPtr = &(B->front());
//        // copy inst to new basic block
//        while (InstPtr != Inst){
//            NewBlock->getInstList().push_back(InstPtr);
//            InstPtr = InstPtr->getNextNode();
//        }
//        InstPtr = &(B->front());
//        Instruction *InstPtrNext;
//        // remove inst from old basic block
//        while (InstPtr != Inst){
//            InstPtrNext = InstPtr->getNextNode();
//            InstPtr->eraseFromParent();
//            InstPtr = InstPtrNext;
//        }
//    }


	unsigned int predCount(Instruction* inst) {
		Instruction *prev = inst->getPrevNode();
		if (prev != 0) return 1;

		BasicBlock* bb = inst->getParent();
		unsigned int count = 0;
		for (BasicBlock *pred : predecessors(bb))
			++count;
		return count;
	}
	
	Instruction* pred(Instruction* inst, unsigned int i) {
		Instruction *prev = inst->getPrevNode();
		if (prev != 0) return prev;
			
		BasicBlock* bb = inst->getParent();
		unsigned int j = 0;
		for (BasicBlock *pred : predecessors(bb))
			if (j++ == i)
				return &(pred->back());
		
		return 0;
	}
	
	unsigned int succCount(Instruction* inst) {
		Instruction *next = inst->getNextNode();
		if (next != 0) return 1;

		BasicBlock* bb = inst->getParent();
		unsigned int count = 0;
		for (succ_iterator sit = succ_begin(bb), set = succ_end(bb); sit != set; ++sit)
			++count;
		return count;
	}
	
	Instruction* succ(Instruction* inst, unsigned int i) {
		Instruction *next = inst->getNextNode();
		if (next != 0) return next;
			
		BasicBlock* bb = inst->getParent();
		unsigned int j = 0;
		for (BasicBlock *succ : successors(bb))
			if (j++ == i)
				return &(succ->front());
		
		return 0;
	}

	void GetVariableReachability(std::set<Instruction*>& RoI
				//std::vector<BasicBlockEdge>& kill, 
				//Function& F,
				//std::map<BasicBlockEdge, std::set<Value*>>& reachableValues
				)
	{
		// Gen[inst], for inst in RoI - Definition created, StoreInst
		// Kill[inst] - SSA, no kill?
		// In[inst] - Gen[i] u (In[i] - Kill[i])
		// Out[inst] - U Out[i'], for all i' in pred(i)
		
		std::map<Instruction*, std::set<Value*>> genI;
        std::map<Instruction*, std::set<Value*>> inI;
        std::map<Instruction*, std::set<Value*>> outI;
		
		
		// Create gen set, per instruction
		
		for (std::set<Instruction*>::iterator it = RoI.begin(); it != RoI.end(); ++it)
		{
			Instruction *inst = *it;
			
			//**TODO: Definitions are not only generated by StoreInst. How do we create gen set?
			
			if (isa<StoreInst>(inst))
			{
				if (inst->getNumOperands() > 0)
				{
					Use* varUse = &(inst->getOperandUse(1));
					Value* varValue = varUse->get();
					
					genI[inst].insert( varValue );
				}
			}
		}
		
		
		// Do dataflow
		
		bool setChanged = true;
		int prevInfLoop = 0;
		while (setChanged && prevInfLoop++ < 10000)
		{
			setChanged = false;
			
			for (std::set<Instruction*>::iterator it = RoI.begin(); it != RoI.end(); ++it)
			{
				Instruction *inst = *it;
			
				std::set<Value*> &gen = genI[inst];
				std::set<Value*> &in = inI[inst];
				std::set<Value*> &out = outI[inst];
				
				std::set<Value*> newOut;
				std::set_union(in.begin(), in.end(), gen.begin(), gen.end(), std::inserter(newOut,newOut.begin()));
				
				std::set<Value*> newIn;
				unsigned int pc = predCount(inst);
				if (pc > 0)
				{
					Instruction* predInst = pred(inst, 0);
					
					std::set<Value*> &pred_out = outI[predInst];
					
					std::set_union(newIn.begin(), newIn.end(), pred_out.begin(), pred_out.end(), std::inserter(newIn,newIn.begin()));
					
					for (unsigned int i = 1; i < pc; ++i)
					{
						predInst = pred(inst, i);
						pred_out = outI[predInst];
						std::set_union(newIn.begin(), newIn.end(), pred_out.begin(), pred_out.end(), std::inserter(newIn,newIn.begin()));
					}
				}
				
				if (in != newIn || out != newOut)
					setChanged = true;
				
				in.swap(newIn);
				out.swap(newOut);
			}
		}
		
		
		// Test
		
		for (std::set<Instruction*>::iterator it = RoI.begin(); it != RoI.end(); ++it)
		{
			Instruction *inst = *it;
			
			errs() << "Instruction " << inst->getName() << ":\n";
			
			std::set<Value*> &gen = genI[inst];
			std::set<Value*> &in = inI[inst];
			std::set<Value*> &out = outI[inst];
			
			errs() << "Gen" << "\n";
			for (std::set<Value*>::iterator it2 = gen.begin(); it2 != gen.end(); ++it2)
			{
				errs() << "- " << (*it2)->getName() << "\n";
			}
			
			errs() << "In" << "\n";
			for (std::set<Value*>::iterator it2 = in.begin(); it2 != in.end(); ++it2)
			{
				errs() << "- " << (*it2)->getName() << "\n";
			}
			
			errs() << "Out" << "\n";
			for (std::set<Value*>::iterator it2 = out.begin(); it2 != out.end(); ++it2)
			{
				errs() << "- " << (*it2)->getName() << "\n";
			}
		}
	}

    void GetRoISmall(PHINode *PHI,
                 const std::set<Instruction*>& InfluencedNodes,
                 Function& F,
                 const DenseMap<BasicBlock*, int>& ID,
                 const std::vector<std::vector<bool> >& Reachable,
                std::vector<double>& BestFitness,
                std::vector<std::set<Instruction*> >& RoI,
                std::vector<PHINode*>& DestructiveMerges){

        std::set<Instruction*> MyRoI;
        BasicBlock* MBB = PHI->getParent();
        auto m = ID.lookup(MBB); //

        // for each u in influenced_nodes(m)
        for (Instruction* UNode : InfluencedNodes){
            BasicBlock* UBB = UNode->getParent();
            // get the id of the Basic Block
            auto u = ID.lookup(UBB);
            // for each n in reachable_nodes(m)
            for(BasicBlock& NBB : F){
                auto n = ID.lookup(&NBB);
                if (!Reachable[m][n] && m != n) continue;
                if (Reachable[n][u] || n == u)
                {
                    Instruction* InstPtr = &(NBB.front());
                    if (m == n) InstPtr = PHI->getNextNode();
                    if (MyRoI.count(InstPtr) == 0 || MyRoI.count(&(NBB.back())) == 0)
                    {
                        while (InstPtr != nullptr ){
                            MyRoI.insert(InstPtr);
                            if (InstPtr == UNode) break;// no need to go further
#if PATH_SENS_VERBOSE_LEVEL == 0
                            if (DestructiveMerges.size() == 2 &&
                                InfluencedNodes.size() * 1.0 / MyRoI.size() <= BestFitness.back())
                            {
                                errs() << "------------------------------------------------------------------------------\n";
                                errs() << toString(PHI) << " : \n";
                                errs() << "\tInfluenced Nodes : \n";
                                for (const auto& Inf: InfluencedNodes)
                                    errs() << "\t\t" << toString(Inf) << "\n";
                                errs() << "\tRoI : \n";
                                for (const auto& Inst: MyRoI)
                                    errs() << "\t\t" << toString(Inst) << "\n";
                                errs() << "\n\n";
                                return;
                            }
#endif
                            InstPtr = InstPtr->getNextNode();
                        }
                    }
                }
            }

        }

        errs() << "------------------------------------------------------------------------------\n";
        errs() << toString(PHI) << " : \n";
        errs() << "\tInfluenced Nodes : \n";
        for (const auto& Inf: InfluencedNodes)
            errs() << "\t\t" << toString(Inf) << "\n";
        errs() << "\tRoI : \n";
        for (const auto& Inst: MyRoI)
            errs() << "\t\t" << toString(Inst) << "\n";
        errs() << "\n\n";

#if PATH_SENS_VERBOSE_LEVEL > 0
        if (DestructiveMerges.size() == 2 &&
            InfluencedNodes.size() * 1.0 / MyRoI.size() <= BestFitness.back())
        {
            return;
        }
#endif
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
        BasicBlock* MBB = PHI->getParent();

        for (Instruction* UNode: InfluencedNodes){
            BasicBlock *UBB = UNode->getParent();
            for (BasicBlock& NB: F){
                BasicBlock *NBB = &NB;
                if (!isPotentiallyReachable(MBB, NBB, DT, LI) && MBB != NBB) continue;
                if (isPotentiallyReachable(NBB, UBB, DT, LI) || NBB == UBB)
                {
                    if (MyRoI.count(&(NBB->front())) == 0 && MyRoI.count(&(NBB->back())) == 0) {
                        Instruction* InstPtr = &NBB->front();
                        if (MBB == NBB) InstPtr = PHI->getNextNode();
                        while (InstPtr != nullptr) {
                            MyRoI.insert(InstPtr);
                            // The following line might be optimized
                            if (DestructiveMerges.size() == 2 &&
                                InfluencedNodes.size() * 1.0 / MyRoI.size() <= BestFitness.back()) {
                                return;
                            }
                            if (InstPtr == UNode) break;
                            InstPtr = InstPtr->getNextNode();
                        }
                    }
                }
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

            //DenseMap<PHINode*, int> DestructiveMergeID;
            std::unordered_map<PHINode*, std::set<Instruction*> > InfluencedNodes;
            //std::vector<std::set<Instruction*> > InfluencedNodes;
            std::vector<std::set<Instruction*> > RoI(2);
            std::vector<double> BestFitness(2, 0);
            std::vector<PHINode*> DestructiveMerges;
            DenseMap<BasicBlock*, int> ID;
            BuildBasicBlockId(F, ID);

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

            std::vector<std::vector<bool> > Reachable;

            if (F.size() <= 64){
                Reachable.resize(F.size());
                // allocate space
                for(auto& v: Reachable){v.resize(F.size());}
                // initialize
                std::for_each(Reachable.begin(), Reachable.end(),
                    [](std::vector<bool>& v){
                    std::fill(v.begin(), v.end(), false);
                });
                GetReachableNodes(F, ID, Reachable);
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
                            for (auto UI = PHI->user_begin(), UE = PHI->user_end();UI != UE;){
                                User *U = *UI++;
                                if (Instruction *I = dyn_cast<Instruction>(U)) {
                                    if (InfluencedNodes.count(PHI) == 0) {
                                        InfluencedNodes.insert({PHI, std::set<Instruction*>()});
                                    }
                                    InfluencedNodes[PHI].insert(I);
                                }
                            }

                            if (F.size() <= 64) GetRoISmall(PHI, InfluencedNodes.at(PHI),
                                                            F, ID, Reachable, BestFitness,
                                                            RoI, DestructiveMerges);
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
            errs() << "\n==============================================================================\n";
        for (const auto& D: DestructiveMerges){
                errs() << "Destructive Merge : " << toString(D) << "\n";
                errs() << "\tInfluenced Nodes :\n";
                for (const auto& I: InfluencedNodes.at(D))
                    errs() << "\t\t" << toString(I) << "\n";

                errs() << "\tRegion of Influence :\n";
                for (const auto& R: RoI[i]){
                    errs() << "\t\t" << toString(R) << "\n";
                }
                ++ i;
        }
#endif


			GetVariableReachability(
				RoI[0]
				//std::vector<BasicBlockEdge>& kill, 
				//Function& F,
				//std::map<BasicBlockEdge, std::set<Value*>>& reachableValues
			);
				

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