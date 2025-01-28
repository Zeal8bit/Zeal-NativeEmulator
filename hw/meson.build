sources += files([
        'flash.c',
        'hostfs.c',
        'keyboard.c',
        'main.c',
        'mmu.c',
        'pio.c',
        'ram.c',
        'uart.c',
        'z80.c',
        'zeal.c',
        'i2c.c',
        'i2c/ds1307.c',
        'i2c/at24c512.c',
        'compactflash.c',
        'userport/snes_adapter.c'])

if get_option('enable_debugger')
    sources += files(['zeal_debugger.c', 'zeal_input.c'])
    subdir('debugger')
endif

subdir('zvb')
