sources = files (
    'rasputin.c',
    'labwc.c',
    'wayfire.c',
    'openbox.c'
)

add_global_arguments('-Wno-unused-result', language : 'c')

gtk = dependency ('gtk+-3.0')
xml = dependency ('libxml-2.0')
deps = [ gtk, xml ]

executable ('rasputin', sources, dependencies: deps, install: true)
