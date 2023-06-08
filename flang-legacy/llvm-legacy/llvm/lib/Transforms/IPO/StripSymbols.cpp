//===- StripSymbols.cpp - Strip symbols and debug info from a module ------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// The StripSymbols transformation implements code stripping. Specifically, it
// can delete:
//
//   * names for virtual registers
//   * symbols for internal globals and functions
//   * debug information
//
// Note that this transformation makes code much less readable, so it should
// only be used in situations where the 'strip' utility would be used, such as
// reducing code size or making it harder to reverse engineer code.
//
//===----------------------------------------------------------------------===//

#include "llvm/Transforms/IPO/StripSymbols.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/TypeFinder.h"
#include "llvm/IR/ValueSymbolTable.h"
#include "llvm/InitializePasses.h"
#include "llvm/Pass.h"
#include "llvm/Transforms/IPO.h"
#include "llvm/Transforms/Utils/Local.h"

using namespace llvm;

namespace {
  class StripSymbols : public ModulePass {
    bool OnlyDebugInfo;
  public:
    static char ID; // Pass identification, replacement for typeid
    explicit StripSymbols(bool ODI = false)
      : ModulePass(ID), OnlyDebugInfo(ODI) {
        initializeStripSymbolsPass(*PassRegistry::getPassRegistry());
      }

    bool runOnModule(Module &M) override;

    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.setPreservesAll();
    }
  };

  class StripNonDebugSymbols : public ModulePass {
  public:
    static char ID; // Pass identification, replacement for typeid
    explicit StripNonDebugSymbols()
      : ModulePass(ID) {
        initializeStripNonDebugSymbolsPass(*PassRegistry::getPassRegistry());
      }

    bool runOnModule(Module &M) override;

    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.setPreservesAll();
    }
  };

  class StripDebugDeclare : public ModulePass {
  public:
    static char ID; // Pass identification, replacement for typeid
    explicit StripDebugDeclare()
      : ModulePass(ID) {
        initializeStripDebugDeclarePass(*PassRegistry::getPassRegistry());
      }

    bool runOnModule(Module &M) override;

    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.setPreservesAll();
    }
  };

  class StripDeadDebugInfo : public ModulePass {
  public:
    static char ID; // Pass identification, replacement for typeid
    explicit StripDeadDebugInfo()
      : ModulePass(ID) {
        initializeStripDeadDebugInfoPass(*PassRegistry::getPassRegistry());
      }

    bool runOnModule(Module &M) override;

    void getAnalysisUsage(AnalysisUsage &AU) const override {
      AU.setPreservesAll();
    }
  };
}

char StripSymbols::ID = 0;
INITIALIZE_PASS(StripSymbols, "strip",
                "Strip all symbols from a module", false, false)

ModulePass *llvm::createStripSymbolsPass(bool OnlyDebugInfo) {
  return new StripSymbols(OnlyDebugInfo);
}

char StripNonDebugSymbols::ID = 0;
INITIALIZE_PASS(StripNonDebugSymbols, "strip-nondebug",
                "Strip all symbols, except dbg symbols, from a module",
                false, false)

ModulePass *llvm::createStripNonDebugSymbolsPass() {
  return new StripNonDebugSymbols();
}

char StripDebugDeclare::ID = 0;
INITIALIZE_PASS(StripDebugDeclare, "strip-debug-declare",
                "Strip all llvm.dbg.declare intrinsics", false, false)

ModulePass *llvm::createStripDebugDeclarePass() {
  return new StripDebugDeclare();
}

char StripDeadDebugInfo::ID = 0;
INITIALIZE_PASS(StripDeadDebugInfo, "strip-dead-debug-info",
                "Strip debug info for unused symbols", false, false)

ModulePass *llvm::createStripDeadDebugInfoPass() {
  return new StripDeadDebugInfo();
}

/// OnlyUsedBy - Return true if V is only used by Usr.
static bool OnlyUsedBy(Value *V, Value *Usr) {
  for (User *U : V->users())
    if (U != Usr)
      return false;

  return true;
}

