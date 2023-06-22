#include "InjectBRAM.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <iostream> //TODO: for debugging. Remove later
#include <fstream> //to read files with function names
#include <string>

using namespace llvm;

#define BRAM_ADDRESS 0xa0010000
#define HLS_VIRT_ADDR 0x7000000

BasicBlock * createCtrlSignalsLocalVarBlock(BasicBlock *thisBblk,
	BasicBlock *nextBblk, Function *func, LLVMContext& context,
	AllocaInst** currCtrlSigs);
BasicBlock *createSpinLockOnIdleBitBlock(BasicBlock *nextBblk, Function *func,
	LLVMContext& context, AllocaInst **currCtrlSigs, GlobalVariable *globVar,
	Value** idleOff);
BasicBlock *createWriteToHLSBlock(BasicBlock *nextBblk, Function *func,
	LLVMContext& context, AllocaInst** currCtrlSigs, GlobalVariable *globVar,
	int id, int bramAddr);
void addTerminatorsToBlocks(BasicBlock *initBblk, BasicBlock *notIdleBblk,
	BasicBlock *idleBblk, BasicBlock *firstBblk, Value** idleOff);
std::vector<Instruction*> getCallInsts(std::vector<std::string>& funcNames,
	Function* func);
// bool isBlockComplete(BasicBlock* bblk);
// BasicBlock *getNextBlockofBlock(BasicBlock *bblk);

PreservedAnalyses InjectBRAMMod::run(Module &M, ModuleAnalysisManager &AM)
{
	std::string line;
  	std::ifstream myfile ("../myLlvmPass/functions.txt");
	std::vector<std::string> funcNames;
	std::vector<int> funcIds;

	while ( getline (myfile,line) )
	{
		int delim_pos = line.find(" ");
		funcNames.push_back( line.substr(0, delim_pos) );
		funcIds.push_back( stoi( line.substr( delim_pos + 1, line.length() - delim_pos + 1 ) ) );
	}
	errs() << "test\n";
	for(int i=0; i < funcNames.size(); i++)
	{
		errs() << funcNames.at(i) << ": " << funcIds.at(i) << '\n';
	}

	//Get the context of the module. Probably the Global Context.
	LLVMContext& context = M.getContext();
	Type *Int64PtrTy = Type::getInt64PtrTy(context);

	// Create a global ptr var with external linkage and a null initializer.
	GlobalVariable *globVar = new GlobalVariable(M, Int64PtrTy, false,
	    GlobalValue::ExternalLinkage, nullptr, "bramptr");
	// Create a constant 64-bit integer value.
	// APInt is an object that represents an integer of specified bitwidth
	ConstantInt *Int64Val = ConstantInt::get(context, APInt(64, HLS_VIRT_ADDR));
	// Convert the integer value to a pointer. Basically a typecast
	Constant *PtrVal = ConstantExpr::getIntToPtr(Int64Val, Int64PtrTy);
	// Align global variable to 8 bytes
	globVar->setAlignment(Align(8));
	// Set the initial value of the global variable. Overwrites if existing
	globVar->setInitializer(PtrVal);
	
	for(int i = 0; i < funcNames.size(); ++i)
	{
		AllocaInst* currCtrlSigs;
		Value* idleOff;
		// get function the function
		Function *func = M.getFunction(funcNames.at(i));
		
		BasicBlock *firstBblk = &func->getEntryBlock();	

		BasicBlock *initBblk = createCtrlSignalsLocalVarBlock(nullptr,
			firstBblk, func, context, &currCtrlSigs);
		
		BasicBlock *notIdleBblk = createSpinLockOnIdleBitBlock(firstBblk, func,
			context, &currCtrlSigs, globVar, &idleOff);

		BasicBlock *idleBblk = createWriteToHLSBlock(firstBblk, func, context,
			&currCtrlSigs, globVar, funcIds.at(i), BRAM_ADDRESS);

		addTerminatorsToBlocks(initBblk, notIdleBblk, idleBblk, firstBblk,
			&idleOff);

		std::vector<Instruction*> callInsts = getCallInsts(funcNames, func);

		for (Instruction* instr : callInsts)
		{
			BasicBlock* bblk = instr->getParent();
			Instruction* firstInstr = bblk->getFirstNonPHI();
			if(firstInstr != instr)
				bblk = SplitBlock(bblk, instr);

			BasicBlock* nextBblk = SplitBlock(bblk,
				instr->getNextNonDebugInstruction());

			BasicBlock *initBblkEnd = createCtrlSignalsLocalVarBlock(bblk,
				nullptr, func, context, &currCtrlSigs);
			
			BasicBlock *notIdleBblkEnd = createSpinLockOnIdleBitBloc(nextBblk, 
				func, context, &currCtrlSigs, globVar, &idleOff);

			BasicBlock *idleBblkEnd = createWriteToHLSBlock(nextBblk, func,
				context, &currCtrlSigs, globVar, 0, BRAM_ADDRESS);

			addTerminatorsToBlocks(initBblkEnd, notIdleBblkEnd, idleBblkEnd,
				nextBblk, &idleOff);
  		}	
	}
	
	return llvm::PreservedAnalyses::all();
}


