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

#include "PathSensitiveAnalysisConfig.h"
#include "Utils.hpp"

using namespace llvm;
using namespace saber;

namespace  {

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
        for ( BasicBlock &BB: F){
            for (BasicBlock::iterator BI = BB.begin(), E = BB.end(); BI != E;) {
                Instruction *Inst = &*BI++;
                if ( PHINode* PHI = dyn_cast<PHINode>(Inst) ){
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
                if (Value* V = dyn_cast<Value>(Inst)){
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
                if (Value* V = dyn_cast<Value>(Inst)){
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
