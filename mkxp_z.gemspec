# frozen_string_literal: true

require_relative "lib/mkxp_z/version"

Gem::Specification.new do |spec|
  spec.name = "mkxp_z"
  spec.version = MkxpZ::VERSION
  spec.authors = ["Francesco Corso"]
  spec.email = ["fcorso2016@gmail.com"]

  spec.summary = "Ruby Gem for MKXP-Z"
  spec.description = "Convert MKXP-Z from a static exe to a library that can be loaded in an Ruby enviornment"
  spec.homepage = "https://github.com/fcorso2016/mkxp-z-gem"
  spec.license = "MIT"
  spec.required_ruby_version = ">= 3.1.0"

  spec.metadata["homepage_uri"] = spec.homepage
  spec.metadata["source_code_uri"] = "https://github.com/fcorso2016/mkxp-z-gem.git"

  # Specify which files should be added to the gem when it is released.
  # The `git ls-files -z` loads the files in the RubyGem that have been added into git.
  spec.files = Dir["{bin,lib,ext,sig}/**/*",
                   "LICENSE.txt",
                   "Rakefile"]

  spec.extensions << "ext/mkxp_z/extconf.rb"

  spec.add_development_dependency "rake-compiler"

  # Uncomment to register a new dependency of your gem
  # spec.add_dependency "example-gem", "~> 1.0"

  # For more information and examples about making a new gem, check out our
  # guide at: https://bundler.io/guides/creating_gem.html
end
