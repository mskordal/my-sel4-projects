#include "attprof.hpp"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/InlineAsm.h"
#include "llvm/Transforms/Utils/BasicBlockUtils.h"
#include <iostream> //TODO: for debugging. Remove later
#include <fstream>	//to read files with function names
#include <string>
#include <filesystem>
#include <sstream>
#include <cstdint>

using namespace llvm;

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
#define BRAM_ADDR 0xa0040000
/** The virtual address corresponding to the HLS IP. Mapped by the app */
#define HLS_VIRT_ADDR 0x10000000 ///< virtual address for our test app
#define BRAM_VIRT_ADDR 0x10001000 ///< virtual address for our test app
#define DISABLE_EL1_EVENT_COUNT 0x80000000

#define TAKES_VAR_ARGS true
#define SHA256_BLOCK_SIZE 32            // SHA256 outputs a 32 byte digest

/** HLS offsets from the base address. Number is the amount of datatypes to
 * what is in the target address. E.g hash is a byte array, bram is a word */
#define HLS_HASH_OFFSET 32 ///< 32 bytes (0x20)
#define FUNC_DATA_OFFSET 6 ///< 6 words (24 bytes, 0x18)
#define RET_NONCE_OFFSET 4 ///< 4 words (16 bytes, 0x10)
#define BRAM_OFFSET 16 ///< 16 words (64 bytes, 0x40)

void inject_local_allocs_rst_counters(IRBuilder<> *block_builder,
	LLVMContext &ctx);
BasicBlock *create_spinlock_idle_bit_bblk(AllocaInst *ctrl_sig_var,
	Function *function, LLVMContext &ctx, Value **is_idle_bit_0_val);
BasicBlock *create_wr_hash_to_hls_bblk(AllocaInst *ctrl_sig_var, int func_id,
	Function *func, LLVMContext &ctx);
BasicBlock *create_wr_metrics_to_globals_bblk(Function *func, LLVMContext &ctx);
void inject_add_globals_to_locals(BasicBlock *bblk, LLVMContext &ctx);
void inject_add_counters_to_locals(BasicBlock *bblk, LLVMContext &ctx);
Instruction *get_last_map_instr(Function *func);
std::vector<Instruction *> get_call_instrs(std::vector<std::string> &func_names,
	Function *func);
std::vector<Instruction *> get_ret_instrs(Function *func);
void create_extern_func_declarations(Module &M, LLVMContext &ctx);
std::vector<std::vector<uint32_t>> event_shifts_file_to_array(std::ifstream*
	file);
int get_event_id(std::string event_name);
#ifdef DEBUG_PRINT
void insertPrint(IRBuilder<> &builder, std::string msg, Value *val, Module &M);
#endif
#ifdef USE_SEL4BENCH
	std::vector<Instruction *> getTermInst(Function *func);
#endif

cl::opt<std::string> func_name_ifname("functions-file",
	cl::desc("<functions input file>"), cl::Required);
cl::opt<std::string> event_name_ifname("events-file",
	cl::desc("<events input file>"), cl::Required);
cl::opt<std::string> event_shift_ifname("event-shifts-file",
	cl::desc("<event shifts input file>"), cl::Required);

enum event
{
	CPU_CYCLES,
	EVENT0,
	EVENT1,
	EVENT2,
	EVENT3,
	EVENT4,
	EVENT5,
	MAX_EVENTS
};

enum decl_funcs
{
	SEL4BENCH_INIT,
	SEL4BENCH_RESET_COUNTERS,	
	SEL4BENCH_SET_COUNT_EVENT,
	SEL4BENCH_START_COUNTERS,
	SEL4BENCH_PRIVATE_READ_PMCR,
	SEL4BENCH_PRIVATE_WRITE_PMCR,
	SEL4BENCH_GET_CYCLE_COUNT,
	SEL4BENCH_GET_COUNTER,
	CREATE_ATTKEY,
	SHA256_HASH,
	TOTAL_DECL_FUNCS
};

int event_pmu_codes[MAX_EVENTS - 1];
int events_num; ///< The actual number of events profiling. Counted at runtime
std::vector<std::vector<uint32_t>> event_shifts;

GlobalVariable *hls_global_var;
GlobalVariable *bram_global_var;
StructType *attkey_struct_typ;

/** 64bit cpu cycles, 64bit event 0-5 */
GlobalVariable *event_global_vars[MAX_EVENTS]; ///< Globals: events0-5 & cpu
AllocaInst *event_local_vars[MAX_EVENTS]; ///< Locals: events0-5 & cpu

GlobalVariable *counters_glob_arr;
GlobalVariable *event_shift_glob_arr;
GlobalVariable *event_shift_glob_idx;
Type *idx_typ;
ArrayType* arr6_i32_typ;

std::string decl_func_names[TOTAL_DECL_FUNCS] =
{
	"sel4bench_init",
	"sel4bench_reset_counters",
	"sel4bench_set_count_event",
	"sel4bench_start_counters",
	"sel4bench_private_read_pmcr",
	"sel4bench_private_write_pmcr",
	"sel4bench_get_cycle_count",
	"sel4bench_get_counter",
	"create_attkey",
	"sha256_hash"
};
Function *decl_funcs[TOTAL_DECL_FUNCS] = {nullptr};

StructType *attkey_typ;

