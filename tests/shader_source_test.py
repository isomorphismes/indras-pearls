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
    for required in ("fragment_color", "gl_FragCoord", "u_initial_region", "containing_region"):
        require(required in fragment, f"fragment shader missing token: {required}")
    require("gl_Position" not in fragment, "fragment shader leaked vertex gl_Position")

    print("embedded shader source boundaries: ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