static void RemoveDeadConstant(Constant *C) {
  assert(C->use_empty() && "Constant is not dead!");
  SmallPtrSet<Constant*, 4> Operands;
  for (Value *Op : C->operands())
    if (OnlyUsedBy(Op, C))
      Operands.insert(cast<Constant>(Op));
  if (GlobalVariable *GV = dyn_cast<GlobalVariable>(C)) {
    if (!GV->hasLocalLinkage()) return;   // Don't delete non-static globals.
    GV->eraseFromParent();
  } else if (!isa<Function>(C)) {
    // FIXME: Why does the type of the constant matter here?
    if (isa<StructType>(C->getType()) || isa<ArrayType>(C->getType()) ||
        isa<VectorType>(C->getType()))
      C->destroyConstant();
  }

  // If the constant referenced anything, see if we can delete it as well.
  for (Constant *O : Operands)
    RemoveDeadConstant(O);
}

// Strip the symbol table of its names.
//
static void StripSymtab(ValueSymbolTable &ST, bool PreserveDbgInfo) {
  for (ValueSymbolTable::iterator VI = ST.begin(), VE = ST.end(); VI != VE; ) {
    Value *V = VI->getValue();
    ++VI;
    if (!isa<GlobalValue>(V) || cast<GlobalValue>(V)->hasLocalLinkage()) {
      if (!PreserveDbgInfo || !V->getName().startswith("llvm.dbg"))
        // Set name to "", removing from symbol table!
        V->setName("");
    }
  }
}

// Strip any named types of their names.
static void StripTypeNames(Module &M, bool PreserveDbgInfo) {
  TypeFinder StructTypes;
  StructTypes.run(M, false);

  for (StructType *STy : StructTypes) {
    if (STy->isLiteral() || STy->getName().empty()) continue;

    if (PreserveDbgInfo && STy->getName().startswith("llvm.dbg"))
      continue;

    STy->setName("");
  }
}

/// Find values that are marked as llvm.used.
static void findUsedValues(GlobalVariable *LLVMUsed,
                           SmallPtrSetImpl<const GlobalValue*> &UsedValues) {
  if (!LLVMUsed) return;
  UsedValues.insert(LLVMUsed);

  ConstantArray *Inits = cast<ConstantArray>(LLVMUsed->getInitializer());

  for (unsigned i = 0, e = Inits->getNumOperands(); i != e; ++i)
    if (GlobalValue *GV =
          dyn_cast<GlobalValue>(Inits->getOperand(i)->stripPointerCasts()))
      UsedValues.insert(GV);
}

/// StripSymbolNames - Strip symbol names.
static bool StripSymbolNames(Module &M, bool PreserveDbgInfo) {

  SmallPtrSet<const GlobalValue*, 8> llvmUsedValues;
  findUsedValues(M.getGlobalVariable("llvm.used"), llvmUsedValues);
  findUsedValues(M.getGlobalVariable("llvm.compiler.used"), llvmUsedValues);

  for (GlobalVariable &GV : M.globals()) {
    if (GV.hasLocalLinkage() && !llvmUsedValues.contains(&GV))
      if (!PreserveDbgInfo || !GV.getName().startswith("llvm.dbg"))
        GV.setName(""); // Internal symbols can't participate in linkage
  }

  for (Function &I : M) {
    if (I.hasLocalLinkage() && !llvmUsedValues.contains(&I))
      if (!PreserveDbgInfo || !I.getName().startswith("llvm.dbg"))
        I.setName(""); // Internal symbols can't participate in linkage
    if (auto *Symtab = I.getValueSymbolTable())
      StripSymtab(*Symtab, PreserveDbgInfo);
  }

  // Remove all names from types.
  StripTypeNames(M, PreserveDbgInfo);

  return true;
}

