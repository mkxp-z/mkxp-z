# frozen_string_literal: true

require 'mkmf'

# @param [Dir] root_dir
# @param [String] dir_name
# @param [Integer] depth_limit
# @return [Void]
def add_include_paths(root_dir, dir_name, depth_limit = 2)
  return if depth_limit == 0
  contains_header = false
  root_dir.each_child do |child|
    child_dir = Dir.new("#{root_dir.path}/#{child}") rescue nil
    add_include_paths(child_dir, "#{dir_name}/#{child}", depth_limit - 1) unless child_dir.nil?
    contains_header = true if child =~ /\w+\.h/i
  end
  $INCFLAGS += " -I#{root_dir.path}" if contains_header
end

# @param [Dir] dir_to_check
# @return [Void]
def add_link_libraries(dir_to_check)
  dir_to_check.each_child do |file|
    $LOCAL_LIBS += " -L#{dir_to_check.path}/#{file}" if file =~ /lib.*\.a$/i
  end
end

extension_name = "mkxp_z"

$CPPFLAGS += " -D MKXPZ_BUILD_MESON -D MKXPZ_VERSION=2.4.2 -D MKXPZ_GIT_HASH=13f6452f65105b66d5bed080b70dedb64ccff190 -D HAVE_NANOSLEEP"

external_dir = "windows/build-mingw64"
add_include_paths(Dir.new("#{__dir__}/#{external_dir}/include"), "#{external_dir}/include")
add_link_libraries(Dir.new("#{__dir__}/#{external_dir}/lib"))

add_include_paths(Dir.new("#{__dir__}/assets"), "assets")
add_include_paths(Dir.new("#{__dir__}/scripts"), "scripts")
add_include_paths(Dir.new("#{__dir__}/windows"), "windows")
add_include_paths(Dir.new("#{__dir__}/shader"), "shader")
add_include_paths(Dir.new("#{__dir__}/src"), "src", 4)
add_link_libraries(Dir.new("#{__dir__}/build"))

dir_config(extension_name)
create_makefile(extension_name)