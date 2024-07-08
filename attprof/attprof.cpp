#include "attprof.hpp"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <iostream> //TODO: for debugging. Remove later
#include <fstream>	//to read files with function names
#include <string>
#include <filesystem>


using namespace llvm;

/**@{*/
/** Uncomment to make the pass insert prints on important information */
// #define DEBUG_PRINT

/** Uncomment to make enable debug prints during compilation */
// #define DEBUG_COMP_PRINT

/**
 * Uncomment when using sel4bench. Benchmarks terminate on benchmark_finished
 * instead of return in their main
 */
// #define USE_SEL4BENCH

/**
 * Uncomment to make the pass inject instructions for HLS IP communication or
 * comment to inject instruction for the software implementation of the profiler
 */
#define USE_HLS

/**
 * Uncomment to compile the pass to be injected during compilaction with clang.
 * Comment to be injected during compilation with opt.
 */
// #define USE_CLANG

/**@}*/

#ifdef DEBUG_COMP_PRINT
	#define debug_errs(str) do { errs() << str << "\n"; } while(0)
#else
	#define debug_errs(str)
#endif

/**@{*/
/** The physical address of the BRAM where the call graph is stored */
#define BRAM_ADDRESS 0xa0040000
/** The virtual address corresponding to the HLS IP. Mapped by the app */
#define HLS_VIRT_ADDR 0x7000000 ///< virtual address for our test app
// #define HLS_VIRT_ADDR 0x10013000 ///< virtual address for ipc benchmark for sel4bench
// #define HLS_VIRT_ADDR 0x10014000 ///< virtual address for page-map benchmark for sel4bench
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
// #define EVENT_0_PMU_CODE EVENT_CACHE_L1D_MISS
// #define EVENT_1_PMU_CODE EVENT_CACHE_L1D_HIT
// #define EVENT_2_PMU_CODE EVENT_TLB_L1D_MISS
// #define EVENT_3_PMU_CODE EVENT_MEMORY_READ
// #define EVENT_4_PMU_CODE EVENT_MEMORY_WRITE
// #define EVENT_5_PMU_CODE EVENT_BRANCH_MISPREDICT

// Use those events for profiling
#define EVENT_0_PMU_CODE EVENT_CACHE_L1D_MISS
#define EVENT_1_PMU_CODE EVENT_CACHE_L1I_MISS
#define EVENT_2_PMU_CODE EVENT_TLB_L1D_MISS
#define EVENT_3_PMU_CODE EVENT_TLB_L1I_MISS
#define EVENT_4_PMU_CODE EVENT_L2D_CACHE_REFILL
#define EVENT_5_PMU_CODE EVENT_L2D_CACHE_WB

// Use those events for the attestation system
// #define EVENT_0_PMU_CODE EVENT_CACHE_L1D_MISS
// #define EVENT_1_PMU_CODE EVENT_EXECUTE_BRANCH_PREDICTABLE
// #define EVENT_2_PMU_CODE EVENT_BUS_ACCESS
// #define EVENT_3_PMU_CODE EVENT_MEMORY_READ
// #define EVENT_4_PMU_CODE EVENT_MEMORY_WRITE
// #define EVENT_5_PMU_CODE EVENT_EXECUTE_INSTRUCTION
/**@}*/

#define TAKES_VAR_ARGS true

void inject_prolog_instructions(BasicBlock *block, int function_id,
	Function *prolog_function, LLVMContext &context);
BasicBlock *create_spinlock_idle_bit_block(AllocaInst *ctrl_signals_var,
	Function *function, LLVMContext &context, Value **is_idle_bit_zero_val);
BasicBlock *create_write_function_id_to_hls_block(AllocaInst *ctrlSigVar, Function *func,
	LLVMContext &context);
BasicBlock *create_write_metrics_to_hls_block(AllocaInst *ctrlSigVar, Function *func,
	LLVMContext &context);
BasicBlock *create_write_metrics_to_globals_block(Function *func,
	LLVMContext &context);
void inject_add_global_to_local_event_vars(BasicBlock *thisBblk,
	LLVMContext &context);
void inject_read_counters_add_to_local_event_vars(BasicBlock *thisBblk, LLVMContext &context);
std::vector<Instruction *> get_call_instructions(
	std::vector<std::string> &function_names, Function *func);
std::vector<Instruction *> get_return_instructions(Function *func);
Function *create_prolog_function(Module &M, LLVMContext &context);
Function* create_epilog_function(Module &M, LLVMContext &context);
void create_external_function_declarations(Module &M, LLVMContext &context);
#ifdef DEBUG_PRINT
void insertPrint(IRBuilder<> &builder, std::string msg, Value *val, Module &M);
#endif
#ifdef USE_SEL4BENCH
	std::vector<Instruction *> getTermInst(Function *func);
#endif

cl::opt<std::string> InputFilename(cl::Positional, cl::desc("<input file>"), cl::Required);

enum event
{
	CPU_CYCLES,
	EVENT0,
	EVENT1,
	EVENT2,
	EVENT3,
	EVENT4,
	EVENT5,
	TOTAL_EVENTS
};

enum declared_functions
{
	SEL4BENCH_INIT,
	SEL4BENCH_RESET_COUNTERS,	
	SEL4BENCH_SET_COUNT_EVENT,
	SEL4BENCH_START_COUNTERS,
	SEL4BENCH_PRIVATE_READ_PMCR,
	SEL4BENCH_PRIVATE_WRITE_PMCR,
	SEL4BENCH_GET_CYCLE_COUNT,
	SEL4BENCH_GET_COUNTER,
	TOTAL_DECLARED_FUNCTIONS
};

int event_pmu_codes[TOTAL_EVENTS - 1];

GlobalVariable *hls_addr_global_var;

/** 64bit cpu cycles, 64bit event 0-5 */
GlobalVariable *event_global_vars[TOTAL_EVENTS]; ///< Globals: events0-5 & cpu
AllocaInst *event_local_vars[TOTAL_EVENTS]; ///< Locals: events0-5 & cpu


