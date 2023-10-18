# frozen_string_literal: true

require 'mkmf'

system("cmake -S #{__dir__} -B . -D CMAKE_BUILD_TYPE=Release -G\"MinGW Makefiles\"")