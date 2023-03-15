#ifndef LLVM_INJECT_BRAM_H
#define LLVM_INJECT_BRAM_H

#include "llvm/IR/PassManager.h"

namespace llvm
{
	class InjectBRAMMod : public PassInfoMixin<InjectBRAMMod>
	{
		public:
			PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
	};
} // namespace llvm

#endif // LLVM_INJECT_BRAM_H
