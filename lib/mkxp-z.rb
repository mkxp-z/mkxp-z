# frozen_string_literal: true

require_relative "mkxp-z/version"

$mkxpz_launch_args ||= []

module MKXP_Z
  class Error < StandardError; end

end

require_relative "mkxp-z/mkxpz"