bool StripSymbols::runOnModule(Module &M) {
  if (skipModule(M))
    return false;

  bool Changed = false;
  Changed |= StripDebugInfo(M);
  if (!OnlyDebugInfo)
    Changed |= StripSymbolNames(M, false);
  return Changed;
}

bool StripNonDebugSymbols::runOnModule(Module &M) {
  if (skipModule(M))
    return false;

  return StripSymbolNames(M, true);
}

static bool stripDebugDeclareImpl(Module &M) {

  Function *Declare = M.getFunction("llvm.dbg.declare");
  std::vector<Constant*> DeadConstants;

  if (Declare) {
    while (!Declare->use_empty()) {
      CallInst *CI = cast<CallInst>(Declare->user_back());
      Value *Arg1 = CI->getArgOperand(0);
      Value *Arg2 = CI->getArgOperand(1);
      assert(CI->use_empty() && "llvm.dbg intrinsic should have void result");
      CI->eraseFromParent();
      if (Arg1->use_empty()) {
        if (Constant *C = dyn_cast<Constant>(Arg1))
          DeadConstants.push_back(C);
        else
          RecursivelyDeleteTriviallyDeadInstructions(Arg1);
      }
      if (Arg2->use_empty())
        if (Constant *C = dyn_cast<Constant>(Arg2))
          DeadConstants.push_back(C);
    }
    Declare->eraseFromParent();
  }

  while (!DeadConstants.empty()) {
    Constant *C = DeadConstants.back();
    DeadConstants.pop_back();
    if (GlobalVariable *GV = dyn_cast<GlobalVariable>(C)) {
      if (GV->hasLocalLinkage())
        RemoveDeadConstant(GV);
    } else
      RemoveDeadConstant(C);
  }

  return true;
}

bool StripDebugDeclare::runOnModule(Module &M) {
  if (skipModule(M))
    return false;
  return stripDebugDeclareImpl(M);
}

/// Collects compilation units referenced by functions or lexical scopes.
/// Accepts any DIScope and uses recursive bottom-up approach to reach either
/// DISubprogram or DILexicalBlockBase.
static void
collectCUsWithScope(const DIScope *Scope, std::set<DICompileUnit *> &LiveCUs,
                    SmallPtrSet<const DIScope *, 8> &VisitedScopes) {
  if (!Scope)
    return;

  auto InS = VisitedScopes.insert(Scope);
  if (!InS.second)
    return;

  if (const auto *SP = dyn_cast<DISubprogram>(Scope)) {
    if (SP->getUnit())
      LiveCUs.insert(SP->getUnit());
    return;
  }
  if (const auto *LB = dyn_cast<DILexicalBlockBase>(Scope)) {
    const DISubprogram *SP = LB->getSubprogram();
    if (SP && SP->getUnit())
      LiveCUs.insert(SP->getUnit());
    return;
  }

  collectCUsWithScope(Scope->getScope(), LiveCUs, VisitedScopes);
}

static void
collectCUsForInlinedFuncs(const DILocation *Loc,
                          std::set<DICompileUnit *> &LiveCUs,
                          SmallPtrSet<const DIScope *, 8> &VisitedScopes) {
  if (!Loc || !Loc->getInlinedAt())
    return;
  collectCUsWithScope(Loc->getScope(), LiveCUs, VisitedScopes);
  collectCUsForInlinedFuncs(Loc->getInlinedAt(), LiveCUs, VisitedScopes);
}