std::string declared_function_names[TOTAL_DECLARED_FUNCTIONS] =
{
	"sel4bench_init",
	"sel4bench_reset_counters",
	"sel4bench_set_count_event",
	"sel4bench_start_counters",
	"sel4bench_private_read_pmcr",
	"sel4bench_private_write_pmcr",
	"sel4bench_get_cycle_count",
	"sel4bench_get_counter"
};
Function *declared_functions[TOTAL_DECLARED_FUNCTIONS] = {nullptr};

PreservedAnalyses AttProfMod::run(Module &M, ModuleAnalysisManager &AM)
{
	std::ifstream *function_names_file;
	std::string line;

	if (InputFilename.getNumOccurrences() > 0)
		function_names_file = new std::ifstream(InputFilename);
	else
	{
		errs() << "List of functions to profile, was not provided";
		exit(1);
	}
	std::vector<std::string> function_names;
	std::vector<int> function_ids;

#ifdef DEBUG_PRINT
	errs() << "DEBUG_PRINT is enabled. printf calls will be instrumented\n";
#endif
#ifdef DEBUG_COMP_PRINT
	errs() <<	"DEBUG_COMP_PRINT is enabled. debug messages will be printed"
				"during compilation\n";
#endif
#ifdef USE_CLANG
	errs() << "USE_CLANG is enabled. Pass is built to be passed through clang\n";
#else
	errs() << "USE_CLANG is disabled. Pass is built to be passed through opt\n";
#endif	

	while (getline(*function_names_file, line))
	{
		int delim_pos = line.find(" ");
		function_names.push_back(line.substr(0, delim_pos));
		function_ids.push_back(
			stoi(line.substr(delim_pos + 1, line.length() - delim_pos + 1)));
	}

	debug_errs("will now print functions got from functions.txt\n");
	for (int i = 0; i < function_names.size(); i++)
	{
		debug_errs(function_names.at(i) << ": " << function_ids.at(i));
	}
	event_pmu_codes[EVENT0 - 1] = EVENT_0_PMU_CODE;
	event_pmu_codes[EVENT1 - 1] = EVENT_1_PMU_CODE;
	event_pmu_codes[EVENT2 - 1] = EVENT_2_PMU_CODE;
	event_pmu_codes[EVENT3 - 1] = EVENT_3_PMU_CODE;
	event_pmu_codes[EVENT4 - 1] = EVENT_4_PMU_CODE;
	event_pmu_codes[EVENT5 - 1] = EVENT_5_PMU_CODE;

	// Get the context of the module. Probably the Global Context.
	LLVMContext &context = M.getContext();
	
	create_external_function_declarations(M, context);
	
	std::string fullPathFilename = M.getName().str();
	std::string src_filename = std::filesystem::path(fullPathFilename).filename().
		replace_extension("").string();
	debug_errs("filename of Module: " << src_filename << "\n");

	Type *Int64PtrTy = Type::getInt64PtrTy(context);
	hls_addr_global_var = new GlobalVariable(M, Int64PtrTy, false,
		GlobalValue::ExternalLinkage, nullptr, src_filename + "-hlsptr");
	ConstantInt *hls_virt_addr_val = ConstantInt::get(context, APInt(64, HLS_VIRT_ADDR));
	Constant *hls_virt_addr_ptr = ConstantExpr::getIntToPtr(hls_virt_addr_val, Int64PtrTy);
	hls_addr_global_var->setAlignment(Align(8));
	hls_addr_global_var->setInitializer(hls_virt_addr_ptr);

	// Create the global variables for the cpu cycles and the 6 events
	for (int event_idx = 0; event_idx < TOTAL_EVENTS; event_idx++)
	{
		event_global_vars[event_idx] = new GlobalVariable(M,
			Type::getInt64Ty(context), false, GlobalValue::ExternalLinkage,
			nullptr, src_filename + "-ev" + std::to_string(event_idx));
		event_global_vars[event_idx]->setDSOLocal(true);
		event_global_vars[event_idx]->setAlignment(Align(8));
		event_global_vars[event_idx]->setInitializer(
			ConstantInt::get(context, APInt(64, 0)));
	}
	Function* prolog_function = create_prolog_function(M, context);
	Function* epilog_function = create_epilog_function(M, context);

	Function *main_function = M.getFunction("main");

	// In main function initialize counters and set them to specific events 
	if(main_function)
	{
		BasicBlock *main_function_entry_block = &main_function->getEntryBlock();

		IRBuilder<> main_function_entry_block_builder(main_function_entry_block,
			main_function_entry_block->begin());

#ifdef DEBUG_PRINT
		insertPrint(main_function_entry_block_builder, "HLS virt address: ", hls_virt_addr_val,
			*main_function_entry_block->getModule());
#endif
		main_function_entry_block_builder.CreateCall(declared_functions[SEL4BENCH_INIT]);

		// Create 6 call instructions to sel4bench_set_count_event
		for (int event_idx = 0; event_idx < TOTAL_EVENTS - 1; ++event_idx)
		{
			Value *s4bsceArgs[2] =
			{
				ConstantInt::get(context, APInt(64, event_idx)),
				ConstantInt::get(context, APInt(64, event_pmu_codes[event_idx]))
			};
			main_function_entry_block_builder.CreateCall(
				declared_functions[SEL4BENCH_SET_COUNT_EVENT], s4bsceArgs);
		}
		debug_errs("main prologue instrumentation done!!");
	}
	else
		debug_errs("This module has no main!");
	for (int i = 0; i < function_names.size(); ++i)
	{
		Value *is_idle_bit_zero_val;

		// get function the function
		Function *func = M.getFunction(function_names.at(i));

		// function may not exist or may only be declared in this module. In
		// that case we don't perform any instrumentation
		if(!func) continue;
		if(func->empty()) continue;

		BasicBlock *firstBblk = &func->getEntryBlock();

		inject_prolog_instructions(&func->getEntryBlock(), function_ids.at(i),
			prolog_function, context);

		std::vector<Instruction *> callInsts = get_call_instructions(function_names, func);

		// For every call instructions split blocks. In the end bblk is a block
		// that contains just the call instruction and nextBblk is the block
		// that bblk branches to at the end
		for (Instruction *instr : callInsts)
		{
			BasicBlock *bblk = instr->getParent();
			// Instruction *firstInstr = bblk->getFirstNonPHI();
			// if (firstInstr != instr)
			if (&(bblk->front()) != instr)
				bblk = SplitBlock(bblk, instr);

			BasicBlock *nextBblk = SplitBlock(bblk,
				instr->getNextNonDebugInstruction());

			inject_read_counters_add_to_local_event_vars(bblk, context);

			inject_add_global_to_local_event_vars(nextBblk, context);
		}
		std::vector<Instruction *> retInsts = get_return_instructions(func);
		for (Instruction *instr : retInsts)
		{
			Value *epilog_functionArgs[TOTAL_EVENTS] =
			{
				event_local_vars[CPU_CYCLES],
				event_local_vars[EVENT0],
				event_local_vars[EVENT1],
				event_local_vars[EVENT2],
				event_local_vars[EVENT3],
				event_local_vars[EVENT4],
				event_local_vars[EVENT5]
			};
			CallInst *epilog_functionCi = CallInst::Create(epilog_function->getFunctionType(),
				epilog_function, epilog_functionArgs, "attprof_epilog");
			epilog_functionCi->insertBefore(instr);
		}
		debug_errs(function_names.at(i) << " instrumentation done!!");
	}
	return llvm::PreservedAnalyses::all();
}