/**
 * Builds a Block for creating a local variable to hold the control signals.
 * @param thisBblk If thisBblk is not null, we add instructions here
 * @param nextBblk We insert our new block before that block
 * @param func The function upon we build the block
 * @param context The context of current Module
 * @param currCtrlSigs a local variable to be used by multiple blocks
 * @return The block created or thisBBlk if not null
 */
BasicBlock * createCtrlSignalsLocalVarBlock(BasicBlock *thisBblk,
	BasicBlock *nextBblk, Function *func, LLVMContext& context, 
	AllocaInst** currCtrlSigs)
{
	if(!thisBblk)
		thisBblk = BasicBlock::Create(context, "", func, nextBblk);

	IRBuilder<> builder(thisBblk, thisBblk->begin());

	// Create a 32bit Memory Allocation inst to later store the control
	// signals from the HLS IP
	*currCtrlSigs = builder.CreateAlloca(IntegerType::getInt32Ty(context));

	//builder.CreateBr(nextBblk);
	return thisBblk;
}


/**
 * Builds a Block that spins on the idle bit of the control signals of the HLS
 * IP. Must keep spinning until the IP is idle.
 * @param nextBblk We insert our new block before that block
 * @param func The function upon we build the block
 * @param context The context of current Module
 * @param currCtrlSigs a local variable to be used by multiple blocks
 * @param globVar The global variable that contains the HLS virtual address
 * @return The block created
 */
BasicBlock *createSpinLockOnIdleBitBlock(BasicBlock *nextBblk, Function *func,
	LLVMContext& context, AllocaInst **currCtrlSigs, GlobalVariable *globVar,
	Value** idleOff)
{
	BasicBlock *bblk = BasicBlock::Create(context, "", func, nextBblk);
	IRBuilder<> builder(bblk, bblk->begin());

	// Create a load inst to read the address of the global pointer variable
	LoadInst *hlsAddrCtrl = builder.CreateLoad(globVar->getValueType(), 
		globVar);
	
	// Create a getelemntptr inst to get the adress of the 0th index of the
	// address
	Value* hlsAddr0 = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(context), hlsAddrCtrl, 0);
	
	// Create a Load inst to load the number in index 0 from the hls address
	// to the Control signals
	LoadInst *hlsAddr0val = builder.CreateAlignedLoad(
		IntegerType::getInt32Ty(context), hlsAddr0, MaybeAlign(4));
	
	// Create a Store inst to store the number to currCtrlSigs
	builder.CreateAlignedStore(hlsAddr0val, *currCtrlSigs, MaybeAlign(4));
	
	// Create an And Op to isolate the idle bit from the ctrl signals
	Value* idleBit = builder.CreateAnd(hlsAddr0val, 4);
	
	// Create a Compare Not Equal inst to check if the Idle Bit is not 0.
	// A true result means the bit is idle so we need to escape the loop
	Value* idleOn = builder.CreateICmpNE(idleBit,
		ConstantInt::get(context, APInt(32, 0)));

	// Create an XOR inst to compare idleOn with 'true' to reverse the
	// boolean value of previous compare and assign result to idleOff 
	*idleOff = builder.CreateXor(idleOn, ConstantInt::getTrue(context));

	return bblk;
}


