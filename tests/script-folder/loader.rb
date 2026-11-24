# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# The Main section that the Essentials v21 script extractor writes,
# cut down to what the engine looks for.

def script_folder_loader_ready?
  true
end

def load_scripts_from_folder(path)
  files   = []
  folders = []
  Dir.foreach(path) do |f|
    next if ['.', '..'].include?(f)

    if File.directory?("#{path}/#{f}")
      folders.push(f) unless f =~ /^\./
    elsif f =~ /\.rb$/i
      files.push(f)
    end
  end
  files.sort.each do |f|
    code = File.read("#{path}/#{f}")
    eval(code, nil, f) # rubocop:disable Security/Eval
  end
  folders.sort.each do |folder|
    load_scripts_from_folder("#{path}/#{folder}")
  end
end

load_scripts_from_folder('Data/Scripts')
