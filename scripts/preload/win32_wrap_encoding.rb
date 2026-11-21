# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# The engine loads this file only on Ruby 1.9 and later, because Ruby
# 1.8 has no Encoding.

module Win32API_Impl
  module Kernel32
    CODEPAGE_TO_ENCODING = {
      65_001 => Encoding::UTF_8,
      20_127 => Encoding::US_ASCII,
      1252 => Encoding::Windows_1252,
      932 => Encoding::Windows_31J,
      949 => Encoding::CP949,
      936 => Encoding::GBK,
      950 => Encoding::Big5,
      28_591 => Encoding::ISO_8859_1,
      28_605 => Encoding::ISO_8859_15
    }.freeze

    def self.to_utf16(text, codepage)
      convert(text, CODEPAGE_TO_ENCODING[codepage] || Encoding::UTF_8, Encoding::UTF_16LE)
    end

    def self.from_utf16(wide, codepage)
      convert(wide, Encoding::UTF_16LE, CODEPAGE_TO_ENCODING[codepage] || Encoding::UTF_8)
    end

    def self.convert(text, from, to)
      text.dup.force_encoding(from)
          .encode(to, :invalid => :replace, :undef => :replace)
          .force_encoding(Encoding::ASCII_8BIT)
    rescue StandardError
      ''
    end
  end
end
