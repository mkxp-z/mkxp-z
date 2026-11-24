# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# Script-folder suite. A game whose one Scripts.rxdata section evals
# every .rb file of Data/Scripts must still get the engine postloads after
# its classes exist and before this last file runs the game.

$script_folder_order << '999_main/999_main.rb'

EngineTest.suite('script-folder', 4)

EngineTest.test('the postloads see the classes of Data/Scripts') do
  EngineTest.assert(Game.respond_to?(:msgbox),
                    'Game.msgbox is missing, so rgss_plugin_stubs ran before Game existed')
end

EngineTest.test('only the .rb files outside dot folders run, in the order of the loader') do
  EngineTest.assert_equal(['001_first.rb', '002_game.rb', '001_folder/001_late.rb', '999_main/999_main.rb'],
                          $script_folder_order, 'order')
end

EngineTest.test('the definitions of the loader section stay') do
  EngineTest.assert(respond_to?(:script_folder_loader_ready?, true),
                    'script_folder_loader_ready? is missing')
end

EngineTest.test('$RGSS_SCRIPTS keeps the packed sections') do
  names = $RGSS_SCRIPTS.map { |section| section[1] }
  EngineTest.assert_equal(%w[harness Main], names, 'section names')
end

EngineTest.finish
exit
