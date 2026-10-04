# ClusterFuzzLite builds the fuzz target in OSS-Fuzz's image; build.sh does the rest
FROM gcr.io/oss-fuzz-base/base-builder
COPY . $SRC/aargh
WORKDIR $SRC/aargh
COPY .clusterfuzzlite/build.sh $SRC/
