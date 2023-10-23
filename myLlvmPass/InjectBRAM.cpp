#include "InjectBRAM.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <iostream> //TODO: for debugging. Remove later
#include <fstream>	//to read files with function names
#include <string>

using namespace llvm;

/**@{*/
/** Uncomment to make the pass insert prints on important information */
// #define DEBUG_PRINT

/** Uncomment to make enable debug prints during compilation */
// #define DEBUG_COMP_PRINT

/** Uncomment to make the pass inject instructions for HLS IP communication */
// #define USE_HLS
/**@}*/

#ifdef DEBUG_COMP_PRINT
	#define debug_errs(str) do { errs() << str << "\n"; } while(0)
#else
	#define debug_errs(str)
#endif

/**@{*/
/** The physical address of the BRAM where the call graph is stored */
#define BRAM_ADDRESS 0xa0010000
/** The virtual address corresponding to the HLS IP. Mapped by the app */
// #define HLS_VIRT_ADDR 0x7000000 ///< virtual address for our test app
#define HLS_VIRT_ADDR 0x10013000 ///< virtual address for ipc benchmark for sel4bench
/**@}*/

/**@{*/
/** Events supported by a53 core used in ZCU102 MPSoC */
#define EVENT_SOFTWARE_INCREMENT 0x00
#define EVENT_CACHE_L1I_MISS 0x01
#define EVENT_TLB_L1I_MISS 0x02
#define EVENT_CACHE_L1D_MISS 0x03
#define EVENT_CACHE_L1D_HIT 0x04
#define EVENT_TLB_L1D_MISS 0x05
#define EVENT_MEMORY_READ 0x06
#define EVENT_MEMORY_WRITE 0x07
#define EVENT_EXECUTE_INSTRUCTION 0x08
#define EVENT_EXCEPTION 0x09
#define EVENT_EXCEPTION_RETURN 0x0A
#define EVENT_CONTEXTIDR_WRITE 0x0B
#define EVENT_SOFTWARE_PC_CHANGE 0x0C
#define EVENT_EXECUTE_BRANCH_IMM 0x0D
#define EVENT_FUNCTION_RETURN 0x0E
#define EVENT_MEMORY_ACCESS_UNALIGNED 0x0F
#define EVENT_BRANCH_MISPREDICT 0x10
#define EVENT_CCNT 0x11
#define EVENT_EXECUTE_BRANCH_PREDICTABLE 0x12
#define EVENT_MEMORY_ACCESS 0x13
#define EVENT_L1I_CACHE 0x14
#define EVENT_L1D_CACHE_WB 0x15
#define EVENT_L2D_CACHE 0x16
#define EVENT_L2D_CACHE_REFILL 0x17
#define EVENT_L2D_CACHE_WB 0x18
#define EVENT_BUS_ACCESS 0x19
#define EVENT_MEMORY_ERROR 0x1A
#define EVENT_INST_SPEC 0x1B
#define EVENT_TTBR_WRITE_RETIRED 0x1C
#define EVENT_BUS_CYCLES 0x1D
#define EVENT_CHAIN 0x1E
#define EVENT_L1D_CACHE_ALLOCATE 0x1F
/**@}*/

/**@{*/
/** Configure which events to be used here */
#define EVENT_0 EVENT_CACHE_L1D_MISS
#define EVENT_1 EVENT_CACHE_L1D_HIT
#define EVENT_2 EVENT_TLB_L1D_MISS
#define EVENT_3 EVENT_MEMORY_READ
#define EVENT_4 EVENT_MEMORY_WRITE
#define EVENT_5 EVENT_BRANCH_MISPREDICT
/**@}*/

#define EVENTS_NUM 7

#define CPU_CYCLES_INDEX 0
#define EVENT0_INDEX 1
#define EVENT1_INDEX 2
#define EVENT2_INDEX 3
#define EVENT3_INDEX 4
#define EVENT4_INDEX 5
#define EVENT5_INDEX 6
#define CTRL_SIGS_INDEX 7

BasicBlock *createLocalVarsBlock(BasicBlock *nextBblk, Function *func,
	LLVMContext &context);
BasicBlock *createSpinLockOnIdleBitBlock(BasicBlock *nextBblk, Function *func,
	LLVMContext &context, Value **idleOff);
BasicBlock *writeFuncStartToHlsBlock(BasicBlock *nextBblk, Function *func,
	LLVMContext &context, int id);
BasicBlock *writeFuncEndToHlsBlock(BasicBlock *nextBblk, Function *func,
	LLVMContext &context);
void readCountersBeforeCallBlock(BasicBlock *thisBblk, LLVMContext &context);
void writeGlobalsBeforeReturnBlock(BasicBlock *thisBblk, LLVMContext &context);
void addTerminatorsToBlocks(BasicBlock *initBblk, BasicBlock *notIdleBblk,
	BasicBlock *idleBblk, BasicBlock *firstBblk, Value **idleOff);
std::vector<Instruction *> getCallInsts(std::vector<std::string> &funcNames,
	Function *func);
std::vector<Instruction *> getReturnInsts(Function *func);
void makeExternalDeclarations(Module &M, LLVMContext &context);
#ifdef DEBUG_PRINT
void insertPrint(IRBuilder<> &builder, std::string msg, Value *val, Module &M);
#endif
// bool isBlockComplete(BasicBlock* bblk);
// BasicBlock *getNextBlockofBlock(BasicBlock *bblk);

int eventIDs[6];

GlobalVariable *hlsAddrVar;

/** 64bit cpu cycles, 64bit event 0-5 */
GlobalVariable *globalEventVars[EVENTS_NUM];

/** 32bit ctrl sigs, 32bit nodedata ls/ms, 64bit cpu cycles, 64bit event 0-5 */
AllocaInst *localEventVars[10];

