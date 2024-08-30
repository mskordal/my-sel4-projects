#include "AccProf.h"
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
#include <cstdlib>
#include <filesystem>

using namespace llvm;

#ifdef DEBUG_COMP_PRINT
	#define debug_errs(str) do { errs() << str << "\n"; } while(0)
#else
	#define debug_errs(str)
#endif

#define BRAM_ADDRESS 0xa0040000
#define HLS_VIRT_ADDR 0x10000000 ///< virtual address for our test app

#define COUNTERS_NUM 6


void inject_local_allocs_rst_counters(BasicBlock *bblk, int func_id,
	Function *prolog_func, LLVMContext &ctx);
BasicBlock *create_spinlock_idle_bit_bblk(BasicBlock *nextBblk, Function *func,
	LLVMContext &ctx, Value **is_idle_bit_0_val);
BasicBlock *create_wr_fid_to_hls_bblk(AllocaInst *ctrl_sig_var, Function *func,
	LLVMContext &ctx, int id);
BasicBlock *create_wr_metrics_to_hls_bblk(AllocaInst *ctrl_sig_var,
	Function *func, LLVMContext &ctx);
BasicBlock *create_wr_metrics_to_globals_bblk(Function *func, LLVMContext &ctx);
void inject_add_globals_to_locals(BasicBlock *bblk, LLVMContext &ctx);
void inject_add_counters_to_locals(BasicBlock *bblk, LLVMContext &ctx);
std::vector<Instruction *> get_call_instrs(std::vector<std::string> &func_names,
	Function *func);
std::vector<Instruction *> get_ret_instrs(Function *func);
Function *create_prolog_func(int funcID, Module &M, LLVMContext &ctx);
Function* create_epilog_func(Module &M, LLVMContext &ctx);
void create_extern_func_declarations(Module &M, LLVMContext &ctx);
int get_event_id(std::string event_name);
#ifdef DEBUG_PRINT
void insertPrint(IRBuilder<> &builder, std::string msg, Value *val, Module &M);
#endif
#ifdef USE_SEL4BENCH
	std::vector<Instruction *> getTermInst(Function *func);
#endif

//cl::opt<std::string> InputFilename(cl::Positional, cl::desc("<input file>"), cl::Required);
cl::opt<std::string> func_name_ifname("functions-file",
	cl::desc("<functions input file>"), cl::Required);
cl::opt<std::string> event_name_ifname("events-file",
	cl::desc("<events input file>"), cl::Required);
cl::opt<std::string> prof_meta_ifname("prof-meta-file",
	cl::desc("<profiling metadata input file>"));

enum eventIndices
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

enum decl_funcs
{
#ifndef USE_HLS
	accprofSoftTopFunc,
	accprofSoftPrintFunc,
#endif
	SEL4BENCH_INIT,
	SEL4BENCH_RESET_COUNTERS,	
	SEL4BENCH_SET_COUNT_EVENT,
	SEL4BENCH_START_COUNTERS,
	SEL4BENCH_PRIVATE_READ_PMCR,
	SEL4BENCH_PRIVATE_WRITE_PMCR,
	SEL4BENCH_GET_CYCLE_COUNT,
	SEL4BENCH_GET_COUNTER,
	TOTAL_DECL_FUNCS
};

int event_pmu_codes[TOTAL_EVENTS - 1];
int func_calls; ///< set to 1 on 1st prof exec. After that set to real num

GlobalVariable *hls_global_var;

/** 64bit cpu cycles, 64bit event 0-5 */
GlobalVariable *event_global_vars[TOTAL_EVENTS]; ///< Globals: events0-5 & cpu
AllocaInst *event_local_vars[TOTAL_EVENTS]; ///< Locals: events0-5 & cpu
//GlobalVariable *event_shift_glob_arr;
//ArrayType* arr6_i32_typ;



