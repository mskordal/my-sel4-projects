#include "InjectBRAM.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"


using namespace llvm;

PreservedAnalyses InjectBRAM::run(	Function &F,
										FunctionAnalysisManager &AM) {
	errs() << F.getName() << "\n";
	return PreservedAnalyses::all();
}

//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getInjectBRAMPluginInfo()
{
	return
	{
		LLVM_PLUGIN_API_VERSION, "InjectBRAM", LLVM_VERSION_STRING,
		[](PassBuilder &PB)
		{
			PB.registerPipelineParsingCallback
			(
				[](StringRef Name, FunctionPassManager &FPM,
				ArrayRef<PassBuilder::PipelineElement>)
				{
					if (Name == "inject-bram")
					{
						FPM.addPass(InjectBRAM());
						return true;
					}
					return false;
				}
			);
		}
	};
}

// This is the core interface for pass plugins. It guarantees that 'opt' will
// be able to recognize HelloWorld when added to the pass pipeline on the
// command line, i.e. via '-passes=hello-world'
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo()
{
	return getInjectBRAMPluginInfo();
}

