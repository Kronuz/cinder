// Copyright (c) Meta Platforms, Inc. and affiliates.

#include "cinderx/Jit/compiler.h"

#include "Python.h"
#include "cinderx/Common/log.h"

#include "cinderx/Jit/config.h"
#include "cinderx/Jit/disassembler.h"
#include "cinderx/Jit/hir/analysis.h"
#include "cinderx/Jit/hir/builder.h"
#include "cinderx/Jit/hir/optimization.h"
#include "cinderx/Jit/hir/preload.h"
#include "cinderx/Jit/hir/printer.h"
#include "cinderx/Jit/hir/ssa.h"
#include "cinderx/Jit/jit_time_log.h"

#include "cinderx/ThirdParty/json/json.hpp"

#include <chrono>
#include <fstream>

namespace jit {

void CompiledFunction::disassemble() const {
  JIT_ABORT("disassemble() cannot be called in a release build.");
}

void CompiledFunction::printHIR() const {
  JIT_ABORT("printHIR() cannot be called in a release build.");
}

void CompiledFunctionDebug::disassemble() const {
  jit::disassemble(
      reinterpret_cast<const char*>(vectorcallEntry()),
      codeSize(),
      reinterpret_cast<vma_t>(vectorcallEntry()));
}

void CompiledFunctionDebug::printHIR() const {
  jit::hir::HIRPrinter printer;
  printer.Print(*irfunc_.get());
}

struct PassTimer {
  explicit PassTimer() : start(std::chrono::steady_clock::now()) {}

  std::size_t finish() {
    auto end = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
        .count();
  }

