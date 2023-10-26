# frozen_string_literal: true

require "mkmf"

# Makes all symbols private by default to avoid unintended conflict
# with other gems. To explicitly export symbols you can use RUBY_FUNC_EXPORTED
# selectively, or entirely remove this flag.
append_cflags("-fvisibility=hidden")

extension_name = "mkxp_z"
dir_config(extension_name, "#{__dir__}/include", "#{__dir__}/lib")
have_library("mkxp_z", "initBindings")

create_makefile("#{extension_name}/#{extension_name}")