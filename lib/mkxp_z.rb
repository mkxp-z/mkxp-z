# frozen_string_literal: true

require_relative "mkxp_z/version"

$mkxpz_launch_args ||= []

module MKXP_Z
  class Error < StandardError; end

end

require_relative "mkxp_z/mkxp_z"