std::string decl_func_names[TOTAL_DECL_FUNCS] =
{
#ifndef USE_HLS
	"accprof_soft_top_func",
	"accprof_soft_print",
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
Function *decl_funcs[TOTAL_DECL_FUNCS] = {nullptr};

//StructType *attkey_typ;

PreservedAnalyses AccProfMod::run(Module &M, ModuleAnalysisManager &AM)
{
	std::ifstream *func_name_ifstream;
	std::ifstream *event_name_ifstream;
	std::ifstream *prof_meta_ifstream;
	std::string line;
	std::vector<std::string> func_names;
	std::vector<int> func_ids;
	//std::vector<uint32_t> event_shifts;

	if (func_name_ifname.getNumOccurrences() > 0)
	{
		func_name_ifstream = new std::ifstream(func_name_ifname);
		while (std::getline(*func_name_ifstream, line))
		{
			int delim_pos = line.find(" ");
			func_names.push_back(line.substr(0, delim_pos));
			func_ids.push_back(std::stoi(line.substr(delim_pos + 1,
				line.length() - delim_pos + 1)));
		}
		errs() << "List of functions to be profiled:";
		for (int i = 0; i < func_names.size(); i++)
		{
			errs() << func_names.at(i) << ": " << func_ids.at(i);
		}
	}
	else
	{
		errs() << "List of functions to profile was not provided";
		exit(1);
	}
	if (event_name_ifname.getNumOccurrences() > 0)
	{
		event_name_ifstream = new std::ifstream(event_name_ifname);
		int i = 0;
		while (std::getline(*event_name_ifstream, line))
		{
			event_pmu_codes[i++] = get_event_id(line);
		}
		errs() << "List of events to be counted:";
		for (i = 0; i < COUNTERS_NUM; i++)
		{
			errs() << "event 0: " << event_pmu_codes[i];
		}
	}
	else
	{
		errs() << "List of events to count was not provided";
		exit(1);
	}
	if (prof_meta_ifname.getNumOccurrences() > 0)
	{
		prof_meta_ifstream = new std::ifstream(prof_meta_ifname);
		std::getline(*prof_meta_ifstream, line);
		func_calls = std::stoi(line);
		errs() << "List of profling metadata:";
		errs() << "func_calls: " << func_calls;
	}
	else
	{
		func_calls = 1;
		errs() << "List of profling metadata was not provided";
		errs() << "func_calls: 1";
	}

#ifdef DEBUG_PRINT
	errs() << "DEBUG_PRINT is enabled. printf calls will be instrumented\n";
#endif
#ifdef DEBUG_COMP_PRINT
	errs() <<	"DEBUG_COMP_PRINT is enabled. debug messages will be printed"
				"during compilation\n";
#endif
#ifdef USE_HLS
	errs() << "USE_HLS is enabled. Pass will rd/wr to HLS\n";
#else
	errs() << "USE_HLS is disabled. pass will rd/wr to AccProf-soft\n";
#endif	
#ifdef USE_CLANG
	errs() << "USE_CLANG is enabled. Pass is used with clang\n";
#else
	errs() << "USE_CLANG is disabled. Pass is used with opt\n";
#endif
	// Get the context of the module. Probably the Global Context.
	LLVMContext &ctx = M.getContext();
	
	create_extern_func_declarations(M, ctx);
	
	Type *i64_ptr_typ = Type::getInt64PtrTy(ctx);

	std::string absolut_filename = M.getName().str();
	std::string src_filename = std::filesystem::path(absolut_filename).
		filename().replace_extension("").string();
	debug_errs("filename of Module: " << src_filename << "\n");
	// Create a global ptr var with external linkage and a null initializer.
	hls_global_var = new GlobalVariable(M, i64_ptr_typ, false,
		GlobalValue::ExternalLinkage, nullptr, src_filename + "-hlsptr");
	// Create a constant 64-bit integer value.
	// APInt is an object that represents an integer of specified bitwidth
	ConstantInt *Int64Val = ConstantInt::get(ctx, APInt(64, HLS_VIRT_ADDR));
	// Convert the integer value to a pointer. Basically a typecast
	Constant *PtrVal = ConstantExpr::getIntToPtr(Int64Val, i64_ptr_typ);
	// Align global variable to 8 bytes
	hls_global_var->setAlignment(Align(8));
	// Set the initial value of the global variable. Overwrites if existing
	hls_global_var->setInitializer(PtrVal);

	// Initialize a random global array of 6 elements for event shifts
	//arr6_i32_typ = ArrayType::get(Type::getInt32Ty(ctx), COUNTERS_NUM);
	//Constant *event_shift_arr_init = ConstantArray::get(arr6_i32_typ,
		//{
			//ConstantInt::get(ctx, APInt(32, rand() % 8)),
			//ConstantInt::get(ctx, APInt(32, rand() % 8)),
			//ConstantInt::get(ctx, APInt(32, rand() % 8)),
			//ConstantInt::get(ctx, APInt(32, rand() % 8)),
			//ConstantInt::get(ctx, APInt(32, rand() % 8)),
			//ConstantInt::get(ctx, APInt(32, rand() % 8))
		//});
	//event_shift_glob_arr = new GlobalVariable(M, arr6_i32_typ, true,
		//GlobalValue::ExternalLinkage, event_shift_arr_init, "event_shifts");
	//event_shift_glob_arr->setAlignment(Align(4));

	// Create the global variables for the cpu cycles and the 6 events
	for (int i = 0; i < TOTAL_EVENTS; i++)
	{
		event_global_vars[i] = new GlobalVariable(M, Type::getInt64Ty(ctx),
			false, GlobalValue::ExternalLinkage, nullptr,
			src_filename + "-ev" + std::to_string(i));
		event_global_vars[i]->setDSOLocal(true);
		event_global_vars[i]->setAlignment(Align(8));
		event_global_vars[i]->setInitializer(
			ConstantInt::get(ctx, APInt(64, 0)));
	}
	Function* prolog_func = create_prolog_func(0, M, ctx);
	Function* epilog_func = create_epilog_func(M, ctx);

	Function *main_func = M.getFunction("main");

	// In main function initialize counters and set them to specific events 
	if(main_func)
	{
		BasicBlock *mainFirstBblk = &main_func->getEntryBlock();

		IRBuilder<> mainBuilder(mainFirstBblk, mainFirstBblk->begin());

#ifdef DEBUG_PRINT
		insertPrint(mainBuilder, "HLS virt address: ", Int64Val,
			*mainFirstBblk->getModule());
#endif
		mainBuilder.CreateCall(decl_funcs[SEL4BENCH_INIT]);

		// Create 6 call instructions to sel4bench_set_count_event
		for (int i = 0; i < 6; ++i)
		{
			Value *s4bsceArgs[2] =
			{
				ConstantInt::get(ctx, APInt(64, i)),
				ConstantInt::get(ctx, APInt(64, event_pmu_codes[i]))
			};
			mainBuilder.CreateCall(decl_funcs[SEL4BENCH_SET_COUNT_EVENT],
				s4bsceArgs);
		}
		debug_errs("main prologue instrumentation done!!");
	}
	else
		debug_errs("This module has no main!");
	for (int i = 0; i < func_names.size(); ++i)
	{
		Value *is_idle_bit_0_val;

		// get function the function
		Function *func = M.getFunction(func_names.at(i));

		// function may not exist or may only be declared in this module. In
		// that case we don't perform any instrumentation
		if(!func) continue;
		if(func->empty()) continue;

		BasicBlock *entry_bblk = &func->getEntryBlock();

		inject_local_allocs_rst_counters(entry_bblk, func_ids.at(i),
			prolog_func, ctx);

		std::vector<Instruction*> call_instrs = get_call_instrs(func_names,
			func);

		// For every call instructions split blocks. In the end bblk is a block
		// that contains just the call instruction and nextBblk is the block
		// that bblk branches to at the end
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
		std::vector<Instruction *> ret_instrs = get_ret_instrs(func);
		for (Instruction *ret_instr : ret_instrs)
		{
			Value *epilog_func_args[TOTAL_EVENTS] =
			{
				event_local_vars[CPU_CYCLES],
				event_local_vars[EVENT0],
				event_local_vars[EVENT1],
				event_local_vars[EVENT2],
				event_local_vars[EVENT3],
				event_local_vars[EVENT4],
				event_local_vars[EVENT5]
			};
			CallInst *epilog_func_call_instr = CallInst::Create(
				epilog_func->getFunctionType(), epilog_func, epilog_func_args,
				"accprof_epilog");
			epilog_func_call_instr->insertBefore(ret_instr);
		}
		debug_errs(func_names.at(i) << " instrumentation done!!");
	}
	if(main_func)
	{
	// If we are on software mode, print results to console
#ifndef USE_HLS
#ifdef USE_SEL4BENCH
		std::vector<Instruction *> retInsts = getTermInst(main_func);
#else
		std::vector<Instruction *> retInsts = get_ret_instrs(main_func);
#endif
		for (Instruction *instr : retInsts)
		{
			BasicBlock *bblk = instr->getParent();
			Instruction *firstInstr = bblk->getFirstNonPHI();
			if (firstInstr != instr)
				bblk = SplitBlock(bblk, instr);
			
			IRBuilder<> endBuilder(bblk, bblk->begin());
			endBuilder.CreateCall(decl_funcs[accprofSoftPrintFunc]);
		}
		debug_errs(decl_func_names[accprofSoftPrintFunc]
			<< " instrumentation done!!");
#endif
	}

	return llvm::PreservedAnalyses::all();
}

/**
 * Injects instructions at the prologue of a function that is being profiled. It
 * allocates 7 local variables to stored counted events and inserts a call to a
 * function that writes to hardware.
 * @param bblk The entry block of the function we add the instructions
 * @param funcID The id of the Function we want to write to the hardware
 * @param prolog_func The function we want to call in that block
 * @param ctx The context of current Module
 * @return The block created or thisBBlk if not null
 */
void inject_local_allocs_rst_counters(BasicBlock *bblk, int func_id,
	Function *prolog_func, LLVMContext &ctx)
{
	debug_errs(__func__<< ": " << bblk->getParent()->getName().str()
		<< ": Enter");

	IRBuilder<> builder(bblk, bblk->begin());

#ifdef DEBUG_PRINT
	insertPrint(builder,  std::string(__func__) + " <<" +
		bblk->getParent()->getName().str() + ">>: START", nullptr,
		*bblk->getModule());
#endif
	// Allocate local vars for events and call prolog function
	for (int i = 0; i < TOTAL_EVENTS; ++i)
	{
		event_local_vars[i] = builder.CreateAlloca(
			IntegerType::getInt64Ty(ctx));
		builder.CreateStore( ConstantInt::get(ctx, llvm::APInt(64, 0)),
			event_local_vars[i]);
	}
	Value *prolog_funcArg[1] = {ConstantInt::get(ctx, APInt(32, func_id))};
	builder.CreateCall(prolog_func, prolog_funcArg);

	// reset/start counters
	builder.CreateCall(decl_funcs[SEL4BENCH_RESET_COUNTERS]);
	Value *start_counter_arg[1] = {ConstantInt::get(ctx, APInt(64, 0x3F))};
	builder.CreateCall(decl_funcs[SEL4BENCH_START_COUNTERS], start_counter_arg);
	CallInst *rd_pmcr_val = builder.CreateCall(
		decl_funcs[SEL4BENCH_PRIVATE_READ_PMCR]);
	Value *rst_pmcr_val = builder.CreateOr(rd_pmcr_val,
		ConstantInt::get(ctx, APInt(32, 0x4)));
	Value *wr_pmcr_arg[1] = {rst_pmcr_val};
	builder.CreateCall(decl_funcs[SEL4BENCH_PRIVATE_WRITE_PMCR], wr_pmcr_arg);
#ifdef DEBUG_PRINT
	insertPrint(builder,  std::string(__func__) + " <<" +
		bblk->getParent()->getName().str() + ">>: END", nullptr,
		*bblk->getModule());
#endif
	debug_errs(__func__<< ": " << bblk->getParent()->getName().str()
		<< ": Enter");

}


/**
 * Builds a Block that spins on the idle bit of the control signals of the HLS
 * IP. Must keep spinning until the IP is idle.
 * @param nextBblk We insert our new block before that block
 * @param func The function upon we build the block
 * @param ctx The context of current Module
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
 * Builds a Block that writes to the HLS IP the BRAM address and a function id.
 * If id is positive, it means we just called a function with the corresponding
 * id. If it is 0, we just returned from a function.
 * @param nextBblk We insert our new block before that block
 * @param func The function upon we build the block
 * @param ctx The context of current Module
 * @param globVar The global variable that contains the HLS virtual address
 * @param id The function id (positive) or 0
 * @return The block created
 */
BasicBlock *create_wr_fid_to_hls_bblk(AllocaInst *ctrl_sig_var, Function *func,
	LLVMContext &ctx, int id)
{

	int event_ids_ls;
	int event_ids_ms;

	BasicBlock *bblk = BasicBlock::Create(ctx, "", func);
	IRBuilder<> builder(bblk, bblk->begin());

#ifdef DEBUG_PRINT
	insertPrint(builder,  std::string(__func__) + " <<" + func->getName().str()
		+ ">>: START", nullptr, *bblk->getModule());
#endif
	debug_errs(__func__<< ": " << func->getName().str() << ": Enter");

#ifdef USE_HLS
	// Write the first 3 event IDs and func ID to the corresponding HLS address
	LoadInst *hls_base_addr = builder.CreateLoad(
		hls_global_var->getValueType(), hls_global_var);
	Value *hls_id_ls_addr = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(ctx), hls_base_addr, 4);
#endif
	Argument *func_id_arg = &*func->arg_begin();
	event_ids_ls = 0;
	event_ids_ls |= (event_pmu_codes[EVENT0 - 1] << 8);
	event_ids_ls |= (event_pmu_codes[EVENT1 - 1] << 16);
	event_ids_ls |= (event_pmu_codes[EVENT2 - 1] << 24);
	ConstantInt *event_ids_ls_val =
		ConstantInt::get(ctx, llvm::APInt(32, event_ids_ls));
	Value *ids_ls_val = builder.CreateOr(func_id_arg, event_ids_ls_val);
#ifdef DEBUG_PRINT
	insertPrint(builder, "\tevent_ids_ls: ", ids_ls_val, *bblk->getModule());
#endif
#ifdef USE_HLS
	builder.CreateStore(ids_ls_val, hls_id_ls_addr);

	// Write the last 3 event IDs to the corresponding HLS address
	hls_base_addr = builder.CreateLoad(
		hls_global_var->getValueType(), hls_global_var);
	Value *hls_id_ms_addr = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(ctx), hls_base_addr, 6);
