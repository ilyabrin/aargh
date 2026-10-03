# aargh (argh.h) as a Meson subproject: put the repository in
# subprojects/aargh, then
#   aargh_dep = dependency('aargh', fallback: ['aargh', 'aargh_dep'])
# As before, define ARGH_IMPLEMENTATION in one source file of your program.
project('aargh', 'c',
  license: 'MIT',
  meson_version: '>= 0.57',
  default_options: ['c_std=c99'])

# The version lives in argh.h only
argh_version = import('fs').read('argh.h').split('#define ARGH_VERSION "')[1].split('"')[0]

aargh_dep = declare_dependency(
  include_directories: include_directories('.'),
  version: argh_version)
meson.override_dependency('aargh', aargh_dep)
# The v1.8 name, deprecated until 2.0
argh_dep = aargh_dep
meson.override_dependency('argh', aargh_dep)

install_headers('argh.h', subdir: 'aargh')
import('pkgconfig').generate(
  name: 'aargh',
  description: 'Single-header command-line argument parser for C99 (argh.h)',
  url: 'https://github.com/ilyabrin/aargh',
  subdirs: 'aargh',
  version: argh_version)
