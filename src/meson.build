sources = files (
    'lxinput.c'
)

add_global_arguments('-Wno-unused-result', language : 'c')

gtk = dependency ('gtk+-3.0')
xml = dependency ('libxml-2.0')
x11 = dependency ('x11')
deps = [ gtk, xml, x11 ]

executable ('rasputin', sources, dependencies: deps, install: true)
