#ifndef LLVM_ACCPROF_H
#define LLVM_ACCPROF_H

#include "llvm/IR/PassManager.h"

namespace llvm
{
	class AccProfMod : public PassInfoMixin<AccProfMod>
	{
		public:
			PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
	};
} // namespace llvm

#endif // LLVM_ACCPROF_H