PreservedAnalyses AttProfMod::run(Module &M, ModuleAnalysisManager &AM)
{
	std::ifstream *func_name_ifstream;
	std::ifstream *event_name_ifstream;
	std::ifstream *event_shift_ifstream;
	std::string line;
	std::vector<std::string> func_names;
	std::vector<int> func_ids;

	if (func_name_ifname.getNumOccurrences() > 0)
		func_name_ifstream = new std::ifstream(func_name_ifname);
	else
	{
		errs() << "List of functions to profile, was not provided";
		exit(1);
	}
	if (event_name_ifname.getNumOccurrences() > 0)
		event_name_ifstream = new std::ifstream(event_name_ifname);
	else
	{
		errs() << "List of events to count, was not provided";
		exit(1);
	}
	if (event_shift_ifname.getNumOccurrences() > 0)
		 event_shift_ifstream = new std::ifstream(event_shift_ifname);
	else
	{
		errs() << "List of events to count, was not provided";
		exit(1);
	}

#ifdef DEBUG_PRINT
	errs() << "DEBUG_PRINT is enabled. printf calls will be instrumented\n";
#endif
#ifdef DEBUG_COMP_PRINT
	errs() <<	"DEBUG_COMP_PRINT is enabled. debug messages will be printed"
				"during compilation\n";
#endif
#ifdef USE_CLANG
	errs() << "aaaaUSE_CLANG is enabled. Pass is built to be passed through clang\n";
#else
	errs() << "USE_CLANG is disabled. Pass is built to be passed through opt\n";
#endif	
#ifdef DEBUG_PRINT
	errs() << "Event shifts file here\n";
#endif
//#ifdef DEBUG_COMP_PRINT
	//errs() << "Event-shift file contents:\n";
	//std::cout << event_shift_ifstream->rdbuf();
//#endif

	while (getline(*func_name_ifstream, line))
	{
		int delim_pos = line.find(" ");
		func_names.push_back(line.substr(0, delim_pos));
		func_ids.push_back(
			stoi(line.substr(delim_pos + 1, line.length() - delim_pos + 1)));
	}

	debug_errs("will now print functions got from functions.txt\n");
	for (int i = 0; i < func_names.size(); i++)
	{
		debug_errs(func_names.at(i) << ": " << func_ids.at(i));
	}
	events_num = 0;
	while (std::getline(*event_name_ifstream, line))
	{
		event_pmu_codes[events_num++] = get_event_id(line);
	}

	event_shifts = event_shifts_file_to_array(event_shift_ifstream);
#ifdef DEBUG_COMP_PRINT
	std::cout << "Event-shift array empty: " << event_shifts.empty() << "\n";
	//for (int i = 0; i < event_shifts.size(); i++)
	//{
		//for (int j = 0; j < event_shifts[i].size(); j++)
		//{
			//std::cout << event_shifts[i][j] << " ";
		//}
		//std::cout << "\n";
	//}
#endif
	// Get the context of the module. Probably the Global Context.
	LLVMContext &ctx = M.getContext();
	
	// Create the struct type for attkey_t
	Type *attkey_elems[] =
	{
		ArrayType::get(Type::getInt32Ty(ctx), 12),
		Type::getInt32Ty(ctx),
		Type::getInt32Ty(ctx)
	};
	attkey_typ = StructType::create(ctx, attkey_elems, "struct.attkey_t");

	create_extern_func_declarations(M, ctx);
	
	std::string absolut_filename = M.getName().str();
	std::string src_fname = std::filesystem::path(absolut_filename).
		filename().replace_extension("").string();
	debug_errs("filename of Module: " << src_fname << "\n");

	// Create the global variable for the hls address
	Type *i64_ptr_typ = Type::getInt64PtrTy(ctx);
	hls_global_var = new GlobalVariable(M, i64_ptr_typ, false,
		GlobalValue::ExternalLinkage, nullptr, src_fname + "-hlsptr");
	ConstantInt *hls_virt_addr_val = ConstantInt::get(ctx,
		APInt(64, HLS_VIRT_ADDR));
	Constant *hls_virt_addr_ptr = ConstantExpr::getIntToPtr(hls_virt_addr_val,
		i64_ptr_typ);
	hls_global_var->setAlignment(Align(8));
	hls_global_var->setInitializer(hls_virt_addr_ptr);

	// Create the global variable for the bram address
	i64_ptr_typ = Type::getInt64PtrTy(ctx);
	bram_global_var = new GlobalVariable(M, i64_ptr_typ, false,
		GlobalValue::ExternalLinkage, nullptr, src_fname + "-bramptr");
	ConstantInt *bram_virt_addr_val = ConstantInt::get(ctx,
		APInt(64, BRAM_VIRT_ADDR));
	Constant *bram_virt_addr_ptr = ConstantExpr::getIntToPtr(bram_virt_addr_val,
		i64_ptr_typ);
	bram_global_var->setAlignment(Align(8));
	bram_global_var->setInitializer(bram_virt_addr_ptr);

	// Global array to get the counter values passed to create_key function
	ArrayType* arr6_i64_typ = ArrayType::get(Type::getInt64Ty(ctx),
		events_num + 1);
	Constant *arr6_i64_init = ConstantAggregateZero::get(arr6_i64_typ);
	counters_glob_arr = new GlobalVariable(M, arr6_i64_typ, true,
		GlobalValue::ExternalLinkage, arr6_i64_init, src_fname + "counters");
	counters_glob_arr->setAlignment(Align(8));

	// Similarly for event shifts. Here we initialise the array with the values
	// since we cant get them dynamically. Pass a row to each create_key call
	// First initialise a type of 1D array of 32-bit ints and size events+1
	arr6_i32_typ = ArrayType::get(Type::getInt32Ty(ctx),
		event_shifts[0].size() - 1);
	// Then init a type of array of these arrays of size number of functions
	ArrayType* arr2d_i32_typ = ArrayType::get(arr6_i32_typ,
		event_shifts.size()); 
	// Make an array of Constants where each constant is a pointer to array
	Constant* event_shift_vals[event_shifts.size()];
	for(int row = 0; row < event_shifts.size(); row++)
	{	// Make an array of constants to pass to each 1D array
		Constant* event_shift_row[event_shifts[0].size() - 1];
		for(int col = 0; col < event_shifts[0].size() - 1; col++)
		{
			// Create a constant to pass to to the 1D array
			event_shift_row[col] = ConstantInt::get(Type::getInt32Ty(ctx),
				event_shifts[row][col]);
		}
		// After 1D array is complete, create the actual 1D array passed to
		// the constant array of arrays
		event_shift_vals[row] = ConstantArray::get(arr6_i32_typ,
			ArrayRef<Constant*>(event_shift_row, event_shifts[0].size() - 1));
	}
	// finally create the 2d array with the constant array of arrays
	Constant *event_shift_arr_init = ConstantArray::get(arr2d_i32_typ,
		ArrayRef<Constant*>(event_shift_vals, event_shifts.size()));
	event_shift_glob_arr = new GlobalVariable(M, arr2d_i32_typ, true,
		GlobalValue::ExternalLinkage, event_shift_arr_init,
		src_fname + "event_shifts");
	event_shift_glob_arr->setAlignment(Align(4));

	// Also create a global index to access each row of the event_shift global
	// array, and increment it after func call 
	idx_typ = Type::getInt32Ty(ctx);
	event_shift_glob_idx = new GlobalVariable(M, idx_typ, false,
		GlobalValue::ExternalLinkage, ConstantInt::get(idx_typ, 0),
		src_fname + "event_shift_i");

	// Create the global variables for the cpu cycles and the 6 events
	// TODO: The counter global array can eventually replace them
	for (int event_idx = 0; event_idx < events_num + 1; event_idx++)
	{
		event_global_vars[event_idx] = new GlobalVariable(M,
			Type::getInt64Ty(ctx), false, GlobalValue::ExternalLinkage,
			nullptr, src_fname + "-ev" + std::to_string(event_idx));
		event_global_vars[event_idx]->setDSOLocal(true);
		event_global_vars[event_idx]->setAlignment(Align(8));
		event_global_vars[event_idx]->setInitializer(
			ConstantInt::get(ctx, APInt(64, 0)));
	}

	Function *main_func = M.getFunction("main");
	if(main_func)
	{
		BasicBlock *main_entry_bblk = &main_func->getEntryBlock();
		IRBuilder<> main_builder(main_entry_bblk,
			main_entry_bblk->begin());
#ifdef DEBUG_PRINT
		insertPrint(main_builder, "HLS virt address: ", hls_virt_addr_val,
			*main_entry_bblk->getModule());
#endif
		// Call counter itialisation functions
		main_builder.CreateCall(decl_funcs[SEL4BENCH_INIT]);

		// For every counter
		for (int event_idx = 0; event_idx < events_num; ++event_idx)
		{
			// Set the corresponding event to count
			Value *event_args[2] =
			{
				ConstantInt::get(ctx, APInt(64, event_idx)),
				ConstantInt::get(ctx, APInt(64, event_pmu_codes[event_idx]))
			};
			main_builder.CreateCall(decl_funcs[SEL4BENCH_SET_COUNT_EVENT],
				event_args);
		}
		debug_errs("main prologue instrumentation done!!");

		// Trigger HLS once in the beginning of main to create nonce
		// Since we access mapped memory, place builder after map call
		Instruction *map_instr = get_last_map_instr(main_func);
		if(map_instr)
		{
			main_builder.SetInsertPoint(map_instr->getNextNode());

			// Write bram address to hls corresponding address
			Value *hls_base_addr = main_builder.CreateLoad(
				hls_global_var->getValueType(), hls_global_var);
			Value *hls_bram_idx = main_builder.CreateConstInBoundsGEP1_32(
				Type::getInt32Ty(ctx), hls_base_addr, BRAM_OFFSET);
			main_builder.CreateStore(ConstantInt::get(ctx, APInt(32,
				BRAM_ADDR)), hls_bram_idx);

			//Trigger top function
			hls_base_addr = main_builder.CreateLoad(
				hls_global_var->getValueType(), hls_global_var);
			Value *ctrl_sig_idx = main_builder.CreateConstInBoundsGEP1_32(
				Type::getInt32Ty(ctx), hls_base_addr, 0);
			LoadInst *ctrl_sig_val = main_builder.CreateLoad(
				Type::getInt32Ty(ctx), ctrl_sig_idx);
			Value *ctrl_sig_val_start = main_builder.CreateOr(ctrl_sig_val,
				ConstantInt::get(ctx, APInt(32, 0x1)));
			main_builder.CreateStore(ctrl_sig_val_start, ctrl_sig_idx);
		}
		// Find returns in main to send terminating signal to hls
		std::vector<Instruction *> ret_instrs = get_ret_instrs(main_func);
		for (Instruction *instr : ret_instrs)
		{
			BasicBlock *bblk = instr->getParent();
			Instruction *firstInstr = bblk->getFirstNonPHI();
			if (firstInstr != instr)
				bblk = SplitBlock(bblk, instr);
			IRBuilder<> builder(bblk, bblk->begin());

			// Write 0 to corresponding hls address denoting the end
			LoadInst *hls_base_addr = builder.CreateLoad(
				hls_global_var->getValueType(), hls_global_var);
			Value *hls_func_data_idx = builder.CreateConstInBoundsGEP1_32(
				Type::getInt32Ty(ctx), hls_base_addr, FUNC_DATA_OFFSET);
			builder.CreateStore(ConstantInt::get(Type::getInt32Ty(ctx), 0),
				hls_func_data_idx);

			// Write BRAM address to HLS
			hls_base_addr = builder.CreateLoad(hls_global_var->getValueType(),
				hls_global_var);
			Value *hls_bram_addr = builder.CreateConstInBoundsGEP1_64(
				Type::getInt32Ty(ctx), hls_base_addr, 16);
			builder.CreateStore(ConstantInt::get(ctx, APInt(32, BRAM_ADDR)),
				hls_bram_addr);

			//Trigger top function
			hls_base_addr = builder.CreateLoad( hls_global_var->getValueType(),
				hls_global_var);
			Value *ctrl_sig_idx = builder.CreateConstInBoundsGEP1_32(
				Type::getInt32Ty(ctx), hls_base_addr, 0);
			LoadInst *ctrl_sig_val = builder.CreateLoad( Type::getInt32Ty(ctx),
				ctrl_sig_idx);
			Value *ctrl_sig_val_start = builder.CreateOr(ctrl_sig_val,
				ConstantInt::get(ctx, APInt(32, 0x1)));
			builder.CreateStore(ctrl_sig_val_start, ctrl_sig_idx);
		}
	}

	for (int i = 0; i < func_names.size(); ++i)
	{
		Value *is_idle_bit_0_val;

		// get function the function
		Function *profile_func = M.getFunction(func_names.at(i));

		// function may not exist or may only be declared in this module. In
		// that case we don't perform any instrumentation
		if(!profile_func) continue;
		if(profile_func->empty()) continue;

		// at the top of the function, allocate locals & reset counters
		BasicBlock *entry_bblk = &profile_func->getEntryBlock();
		IRBuilder<> entry_bblk_builder(entry_bblk, entry_bblk->begin());
		inject_local_allocs_rst_counters(&entry_bblk_builder, ctx);

		// before each call store metrics to resume after the return
		std::vector<Instruction *> call_instrs = get_call_instrs(
			func_names, profile_func);
		for (Instruction *call_instr : call_instrs)
		{
			BasicBlock *call_instr_bblk = call_instr->getParent();
			if (&(call_instr_bblk->front()) != call_instr)
				call_instr_bblk = SplitBlock(call_instr_bblk, call_instr);
			BasicBlock *after_call_instr_bblk = SplitBlock(call_instr_bblk,
				call_instr->getNextNonDebugInstruction());
			inject_add_counters_to_locals(call_instr_bblk, ctx);
			inject_add_globals_to_locals(after_call_instr_bblk, ctx);
		}

		// before each return instruction, add block that write to HLS
		std::vector<Instruction *> ret_instrs = get_ret_instrs(profile_func);
		for (Instruction *ret_instr : ret_instrs)
		{
			BasicBlock *pre_ret_bblk = ret_instr->getParent();
			// if return is not the first block instruction do an extra split
			if (&(pre_ret_bblk->front()) != ret_instr)
				pre_ret_bblk = SplitBlock(pre_ret_bblk, ret_instr);

			// This creates an empty block before the return block to avoid
			// cases where loops from existing blocks will miss the new blocks.
			// ret_bblk is the empty block now and new_ret_bblk contains return.
			BasicBlock *ret_bblk = SplitBlock(pre_ret_bblk, ret_instr);

			Value *is_idle_bit_0_val;
			// allocate o local var in the empty block to read control register
			IRBuilder<> pre_ret_bblk_builder(pre_ret_bblk,
				pre_ret_bblk->begin());
			AllocaInst* ctrl_sig_var = pre_ret_bblk_builder.CreateAlloca(
				IntegerType::getInt32Ty(ctx));
			// Create other blocks
			BasicBlock *wr_metrics_to_globals_bblk =
				create_wr_metrics_to_globals_bblk(profile_func, ctx);
			BasicBlock *spinlock_idle_bit_bblk = create_spinlock_idle_bit_bblk(
				ctrl_sig_var, profile_func, ctx, &is_idle_bit_0_val);
			BasicBlock *wr_hash_to_hls_bblk = create_wr_hash_to_hls_bblk(
				ctrl_sig_var, func_ids.at(i), profile_func, ctx);
			// Connect blocks. Connect the empty block to the first created
			Instruction *pre_ret_bblk_term_instr = pre_ret_bblk->getTerminator();
			BranchInst *br_instr = dyn_cast<BranchInst>(pre_ret_bblk_term_instr);
			br_instr->setSuccessor(0, wr_metrics_to_globals_bblk);

			IRBuilder<> wr_metrics_to_globals_bblk_builder(
				wr_metrics_to_globals_bblk, wr_metrics_to_globals_bblk->end());
			wr_metrics_to_globals_bblk_builder.CreateBr(spinlock_idle_bit_bblk);

			IRBuilder<> spinlock_idle_bit_bblk_builder(spinlock_idle_bit_bblk,
				spinlock_idle_bit_bblk->end());
			spinlock_idle_bit_bblk_builder.CreateCondBr(is_idle_bit_0_val,
				spinlock_idle_bit_bblk, wr_hash_to_hls_bblk);

			IRBuilder<> wr_hash_to_hls_bblk_builder(
				wr_hash_to_hls_bblk, wr_hash_to_hls_bblk->end());
			wr_hash_to_hls_bblk_builder.CreateBr(ret_bblk);
		}
		debug_errs(func_names.at(i) << " instrumentation done!!");
	}
	return llvm::PreservedAnalyses::all();
}