enum declareFuncs
{
#ifndef USE_HLS
	attesterTopFunc,
	attesterPrintFunc,
#endif
	sel4BenchInitFunc,
	sel4BenchRstCounts,	
	sel4BenchSetCountEventFunc,
	sel4BenchStartCountsFunc,
	sel4BenchPrivReadPmcrFunc,
	sel4BenchPrivWritePmcrFunc,
	sel4BenchGetCycleCountFunc,
	sel4BenchGetCounterFunc,
	declareFuncsNum
};

Function *declareFuncs[declareFuncsNum] = {nullptr};

PreservedAnalyses InjectBRAMMod::run(Module &M, ModuleAnalysisManager &AM)
{
	std::string line;
	std::ifstream myfile("/home/mskordal/workspace/myRepos/my-sel4-projects/myLlvmPass/functions.txt");
	std::vector<std::string> funcNames;
	std::vector<int> funcIds;

#ifdef DEBUG_PRINT
	errs() << "DEBUG_PRINT define is enabled. This will enable in-app prints\n";
#endif
#ifdef USE_HLS
	errs() << "USE_HLS define is enabled. The pass will rd/wr to HLS IP\n";
#else
	errs() << 	"USE_HLS define is disabled. The pass will rd/wr to the"
				"software implementation of the HLS IP\n";
#endif	

	while (getline(myfile, line))
	{
		int delim_pos = line.find(" ");
		funcNames.push_back(line.substr(0, delim_pos));
		funcIds.push_back(
			stoi(line.substr(delim_pos + 1, line.length() - delim_pos + 1)));
	}

	debug_errs("will now print functions got from functions.txt\n");
	for (int i = 0; i < funcNames.size(); i++)
	{
		debug_errs(funcNames.at(i) << ": " << funcIds.at(i));
	}
	eventIDs[0] = EVENT_0;
	eventIDs[1] = EVENT_1;
	eventIDs[2] = EVENT_2;
	eventIDs[3] = EVENT_3;
	eventIDs[4] = EVENT_4;
	eventIDs[5] = EVENT_5;

	// Get the context of the module. Probably the Global Context.
	LLVMContext &context = M.getContext();
	
	/**
	 * Create the external declarations of functions.
	 */
	makeExternalDeclarations(M, context);
	
	Type *Int64PtrTy = Type::getInt64PtrTy(context);

	// Create a global ptr var with external linkage and a null initializer.
	hlsAddrVar = new GlobalVariable(M, Int64PtrTy, false,
		GlobalValue::ExternalLinkage, nullptr, "bramptr");
	// Create a constant 64-bit integer value.
	// APInt is an object that represents an integer of specified bitwidth
	ConstantInt *Int64Val = ConstantInt::get(context, APInt(64, HLS_VIRT_ADDR));
	// Convert the integer value to a pointer. Basically a typecast
	Constant *PtrVal = ConstantExpr::getIntToPtr(Int64Val, Int64PtrTy);
	// Align global variable to 8 bytes
	hlsAddrVar->setAlignment(Align(8));
	// Set the initial value of the global variable. Overwrites if existing
	hlsAddrVar->setInitializer(PtrVal);

	// Create the global variables for the cpu cycles and the 6 events
	for (int i = 0; i < EVENTS_NUM; i++)
	{
		globalEventVars[i] = new GlobalVariable(M, Type::getInt64Ty(context),
			false, GlobalValue::ExternalLinkage, nullptr,
			"event" + std::to_string(i));
		globalEventVars[i]->setDSOLocal(true);
		globalEventVars[i]->setAlignment(Align(8));
		globalEventVars[i]->setInitializer(
			ConstantInt::get(context, APInt(64, 0)));
	}

	Function *mainFunc = M.getFunction("main");

	BasicBlock *mainFirstBblk = &mainFunc->getEntryBlock();

	IRBuilder<> mainBuilder(mainFirstBblk, mainFirstBblk->begin());

#ifdef DEBUG_PRINT
	insertPrint(mainBuilder, "HLS virt address: ", Int64Val,
		*mainFirstBblk->getModule());
#endif

	mainBuilder.CreateCall(declareFuncs[sel4BenchInitFunc]);

	// Create 6 call instructions to sel4bench_set_count_event
	for (int i = 0; i < 6; ++i)
	{
		Value *s4bsceArgs[2] =
		{
			ConstantInt::get(context, APInt(64, i)),
			ConstantInt::get(context, APInt(64, eventIDs[i]))
		};
		mainBuilder.CreateCall(declareFuncs[sel4BenchSetCountEventFunc],
			s4bsceArgs);
	}
	debug_errs("passed main prologue instrumentation");

	for (int i = 0; i < funcNames.size(); ++i)
	{
		Value *idleOff;

		// get function the function
		Function *func = M.getFunction(funcNames.at(i));

		BasicBlock *firstBblk = &func->getEntryBlock();

		BasicBlock *initBblk = createLocalVarsBlock(firstBblk, func, context);

		BasicBlock *notIdleBblk = createSpinLockOnIdleBitBlock(firstBblk, func,
			context, &idleOff);

		BasicBlock *idleBblk = writeFuncStartToHlsBlock(firstBblk, func,
			context, funcIds.at(i));
		IRBuilder<> idleBblkBuilder(idleBblk, idleBblk->end());
#ifdef DEBUG_PRINT
		insertPrint(idleBblkBuilder, "finished prologue of function: " +
			funcNames.at(i) , nullptr, *idleBblk->getModule());
#endif
		addTerminatorsToBlocks(initBblk, notIdleBblk, idleBblk, firstBblk,
			&idleOff);

		std::vector<Instruction *> callInsts = getCallInsts(funcNames, func);

		for (Instruction *instr : callInsts)
		{
			BasicBlock *bblk = instr->getParent();
			Instruction *firstInstr = bblk->getFirstNonPHI();
			if (firstInstr != instr)
				bblk = SplitBlock(bblk, instr);

			BasicBlock *nextBblk = SplitBlock(bblk,
				instr->getNextNonDebugInstruction());

			readCountersBeforeCallBlock(bblk, context);

			BasicBlock *notIdleBblkEnd = createSpinLockOnIdleBitBlock(nextBblk,
				func, context, &idleOff);

			BasicBlock *idleBblkEnd = writeFuncEndToHlsBlock(nextBblk, func,
				context);

			addTerminatorsToBlocks(bblk, notIdleBblkEnd, idleBblkEnd, nextBblk,
				&idleOff);
		}

		std::vector<Instruction *> retInsts = getReturnInsts(func);
		for (Instruction *instr : retInsts)
		{
			BasicBlock *bblk = instr->getParent();
			Instruction *firstInstr = bblk->getFirstNonPHI();
			if (firstInstr != instr)
				bblk = SplitBlock(bblk, instr);

			writeGlobalsBeforeReturnBlock(bblk, context);
		}
	}
	debug_errs("passed function instrumentation");


	// Before return, do the callinst operation for main as well
	std::vector<Instruction *> callInsts = getCallInsts(funcNames, mainFunc);
	Value *idleOff;
	// for main func we only allocate ctrl sigs local for spinning
	localEventVars[CTRL_SIGS_INDEX] = mainBuilder.CreateAlloca(
		IntegerType::getInt32Ty(context));
	for (Instruction *instr : callInsts)
	{
		BasicBlock *bblk = instr->getParent();
		Instruction *firstInstr = bblk->getFirstNonPHI();
		if (firstInstr != instr)
			bblk = SplitBlock(bblk, instr);

		BasicBlock *nextBblk = SplitBlock(bblk,
			instr->getNextNonDebugInstruction());

		BasicBlock *notIdleBblkEnd = createSpinLockOnIdleBitBlock(nextBblk,
			mainFunc, context, &idleOff);

		BasicBlock *idleBblkEnd = writeFuncEndToHlsBlock(nextBblk, mainFunc,
			context);

		addTerminatorsToBlocks(bblk, notIdleBblkEnd, idleBblkEnd, nextBblk,
			&idleOff);
	}
	debug_errs("passed call instructions instrumentation");

// If we are on software mode, print results to console
#ifndef USE_HLS
	std::vector<Instruction *> retInsts = getReturnInsts(mainFunc);
	for (Instruction *instr : retInsts)
	{
		BasicBlock *bblk = instr->getParent();
		Instruction *firstInstr = bblk->getFirstNonPHI();
		if (firstInstr != instr)
			bblk = SplitBlock(bblk, instr);
		
		IRBuilder<> endBuilder(bblk, bblk->begin());
		endBuilder.CreateCall(declareFuncs[attesterPrintFunc]);
	}
	debug_errs("passed attester print instrumentation");

#endif

	return llvm::PreservedAnalyses::all();
}

