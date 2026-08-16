#!/usr/bin/env python3
# Validate DXBC asm output structure (both generated and passthrough lines).
# Usage: python validate_asm.py <output.txt>
import re
import sys

# Expected operand counts for sm5 mnemonics the translator emits.
# (Passthrough mnemonics not listed are only structurally checked.)
EXPECTED = {
    'add': 3, 'and': 3, 'div': 3, 'dp2': 3, 'dp3': 3, 'dp4': 3, 'eq': 3,
    'exp': 2, 'frc': 2, 'ftoi': 2, 'ftou': 2, 'ge': 3, 'iadd': 3, 'idiv': 3,
    'ieq': 3, 'if_nz': 1, 'if_z': 1, 'imul': 3, 'ine': 3, 'inot': 2,
    'ishl': 3, 'ishr': 3, 'itof': 2, 'log': 2, 'lt': 3, 'mad': 4, 'mad_sat': 4,
    'max': 3, 'min': 3, 'mov': 2, 'mov_sat': 2, 'movc': 4, 'mul': 3,
    'mul_sat': 3, 'ne': 3, 'not': 2, 'or': 3, 'rcp': 2, 'round_ne': 2,
    'round_ni': 2, 'round_pi': 2, 'round_z': 2, 'rsq': 2, 'sample': 4,
    'sample_b': 5, 'sample_c': 5, 'sample_d': 6,
    'sincos': 3, 'sqrt': 2, 'udiv': 3, 'uge': 3, 'ult': 3, 'umod': 3,
    'ushr': 3, 'utof': 2, 'xor': 3,
}

# Operand forms 3Dmigoto accepts (permissive):
#   [modifiers] register[.swizzle]  where modifiers = - and/or |...|
#   l(...) immediates,  null
REG = re.compile(
    r'^(?:-(?:\|[^|]+\|)?|\|[^|]+\||-)?'
    r'(?:[rvot][0-9]+|cb[0-9]+\[[^]]+\]|s[0-9]+|x[0-9]+|null)'
    r'(?:\.[xyzw]{1,4})?$')
ABS = re.compile(r'^-?\|[^|]+\|$')
IMM = re.compile(r'^(?:-|\|)?l\([^)]*\)$')


def strip_indexable(mnem):
    # ld_indexable(texture2d)(float,...) -> ld_indexable
    return re.match(r'^([a-z_0-9]+)', mnem).group(1)


def split_operands(rest):
    parts, cur, depth = [], [], 0
    for ch in rest:
        if ch == '(':
            depth += 1; cur.append(ch)
        elif ch == ')':
            depth -= 1; cur.append(ch)
        elif ch == ',' and depth == 0:
            parts.append(''.join(cur).strip()); cur = []
        else:
            cur.append(ch)
    if cur:
        parts.append(''.join(cur).strip())
    return parts


def main():
    path = sys.argv[1]
    errors, n = [], 0
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        for ln, raw in enumerate(f, 1):
            line = raw.rstrip('\r\n')
            stripped = line.lstrip()
            if not stripped or stripped.startswith('//'):
                continue
            if stripped == 'ps_5_0' or stripped.startswith('dcl_'):
                continue
            if stripped in ('else', 'endif', 'loop', 'endloop', 'ret', 'break', 'continue', 'discard'):
                continue
            if stripped in ('switch', 'endswitch', 'default'):
                continue
            if re.match(r'^case l\(', stripped):
                continue
            m = re.match(r'^(\S+)\s+(.*)$', stripped)
            if not m:
                errors.append(f'{ln}: cannot parse: {line[:80]}')
                continue
            mnem_raw, rest = m.group(1), m.group(2)
            rest = rest.split('//')[0].strip()
            mnem = strip_indexable(mnem_raw)
            if mnem in EXPECTED:
                n += 1
                ops = split_operands(rest)
                expected = EXPECTED[mnem]
                ok_count = expected == len(ops)
                # imul has a "null" dest form: imul null, a, b (3) or imul dst, a, b, c (4)
                if not ok_count and mnem == 'imul' and len(ops) in (3, 4):
                    ok_count = True
                if not ok_count:
                    errors.append(f'{ln}: {mnem}: expected {expected} ops, got {len(ops)}: {rest[:60]}')
                    continue
                for op in ops:
                    op = re.sub(r'\{[^}]*\}', '', op).strip()  # strip min16f markers
                    if not (REG.match(op) or IMM.match(op) or ABS.match(op)):
                        errors.append(f'{ln}: {mnem}: bad operand "{op}"')
    print(f'instructions checked (emitted set): {n}')
    if errors:
        print(f'ERRORS ({len(errors)}):')
        for e in errors[:40]:
            print('  ' + e)
        sys.exit(1)
    print('OK: all operands well-formed')


if __name__ == '__main__':
    main()
