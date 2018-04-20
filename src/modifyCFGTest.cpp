#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/PointerIntPair.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/Statistic.h"
#include "llvm/ADT/GraphTraits.h"  // for GraphTraits

//#include "llvm/Analysis/ConstantFolding.h"
//#include "llvm/Analysis/GlobalsModRef.h"
//#include "llvm/Analysis/TargetLibraryInfo.h"
//#include "llvm/Analysis/Utils/Local.h"
//#include "llvm/Analysis/ValueLattice.h"
//#include "llvm/Analysis/ValueLatticeUtils.h"

#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h"  // TerminatorInst
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"// PHINode
#include "llvm/IR/Module.h"  // for ModulePass
#include "llvm/IR/User.h"
#include "llvm/IR/Value.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/CFG.h"
#include "llvm/IR/BasicBlock.h"

#include "llvm/Pass.h"
//#include "llvm/Support/Format.h"  // for format()
#include "llvm/Support/raw_ostream.h"
//#include "llvm/Transforms/Scalar.h"
//#include "llvm/Analysis/ConstantFolding.h"
//#include "llvm/Analysis/TargetLibraryInfo.h"

//#include "llvm/Transforms/IPO.h"
//#include "llvm/Transforms/Scalar.h"
//#include "llvm/Transforms/Scalar/SCCP.h"


#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/CFG.h"//

#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include "llvm/Transforms/Utils/Cloning.h"


#include <queue>

#include "PathSensitiveAnalysisConfig.h"
#include "Utils.hpp"

using namespace llvm;
using namespace saber;

namespace {
    using BasicBlockToBasicBlockMapTy = DenseMap<BasicBlock *, BasicBlock *>;
    using Edge = std::pair<Instruction *, Instruction *>;
    using EqClass = int16_t;

    class BasicBlockHepler {
        static const EqClass CZero = -1;
    public:
        static void HandleRegion(std::set<BasicBlock *> &Region,
                                 const std::set<Edge> &InEdges,
                                 const std::set<Edge> &OutEdges,
                                 const DenseMap<Edge, EqClass> &RMap,
                                 const unsigned NumOfClasses,
                                 const DenseMap<Edge, std::set<Value *> > &RDMap) {
            EqClass s = CZero;
            std::vector<ValueToValueMapTy> VMapVec(NumOfClasses);
            std::vector<BasicBlockToBasicBlockMapTy> BBMapVec(NumOfClasses);
            std::vector<Instruction *> DeadInst;
            DenseSet<std::pair<BasicBlock *, EqClass>> Copied;
            DenseSet<std::pair<Edge, EqClass>> Visited;

            std::queue<Edge> Queue;

            // Push in queue all the in edges
            for (auto &InEdge : InEdges) {
                Queue.push(InEdge);
            }
            // traverse each edge
            while (!Queue.empty()) {
                auto CurEdge = Queue.front();
                Queue.pop();
                if (OutEdges.count(CurEdge)) {// kill edge
                    s = CZero;
                    if (!Visited.count({CurEdge, s})) {
                        HandleKillEdge(CurEdge, RDMap.lookup(CurEdge));
                    }
                    else {
                        Visited.insert({CurEdge, s});
                    }
                    continue;
                }

                if (RMap.count(CurEdge)) {// revival edge
                    s = RMap.lookup(CurEdge);
                    assert(s + 1 < NumOfClasses);
                }
                BasicBlock *Predecessor = CurEdge.first->getParent();
                BasicBlock *TargetBlock = CurEdge.second->getParent();
                assert(Region.count(TargetBlock));
                std::string NameSuffix("p" + std::to_string(s + 1));
                if (Copied.count({TargetBlock, s})) {
                    HandleCopiedBasicBlock(TargetBlock,
                                           NameSuffix,
                                           Predecessor,
                                           VMapVec[s + 1],
                                           BBMapVec[s + 1],
                                           DeadInst);
                }
                else {
                    HandleBasicBlock(TargetBlock,
                                     NameSuffix,
                                     Predecessor,
                                     VMapVec[s + 1],
                                     BBMapVec[s + 1],
                                     DeadInst);
                    Copied.insert({TargetBlock, s});
                }
                Visited.insert({CurEdge, s});
                auto *TI = dyn_cast<TerminatorInst>(CurEdge.second);
                for (unsigned i = 0;i < TI->getNumSuccessors();++ i){
                    BasicBlock *Pre = TI->getSuccessor(i);
                    Edge NextEdge = std::make_pair(CurEdge.second, &(Pre->front()));
                    if (!Visited.count({NextEdge, s})) Queue.push(NextEdge);
                }
            }


        }

