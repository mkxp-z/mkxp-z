FROM ubuntu:22.04
LABEL authors="fcors"

SHELL ["/bin/bash", "-c"]
RUN apt-get update
RUN apt-get install -y git build-essential cmake meson autoconf automake libtool pkg-config ruby bison zlib1g-dev  \
    libbz2-dev xorg-dev libgl1-mesa-dev libasound2-dev libpulse-dev
RUN apt-get install -y wget xxd

COPY . build/mkxp-z
WORKDIR build/mkxp-z/linux
RUN make

ENV MKXPZ_PREFIX $(uname -m)
ENV LDFLAGS -L$(pwd)/build-${MKXPZ_PREFIX}/lib
ENV CFLAGS -I$(pwd)/build-${MKXPZ_PREFIX}/include
ENV PKG_CONFIG_LIBDIR $(pwd)/build-${MKXPZ_PREFIX}/lib/pkgconfig:$(pwd)/build-${MKXPZ_PREFIX}/lib64/pkgconfig:$(pkg-config --variable pc_path pkg-config)
ENV LD_LIBRARY_PATH $(pwd)/build-${MKXPZ_PREFIX}/lib:${LD_LIBRARY_PATH}
ENV MKXPZ_PREFIX $(pwd)/build-${MKXPZ_PREFIX}

WORKDIR ..
RUN meson build -Dbuild_gem=true
RUN cd build && ninja install

WORKDIR ..
RUN gem build mkxp-z.gemspec
RUN GEMNAME=$(find -type f -name 'mkxp-z-\d+\.\d+\.\d+\.gem') && gem install $GEMNAME


