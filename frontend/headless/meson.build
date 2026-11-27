headless_sources = files(
    'console.c',
    'console_debug.c',
    'console_keys.c',
    'console_screenshot.c',
    'frontend.c',
    'main.c',
)

headless_sources += custom_target('headless-skill',
    input: 'SKILL.md',
    output: 'skill.inc',
    command: [import('python').find_installation(), files('../../tools/file_to_string.py'), '@INPUT@'],
    capture: true,
)
