shader_files = [
  'gfx_shader.glsl',
  'text_shader.glsl',
  'text_debug.glsl',
  'bitmap_shader.glsl',
  'gfx_debug.glsl'
]

foreach f : shader_files
  configure_file(
    input: f,
    output: f,
    copy: true
  )
endforeach
