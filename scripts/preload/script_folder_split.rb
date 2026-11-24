# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# The script extractor of Essentials v21 games (Pokémon Re:Union)
# leaves one Scripts.rxdata section that defines
# load_scripts_from_folder and calls it on Data/Scripts, and the last
# file it evals runs the game loop. The engine runs its postloads
# before the last section, so they would run before any game class
# exists. binding-mri.cpp calls MKXPScriptFolder.expand to split that
# section into the loader's definitions plus one section per file, in
# the loader's order: the files of a folder by name, then its
# subfolders by name. The loader takes only the .rb files and skips
# the folders whose name starts with a dot. Plugins keep a meta.txt
# next to their scripts.
module MKXPScriptFolder
  CALL = %r{^[ \t]*load_scripts_from_folder\([ \t]*(["'])Data/Scripts\1[ \t]*\)[ \t]*\r?$}.freeze
  DEFINITION = /^[ \t]*def load_scripts_from_folder\b/.freeze

  # Returns nil when the section at index is not that loader.
  def self.expand(scripts, index)
    main = scripts[index]
    return nil unless main.is_a?(Array) && main[3].is_a?(String)

    code = main[3]
    return nil unless code =~ DEFINITION && code =~ CALL

    main = main.dup
    main[3] = code.sub(CALL, '')
    scripts[0, index] + [main] + sections_in('Data/Scripts') + scripts[(index + 1)..-1]
  end

  def self.sections_in(path)
    entries = Dir.entries(path) - ['.', '..']
    folders, files = entries.partition { |f| File.directory?("#{path}/#{f}") }
    folders = folders.reject { |f| f =~ /^\./ }
    files = files.grep(/\.rb$/i)
    sections = files.sort.map { |f| [0, f, '', File.read("#{path}/#{f}")] }
    folders.sort.each { |f| sections.concat(sections_in("#{path}/#{f}")) }
    sections
  end
end
