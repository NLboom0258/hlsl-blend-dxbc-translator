# HLSL Blend DXBC Translator

[English](README.md) | [中文](README.zh-CN.md)

A C++17 translator that converts shaders written as a **mix of DXBC SM5
assembly text and embedded HLSL fragments** back into **pure DXBC SM5 assembly
text**.

The DXBC assembly format is the standard disassembly output of the Microsoft
HLSL shader compiler (fxc) — the same format used by 3Dmigoto and other
shader-modding tools. A "blend" file is normal DXBC assembly with a few HLSL
markers sprinkled in, so math that is awkward to express in raw assembly can be
written in HLSL instead. This tool parses those files and translates every HLSL
fragment into correct DXBC SM5 instructions.

This project was written with heavy assistance from an AI agent, which authored
most of the code. It is a C++ rewrite of the author's earlier Python prototype,
which defined the input/output format and behavior. The translation logic
follows the official compilers — DXC (`DirectXShaderCompiler`) as the primary
reference, vkd3d as a secondary one — for correctness.

## The 8 marker syntax

Non-marker lines are passed through verbatim. Markers are translated:

| Marker | Purpose |
|---|---|
| `HLSLMov <type> <var>[.swizzle] = <reg>;` | Bind an HLSL variable to a DXBC register |
| `DXBCMov <dest reg>, <var>[.swizzle]` | Write an HLSL variable back to a DXBC register |
| `HLSL <single-line HLSL statement>` | Translate one HLSL statement |
| `HLSLSnippet { ... }` | Translate a block of HLSL (if/else/for/while/switch supported) |
| `HLSLInit` | Zero every allocated temp register at this point |
| `HLSLTexture Texture2D X = tN;` | Bind a texture alias |
| `HLSLSampler SamplerState X = sN;` | Bind a sampler alias |
| `HLSLFunctionImport "functions/lib.txt";` | Import a custom function library |

Example:

```
HLSLTexture Texture2D IlmMapTex0 = t50;
HLSLSampler SamplerState IlmMapSampler0 = s13;

HLSLSnippet {
    float3 col = IlmMapTex0.SampleLevel(IlmMapSampler0, uv, 0).xyz;
    float3 n = normalize(col);
    col = saturate(n * dot(n, LightDir));
}
```

## Build

### MSBuild (Visual Studio 2022, v143)

```
MSBuild.exe hlsl_blend_dxbc_translator.sln -p:Configuration=Release -p:Platform=x64
```

### CMake / MinGW

The project also builds with CMake and MinGW g++ (C++17).

## Usage

```
hlsl_blend_dxbc_translator.exe -input <file> [-output <out.asm>] [-data <lib dir>]
```

- `-data` adds a search directory for libraries referenced by
  `HLSLFunctionImport`. Lookup order: `-data` dir → input file's directory →
  current directory. There is no default library — only files referenced by an
  `HLSLFunctionImport` marker are loaded.
- `-no-sat-fold` disables `saturate` folding (uses min/max instead) — a debug
  flag for comparing against the reference output.

## Function libraries

The translator has no bundled "standard library". All HLSL instructions and
syntax are built in; `HLSLFunctionImport` is an extension mechanism for
user-defined functions, not a required dependency.

The `data/functions/lib.txt` file is a **reference example**, not a standard
library. Its functions were written to fit a specific mod — the M2 global
lighting v0.5 test shader in `tests/` — and are used by that shader. They date
from the early days of the predecessor translator, when syntax support was
limited, so the code is rough in places (e.g. ID switching is done with
hard-coded nested `if/else`). Treat the file as a syntax reference for writing
your own function library rather than something to rely on as-is.

## Supported HLSL

- **Types**: `float`/`int`/`uint`/`bool`/`double`/`half`/`min16*`, vectors and
  matrices up to 4x4. Matrix math is supported (`mul(M,v)`, `mul(v,M)`,
  `(float3x3)` casts) with **column-major** semantics, fxc-verified.
- **Full control flow**: `if/else`, `for`, `while`, `switch/case`, `discard`,
  `break`, `continue`, arbitrary-position `return` in imported functions
- **Bitwise ops** on int/uint, compound assignment (`+=` `&=` `<<=` ...),
  prefix/postfix `++/--`
- **Sampling**: `Sample`, `SampleLevel`, `SampleCmp`, `SampleBias`, `SampleGrad`
- **~59 intrinsics** including `normalize`, `smoothstep` (mad-based, matching
  fxc), `mad`, `reflect`, `clip`, `fwidth`, `any/all`, `ddx/ddy`, `fmod`,
  `radians/degrees`, ...
- **FXC-standard float comparison** (`lt/ge/eq/ne` are normalized with
  `and dst, dst, l(0x3f800000)` — this is the standard fix for the fact that
  DXBC float comparisons return 0 or all-ones/NaN, not 0.0/1.0)

## Verification

- `bash tests/run_tests.sh` — 12 regression tests (a real mod shader + feature
  tests + assembly structure validation)
- **fxc cross-validation**: HLSL fragments (including imported functions) are
  compiled with `fxc -T ps_5_0 -E main -Od -Fc out.asm in.hlsl` and compared
  against the translator output
- Read-component comparison against known-good reference output
- `python tests/validate_asm.py <output>` — structural validator

## Known limitations

TextureCube/3D sampling (`sample_cube`), array writes / dynamic indexing,
`do-while`, `struct`, `transpose`, `mul(M1, M2)` (matrix × matrix), and a
handful of exotic intrinsics (`atan/asin/acos/tan` series expansions, etc.) are
not yet implemented. The full gap list is maintained in the project notes.

## References

- **DXC** — primary reference: `https://github.com/microsoft/DirectXShaderCompiler`
- **vkd3d** — secondary reference: `https://gitlab.winehq.org/wine/vkd3d`
- **3Dmigoto** — a shader-modding tool that consumes this assembly format

## License

GPL-3.0. See [LICENSE](LICENSE).
