#!/usr/bin/env python3
import ast
import pathlib
import sys


def extract_c_string(source: str, name: str) -> str:
    marker = f"static const char *{name} ="
    start = source.find(marker)
    if start < 0:
        raise AssertionError(f"missing {name}")

    pieces = []
    for line in source[start + len(marker):].splitlines():
        token = line.strip()
        if not token:
            continue
        if not token.startswith('"'):
            break
        terminal = token.endswith('";')
        if terminal:
            token = token[:-1]
        pieces.append(ast.literal_eval(token))
        if terminal:
            break

    if not pieces:
        raise AssertionError(f"empty {name}")
    return "".join(pieces)


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> int:
    if len(sys.argv) != 2:
        raise SystemExit("usage: shader_source_test.py path/to/limit_set_renderer.c")

    source = pathlib.Path(sys.argv[1]).read_text()
    vertex = extract_c_string(source, "vertex_shader_source")
    fragment = extract_c_string(source, "fragment_shader_source")

    require(vertex.count("void main()") == 1, "vertex shader must have exactly one main")
    require("gl_Position" in vertex, "vertex shader must write gl_Position")
    for fragment_only in ("fragment_color", "gl_FragCoord", "discard", "u_initial_region"):
        require(fragment_only not in vertex, f"vertex shader leaked fragment token: {fragment_only}")

    require(fragment.count("void main()") == 1, "fragment shader must have exactly one main")
    for required in (
        "fragment_color",
        "gl_FragCoord",
        "u_initial_region",
        "containing_next_region",
        "ivec3 candidate = ivec3(previous_region, opposite_pair, opposite_pair + 1)",
    ):
        require(required in fragment, f"fragment shader missing token: {required}")
    require("int containing_region(vec2 z)" not in fragment,
            "hot orbit path must not restore the four-circle classifier")
    require("gl_Position" not in fragment, "fragment shader leaked vertex gl_Position")

    for previous_region in range(4):
        opposite_pair = 2 - 2 * (previous_region // 2)
        candidate = (previous_region, opposite_pair, opposite_pair + 1)
        forbidden_inverse = previous_region ^ 1
        require(len(set(candidate)) == 3, "reduced candidate set must contain three regions")
        require(forbidden_inverse not in candidate, "reduced candidate set included paired inverse")
        require(set(candidate) == set(range(4)) - {forbidden_inverse},
                "reduced candidate set must contain every non-inverse region")

    print("embedded shader boundaries and reduced-word classifier: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
