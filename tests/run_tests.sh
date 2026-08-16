#!/bin/bash
# Regression test suite for the HLSL Blend DXBC Translator.
# Usage: bash tests/run_tests.sh
set -u
cd "$(dirname "$0")/.." || exit 1
EXE=x64/Release/hlsl_blend_dxbc_translator.exe
DATA=data
TMP=/tmp/hb_tests
mkdir -p "$TMP"
pass=0; fail=0

check() {
    local name="$1"; local in="$2"; shift 2
    if "$EXE" -input "$in" -output "$TMP/out.txt" -data "$DATA" >/dev/null 2>&1; then
        local errs; errs=$(grep -c "Error:" "$TMP/out.txt" 2>/dev/null); errs=${errs:-0}
        if [ "$errs" = "0" ]; then
            if python tests/validate_asm.py "$TMP/out.txt" >/dev/null 2>&1; then
                echo "PASS: $name"; pass=$((pass+1))
            else
                echo "FAIL(结构): $name"; fail=$((fail+1))
            fi
        else
            echo "FAIL($errs errors): $name"; fail=$((fail+1))
        fi
    else
        echo "FAIL(崩溃): $name"; fail=$((fail+1))
    fi
}

echo "== 真实混合 shader =="
for f in tests/*HLSLBlend*.txt; do
    check "$(basename "$f")" "$f"
done

echo "== 语法示例 =="
check "sample.txt" tests/sample.txt

echo "== 特性测试 =="
# int/uint bitwise
cat > "$TMP/t_int.txt" << 'EOF'
HLSL uint FrameIndex = 42;
HLSL int SignedVal = -7;
HLSL uint Masked = FrameIndex & 0xFF;
HLSL uint Shifted = FrameIndex >> 2;
HLSL uint OrResult = Masked | Shifted;
HLSL uint XorResult = Masked ^ 0x0F;
HLSL int NotVal = ~SignedVal;
HLSL uint LeftShift = FrameIndex << 3;
HLSL float F = float(Masked) * 2.0;
EOF
check "int/uint 位运算" "$TMP/t_int.txt"

# loops + int ops
cat > "$TMP/t_loop.txt" << 'EOF'
HLSL int i = 0;
HLSL float Sum = 0.0;
HLSLSnippet {
    for (i = 0; i < 4; i += 1) {
        Sum += 0.5;
    }
    while (i > 0) {
        i -= 1;
        Sum *= 2.0;
    }
    i++;
}
EOF
check "循环+int运算" "$TMP/t_loop.txt"

# sampling variants
cat > "$TMP/t_sample.txt" << 'EOF'
HLSLTexture Texture2D Tex = t0;
HLSLTexture Texture2D ShadowMap = t7;
HLSLSampler SamplerState Samp = s0;
HLSLSampler SamplerComparisonState ShadowSamp = s1;
HLSL float2 UV = float2(0.25, 0.75);
HLSL float Depth = 0.9;
HLSLSnippet {
    float4 C = Tex.Sample(Samp, UV);
    float4 L = Tex.SampleLevel(Samp, UV, 2.0);
    float S = ShadowMap.SampleCmp(ShadowSamp, UV, Depth);
}
EOF
check "采样 Sample/SampleLevel/SampleCmp" "$TMP/t_sample.txt"

# functions + any/all + discard
cat > "$TMP/t_func.txt" << 'EOF'
HLSLFunctionImport "functions/lib.txt";
HLSL float3 X = GetToonNoL(1.0, float3(0,1,0), float3(1,0,0));
HLSL float3 N = float3(0.1, 0.2, 0.3);
HLSL float H = any(N > 0.05);
HLSL float A = all(N < 1.0);
HLSLSnippet {
    if (any(N < 0.0)) {
        discard;
    }
}
EOF
check "函数库+any/all+discard" "$TMP/t_func.txt"

echo
echo "===== 结果: $pass 通过, $fail 失败 ====="
[ "$fail" = "0" ]