/**
 * Injects instructions at the prologue of a function that is being profiled. It
 * allocates 7 local variables to store counted events and inserts a call to a
 * function that writes to hardware (prolog_func).
 * @param block The block of the function we add the instructions (entry block)
 * @param function_id The id of the Function we want to write to the hardware
 * @param prolog_func The function we want to call in that block
 * @param context The context of current Module
 */
void inject_local_allocs_rst_counters(IRBuilder<> *builder, LLVMContext &ctx)
{
	debug_errs(__func__<< ": " << builder->GetInsertBlock()->
		getParent()->getName().str() << ": Enter");
#ifdef DEBUG_PRINT
	insertPrint(*builder,  std::string(__func__) + " <<" + builder->
		GetInsertBlock()->getParent()->getName().str() + ">>: START", nullptr,
		*builder->GetInsertBlock()->getModule());
#endif
	// allocate local variables to hold counter results
	for (int i = 0; i < events_num + 1; ++i)
	{
		event_local_vars[i] = builder->CreateAlloca(Type::getInt64Ty(ctx));
		builder->CreateStore(ConstantInt::get(ctx, APInt(64, 0)),
			event_local_vars[i]);
	}

	// Insert instruction/data sync barriers
	InlineAsm *dsb_asm = InlineAsm::get(FunctionType::get(Type::getVoidTy(ctx),
		false), "dsb sy", "", true);
	InlineAsm *isb_asm = InlineAsm::get(FunctionType::get(Type::getVoidTy(ctx),
		false), "isb", "", true);
	builder->CreateCall(dsb_asm);
	builder->CreateCall(isb_asm);

	// reset/start counters
	builder->CreateCall(decl_funcs[SEL4BENCH_RESET_COUNTERS]);
	Value *s4bsceArg[1] = {ConstantInt::get(ctx, APInt(64, 0x3F))};
	builder->CreateCall(decl_funcs[SEL4BENCH_START_COUNTERS], s4bsceArg);
	CallInst *readPmcr = builder->CreateCall(
		decl_funcs[SEL4BENCH_PRIVATE_READ_PMCR]);
	Value *cycleCounter = builder->CreateOr(readPmcr,
		ConstantInt::get(ctx, APInt(32, 0x4)));
	Value *s4bpwpArg[1] = {cycleCounter};
	builder->CreateCall(decl_funcs[SEL4BENCH_PRIVATE_WRITE_PMCR], s4bpwpArg);
#ifdef DEBUG_PRINT
	insertPrint(*builder,  std::string(__func__) + " <<" + builder->
		GetInsertBlock()->getParent()->getName().str() + ">>: END", nullptr,
		*builder->GetInsertBlock()->getModule());
#endif
	debug_errs(__func__<< ": " << builder->GetInsertBlock()->getParent()->
		getName().str() << ": Enter");
}