#endif
	event_ids_ms = 0;
	event_ids_ms |= (event_pmu_codes[EVENT3 - 1] << 8);
	event_ids_ms |= (event_pmu_codes[EVENT4 - 1] << 16);
	event_ids_ms |= (event_pmu_codes[EVENT5 - 1] << 24);
	ConstantInt *ids_ms_val =
		ConstantInt::get(ctx, llvm::APInt(32, event_ids_ms));
#ifdef USE_HLS
	builder.CreateStore(ids_ms_val, hls_id_ms_addr);

	// Write BRAM address to HLS
	hls_base_addr = builder.CreateLoad(hls_global_var->getValueType(),
		hls_global_var);
	Value *hls_bram_addr = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(ctx), hls_base_addr, 36);
#ifdef DEBUG_PRINT
	insertPrint(builder, "\thls bram: ", hls_bram_addr, *bblk->getModule());
#endif
	builder.CreateStore(ConstantInt::get(ctx, llvm::APInt(32, BRAM_ADDRESS)),
		hls_bram_addr);

	// Trigger top function by setting the start bit
	LoadInst *ctrl_sig_val = builder.CreateLoad(
		ctrl_sig_var->getAllocatedType(), ctrl_sig_var);
	Value *start_bit_val = builder.CreateOr(ctrl_sig_val,
		ConstantInt::get(ctx, APInt(32, 0x1)));
	hls_base_addr = builder.CreateLoad(hls_global_var->getValueType(),
		hls_global_var);
	Value *hls_ctrl_addr = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(ctx), hls_base_addr, 0);
	builder.CreateStore(start_bit_val, hls_ctrl_addr);