static bool stripDeadDebugInfoImpl(Module &M) {
  bool Changed = false;

  LLVMContext &C = M.getContext();

  // Find all debug info in F. This is actually overkill in terms of what we
  // want to do, but we want to try and be as resilient as possible in the face
  // of potential debug info changes by using the formal interfaces given to us
  // as much as possible.
  DebugInfoFinder F;
  F.processModule(M);

  // For each compile unit, find the live set of global variables/functions and
  // replace the current list of potentially dead global variables/functions
  // with the live list.
  SmallVector<Metadata *, 64> LiveGlobalVariables;
  DenseSet<DIGlobalVariableExpression *> VisitedSet;

  std::set<DIGlobalVariableExpression *> LiveGVs;
  for (GlobalVariable &GV : M.globals()) {
    SmallVector<DIGlobalVariableExpression *, 1> GVEs;
    GV.getDebugInfo(GVEs);
    for (auto *GVE : GVEs)
      LiveGVs.insert(GVE);
  }

  std::set<DICompileUnit *> LiveCUs;
  SmallPtrSet<const DIScope *, 8> VisitedScopes;
  // Any CU is live if is referenced from a subprogram metadata that is attached
  // to a function defined or inlined in the module.
  for (const Function &Fn : M.functions()) {
    collectCUsWithScope(Fn.getSubprogram(), LiveCUs, VisitedScopes);
    for (const_inst_iterator I = inst_begin(&Fn), E = inst_end(&Fn); I != E;
         ++I) {
      if (!I->getDebugLoc())
        continue;
      const DILocation *DILoc = I->getDebugLoc().get();
      collectCUsForInlinedFuncs(DILoc, LiveCUs, VisitedScopes);
    }
  }

  bool HasDeadCUs = false;
  for (DICompileUnit *DIC : F.compile_units()) {
    // Create our live global variable list.
    bool GlobalVariableChange = false;
    for (auto *DIG : DIC->getGlobalVariables()) {
      if (DIG->getExpression() && DIG->getExpression()->isConstant())
        LiveGVs.insert(DIG);

      // Make sure we only visit each global variable only once.
      if (!VisitedSet.insert(DIG).second)
        continue;

      // If a global variable references DIG, the global variable is live.
      if (LiveGVs.count(DIG))
        LiveGlobalVariables.push_back(DIG);
      else
        GlobalVariableChange = true;
    }

    if (!LiveGlobalVariables.empty())
      LiveCUs.insert(DIC);
    else if (!LiveCUs.count(DIC))
      HasDeadCUs = true;

    // If we found dead global variables, replace the current global
    // variable list with our new live global variable list.
    if (GlobalVariableChange) {
      DIC->replaceGlobalVariables(MDTuple::get(C, LiveGlobalVariables));
      Changed = true;
    }

    // Reset lists for the next iteration.
    LiveGlobalVariables.clear();
  }

  if (HasDeadCUs) {
    // Delete the old node and replace it with a new one
    NamedMDNode *NMD = M.getOrInsertNamedMetadata("llvm.dbg.cu");
    NMD->clearOperands();
    if (!LiveCUs.empty()) {
      for (DICompileUnit *CU : LiveCUs)
        NMD->addOperand(CU);
    }
    Changed = true;
  }

  return Changed;
}

/// Remove any debug info for global variables/functions in the given module for
/// which said global variable/function no longer exists (i.e. is null).
///
/// Debugging information is encoded in llvm IR using metadata. This is designed
/// such a way that debug info for symbols preserved even if symbols are
/// optimized away by the optimizer. This special pass removes debug info for
/// such symbols.
bool StripDeadDebugInfo::runOnModule(Module &M) {
  if (skipModule(M))
    return false;
  return stripDeadDebugInfoImpl(M);
}

PreservedAnalyses StripSymbolsPass::run(Module &M, ModuleAnalysisManager &AM) {
  StripDebugInfo(M);
  StripSymbolNames(M, false);
  return PreservedAnalyses::all();
}

PreservedAnalyses StripNonDebugSymbolsPass::run(Module &M,
                                                ModuleAnalysisManager &AM) {
  StripSymbolNames(M, true);
  return PreservedAnalyses::all();
}

PreservedAnalyses StripDebugDeclarePass::run(Module &M,
                                             ModuleAnalysisManager &AM) {
  stripDebugDeclareImpl(M);
  return PreservedAnalyses::all();
}

PreservedAnalyses StripDeadDebugInfoPass::run(Module &M,
                                              ModuleAnalysisManager &AM) {
  stripDeadDebugInfoImpl(M);
  return PreservedAnalyses::all();
}
