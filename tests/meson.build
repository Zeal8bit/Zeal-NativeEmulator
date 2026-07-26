keyboard_stdin_test = executable(
    'keyboard-stdin-test',
    [
        'keyboard_stdin_test.c',
        '../hw/host/stdin.c',
        '../hw/keyboard.c',
        '../utils/fifo.c',
    ],
    c_args: ['-DHOST_STDIN_TESTING'],
    include_directories: ['../include'],
)

test('keyboard-stdin', keyboard_stdin_test)

if host_machine.system() != 'windows'
    python = find_program('python3')
    keyboard_stdin_integration = files('keyboard_stdin_integration.py')

    test(
        'keyboard-stdin-pty',
        python,
        args: [
            keyboard_stdin_integration,
            'pty',
            exec.full_path(),
            join_paths(meson.project_build_root(), 'roms', 'default.img'),
        ],
        timeout: 15,
    )

    test(
        'keyboard-stdin-headless',
        python,
        args: [
            keyboard_stdin_integration,
            'pipe',
            exec.full_path(),
            join_paths(meson.project_build_root(), 'roms', 'default.img'),
        ],
        timeout: 15,
    )
endif
