# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# Dir.chdir through windows_fs.rb on the interpreters that the engine
# ships. tools/test_save_fs.rb runs only on the host Ruby.
#
#   tools/run-engine-tests.sh --suite dir_chdir.rb --ruby 18
#
# This file must parse on all three.

unless defined?(EngineTest)
  harness_path = File.join(File.dirname(__FILE__), 'harness.rb')
  eval(File.read(harness_path), TOPLEVEL_BINDING, harness_path) # rubocop:disable Security/Eval
end

EngineTest.suite('dir-chdir', 3)

EngineTest.load_preloads(%w[windows_fs])

CHDIR_GAME = Dir.pwd
CHDIR_SUB = File.join(CHDIR_GAME, 'dir_chdir_suite')
Dir.mkdir(CHDIR_SUB) unless File.directory?(CHDIR_SUB)

# The error class and the folder after the call, read before the
# folder goes back to the game folder.
def chdir_result
  error = nil
  begin
    yield
  rescue Exception => e # rubocop:disable Lint/RescueException
    error = e.class
  end
  [error, Dir.pwd]
ensure
  Dir.chdir(CHDIR_GAME)
end

EngineTest.test('Dir.chdir(nil) raises TypeError and stays in the game folder') do
  EngineTest.assert_equal([TypeError, CHDIR_GAME], chdir_result { Dir.chdir(nil) }, 'Dir.chdir(nil)')
  EngineTest.assert_equal([TypeError, CHDIR_GAME], chdir_result { Dir.chdir(nil) { nil } },
                          'Dir.chdir(nil) with a block')
end

EngineTest.test('Dir.chdir("") raises EINVAL and stays in the game folder') do
  EngineTest.assert_equal([Errno::EINVAL, CHDIR_GAME], chdir_result { Dir.chdir('') }, 'Dir.chdir("")')
  EngineTest.assert_equal([Errno::EINVAL, CHDIR_GAME], chdir_result { Dir.chdir('') { nil } },
                          'Dir.chdir("") with a block')
end

EngineTest.test('a NUL in a Dir.chdir path acts like RGSS') do
  if RUBY_VERSION < '1.9'
    seen = Dir.chdir("dir_chdir_suite\0junk") { Dir.pwd }
    EngineTest.assert_equal(CHDIR_SUB, seen, 'Ruby 1.8 ends the path at the NUL')
    EngineTest.assert_equal([Errno::EINVAL, CHDIR_GAME], chdir_result { Dir.chdir("\0junk") },
                            'Ruby 1.8 with a NUL first')
  else
    EngineTest.assert_equal([ArgumentError, CHDIR_GAME], chdir_result { Dir.chdir("dir_chdir_suite\0junk") },
                            'Ruby 1.9 and later refuse a NUL')
  end
end

Dir.rmdir(CHDIR_SUB)

EngineTest.finish
exit