/**
 * Injects instructions at the prologue of a function that is being profiled. It
 * allocates 7 local variables to store counted events and inserts a call to a
 * function that writes to hardware (prolog_function).
 * @param block The block of the function we add the instructions (entry block)
 * @param function_id The id of the Function we want to write to the hardware
 * @param prolog_function The function we want to call in that block
 * @param context The context of current Module
 */
void inject_prolog_instructions(BasicBlock *block, int function_id,
	Function *prolog_function, LLVMContext &context)
{
	debug_errs(__func__<< ": " << block->getParent()->getName().str() << ": Enter");
	IRBuilder<> block_builder(block, block->begin());
#ifdef DEBUG_PRINT
	insertPrint(block_builder,  std::string(__func__) + " <<" + block->getParent()->
		getName().str() + ">>: START", nullptr, *block->getModule());
#endif
	// Create 7 64bit Mem Alloc Insts for local vars to store cpu cycles and 6
	// events for this function.
	for (int i = 0; i < TOTAL_EVENTS; ++i)
	{
		event_local_vars[i] = block_builder.CreateAlloca(
			IntegerType::getInt64Ty(context));
		block_builder.CreateStore( ConstantInt::get(context, llvm::APInt(64, 0)),
			event_local_vars[i]);
	}
	Value *prolog_function_arg_val[1] = {ConstantInt::get(context, APInt(32, function_id))};
	block_builder.CreateCall(prolog_function, prolog_function_arg_val);
#ifdef DEBUG_PRINT
	insertPrint(block_builder,  std::string(__func__) + " <<" + block->getParent()->
		getName().str() + ">>: END", nullptr, *block->getModule());
#endif
	debug_errs(__func__<< ": " << block->getParent()->getName().str() << ": Enter");
}


/**
 * Builds a Block that spins on the idle bit of the control signals of the HLS
 * IP. Must keep spinning until the IP is idle.
 * @param nextBblk We insert our new block before that block
 * @param func The function upon we build the block
 * @param context The context of current Module
 * @return The block created
 */
BasicBlock *create_spinlock_idle_bit_block(AllocaInst *ctrl_signals_var, Function *function,
	LLVMContext &context, Value **is_idle_bit_zero_val)
{
	debug_errs(__func__<< ": " << function->getName().str() << ": Enter");
	BasicBlock *bblk = BasicBlock::Create(context, "", function);
	IRBuilder<> builder(bblk, bblk->begin());
#ifdef DEBUG_PRINT
	insertPrint(builder,  std::string(__func__) + " <<" + function->getName().str()
		+ ">>: START", nullptr, *bblk->getModule());
#endif
	debug_errs(__func__<< ": " << function->getName().str() << ": Enter");
#ifdef USE_HLS
	// Create a load inst to read the address of the global pointer variable
	LoadInst *hlsAddrCtrl = builder.CreateLoad(hls_addr_global_var->getValueType(),
		hls_addr_global_var);
	// Create a getelemntptr inst to get the adress of the 0th index of the
	// address
	Value *hlsAddr0 = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(context), hlsAddrCtrl, 0);
	// Create a Load inst to load the number in index 0 from the hls address
	// to the Control signals
	LoadInst *hlsAddr0val = builder.CreateAlignedLoad(
		IntegerType::getInt32Ty(context), hlsAddr0, MaybeAlign(4));
	// Create a Store inst to store the number to currCtrlSigs
	builder.CreateStore(hlsAddr0val, ctrl_signals_var);
	// Create an And Op to isolate the idle bit from the ctrl signals
	Value *idleBit = builder.CreateAnd(hlsAddr0val, 4);
	// Create a Compare Not Equal inst to check if the Idle Bit is not 0.
	// A true result means the bit is idle so we need to escape the loop
	Value *idleOn = builder.CreateICmpNE(idleBit,
		ConstantInt::get(context, APInt(32, 0)));
	// Create an XOR inst to compare idleOn with 'true' to reverse the
	// boolean value of previous compare and assign result to is_idle_bit_zero_val
	*is_idle_bit_zero_val = builder.CreateXor(idleOn, ConstantInt::getTrue(context));
#else //This case works for both the software case or if not using any case
	// We want is_idle_bit_zero_val to be false so that we never loop when not using HLS
	*is_idle_bit_zero_val = builder.CreateXor(ConstantInt::getTrue(context),
		ConstantInt::getTrue(context));
#endif
#ifdef DEBUG_PRINT
	insertPrint(builder,  std::string(__func__) + " <<" + function->getName().str()
		+ ">>: END", nullptr, *bblk->getModule());
