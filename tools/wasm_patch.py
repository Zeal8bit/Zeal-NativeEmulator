import sys
import re


def patch_file(path):
    # Define pattern-replacement pairs as a list of tuples
    patches = [
        (
            re.compile(
                r'window\.addEventListener\(\'keydown\', GLFW\.onKeydown, true\);\s*'
                r'window\.addEventListener\(\'keypress\', GLFW\.onKeyPress, true\);\s*'
                r'window\.addEventListener\(\'keyup\', GLFW\.onKeyup, true\);',
                re.MULTILINE,
            ),
            "Browser.getCanvas().addEventListener('keydown', GLFW.onKeydown, true);\n"
            "      Browser.getCanvas().addEventListener('keypress', GLFW.onKeyPress, true);\n"
            "      Browser.getCanvas().addEventListener('keyup', GLFW.onKeyup, true);",
        ),
        (
            re.compile(r'          case 0x3B:return 59; // DOM_VK_SEMICOLON -> GLFW_KEY_SEMICOLON'),
            "          case 0x3B:return 59; // DOM_VK_SEMICOLON -> GLFW_KEY_SEMICOLON\n"
            "          case 0xBA:return 59; // DOM_VK_SEMICOLON -> GLFW_KEY_SEMICOLON",
        ),
        (
            re.compile(r'var _emscripten_set_window_title = \(title\) => document.title = UTF8ToString\(title\);'),
            "var _emscripten_set_window_title = () => null;"
        )
    ]

    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()

    total_count = 0
    for pattern, replacement in patches:
        content, count = pattern.subn(replacement, content)
        total_count += count

    if total_count == 0:
        print("[WASM_PATCH] No matching lines found to patch.")
    else:
        with open(path, 'w', encoding='utf-8') as f:
            f.write(content)
        print(f"[WASM_PATCH] Patched {total_count} occurrence(s).")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: python patch_glfw_listeners.py <path_to_file>")
        sys.exit(1)

    patch_file(sys.argv[1])