        static void HandleKillEdge(Edge &KillEdge, const std::set<Value *> &ReachingDefinitions) {

        }

        static void HandleCopiedBasicBlock(BasicBlock *TargetBlock,
                                           const Twine &NameSuffix = "",
                                           BasicBlock *&Predecessor,
                                           ValueToValueMapTy &VMap,
                                           BasicBlockToBasicBlockMapTy &BBMap,
                                           std::vector<Instruction *> &DeadInst) {

        }

        static void HandleBasicBlock(BasicBlock *TargetBlock,
                                     const Twine &NameSuffix = "",
                                     BasicBlock *&Predecessor,
                                     ValueToValueMapTy &VMap,
                                     BasicBlockToBasicBlockMapTy &BBMap,
                                     std::vector<Instruction *> &DeadInst) {
            Function *Func = TargetBlock->getParent();
            // clone the target basic block
            BasicBlock *NewBB = CloneBasicBlock(TargetBlock,
                                                VMap,
                                                NameSuffix,
                                                Func);
            // add it to BasicBlock Map
            BBMap[TargetBlock] = NewBB;

            // Process Instruction one by one
            BasicBlock::iterator NewBI = NewBB->begin();
            // (Value, BasicBlock) pair for each PHI Node
            std::vector<std::vector<std::pair<Value *, BasicBlock *> > > KeepIncoming;
            // all PHI Node in the basic block
            std::vector<std::pair<PHINode *, PHINode *> > PNs;
            for (BasicBlock::iterator BI = TargetBlock->begin(), E = TargetBlock->end(); BI != E;) {
                Instruction *Inst = &*BI++;
                Instruction *NewInst = &*NewBI++;

                if (auto *PN = dyn_cast<PHINode>(Inst)) {// is a PHI Node
                    auto *PNInNewBB = dyn_cast<PHINode>(NewInst);
                    PNs.emplace_back(PN, PNInNewBB);
                    KeepIncoming.emplace_back(std::vector<std::pair<Value *, BasicBlock *> >());
                    for (unsigned k = 0; k < PN->getNumIncomingValues(); ++k) {
                        if (Predecessor == PN->getIncomingBlock(k)) {//
                            KeepIncoming.back().emplace_back(get(VMap, PN->getIncomingValue(k)),
                                                             get(BBMap, PN->getIncomingBlock(k)));
                        }
                    }
                } else if (!PNs.empty()) {// Here we assume that PHI nodes are all at the beginning of a Basic block
                    for (size_t i = 0; i < PNs.size(); ++i) {
                        auto OrgN = PNs[i].first;
                        auto OldN = PNs[i].second;
                        auto NewN = HandlePHINode(OldN,
                                                  NameSuffix,
                                                  KeepIncoming[i],
                                                  DeadInst);
                        VMap[OrgN] = NewN;// Update @VMap
                    }

                    break; // jump out the loop
                }
            }


        }

        // Note that this function only handles the predecessors of a PHI Node
        static PHINode *HandlePHINode(PHINode *PN,
                                      const Twine &NameSuffix = "",
                                      const std::vector<std::pair<Value *, BasicBlock *> > &RevivalIncoming,
                                      std::vector<Instruction *> &DeadInst) {
            BasicBlock *BB = PN->getParent();
            auto Ty = PN->getType();
            auto NewPHIName = PN->getName() + NameSuffix;
            // Insert a new PHI Node before @PN
            PHINode *NewPN = PHINode::Create(Ty, static_cast<unsigned int>(RevivalIncoming.size()), NewPHIName, PN);
            // might not be necessary
            replaceAllUsesOfWithIn(PN, NewPN, BB);// PS: replaceAllUsesWith might work
            //
            for (auto VBPair: RevivalIncoming) {
                NewPN->addIncoming(VBPair.first, VBPair.second);
            }
            // Add @PN to DeadInst
            // Note that we can not remove @PN here, because other instructions
            // might use the value defined by it.
            DeadInst.push_back(PN);
            // return the new PHI Node so that we can fix the value map for it
            return NewPN;
        }

    };

    struct ModifyCFG : FunctionPass {
        static char ID;

        ModifyCFG() : FunctionPass(ID) {}

        bool runOnFunction(Function &F) override;

        void getAnalysisUsage(AnalysisUsage &AU) const override {
            AU.setPreservesCFG();
//            AU.addRequired<DominatorTreeWrapperPass>();
//            AU.addRequired<LoopInfoWrapperPass>();
        }
    };

    char ModifyCFG::ID = 0;