#endif
	debug_errs("in " << __func__  <<": Exit");
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
BasicBlock *create_write_function_id_to_hls_block(AllocaInst *ctrlSigVar, Function *func,
	LLVMContext &context)
{

	int nodeDataLS;
	int nodeDataMS;

	BasicBlock *bblk = BasicBlock::Create(context, "", func);
	IRBuilder<> builder(bblk, bblk->begin());

#ifdef DEBUG_PRINT
	insertPrint(builder,  std::string(__func__) + " <<" + func->getName().str()
		+ ">>: START", nullptr, *bblk->getModule());
#endif
	debug_errs(__func__<< ": " << func->getName().str() << ": Enter");

	// Create a load inst to read the address of the global pointer variable
	LoadInst *nodeDataLsAddr = builder.CreateLoad(
		hls_addr_global_var->getValueType(), hls_addr_global_var);

	// Create a getelemntptr inst to get the adress of the 4th index of the
	// address
	Value *hlsAddr4 = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(context), nodeDataLsAddr, 4);
	Argument *funcIdArg = &*func->arg_begin();
	nodeDataLS = 0;
	nodeDataLS |= (event_pmu_codes[EVENT0 - 1] << 8);
	nodeDataLS |= (event_pmu_codes[EVENT1 - 1] << 16);
	nodeDataLS |= (event_pmu_codes[EVENT2 - 1] << 24);

	// Create a Store inst to store the function ID to the 4th index from
	// the HLS base address which is the id parameter
	ConstantInt *eventsLsInt =
		ConstantInt::get(context, llvm::APInt(32, nodeDataLS));

	Value *nodatDataLsInt = builder.CreateOr(funcIdArg, eventsLsInt);

#ifdef DEBUG_PRINT
	insertPrint(builder, "\tnodeDataLS: ", nodatDataLsInt, *bblk->getModule());
#endif
	builder.CreateStore(nodatDataLsInt, hlsAddr4);
	// Create a load inst to read the address of the global pointer variable
	LoadInst *nodeDataMsAddr = builder.CreateLoad(
		hls_addr_global_var->getValueType(), hls_addr_global_var);

	// Create a getelemntptr inst to get the adress of the 6th index of the
	// address
	Value *hlsAddr6 = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(context), nodeDataMsAddr, 6);

	nodeDataMS = 0;
	nodeDataMS |= (event_pmu_codes[EVENT3 - 1] << 8);
	nodeDataMS |= (event_pmu_codes[EVENT4 - 1] << 16);
	nodeDataMS |= (event_pmu_codes[EVENT5 - 1] << 24);

	// Create a Store inst to store the bram address to the 6th index from
	// the HLS base address which is the bram parameter
	ConstantInt *nodatDataMsInt =
		ConstantInt::get(context, llvm::APInt(32, nodeDataMS));
#ifdef DEBUG_PRINT
	// insertPrint(builder, "\tnodeDataMS: ", nodatDataMsInt, *bblk->getModule());
#endif
	builder.CreateStore(nodatDataMsInt, hlsAddr6);

	/**
	 * Write the bram address to the corresponding HLS address. This is done
	 * every time we create this block
	 */

	// Create a load inst to read the address of the global pointer variable
	LoadInst *hlsAddrBramAddr = builder.CreateLoad(
		hls_addr_global_var->getValueType(), hls_addr_global_var);

	// Create a getelemntptr inst to get the adress of the 20th index of the
	// address
	Value *hlsAddr36 = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(context), hlsAddrBramAddr, 36);
#ifdef DEBUG_PRINT
	insertPrint(builder, "\tWill write to: ", hlsAddr36, *bblk->getModule());
#endif
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
		ctrlSigVar->getAllocatedType(),
		ctrlSigVar);

	// Create an OR inst to set the ap_start(bit 0) and ap_continue(bit 4)
	Value *startBitSet = builder.CreateOr(currSigs,
		ConstantInt::get(context, APInt(32, 0x11)));

	// Create a load inst to read the address of the global pointer variable
	LoadInst *hlsAddrCtrl = builder.CreateLoad(
		hls_addr_global_var->getValueType(), hls_addr_global_var);

	// Create a getelemntptr inst to get the HLS adress at the 0th index
	Value *hlsAddr0 = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(context), hlsAddrCtrl, 0);

	// Create a Store inst to store the set signals to the corresp. HLS address
	builder.CreateStore(startBitSet, hlsAddr0);

	// Create a call inst to sel4bench_reset_counters
	builder.CreateCall(declared_functions[SEL4BENCH_RESET_COUNTERS]);

	// Create a call inst to sel4bench_start_counters/ 0x3F = 0b111111
	Value *s4bsceArg[1] = {ConstantInt::get(context, APInt(64, 0x3F))};
	builder.CreateCall(declared_functions[SEL4BENCH_START_COUNTERS], s4bsceArg);

	CallInst *readPmcr = builder.CreateCall(
		declared_functions[SEL4BENCH_PRIVATE_READ_PMCR]);

	// Create an OR inst to set the ap_start(bit 0) and ap_continue(bit 4)
	Value *cycleCounter = builder.CreateOr(readPmcr,
		ConstantInt::get(context, APInt(32, 0x4)));

	// Create a call inst to sel4bench_private_write_pmcr
	Value *s4bpwpArg[1] = {cycleCounter};
	builder.CreateCall(declared_functions[SEL4BENCH_PRIVATE_WRITE_PMCR],
		s4bpwpArg);

#ifdef DEBUG_PRINT
	insertPrint(builder,  std::string(__func__) + " <<" + func->getName().str()
		+ ">>: END", nullptr, *bblk->getModule());
#endif
	debug_errs(__func__<< ": " << func->getName().str() << ": Exit");
	return bblk;
}

/**
 * Builds a Block that writes to the HLS IP, CPU cycles and the 6 events counted
 * of a function before it returns. We write 0 to function id parameter to mark
 * the end of the function.
 * @param nextBblk We insert our new block before that block
 * @param func The function upon we build the block
 * @param context The context of current Module
 * @return The block created
 */
BasicBlock *create_write_metrics_to_hls_block(AllocaInst *ctrlSigVar, Function *func,
	LLVMContext &context)
{
	Value *atfArgs[16];
	BasicBlock *bblk = BasicBlock::Create(context, "", func);
	IRBuilder<> builder(bblk, bblk->begin());
	
#ifdef DEBUG_PRINT
	insertPrint(builder,  std::string(__func__) + " <<" + func->getName().str()
		+ ">>: START", nullptr, *bblk->getModule());
#endif
	debug_errs(__func__<< ": " << func->getName().str() << ": Enter");

	// Create a load inst to read the address of the global pointer variable
	LoadInst *nodeDataLsAddr = builder.CreateLoad(
		hls_addr_global_var->getValueType(), hls_addr_global_var);

	// Create a getelemntptr inst to get the adress of the 4th index of the
	// address
	Value *hlsAddr4 = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(context), nodeDataLsAddr, 4);