#endif

#ifndef USE_HLS
	Value *atfArgs[16] =
	{
		ids_ls_val,
		ids_ms_val,
		ConstantPointerNull::get(Type::getInt32PtrTy(ctx)),
		ConstantPointerNull::get(Type::getInt32PtrTy(ctx)),
		ConstantPointerNull::get(Type::getInt32PtrTy(ctx)),
		ConstantPointerNull::get(Type::getInt32PtrTy(ctx)),
		ConstantPointerNull::get(Type::getInt32PtrTy(ctx)),
		ConstantPointerNull::get(Type::getInt32PtrTy(ctx)),
		ConstantPointerNull::get(Type::getInt32PtrTy(ctx)),
		ConstantPointerNull::get(Type::getInt32PtrTy(ctx)),
		ConstantPointerNull::get(Type::getInt32PtrTy(ctx)),
		ConstantPointerNull::get(Type::getInt32PtrTy(ctx)),
		ConstantPointerNull::get(Type::getInt32PtrTy(ctx)),
		ConstantPointerNull::get(Type::getInt32PtrTy(ctx)),
		ConstantPointerNull::get(Type::getInt32PtrTy(ctx)),
		ConstantPointerNull::get(Type::getInt32PtrTy(ctx))
	};
	builder.CreateCall(decl_funcs[accprofSoftTopFunc], atfArgs);
	debug_errs("\ninserted call to accprof_soft_top_func");
