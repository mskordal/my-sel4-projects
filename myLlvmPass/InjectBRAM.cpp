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

#define EVENT_SOFTWARE_INCREMENT          0x00
#define EVENT_CACHE_L1I_MISS              0x01
#define EVENT_TLB_L1I_MISS                0x02
#define EVENT_CACHE_L1D_MISS              0x03
#define EVENT_CACHE_L1D_HIT               0x04
#define EVENT_TLB_L1D_MISS                0x05
#define EVENT_MEMORY_READ                 0x06
#define EVENT_MEMORY_WRITE                0x07
#define EVENT_EXECUTE_INSTRUCTION         0x08
#define EVENT_EXCEPTION                   0x09
#define EVENT_EXCEPTION_RETURN            0x0A
#define EVENT_CONTEXTIDR_WRITE            0x0B
#define EVENT_SOFTWARE_PC_CHANGE          0x0C
#define EVENT_EXECUTE_BRANCH_IMM          0x0D
#define EVENT_FUNCTION_RETURN             0x0E
#define EVENT_MEMORY_ACCESS_UNALIGNED     0x0F
#define EVENT_BRANCH_MISPREDICT           0x10
#define EVENT_CCNT                        0x11
#define EVENT_EXECUTE_BRANCH_PREDICTABLE  0x12
#define EVENT_MEMORY_ACCESS               0x13
#define EVENT_L1I_CACHE                   0x14
#define EVENT_L1D_CACHE_WB                0x15
#define EVENT_L2D_CACHE                   0x16
#define EVENT_L2D_CACHE_REFILL            0x17
#define EVENT_L2D_CACHE_WB                0x18
#define EVENT_BUS_ACCESS                  0x19
#define EVENT_MEMORY_ERROR                0x1A
#define EVENT_INST_SPEC                   0x1B
#define EVENT_TTBR_WRITE_RETIRED          0x1C
#define EVENT_BUS_CYCLES                  0x1D
#define EVENT_CHAIN                       0x1E
#define EVENT_L1D_CACHE_ALLOCATE          0x1F

#define EVENTS_NUM 7

#define CPU_CYCLES_INDEX	0
#define EVENT0_INDEX		1
#define EVENT1_INDEX		2
#define EVENT2_INDEX		3
#define EVENT3_INDEX		4
#define EVENT4_INDEX		5
#define EVENT5_INDEX		6
#define CTRL_SIGS_INDEX		7
#define NODEDATA_LS_INDEX	8
#define NODEDATA_MS_INDEX	9


BasicBlock *createLocalVarsBlock(BasicBlock *nextBblk, Function *func,
	LLVMContext& context);
BasicBlock *createSpinLockOnIdleBitBlock(BasicBlock *nextBblk, Function *func,
	LLVMContext& context, Value** idleOff);
BasicBlock *writeFuncStartToHlsBlock(BasicBlock *nextBblk, Function *func,
	LLVMContext& context, int id);
BasicBlock *writeFuncEndToHlsBlock(BasicBlock *nextBblk, Function *func,
	LLVMContext& context);
void readCountersBeforeCallBlock(BasicBlock *thisBblk, LLVMContext& context);
void writeGlobalsBeforeReturnBlock(BasicBlock *thisBblk, LLVMContext& context);
void addTerminatorsToBlocks(BasicBlock *initBblk, BasicBlock *notIdleBblk,
	BasicBlock *idleBblk, BasicBlock *firstBblk, Value** idleOff);
std::vector<Instruction*> getCallInsts(std::vector<std::string>& funcNames,
	Function* func);
std::vector<Instruction*> getReturnInsts(Function* func);
void insertPrint(IRBuilder<> &builder, std::string msg, Value* val, Module* m);
// bool isBlockComplete(BasicBlock* bblk);
// BasicBlock *getNextBlockofBlock(BasicBlock *bblk);

int eventIDs[6];

Module* Mp;

GlobalVariable* hlsAddrVar;

// 64bit cpu cycles, 64bit event 0-5
GlobalVariable* globalEventVars[EVENTS_NUM];

//32bit ctrl sigs, 32bit nodedata ls/ms, 64bit cpu cycles, 64bit event 0-5
AllocaInst* localEventVars[10];

