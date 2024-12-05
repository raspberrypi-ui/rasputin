install_data('rasputin.ui', install_dir: ui_dir)
if not meson.project_name().contains('rpcc')
  i18n.merge_file(input: 'rasputin.desktop.in',
      output: 'rasputin.desktop',
      type: 'desktop',
      po_dir: '../po',
      install: true,
      install_dir: desktop_dir
    )
endif
