# Uses argh as a Meson subproject; check.sh puts this repository in
# subprojects/argh
project('argh_package_test', 'c')
argh_dep = dependency('argh', fallback: ['argh', 'argh_dep'])
executable('app', 'app.c', dependencies: argh_dep)
