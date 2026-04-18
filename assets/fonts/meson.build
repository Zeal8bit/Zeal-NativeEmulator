foreach font : [
    'BigBlue_Terminal_437TT.TTF',
    'BigBlue_TerminalPlus.TTF',
]
    custom_target('copy_font_' + font,
        input:  font,
        output: font,
        command: ['cp', '@INPUT@', '@OUTPUT@'],
        build_by_default: true,
    )
endforeach
