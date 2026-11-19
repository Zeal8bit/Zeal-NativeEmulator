sources += files([
        'flash.c',
        'hostfs.c',
        'keyboard.c',
        'mmu.c',
        'pio.c',
        'ram.c',
        'semihost.c',
        'uart.c',
        'z80.c',
        'zeal.c',
        'i2c.c',
        'i2c/ds1307.c',
        'i2c/at24c512.c',
        'compactflash.c'])

subdir('userport/snes_adapter')

if debugger_api
    sources += files('zeal_debugger.c', 'zeal_trace.c')
    subdir('debugger')
endif

subdir('zvb')
