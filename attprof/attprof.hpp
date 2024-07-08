#ifndef LLVM_ACCPROF_H
#define LLVM_ACCPROF_H

#include "llvm/IR/PassManager.h"

namespace llvm
{
	class AttProfMod : public PassInfoMixin<AttProfMod>
	{
		public:
			PreservedAnalyses run(Module &M, ModuleAnalysisManager &AM);
	};
} // namespace llvm

#endif // LLVM_ACCPROF_H
