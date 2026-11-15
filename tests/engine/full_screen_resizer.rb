# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# The full-screen option of Pokemon Essentials on the interpreters that
# the engine ships. tools/test_win32_stubs.rb runs the same math only
# on the host Ruby.
#
#   tools/run-engine-tests.sh --suite full_screen_resizer.rb --ruby 18
#
# This file must parse on all three.

unless defined?(EngineTest)
  harness_path = File.join(File.dirname(__FILE__), 'harness.rb')
  eval(File.read(harness_path), TOPLEVEL_BINDING, harness_path) # rubocop:disable Security/Eval
end

EngineTest.suite('full-screen-resizer', 2)

EngineTest.load_preloads(%w[windows_fs platform_compat pokemon_compat win32_wrap])

# rubocop:disable all -- Win32API.fillScreen as Essentials ships it
class Win32API
  @@RGSSWINDOW=nil
  @@GetCurrentThreadId=Win32API.new('kernel32','GetCurrentThreadId', '%w()','l')
  @@GetWindowThreadProcessId=Win32API.new('user32','GetWindowThreadProcessId', '%w(l p)','l')
  @@FindWindowEx=Win32API.new('user32','FindWindowEx', '%w(l l p p)','l')

  def Win32API.pbFindRgssWindow
    return @@RGSSWINDOW if @@RGSSWINDOW
    processid=[0].pack('l')
    threadid=@@GetCurrentThreadId.call
    nextwindow=0
    begin
      nextwindow=@@FindWindowEx.call(0,nextwindow,"RGSS Player",0)
      if nextwindow!=0
        wndthreadid=@@GetWindowThreadProcessId.call(nextwindow,processid)
        if wndthreadid==threadid
          @@RGSSWINDOW=nextwindow
          return @@RGSSWINDOW
        end
      end
    end until nextwindow==0
    raise "Can't find RGSS player window"
  end

  def Win32API.focusWindow
    window = Win32API.new('user32', 'ShowWindow', 'LL' ,'L')
    hWnd = pbFindRgssWindow
    window.call(hWnd, 9)
  end

  def Win32API.fillScreen
    setWindowLong = Win32API.new('user32', 'SetWindowLong', 'LLL', 'L')
    setWindowPos = Win32API.new('user32', 'SetWindowPos', 'LLIIIII', 'I')
    metrics = Win32API.new('user32', 'GetSystemMetrics', 'I', 'I')
    hWnd =  pbFindRgssWindow
    width = metrics.call(0)
    height = metrics.call(1)
    setWindowLong.call(hWnd, -16, 0x00000000)
    setWindowPos.call(hWnd, 0, 0, 0, width, height, 0)
    Win32API.focusWindow
    return [width,height]
  end
end
# rubocop:enable all

def essentials_full_screen(border)
  screen_width, screen_height = Win32API.fillScreen
  width = Graphics.width
  height = Graphics.height
  border_size = 160 * border
  factor = [(2 * screen_width / (width + border_size)).floor,
            (2 * screen_height / (height + border_size)).floor].min / 2.0
  offset_x = (screen_width - (width * factor)) / (2 * factor)
  offset_y = (screen_height - (height * factor)) / (2 * factor)
  move_viewport(offset_x, offset_y)
  [factor, offset_x, offset_y]
end

def move_viewport(offset_x, offset_y)
  EngineTest.owning(Viewport.new(0, 0, Graphics.width, Graphics.height)) do |viewport|
    viewport.rect.x += offset_x
    viewport.rect.y += offset_y
  end
end

EngineTest.test('full screen keeps the game size with no border') do
  EngineTest.assert_equal([1.0, 0.0, 0.0], essentials_full_screen(0), 'factor and offsets')
end

EngineTest.test('full screen with the border has a factor above 0') do
  factor, = essentials_full_screen(1)
  EngineTest.assert(factor > 0, "factor #{factor}")
end

EngineTest.finish
exit