#endif


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
 * @param ctx The context of current Module
 * @return The block created
 */
BasicBlock *create_wr_metrics_to_hls_bblk(AllocaInst *ctrl_sig_var,
	Function *func, LLVMContext &ctx)
{
	Value *atfArgs[16];
	BasicBlock *bblk = BasicBlock::Create(ctx, "", func);
	IRBuilder<> builder(bblk, bblk->begin());
	
#ifdef DEBUG_PRINT
	insertPrint(builder,  std::string(__func__) + " <<" + func->getName().str()
		+ ">>: START", nullptr, *bblk->getModule());
#endif
	debug_errs(__func__<< ": " << func->getName().str() << ": Enter");

#ifdef USE_HLS
	// Create a load inst to read the address of the global pointer variable
	LoadInst *hls_base_addr = builder.CreateLoad(
		hls_global_var->getValueType(), hls_global_var);

	// Create a getelemntptr inst to get the adress of the 4th index of the
	// address
	Value *hls_id_ls_addr = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(ctx), hls_base_addr, 4);
		//IntegerType::getInt32Ty(ctx), hls_global_var, 4);
#ifdef DEBUG_PRINT
	//insertPrint(builder,"\twrite 0 to: ",hls_id_ls_addr,*bblk->getModule());
#endif
#endif

#ifdef USE_HLS
	// Create a Store inst to store the function ID to the 4th index from
	// the HLS base address which is the id parameter
	builder.CreateStore(ConstantInt::get(ctx, llvm::APInt(32, 0)),
		hls_id_ls_addr);
#endif

#ifndef USE_HLS
	atfArgs[0] = ConstantInt::get(ctx, llvm::APInt(32, 0));
	atfArgs[1] = ConstantInt::get(ctx, llvm::APInt(32, 0));
#endif

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
				IntegerType::getInt64Ty(ctx), event_global_vars[currEvent/4],
				MaybeAlign(8));

#ifdef DEBUG_PRINT
			insertPrint(builder, "\tEvent : ",
				ConstantInt::get(ctx, llvm::APInt(32, currEvent/4)),
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
				IntegerType::getInt32Ty(ctx));

#ifdef DEBUG_PRINT
			insertPrint(builder, "\t\thalf val: ", eventVal32,
				*bblk->getModule());
#endif
		