/**
 * Builds a Block for creating 10 local variables to hold the control signals,
 * node data, cpu cycles and events 0 to 5.
 * @param nextBblk We insert our new block before that block
 * @param func The function upon we build the block
 * @param context The context of current Module
 * @return The block created or thisBBlk if not null
 */
BasicBlock *createLocalVarsBlock(BasicBlock *nextBblk, Function *func,
	LLVMContext &context)
{
	debug_errs("in " << __func__  <<":");

	BasicBlock *bblk = BasicBlock::Create(context, "", func, nextBblk);

	IRBuilder<> builder(bblk, bblk->begin());

	// Create 7 64bit Mem Alloc Insts for local vars to store cpu cycles and 6
	// events for this function.
	for (int i = 0; i < EVENTS_NUM; ++i)
	{
		localEventVars[i] = builder.CreateAlloca(
			IntegerType::getInt64Ty(context));
		builder.CreateStore(
			ConstantInt::get(context, llvm::APInt(64, 0)), localEventVars[i]);
	}
	// Create 3 32bit Memo Alloc Insts for local var to hold store the control
	// signals and nodeData LS/MS for this function
	for (int i = 7; i < 10; ++i)
		localEventVars[i] = builder.CreateAlloca(
			IntegerType::getInt32Ty(context));

#ifdef DEBUG_PRINT
	insertPrint(builder, "added locals of function: " +
		func->getName().str() , nullptr, *bblk->getModule());
#endif
	builder.CreateBr(nextBblk);

	return bblk;
}

/**
 * Builds a Block that spins on the idle bit of the control signals of the HLS
 * IP. Must keep spinning until the IP is idle.
 * @param nextBblk We insert our new block before that block
 * @param func The function upon we build the block
 * @param context The context of current Module
 * @return The block created
 */
BasicBlock *createSpinLockOnIdleBitBlock(BasicBlock *nextBblk, Function *func,
	LLVMContext &context, Value **idleOff)
{
	debug_errs("in " << __func__  <<":");

	BasicBlock *bblk = BasicBlock::Create(context, "", func, nextBblk);
	IRBuilder<> builder(bblk, bblk->begin());

#ifdef USE_HLS
	// Create a load inst to read the address of the global pointer variable
	LoadInst *hlsAddrCtrl = builder.CreateLoad(hlsAddrVar->getValueType(),
		hlsAddrVar);
	// Create a getelemntptr inst to get the adress of the 0th index of the
	// address
	Value *hlsAddr0 = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(context), hlsAddrCtrl, 0);

	// Create a Load inst to load the number in index 0 from the hls address
	// to the Control signals
	LoadInst *hlsAddr0val = builder.CreateAlignedLoad(
		IntegerType::getInt32Ty(context), hlsAddr0, MaybeAlign(4));

	// Create a Store inst to store the number to currCtrlSigs
	builder.CreateStore(hlsAddr0val, localEventVars[CTRL_SIGS_INDEX]);

	// Create an And Op to isolate the idle bit from the ctrl signals
	Value *idleBit = builder.CreateAnd(hlsAddr0val, 4);

	// Create a Compare Not Equal inst to check if the Idle Bit is not 0.
	// A true result means the bit is idle so we need to escape the loop
	Value *idleOn = builder.CreateICmpNE(idleBit,
		ConstantInt::get(context, APInt(32, 0)));

	// Create an XOR inst to compare idleOn with 'true' to reverse the
	// boolean value of previous compare and assign result to idleOff
	*idleOff = builder.CreateXor(idleOn, ConstantInt::getTrue(context));
