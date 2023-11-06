FROM ubuntu:22.04
LABEL authors="fcors"

RUN apt-get update
RUN apt-get install -y git build-essential cmake meson autoconf automake libtool pkg-config ruby bison zlib1g-dev  \
    libbz2-dev xorg-dev libgl1-mesa-dev libasound2-dev libpulse-dev
RUN apt-get install -y wget xxd

COPY . build/mkxp-z
WORKDIR build/mkxp-z/linux
#RUN make

ENV DIR build/mkxp-z/linux
ENV MKXPZ_PREFIX x86_64
ENV LDFLAGS -L$DIR/build-${MKXPZ_PREFIX}/lib
ENV CFLAGS -I$DIR/build-${MKXPZ_PREFIX}/include

RUN export MKXPZ_OLD_PC=$(pkg-config --variable pc_path pkg-config)
ENV PKG_CONFIG_LIBDIR $DIR/build-${MKXPZ_PREFIX}/lib/pkgconfig:$DIR/build-${MKXPZ_PREFIX}/lib64/pkgconfig:${MKXPZ_OLD_PC}
ENV LD_LIBRARY_PATH $DIR/build-${MKXPZ_PREFIX}/lib:${LD_LIBRARY_PATH}
ENV MKXPZ_PREFIX $DIR/build-${MKXPZ_PREFIX}

WORKDIR ..
RUN echo $PKG_CONFIG_LIBDIR && meson build -Dbuild_gem=true
RUN cd build && ninja install

WORKDIR ..
RUN gem build mkxp-z.gemspec
RUN GEMNAME=$(find -type f -name 'mkxp-z-\d+\.\d+\.\d+\.gem') && gem install $GEMNAME


