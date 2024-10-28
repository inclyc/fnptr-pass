#include "FunctionSetAnalysis.h"
#include "Support.h"

#include <llvm/IR/Instructions.h>
#include <llvm/IR/IntrinsicInst.h>
#include <llvm/Support/raw_ostream.h>

using namespace llvm;

namespace {

/// Store the arguments of function call.
struct Frame {
  std::vector<FunctionSet> Args;
};

struct Context {
  FunctionSetMap Set;
  const Module &M;
  std::vector<Frame> Stack;
};

std::set<const Function *> invoke(Context &Ctx, const Function &Callee);

std::set<const Function *> analyzeValue(Context &Ctx, const Value &V) {
  if (const auto PFunc = dyn_cast<const Function>(&V)) {
    Ctx.Set[&V].insert(PFunc);
    return Ctx.Set[&V];
  } else if (dyn_cast_or_null<DbgInfoIntrinsic>(&V)) {
    return {};
  } else if (const auto PCall = dyn_cast_or_null<CallInst>(&V)) {
    const auto &Call = assertDeref(PCall);
    const auto &CallOperand = assertDeref(Call.getCalledOperand());
    const auto PossibleFunctions = analyzeValue(Ctx, CallOperand);

    // For each possible functions, analyze it's returned set.
    // The arguments are hereby passed in.
    std::vector<FunctionSet> Args;
    auto &Set = Ctx.Set[&V];
    for (const auto *PFunc : PossibleFunctions) {
      if (!PFunc)
        continue;
      const auto &Function = assertDeref(PFunc);
      for (const auto &Arg : Call.args()) {
        Args.emplace_back(analyzeValue(Ctx, assertDeref(Arg.get())));
      }
      Ctx.Stack.emplace_back(Frame{std::move(Args)});
      // Invoke that function.
      const auto ReturnedSet = invoke(Ctx, Function);
      Ctx.Stack.pop_back();
      Set.insert(ReturnedSet.begin(), ReturnedSet.end());
    }
    return Set;
  } else if (const auto PReturn = dyn_cast_or_null<ReturnInst>(&V)) {
    const auto &Return = *PReturn;
    const auto *PValue = Return.getReturnValue();
    if (!PValue)
      return {};
    return analyzeValue(Ctx, *PValue);
  } else if (!V.getType()->isPointerTy()) {
    return {};
  } else if (const auto PNull = dyn_cast_or_null<ConstantPointerNull>(&V)) {
    Ctx.Set[&V].insert(nullptr);
    return Ctx.Set[&V];
  } else if (const auto PPHi = dyn_cast_or_null<PHINode>(&V)) {
    const auto &PHI = assertDeref(PPHi);
    std::set<const llvm::Function *> &Set = Ctx.Set[&PHI];
    for (const auto &Use : PHI.incoming_values()) {
      const auto &Value = assertDeref(Use.get());
      const auto S = analyzeValue(Ctx, Value);
      Set.insert(S.begin(), S.end());
    }
    return Set;
  } else if (const auto PArg = dyn_cast_or_null<Argument>(&V)) {
    auto &Set = Ctx.Set[&V];
    const auto &Arg = assertDeref(PArg);
    const auto ArgNo = Arg.getArgNo();
    const auto StackFunctions = Ctx.Stack.back().Args[ArgNo];
    Set.insert(StackFunctions.begin(), StackFunctions.end());
    return Set;
  }

  errs() << V << "\n";
  assert(false && "don't know how to deal with this value!");
}

/// Propagate possible function pointers in the callee
FunctionSet invoke(Context &Ctx, const Function &Callee) {
  FunctionSet Set{};
  for (const auto &BB : Callee) {
    for (const auto &Inst : BB) {
      const auto ValueFunctions = analyzeValue(Ctx, Inst);
      if (const auto *PRet = dyn_cast_or_null<ReturnInst>(&Inst)) {
        Set.insert(ValueFunctions.begin(), ValueFunctions.end());
      }
    }
  }
  return Set;
}

void runOnModule(Context &Ctx) {
  const auto &M = Ctx.M;
  for (const auto &F : M) {
    // Create a dummy stack for each function, and invoke it.
    Frame DummyFrame{};
    DummyFrame.Args.resize(F.arg_size());
    Ctx.Stack.emplace_back(DummyFrame);
    invoke(Ctx, F);
    Ctx.Stack.pop_back();
  }
}

/// Stores all possible function for each value.
void analyzeFunctionImpl(Context &Ctx) { runOnModule(Ctx); }

} // namespace

/// Stores all possible function for each value.
FunctionSetMap analyzeFunctions(const Module &M) {
  Context Ctx{{}, M};
  analyzeFunctionImpl(Ctx);
  return Ctx.Set;
}