PreservedAnalyses InjectBRAMMod::run(Module &M, ModuleAnalysisManager &AM)
{
	std::string line;
  	std::ifstream myfile ("../myLlvmPass/functions.txt");
	std::vector<std::string> funcNames;
	std::vector<int> funcIds;

	Mp = &M;
	for (Module::iterator func = M.begin(); func != M.end(); ++func)
    	errs() << func->getName().str() << '\n';

	while ( getline (myfile,line) )
	{
		int delim_pos = line.find(" ");
		funcNames.push_back( line.substr(0, delim_pos) );
		funcIds.push_back( stoi( line.substr( delim_pos + 1, line.length() - delim_pos + 1 ) ) );
	}

	errs() << "++ now\n";
	for(int i=0; i < funcNames.size(); i++)
	{
		errs() << funcNames.at(i) << ": " << funcIds.at(i) << '\n';
	}

	eventIDs[0] = EVENT_CACHE_L1D_MISS;
	eventIDs[1] = EVENT_CACHE_L1D_HIT;
	eventIDs[2] = EVENT_TLB_L1D_MISS;
	eventIDs[3] = EVENT_MEMORY_READ;
	eventIDs[4] = EVENT_MEMORY_WRITE;
	eventIDs[5] = EVENT_BRANCH_MISPREDICT;

	//Get the context of the module. Probably the Global Context.
	LLVMContext& context = M.getContext();
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

	mainBuilder.CreateCall(Mp->getFunction("sel4bench_init"));
	
	// Create 6 call instructions to sel4bench_set_count_event
	for(int i = 0; i < 6; ++i)
	{
		Value* s4bsceArgs[2] =
		{
			ConstantInt::get(context, APInt(64, i)),
			ConstantInt::get(context, APInt(64, eventIDs[i]))
		};
		mainBuilder.CreateCall(Mp->getFunction("sel4bench_set_count_event"),
			s4bsceArgs);
	}

	for(int i = 0; i < funcNames.size(); ++i)
	{
		Value* idleOff;

		// get function the function
		Function *func = M.getFunction(funcNames.at(i));
		
		BasicBlock *firstBblk = &func->getEntryBlock();	

		BasicBlock *initBblk = createLocalVarsBlock(firstBblk, func, context);
		
		BasicBlock *notIdleBblk = createSpinLockOnIdleBitBlock(firstBblk, func,
			context, &idleOff);

		BasicBlock *idleBblk = writeFuncStartToHlsBlock(firstBblk, func,
			context, funcIds.at(i));

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

			readCountersBeforeCallBlock(bblk, context);

			BasicBlock *notIdleBblkEnd = createSpinLockOnIdleBitBlock(nextBblk, 
				func, context, &idleOff);

			BasicBlock *idleBblkEnd = writeFuncEndToHlsBlock(nextBblk, func,
				context);

			addTerminatorsToBlocks(bblk, notIdleBblkEnd, idleBblkEnd, nextBblk,
				&idleOff);
  		}

		std::vector<Instruction*> retInsts = getReturnInsts(func);
		for (Instruction* instr : retInsts)
		{
			BasicBlock* bblk = instr->getParent();
			Instruction* firstInstr = bblk->getFirstNonPHI();
			if(firstInstr != instr)
				bblk = SplitBlock(bblk, instr);

			writeGlobalsBeforeReturnBlock(bblk, context);
            
		}
	}
	
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
BasicBlock * createLocalVarsBlock(BasicBlock *nextBblk, Function *func,
	LLVMContext& context)
{
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
	LLVMContext& context, Value** idleOff)
{
	BasicBlock *bblk = BasicBlock::Create(context, "", func, nextBblk);
	IRBuilder<> builder(bblk, bblk->begin());

	// Create a load inst to read the address of the global pointer variable
	LoadInst *hlsAddrCtrl = builder.CreateLoad(hlsAddrVar->getValueType(), 
		hlsAddrVar);
	
	// Create a getelemntptr inst to get the adress of the 0th index of the
	// address
	Value* hlsAddr0 = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(context), hlsAddrCtrl, 0);
	
	// Create a Load inst to load the number in index 0 from the hls address
	// to the Control signals
	LoadInst *hlsAddr0val = builder.CreateAlignedLoad(
		IntegerType::getInt32Ty(context), hlsAddr0, MaybeAlign(4));
	
	// Create a Store inst to store the number to currCtrlSigs
	builder.CreateStore(hlsAddr0val, localEventVars[CTRL_SIGS_INDEX]);
	
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
 * @param globVar The global variable that contains the HLS virtual address
 * @param id The function id (positive) or 0
 * @return The block created
 */