/**
 * Builds a Block that spins on the idle bit of the control signals of the HLS
 * IP. Must keep spinning until the IP is idle.
 * @param nextBblk We insert our new block before that block
 * @param func The function upon we build the block
 * @param context The context of current Module
 * @return The block created
 */
BasicBlock *create_spinlock_idle_bit_bblk(AllocaInst *ctrl_sig_var,
	Function *func, LLVMContext &ctx, Value **is_idle_bit_0_val)
{
	debug_errs(__func__<< ": " << func->getName().str() << ": Enter");
	BasicBlock *bblk = BasicBlock::Create(ctx, "", func);
	IRBuilder<> builder(bblk, bblk->begin());
#ifdef DEBUG_PRINT
	insertPrint(builder,  std::string(__func__) + " <<" + func->getName().str()
		+ ">>: START", nullptr, *bblk->getModule());
#endif
	debug_errs(__func__<< ": " << func->getName().str() << ": Enter");
#ifdef USE_HLS
	LoadInst *hls_base_addr = builder.CreateLoad(hls_global_var->getValueType(),
		hls_global_var);
	Value *hls_ctrl_addr = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(ctx), hls_base_addr, 0);
	LoadInst *hls_ctrl_val = builder.CreateAlignedLoad(
		IntegerType::getInt32Ty(ctx), hls_ctrl_addr, MaybeAlign(4));
	builder.CreateStore(hls_ctrl_val, ctrl_sig_var);
	Value *idle_bit_val = builder.CreateAnd(hls_ctrl_val, 4);
	Value *is_idle_bit_1_val = builder.CreateICmpNE(idle_bit_val,
		ConstantInt::get(ctx, APInt(32, 0)));
	*is_idle_bit_0_val = builder.CreateXor(is_idle_bit_1_val,
		ConstantInt::getTrue(ctx));
