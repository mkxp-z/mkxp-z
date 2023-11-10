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
    MKXP_Z.init_game_state("MKXP-Z", [], false)
    color = Color.new(255, 240, 20)
    assert_equal(255, color.red)
    assert_equal(240, color.green)
    assert_equal(20, color.blue)
    assert_equal(255, color.alpha)
  end
end
