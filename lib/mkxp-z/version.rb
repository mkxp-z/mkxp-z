# frozen_string_literal: true

module MkxpZ
  VERSION = "2.4.2"

  def self.get_platform_name
    if RUBY_PLATFORM =~ /cygwin|mswin|mingw|bccwin|wince|emx/
      return "windows"
    elsif RUBY_PLATFORM =~ /darwin/
      return "osx"
    else
      return "linux"
    end
  end
end
