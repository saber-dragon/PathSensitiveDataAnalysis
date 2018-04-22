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
    struct JustTryMe : FunctionPass {
        static char ID;
        JustTryMe() : FunctionPass(ID){}

        bool runOnFunction(Function &F) override {
            for (BasicBlock &BB: F){
                errs() << "Basic Block : " << BB.getName() << "\n";
                for (BasicBlock::iterator I = BB.begin(), E = BB.end(); I != E;){
                    Instruction* Inst = &*I++;
                    for (auto k = 0;k < Inst->getNumOperands();++ k){
                        errs() << Inst->getOperand(k)->getName() << " ";
                    }
                    errs() << "\n";
                }

//                for (BasicBlock::use_iterator I = BB.use_begin(), E = BB.use_end();I != E;){
//                    Use *U = &*I++;
//                    errs() << toString(U->get()) << " : ";
//                    errs() << U->get()->getName() << "\n";
//                }
            }
            return false;
        }

    };
    char JustTryMe::ID = 0;
    static RegisterPass<JustTryMe> X(PASS_NAME,
                                     "Just Try Me",
                                     false,
                                     false);
}