#else //This case works for both the software case or if not using any case
	// We want idleOff to be false so that we never loop when not using HLS
	*idleOff = builder.CreateXor(ConstantInt::getTrue(context),
		ConstantInt::getTrue(context));
#endif
#ifdef DEBUG_PRINT
	insertPrint(builder, "made spining block in: " +
		func->getName().str() , nullptr, *bblk->getModule());
#endif
	// builder.CreateBr(nextBblk);

	return bblk;
}

/**
 * Builds a Block that writes to the HLS IP the BRAM address and a function id.
 * If id is positive, it means we just called a function with the corresponding
 * id. If it is 0, we just returned from a function.
 * @param nextBblk We insert our new block before that block
 * @param func The function upon we build the block
 * @param context The context of current Module
 * @param globVar The global variable that contains the HLS virtual address
 * @param id The function id (positive) or 0
 * @return The block created
 */
BasicBlock *writeFuncStartToHlsBlock(BasicBlock *nextBblk, Function *func,
	LLVMContext &context, int id)
{
	debug_errs("in " << __func__  <<":");

	int nodeDataLS;
	int nodeDataMS;

	BasicBlock *bblk = BasicBlock::Create(context, "", func, nextBblk);
	IRBuilder<> builder(bblk, bblk->begin());

#ifdef DEBUG_PRINT
	insertPrint(builder, "writeFuncStartToHlsBlock: " + func->getName().str(),
		nullptr, *bblk->getModule());
#endif

#ifdef USE_HLS
	// Create a load inst to read the address of the global pointer variable
	LoadInst *nodeDataLsAddr = builder.CreateLoad(
		hlsAddrVar->getValueType(), hlsAddrVar);

	// Create a getelemntptr inst to get the adress of the 4th index of the
	// address
	Value *hlsAddr4 = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(context), nodeDataLsAddr, 4);
#endif

	nodeDataLS = id;
	nodeDataLS |= (eventIDs[0] << 8);
	nodeDataLS |= (eventIDs[1] << 16);
	nodeDataLS |= (eventIDs[2] << 24);

	// Create a Store inst to store the function ID to the 4th index from
	// the HLS base address which is the id parameter
	ConstantInt *nodatDataLsInt =
		ConstantInt::get(context, llvm::APInt(32, nodeDataLS));

#ifdef DEBUG_PRINT
	insertPrint(builder, "\tnodeDataLS: ", nodatDataLsInt, *bblk->getModule());
#endif

#ifdef USE_HLS
	builder.CreateStore(nodatDataLsInt, hlsAddr4);
	// Create a load inst to read the address of the global pointer variable
	LoadInst *nodeDataMsAddr = builder.CreateLoad(
		hlsAddrVar->getValueType(), hlsAddrVar);

	// Create a getelemntptr inst to get the adress of the 6th index of the
	// address
	Value *hlsAddr6 = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(context), nodeDataMsAddr, 6);
#endif

	nodeDataMS |= (eventIDs[3] << 8);
	nodeDataMS |= (eventIDs[4] << 16);
	nodeDataMS |= (eventIDs[5] << 24);

	// Create a Store inst to store the bram address to the 6th index from
	// the HLS base address which is the bram parameter
	ConstantInt *nodatDataMsInt =
		ConstantInt::get(context, llvm::APInt(32, nodeDataMS));

#ifdef DEBUG_PRINT
	// insertPrint(builder, "\tnodeDataMS: ", nodatDataMsInt, *bblk->getModule());
#endif

#ifdef USE_HLS
	builder.CreateStore(nodatDataMsInt, hlsAddr6);

	/**
	 * Write the bram address to the corresponding HLS address. This is done
	 * every time we create this block
	 */

	// Create a load inst to read the address of the global pointer variable
	LoadInst *hlsAddrBramAddr = builder.CreateLoad(
		hlsAddrVar->getValueType(), hlsAddrVar);

	// Create a getelemntptr inst to get the adress of the 20th index of the
	// address
	Value *hlsAddr36 = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(context), hlsAddrBramAddr, 36);
#ifdef DEBUG_PRINT
	insertPrint(builder, "\tWill write to: ", hlsAddr36, *bblk->getModule());
#endif
#endif

#ifdef USE_HLS
	// Create a Store inst to store the bram address to the 6th index from
	// the HLS base address which is the bram parameter
	builder.CreateStore(
		ConstantInt::get(context, llvm::APInt(32, BRAM_ADDRESS)), hlsAddr36);
	/**
	 * Write value 0x11 to the control signals. This will fire-off the HLS
	 * function with the values currently written. This is done every time we
	 * create this block
	 */

	// Create a Load inst to load the local control signals value
	LoadInst *currSigs = builder.CreateLoad(
		localEventVars[CTRL_SIGS_INDEX]->getAllocatedType(),
		localEventVars[CTRL_SIGS_INDEX]);

	// Create an OR inst to set the ap_start(bit 0) and ap_continue(bit 4)
	Value *startBitSet = builder.CreateOr(currSigs,
		ConstantInt::get(context, APInt(32, 0x11)));

	// Create a load inst to read the address of the global pointer variable
	LoadInst *hlsAddrCtrl = builder.CreateLoad(
		hlsAddrVar->getValueType(), hlsAddrVar);

	// Create a getelemntptr inst to get the HLS adress at the 0th index
	Value *hlsAddr0 = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(context), hlsAddrCtrl, 0);

	// Create a Store inst to store the set signals to the corresp. HLS address
	builder.CreateStore(startBitSet, hlsAddr0);
