python3 = find_program('python3')
fs = import('fs')

shader_files = [
  'gfx_shader.glsl',
  'text_shader.glsl',
  'text_debug.glsl',
  'bitmap_shader.glsl',
  'gfx_debug.glsl'
]


file_to_string = meson.project_source_root() / 'tools/file_to_string.py'

foreach f : shader_files
    shader_content = fs.read(f)
    conf = configuration_data()

    # filename without extension
    shader_name = f.split('.')[0]

    # Convert each character to \xHH, this is the simplest solution to avoid having issues
    # with escaping backslash or new line characters
    byte_string = run_command(python3, file_to_string, f, check: true).stdout().strip()

    # Set the variables that will be replaced in the template
    conf.set('SHADER_NAME', shader_name)
    conf.set('SHADER_SRC', byte_string)

    configure_file(
        input: 'shader_template.h.in',
        output: shader_name + '.h',
        configuration: conf
    )
endforeach