    bool ModifyCFG::runOnFunction(Function &F) {
        bool Change = false;
        Instruction *SplitInst = nullptr;
        BasicBlock *TargetBlock = nullptr;
        for (BasicBlock &BB: F) {
            for (BasicBlock::iterator BI = BB.begin(), E = BB.end(); BI != E;) {
                Instruction *Inst = &*BI++;
                if (PHINode *PHI = dyn_cast<PHINode>(Inst)) {
                    TargetBlock = &BB;
                    SplitInst = Inst;
                    break;
                }
            }
            if (SplitInst) break;
        }
        // Note that the following several lines only work for the contrived example, i.e., cfg_modify.ll
        // in tests folder
        if (TargetBlock && SplitInst) {
            Change = true;// Yes, we will change the CFG

            // extract all information regarding the PHINode
            PHINode *PHI = dyn_cast<PHINode>(SplitInst);
            Value *V1 = PHI->getIncomingValue(0);
            BasicBlock *Pre1 = PHI->getIncomingBlock(0);
            Value *V2 = PHI->getIncomingValue(1);
            BasicBlock *Pre2 = PHI->getIncomingBlock(1);
            auto Ty = PHI->getType();
            auto Name = PHI->getName();
            TerminatorInst *TI = PHI->getIncomingBlock(0)->getTerminator();
            TerminatorInst *TI2 = PHI->getIncomingBlock(1)->getTerminator();

            // Clone the basic block
            ValueToValueMapTy VMap;
            BasicBlock *NewBB = CloneBasicBlock(TargetBlock, VMap,
                                                ".p1", TargetBlock->getParent());

            // make the first predecessor of @PHI points to the new basic block
            TI->replaceUsesOfWith(TargetBlock, NewBB);
            // replace the PHINode in the new basic block with a new one
            // Note that the value associated with the PHINode will miss
            // so we have to handle all information regarding this
            // new PHINode manually. What's worse, if there are multiple
            // PHINode inside the basic block, we have to handle them
            // one by one. Currently, I did not find a better way to do this.
            PHINode *oldPN = dyn_cast<PHINode>(NewBB->begin());
            PHINode *NewPN = PHINode::Create(Ty, (unsigned) 1, Name + ".p1", oldPN);

            replaceAllUsesOfWithIn(oldPN, NewPN, NewBB);// PS: replaceAllUsesWith might work
            // Pay attention: The following line is added because the value associating with
            // the PHINode will miss after cloning. That is, it still believe that the corresponding
            // value is a use of the old PHINode before cloning (i.e., @PHI)
            replaceAllUsesOfWithIn(PHI, NewPN, NewBB);// PS: replaceAllUsesWith will not work
            // remove old PHINode
            oldPN->eraseFromParent();
            // Add a predecessor
            NewPN->addIncoming(V1, Pre1);

            // Note that we need also handle values in successors (not in this example, but Target is
            // the last one)
            for (BasicBlock::iterator BI = TargetBlock->begin(), E = TargetBlock->end(); BI != E;) {
                Instruction *Inst = &*BI++;
                if (Value *V = dyn_cast<Value>(Inst)) {
                    replaceAllUsesOfWithIn(V, VMap[V], NewBB);
                }
            }

            // The following line is for debug purpose
            PrintBB(NewBB);

            // Note that we can achieve this part by modifying the existing
            // basic block (i.e., @TargetBlock). However, it seems easier
            // to clone a new one and then remove the old new.

            VMap.clear();
            BasicBlock *NewBB2 = CloneBasicBlock(TargetBlock, VMap,
                                                 ".p2", TargetBlock->getParent());
            TI2->replaceUsesOfWith(TargetBlock, NewBB2);
            PHINode *OldPN2 = dyn_cast<PHINode>(NewBB2->begin());
            PHINode *NewPN2 = PHINode::Create(Ty, (unsigned) 1, Name + ".p2",
                                              OldPN2);
            replaceAllUsesOfWithIn(OldPN2, NewPN2, NewBB2);
            replaceAllUsesOfWithIn(PHI, NewPN2, NewBB2);
            OldPN2->eraseFromParent();
            NewPN2->addIncoming(V2, Pre2);


            for (BasicBlock::iterator BI = TargetBlock->begin(), E = TargetBlock->end(); BI != E;) {
                Instruction *Inst = &*BI++;
                if (Value *V = dyn_cast<Value>(Inst)) {
                    replaceAllUsesOfWithIn(V, VMap[V], NewBB2);
                }
            }

            // The following line is for debug purpose
            PrintBB(NewBB2);
            // remove the old basic block
            TargetBlock->eraseFromParent();
        }

        return Change;
    }

    static RegisterPass<ModifyCFG> X(PASS_NAME,
                                     "CFG Modification Test",
                                     false,
                                     false);
}
