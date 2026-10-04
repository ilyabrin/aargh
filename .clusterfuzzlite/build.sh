#!/bin/bash -eu
# The fuzz target, its saved inputs as the seed corpus, and its dictionary.
# $CC, $CFLAGS (the sanitizer) and $LIB_FUZZING_ENGINE come from the image.
$CC $CFLAGS -std=c99 -c tests/fuzz_argh.c -o fuzz_argh.o
$CXX $CXXFLAGS $LIB_FUZZING_ENGINE fuzz_argh.o -o "$OUT/fuzz_argh"
zip -q -j "$OUT/fuzz_argh_seed_corpus.zip" tests/fuzz/*
cp tests/fuzz_argh.dict "$OUT/fuzz_argh.dict"