  std::chrono::steady_clock::time_point start;
};

template <typename T>
static void runPass(hir::Function& func, PostPassFunction callback) {
  T pass;
  COMPILE_TIMER(func.compilation_phase_timer,
                pass.name(),
                JIT_LOGIF(
                    g_dump_hir_passes,
                    "HIR for {} before pass {}:\n{}",
                    func.fullname,
                    pass.name(),
                    func);

                PassTimer timer;
                pass.Run(func);
                std::size_t time_ns = timer.finish();
                callback(func, pass.name(), time_ns);

                JIT_LOGIF(
                    g_dump_hir_passes,
                    "HIR for {} after pass {}:\n{}",
                    func.fullname,
                    pass.name(),
                    func);

                JIT_DCHECK(
                    checkFunc(func, std::cerr),
                    "Function {} failed verification after pass {}:\n{}",
                    func.fullname,
                    pass.name(),
                    func);

                JIT_DCHECK(
                    funcTypeChecks(func, std::cerr),
                    "Function {} failed type checking after pass {}:\n{}",
                    func.fullname,
                    pass.name(),
                    func);)
}

void Compiler::runPasses(jit::hir::Function& irfunc, PassConfig config) {
  PostPassFunction callback = [](hir::Function&, const char*, std::size_t) {};
  runPasses(irfunc, config, callback);
}

void Compiler::runPasses(
    jit::hir::Function& irfunc,
    PassConfig config,
    PostPassFunction callback) {
  // SSAify must come first; nothing but SSAify should ever see non-SSA HIR.
  runPass<jit::hir::SSAify>(irfunc, callback);
  runPass<jit::hir::Simplify>(irfunc, callback);
  runPass<jit::hir::DynamicComparisonElimination>(irfunc, callback);
  runPass<jit::hir::GuardTypeRemoval>(irfunc, callback);
  runPass<jit::hir::PhiElimination>(irfunc, callback);
  if (config & PassConfig::kEnableHIRInliner) {
    runPass<jit::hir::InlineFunctionCalls>(irfunc, callback);
    runPass<jit::hir::Simplify>(irfunc, callback);
    runPass<jit::hir::BeginInlinedFunctionElimination>(irfunc, callback);
  }
  runPass<jit::hir::BuiltinLoadMethodElimination>(irfunc, callback);
  runPass<jit::hir::Simplify>(irfunc, callback);
  runPass<jit::hir::CleanCFG>(irfunc, callback);
  runPass<jit::hir::DeadCodeElimination>(irfunc, callback);
  runPass<jit::hir::CleanCFG>(irfunc, callback);
  // RefcountInsertion must come last
  runPass<jit::hir::RefcountInsertion>(irfunc, callback);
  JIT_LOGIF(
      g_dump_final_hir, "Optimized HIR for {}:\n{}", irfunc.fullname, irfunc);
}

std::unique_ptr<CompiledFunction> Compiler::Compile(
    BorrowedRef<PyFunctionObject> func) {
  JIT_CHECK(PyFunction_Check(func), "Expected PyFunctionObject");
  JIT_CHECK(
      !g_threaded_compile_context.compileRunning(),
      "multi-thread compile must preload first");
  std::unique_ptr<hir::Preloader> preloader =
      hir::Preloader::makePreloader(func);
  return preloader ? Compile(*preloader) : nullptr;
}

PassConfig createConfig() {
  PassConfig result{PassConfig::kDefault};
  if (getConfig().hir_inliner_enabled) {
    result = static_cast<PassConfig>(result | PassConfig::kEnableHIRInliner);
  }
  return result;
}

std::unique_ptr<CompiledFunction> Compiler::Compile(
    const jit::hir::Preloader& preloader) {
  const std::string& fullname = preloader.fullname();
  if (!PyDict_CheckExact(preloader.globals())) {
    JIT_DLOG(
        "Refusing to compile {}: globals is a {:.200}, not a dict",
        fullname,
        Py_TYPE(preloader.globals())->tp_name);
    return nullptr;
  }

  PyObject* builtins = preloader.builtins();
  if (!PyDict_CheckExact(builtins)) {
    JIT_DLOG(
        "Refusing to compile {}: builtins is a {:.200}, not a dict",
        fullname,
        Py_TYPE(builtins)->tp_name);
    return nullptr;
  }
  JIT_DLOG(
      "Compiling {} @ {}",
      fullname,
      reinterpret_cast<void*>(preloader.code().get()));

  std::unique_ptr<CompilationPhaseTimer> compilation_phase_timer{nullptr};

  if (captureCompilationTimeFor(fullname)) {
    compilation_phase_timer = std::make_unique<CompilationPhaseTimer>(fullname);
    compilation_phase_timer->start("Overall compilation");
    compilation_phase_timer->start("Lowering into HIR");
  }

  PassTimer hir_build_timer;
  std::unique_ptr<jit::hir::Function> irfunc(jit::hir::buildHIR(preloader));
  std::size_t hir_build_time_ns = hir_build_timer.finish();
  if (nullptr != compilation_phase_timer) {
    compilation_phase_timer->end();
  }
  if (irfunc == nullptr) {
    JIT_DLOG("Lowering to HIR failed {}", fullname);
    return nullptr;
  }

  if (g_dump_hir) {
    JIT_LOG("Initial HIR for {}:\n{}", fullname, *irfunc);
  }

  if (nullptr != compilation_phase_timer) {
    irfunc->setCompilationPhaseTimer(std::move(compilation_phase_timer));
  }

  PassConfig config = createConfig();
  std::unique_ptr<nlohmann::json> json{nullptr};
  if (!g_dump_hir_passes_json.empty()) {
    // TODO(emacs): For inlined functions, grab the sources from all the
    // different functions inlined.
    json.reset(new nlohmann::json());
    nlohmann::json passes;
    hir::JSONPrinter hir_printer;
    passes.emplace_back(hir_printer.PrintSource(*irfunc));
    passes.emplace_back(hir_printer.PrintBytecode(*irfunc));
    PostPassFunction dump =
        [&hir_printer, &passes](
            hir::Function& func, const char* pass_name, std::size_t time_ns) {
          hir_printer.Print(passes, func, pass_name, time_ns);
        };
    dump(*irfunc, "Initial HIR", hir_build_time_ns);
    COMPILE_TIMER(
        irfunc->compilation_phase_timer,
        "HIR transformations",
        Compiler::runPasses(*irfunc, config, dump))
    (*json)["fullname"] = fullname;
    (*json)["cols"] = passes;
  } else {
    COMPILE_TIMER(
        irfunc->compilation_phase_timer,
        "HIR transformations",
        Compiler::runPasses(*irfunc, config))
  }
  hir::OpcodeCounts hir_opcode_counts = hir::count_opcodes(*irfunc);

  auto ngen = ngen_factory_(irfunc.get());
  if (ngen == nullptr) {
    return nullptr;
  }

  if (!g_dump_hir_passes_json.empty()) {
    ngen->SetJSONOutput(json.get());
  }

  void* entry = nullptr;
  COMPILE_TIMER(
      irfunc->compilation_phase_timer,
      "Native code Generation",
      entry = ngen->getVectorcallEntry())
  if (entry == nullptr) {
    JIT_DLOG("Generating native code for {} failed", fullname);
    return nullptr;
  }

  JIT_DLOG("Finished compiling {}", fullname);
  if (nullptr != irfunc->compilation_phase_timer) {
    irfunc->compilation_phase_timer->end();
    irfunc->setCompilationPhaseTimer(nullptr);
  }

  int func_size = ngen->GetCompiledFunctionSize();
  int stack_size = ngen->GetCompiledFunctionStackSize();
  int spill_stack_size = ngen->GetCompiledFunctionSpillStackSize();

  if (!g_dump_hir_passes_json.empty()) {
    std::string filename =
        fmt::format("{}/function_{}.json", g_dump_hir_passes_json, fullname);
    JIT_DLOG("Dumping JSON for {} to {}", fullname, filename);
    std::ofstream json_file;
    json_file.open(
        filename,
        std::ios_base::out | std::ios_base::trunc | std::ios_base::binary);
    json_file << json->dump() << std::endl;
    json_file.close();
  }

  // Grab some fields off of irfunc and ngen before moving them.
  hir::Function::InlineFunctionStats inline_stats =
      std::move(irfunc->inline_function_stats);
  void* static_entry = ngen->getStaticEntry();
  CodeRuntime* code_runtime = ngen->codeRuntime();

  if (g_debug) {
    irfunc->setCompilationPhaseTimer(nullptr);
    return std::make_unique<CompiledFunctionDebug>(
        std::move(irfunc),
        std::move(ngen),
        reinterpret_cast<vectorcallfunc>(entry),
        static_entry,
        code_runtime,
        func_size,
        stack_size,
        spill_stack_size,
        std::move(inline_stats),
        hir_opcode_counts);
  } else {
    return std::make_unique<CompiledFunction>(
        reinterpret_cast<vectorcallfunc>(entry),
        static_entry,
        code_runtime,
        func_size,
        stack_size,
        spill_stack_size,
        std::move(inline_stats),
        hir_opcode_counts);
  }
}

} // namespace jit