#ifdef DEBUG_PRINT
	insertPrint(builder, "\tWill write 0 to: ", hlsAddr4, *bblk->getModule());
#endif

	// Create a Store inst to store the function ID to the 4th index from
	// the HLS base address which is the id parameter
	builder.CreateStore(ConstantInt::get(context, llvm::APInt(32, 0)),
		hlsAddr4);
	// This for loop gets the values from the performance counters, splits their
	// 64bit values to 32bits, and if we use the Hls, the values are written to
	// the HLS address, otherwise, they are added as parameters to an array for
	// calling the software implementation of HLS.
	Value *eventVal32;
	for (int currHalf = 0; currHalf < 4; currHalf += 2)
	{
		for (int currEvent = 0; currEvent < TOTAL_EVENTS * 4; currEvent += 4)
		{
			// Create Load Inst to load current counted event val from global
			LoadInst *wholeEventVal = builder.CreateAlignedLoad(
				IntegerType::getInt64Ty(context), event_global_vars[currEvent/4],
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
			// Create a load inst to read the address of the global pointer
			// variable
			LoadInst *hlsAddr = builder.CreateLoad(
				hls_addr_global_var->getValueType(), hls_addr_global_var);

			// Create a getelemntptr inst to get the adress of the index
			// required to write specific event
			Value *hlsAddrAtIndex = builder.CreateConstInBoundsGEP1_64(
				IntegerType::getInt32Ty(context), hlsAddr,
				8 + currEvent + currHalf);
#ifdef DEBUG_PRINT
			insertPrint(builder, "\t\twill write to: ", hlsAddrAtIndex,
				*bblk->getModule());
#endif
			// Create a Store inst to store the value of current event to the
			// corresponding index of the HLS base address
			builder.CreateStore(eventVal32, hlsAddrAtIndex);
			atfArgs[2 + currEvent/2 + currHalf/2] = eventVal32;
		}
	}

	/**
	 * Write the bram address to the corresponding HLS address.
	 */
	// Create a load inst to read the address of the global pointer variable
	LoadInst *hlsAddrBramAddr = builder.CreateLoad(
		hls_addr_global_var->getValueType(), hls_addr_global_var);

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
		IntegerType::getInt32Ty(context), ctrlSigVar,
		MaybeAlign(4));

	// Create an OR inst to set the ap_start(bit 0) and ap_continue(bit 4)
	Value *startBitSet = builder.CreateOr(currSigs,
		ConstantInt::get(context, APInt(32, 0x11)));

	// Create a load inst to read the address of the global pointer variable
	LoadInst *hlsAddrCtrl = builder.CreateLoad(hls_addr_global_var->getValueType(),
		hls_addr_global_var);

	// Create a getelemntptr inst to get the adress of the 0th index of the
	// address
	Value *hlsAddr0 = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(context), hlsAddrCtrl, 0);

	// Create a Store inst to store the number to currCtrlSigs
	builder.CreateAlignedStore(startBitSet, hlsAddr0, MaybeAlign(4));

#ifdef DEBUG_PRINT
	insertPrint(builder,  std::string(__func__) + " <<" + func->getName().str()
		+ ">>: END", nullptr, *bblk->getModule());
#endif
	debug_errs(__func__<< ": " << func->getName().str() << ": Exit");
	return bblk;
}

/**
 * Takes a block that is directly after a block that ends with a call
 * instruction to a profiling function. It adds the metrics carried over from
 * the returned function stored in the global variables, with the current
 * function metrics stored in the local variables and stores the result back in
 * the local variables. 
 * @param thisBblk We add instruction to this block
 * @param context The context of current Module
 */
void inject_add_global_to_local_event_vars(BasicBlock *thisBblk, LLVMContext &context)
{
	IRBuilder<> builder(thisBblk, thisBblk->begin());
#ifdef DEBUG_PRINT
	insertPrint(builder,  std::string(__func__) + " <<" + thisBblk->getParent()->getName().str()
		+ ">>: START", nullptr, *thisBblk->getModule());
#endif
	debug_errs(__func__<< ": " << thisBblk->getParent()->getName().str() << ": Enter");

	for (int i = 0; i < TOTAL_EVENTS; ++i)
	{
		// Create a load inst to read the value of the global event variable
		LoadInst *globalEventVal = builder.CreateLoad(
			event_global_vars[i]->getValueType(), event_global_vars[i]);

		// Create a load inst to read the value of the local event variable
		LoadInst *localEventVal = builder.CreateLoad(
			event_local_vars[i]->getAllocatedType(), event_local_vars[i]);

		// Create an Add inst to add local event value and global event value
		Value *totalEventVal = builder.CreateAdd(localEventVal, globalEventVal);

		// Create a Store inst to store sum back to local event var
		builder.CreateStore(totalEventVal, event_local_vars[i]);
	}
	// Create a call inst to sel4bench_reset_counters
	builder.CreateCall(declared_functions[SEL4BENCH_RESET_COUNTERS]);

	// Create a call inst to sel4bench_start_counters/ 0x3F = 0b111111
	Value *s4bsceArg[1] = {ConstantInt::get(context, APInt(64, 0x3F))};
	builder.CreateCall(declared_functions[SEL4BENCH_START_COUNTERS], s4bsceArg);

	CallInst *readPmcr = builder.CreateCall(
		declared_functions[SEL4BENCH_PRIVATE_READ_PMCR]);

	// Create an OR inst to set the ap_start(bit 0) and ap_continue(bit 4)
	Value *cycleCounter = builder.CreateOr(readPmcr,
		ConstantInt::get(context, APInt(32, 0x4)));

	// Create a call inst to sel4bench_private_write_pmcr
	Value *s4bpwpArg[1] = {cycleCounter};
	builder.CreateCall(declared_functions[SEL4BENCH_PRIVATE_READ_PMCR],
		s4bpwpArg);

#ifdef DEBUG_PRINT
	insertPrint(builder,  std::string(__func__) + " <<" + thisBblk->getParent()->getName().str() + ">>: END",
		nullptr, *thisBblk->getModule());
#endif
	debug_errs(__func__<< ": " << thisBblk->getParent()->getName().str() << ": Exit");
}

