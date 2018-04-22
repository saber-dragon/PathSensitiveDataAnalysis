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
#include <set>

#include "PathSensitiveAnalysisConfig.h"
#include "Utils.hpp"

using namespace llvm;
using namespace saber;

namespace {
    using BasicBlockToBasicBlockMapTy = DenseMap<BasicBlock *, BasicBlock *>;
    using Edge = std::pair<Instruction *, Instruction *>;
    using EqClass = int;

    void BuildBasicBlockId(Function &F, DenseMap<BasicBlock *, int> &ID) {
        int id = 0;
        for (BasicBlock &BB: F) {
            ID[&BB] = id++;
        }
    }

    void GetReachableNodes(Function &F,
                           const DenseMap<BasicBlock *, int> &ID,
                           std::vector<std::vector<bool> > &TransitiveClosureBB) {
        assert(ID.size() == F.size());

        if (TransitiveClosureBB.size() != F.size()) {
            TransitiveClosureBB.resize(F.size());
            // allocate space
            for (auto &v: TransitiveClosureBB) { v.resize(F.size()); }
            // initialize
            std::for_each(TransitiveClosureBB.begin(), TransitiveClosureBB.end(),
                          [](std::vector<bool> &v) {
                              std::fill(v.begin(), v.end(), false);
                          });
        }
        for (BasicBlock &BB : F) {
            auto *TI = BB.getTerminator();
            auto i = ID.lookup(&BB);

            for (unsigned int k = 0, e = TI->getNumSuccessors(); k < e; ++k) {
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
                    if (TransitiveClosureBB[i][k] && TransitiveClosureBB[k][j]) {
                        TransitiveClosureBB[i][j] = true;
                    }
                }
            }
        }
    }

    class BasicBlockHepler {
        static const EqClass CZero = -1;

    public:
        static std::set<Value *> LocalDefinition(const std::set<BasicBlock *> &Region,
                                                 DenseMap<BasicBlock *, std::set<Value *> > &DefInBlock) {
            std::set<Value *> AllDefinitions;
            for (BasicBlock *BB: Region) {
                DefInBlock[BB] = std::set<Value *>();
                for (BasicBlock::iterator BI = BB->begin(), E = BB->end(); BI != E;) {
                    Instruction *Inst = &*BI++;
                    if (auto *V = dyn_cast<Value>(Inst)) {
                        AllDefinitions.insert(V);
                        DefInBlock[BB].insert(V);
                    }
                }
            }
            return AllDefinitions;
        }

        static std::set<Value *> LocalDefinition(std::set<Instruction *> &Region) {
            std::set<Value *> AllDefinitions;
            for (auto *Inst: Region) {
                if (auto *V = dyn_cast<Value>(Inst)) {
                    AllDefinitions.insert(V);
                }
            }
            return AllDefinitions;
        }

        static void HandleRegion(std::set<BasicBlock *> &Region,
                                 std::set<BasicBlock *> &RegionOutside,
                                 const std::set<Edge> &InEdges,
                                 const std::set<Edge> &OutEdges,
                                 const DenseMap<Edge, EqClass> &RMap,
                                 const std::vector<std::vector<unsigned> > &Revivals,
                                 const unsigned NumOfClasses,
                                 const DenseMap<BasicBlock *, int> &ID,
                                 const std::vector<std::vector<bool> > &Reachable,
                                 DominatorTree *DT,
                                 LoopInfo *LI) {
            EqClass s = CZero;
            std::vector<ValueToValueMapTy> VMapVec(NumOfClasses);
            std::vector<BasicBlockToBasicBlockMapTy> BBMapVec(NumOfClasses);
            std::vector<Instruction *> DeadInst;
            DenseSet<std::pair<BasicBlock *, EqClass>> Copied;
            DenseSet<std::pair<Edge, EqClass>> Visited;
            std::vector<std::set<BasicBlock *> > ClonedBB(NumOfClasses);
            DenseMap<std::pair<BasicBlock *, EqClass>, BasicBlock */* cloned bb */> DMBlockMap;

            std::queue<Edge> Queue;
            bool IsRevivalEdge;

            // Push in queue all the inedges
            for (auto &InEdge : InEdges) {
                Queue.push(InEdge);// go into the RoI through another edge
                s = CZero; // reset state
                // traverse each edge
                while (!Queue.empty()) {
                    auto CurEdge = Queue.front();

                    errs() << "Processing edge : " << toString(CurEdge) << "\n";
                    errs() << "Current state : " << (s + 1) << "\n";

                    Queue.pop();
                    IsRevivalEdge = false;
                    if (OutEdges.count(CurEdge)) {// kill edge
                        s = CZero;
                        if (!Visited.count({CurEdge, s})) {
                            Visited.insert({CurEdge, s});
                        }
                        // HandleKillEdge(CurEdge, RDMap.lookup(CurEdge));
                        continue;
                    }

                    if (RMap.count(CurEdge)) {// revival edge
                        s = RMap.lookup(CurEdge);
                        assert(s + 1 < NumOfClasses);
                        IsRevivalEdge = true;
                    }
                    BasicBlock *Predecessor = CurEdge.first->getParent();
                    BasicBlock *TargetBlock = CurEdge.second->getParent();
                    assert(Region.count(TargetBlock));
                    std::string NameSuffix(".p" + std::to_string(s + 1));

                    if (!Copied.count({TargetBlock, s})) {
                        Function *Func = TargetBlock->getParent();
                        // clone the target basic block
                        BasicBlock *NewBB = CloneBasicBlock(TargetBlock,
                                                            VMapVec[s + 1],
                                                            NameSuffix,
                                                            Func);
                        ClonedBB[s + 1].insert(NewBB);
                        if (IsRevivalEdge) {//
                            auto BCPair = std::make_pair(TargetBlock, s);
                            if (!DMBlockMap.count(BCPair)) {
                                DMBlockMap[BCPair] = NewBB;
                            }
                        }
                        // add it to BasicBlock Map
                        BBMapVec[s + 1][TargetBlock] = NewBB;
                        Copied.insert({TargetBlock, s});
                    }
                    // handle phinodes
                    if (!IsRevivalEdge)
                        HandlePHINodes(BBMapVec[s + 1][TargetBlock],
                                       TargetBlock,
                                       Predecessor,
                                       NameSuffix,
                                       VMapVec[s + 1],
                                       BBMapVec[s + 1],
                                       DeadInst);
                    Visited.insert({CurEdge, s});
                    // pay attention
                    auto *TI = CurEdge.second->getParent()->getTerminator();
                    for (unsigned i = 0; i < TI->getNumSuccessors(); ++i) {
                        BasicBlock *Pre = TI->getSuccessor(i);
                        Edge NextEdge = std::make_pair(dyn_cast<Instruction>(TI), &(Pre->front()));
                        if (!Visited.count({NextEdge, s})) Queue.push(NextEdge);
                    }
                }
            }

            errs() << "Cloning finished ...\n";
            DenseMap<BasicBlock *, std::set<Value *> > DefInBlock;
            auto AllDefinitions = LocalDefinition(Region, DefInBlock);

            errs() << "Fix PHI nodes ...\n";
            // fix PHI nodes
            for (auto &BBPair : DMBlockMap) {
                auto *OldBB = BBPair.first.first;
                auto *NewBB = BBPair.second;
                EqClass EC = BBPair.first.second;
                BasicBlock::iterator BIOrg = OldBB->begin();
                for (BasicBlock::iterator BI = NewBB->begin(), BE = NewBB->end(); BI != BE;) {
                    if (PHINode *PN = dyn_cast<PHINode>(BI++)) {
                        auto *PNOrg = dyn_cast<PHINode>(BIOrg++);
                        std::vector<std::pair<Value *, BasicBlock *> > RevivalIncoming;
                        assert(EC != CZero);
                        for (const auto &k : Revivals[EC]) {
                            RevivalIncoming.emplace_back(PNOrg->getIncomingValue(k),
                                                         PNOrg->getIncomingBlock(k));
                        }
                        auto *NewPN = HandlePHINode(PN, OldBB, "", RevivalIncoming, DeadInst);
                        VMapVec[EC + 1][PNOrg] = NewPN;
                    }
                }
            }

            errs() << "Fix def-use issues ...\n";
            // pay attention
            for (EqClass i = CZero + 1; i < static_cast<EqClass >(NumOfClasses); ++i) {
                //auto i = k + 1;
                errs() << "For state " << i << " : ";
                if (ClonedBB[i].empty()) {
                    errs() << "No cloned Blocks" << "\n";
                    continue;
                }
                // for debug purpose
                for (auto &BB : ClonedBB[i]) {
                    PrintBB(BB);
                }
                // handle defs
                for (auto &V : AllDefinitions) {

                    errs() << "\tReplace " << V->getName() << " with " << VMapVec[i][V]->getName() << "\n";

                    replaceAllUsesOfWithIn(V, VMapVec[i][V], ClonedBB[i]);
                }
            }


            errs() << "Handle kill edges ...\n";
            // handle kill edges
            for (auto &KillEdge: OutEdges) {
                DenseMap<Value *, PHINode *> PNOnKillEdge;
                auto *From = KillEdge.first->getParent();
                auto *To = KillEdge.second->getParent();
                auto *NewBB = SplitEdge(From, To, DT, LI);
                auto FromId = ID.lookup(From);

                for (auto *OtherBB : Region) {
                    auto OtherId = ID.lookup(OtherBB);
                    if (OtherBB == From || Reachable[OtherId][FromId]) {
                        for (auto &Def : DefInBlock[OtherBB]) {
                            bool UsedOutSide = false;

                            for (auto UI = Def->user_begin(), UE = Def->user_end(); UI != UE;) {
                                Use &TheUse = UI.getUse();
                                ++UI;
                                if (Instruction *Inst = dyn_cast<Instruction>(TheUse.getUser())) {
                                    if (!Region.count(Inst->getParent())) {
                                        UsedOutSide = true;
                                        break;
                                    }
                                }
                            }
                            if (UsedOutSide) {
                                if (!PNOnKillEdge.count(Def)) {
                                    PHINode *NewPN = PHINode::Create(Def->getType(), 1, ".insert", NewBB->getTerminator());
                                    PNOnKillEdge[Def] = NewPN;
                                    replaceAllUsesOfWithIn(Def, NewPN, RegionOutside);
                                    for (unsigned k = 0; k < NumOfClasses; ++k) {
                                        if (VMapVec[k].count(Def)) {
                                            PrintBB(BBMapVec[k][From]);

                                            NewPN->addIncoming(VMapVec[k][Def], BBMapVec[k][From]);
                                            BBMapVec[k][From]->getTerminator()->replaceUsesOfWith(To,
                                                                                                  NewBB);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                PrintBB(NewBB);
                PrintBB(To);
            }


            //
            errs() << "Remove duplicate PHI nodes ...\n";
            for (auto &Dead : DeadInst) {
                Dead->eraseFromParent();
            }
            //
            // Loop over all dead blocks, remembering them and deleting all instructions
            // in them.
            errs() << "Remove dead blocks ...\n";
            std::vector<BasicBlock *> DeadBlocks;
            for (auto BB : Region) {
                DeadBlocks.push_back(BB);
//                while (PHINode *PN = dyn_cast<PHINode>(BB->begin())) {
//                    PN->replaceAllUsesWith(Constant::getNullValue(PN->getType()));
//                    BB->getInstList().pop_front();
//                }
//                for (succ_iterator SI = succ_begin(BB), E = succ_end(BB); SI != E; ++SI)
//                    (*SI)->removePredecessor(BB);
                BB->dropAllReferences();
            }

            // Actually remove the blocks now.
            for (unsigned i = 0, e = static_cast<unsigned int>(DeadBlocks.size()); i != e; ++i) {
                DeadBlocks[i]->eraseFromParent();
            }
        }

        static void HandlePHINodes(BasicBlock *NewBB,
                                   BasicBlock *TargetBlock,
                                   BasicBlock *Predecessor,
                                   const std::string &NameSuffix,
                                   ValueToValueMapTy &VMap,
                                   BasicBlockToBasicBlockMapTy &BBMap,
                                   std::vector<Instruction *> &DeadInst) {
            BasicBlock::iterator BIOrg = TargetBlock->begin();
            for (BasicBlock::iterator BI = NewBB->begin(), BE = NewBB->end(); BI != BE;) {
                if (PHINode *PN = dyn_cast<PHINode>(BI++)) {
                    auto *PNOrg = dyn_cast<PHINode>(BIOrg++);
                    int TargetIncomingInd = -1;
                    Value *VOld = nullptr;
                    for (unsigned i = 0, e = PN->getNumIncomingValues(); i != e; ++i) {
                        if (PNOrg->getIncomingBlock(i) == Predecessor) {
                            TargetIncomingInd = i;
                            VOld = PN->getIncomingValue(i);
                            break;
                        }
                    }
                    if (TargetIncomingInd >= 0) {// fix predecessor
                        auto *NewPN = HandlePHINode(
                                PN, TargetBlock, NameSuffix,
                                {{get(VMap, VOld), BBMap[Predecessor]}},
                                DeadInst);
//                BBMap[Predecessor]->getTerminator()->replaceUsesOfWith(
//                        TargetBlock, NewBB
//                );
                        VMap[PN] = NewPN;
                    }
                } else {
                    break;
                }
            }
        }
//        static void HandleKillEdge(Edge &KillEdge, const std::set<Value *> &ReachingDefinitions) {
//
//        }

//        static void HandleCopiedBasicBlock(BasicBlock *TargetBlock,
//                                           const Twine &NameSuffix = "",
//                                           BasicBlock *&Predecessor,
//                                           ValueToValueMapTy &VMap,
//                                           BasicBlockToBasicBlockMapTy &BBMap,
//                                           std::vector<Instruction *> &DeadInst) {
//
//        }

//        static void HandleBasicBlock(BasicBlock *TargetBlock,
//                                     const Twine &NameSuffix = "",
//                                     BasicBlock *&Predecessor,
//                                     ValueToValueMapTy &VMap,
//                                     BasicBlockToBasicBlockMapTy &BBMap,
//                                     std::vector<Instruction *> &DeadInst) {
//            Function *Func = TargetBlock->getParent();
//            // clone the target basic block
//            BasicBlock *NewBB = CloneBasicBlock(TargetBlock,
//                                                VMap,
//                                                NameSuffix,
//                                                Func);
//            // add it to BasicBlock Map
//            BBMap[TargetBlock] = NewBB;
//
////            // Process Instruction one by one
////            BasicBlock::iterator NewBI = NewBB->begin();
////            // (Value, BasicBlock) pair for each PHI Node
////            std::vector<std::vector<std::pair<Value *, BasicBlock *> > > KeepIncoming;
////            // all PHI Node in the basic block
////            std::vector<std::pair<PHINode *, PHINode *> > PNs;
////            for (BasicBlock::iterator BI = TargetBlock->begin(), E = TargetBlock->end(); BI != E;) {
////                Instruction *Inst = &*BI++;
////                Instruction *NewInst = &*NewBI++;
////
////                if (auto *PN = dyn_cast<PHINode>(Inst)) {// is a PHI Node
////                    auto *PNInNewBB = dyn_cast<PHINode>(NewInst);
////                    PNs.emplace_back(PN, PNInNewBB);
////                    KeepIncoming.emplace_back(std::vector<std::pair<Value *, BasicBlock *> >());
////                    for (unsigned k = 0; k < PN->getNumIncomingValues(); ++k) {
////                        if (Predecessor == PN->getIncomingBlock(k)) {//
////                            KeepIncoming.back().emplace_back(get(VMap, PN->getIncomingValue(k)),
////                                                             get(BBMap, PN->getIncomingBlock(k)));
////                        }
////                    }
////                } else if (!PNs.empty()) {// Here we assume that PHI nodes are all at the beginning of a Basic block
////                    for (size_t i = 0; i < PNs.size(); ++i) {
////                        auto OrgN = PNs[i].first;
////                        auto OldN = PNs[i].second;
////                        auto NewN = HandlePHINode(OldN,
////                                                  NameSuffix,
////                                                  KeepIncoming[i],
////                                                  DeadInst);
////                        VMap[OrgN] = NewN;// Update @VMap
////                    }
////
////                    break; // jump out the loop
////                }
////            }
//
//
//        }

        // Note that this function only handles the predecessors of a PHI Node
        static PHINode *HandlePHINode(PHINode *PN,
                                      BasicBlock *OldBB,
                                      const Twine &NameSuffix,
                                      const std::vector<std::pair<Value *, BasicBlock *> > &RevivalIncoming,
                                      std::vector<Instruction *> &DeadInst) {
            BasicBlock *BB = PN->getParent();
            auto Ty = PN->getType();
            auto NewPHIName = PN->getName() + NameSuffix;
            // Insert a new PHI Node before @PN
            PHINode *NewPN = PHINode::Create(Ty, static_cast<unsigned int>(RevivalIncoming.size()), NewPHIName, PN);
            // might not be necessary
//            replaceAllUsesOfWithIn(PN, NewPN, BB);// PS: replaceAllUsesWith might work
            //
            for (auto VBPair: RevivalIncoming) {
                VBPair.second->getTerminator()->replaceUsesOfWith(OldBB, BB);
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
            AU.addRequired<TargetLibraryInfoWrapperPass>();
            AU.addRequired<DominatorTreeWrapperPass>();
            AU.addRequired<LoopInfoWrapperPass>();
        }

        bool tryToSplitASingleBlock(Function &F);

        bool tryToSplitABlock(Function &F);
    };

    char ModifyCFG::ID = 0;

    bool ModifyCFG::runOnFunction(Function &F) {

        return tryToSplitABlock(F);
    }

    bool ModifyCFG::tryToSplitABlock(Function &F) {
        DenseMap<BasicBlock *, int> ID;
        std::vector<std::vector<bool> > Reachable;
        BuildBasicBlockId(F, ID);
        if (F.size() <= 64) {
            Reachable.resize(F.size());
            // allocate space
            for (auto &v: Reachable) { v.resize(F.size()); }
            // initialize
            std::for_each(Reachable.begin(), Reachable.end(),
                          [](std::vector<bool> &v) {
                              std::fill(v.begin(), v.end(), false);
                          });
            GetReachableNodes(F, ID, Reachable);
        }

        std::set<BasicBlock *> Region;
        std::set<BasicBlock *> RegionOutside;
        std::set<Edge> InEdges;
        std::set<Edge> OutEdges;
        DenseMap<Edge, EqClass> RMap;
        std::vector<std::vector<unsigned >> Revivals;
        unsigned NumOfClasses;
        for (BasicBlock &BB: F) {
            if (BB.getName() == "if.end") {
                Region.insert(&BB);
                PHINode *PN = dyn_cast<PHINode>(BB.begin());
                NumOfClasses = PN->getNumIncomingValues() + 1;
                Revivals.resize(NumOfClasses - 1);
                for (unsigned i = 0; i < NumOfClasses - 1; ++i) {
                    auto RevivalEdge = std::make_pair(&(PN->getIncomingBlock(i)->back()), &(BB.front()));
                    InEdges.insert(RevivalEdge);
                    RMap[RevivalEdge] = static_cast<EqClass>(i);
                    Revivals[i].push_back(i);
                }
                TerminatorInst *TI = BB.getTerminator();
                for (unsigned i = 0; i < TI->getNumSuccessors(); ++i)
                    OutEdges.insert({dyn_cast<Instruction>(TI), dyn_cast<Instruction>(TI->getSuccessor(i)->begin())});
            } else {
                RegionOutside.insert(&BB);
            }
        }

        errs() << "In Edges : \n";
        for (const auto &e: InEdges) {
            errs() << "\t" << toString(e) << "\n";
        }
        errs() << "Out Edges : \n";
        for (const auto &e: OutEdges) {
            errs() << "\t" << toString(e) << "\n";
        }

        errs() << "# of classes : " << NumOfClasses << "\n";

        DominatorTree *DT =
                &getAnalysis<DominatorTreeWrapperPass>().getDomTree();
        LoopInfo *LI =
                &getAnalysis<LoopInfoWrapperPass>().getLoopInfo();

        BasicBlockHepler::HandleRegion(Region,
                                       RegionOutside,
                                       InEdges,
                                       OutEdges,
                                       RMap,
                                       Revivals,
                                       NumOfClasses,
                                       ID,
                                       Reachable,
                                       DT,
                                       LI);

        return true;
    }

    bool ModifyCFG::tryToSplitASingleBlock(Function &F) {
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
