#ifndef LLVM_INJECT_BRAM_H
#define LLVM_INJECT_BRAM_H

#include "llvm/IR/PassManager.h"

namespace llvm
{
	class InjectBRAM : public PassInfoMixin<InjectBRAM>
	{
		public:
			PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
	};
} // namespace llvm

#endif // LLVM_INJECT_BRAM_H
