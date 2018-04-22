//
// Created by saber on 4/21/18.
//

#ifndef PATHSENSITIVEANALYSIS_CFGMODIFIER_HPP
#define PATHSENSITIVEANALYSIS_CFGMODIFIER_HPP
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

//#include "PathSensitiveAnalysisConfig.h"
#include "Utils.hpp"

using namespace llvm;
using namespace saber;

namespace saber {
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
                                 //const DenseMap<BasicBlock *, int> &ID,
                                 //const std::vector<std::vector<bool> > &Reachable,
                                 DenseMap<BasicBlock*, DenseSet<BasicBlock*> > &ReachTo,
                                 DominatorTree *DT,
                                 LoopInfo *LI) {
            EqClass s, sOld;
            std::vector<ValueToValueMapTy> VMapVec(NumOfClasses);
            std::vector<BasicBlockToBasicBlockMapTy> BBMapVec(NumOfClasses);
            std::vector<Instruction *> DeadInst;
            DenseSet<std::pair<BasicBlock *, EqClass>> Copied;
            DenseSet<std::pair<Edge, EqClass>> Visited;
            std::vector<std::set<BasicBlock *> > ClonedBB(NumOfClasses);
            DenseMap<std::pair<BasicBlock *, EqClass>, DenseSet<std::pair<BasicBlock *, EqClass > >/* cloned bb */> DMBlockMap;

            std::queue<std::pair<Edge, EqClass > > Queue;
            bool IsRevivalEdge;

            // Push in queue all the inedges
            for (auto &InEdge : InEdges) {
                s = CZero;
                Queue.push({InEdge, s});// go into the RoI through another edge

                // traverse each edge
                while (!Queue.empty()) {
                    auto CSPair = Queue.front();
                    auto CurEdge = CSPair.first;
                    s = CSPair.second;
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
                    sOld = s;
                    if (RMap.count(CurEdge)) {// revival edge
//                        sOld = s;
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
                        //
                        if (NewBB->getFirstNonPHI() == &(NewBB->front())){
                            // no phi, need fix precessor
                            get(BBMapVec[sOld + 1], Predecessor)->getTerminator()->replaceUsesOfWith(
                                    TargetBlock, NewBB
                                    );
                        }
                        ClonedBB[s + 1].insert(NewBB);
//                        if (IsRevivalEdge) {// pay attention
//                            auto BCPair = std::make_pair(TargetBlock, sOld);
//                            if (!DMBlockMap.count(BCPair)) {
//                                DMBlockMap.insert({BCPair, std::make_pair(NewBB, s)});
//                            }
//                        }
                        // add it to BasicBlock Map
                        BBMapVec[s + 1][TargetBlock] = NewBB;
                        Copied.insert({TargetBlock, s});
                    }
//                    else {
                    if (IsRevivalEdge) {// pay attention
                        auto BCPair = std::make_pair(TargetBlock, sOld);
                        if (!DMBlockMap.count(BCPair)) {
                            DMBlockMap[BCPair] = DenseSet<std::pair<BasicBlock*,EqClass > >();
                        }
                        DMBlockMap[BCPair].insert({BBMapVec[s + 1][TargetBlock], s});

                    }
//                    }
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
                        if (!Visited.count({NextEdge, s})) Queue.push({NextEdge, s});
                        else if (RMap.count(NextEdge)){
                            auto BCPair = std::make_pair(Pre, s);
                            auto sNew = RMap.lookup(NextEdge);
                            if (!DMBlockMap.count(BCPair)) {
                                DMBlockMap[BCPair] = DenseSet<std::pair<BasicBlock*,EqClass > >();
                            }
                            DMBlockMap[BCPair].insert({BBMapVec[sNew + 1][Pre], sNew});
                        }
                    }
                }
            }

            errs() << "Cloning finished ...\n";
            DenseMap<BasicBlock *, std::set<Value *> > DefInBlock;
            auto AllDefinitions = LocalDefinition(Region, DefInBlock);

            DenseMap<std::pair<BasicBlock*, EqClass >, PHINode*> NewDMPN;
            //DenseSet<PHINode*> NewPNSet;
            DenseMap<PHINode*, PHINode*> PNReverseMap;
