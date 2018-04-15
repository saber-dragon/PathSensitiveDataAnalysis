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

#include "llvm/Pass.h"
#include "llvm/IR/Module.h"// for ModulePass
#include "llvm/IR/Function.h"
#include "llvm/IR/InstrTypes.h" // TerminatorInst
#include "llvm/IR/Value.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Instruction.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Format.h"// for format()
#include "llvm/ADT/GraphTraits.h"// for GraphTraits

#include "PathSensitiveAnalysisConfig.h"

using namespace llvm;

namespace {

    struct PathSensitiveAnalysis : FunctionPass {
        static char ID;

        PathSensitiveAnalysis() : FunctionPass(ID){

        }
        bool runOnFunction(Function& F) override{

            return true;// change CFG
        }
        void print(raw_ostream& os, const Module*) const override {

        }
    };

}