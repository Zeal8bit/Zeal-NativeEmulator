sources += files([
    'default_font.c',
    'zvb.c',
    'zvb_font.c',
    'zvb_palette.c',
    'zvb_sprites.c',
    'zvb_text.c',
    'zvb_spi.c',
    'zvb_tilemap.c',
    'zvb_tileset.c',
    'zvb_crc32.c',
    'zvb_dma.c',
    'zvb_sound.c'
])

if (get_option('blitter_software'))
    sources += files([
        'blitter/software.c'
    ])
else
    sources += files([
        'blitter/shader.c'
    ])
endif
