resource_files = files(
    'gamecontrollerdb.txt',
)

foreach f : ['gamecontrollerdb.txt']
    custom_target('copy_resource_' + f,
        input:  f,
        output: f,
        command: ['cp', '@INPUT@', '@OUTPUT@'],
        build_by_default: true,
    )
endforeach

install_data(
    resource_files,
    install_dir: join_paths(get_option('datadir'), 'zeal-native', 'assets', 'resources'),
)
