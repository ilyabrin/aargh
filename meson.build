# argh.h as a Meson subproject: put the repository in subprojects/argh, then
#   argh_dep = dependency('argh', fallback: ['argh', 'argh_dep'])
# As before, define ARGH_IMPLEMENTATION in one source file of your program.
project('argh', 'c',
  license: 'MIT',
  meson_version: '>= 0.57',
  default_options: ['c_std=c99'])

# The version lives in argh.h only
argh_version = import('fs').read('argh.h').split('#define ARGH_VERSION "')[1].split('"')[0]

argh_dep = declare_dependency(
  include_directories: include_directories('.'),
  version: argh_version)
meson.override_dependency('argh', argh_dep)

install_headers('argh.h')
import('pkgconfig').generate(
  name: 'argh',
  description: 'Single-header command-line argument parser for C99',
  url: 'https://github.com/ilyabrin/argh',
  version: argh_version)