/**
 * Takes a block that contains a call instruction. Before the call it reads
 * current events and cpu cycles running and stores to local variables.
 * @param thisBblk We add instruction to this block
 * @param context The context of current Module
 */
void inject_read_counters_add_to_local_event_vars(BasicBlock *thisBblk, LLVMContext &context)
{
	IRBuilder<> builder(thisBblk, thisBblk->begin());
#ifdef DEBUG_PRINT
	insertPrint(builder,  std::string(__func__) + " <<" + thisBblk->getParent()->
		getName().str() + ">>: START", nullptr, *thisBblk->getModule());
#endif
	debug_errs(__func__<< ": " << thisBblk->getParent()->getName().str() << ": Enter");
	CallInst *countEventVals[TOTAL_EVENTS];

	// Create call inst to sel4bench_get_cycle_count to get cpu cycles count
	countEventVals[CPU_CYCLES] = builder.CreateCall(
		declared_functions[SEL4BENCH_GET_CYCLE_COUNT]);

	// Create call inst to sel4bench_get_counter to get the other event counts
	for (int i = 1; i < TOTAL_EVENTS; ++i)
	{
		Value *s4bgcArg[1] = {ConstantInt::get(context, APInt(64, i - 1))};
		countEventVals[i] = builder.CreateCall(
			declared_functions[SEL4BENCH_GET_COUNTER], s4bgcArg);
	}
	// For each count, load local, add with recent counts, store back to local
	for (int i = 0; i < TOTAL_EVENTS; ++i)
	{
		LoadInst *localEventVal = builder.CreateLoad(
			event_local_vars[i]->getAllocatedType(), event_local_vars[i]);
		Value *totalEventVal = builder.CreateAdd(localEventVal,
			countEventVals[i]);
		builder.CreateStore(totalEventVal, event_local_vars[i]);
	}
	#ifdef DEBUG_PRINT
	insertPrint(builder,  std::string(__func__) + " <<" + thisBblk->getParent()->getName().str() + ">>: END",
		nullptr, *thisBblk->getModule());
#endif
	debug_errs(__func__<< ": " << thisBblk->getParent()->getName().str() << ": Exit");
}

/**
 * Takes a block that contains a return instruction. Before that instruction, it
 * adds each local event var and the perf counter of the same event, and stores
 * the sum to the corresponding global variable.
 * @param nextBblk We insert our new block before that block
 * @param func The function upon we build the block
 * @param context The context of current Module
 */
BasicBlock *create_write_metrics_to_globals_block(Function *func, LLVMContext &context)
{
	BasicBlock *bblk = BasicBlock::Create(context, "", func);
	IRBuilder<> builder(bblk, bblk->begin());

#ifdef DEBUG_PRINT
	insertPrint(builder,  std::string(__func__) + " <<" + func->getName().str()
		+ ">>: START", nullptr, *bblk->getModule());
#endif
	debug_errs(__func__<< ": " << func->getName().str() << ": Enter");

	CallInst *countEventVals[TOTAL_EVENTS];
	// Create call inst to sel4bench_get_cycle_count to get cpu cycles count
	countEventVals[CPU_CYCLES] = builder.CreateCall(
		declared_functions[SEL4BENCH_GET_CYCLE_COUNT]);

	// Create call inst to sel4bench_get_counter to get the other event counts
	for (int i = 1; i < TOTAL_EVENTS; ++i)
	{
		Value *s4bgcArg[1] = {ConstantInt::get(context, APInt(64, i - 1))};
		countEventVals[i] = builder.CreateCall(
			declared_functions[SEL4BENCH_GET_COUNTER], s4bgcArg);
	}
	// For each count, load local, add with recent counts, store back to local
	Function::arg_iterator  arg = func->arg_begin();
	for (int i = 0; i < TOTAL_EVENTS; ++i)
	{
		Value* argVal = &*arg;

		LoadInst *localEventVal = builder.CreateLoad(
			argVal->getType(), argVal);
#ifdef DEBUG_PRINT
			insertPrint(builder, "\tadding arg: ", argVal, 
				*func->getParent());
			insertPrint(builder, "\twith count event: ", countEventVals[i], 
				*func->getParent());
#endif
		// LoadInst *localEventVal = builder.CreateLoad(
		// 	event_local_vars[i]->getAllocatedType(), event_local_vars[i]);
		Value *totalEventVal = builder.CreateAdd(localEventVal,
			countEventVals[i]);
		builder.CreateStore(totalEventVal, event_global_vars[i]);
		++arg;
	}
#ifdef DEBUG_PRINT
	insertPrint(builder,  std::string(__func__) + " <<" + func->getName().str()
		+ ">>: END", nullptr, *bblk->getModule());
#endif
	debug_errs(__func__<< ": " << func->getName().str() << ": Exit");
	return bblk;
}


/**
 * Iterates over a function's basic blocks searching for call instructions which
 * call functions that match the name of the functions we want to instrument.
 * Finally it returns a vector with those instructions.
 * @param function_names A vector containing the function names of which the call
 * instructions must match with
 * @param func The function of which we iterate over the basic blocks and
 * instructions
 * @return The vector of the maching call instructions
 */
std::vector<Instruction *> get_call_instructions(
	std::vector<std::string> &function_names, Function *function)
{
	debug_errs(__func__<< ": " << function->getName().str() << ": Enter");

	std::vector<Instruction *> call_instructions;

	for (Function::iterator basic_block = function->begin();
		basic_block != function->end(); ++basic_block)
	{
		for (BasicBlock::iterator instruction = basic_block->begin();
			instruction != basic_block->end(); ++instruction)
		{
			CallInst *call_instruction = dyn_cast<CallInst>(&*instruction);
			if (!call_instruction)
				continue;
			std::vector<std::string>::iterator function_names_iter;
			Function *call_instruction_function =
				call_instruction->getCalledFunction();
			if(!call_instruction_function)
				continue;
			std::string call_instruction_function_name =
			call_instruction->getCalledFunction()->getName().str();
			function_names_iter = std::find(function_names.begin(),
				function_names.end(), call_instruction_function_name);
			if (function_names_iter == function_names.end())
				continue;
			call_instructions.push_back(&*instruction);
#ifdef DEBUG_PRINT
			IRBuilder<> builder(&*basic_block, (&*basic_block)->begin());
			insertPrint(builder,  std::string(__func__) + " <<" +
				func->getName().str() + ">>: call found to " +
				calledFuncName,	nullptr, *basic_block->getModule());
#endif
		}
	}
	debug_errs(__func__<< ": " << function->getName().str() << ": Exit");
	return call_instructions;
}

