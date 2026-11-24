# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# A Windows game reads its text files in text mode, which drops the CR of
# each CR LF, and it reads them as UTF-8. LonaRPG's JSON parser recurses
# forever on a CR, and it encodes its translation files to UTF-8.
#
#   tools/run-engine-tests.sh --suite text_reads.rb --ruby 31
#   tools/run-engine-tests.sh --suite text_reads.rb --ruby 19
#   tools/run-engine-tests.sh --suite text_reads.rb --ruby 18
#   tools/run-engine-tests.sh --suite text_reads.rb --ruby 31 \
#     --config '{"syntaxTransform":"legacy","rgssVersion":1}'
#
# This file must parse on Ruby 1.8, 1.9 and 3.1.

unless defined?(EngineTest)
  harness_path = File.join(File.dirname(__FILE__), 'harness.rb')
  eval(File.read(harness_path), TOPLEVEL_BINDING, harness_path) # rubocop:disable Security/Eval
end

EngineTest.suite('text-reads', defined?(Encoding) ? 10 : 9)

EngineTest.load_preloads(%w[windows_fs])

# rubocop:disable Style/FileWrite -- File.binwrite is not on Ruby 1.8.
TEXT_FILE = 'text_reads.json'.freeze
File.open(TEXT_FILE, 'wb') { |file| file.write("[\r\n1,\r\n2\r\n]\r\n") }
# rubocop:enable Style/FileWrite

# Ruby 1.8 takes no "t" in a mode, so it keeps the CR.
FOLDED = System.ruby_version == '1.8' ? "[\r\n1,\r\n2\r\n]\r\n" : "[\n1,\n2\n]\n"

EngineTest.test('File.read drops the CR of each CR LF') do
  EngineTest.assert_equal(FOLDED, File.read(TEXT_FILE), 'File.read')
end

# rubocop:disable Style/FileRead -- these tests read through File.open with a mode, and File.binread is not on Ruby 1.8.
EngineTest.test('a block read in mode r drops the CR of each CR LF') do
  EngineTest.assert_equal(FOLDED, File.open(TEXT_FILE, 'r', &:read), "File.open 'r'")
end

EngineTest.test('a binary read keeps the CR') do
  EngineTest.assert_equal("[\r\n1,\r\n2\r\n]\r\n", File.open(TEXT_FILE, 'rb', &:read), "File.open 'rb'")
end

EngineTest.test('a read in mode r+ keeps the CR') do
  EngineTest.assert_equal("[\r\n1,\r\n2\r\n]\r\n", File.open(TEXT_FILE, 'r+', &:read), "File.open 'r+'")
end

EngineTest.test('a read with an encoding drops the CR of each CR LF') do
  mode = defined?(Encoding) ? 'r:utf-8' : 'r'
  EngineTest.assert_equal(FOLDED, File.open(TEXT_FILE, mode, &:read), "File.open '#{mode}'")
end

EngineTest.test('File.new drops the CR of each CR LF') do
  file = File.new(TEXT_FILE)
  begin
    EngineTest.assert_equal(FOLDED, file.read, 'File.new')
  ensure
    file.close
  end
end
# rubocop:enable Style/FileRead

EngineTest.test('File.readlines drops the CR of each CR LF') do
  EngineTest.assert_equal(FOLDED.lines.to_a, File.readlines(TEXT_FILE), 'File.readlines')
end

EngineTest.test('File.foreach drops the CR of each CR LF') do
  lines = []
  File.foreach(TEXT_FILE) { |line| lines << line }
  EngineTest.assert_equal(FOLDED.lines.to_a, lines, 'File.foreach')
end

# Ruby 1.9 keeps converted text in a buffer after gets, and a byte read
# from that buffer can raise.
EngineTest.test('a read after gets gives the next bytes') do
  rest = File.open(TEXT_FILE, 'r') { |file| [file.gets, file.read(2)] }
  first = FOLDED.lines.to_a[0]
  EngineTest.assert_equal([first, FOLDED[first.length, 2]], rest, 'gets then read(2)')
end

# Ruby 1.8 has no Encoding.
# A game that targets Ruby 1.8 on Ruby 3.1 reads raw bytes.
if defined?(Encoding)
  EngineTest.test('a text file reads in the encoding of the target Ruby') do
    legacy = CFG['syntaxTransform'] == 'legacy' && CFG['rgssVersion'] != 3
    expected = legacy ? Encoding::ASCII_8BIT : Encoding::UTF_8
    EngineTest.info('syntaxTransform', CFG['syntaxTransform'].inspect)
    EngineTest.assert_equal(expected, Encoding.default_external, 'Encoding.default_external')
    EngineTest.assert_equal(expected, File.read(TEXT_FILE).encoding, 'File.read encoding')
  end
end

File.delete(TEXT_FILE)

EngineTest.finish
exit