/**
 * Builds a Block that writes to the HLS IP the BRAM address and a function id.
 * If id is positive, it means we just called a function with the corresponding
 * id. If it is 0, we just returned from a function.
 * @param nextBblk We insert our new block before that block
 * @param func The function upon we build the block
 * @param context The context of current Module
 * @param currCtrlSigs a local variable to be used by multiple blocks
 * @param globVar The global variable that contains the HLS virtual address
 * @param id The function id (positive) or 0
 * @param bramAddr The BRAM address
 * @return The block created
 */
BasicBlock *createWriteToHLSBlock(BasicBlock *nextBblk, Function *func,
	LLVMContext& context, AllocaInst** currCtrlSigs, GlobalVariable *globVar,
	int id, int bramAddr)
{
	BasicBlock *bblk = BasicBlock::Create(context, "", func, nextBblk);
	IRBuilder<> builder(bblk, bblk->begin());

	// Create a load inst to read the address of the global pointer variable
	LoadInst *hlsAddrFuncID = builder.CreateLoad(
		globVar->getValueType(), globVar);

	// Create a getelemntptr inst to get the adress of the 4th index of the
	// address
	Value* hlsAddr4 = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(context), hlsAddrFuncID, 4);

	// Create a Store inst to store the function ID to the 4th index from
	// the HLS base address which is the id parameter
	builder.CreateStore(ConstantInt::get(context, llvm::APInt(32, id)),
		hlsAddr4);

	//Create a load inst to read the address of the global pointer variable
	LoadInst *hlsAddrBramAddr = builder.CreateLoad(
		globVar->getValueType(), globVar);

	// Create a getelemntptr inst to get the adress of the 6th index of the
	// address
	Value* hlsAddr6 = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(context), hlsAddrBramAddr, 6);

	// Create a Store inst to store the bram address to the 6th index from
	// the HLS base address which is the bram parameter
	builder.CreateStore(
		ConstantInt::get(context, llvm::APInt(32, bramAddr)), hlsAddr6);

	// Create a Load inst to load the control signals previously stored in
	// currCtrlSigs variable
	LoadInst *currSigs = builder.CreateAlignedLoad(
		IntegerType::getInt32Ty(context), *currCtrlSigs, MaybeAlign(4));

	// Create an OR inst to set the the LS bit of currSigs
	Value * startBitSet = builder.CreateOr(currSigs,
		ConstantInt::get(context, APInt(32, 1)));

	// Create a load inst to read the address of the global pointer variable
	LoadInst *hlsAddrCtrl = builder.CreateLoad(
		globVar->getValueType(), globVar);
	
	// Create a getelemntptr inst to get the adress of the 0th index of the
	// address
	Value* hlsAddr0 = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(context), hlsAddrCtrl, 0);

	// Create a Store inst to store the number to currCtrlSigs
	builder.CreateAlignedStore(startBitSet, hlsAddr0, MaybeAlign(4));

	return bblk;
}

/**
 * Adds terminators to the basic blocks we created if required.
 * @param initBblk The block that creates the local variable
 * @param notIdleBblk The block that spins on the idle bit
 * @param idleBblk The block that writes to HLS
 * @param firstBblk The block that was initially the first of its function. Now
 * dropped of its rank, deprived and shamefull it is simply used as a target for
 * the idleBblk to branch to
 * @param iddleOff The Outcome of the condition calculated in the notIdleBblk.
 * Need to add this to a conditional branch here
 */
void addTerminatorsToBlocks(BasicBlock *initBblk, BasicBlock *notIdleBblk,
	BasicBlock *idleBblk, BasicBlock *firstBblk, Value** idleOff)
{
	IRBuilder<> initBblkBuilder(initBblk, initBblk->end());
	Instruction* termInst = initBblk->getTerminator();
	if(termInst)
	{
		BranchInst* BrI = dyn_cast<BranchInst>(termInst);
		BrI->setSuccessor(0, notIdleBblk);
	}
	else
		initBblkBuilder.CreateBr(notIdleBblk);
	
	IRBuilder<> notIdleBblkBuilder(notIdleBblk, notIdleBblk->end());
	notIdleBblkBuilder.CreateCondBr(*idleOff, notIdleBblk, idleBblk);

	IRBuilder<> idleBblkBuilder(idleBblk, idleBblk->end());
	idleBblkBuilder.CreateBr(firstBblk);
}