#ifdef USE_HLS
			// Create a load inst to read the address of the global pointer
			// variable
			hls_base_addr = builder.CreateLoad(
				hls_global_var->getValueType(), hls_global_var);

			// Create a getelemntptr inst to get the adress of the index
			// required to write specific event
			Value *hlsAddrAtIndex = builder.CreateConstInBoundsGEP1_64(
				IntegerType::getInt32Ty(ctx), hls_base_addr,
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
		hls_global_var->getValueType(), hls_global_var);

	// Create a getelemntptr inst to get the adress of the 20th index of the
	// address
	Value *hlsAddr36 = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(ctx), hlsAddrBramAddr, 36);

	// Create a Store inst to store the bram address to the 6th index from
	// the HLS base address which is the bram parameter
	builder.CreateStore(
		ConstantInt::get(ctx, llvm::APInt(32, BRAM_ADDRESS)), hlsAddr36);

	/**
	 * Write value 0x1 to the control signals.
	 */

	// Create a Load inst to load the control signals previously stored in
	// currCtrlSigs variable
	LoadInst *ctrl_sig_val = builder.CreateAlignedLoad(
		IntegerType::getInt32Ty(ctx), ctrl_sig_var,
		MaybeAlign(4));

	// Create an OR inst to set the ap_start(bit 0) and ap_continue(bit 4)
	Value *start_bit_val = builder.CreateOr(ctrl_sig_val,
		ConstantInt::get(ctx, APInt(32, 0x1)));

	// Create a load inst to read the address of the global pointer variable
	LoadInst *hlsAddrCtrl = builder.CreateLoad(hls_global_var->getValueType(),
		hls_global_var);

	// Create a getelemntptr inst to get the adress of the 0th index of the
	// address
	Value *hls_ctrl_addr = builder.CreateConstInBoundsGEP1_64(
		IntegerType::getInt32Ty(ctx), hlsAddrCtrl, 0);

	// Create a Store inst to store the number to currCtrlSigs
	builder.CreateAlignedStore(start_bit_val, hls_ctrl_addr, MaybeAlign(4));
#endif

#ifndef USE_HLS
	builder.CreateCall(decl_funcs[accprofSoftTopFunc], atfArgs);
#endif
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
	insertPrint(builder,  std::string(__func__) + " <<" +
		bblk->getParent()->getName().str() + ">>: START", nullptr,
		*bblk->getModule());
#endif
	debug_errs(__func__<< ": " << bblk->getParent()->getName().str()
		<< ": Enter");

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
	builder.CreateCall(decl_funcs[SEL4BENCH_RESET_COUNTERS]);

	// Create a call inst to sel4bench_start_counters/ 0x3F = 0b111111
	Value *s4bsceArg[1] = {ConstantInt::get(ctx, APInt(64, 0x3F))};
	builder.CreateCall(decl_funcs[SEL4BENCH_START_COUNTERS], s4bsceArg);

	CallInst *readPmcr = builder.CreateCall(
		decl_funcs[SEL4BENCH_PRIVATE_READ_PMCR]);

	// Create an OR inst to set the ap_start(bit 0) and ap_continue(bit 4)
	Value *cycleCounter = builder.CreateOr(readPmcr,
		ConstantInt::get(ctx, APInt(32, 0x4)));

	// Create a call inst to sel4bench_private_write_pmcr
	Value *s4bpwpArg[1] = {cycleCounter};
	builder.CreateCall(decl_funcs[SEL4BENCH_PRIVATE_READ_PMCR], s4bpwpArg);

#ifdef DEBUG_PRINT
	insertPrint(builder,  std::string(__func__) + " <<" +
		bblk->getParent()->getName().str() + ">>: END", nullptr,
		*bblk->getModule());
#endif
	debug_errs(__func__<< ": " << bblk->getParent()->getName().str()
		<< ": Exit");
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
	insertPrint(builder,  std::string(__func__) + " <<" +
		bblk->getParent()->getName().str() + ">>: START", nullptr,
		*bblk->getModule());
