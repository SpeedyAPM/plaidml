// Copyright 2020 Intel Corporation

#include <chrono>
#include <cstdlib>

#include "half.hpp"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"

#include "mlir/ExecutionEngine/RunnerUtils.h"

#include "pmlc/rt/symbol_registry.h"

extern "C" void plaidml_rt_trace(const char *msg) {
  using clock = std::chrono::steady_clock;
  static bool isFirst = true;
  static std::chrono::time_point<clock> lastTime = clock::now();
  auto now = clock::now();
  auto delta = now - lastTime;
  lastTime = now;
  auto deltaSecs =
      std::chrono::duration_cast<std::chrono::duration<double>>(delta).count();
  if (isFirst) {
    isFirst = false;
  } else {
    llvm::outs() << llvm::format(": %7.6f\n", deltaSecs);
    llvm::outs().flush();
  }
  llvm::outs() << msg;
  llvm::outs().flush();
}

float h2f(half_float::half n) { return n; }

half_float::half f2h(float n) {
  return half_float::half_cast<half_float::half>(n);
}

namespace pmlc::rt {

void registerBuiltins() {
  using pmlc::rt::registerSymbol;

  // compiler_rt functions
  // TODO: replace with @llvm-project//compiler-rt/lib/builtins
  registerSymbol("__gnu_h2f_ieee", reinterpret_cast<void *>(h2f));
  registerSymbol("__gnu_f2h_ieee", reinterpret_cast<void *>(f2h));
  registerSymbol("___extendhfsf2", reinterpret_cast<void *>(h2f));
  registerSymbol("___truncsfhf2", reinterpret_cast<void *>(f2h));

  // cstdlib functions
  REGISTER_SYMBOL(free);
  REGISTER_SYMBOL(malloc);

  // RunnerUtils functions
  REGISTER_SYMBOL(_mlir_ciface_printMemrefF32);
  REGISTER_SYMBOL(plaidml_rt_trace);
}

} // namespace pmlc::rt
