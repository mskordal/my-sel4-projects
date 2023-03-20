#include "InjectBRAM.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/BasicBlock.h"
#include <iostream> //TODO: for debugging. Remove later

// store i64 117440512, i64* %6, align 8

using namespace llvm;

PreservedAnalyses InjectBRAMMod::run(Module &M, ModuleAnalysisManager &AM)
{
	// Get the context of the module. Probably the Global Context.
	LLVMContext &context = M.getContext();

	Type *Int64PtrTy = Type::getInt64PtrTy(context);

    // Create a global ptr variable with external linkage and a null initializer.
    GlobalVariable *GV = new GlobalVariable(M, Int64PtrTy, false,
		GlobalValue::ExternalLinkage, nullptr, "bramptr");

	// Create a constant 64-bit integer value.
	// APInt is an object that represents an integer of specified bitwidth
    ConstantInt *Int64Val = ConstantInt::get(context, APInt(64, 0xa0010000));

    // Convert the integer value to a pointer. Basically a typecast
    Constant *PtrVal = ConstantExpr::getIntToPtr(Int64Val, Int64PtrTy);

	// Align global variable to 8 bytes
	GV->setAlignment(Align(8));

	// Set the initial value of the global variable. Overwrites if existing
	GV->setInitializer(PtrVal);
	
	// get function main of this module
	Function* mainF = M.getFunction("main");

	//get the first basic block of main
	BasicBlock* main1stBB = &mainF->getEntryBlock();

	// init a IRbuilder pointing in the beginning of the 1st block
	IRBuilder<> builder(main1stBB, main1stBB->begin());

	//Create a load inst of the value of the global variable into a local
	Value* loadedPtr = builder.CreateLoad(GV->getValueType(), GV);

	// create a constant integer of value 1
	Value* constOne = llvm::ConstantInt::get(context, llvm::APInt(64, 1));

	//create a store instruction that stores 1 to the address of pointer
	builder.CreateStore(constOne, loadedPtr);

	return llvm::PreservedAnalyses::all();
}


//-----------------------------------------------------------------------------
// New PM Registration
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getInjectBRAMModPluginInfo()
{
	//std::cerr << "getInjectBRAMModPluginInfo() called\n"; // add this line
	return
	{
		LLVM_PLUGIN_API_VERSION, "InjectBRAMMod", LLVM_VERSION_STRING,
		[](PassBuilder &PB)
		{
			//PB.registerPipelineParsingCallback
			PB.registerOptimizerEarlyEPCallback
			(
				[](ModulePassManager &MPM, OptimizationLevel)
				//[](StringRef Name, ModulePassManager &MPM,
				//ArrayRef<PassBuilder::PipelineElement>)
				{
					//if (Name == "inject-bram")
					//{
					MPM.addPass(InjectBRAMMod());
					return true;
					//}
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
	return getInjectBRAMModPluginInfo();
}

