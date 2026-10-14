# First bounded Xenos pixel microcode -> GLSL -> UAM/DKSH execution

Continuation of PR #5, parent 56e7468a82668c91d852aa59a834a1bde60f708e.
Pinyon pin a72a92a5f215a32875386b824b8b6f0f34e1caf9, embedded ShiftGlue SDK
1e8968f712824b97d4f4330c5a22389c4c8be1b5. Only original synthetic input.

## Actual upstream architecture inspected

Paths below are relative to pinyon-shift/thirdparty/shiftglue-sdk:

| Path | Contract | This delivery |
|---|---|---|
| include/rex/graphics/format/ucode.h | 48-bit CF pair unpacking, 96-bit ALU fields, relative swizzle, export identities | Uses actual public types and UnpackControlFlowInstructions/GetSwizzledComponentIndex |
| src/graphics/format/ucode.cpp | Real opcode metadata | Compiled directly into host producer/tests and ARM64 probe |
| include/rex/graphics/pipeline/shader/shader.h | Shader native-endian words; AnalyzeUcode; ParsedAluInstruction/operands/exports; Translation binary | Inspected, not linked; the bounded adapter has its own validated IR |
| src/graphics/pipeline/shader/translator.cpp | ParseAluInstruction and operand/result handling | Inspected to establish temp/constants/swizzle/export contract, not claimed as linked parser |
| include/rex/graphics/pipeline/shader/spirv_translator.h | Features, modification bits, constant/resource layout, EDRAM interlock/device contracts | Inspected; not the deko backend ABI |
| src/graphics/pipeline/shader/spirv_translator.cpp / _alu.cpp | SPIR-V lowering, uniform constants and Vulkan descriptor-set decorations | Inspected for compatibility; not reused as a complete translator |
| src/graphics/vulkan/pipeline_cache.cpp | AnalyzeUcode -> TranslateAnalyzedShader -> translated binary | Inspected; not transported to Switch |
| src/graphics/CMakeLists.txt | Vulkan plugin bundles SPIR-V translator with renderer/runtime dependencies | Confirmed actual build boundary |

UAM 1.1's advertised interface accepts a source file and stage and emits a
DKSH module, raw Maxwell code or TGSI. There is no SPIR-V input mode in its
advertised CLI. This delivery does **not** convert upstream-generated SPIR-V.
A general SPIR-V -> GLSL -> UAM path still needs validated remapping of descriptor
sets, shared memory, fetches, system constants, stage interfaces, interlocks and
unsupported UAM capabilities. No XenosRecomp HLSL or commercial shader pack is
used or claimed to have been converted.

## Implemented executable subset

XenosPixelShader decodes big-endian binary words through actual SDK CF/ALU types
and opcode metadata, normalizes operands and exports, validates IR and produces
GLSL 460. A host CLI reads bounded microcode files, then the build invokes real
UAM to generate two DKSH modules. The ARM64 client checks that its decoded
microcode and emitted GLSL exactly match the host bundle before using DKSH.
SHA256 links microcode/GLSL/DKSH in the generated manifest.

| Feature | Bound |
|---|---|
| Stage | Pixel only; vertex explicitly rejected |
| CF | Canonical ALLOC of one color followed by absolute EXEC_END |
| ALU | 1..6 vector instructions; ADD, MUL, MAX; at most two MUL |
| Destinations | Eight statically addressed temps; full vec4 writes; final color0 export |
| Sources | Static logical float constants c0..c3 or previously defined temps |
| Swizzle | Actual Xenos component-relative encoding -> GLSL component selection |
| Clamp | Per-vector clamp supported |
| Constants | Native FP32 vec4[4], std140, binding1; finite values with abs <=16 |
| Arithmetic | Precise destinations; zero-operand multiplication rule; bounded finite domain |

Rejects fetches/serialization flags, loops/branches/predication, addressed
constants/temps, negation/absolute modifiers, scalar execution, partial writes,
unknown ALU, uninitialized/large temps, large constants, unsupported exports,
missing exports, vertex stage, truncated/trailing/oversized data and invalid IR.
The op/constant limits keep finite intermediates within FP32 range; this is not
a general NaN/Inf/signed-zero or arbitrary shader arithmetic validation.

Binding0 remains the original GLSL vertex transform/tint buffer. Binding1 is the
new pixel constants block. This is an explicit adapter ABI, **not** the Vulkan
constant descriptor ABI. PM4 currently does not decode shader-load packets or
pixel constant-register writes into this ABI; the synthetic client binds these
resources explicitly through the reusable adapter.

## GPU validation

NRO SHA256:
691da0f69b332d38446bf7a69d542f3b682d3b45e343e3ad04dbb59d999b4667.
Two genuine 48-byte synthetic microcode programs run MUL(c0,c1), ADD(temp,c2),
then MAX(temp,temp) with clamped color0 export. The second swaps R/G using Xenos
swizzle 0x0d. These are binary Xenos instructions, not a made-up shader protocol.

Draws use PM4 indexed commands and actual deko3d vertex/index/uniform buffers.
Both pixel modules are generated from microcode. The vertex module and display
sampling module remain original GLSL compiled by UAM; no vertex translation is
claimed. The client alternates three constant sets for 32 frames and checks
three distinct draw positions per frame (96 color comparisons per attempt).

| Constant phase | Program 0 RGBA8 | Program 1 RGBA8 |
|---|---|---|
| MUL + ADD offset | (159,64,64,255) | (96,96,64,255) |
| MUL + zero offset | (128,32,48,255) | (64,64,48,255) |
| Negative/high offset + clamp | (0,255,255,255) | (0,255,255,255) |

The independent references are literal byte values, not values produced by the
translator IR or a CPU evaluator. Each comparison permits only one quantization
byte of error. Final screenshot checks compare the second phase at all three
positions and reject black/flat-color captures. A sample pass is not counted as
successful execution without controls, resource destruction and clean exit.

Official Eden GCC and PGO use explicit High GPU accuracy and isolated profiles;
GCC 01, PGO 01 and GCC 02 each completed all 96 color comparisons, independent
screenshot checks, A x3, Plus, zero resource counters, shutdown and process exit0.
Observed process memory: 69,062,656 bytes; images 5,570,560; buffers 32,768 before
destruction. All three attempts are preserved individually. No performance
on physical Switch or long-duration stability is inferred from llvmpipe.

## Tests and limitations

Host CF/ALU and negative contracts pass ASan+UBSan. The standalone file-input
producer was exercised with a valid file and an unsupported MAD file (exit2,
no GLSL output). 35 Python tests include screenshot negatives and attempt
classification. ARM64 compiles the real SDK opcode table and runs decoder
negatives for unsupported MAD, vertex stage and relative constants.

SDK code stays pinned and keeps its BSD and ucode-header MIT notices. Public
fixtures and generated GLSL are original; DKSH/NRO build artifacts are generated
by CI rather than commercial assets. Full logs, source snapshots and input/output
hashes are in [evidence](evidence/xenos-shader-subset-2026-10-14/). Final CI status
is recorded in the PR; compilation alone does not approve Eden execution.

Remaining blockers: no actual full Shader::AnalyzeUcode/SpirvShaderTranslator
integration, no vertex/texture fetch or scalar/control-flow lowering, no complete
resource remapping, no on-console UAM compiler, no general Forza shader support,
no shader-load PM4 state integration, and the known sparse-memory/alias limitations.
Next useful increment: real shader-load identity/cache and constant-register
binding, then a limited vertex-fetch/position-export contract with GPU evidence.
