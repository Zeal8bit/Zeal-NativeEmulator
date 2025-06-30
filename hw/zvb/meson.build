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
    'zvb_sound.c'
])

configure_file(
    copy: true,
    input: 'gfx_shader.glsl',
    output: 'gfx_shader.glsl'
)

configure_file(
    copy: true,
    input: 'text_shader.glsl',
    output: 'text_shader.glsl'
)