//            DenseSet<std::pair<BasicBlock*, PHINode*> > ClonedPN;
            errs() << "Fix PHI nodes ...\n";
            // fix PHI nodes
            for (auto &BBPair : DMBlockMap) {
                auto *OldBB = BBPair.first.first;
                EqClass EC = BBPair.first.second;

                for (auto &NewPair : BBPair.second){
                    auto *NewBB = NewPair.first;

                    errs() << "replace " << OldBB->getName() << " with " << NewBB->getName() << "\n";

                    EqClass ECNew = NewPair.second;
//                    BasicBlock::iterator BIOrg = OldBB->begin();
                    BasicBlock::iterator BI = NewBB->begin();

                    PHINode *PN = nullptr;
                    //for (BasicBlock::iterator BI = NewBB->begin(), BE = NewBB->end(); BI != BE;) {
                    for (BasicBlock::iterator BIOrg = OldBB->begin(), BEOrg = OldBB->end(); BIOrg != BEOrg;) {
                        PN = dyn_cast<PHINode>(BI++);
                        if (PHINode *PNOrg =  dyn_cast<PHINode>(BIOrg++)){
//                            if (PNReverseMap.count(PN)) {
//                                PNOrg = PNReverseMap.lookup(PN);
//                            }
//                            else {
//                                PNOrg = dyn_cast<PHINode>(BIOrg++);
//                            }

                            std::vector<std::pair<Value *, BasicBlock *> > RevivalIncoming;
                            // assert(EC != CZero);
                            // pay attention
                            for (const auto &k : Revivals[ECNew]) {
                                RevivalIncoming.emplace_back(PNOrg->getIncomingValue(k),
                                                             get(BBMapVec[EC + 1], PNOrg->getIncomingBlock(k)));
                            }
                            if (!NewDMPN.count({OldBB, ECNew}))
                            {
                                auto *NewPN = HandlePHINode(PN, OldBB, "", RevivalIncoming, DeadInst);
                                VMapVec[ECNew + 1][PNOrg] = NewPN;
                                NewDMPN[{OldBB, ECNew}] = NewPN;
                                PNReverseMap[NewPN] = PNOrg;
                            }
                            else
                            {
                                for (auto VBPair: RevivalIncoming) {
                                    VBPair.second->getTerminator()->replaceUsesOfWith(OldBB, NewDMPN[{OldBB, ECNew}]->getParent());
                                    NewDMPN[{OldBB, ECNew}]->addIncoming(VBPair.first, VBPair.second);
                                }
                            }

                        }
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
                    if (VMapVec[i].count(V)) {
                        errs() << "\tReplace " << V->getName() << " with " << VMapVec[i][V]->getName() << "\n";
                        replaceAllUsesOfWithIn(V, VMapVec[i][V], ClonedBB[i]);
                    }
                }
            }


            errs() << "Handle kill edges ...\n";
            // handle kill edges
            for (auto &KillEdge: OutEdges) {
                DenseMap<Value *, PHINode *> PNOnKillEdge;
                auto *From = KillEdge.first->getParent();
                auto *To = KillEdge.second->getParent();
                auto *NewBB = SplitEdge(From, To, DT, LI);
                //auto FromId = ID.lookup(From);

                for (auto *OtherBB : Region) {
                    //auto OtherId = ID.lookup(OtherBB);
                    //if (OtherBB == From || Reachable[OtherId][FromId]) {
                    if (ReachTo[From].count(OtherBB)) {
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
                        // pay attention : Predecessor might not being copied
                        auto *NewPN = HandlePHINode(
                                PN, TargetBlock, NameSuffix,
                                //{{get(VMap, VOld), get(BBMap, Predecessor)}},
                                {{VOld, get(BBMap, Predecessor)}},
                                //{{get(VMap, VOld), get(BBMap, Predecessor)}},
                                DeadInst);
//                BBMap[Predecessor]->getTerminator()->replaceUsesOfWith(
//                        TargetBlock, NewBB
//                );
                        VMap[PNOrg] = NewPN;
                    }
                } else {
                    break;
                }
            }
        }

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
}
#endif //PATHSENSITIVEANALYSIS_CFGMODIFIER_HPP