#endif
	debug_errs(__func__<<": "<<bblk->getParent()->getName().str()<<": Enter");
	CallInst *countEventVals[TOTAL_EVENTS];

	// Create call inst to sel4bench_get_cycle_count to get cpu cycles count
	countEventVals[CPU_CYCLES] = builder.CreateCall(
		decl_funcs[SEL4BENCH_GET_CYCLE_COUNT]);

	// Create call inst to sel4bench_get_counter to get the other event counts
	for (int i = 1; i < TOTAL_EVENTS; ++i)
	{
		Value *s4bgcArg[1] = {ConstantInt::get(ctx, APInt(64, i - 1))};
		countEventVals[i] = builder.CreateCall(
			decl_funcs[SEL4BENCH_GET_COUNTER], s4bgcArg);
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
	insertPrint(builder, std::string(__func__) + " <<" +
		bblk->getParent()->getName().str() + ">>: END", nullptr,
		*bblk->getModule());
#endif
	debug_errs(__func__<<": "<< bblk->getParent()->getName().str() << ": Exit");
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

	CallInst *countEventVals[TOTAL_EVENTS];
	// Create call inst to sel4bench_get_cycle_count to get cpu cycles count
	countEventVals[CPU_CYCLES] = builder.CreateCall(
		decl_funcs[SEL4BENCH_GET_CYCLE_COUNT]);

	// Create call inst to sel4bench_get_counter to get the other event counts
	for (int i = 1; i < TOTAL_EVENTS; ++i)
	{
		Value *s4bgcArg[1] = {ConstantInt::get(ctx, APInt(64, i - 1))};
		countEventVals[i] = builder.CreateCall(
			decl_funcs[SEL4BENCH_GET_COUNTER], s4bgcArg);
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
 * @param func_names A vector containing the function names of which the call
 * instructions must match with
 * @param func The function of which we iterate over the basic blocks and
 * instructions
 * @return The vector of the maching call instructions
 */
std::vector<Instruction *> get_call_instrs(std::vector<std::string> &func_names,
	Function *func)
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
 * Defines a function to which call instructions will be injected by the pass,
 * at the prologue of each function that will be profiled.
 * @param M The current Module Object
 * @param ctx The context of current Module
 * @return The object to the function created
 */
Function* create_prolog_func(int funcID, Module &M, LLVMContext &ctx)
{
	debug_errs(__func__<< ": Enter");
	Value *is_idle_bit_0_val;

	std::vector<Type*> func_param_typ { Type::getInt32Ty(ctx) };// func ID 
	Type* func_ret_typ = Type::getVoidTy(ctx);
	FunctionType* ft = FunctionType::get(func_ret_typ, func_param_typ, false);
	Function* prolog_func = Function::Create(ft, Function::ExternalLinkage,
		"accprof_prolog", &M);

	std::string absolut_filename = M.getName().str();
	std::string src_filename = std::filesystem::path(absolut_filename).
		filename().replace_extension("").string();
	// define function only once in main.c
	if(src_filename == "main")
	{	// create blocks
		BasicBlock *entry_bblk = BasicBlock::Create(ctx, "", prolog_func);
		IRBuilder<> entry_bblk_builder(entry_bblk, entry_bblk->begin());
		AllocaInst* ctrl_sig_var = entry_bblk_builder.CreateAlloca(
				IntegerType::getInt32Ty(ctx));
		BasicBlock *spinlock_idle_bit_bblk = create_spinlock_idle_bit_bblk(
			ctrl_sig_var, prolog_func, ctx, &is_idle_bit_0_val);
		BasicBlock *wr_fid_to_hls_bblk = create_wr_fid_to_hls_bblk(ctrl_sig_var,
			prolog_func, ctx, funcID);
		// connect blocks
		entry_bblk_builder.CreateBr(spinlock_idle_bit_bblk);
		IRBuilder<> spinlock_idle_bit_bblk_builder(spinlock_idle_bit_bblk,
			spinlock_idle_bit_bblk->end());
		spinlock_idle_bit_bblk_builder.CreateCondBr(is_idle_bit_0_val,
			spinlock_idle_bit_bblk, wr_fid_to_hls_bblk);
		IRBuilder<> wr_fid_to_hls_bblk_builder(wr_fid_to_hls_bblk,
			wr_fid_to_hls_bblk->end());
		wr_fid_to_hls_bblk_builder.CreateRetVoid();
		debug_errs(__func__<< ": Exit");
	}
	return prolog_func;
}

/**
 * Defines a function to which call instructions will be injected by the pass,
 * before each return instruction of a function that will be profiled.
 * @param M The current Module Object
 * @param ctx The context of current Module
 * @return The object to the function created
 */
Function* create_epilog_func(Module &M, LLVMContext &ctx)
{
	debug_errs(__func__<< ": Enter");
	Value *is_idle_bit_0_val;

	std::vector<Type*> func_param_typ
	{
		Type::getInt64Ty(ctx), // cpu cycles
		Type::getInt64Ty(ctx), // event 0
		Type::getInt64Ty(ctx), // event 1
		Type::getInt64Ty(ctx), // event 2
		Type::getInt64Ty(ctx), // event 3
		Type::getInt64Ty(ctx), // event 4
		Type::getInt64Ty(ctx)  // event 5
	};
	Type* func_ret_typ = Type::getVoidTy(ctx);
	FunctionType* ft = FunctionType::get(func_ret_typ, func_param_typ, false);
	Function* epilog_func = Function::Create(ft, Function::ExternalLinkage,
		"accprof_epilog", &M);
	std::string absolut_filename = M.getName().str();
	std::string src_filename = std::filesystem::path(absolut_filename).
		filename().replace_extension("").string();
	
	// In case of profiling multiple files, we want the epilogue function to
	// only be defined in the main file and be declared in the rest.
	if(src_filename == "main")
	{
		// Create a dummy block with a return to add all other blocks before it
		BasicBlock *entry_bblk = BasicBlock::Create(ctx, "", epilog_func);
		IRBuilder<> entry_bblk_builder(entry_bblk, entry_bblk->begin());
		AllocaInst* ctrl_sig_var = entry_bblk_builder.CreateAlloca(
				IntegerType::getInt32Ty(ctx));
		BasicBlock *wr_metrics_to_globals_bblk =
			create_wr_metrics_to_globals_bblk(epilog_func, ctx);
		BasicBlock *spinlock_idle_bit_bblk = create_spinlock_idle_bit_bblk(
			ctrl_sig_var, epilog_func, ctx, &is_idle_bit_0_val);
		BasicBlock *wr_metrics_to_hls_bblk = create_wr_metrics_to_hls_bblk(
			ctrl_sig_var, epilog_func, ctx);

		entry_bblk_builder.CreateBr(wr_metrics_to_globals_bblk);
		IRBuilder<> wr_metrics_to_globals_bblk_builder(
			wr_metrics_to_globals_bblk, wr_metrics_to_globals_bblk->end());
		wr_metrics_to_globals_bblk_builder.CreateBr(spinlock_idle_bit_bblk);
		IRBuilder<> spinlock_idle_bit_bblk_builder(spinlock_idle_bit_bblk,
			spinlock_idle_bit_bblk->end());
		spinlock_idle_bit_bblk_builder.CreateCondBr(is_idle_bit_0_val,
			spinlock_idle_bit_bblk, wr_metrics_to_hls_bblk);
		IRBuilder<> wr_metrics_to_hls_bblk_builder(wr_metrics_to_hls_bblk,
			wr_metrics_to_hls_bblk->end());
		wr_metrics_to_hls_bblk_builder.CreateRetVoid();
		debug_errs(__func__<< ": Exit");
	}
	return epilog_func;
}

/**
 * Iterates over the SeL4 function names in @c decl_func_names and for every
 * function that is not visible in the module, it creates an external
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
#ifndef USE_HLS
		{
			Type::getInt32Ty(ctx), Type::getInt32Ty(ctx),
			Type::getInt32Ty(ctx), Type::getInt32Ty(ctx),
			Type::getInt32Ty(ctx), Type::getInt32Ty(ctx),
			Type::getInt32Ty(ctx), Type::getInt32Ty(ctx),
			Type::getInt32Ty(ctx), Type::getInt32Ty(ctx),
			Type::getInt32Ty(ctx), Type::getInt32Ty(ctx),
			Type::getInt32Ty(ctx), Type::getInt32Ty(ctx),
			Type::getInt32Ty(ctx), Type::getInt32Ty(ctx)
		},
		{},
#endif
		{},
		{},
		{Type::getInt64Ty(ctx)},
		{Type::getInt64Ty(ctx), Type::getInt64Ty(ctx)},
		{},
		{Type::getInt32Ty(ctx)},
		{},
		{Type::getInt64Ty(ctx)}
		//{Type::getInt64PtrTy(ctx), Type::getInt32PtrTy(ctx)},
		//{attkey_typ, Type::getInt32Ty(ctx)}
	};
	Type* decl_func_ret_types[TOTAL_DECL_FUNCS] =
	{
#ifndef USE_HLS
		Type::getVoidTy(ctx),
		Type::getVoidTy(ctx),
#endif
		Type::getVoidTy(ctx),
		Type::getVoidTy(ctx),
		Type::getVoidTy(ctx),
		Type::getVoidTy(ctx),
		Type::getInt32Ty(ctx),
		Type::getVoidTy(ctx),
		Type::getInt64Ty(ctx),
		Type::getInt64Ty(ctx)
		//attkey_typ,
		//Type::getInt8PtrTy(ctx)
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
llvm::PassPluginLibraryInfo getAccProfModPluginInfo()
{
	// std::cerr << "getAccProfModPluginInfo() called\n"; // add this line
	return {
		LLVM_PLUGIN_API_VERSION, "AccProfMod", LLVM_VERSION_STRING,
		[](PassBuilder &PB)
		{
			PB.registerOptimizerEarlyEPCallback(
				[](ModulePassManager &MPM, OptimizationLevel)
				{
					MPM.addPass(AccProfMod());
					return true;
				});
		}};
}
#else
// -----------------------------------------------------------------------------
// New PM Registration - use this for loading the pass on an ll file with opt
// -----------------------------------------------------------------------------
llvm::PassPluginLibraryInfo getAccProfModPluginInfo()
{
	 //std::cerr << "getAccProfModPluginInfo() called\n"; // add this line
	return
	{
		LLVM_PLUGIN_API_VERSION, "AccProfMod", LLVM_VERSION_STRING,
		[](PassBuilder &PB)
		{
			PB.registerPipelineParsingCallback
			(
				[](StringRef Name, ModulePassManager &MPM,
				ArrayRef<PassBuilder::PipelineElement>)
				{
					if (Name == "accprof")
					{
						MPM.addPass(AccProfMod());
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
	return getAccProfModPluginInfo();
}