#endif

#ifndef USE_HLS
	Value *atfArgs[16] =
	{
		nodatDataLsInt,
		nodatDataMsInt,
		ConstantPointerNull::get(Type::getInt64PtrTy(context)),
		ConstantPointerNull::get(Type::getInt64PtrTy(context)),
		ConstantPointerNull::get(Type::getInt64PtrTy(context)),
		ConstantPointerNull::get(Type::getInt64PtrTy(context)),
		ConstantPointerNull::get(Type::getInt64PtrTy(context)),
		ConstantPointerNull::get(Type::getInt64PtrTy(context)),
		ConstantPointerNull::get(Type::getInt64PtrTy(context)),
		ConstantPointerNull::get(Type::getInt64PtrTy(context)),
		ConstantPointerNull::get(Type::getInt64PtrTy(context)),
		ConstantPointerNull::get(Type::getInt64PtrTy(context)),
		ConstantPointerNull::get(Type::getInt64PtrTy(context)),
		ConstantPointerNull::get(Type::getInt64PtrTy(context)),
		ConstantPointerNull::get(Type::getInt64PtrTy(context)),
		ConstantPointerNull::get(Type::getInt64PtrTy(context))
	};
	builder.CreateCall(declareFuncs[attesterTopFunc], atfArgs);
	debug_errs("inserted call to attester-top");
#endif

	// Create a call inst to sel4bench_reset_counters
	builder.CreateCall(declareFuncs[sel4BenchRstCounts]);

	// Create a call inst to sel4bench_start_counters/ 0x3F = 0b111111
	Value *s4bsceArg[1] = {ConstantInt::get(context, APInt(64, 0x3F))};
	builder.CreateCall(declareFuncs[sel4BenchStartCountsFunc], s4bsceArg);

	CallInst *readPmcr = builder.CreateCall(
		declareFuncs[sel4BenchPrivReadPmcrFunc]);

	// Create an OR inst to set the ap_start(bit 0) and ap_continue(bit 4)
	Value *cycleCounter = builder.CreateOr(readPmcr,
		ConstantInt::get(context, APInt(32, 0x4)));

	// Create a call inst to sel4bench_private_write_pmcr
	Value *s4bpwpArg[1] = {cycleCounter};
	builder.CreateCall(declareFuncs[sel4BenchPrivWritePmcrFunc],
		s4bpwpArg);

	return bblk;
}

/**
 * Builds a Block that writes to the HLS IP cpu cycles and the 6 events counted
 * of a function that just returned. We write 0 to function id parameter to mark
 * the end of the function.
 * @param nextBblk We insert our new block before that block
 * @param func The function upon we build the block
 * @param context The context of current Module
 * @return The block created
 */
BasicBlock *writeFuncEndToHlsBlock(BasicBlock *nextBblk, Function *func,
	LLVMContext &context)
{
	Value *atfArgs[16];
	BasicBlock *bblk = BasicBlock::Create(context, "", func, nextBblk);
	IRBuilder<> builder(bblk, bblk->begin());

#ifdef DEBUG_PRINT
	// insertPrint(builder, "writeFuncEndToHlsBlock: ", nullptr, *bblk->getModule());
#endif

#ifdef USE_HLS
	// Create a load inst to read the address of the global pointer variable
	LoadInst *nodeDataLsAddr = builder.CreateLoad(
		hlsAddrVar->getValueType(), hlsAddrVar);

	// Create a getelemntptr inst to get the adress of the 4th index of the
	// address
	Value *hlsAddr4 = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(context), nodeDataLsAddr, 4);
#ifdef DEBUG_PRINT
	insertPrint(builder, "\tWill write 0 to: ", hlsAddr4, *bblk->getModule());
#endif
#endif

#ifdef USE_HLS
	// Create a Store inst to store the function ID to the 4th index from
	// the HLS base address which is the id parameter
	builder.CreateStore(ConstantInt::get(context, llvm::APInt(32, 0)),
						hlsAddr4);
#endif

#ifndef USE_HLS
	atfArgs[0] = ConstantInt::get(context, llvm::APInt(32, 0));
	atfArgs[1] = ConstantInt::get(context, llvm::APInt(32, 0));
