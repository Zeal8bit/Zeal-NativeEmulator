import sys
import re

def patch_file(path):
    pattern = re.compile(
        r'window\.addEventListener\(\'(keydown|keypress|keyup)\', GLFW\.onKey\1, true\);'
    )

    pattern = re.compile(
        r'window\.addEventListener\(\'keydown\', GLFW\.onKeydown, true\);\s*'
        r'window\.addEventListener\(\'keypress\', GLFW\.onKeyPress, true\);\s*'
        r'window\.addEventListener\(\'keyup\', GLFW\.onKeyup, true\);',
        re.MULTILINE
    )

    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()

    replacement = (
        "Browser.getCanvas().addEventListener('keydown', GLFW.onKeydown, true);\n"
        "      Browser.getCanvas().addEventListener('keypress', GLFW.onKeyPress, true);\n"
        "      Browser.getCanvas().addEventListener('keyup', GLFW.onKeyup, true);"
    )

    new_content, count = pattern.subn(replacement, content)

    if count == 0:
        print("[WASM_PATCH] No matching lines found to patch.")
    else:
        with open(path, 'w', encoding='utf-8') as f:
            f.write(new_content)
        print(f"[WASM_PATCH] Patched {count} occurrence(s).")

if __name__ == "__main__":

    if len(sys.argv) != 2:
        print("Usage: python patch_glfw_listeners.py <path_to_file>")
        sys.exit(1)

    patch_file(sys.argv[1])
