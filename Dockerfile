FROM ruby:3.1.4 AS base

RUN apt-get update && apt-get --no-install-recommends install -y git build-essential cmake meson autoconf automake libtool pkg-config bison  \
    zlib1g-dev libbz2-dev xorg-dev libgl1-mesa-dev libasound2-dev libpulse-dev wget xxd && apt-get clean

FROM base AS build

USER nonroot

SHELL ["/bin/bash", "-c"]

COPY . build/mkxp-z
WORKDIR build/mkxp-z/linux
RUN export CMAKE_EXTRA_ARGS="-DCMAKE_POSITION_INDEPENDENT_CODE=ON" && export EXTRA_CONFIG_OPTIONS=" --with-pic" && make deps-core
RUN source vars.sh; cd ..; meson build -Dbuild_gem=true; cd build && ninja

WORKDIR ..
RUN gem build mkxp-z.gemspec
RUN GEMNAME=$(find -type f -name 'mkxp-z-*.gem') && echo $GEMNAME && gem install $GEMNAME