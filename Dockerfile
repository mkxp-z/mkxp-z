FROM ruby:3.1.4
LABEL authors="fcors"

SHELL ["/bin/bash", "-c"]

RUN apt-get update
RUN apt-get install -y git build-essential cmake meson autoconf automake libtool pkg-config bison zlib1g-dev  \
    libbz2-dev xorg-dev libgl1-mesa-dev libasound2-dev libpulse-dev wget xxd


COPY . build/mkxp-z
WORKDIR build/mkxp-z/linux
RUN make
RUN source vars.sh; cd ..; meson build -Dbuild_gem=true; cd build && ninja

WORKDIR ..
RUN gem build mkxp-z.gemspec
RUN GEMNAME=$(find -type f -name 'mkxp-z-*.gem') && echo $GEMNAME && gem install $GEMNAME