/**
 * Iterates over a function's basic blocks searching for return instructions and
 * returns a vector with those instructions.
 * @param func The function of which we iterate over the basic blocks and
 * instructions
 * @return The vector of the maching return instructions
 */
std::vector<Instruction *> get_return_instructions(Function *function)
{
	debug_errs(__func__<< ": " << function->getName().str() << ": Enter");

	std::vector<Instruction *> return_instructions;
	for (Function::iterator basic_block = function->begin();
		basic_block != function->end(); ++basic_block)
	{
		for (BasicBlock::iterator instruction = basic_block->begin();
			instruction != basic_block->end(); ++instruction)
		{
			ReturnInst *return_instruction = dyn_cast<ReturnInst>(&*instruction);
			if (return_instruction)
			{
#ifdef DEBUG_PRINT
				IRBuilder<> builder(&*bblk, (&*bblk)->begin());
				insertPrint(builder,  std::string(__func__) + " <<" +
					func->getName().str() + ">>: return found!", nullptr,
					*bblk->getModule());
#endif
				return_instructions.push_back(&*instruction);
			}
		}
	}
	debug_errs(__func__<< ": " << function->getName().str() << ": Exit");
	return return_instructions;
}
#ifdef USE_SEL4BENCH
std::vector<Instruction *> getTermInst(Function *func)
{
	debug_errs(__func__<< ": " << func->getName().str() << ": Enter");

	std::vector<Instruction *> retInsts;

	for (Function::iterator bblk = func->begin(); bblk != func->end(); ++bblk)
	{
		for (BasicBlock::iterator instr = bblk->begin(); instr != bblk->end();
			++instr)
		{
			CallInst *callInst = dyn_cast<CallInst>(&*instr);
			if (callInst)
			{
				Function *calledFunction = callInst->getCalledFunction();
				if(calledFunction != nullptr) //.equals(StringRef("", 0))
				{
					if(calledFunction->getName().str() == "benchmark_finished")
					{
						retInsts.push_back(&*instr);
					}
				}
			}
		}
	}
	debug_errs(__func__<< ": " << func->getName().str() << ": Exit");
	return retInsts;
}
#endif


/**
 * Defines a function that will be injected by the pass as a call instruction
 * to the prologue of functions that will be profiled. First, it spinlocks at
 * the HLS hardware until it is idle. When idle, it will write the profiling
 * function's ID and profiling events' IDs to HLS.
 *
 * @param M The current Module Object
 * @param context The context of current Module
 * @return The object to the function created
 */
Function* create_prolog_function(Module &M, LLVMContext &context)
{
	debug_errs(__func__<< ": Enter");
	Value *is_idle_bit_zero_val;

	std::vector<Type*> prolog_function_parameter_type {Type::getInt32Ty(context)}; 
	Type* prolog_function_return_type = Type::getVoidTy(context);
	FunctionType* prolog_function_type = FunctionType::get(prolog_function_return_type,
		prolog_function_parameter_type, !TAKES_VAR_ARGS);
	Function* prolog_function = Function::Create(prolog_function_type,
		Function::ExternalLinkage, "attprof_prolog", &M);
	std::string absolut_path_filename = M.getName().str();
	std::string src_filename = std::filesystem::path(absolut_path_filename).
		filename().replace_extension("").string();
	if(src_filename == "main")
	{	// create blocks
		BasicBlock *entry_block = BasicBlock::Create(context, "", prolog_function);
		IRBuilder<> entry_block_builder(entry_block, entry_block->begin());
		AllocaInst* control_signals_var = entry_block_builder.CreateAlloca(
			IntegerType::getInt32Ty(context));
		BasicBlock *spinlock_idle_bit_block = create_spinlock_idle_bit_block(
			control_signals_var,prolog_function, context, &is_idle_bit_zero_val);
		BasicBlock *write_function_id_to_hls_block = create_write_function_id_to_hls_block(
			control_signals_var, prolog_function, context);
		// connect blocks
		entry_block_builder.CreateBr(spinlock_idle_bit_block);
		IRBuilder<> spinlock_idle_bit_block_builder(spinlock_idle_bit_block,
			spinlock_idle_bit_block->end());
		spinlock_idle_bit_block_builder.CreateCondBr(is_idle_bit_zero_val,
			spinlock_idle_bit_block, write_function_id_to_hls_block);
		IRBuilder<> write_function_id_to_hls_block_builder(write_function_id_to_hls_block,
			write_function_id_to_hls_block->end());
		write_function_id_to_hls_block_builder.CreateRetVoid();
		debug_errs(__func__<< ": Exit");
	}
	return prolog_function;
}

/**
 * Defines a function to which call instructions will be injected by the pass,
 * before each return instruction of a function that will be profiled.
 * @param M The current Module Object
 * @param context The context of current Module
 * @return The object to the function created
 */