/**
 * Iterates over a function's basic blocks searching for call instructions which
 * call functions that match the name of the functions we want to instrument.
 * Finally it returns a vector with those instructions.
 * @param funcNames A vector containing the function names of which the call
 * instructions must match with
 * @param func The function of which we iterate over the basic blocks and
 * instructions
 * @return The vector of the maching call instructions
 */
std::vector<Instruction*> getCallInsts(std::vector<std::string>& funcNames,
	Function* func)
{
	std::vector<Instruction*> callInsts;

	for (Function::iterator bblk = func->begin(); bblk != func->end(); ++bblk)
		{
		for (BasicBlock::iterator instr = bblk->begin(); instr != bblk->end();
			++instr)
		{
			CallInst* callInst = dyn_cast<CallInst>(&*instr);
			if (callInst)
			{
				std::vector<std::string>::iterator funcNamesIt;
				std::string calledFuncName =
					callInst->getCalledFunction()->getName().str();
				//search if called function name exists in vector of names
				funcNamesIt = std::find(funcNames.begin(), funcNames.end(),
					calledFuncName);
				//If it does, push the instruction
				if(funcNamesIt != funcNames.end())	
					callInsts.push_back(&*instr);
			}
		}
	}
	return callInsts;
}


/**
 * Evaluates whether a basic block is complete or. We consider as complete, a
 * block that contains only the call instruction and a terminator.
 * @param bblk The basic block to evaluate
 * @return True if block is complete, false otherwsie
 */
// bool isBlockComplete(BasicBlock* bblk)
// {
// 	bool isCallInstFirst = false;
// 	bool hasTerminator = false;

// 	Instruction* callInst = bblk->getFirstNonPHI();
// 	if(dyn_cast<CallInst>(callInst))
// 		isCallInstFirst = true;

// 	if(bblk->getTerminator())
// 		hasTerminator = true;

// 	if(isCallInstFirst && hasTerminator && (bblk->size() == 2))
// 		return true;
// 	return false;
// }

/**
 * Gives the next Basic Block of the given Basic Block in code if it exists.
 * @param bblk The basic block of which we want to return the next
 * @return The next basic block of bblk code-wise. Null otherwise.
 */
// BasicBlock *getNextBlockofBlock(BasicBlock *bblk)
// {
// 	Function* func = bblk->getParent();

// 	for (Function::iterator ibblk = func->begin(); ibblk != func->end(); 
// 		++ibblk)
// 	{
// 		if(&*ibblk == bblk)
// 		{
// 			++ibblk;
// 			if(ibblk != func->end())
// 				return &*ibblk;
// 		}
// 	}
// 	return nullptr;
// }

//-----------------------------------------------------------------------------
// New PM Registration - use this for loading the pass on a c file with clang
//-----------------------------------------------------------------------------
//llvm::PassPluginLibraryInfo getInjectBRAMModPluginInfo()
//{
	////std::cerr << "getInjectBRAMModPluginInfo() called\n"; // add this line
	//return
	//{
		//LLVM_PLUGIN_API_VERSION, "InjectBRAMMod", LLVM_VERSION_STRING,
		//[](PassBuilder &PB)
		//{
			//PB.registerOptimizerEarlyEPCallback
			//(
				//[](ModulePassManager &MPM, OptimizationLevel)
				//{
					//MPM.addPass(InjectBRAMMod());
					//return true;
				//}
			//);
		//}
	//};
//}


//-----------------------------------------------------------------------------
// New PM Registration - use this for loading the pass on an ll file with opt
//-----------------------------------------------------------------------------
 llvm::PassPluginLibraryInfo getInjectBRAMModPluginInfo()
 {
	 //std::cerr << "getInjectBRAMModPluginInfo() called\n"; // add this line
	 return
	 {
		 LLVM_PLUGIN_API_VERSION, "InjectBRAMMod", LLVM_VERSION_STRING,
		 [](PassBuilder &PB)
		 {
			 PB.registerPipelineParsingCallback
			 (
				 [](StringRef Name, ModulePassManager &MPM,
				 ArrayRef<PassBuilder::PipelineElement>)
				 {
					 if (Name == "inject-bram")
					 {
						 MPM.addPass(InjectBRAMMod());
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
	return getInjectBRAMModPluginInfo();
}