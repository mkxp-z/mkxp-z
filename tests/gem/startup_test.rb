# frozen_string_literal: true

require "minitest/autorun"
require "mkxp-z"

class StartupTest < Minitest::Test
  def setup
    # Do nothing
  end

  def teardown
    # Do nothing
  end

  def test_gem_startup
    assert_equal(true, MKXP_Z.init_game_state("MKXP-Z", [], false))
  end
end