#endif

	// This for loop gets the values from the performance counters, splits their
	// 64bit values to 32bits, and if we use the Hls, the values are written to
	// the HLS address, otherwise, they are added as parameters to an array for
	// calling the software implementation of HLS.
	Value *eventVal32;
	for (int currHalf = 0; currHalf < 4; currHalf += 2)
	{
		for (int currEvent = 0; currEvent < EVENTS_NUM * 4; currEvent += 4)
		{
			// Create Load Inst to load current counted event val from global
			LoadInst *wholeEventVal = builder.CreateAlignedLoad(
				IntegerType::getInt64Ty(context), globalEventVars[currEvent/4],
				MaybeAlign(8));

#ifdef DEBUG_PRINT
			insertPrint(builder, "\tEvent : ",
				ConstantInt::get(context, llvm::APInt(32, currEvent/4)),
				*bblk->getModule());
			insertPrint(builder, "\t\twhole val: ", wholeEventVal, 
				*bblk->getModule());
#endif

			// HLS params are 32bit, so we break each event in half
			Value *halfEventVal;
			if (currHalf == 0)
				// Create an And Op to isolate the 32 LS bits of event counted
				halfEventVal = builder.CreateAnd(wholeEventVal,
					0x00000000ffffffff);
			else
			{
				// Create an And Op to isolate the 32 MS bits of event counted
				Value *EventMaskVal = builder.CreateAnd(wholeEventVal,
					0xffffffff00000000);
				// Create a logical shift right to Shift the MS value to 32 LS
				halfEventVal = builder.CreateLShr(EventMaskVal, 32);
			}
			// Create a truncate Inst to convert 64bit value to 32bit
			eventVal32 = builder.CreateTrunc(halfEventVal,
				IntegerType::getInt32Ty(context));

#ifdef DEBUG_PRINT
			insertPrint(builder, "\t\thalf val: ", eventVal32,
				*bblk->getModule());
#endif
		
#ifdef USE_HLS
			// Create a load inst to read the address of the global pointer
			// variable
			LoadInst *hlsAddr = builder.CreateLoad(
				hlsAddrVar->getValueType(), hlsAddrVar);

			// Create a getelemntptr inst to get the adress of the index
			// required to write specific event
			Value *hlsAddrAtIndex = builder.CreateConstInBoundsGEP1_64(
				IntegerType::getInt32Ty(context), hlsAddr,
				8 + currEvent + currHalf);
#ifdef DEBUG_PRINT
			insertPrint(builder, "\t\twill write to: ", hlsAddrAtIndex,
				*bblk->getModule());
#endif
#endif
#ifdef USE_HLS
			// Create a Store inst to store the value of current event to the
			// corresponding index of the HLS base address
			builder.CreateStore(eventVal32, hlsAddrAtIndex);
#endif
#ifndef USE_HLS
			atfArgs[2 + currEvent/2 + currHalf/2] = eventVal32;
#endif
		}
	}

	/**
	 * Write the bram address to the corresponding HLS address.
	 */
#ifdef USE_HLS
	// Create a load inst to read the address of the global pointer variable
	LoadInst *hlsAddrBramAddr = builder.CreateLoad(
		hlsAddrVar->getValueType(), hlsAddrVar);

	// Create a getelemntptr inst to get the adress of the 20th index of the
	// address
	Value *hlsAddr36 = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(context), hlsAddrBramAddr, 36);

	// Create a Store inst to store the bram address to the 6th index from
	// the HLS base address which is the bram parameter
	builder.CreateStore(
		ConstantInt::get(context, llvm::APInt(32, BRAM_ADDRESS)), hlsAddr36);

	/**
	 * Write value 0x11 to the control signals.
	 */

	// Create a Load inst to load the control signals previously stored in
	// currCtrlSigs variable
	LoadInst *currSigs = builder.CreateAlignedLoad(
		IntegerType::getInt32Ty(context), localEventVars[CTRL_SIGS_INDEX],
		MaybeAlign(4));

	// Create an OR inst to set the ap_start(bit 0) and ap_continue(bit 4)
	Value *startBitSet = builder.CreateOr(currSigs,
		ConstantInt::get(context, APInt(32, 0x11)));

	// Create a load inst to read the address of the global pointer variable
	LoadInst *hlsAddrCtrl = builder.CreateLoad(hlsAddrVar->getValueType(),
		hlsAddrVar);

	// Create a getelemntptr inst to get the adress of the 0th index of the
	// address
	Value *hlsAddr0 = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(context), hlsAddrCtrl, 0);

	// Create a Store inst to store the number to currCtrlSigs
	builder.CreateAlignedStore(startBitSet, hlsAddr0, MaybeAlign(4));
#endif

#ifndef USE_HLS
	builder.CreateCall(declareFuncs[attesterTopFunc], atfArgs);
#endif

	// MSKORDALif we are in main we do not need to add to locals or restart
	// events. Exit
	if(func->getName().str() == "main")
		return bblk;

	for (int i = 0; i < EVENTS_NUM; ++i)
	{
		// Create a load inst to read the value of the global event variable
		LoadInst *globalEventVal = builder.CreateLoad(
			globalEventVars[i]->getValueType(), globalEventVars[i]);

		// Create a load inst to read the value of the local event variable
		LoadInst *localEventVal = builder.CreateLoad(
			localEventVars[i]->getAllocatedType(), localEventVars[i]);

		// Create an Add inst to add local event value and global event value
		Value *totalEventVal = builder.CreateAdd(localEventVal, globalEventVal);

		// Create a Store inst to store sum back to local event var
		builder.CreateStore(totalEventVal, localEventVars[i]);
	}

	// Create a call inst to sel4bench_reset_counters
	builder.CreateCall(declareFuncs[sel4BenchRstCounts]);

	// Create a call inst to sel4bench_start_counters/ 0x3F = 0b111111
	Value *s4bsceArg[1] = {ConstantInt::get(context, APInt(64, 0x3F))};
	builder.CreateCall(declareFuncs[sel4BenchStartCountsFunc], s4bsceArg);

	CallInst *readPmcr = builder.CreateCall(
		declareFuncs[sel4BenchPrivReadPmcrFunc]);

	// Create an OR inst to set the ap_start(bit 0) and ap_continue(bit 4)
	Value *cycleCounter = builder.CreateOr(readPmcr,
		ConstantInt::get(context, APInt(32, 0x4)));

	// Create a call inst to sel4bench_private_write_pmcr
	Value *s4bpwpArg[1] = {cycleCounter};
	builder.CreateCall(declareFuncs[sel4BenchPrivReadPmcrFunc],
		s4bpwpArg);

	return bblk;
}

/**
 * Takes a block that contains a call instruction. Before the call it reads
 * current events and cpu cycles running and stores to local variables.
 * @param thisBblk We add instruction to this block
 * @param context The context of current Module
 */
