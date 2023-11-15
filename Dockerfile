FROM ruby:3.1.4 AS base

RUN apt-get update && apt-get --no-install-recommends install -y zlib1g-dev libbz2-dev xorg-dev libgl1-mesa-dev  \
    libasound2-dev libpulse-dev && apt-get clean

RUN apt-get update && apt-get install -y --no-install-recommends \
    mesa-utils \
    libgl1-mesa-dri \
    libgl1-mesa-glx \
    libglu1-mesa \
    mesa-common-dev && apt-get clean

RUN apt-get update && apt-get --no-install-recommends install -y \
   xvfb \
   x11-utils \
   xfonts-base \
   xfonts-75dpi \
   xfonts-100dpi \
   xfonts-scalable \
   xfonts-100dpi \
   x11-apps \
   && apt-get clean

ENV LIBGL_ALWAYS_INDIRECT=1

RUN apt-get update -y && \
    apt-get install -y --no-install-recommends alsa-utils pulseaudio libpulse0 && \
    apt-get clean

FROM base AS dev

SHELL ["/bin/bash", "-c"]

RUN apt-get update && apt-get --no-install-recommends install -y git build-essential cmake meson autoconf automake \
    libtool pkg-config bison wget xxd && apt-get clean

FROM dev AS deps
COPY ./linux/Makefile build/mkxp-z/linux/
WORKDIR build/mkxp-z/linux
RUN export CMAKE_EXTRA_ARGS="-DCMAKE_POSITION_INDEPENDENT_CODE=ON" && export EXTRA_CONFIG_OPTIONS=" --with-pic" && make deps-core

FROM dev AS build
COPY . build/mkxp-z
COPY --from=deps build/mkxp-z/linux/ build/mkxp-z/linux/
WORKDIR build/mkxp-z/linux
RUN source vars.sh; cd ..; meson build -Dbuild_gem=true -Dwith_lanczos3=false; cd build && ninja

WORKDIR ..
RUN cp build/mkxpz.so lib/mkxp-z
RUN gem build mkxp-z.gemspec

FROM base AS gem

COPY --from=build build/mkxp-z/mkxp-z-*.gem build/
COPY --from=build build/mkxp-z/startpulse.sh build/
COPY --from=build build/mkxp-z/pulseaudio.service /etc/systemd/system

WORKDIR build
RUN GEMNAME=$(find -type f -name 'mkxp-z-*.gem') && echo "$GEMNAME" && gem install "$GEMNAME"