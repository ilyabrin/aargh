# Uses argh as a Meson subproject; check.sh puts this repository in
# subprojects/aargh
project('argh_package_test', 'c')
aargh_dep = dependency('aargh', fallback: ['aargh', 'aargh_dep'])
executable('app', 'app.c', dependencies: aargh_dep)