void readCountersBeforeCallBlock(BasicBlock *thisBblk, LLVMContext &context)
{
	debug_errs("in " << __func__  <<":");

	IRBuilder<> builder(thisBblk, thisBblk->begin());

	// Create a Call Inst to sel4bench_get_cycle_count to get cpu cycles
	CallInst *cycleCount = builder.CreateCall(
		declareFuncs[sel4BenchGetCycleCountFunc]);

	// Create a load inst to read the value of the local event variable
	LoadInst *localCpuCyclesVal = builder.CreateLoad(
		localEventVars[CPU_CYCLES_INDEX]->getAllocatedType(),
		localEventVars[CPU_CYCLES_INDEX]);

	// Create an Add inst to add current local cpu cycles and  cycle count
	// got from the sel4bench_get_cycle_count function call.
	Value *totalCycles = builder.CreateAdd(localCpuCyclesVal, cycleCount);

	// Create a Store inst to store total CreateCacpu cycles back to the memory
	// allocated area.
	builder.CreateStore(totalCycles, localEventVars[CPU_CYCLES_INDEX]);

	// Do the same as above for all 6 events
	for (int i = 1; i < EVENTS_NUM; ++i)
	{
		Value *s4bgcArg[1] = {ConstantInt::get(context, APInt(64, i - 1))};
		CallInst *eventCount = builder.CreateCall(
			declareFuncs[sel4BenchGetCounterFunc], s4bgcArg);
		LoadInst *localeventCount = builder.CreateLoad(
			localEventVars[i]->getAllocatedType(), localEventVars[i]);
		Value *totalEventCount = builder.CreateAdd(localeventCount, eventCount);
		builder.CreateStore(totalEventCount, localEventVars[i]);
	}
}

/**
 * Takes a block that contains a return instruction. Before that instruction, it
 * adds each local event var and the perf counter of the same event, and stores
 * the sum to the corresponding global variable.
 * @param thisBblk We add instruction to this block
 * @param context The context of current Module
 */