BasicBlock *writeFuncStartToHlsBlock(BasicBlock *nextBblk, Function *func,
	LLVMContext& context, int id)
{
	int nodeDataLS;
	int nodeDataMS;

	BasicBlock *bblk = BasicBlock::Create(context, "", func, nextBblk);
	IRBuilder<> builder(bblk, bblk->begin());

	// Create a load inst to read the address of the global pointer variable
	LoadInst *nodeDataLsAddr = builder.CreateLoad(
		hlsAddrVar->getValueType(), hlsAddrVar);

	// Create a getelemntptr inst to get the adress of the 4th index of the
	// address
	Value* hlsAddr4 = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(context), nodeDataLsAddr, 4);

	nodeDataLS = id;
	nodeDataLS |= (eventIDs[0] << 8);
	nodeDataLS |= (eventIDs[1] << 16);
	nodeDataLS |= (eventIDs[2] << 24);

	// Create a Store inst to store the function ID to the 4th index from
	// the HLS base address which is the id parameter
	builder.CreateStore(ConstantInt::get(context, llvm::APInt(32, nodeDataLS)),
		hlsAddr4);

	//Create a load inst to read the address of the global pointer variable
	LoadInst *nodeDataMsAddr = builder.CreateLoad(
		hlsAddrVar->getValueType(), hlsAddrVar);

	// Create a getelemntptr inst to get the adress of the 6th index of the
	// address
	Value* hlsAddr5 = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(context), nodeDataMsAddr, 5);

	nodeDataMS = eventIDs[3];
	nodeDataMS |= (eventIDs[4] << 8);
	nodeDataMS |= (eventIDs[5] << 16);

	// Create a Store inst to store the bram address to the 6th index from
	// the HLS base address which is the bram parameter
	builder.CreateStore(
		ConstantInt::get(context, llvm::APInt(32, nodeDataMS)), hlsAddr5);

	/**
	 * Write the bram address to the corresponding HLS address. This is done
	 * every time we create this block
	 */

	//Create a load inst to read the address of the global pointer variable
	LoadInst *hlsAddrBramAddr = builder.CreateLoad(
		hlsAddrVar->getValueType(), hlsAddrVar);

	// Create a getelemntptr inst to get the adress of the 20th index of the
	// address
	Value* hlsAddr28 = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(context), hlsAddrBramAddr, 28);

	// Create a Store inst to store the bram address to the 6th index from
	// the HLS base address which is the bram parameter
	builder.CreateStore(
		ConstantInt::get(context, llvm::APInt(32, BRAM_ADDRESS)), hlsAddr28);


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
	Value * startBitSet = builder.CreateOr(currSigs,
		ConstantInt::get(context, APInt(32, 0x11)));

	// Create a load inst to read the address of the global pointer variable
	LoadInst *hlsAddrCtrl = builder.CreateLoad(
		hlsAddrVar->getValueType(), hlsAddrVar);
	
	// Create a getelemntptr inst to get the HLS adress at the 0th index
	Value* hlsAddr0 = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(context), hlsAddrCtrl, 0);

	// Create a Store inst to store the set signals to the corresp. HLS address
	builder.CreateStore(startBitSet, hlsAddr0);
	
	// Create a call inst to sel4bench_reset_counters
	builder.CreateCall(Mp->getFunction("sel4bench_reset_counters"));

	// Create a call inst to sel4bench_start_counters/ 0x3F = 0b111111
	Value* s4bsceArg[1] = { ConstantInt::get(context, APInt(64, 0x3F)) };
	builder.CreateCall(Mp->getFunction("sel4bench_start_counters"), s4bsceArg);
	
	CallInst* readPmcr = builder.CreateCall(
		Mp->getFunction("sel4bench_private_read_pmcr"));

	// Create an OR inst to set the ap_start(bit 0) and ap_continue(bit 4)
	Value * cycleCounter = builder.CreateOr(readPmcr,
		ConstantInt::get(context, APInt(32, 0x4)));

	// Create a call inst to sel4bench_private_write_pmcr
	Value* s4bpwpArg[1] = { cycleCounter };
	builder.CreateCall(Mp->getFunction("sel4bench_private_write_pmcr"),
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
	LLVMContext& context)
{

	BasicBlock *bblk = BasicBlock::Create(context, "", func, nextBblk);
	IRBuilder<> builder(bblk, bblk->begin());
	
    // Create a load inst to read the address of the global pointer variable
	LoadInst *nodeDataLsAddr = builder.CreateLoad(
		hlsAddrVar->getValueType(), hlsAddrVar);

	// Create a getelemntptr inst to get the adress of the 4th index of the
	// address
	Value* hlsAddr4 = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(context), nodeDataLsAddr, 4);

	// Create a Store inst to store the function ID to the 4th index from
	// the HLS base address which is the id parameter
	builder.CreateStore(ConstantInt::get(context, llvm::APInt(32, 0)),
		hlsAddr4);

	for(int currHalf = 0; currHalf < 2; ++currHalf)
	{
		for(int currEvent = 0; currEvent < EVENTS_NUM*3; currEvent += 3)
		{
			// Create Load Inst to load current counted event val from global
			LoadInst *wholeEventVal = builder.CreateAlignedLoad(
				IntegerType::getInt64Ty(context), globalEventVars[currEvent/3],
				MaybeAlign(8));

			// HLS params are 32bit, so we break each event in half
			Value* halfEventVal;
			if(currHalf == 0)
				// Create an And Op to isolate the 32 LS bits of event counted
				halfEventVal = builder.CreateAnd(wholeEventVal,
					0x00000000ffffffff);
			else
			{
				// Create an And Op to isolate the 32 MS bits of event counted
				Value* EventMaskVal = builder.CreateAnd(wholeEventVal,
					0xffffffff00000000);
				// Create a logical shift right to Shift the MS value to 32 LS
				halfEventVal = builder.CreateLShr(EventMaskVal, 32);
			}
			// Create a truncate Inst to convert 64bit value to 32bit
			Value* eventVal32 = builder.CreateTrunc(halfEventVal,
				IntegerType::getInt32Ty(context));

			// Create a load inst to read the address of the global pointer
			// variable
			LoadInst *hlsAddr = builder.CreateLoad(
			hlsAddrVar->getValueType(), hlsAddrVar);

			// Create a getelemntptr inst to get the adress of the index 
            // required to write specific event
			Value* hlsAddrAtIndex = builder.CreateConstInBoundsGEP1_64(
				IntegerType::getInt32Ty(context),
				hlsAddr, 7 + currEvent + currHalf);

			// Create a Store inst to store the value of current event to the
			// corresponding index of the HLS base address
			builder.CreateStore(eventVal32, hlsAddrAtIndex);
		}
	}
	/**
	 * Write the bram address to the corresponding HLS address.
	 */

	//Create a load inst to read the address of the global pointer variable
	LoadInst *hlsAddrBramAddr = builder.CreateLoad(
		hlsAddrVar->getValueType(), hlsAddrVar);

	// Create a getelemntptr inst to get the adress of the 20th index of the
	// address
	Value* hlsAddr28 = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(context), hlsAddrBramAddr, 28);

	// Create a Store inst to store the bram address to the 6th index from
	// the HLS base address which is the bram parameter
	builder.CreateStore(
		ConstantInt::get(context, llvm::APInt(32, BRAM_ADDRESS)), hlsAddr28);


	/**
	 * Write value 0x11 to the control signals. 
	 */

	// Create a Load inst to load the control signals previously stored in
	// currCtrlSigs variable
	LoadInst *currSigs = builder.CreateAlignedLoad(
		IntegerType::getInt32Ty(context), localEventVars[0], MaybeAlign(4));

	// Create an OR inst to set the ap_start(bit 0) and ap_continue(bit 4)
	Value * startBitSet = builder.CreateOr(currSigs,
		ConstantInt::get(context, APInt(32, 0x11)));

	// Create a load inst to read the address of the global pointer variable
	LoadInst *hlsAddrCtrl = builder.CreateLoad(hlsAddrVar->getValueType(),
		hlsAddrVar);
	
	// Create a getelemntptr inst to get the adress of the 0th index of the
	// address
	Value* hlsAddr0 = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(context), hlsAddrCtrl, 0);

	// Create a Store inst to store the number to currCtrlSigs
	builder.CreateAlignedStore(startBitSet, hlsAddr0, MaybeAlign(4));

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
	builder.CreateCall(Mp->getFunction("sel4bench_reset_counters"));

	// Create a call inst to sel4bench_start_counters/ 0x3F = 0b111111
	Value* s4bsceArg[1] = { ConstantInt::get(context, APInt(64, 0x3F)) };
	builder.CreateCall(Mp->getFunction("sel4bench_start_counters"), s4bsceArg);
	
	CallInst* readPmcr = builder.CreateCall(
		Mp->getFunction("sel4bench_private_read_pmcr"));

	// Create an OR inst to set the ap_start(bit 0) and ap_continue(bit 4)
	Value * cycleCounter = builder.CreateOr(readPmcr,
		ConstantInt::get(context, APInt(32, 0x4)));

	// Create a call inst to sel4bench_private_write_pmcr
	Value* s4bpwpArg[1] = { cycleCounter };
	builder.CreateCall(Mp->getFunction("sel4bench_private_read_pmcr"), 
		s4bpwpArg);

	return bblk;
}