#else
	*is_idle_bit_0_val = builder.CreateXor(ConstantInt::getTrue(ctx),
		ConstantInt::getTrue(ctx));
#endif
#ifdef DEBUG_PRINT
	insertPrint(builder,  std::string(__func__) + " <<" + func->getName().str()
		+ ">>: END", nullptr, *bblk->getModule());
#endif
	debug_errs("in " << __func__  <<": Exit");
	return bblk;
}


/**
 * Builds a Block that writes to the HLS IP, CPU cycles and the 6 events counted
 * of a function before it returns. We write 0 to function id parameter to mark
 * the end of the function.
 * @param nextBblk We insert our new block before that block
 * @param func The function upon we build the block
 * @param ctx The context of current Module
 * @return The block created
 */
BasicBlock *create_wr_hash_to_hls_bblk(AllocaInst *ctrl_sig_var, int func_id,
	Function *func, LLVMContext &ctx)
{
	BasicBlock *bblk = BasicBlock::Create(ctx, "", func);
	IRBuilder<> builder(bblk, bblk->begin());

#ifdef DEBUG_PRINT
	insertPrint(builder,  std::string(__func__) + " <<" + func->getName().str()
		+ ">>: START", nullptr, *bblk->getModule());
#endif
	debug_errs(__func__<< ": " << func->getName().str() << ": Enter");

	LoadInst *hls_base_addr = builder.CreateLoad(
		hls_global_var->getValueType(), hls_global_var);
	Value *hls_nonce_addr = builder.CreateConstInBoundsGEP1_32(
		Type::getInt32Ty(ctx), hls_base_addr, RET_NONCE_OFFSET);
	Value *nonce_val = builder.CreateLoad(Type::getInt32Ty(ctx),
		hls_nonce_addr);

#ifdef DEBUG_PRINT
	insertPrint(builder, "nonce value:", nonce_val, *bblk->getModule());
#endif

	// assign counter results to counter global array
	for(int i = 0; i < events_num; i++)
	{
		Value *arr_idx_addr = builder.CreateConstInBoundsGEP1_32(
			IntegerType::getInt64Ty(ctx), counters_glob_arr, i);
		Value *counter_val = builder.CreateLoad(Type::getInt64Ty(ctx),
			event_global_vars[i+1]);
		builder.CreateStore(counter_val, arr_idx_addr);
	}
	// get the index to the correct row
	Value *idx_val = nullptr;
	for(int i = 0; i < event_shifts.size(); i++)
	{
		if(func_id == event_shifts[i][events_num])
		{
			idx_val = (ConstantInt::get(Type::getInt32Ty(ctx), i));
			break;
		}
	}
	// get the ptr to the event shift row
	//Value *idx_val = builder.CreateLoad(idx_typ, event_shift_glob_idx);
	Value *event_shift_row = builder.CreateGEP(arr6_i32_typ,
		event_shift_glob_arr, {idx_val});

	// Create struct for key and get pointer to it
	AllocaInst *attkey_var = builder.CreateAlloca(attkey_typ);
	Value *attkey_ptr = builder.CreateBitCast(attkey_var,
		Type::getInt8PtrTy(ctx), "attkey_ptr");

	// Generate hash result
	builder.CreateCall(decl_funcs[CREATE_ATTKEY],
		{ attkey_ptr, counters_glob_arr, event_shift_row });
	CallInst *res_hash = builder.CreateCall(decl_funcs[SHA256_HASH],
		{attkey_var, nonce_val});

	// Increment the index value
	//Value *idx_val_plus = builder.CreateAdd(idx_val,
		//ConstantInt::get(idx_typ, 1));
	//builder.CreateStore(idx_val_plus, event_shift_glob_idx);

	// Write hash result to hls corresponding bytes
	for(int i = 0; i < SHA256_BLOCK_SIZE; i++)
	{
		Value *res_hash_idx = builder.CreateConstInBoundsGEP1_32(
			Type::getInt8Ty(ctx), res_hash, i);
		Value *hash_byte = builder.CreateLoad(Type::getInt8Ty(ctx),
			res_hash_idx);
		hls_base_addr = builder.CreateLoad(
			hls_global_var->getValueType(), hls_global_var);
		Value *hls_hash_idx = builder.CreateConstInBoundsGEP1_32(
			Type::getInt8Ty(ctx), hls_base_addr, i + HLS_HASH_OFFSET);
		builder.CreateStore(hash_byte, hls_hash_idx);
	}
	// Write function id to corresponding hls address
	Value *func_id_val = (ConstantInt::get(Type::getInt32Ty(ctx), func_id));
	Value *hls_func_data_idx = builder.CreateConstInBoundsGEP1_32(
		Type::getInt32Ty(ctx), hls_base_addr, FUNC_DATA_OFFSET);
	builder.CreateStore(func_id_val, hls_func_data_idx);

	// Write bram address to hls corresponding address
	hls_base_addr = builder.CreateLoad(
		hls_global_var->getValueType(), hls_global_var);
	Value *hls_bram_idx = builder.CreateConstInBoundsGEP1_32(
		Type::getInt32Ty(ctx), hls_base_addr, BRAM_OFFSET);
	builder.CreateStore(ConstantInt::get(ctx, APInt(32, BRAM_ADDR)), hls_bram_idx);

	// Trigger hls top function by writing the start bit of ctrl signals
	hls_base_addr = builder.CreateLoad(
		hls_global_var->getValueType(), hls_global_var);
	Value *ctrl_sig_idx = builder.CreateConstInBoundsGEP1_32(
		Type::getInt32Ty(ctx), hls_base_addr, 0);
	LoadInst *ctrl_sig_val = builder.CreateLoad(
		Type::getInt32Ty(ctx), ctrl_sig_idx);
	Value *ctrl_sig_val_start = builder.CreateOr(ctrl_sig_val,
		ConstantInt::get(ctx, APInt(32, 0x1)));
	builder.CreateStore(ctrl_sig_val_start, ctrl_sig_idx);

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
 * @param bblk We add instruction to this block
 * @param ctx The context of current Module
 */
void inject_add_globals_to_locals(BasicBlock *bblk, LLVMContext &ctx)
{
	IRBuilder<> builder(bblk, bblk->begin());
#ifdef DEBUG_PRINT
	insertPrint(builder,  std::string(__func__) + " <<" + bblk->getParent()->
		getName().str() + ">>: START", nullptr, *bblk->getModule());
#endif
	debug_errs(__func__<< ": " << bblk->getParent()->getName().str() << ": Enter");
	for (int i = 0; i < events_num + 1; ++i)
	{
		LoadInst *event_global_val = builder.CreateLoad(
			event_global_vars[i]->getValueType(), event_global_vars[i]);
		LoadInst *event_local_val = builder.CreateLoad(
			event_local_vars[i]->getAllocatedType(), event_local_vars[i]);
		Value *event_sum_val = builder.CreateAdd(event_local_val, event_global_val);
		builder.CreateStore(event_sum_val, event_local_vars[i]);
	}
	builder.CreateCall(decl_funcs[SEL4BENCH_RESET_COUNTERS]);
	Value *s4bsceArg[1] = {ConstantInt::get(ctx, APInt(64, 0x3F))};
	builder.CreateCall(decl_funcs[SEL4BENCH_START_COUNTERS], s4bsceArg);
	CallInst *readPmcr = builder.CreateCall(
		decl_funcs[SEL4BENCH_PRIVATE_READ_PMCR]);
	Value *cycle_counter_reset_val = builder.CreateOr(readPmcr,
		ConstantInt::get(ctx, APInt(32, 0x4)));
	Value *s4bpwpArg[1] = {cycle_counter_reset_val};
	builder.CreateCall(decl_funcs[SEL4BENCH_PRIVATE_READ_PMCR],
		s4bpwpArg);
#ifdef DEBUG_PRINT
	insertPrint(builder,  std::string(__func__) + " <<" + bblk->getParent()->
		getName().str() + ">>: END", nullptr, *bblk->getModule());
#endif
	debug_errs(__func__<< ": " << bblk->getParent()->getName().str() << ": Exit");
}

/**
 * Takes a block that contains a call instruction. Before the call it reads
 * current events and cpu cycles running and stores to local variables.
 * @param bblk We add instruction to this block
 * @param ctx The context of current Module
 */
void inject_add_counters_to_locals(BasicBlock *bblk, LLVMContext &ctx)
{
	IRBuilder<> builder(bblk, bblk->begin());
#ifdef DEBUG_PRINT
	insertPrint(builder,  std::string(__func__) + " <<" + bblk->getParent()->
		getName().str() + ">>: START", nullptr, *bblk->getModule());
#endif
	debug_errs(__func__<< ": " << bblk->getParent()->getName().str() << ": Enter");
	CallInst *countEventVals[events_num + 1];

	// Put synchronisation barriers before collecting results
	InlineAsm *dsb_asm = InlineAsm::get(FunctionType::get(Type::getVoidTy(ctx),
		false), "dsb sy", "", true);
	InlineAsm *isb_asm = InlineAsm::get(FunctionType::get(Type::getVoidTy(ctx),
		false), "isb", "", true);
	builder.CreateCall(dsb_asm);
	builder.CreateCall(isb_asm);

	// Create call inst to sel4bench_get_cycle_count to get cpu cycles count
	countEventVals[CPU_CYCLES] = builder.CreateCall(
		decl_funcs[SEL4BENCH_GET_CYCLE_COUNT]);

	// Create call inst to sel4bench_get_counter to get the other event counts
	for (int i = 0; i < events_num; ++i)
	{
		Value *s4bgcArg[1] = {ConstantInt::get(ctx, APInt(64, i))};
		countEventVals[i + 1] = builder.CreateCall(
			decl_funcs[SEL4BENCH_GET_COUNTER], s4bgcArg);
	}
	// For each count, load local, add with recent counts, store back to local
	for (int i = 0; i < events_num + 1; ++i)
	{
		LoadInst *localEventVal = builder.CreateLoad(
			event_local_vars[i]->getAllocatedType(), event_local_vars[i]);
		Value *totalEventVal = builder.CreateAdd(localEventVal,
			countEventVals[i]);
		builder.CreateStore(totalEventVal, event_local_vars[i]);
	}
	#ifdef DEBUG_PRINT
	insertPrint(builder,  std::string(__func__) + " <<" + bblk->getParent()->getName().str() + ">>: END",
		nullptr, *bblk->getModule());
#endif
	debug_errs(__func__<< ": " << bblk->getParent()->getName().str() << ": Exit");
}


/**
 * Takes a block that contains a return instruction. Before that instruction, it
 * adds each local event var and the perf counter of the same event, and stores
 * the sum to the corresponding global variable.
 * @param nextBblk We insert our new block before that block
 * @param func The function upon we build the block
 * @param ctx The context of current Module
 */
BasicBlock *create_wr_metrics_to_globals_bblk(Function *func, LLVMContext &ctx)
{
	BasicBlock *bblk = BasicBlock::Create(ctx, "", func);
	IRBuilder<> builder(bblk, bblk->begin());

#ifdef DEBUG_PRINT
	insertPrint(builder,  std::string(__func__) + " <<" + func->getName().str()
		+ ">>: START", nullptr, *bblk->getModule());
#endif
	debug_errs(__func__<< ": " << func->getName().str() << ": Enter");

	// Put synchronisation barriers before collecting results
	InlineAsm *dsb_asm = InlineAsm::get(FunctionType::get(Type::getVoidTy(ctx),
		false), "dsb sy", "", true);
	InlineAsm *isb_asm = InlineAsm::get(FunctionType::get(Type::getVoidTy(ctx),
		false), "isb", "", true);
	builder.CreateCall(dsb_asm);
	builder.CreateCall(isb_asm);

	CallInst *countEventVals[events_num + 1];
	// Create call inst to sel4bench_get_cycle_count to get cpu cycles count
	countEventVals[CPU_CYCLES] = builder.CreateCall(
		decl_funcs[SEL4BENCH_GET_CYCLE_COUNT]);

	// Create call inst to sel4bench_get_counter to get the other event counts
	for (int i = 0; i < events_num; ++i)
	{
		Value *s4bgcArg[1] = {ConstantInt::get(ctx, APInt(64, i))};
		countEventVals[i + 1] = builder.CreateCall(
			decl_funcs[SEL4BENCH_GET_COUNTER], s4bgcArg);
	}
	// For each count, load local, add with recent counts, store back to local
	Function::arg_iterator  arg = func->arg_begin();
	for (int i = 0; i < events_num + 1; ++i)
	{
		LoadInst *localEventVal = builder.CreateLoad(
			event_local_vars[i]->getAllocatedType(), event_local_vars[i]);
		Value *totalEventVal = builder.CreateAdd(localEventVal,
			countEventVals[i]);
		builder.CreateStore(totalEventVal, event_global_vars[i]);
	}
#ifdef DEBUG_PRINT
	insertPrint(builder,  std::string(__func__) + " <<" + func->getName().str()
		+ ">>: END", nullptr, *bblk->getModule());
#endif
	debug_errs(__func__<< ": " << func->getName().str() << ": Exit");
	return bblk;
}


Instruction *get_last_map_instr(Function *func)
{
	bool first_map_found = false;
	Instruction *last_map_instr = nullptr;
	for (Function::iterator bblk = func->begin(); bblk != func->end(); ++bblk)
	{
		for (BasicBlock::iterator instr = bblk->begin(); instr != bblk->end(); ++instr)
		{
			CallInst *call_instr = dyn_cast<CallInst>(&*instr);
			if (!call_instr)
				continue;
			if(call_instr->getCalledFunction()->getName().str() != "sel4utils_map_page")
				continue;
			last_map_instr = call_instr;
		}
	}
	return last_map_instr;
}


/**
 * Iterates over a function's basic blocks searching for call instructions which
 * call functions that match the name of the functions we want to instrument.
 * Finally it returns a vector with those instructions.
 * @param func_names A vector containing the function names of which the call
 * instructions must match with
 * @param func The function of which we iterate over the basic blocks and
 * instructions
 * @return The vector of the maching call instructions
 */
std::vector<Instruction *> get_call_instrs(
	std::vector<std::string> &func_names, Function *func)
{
	debug_errs(__func__<< ": " << func->getName().str() << ": Enter");
	std::vector<Instruction *> call_instrs;

	for (Function::iterator basic_bblk = func->begin();
		basic_bblk != func->end(); ++basic_bblk)
	{
		for (BasicBlock::iterator instruction = basic_bblk->begin();
			instruction != basic_bblk->end(); ++instruction)
		{
			CallInst *call_instr = dyn_cast<CallInst>(&*instruction);
			if (!call_instr)
				continue;
			std::vector<std::string>::iterator func_names_iter;
			Function *call_instr_func =
				call_instr->getCalledFunction();
			if(!call_instr_func)
				continue;
			std::string call_instr_func_name =
			call_instr->getCalledFunction()->getName().str();
			func_names_iter = std::find(func_names.begin(),
				func_names.end(), call_instr_func_name);
			if (func_names_iter == func_names.end())
				continue;
			call_instrs.push_back(&*instruction);
#ifdef DEBUG_PRINT
			IRBuilder<> builder(&*basic_bblk, (&*basic_bblk)->begin());
			insertPrint(builder,  std::string(__func__) + " <<" +
				func->getName().str() + ">>: call found to " +
				call_instr_func_name,	nullptr, *basic_bblk->getModule());
#endif
		}
	}
	debug_errs(__func__<< ": " << func->getName().str() << ": Exit");
	return call_instrs;
}

/**
 * Iterates over a function's basic blocks searching for return instructions and
 * returns a vector with those instructions.
 * @param func The function of which we iterate over the basic blocks and
 * instructions
 * @return The vector of the maching return instructions
 */
std::vector<Instruction *> get_ret_instrs(Function *func)
{
	debug_errs(__func__<< ": " << func->getName().str() << ": Enter");

	std::vector<Instruction *> ret_instrs;
	for (Function::iterator bblk = func->begin(); bblk != func->end(); ++bblk)
	{
		for (BasicBlock::iterator instr = bblk->begin(); instr != bblk->end(); ++instr)
		{
			ReturnInst *ret_instr = dyn_cast<ReturnInst>(&*instr);
			if (ret_instr)
			{
#ifdef DEBUG_PRINT
				IRBuilder<> builder(&*bblk, (&*bblk)->begin());
				insertPrint(builder,  std::string(__func__) + " <<" +
					func->getName().str() + ">>: return found!", nullptr,
					*bblk->getModule());
#endif
				ret_instrs.push_back(&*instr);
			}
		}
	}
	debug_errs(__func__<< ": " << func->getName().str() << ": Exit");
	return ret_instrs;
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
 * Iterates over the SeL4 function names in @c decl_func_names and for every
 * function that is not visible in the module, it creates an extern
 * declaration to it, so that it can be compiled successfully before linking
 * phase.
 * @param M The current Module Object
 * @param ctx The context of current Module
 */
void create_extern_func_declarations(Module &M, LLVMContext &ctx)
{
	debug_errs(__func__<< ": Enter");
	std::vector<std::vector<Type*>> decl_func_param_typ
	{
		{},
		{},
		{Type::getInt64Ty(ctx)},
		{Type::getInt64Ty(ctx), Type::getInt64Ty(ctx)},
		{},
		{Type::getInt32Ty(ctx)},
		{},
		{Type::getInt64Ty(ctx)},
		{Type::getInt64PtrTy(ctx), Type::getInt32PtrTy(ctx)},
		{attkey_typ, Type::getInt32Ty(ctx)}
	};
	Type* decl_func_ret_types[TOTAL_DECL_FUNCS] =
	{
		Type::getVoidTy(ctx),
		Type::getVoidTy(ctx),
		Type::getVoidTy(ctx),
		Type::getVoidTy(ctx),
		Type::getInt32Ty(ctx),
		Type::getVoidTy(ctx),
		Type::getInt64Ty(ctx),
		Type::getInt64Ty(ctx),
		attkey_typ,
		Type::getInt8PtrTy(ctx)
	};
	debug_errs("\tModule initially visible functions:");
	for (Module::iterator func = M.begin(); func != M.end(); ++func)
	{
		for (int i = 0; i < TOTAL_DECL_FUNCS; i++)
		{
			if(func->getName().str() == decl_func_names[i])
			{
				decl_funcs[i] = M.getFunction(decl_func_names[i]);
				break;
			}
		}
		debug_errs("\t\t" << func->getName().str());
	}
	for (int i = 0; i < TOTAL_DECL_FUNCS; i++)
	{
		if(!decl_funcs[i])
		{
			FunctionType* ft;
			if(!decl_func_param_typ[i].empty())
				ft = FunctionType::get(decl_func_ret_types[i],
					decl_func_param_typ[i], false);
			else
				ft = FunctionType::get(decl_func_ret_types[i], false);

			decl_funcs[i] = Function::Create(ft, Function::ExternalLinkage,
				decl_func_names[i], &M);
		}
	}
	debug_errs(__func__<< ": Exit");
}


std::vector<std::vector<uint32_t>> event_shifts_file_to_array(std::ifstream* file)
{
	using namespace std;
	vector<vector<uint32_t>> arr;
	string line;
	string val;

	while(getline(*file, line))
	{
		vector<uint32_t> row;
		stringstream line_stream(line);
		while(getline(line_stream, val, ','))
		{
			row.push_back(stoul(val));
		}
		arr.push_back(row);
	}
	return arr;
}


int get_event_id(std::string event_name)
{
	if(event_name == "SW_INCR")					return 0x00;
	if(event_name == "L1I_CACHE_REFILL")		return 0x01;
	if(event_name == "L1I_TLB_REFILL")			return 0x02;
	if(event_name == "L1D_CACHE_REFILL")		return 0x03;
	if(event_name == "L1D_CACHE")				return 0x04;
	if(event_name == "L1D_TLB_REFILL")			return 0x05;
	if(event_name == "LD_RETIRED")				return 0x06;
	if(event_name == "ST_RETIRED")				return 0x07;
	if(event_name == "INST_RETIRED")			return 0x08;
	if(event_name == "EXC_TAKEN")				return 0x09;
	if(event_name == "EXC_RETURN")				return 0x0A;
	if(event_name == "CID_WRITE_RETIRED")		return 0x0B;
	if(event_name == "PC_WRITE_RETIRED")		return 0x0C;
	if(event_name == "BR_IMMED_RETIRED")		return 0x0D;
	if(event_name == "UNALIGNED_LDST_RETIRED")	return 0x0F;
	if(event_name == "BR_MIS_PRED")				return 0x10;
	if(event_name == "CPU_CYCLES")				return 0x11;
	if(event_name == "BR_PRED")					return 0x12;
	if(event_name == "MEM_ACCESS")				return 0x13;
	if(event_name == "L1I_CACHE")				return 0x14;
	if(event_name == "L1D_CACHE_WB")			return 0x15;
	if(event_name == "L2D_CACHE")				return 0x16;
	if(event_name == "L2D_CACHE_REFILL")		return 0x17;
	if(event_name == "L2D_CACHE_WB")			return 0x18;
	if(event_name == "BUS_ACCESS")				return 0x19;
	if(event_name == "MEMORY_ERROR")			return 0x1A;
	if(event_name == "BUS_CYCLES")				return 0x1D;
	if(event_name == "CHAIN")					return 0x1E;
	if(event_name == "BUS_ACCESS_LD")			return 0x60;
	if(event_name == "BUS_ACCESS_ST")			return 0x61;
	if(event_name == "BR_INDIRECT_SPEC")		return 0x7A;
	if(event_name == "EXC_IRQ")					return 0x86;
	if(event_name == "EXC_FIQ")					return 0x87;
	return -1;
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
	return
	{
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
					if (Name == "attprof")
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