void writeGlobalsBeforeReturnBlock(BasicBlock *thisBblk, LLVMContext &context)
{
	debug_errs("in " << __func__  <<":");

	IRBuilder<> builder(thisBblk, thisBblk->begin());

	// Create a Call Inst to sel4bench_get_cycle_count to get cpu cycles
	CallInst *cycleCount = builder.CreateCall(
		declareFuncs[sel4BenchGetCycleCountFunc]);

	// Create a load inst to read the value of the local event variable
	LoadInst *localCpuCyclesVal = builder.CreateLoad(
		localEventVars[CPU_CYCLES_INDEX]->getAllocatedType(),
		localEventVars[CPU_CYCLES_INDEX]);

	// Create an Add inst to add current local cpu cycles and  cycle count
	// got from the sel4bench_get_cycle_count function call.
	Value *totalCycles = builder.CreateAdd(localCpuCyclesVal, cycleCount);

	// Create a Store inst to store total cpu cycles back to the memory
	// allocated area.
	builder.CreateStore(totalCycles, globalEventVars[CPU_CYCLES_INDEX]);

	// Do the same as above for all 6 events
	for (int i = 1; i < EVENTS_NUM; ++i)
	{
		Value *s4bgcArg[1] = {ConstantInt::get(context, APInt(64, i - 1))};
		CallInst *eventCount = builder.CreateCall(
			declareFuncs[sel4BenchGetCounterFunc], s4bgcArg);
		LoadInst *localEventVal = builder.CreateLoad(
			localEventVars[i]->getAllocatedType(), localEventVars[i]);
		Value *totalEvent = builder.CreateAdd(localEventVal, eventCount);
		builder.CreateStore(totalEvent, globalEventVars[i]);
	}
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
	BasicBlock *idleBblk, BasicBlock *firstBblk, Value **idleOff)
{
	debug_errs("in " << __func__  <<":");

	IRBuilder<> initBblkBuilder(initBblk, initBblk->end());
	Instruction *termInst = initBblk->getTerminator();
	if (termInst)
	{
		BranchInst *BrI = dyn_cast<BranchInst>(termInst);
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
std::vector<Instruction *> getCallInsts(std::vector<std::string> &funcNames,
	Function *func)
{
	debug_errs("in " << __func__  <<":");

	std::vector<Instruction *> callInsts;

	for (Function::iterator bblk = func->begin(); bblk != func->end(); ++bblk)
	{
		for (BasicBlock::iterator instr = bblk->begin(); instr != bblk->end();
			++instr)
		{
			CallInst *callInst = dyn_cast<CallInst>(&*instr);
			if (callInst)
			{
				std::vector<std::string>::iterator funcNamesIt;
				Function *calledFunction = callInst->getCalledFunction();
				if(calledFunction != nullptr) //.equals(StringRef("", 0))
				{
					std::string calledFuncName =
					callInst->getCalledFunction()->getName().str();
					// search if called function name exists in vector of names
					funcNamesIt = std::find(funcNames.begin(), funcNames.end(),
						calledFuncName);
					// If it does, push the instruction
					if (funcNamesIt != funcNames.end())
						callInsts.push_back(&*instr);
				}
			}
		}
	}
	return callInsts;
}

/**
 * Iterates over a function's basic blocks searching for return instructions and
 * returns a vector with those instructions.
 * @param func The function of which we iterate over the basic blocks and
 * instructions
 * @return The vector of the maching return instructions
 */
std::vector<Instruction *> getReturnInsts(Function *func)
{
	debug_errs("in " << __func__  <<":");

	std::vector<Instruction *> retInsts;

	for (Function::iterator bblk = func->begin(); bblk != func->end(); ++bblk)
	{
		for (BasicBlock::iterator instr = bblk->begin(); instr != bblk->end();
			++instr)
		{
			ReturnInst *retInst = dyn_cast<ReturnInst>(&*instr);
			if (retInst)
			// DO NOT DELETE THIS COMMENTED CODE. KEEP FOR SEL4BENCH
			// CallInst *callInst = dyn_cast<CallInst>(&*instr);
			// if (callInst)
			// {
			// 	Function *calledFunction = callInst->getCalledFunction();
			// 	if(calledFunction != nullptr) //.equals(StringRef("", 0))
			// 	{
			// 		if(calledFunction->getName().str() == "benchmark_finished")
			// 		{
						retInsts.push_back(&*instr);
			// 		}
			// 	}
			// }
		}
	}
	return retInsts;
}

void makeExternalDeclarations(Module &M, LLVMContext &context)
{
	std::string declareFuncNames[declareFuncsNum] =
	{
#ifndef USE_HLS
		"attester_top_func",
		"attester_print",
#endif
		"sel4bench_init",
		"sel4bench_reset_counters",
		"sel4bench_set_count_event",
		"sel4bench_start_counters",
		"sel4bench_private_read_pmcr",
		"sel4bench_private_write_pmcr",
		"sel4bench_get_cycle_count",
		"sel4bench_get_counter"
	};

	// Manual external declarations start here
	std::vector<std::vector<Type*>> declareFuncParamsType
	{
#ifndef USE_HLS
		{
			Type::getInt32Ty(context), Type::getInt32Ty(context),
			Type::getInt32Ty(context), Type::getInt32Ty(context),
			Type::getInt32Ty(context), Type::getInt32Ty(context),
			Type::getInt32Ty(context), Type::getInt32Ty(context),
			Type::getInt32Ty(context), Type::getInt32Ty(context),
			Type::getInt32Ty(context), Type::getInt32Ty(context),
			Type::getInt32Ty(context), Type::getInt32Ty(context),
			Type::getInt32Ty(context), Type::getInt32Ty(context)
		},
		{},
#endif
		{},
		{},
		{Type::getInt64Ty(context)},
		{Type::getInt64Ty(context), Type::getInt64Ty(context)},
		{},
		{Type::getInt32Ty(context)},
		{},
		{Type::getInt64Ty(context)}
	};
	Type* declareFuncRetType[declareFuncsNum] =
	{
#ifndef USE_HLS
		Type::getVoidTy(context),
		Type::getVoidTy(context),
#endif
		Type::getVoidTy(context),
		Type::getVoidTy(context),
		Type::getVoidTy(context),
		Type::getVoidTy(context),
		Type::getInt32Ty(context),
		Type::getVoidTy(context),
		Type::getInt64Ty(context),
		Type::getInt64Ty(context)
	};

	for (int i = 0; i < declareFuncsNum; i++)
	{
		for (Module::iterator func = M.begin(); func != M.end(); ++func)
		{
			if(func->getName().str() == declareFuncNames[i])
			{
				declareFuncs[i] = M.getFunction(declareFuncNames[i]);
				break;
			}
		}
	}
	for (int i = 0; i < declareFuncsNum; i++)
	{
		if(!declareFuncs[i])
		{
			FunctionType* ft;
			if(!declareFuncParamsType[i].empty())
				ft = FunctionType::get(declareFuncRetType[i],
					declareFuncParamsType[i], false);
			else
				ft = FunctionType::get(declareFuncRetType[i], false);

			declareFuncs[i] = Function::Create(ft,
				Function::ExternalLinkage, declareFuncNames[i], &M);
		}
	}
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

#ifdef DEBUG_PRINT
void insertPrint(IRBuilder<> &builder, std::string msg, Value *val, Module &M)
{
	Value *formatString;

	// ConstantInt* ciVal = dyn_cast<ConstantInt>(val);
	if (val == nullptr)
	{
		formatString = builder.CreateGlobalStringPtr(msg + "\n");
		std::vector<Value *> printfArgs = {formatString};
		builder.CreateCall(M.getFunction("printf"), printfArgs);

	}
	else if (val->getType()->isIntOrPtrTy())
	{
		formatString = builder.CreateGlobalStringPtr(msg + "%lx\n");
		std::vector<Value *> printfArgs = {formatString, val};
		builder.CreateCall(M.getFunction("printf"), printfArgs);
	}
	else
	{
		formatString = builder.CreateGlobalStringPtr(msg + "NAN\n");
		std::vector<Value *> printfArgs = {formatString};
		builder.CreateCall(M.getFunction("printf"), printfArgs);
	}
}
#endif

//-----------------------------------------------------------------------------
// New PM Registration - use this for loading the pass on a c file with clang
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getInjectBRAMModPluginInfo()
{
	// std::cerr << "getInjectBRAMModPluginInfo() called\n"; // add this line
	return {
		LLVM_PLUGIN_API_VERSION, "InjectBRAMMod", LLVM_VERSION_STRING,
		[](PassBuilder &PB)
		{
			PB.registerOptimizerEarlyEPCallback(
				[](ModulePassManager &MPM, OptimizationLevel)
				{
					MPM.addPass(InjectBRAMMod());
					return true;
				});
		}};
}

// -----------------------------------------------------------------------------
// New PM Registration - use this for loading the pass on an ll file with opt
// -----------------------------------------------------------------------------
// llvm::PassPluginLibraryInfo getInjectBRAMModPluginInfo()
// {
// 	 //std::cerr << "getInjectBRAMModPluginInfo() called\n"; // add this line
// 	return
// 	{
// 		LLVM_PLUGIN_API_VERSION, "InjectBRAMMod", LLVM_VERSION_STRING,
// 		[](PassBuilder &PB)
// 		{
// 			PB.registerPipelineParsingCallback
// 			(
// 				[](StringRef Name, ModulePassManager &MPM,
// 				ArrayRef<PassBuilder::PipelineElement>)
// 				{
// 					if (Name == "inject-bram")
// 					{
// 						MPM.addPass(InjectBRAMMod());
// 						return true;
// 					}
// 					return false;
// 				}
// 			);
// 		}
// 	};
// }

// This is the core interface for pass plugins. It guarantees that 'opt' will
// be able to recognize HelloWorld when added to the pass pipeline on the
// command line, i.e. via '-passes=hello-world'
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo()
{
	return getInjectBRAMModPluginInfo();
}