Function* create_epilog_function(Module &M, LLVMContext &context)
{
	debug_errs(__func__<< ": Enter");
	Value *is_idle_bit_zero_val;

	std::vector<Type*> epilog_function_parameter_type
	{
		Type::getInt64Ty(context), // cpu cycles
		Type::getInt64Ty(context), // event 0
		Type::getInt64Ty(context), // event 1
		Type::getInt64Ty(context), // event 2
		Type::getInt64Ty(context), // event 3
		Type::getInt64Ty(context), // event 4
		Type::getInt64Ty(context)  // event 5
	};
	Type* epilog_function_return_type = Type::getVoidTy(context);
	FunctionType* epilog_function_type = FunctionType::get(epilog_function_return_type,
		epilog_function_parameter_type, !TAKES_VAR_ARGS);
	Function* epilog_function = Function::Create(epilog_function_type,
		Function::ExternalLinkage, "attprof_epilog", &M);
	std::string absolut_path_filename = M.getName().str();
	std::string src_filename = std::filesystem::path(absolut_path_filename).
		filename().replace_extension("").string();
	
	if(src_filename == "main")
	{	// create blocks
		BasicBlock *entry_block = BasicBlock::Create(context, "", epilog_function);
		IRBuilder<> entry_block_builder(entry_block, entry_block->begin());
		AllocaInst* control_signals_var = entry_block_builder.CreateAlloca(
			IntegerType::getInt32Ty(context));
		BasicBlock *write_metrics_to_globals_block =
			create_write_metrics_to_globals_block(epilog_function, context);
		BasicBlock *spinlock_idle_bit_block = create_spinlock_idle_bit_block(
			control_signals_var, epilog_function, context, &is_idle_bit_zero_val);
		BasicBlock *write_metrics_to_hls_block = create_write_metrics_to_hls_block(
			control_signals_var, epilog_function, context);
		// connect blocks
		entry_block_builder.CreateBr(write_metrics_to_globals_block);
		IRBuilder<> write_metrics_to_globals_block_builder(
			write_metrics_to_globals_block, write_metrics_to_globals_block->end());
		write_metrics_to_globals_block_builder.CreateBr(spinlock_idle_bit_block);
		IRBuilder<> spinlock_idle_bit_block_builder(spinlock_idle_bit_block,
			spinlock_idle_bit_block->end());
		spinlock_idle_bit_block_builder.CreateCondBr(is_idle_bit_zero_val,
			spinlock_idle_bit_block, write_metrics_to_hls_block);
		IRBuilder<> write_metrics_to_hls_block_builder(
			write_metrics_to_hls_block, write_metrics_to_hls_block->end());
		write_metrics_to_hls_block_builder.CreateRetVoid();
		debug_errs(__func__<< ": Exit");
	}
	return epilog_function;
}

/**
 * Iterates over the SeL4 function names in @c declared_function_names and for every
 * function that is not visible in the module, it creates an external
 * declaration to it, so that it can be compiled successfully before linking
 * phase.
 * @param M The current Module Object
 * @param context The context of current Module
 */
void create_external_function_declarations(Module &M, LLVMContext &context)
{
	debug_errs(__func__<< ": Enter");
	std::vector<std::vector<Type*>> declared_function_parameter_types
	{
		{},
		{},
		{Type::getInt64Ty(context)},
		{Type::getInt64Ty(context), Type::getInt64Ty(context)},
		{},
		{Type::getInt32Ty(context)},
		{},
		{Type::getInt64Ty(context)}
	};
	Type* declared_function_return_types[TOTAL_DECLARED_FUNCTIONS] =
	{
		Type::getVoidTy(context),
		Type::getVoidTy(context),
		Type::getVoidTy(context),
		Type::getVoidTy(context),
		Type::getInt32Ty(context),
		Type::getVoidTy(context),
		Type::getInt64Ty(context),
		Type::getInt64Ty(context)
	};
	debug_errs("\tModule initially visible functions:");
	for (Module::iterator func = M.begin(); func != M.end(); ++func)
	{
		for (int i = 0; i < TOTAL_DECLARED_FUNCTIONS; i++)
		{
			if(func->getName().str() == declared_function_names[i])
			{
				declared_functions[i] = M.getFunction(declared_function_names[i]);
				break;
			}
		}
		debug_errs("\t\t" << func->getName().str());
	}
	for (int i = 0; i < TOTAL_DECLARED_FUNCTIONS; i++)
	{
		if(!declared_functions[i])
		{
			FunctionType* ft;
			if(!declared_function_parameter_types[i].empty())
				ft = FunctionType::get(declared_function_return_types[i],
					declared_function_parameter_types[i], false);
			else
				ft = FunctionType::get(declared_function_return_types[i], false);

			declared_functions[i] = Function::Create(ft, Function::ExternalLinkage,
				declared_function_names[i], &M);
		}
	}
	debug_errs(__func__<< ": Exit");
}


#ifdef DEBUG_PRINT
/**
 * Convenient function that injects printf calls. Useful for inspecting values
 * generated at runtime.
 * @param builder Builder of the Basic Block where the printf call will be added
 * @param msg A string message that will be displayed by printf
 * @param val An optional value to be displayed after msg by printf
 * @param M The current Module Object
 */
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

#ifdef USE_CLANG
//-----------------------------------------------------------------------------
// New PM Registration - use this for loading the pass on a c file with clang
//-----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getAttProfModPluginInfo()
{
	// std::cerr << "getAttProfModPluginInfo() called\n"; // add this line
	return {
		LLVM_PLUGIN_API_VERSION, "AttProfMod", LLVM_VERSION_STRING,
		[](PassBuilder &PB)
		{
			PB.registerOptimizerEarlyEPCallback(
				[](ModulePassManager &MPM, OptimizationLevel)
				{
					MPM.addPass(AttProfMod());
					return true;
				});
		}};
}
#else
// -----------------------------------------------------------------------------
// New PM Registration - use this for loading the pass on an ll file with opt
// -----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getAttProfModPluginInfo()
{
	 //std::cerr << "getAttProfModPluginInfo() called\n"; // add this line
	return
	{
		LLVM_PLUGIN_API_VERSION, "AttProfMod", LLVM_VERSION_STRING,
		[](PassBuilder &PB)
		{
			PB.registerPipelineParsingCallback
			(
				[](StringRef Name, ModulePassManager &MPM,
				ArrayRef<PassBuilder::PipelineElement>)
				{
					if (Name == "AttProf")
					{
						MPM.addPass(AttProfMod());
						return true;
					}
					return false;
				}
			);
		}
	};
}
#endif
// This is the core interface for pass plugins. It guarantees that 'opt' will
// be able to recognize HelloWorld when added to the pass pipeline on the
// command line, i.e. via '-passes=hello-world'
extern "C" LLVM_ATTRIBUTE_WEAK ::llvm::PassPluginLibraryInfo
llvmGetPassPluginInfo()
{
	return getAttProfModPluginInfo();
}