/**
 * Takes a block that contains a call instruction. Before the call it reads
 * current events and cpu cycles running and stores to local variables. After
 * the call, it writes to the HLS total numbers counted of the function that
 * returned from the global vars and marks the function termination
 * @param thisBblk We add instruction to this block
 * @param context The context of current Module
 */
void readCountersBeforeCallBlock(BasicBlock *thisBblk, LLVMContext& context)
{
	IRBuilder<> builder(thisBblk, thisBblk->begin());

	// Create a Call Inst to sel4bench_get_cycle_count to get cpu cycles
	CallInst* cycleCount = builder.CreateCall(
		Mp->getFunction("sel4bench_get_cycle_count"));

	// Create a load inst to read the value of the local event variable
	LoadInst *localCpuCyclesVal = builder.CreateLoad(
		localEventVars[0]->getAllocatedType(), localEventVars[0]);

	// Create an Add inst to add current local cpu cycles and  cycle count
	// got from the sel4bench_get_cycle_count function call.
	Value *totalCycles = builder.CreateAdd(localCpuCyclesVal, cycleCount);

	// Create a Store inst to store total cpu cycles back to the memory
	// allocated area.
	builder.CreateStore(totalCycles, localEventVars[CPU_CYCLES_INDEX]);

	// Do the same as above for all 6 events
	for (int i = 1; i < EVENTS_NUM; ++i)
	{
		Value* s4bgcArg[1] = { ConstantInt::get(context, APInt(64, i-1)) };
		CallInst* eventCount = builder.CreateCall(
			Mp->getFunction("sel4bench_get_counter"), s4bgcArg);
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
void writeGlobalsBeforeReturnBlock(BasicBlock *thisBblk, LLVMContext& context)
{
	IRBuilder<> builder(thisBblk, thisBblk->begin());

	// Create a Call Inst to sel4bench_get_cycle_count to get cpu cycles
	CallInst* cycleCount = builder.CreateCall(
		Mp->getFunction("sel4bench_get_cycle_count"));

	// Create a load inst to read the value of the local event variable
	LoadInst *localCpuCyclesVal = builder.CreateLoad(
		localEventVars[0]->getAllocatedType(), localEventVars[0]);

	// Create an Add inst to add current local cpu cycles and  cycle count
	// got from the sel4bench_get_cycle_count function call.
	Value *totalCycles = builder.CreateAdd(localCpuCyclesVal, cycleCount);

	// Create a Store inst to store total cpu cycles back to the memory
	// allocated area.
	builder.CreateStore(totalCycles, globalEventVars[0]);

	// Do the same as above for all 6 events
	for (int i = 1; i < EVENTS_NUM; ++i)
	{
		Value* s4bgcArg[1] = { ConstantInt::get(context, APInt(64, i-1)) };
		CallInst* eventCount = builder.CreateCall(
			Mp->getFunction("sel4bench_get_counter"), s4bgcArg);
		LoadInst *localEventVal = builder.CreateLoad(
			localEventVars[i]->getAllocatedType(), localEventVars[i]);
		Value *totalEvent = builder.CreateAdd(localEventVal, eventCount);
		builder.CreateStore(totalEvent, globalEventVars[i]);

		insertPrint(builder, "event before return: ", totalEvent, 
			thisBblk->getModule());
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
 * Iterates over a function's basic blocks searching for return instructions and
 * returns a vector with those instructions.
 * @param func The function of which we iterate over the basic blocks and
 * instructions
 * @return The vector of the maching return instructions
 */
std::vector<Instruction*> getReturnInsts(Function* func)
{
	std::vector<Instruction*> retInsts;

	for (Function::iterator bblk = func->begin(); bblk != func->end(); ++bblk)
	{
		for (BasicBlock::iterator instr = bblk->begin(); instr != bblk->end();
			++instr)
		{
			ReturnInst* retInst = dyn_cast<ReturnInst>(&*instr);
			if (retInst)
				retInsts.push_back(&*instr);
		}
	}
	return retInsts;
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


void insertPrint(IRBuilder<> &builder, std::string msg, Value* val, Module* m)
{
	Value* formatString;
	
	// ConstantInt* ciVal = dyn_cast<ConstantInt>(val);
	if(val->getType()->isIntegerTy(64))
	{
		formatString = builder.CreateGlobalStringPtr(msg + "%llu\n");
    	std::vector<Value*> printfArgs = {formatString, val};
    	builder.CreateCall(Mp->getFunction("printf"), printfArgs);
	}
	else
	{
		formatString = builder.CreateGlobalStringPtr(msg + "Invalid Value\n");
    	std::vector<Value*> printfArgs = {formatString};
    	builder.CreateCall(Mp->getFunction("printf"), printfArgs);
	}
}


//-----------------------------------------------------------------------------
// New PM Registration - use this for loading the pass on a c file with clang
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getInjectBRAMModPluginInfo()
{
	//std::cerr << "getInjectBRAMModPluginInfo() called\n"; // add this line
	return
	{
		LLVM_PLUGIN_API_VERSION, "InjectBRAMMod", LLVM_VERSION_STRING,
		[](PassBuilder &PB)
		{
			PB.registerOptimizerEarlyEPCallback
			(
				[](ModulePassManager &MPM, OptimizationLevel)
				{
					MPM.addPass(InjectBRAMMod());
					return true;
				}
			);
		}
	};
}


// -----------------------------------------------------------------------------
// New PM Registration - use this for loading the pass on an ll file with opt
// -----------------------------------------------------------------------------
//  llvm::PassPluginLibraryInfo getInjectBRAMModPluginInfo()
//  {
// 	 //std::cerr << "getInjectBRAMModPluginInfo() called\n"; // add this line
// 	 return
// 	 {
// 		 LLVM_PLUGIN_API_VERSION, "InjectBRAMMod", LLVM_VERSION_STRING,
// 		 [](PassBuilder &PB)
// 		 {
// 			 PB.registerPipelineParsingCallback
// 			 (
// 				 [](StringRef Name, ModulePassManager &MPM,
// 				 ArrayRef<PassBuilder::PipelineElement>)
// 				 {
// 					 if (Name == "inject-bram")
// 					 {
// 						 MPM.addPass(InjectBRAMMod());
// 						 return true;
// 					 }
// 					 return false;
// 				 }
// 			 );
// 		 }
// 	 };
//  }



// This is the core interface for pass plugins. It guarantees that 'opt' will
// be able to recognize HelloWorld when added to the pass pipeline on the
// command line, i.e. via '-passes=hello-world'
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo()
{
	return getInjectBRAMModPluginInfo();
}
