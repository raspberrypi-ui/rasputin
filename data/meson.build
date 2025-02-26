if build_standalone
  install_data('rasputin.ui', install_dir: ui_dir)
  i18n.merge_file(input: 'rasputin.desktop.in',
    output: 'rasputin.desktop',
    type: 'desktop',
    po_dir: '../po',
    install: true,
    install_dir: desktop_dir
  )
endif

if build_plugin
  install_data('rasputin.ui', install_dir: pui_dir)
endif
