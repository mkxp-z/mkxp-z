FROM ruby:3.1.4 AS base

RUN apt-get update && apt-get --no-install-recommends install -y zlib1g-dev libbz2-dev xorg-dev libgl1-mesa-dev  \
    libasound2-dev libpulse-dev && apt-get clean

RUN apt-get update && apt-get install -y --no-install-recommends \
    mesa-utils \
    libgl1-mesa-dri \
    libgl1-mesa-glx \
    libglu1-mesa \
    mesa-common-dev && apt-get clean

RUN apt-get update && apt-get install -y \
   xvfb \
   x11-utils \
   xfonts-base \
   xfonts-75dpi \
   xfonts-100dpi \
   xfonts-scalable \
   xfonts-100dpi \
   x11-apps \
   --no-install-recommends \
   && apt-get clean

ENV LIBGL_ALWAYS_INDIRECT=1

RUN apt-get update -y && \
    apt-get install -y --no-install-recommends alsa-utils pulseaudio libpulse0 && \
    apt-get clean

FROM base AS build

SHELL ["/bin/bash", "-c"]

RUN apt-get update && apt-get --no-install-recommends install -y git build-essential cmake meson autoconf automake \
    libtool pkg-config bison wget xxd && apt-get clean

COPY . build/mkxp-z
WORKDIR build/mkxp-z/linux
RUN export CMAKE_EXTRA_ARGS="-DCMAKE_POSITION_INDEPENDENT_CODE=ON" && export EXTRA_CONFIG_OPTIONS=" --with-pic" && make deps-core
RUN source vars.sh; cd ..; meson build -Dbuild_gem=true -Dwith_lanczos3=false; cd build && ninja

WORKDIR ..
RUN cp build/mkxpz.so lib/mkxp-z
RUN gem build mkxp-z.gemspec

FROM base AS gem

RUN groupadd --gid 1000 user && \
    useradd --uid 1000 --gid 1000 -m user
COPY --from=build build/mkxp-z/mkxp-z-*.gem build/
COPY --from=build build/mkxp-z/tests build/tests
COPY --from=build build/mkxp-z/Rakefile build/
COPY --from=build build/mkxp-z/fake-ui.sh build/

WORKDIR build
RUN GEMNAME=$(find -type f -name 'mkxp-z-*.gem') && echo $GEMNAME && gem install $GEMNAME

USER user