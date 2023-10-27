# frozen_string_literal: true

require_relative "mkxp_z/version"

module MKXP_Z

  @@launch_args = []
  
  def self.get_launch_args
    @@launch_args
  end

  def self.set_launch_args(*args)
    @@launch_args = args
  end

  class Error < StandardError; end

end

require_relative "mkxp_z/mkxp_z"