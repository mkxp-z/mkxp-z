# win32_wrap.rb
# Author: Ancurio (2014)
# https://github.com/Ancurio/mkxp/issues/73
# https://pastebin.com/zXW1hdrx

# Creative Commons CC0: To the extent possible under law, Ancurio has waived
# all copyright and related or neighboring rights to win32_wrap.rb.
# https://creativecommons.org/publicdomain/zero/1.0/

# Edits by Splendide Imaginarius (2023-2024) also CC0.

# This preload script provides a subset of Win32API in a cross-platform way, so
# you can play Win32API-based games on Linux and macOS.

# To tweak behavior, you can set the following Win32API class constants in an
# earlier preload script (these are usually only helpful for debugging):
#
# NATIVE_ON_WINDOWS=false
# TOLERATE_ERRORS=false
# LOG_NATIVE=true

# RGSS Linker (berka_91) compatibility: stub Kernel#load_module so
# games that call it (FMODEX wrapper, network loaders, etc.) don't
# crash with NoMethodError. Real Windows games rely on the linker
# DLL side-effect of defining module constants. On iOS we can't load
# DLLs, so the constants reference later in the script will raise
# NameError - which binding-mri.cpp's SKIPPED handler swallows for
# LoadError/NoMethodError cases.
unless Kernel.respond_to?(:load_module)
  module Kernel
    def load_module(*_args)
      # No-op on iOS - Win32 DLL loading is not supported.
      nil
    end
    module_function :load_module
  end
end

module Scancodes
  # rubocop:disable Style/MutableConstant -- SDL/WIN32/WIN2SDL get
  # `.default = ...` set after definition (lines 104, 179) so they
  # can't be frozen.
  SDL = { :UNKNOWN => 0x00,
          :A => 0x04, :B => 0x05, :C => 0x06, :D => 0x07,
          :E => 0x08, :F => 0x09, :G => 0x0A, :H => 0x0B,
          :I => 0x0C, :J => 0x0D, :K => 0x0E, :L => 0x0F,
          :M => 0x10, :N => 0x11, :O => 0x12, :P => 0x13,
          :Q => 0x14, :R => 0x15, :S => 0x16, :T => 0x17,
          :U => 0x18, :V => 0x19, :W => 0x1A, :X => 0x1B,
          :Y => 0x1C, :Z => 0x1D, :N1 => 0x1E, :N2 => 0x1F,
          :N3 => 0x20, :N4 => 0x21, :N5 => 0x22, :N6 => 0x23,
          :N7 => 0x24, :N8 => 0x25, :N9 => 0x26, :N0 => 0x27,
          :RETURN => 0x28, :ESCAPE => 0x29, :BACKSPACE => 0x2A, :TAB => 0x2B,
          :SPACE => 0x2C, :MINUS => 0x2D, :EQUALS => 0x2E, :LEFTBRACKET => 0x2F,
          :RIGHTBRACKET => 0x30, :BACKSLASH => 0x31, :NONUSHASH => 0x32, :SEMICOLON => 0x33,
          :APOSTROPHE => 0x34, :GRAVE => 0x35, :COMMA => 0x36, :PERIOD => 0x37,
          :SLASH => 0x38, :CAPSLOCK => 0x39, :F1 => 0x3A, :F2 => 0x3B,
          :F3 => 0x3C, :F4 => 0x3D, :F5 => 0x3E, :F6 => 0x3F,
          :F7 => 0x40, :F8 => 0x41, :F9 => 0x42, :F10 => 0x43,
          :F11 => 0x44, :F12 => 0x45, :PRINTSCREEN => 0x46, :SCROLLLOCK => 0x47,
          :PAUSE => 0x48, :INSERT => 0x49, :HOME => 0x4A, :PAGEUP => 0x4B,
          :DELETE => 0x4C, :END => 0x4D, :PAGEDOWN => 0x4E, :RIGHT => 0x4F,
          :LEFT => 0x50, :DOWN => 0x51, :UP => 0x52, :NUMLOCKCLEAR => 0x53,
          :KP_DIVIDE => 0x54, :KP_MULTIPLY => 0x55, :KP_MINUS => 0x56, :KP_PLUS => 0x57,
          :KP_ENTER => 0x58, :KP_1 => 0x59, :KP_2 => 0x5A, :KP_3 => 0x5B,
          :KP_4 => 0x5C, :KP_5 => 0x5D, :KP_6 => 0x5E, :KP_7 => 0x5F,
          :KP_8 => 0x60, :KP_9 => 0x61, :KP_0 => 0x62, :KP_PERIOD => 0x63,
          :NONUSBACKSLASH => 0x64, :APPLICATION => 0x65, :POWER => 0x66, :KP_EQUALS => 0x67,
          :F13 => 0x68, :F14 => 0x69, :F15 => 0x6A, :F16 => 0x6B,
          :F17 => 0x6C, :F18 => 0x6D, :F19 => 0x6E, :F20 => 0x6F,
          :F21 => 0x70, :F22 => 0x71, :F23 => 0x72, :F24 => 0x73,
          :EXECUTE => 0x74, :HELP => 0x75, :MENU => 0x76, :SELECT => 0x77,
          :STOP => 0x78, :AGAIN => 0x79, :UNDO => 0x7A, :CUT => 0x7B,
          :COPY => 0x7C, :PASTE => 0x7D, :FIND => 0x7E, :MUTE => 0x7F,
          :VOLUMEUP => 0x80, :VOLUMEDOWN => 0x81, :LOCKINGCAPSLOCK => 0x82, :LOCKINGNUMLOCK => 0x83,
          :LOCKINGSCROLLLOCK => 0x84, :KP_COMMA => 0x85, :KP_EQUALSAS400 => 0x86, :INTERNATIONAL1 => 0x87,
          :INTERNATIONAL2 => 0x88, :INTERNATIONAL3 => 0x89, :INTERNATIONAL4 => 0x8A, :INTERNATIONAL5 => 0x8B,
          :INTERNATIONAL6 => 0x8C, :INTERNATIONAL7 => 0x8D, :INTERNATIONAL8 => 0x8E, :INTERNATIONAL9 => 0x8F,
          :LANG1 => 0x90, :LANG2 => 0x91, :LANG3 => 0x92, :LANG4 => 0x93,
          :LANG5 => 0x94, :LANG6 => 0x95, :LANG7 => 0x96, :LANG8 => 0x97,
          :LANG9 => 0x98, :ALTERASE => 0x99, :SYSREQ => 0x9A, :CANCEL => 0x9B,
          :CLEAR => 0x9C, :PRIOR => 0x9D, :RETURN2 => 0x9E, :SEPARATOR => 0x9F,
          :OUT => 0xA0, :OPER => 0xA1, :CLEARAGAIN => 0xA2, :CRSEL => 0xA3,
          :EXSEL => 0xA4, :KP_00 => 0xB0, :KP_000 => 0xB1, :THOUSANDSSEPARATOR => 0xB2,
          :DECIMALSEPARATOR => 0xB3, :CURRENCYUNIT => 0xB4, :CURRENCYSUBUNIT => 0xB5, :KP_LEFTPAREN => 0xB6,
          :KP_RIGHTPAREN => 0xB7, :KP_LEFTBRACE => 0xB8, :KP_RIGHTBRACE => 0xB9, :KP_TAB => 0xBA,
          :KP_BACKSPACE => 0xBB, :KP_A => 0xBC, :KP_B => 0xBD, :KP_C => 0xBE,
          :KP_D => 0xBF, :KP_E => 0xC0, :KP_F => 0xC1, :KP_XOR => 0xC2,
          :KP_POWER => 0xC3, :KP_PERCENT => 0xC4, :KP_LESS => 0xC5, :KP_GREATER => 0xC6,
          :KP_AMPERSAND => 0xC7, :KP_DBLAMPERSAND => 0xC8,
          :KP_VERTICALBAR => 0xC9, :KP_DBLVERTICALBAR => 0xCA,
          :KP_COLON => 0xCB, :KP_HASH => 0xCC, :KP_SPACE => 0xCD, :KP_AT => 0xCE,
          :KP_EXCLAM => 0xCF, :KP_MEMSTORE => 0xD0, :KP_MEMRECALL => 0xD1, :KP_MEMCLEAR => 0xD2,
          :KP_MEMADD => 0xD3, :KP_MEMSUBTRACT => 0xD4, :KP_MEMMULTIPLY => 0xD5, :KP_MEMDIVIDE => 0xD6,
          :KP_PLUSMINUS => 0xD7, :KP_CLEAR => 0xD8, :KP_CLEARENTRY => 0xD9, :KP_BINARY => 0xDA,
          :KP_OCTAL => 0xDB, :KP_DECIMAL => 0xDC, :KP_HEXADECIMAL => 0xDD, :LCTRL => 0xE0,
          :LSHIFT => 0xE1, :LALT => 0xE2, :LGUI => 0xE3, :RCTRL => 0xE4,
          :RSHIFT => 0xE5, :RALT => 0xE6, :RGUI => 0xE7, :MODE => 0x101,
          :AUDIONEXT => 0x102, :AUDIOPREV => 0x103, :AUDIOSTOP => 0x104, :AUDIOPLAY => 0x105,
          :AUDIOMUTE => 0x106, :MEDIASELECT => 0x107, :WWW => 0x108, :MAIL => 0x109,
          :CALCULATOR => 0x10A, :COMPUTER => 0x10B, :AC_SEARCH => 0x10C, :AC_HOME => 0x10D,
          :AC_BACK => 0x10E, :AC_FORWARD => 0x10F, :AC_STOP => 0x110, :AC_REFRESH => 0x111,
          :AC_BOOKMARKS => 0x112, :BRIGHTNESSDOWN => 0x113, :BRIGHTNESSUP => 0x114, :DISPLAYSWITCH => 0x115,
          :KBDILLUMTOGGLE => 0x116, :KBDILLUMDOWN => 0x117, :KBDILLUMUP => 0x118, :EJECT => 0x119,
          :SLEEP => 0x11A, :APP1 => 0x11B, :APP2 => 0x11C }

  SDL.default = SDL[:UNKNOWN]

  WIN32 = {
    :LBUTTON => 0x01, :RBUTTON => 0x02, :MBUTTON => 0x04,

    :BACK => 0x08, :TAB => 0x09, :RETURN => 0x0D, :SHIFT => 0x10,
    :CONTROL => 0x11, :MENU => 0x12, :PAUSE => 0x13, :CAPITAL => 0x14,
    :ESCAPE => 0x1B, :SPACE => 0x20, :PRIOR => 0x21, :NEXT => 0x22,
    :END => 0x23, :HOME => 0x24, :LEFT => 0x25, :UP => 0x26,
    :RIGHT => 0x27, :DOWN => 0x28, :PRINT => 0x2A, :INSERT => 0x2D,
    :DELETE => 0x2E,

    :N0 => 0x30, :N1 => 0x31, :N2 => 0x32, :N3 => 0x33,
    :N4 => 0x34, :N5 => 0x35, :N6 => 0x36, :N7 => 0x37, :N8 => 0x38,
    :N9 => 0x39,

    :A => 0x41, :B => 0x42, :C => 0x43, :D => 0x44, :E => 0x45, :F => 0x46,
    :G => 0x47, :H => 0x48, :I => 0x49, :J => 0x4A, :K => 0x4B, :L => 0x4C,
    :M => 0x4D, :N => 0x4E, :O => 0x4F, :P => 0x50, :Q => 0x51, :R => 0x52,
    :S => 0x53, :T => 0x54, :U => 0x55, :V => 0x56, :W => 0x57, :X => 0x58,
    :Y => 0x59, :Z => 0x5A,

    :LWIN => 0x5B, :RWIN => 0x5C,

    :NUMPAD0 => 0x60, :NUMPAD1 => 0x61, :NUMPAD2 => 0x62, :NUMPAD3 => 0x63,
    :NUMPAD4 => 0x64, :NUMPAD5 => 0x65, :NUMPAD6 => 0x66, :NUMPAD7 => 0x67,
    :NUMPAD8 => 0x68, :NUMPAD9 => 0x69,
    :MULTIPLY => 0x6A, :ADD => 0x6B, :SEPARATOR => 0x6C, :SUBSTRACT => 0x6D,
    :DECIMAL => 0x6E, :DIVIDE => 0x6F,

    :F1 => 0x70, :F2 => 0x71, :F3 => 0x72, :F4 => 0x73,
    :F5 => 0x74, :F6 => 0x75, :F7 => 0x76, :F8 => 0x77,
    :F9 => 0x78, :F10 => 0x79, :F11 => 0x7A, :F12 => 0x7B,
    :F13 => 0x7C, :F14 => 0x7D, :F15 => 0x7E, :F16 => 0x7F,
    :F17 => 0x80, :F18 => 0x81, :F19 => 0x82, :F20 => 0x83,
    :F21 => 0x84, :F22 => 0x85, :F23 => 0x86, :F24 => 0x87,

    :NUMLOCK => 0x90, :SCROLL => 0x91,
    :LSHIFT => 0xA0, :RSHIFT => 0xA1, :LCONTROL => 0xA2, :RCONTROL => 0xA3,
    :LMENU => 0xA4, :RMENU => 0xA5,

    :OEM_1 => 0xBA,
    :OEM_PLUS => 0xBB, :OEM_COMMA => 0xBC, :OEM_MINUS => 0xBD, :OEM_PERIOD => 0xBE,
    :OEM_2 => 0xBF, :OEM_3 => 0xC0, :OEM_4 => 0xDB, :OEM_5 => 0xDC,
    :OEM_6 => 0xDD, :OEM_7 => 0xDE
  }

  WIN32INV = WIN32.invert

  WIN2SDL = {
    :BACK => :BACKSPACE,
    :CAPITAL => :CAPSLOCK,
    :PRIOR => :PAGEUP, :NEXT => :PAGEDOWN,
    :PRINT => :PRINTSCREEN,

    :LWIN => :LGUI, :RWIN => :RGUI,

    :NUMPAD0 => :KP_0, :NUMPAD1 => :KP_1, :NUMPAD2 => :KP_2, :NUMPAD3 => :KP_3,
    :NUMPAD4 => :KP_4, :NUMPAD5 => :KP_5, :NUMPAD6 => :KP_6, :NUMPAD7 => :KP_7,
    :NUMPAD8 => :KP_8, :NUMPAD9 => :KP_9,
    :MULTIPLY => :KP_MULTIPLY, :ADD => :KP_PLUS, :SUBSTRACT => :KP_MINUS,
    :DECIMAL => :KP_DECIMAL, :DIVIDE => :KP_DIVIDE,

    :NUMLOCK => :NUMLOCKCLEAR, :SCROLL => :SCROLLLOCK,
    :LCONTROL => :LCTRL, :RCONTROL => :RCTRL,
    :LMENU => :LALT, :RMENU => :RALT,

    # These are OEM and can vary by country
    # Values taken from Joiplay's src/input.cpp
    :OEM_1 => :SEMICOLON,
    :OEM_PLUS => :EQUALS, :OEM_COMMA => :COMMA, :OEM_MINUS => :MINUS, :OEM_PERIOD => :PERIOD,
    :OEM_2 => :SLASH, :OEM_3 => :GRAVE, :OEM_4 => :LEFTBRACKET, :OEM_5 => :BACKSLASH,
    :OEM_6 => :RIGHTBRACKET, :OEM_7 => :APOSTROPHE
  }

  WIN2SDL.default = :UNKNOWN
  # rubocop:enable Style/MutableConstant
end

$win32KeyStates = nil

module Graphics
  class << self
    alias win32wrap_update update unless method_defined?(:win32wrap_update)
    def update
      win32wrap_update
      $win32KeyStates = nil
    end
  end
end

# rubocop:disable Naming/AccessorMethodName -- mirrors the
# Win32::raw_keystates accessor name games and shims call.
def get_raw_keystates
  $win32KeyStates = Input.raw_key_states if $win32KeyStates.nil?

  $win32KeyStates
end
# rubocop:enable Naming/AccessorMethodName

def common_keystate(vkey)
  vkey_name = Scancodes::WIN32INV[vkey]

  states = get_raw_keystates
  pressed = false

  case vkey_name
  when :LBUTTON
    pressed = Input.press?(Input::MOUSELEFT)
  when :RBUTTON
    pressed = Input.press?(Input::MOUSERIGHT)
  when :MBUTTON
    pressed = Input.press?(Input::MOUSEMIDDLE)
  when :SHIFT
    pressed = double_state(states, :LSHIFT, :RSHIFT)
  when :MENU
    pressed = double_state(states, :LALT, :RALT)
  when :CONTROL
    pressed = double_state(states, :LCTRL, :RCTRL)
  else
    nil
    scan = if Scancodes::SDL.key?(vkey_name)
             vkey_name
           else
             Scancodes::WIN2SDL[vkey_name]
           end

    pressed = state_pressed(states, scan)
  end

  pressed ? 1 : 0
end

def memcpy_string(dst, src)
  i = 0
  src.each_byte do |b|
    dst[i] = b
    i += 1
  end
end

def state_pressed(states, sdl_scan)
  states[Scancodes::SDL[sdl_scan]]
end

def double_state(states, left, right)
  state_pressed(states, left) || state_pressed(states, right)
end

module Win32API_Impl
  module User32
    class Keybd_event
      Seq = [
        [0xA4, 0, 0, 0],
        [0xD, 0, 0, 0],
        [0xD, 0, 2, 0],
        [0xA4, 0, 2, 0]
      ].freeze
      Seq2 = [
        [0x12, 0, 0, 0],
        [0xD, 0, 0, 0],
        [0xD, 0, 2, 0],
        [0x12, 0, 2, 0]
      ].freeze
      def initialize
        @index = 0
      end

      def call(args)
        seq = [args[0], args[1], args[2], args[3]]

        if (seq == Seq[@index]) || (seq == Seq2[@index])
          @index += 1
        else
          @index = 0
        end

        return unless @index == 4

        @index = 0
        Graphics.fullscreen = !Graphics.fullscreen
      end
    end

    class GetKeyState
      def call(vkey)
        # Use C-level asyncKeyState which reads directly from
        # EventThread::keyStates[], bypassing Input::update().
        # This is critical because games like Pokemon Essentials
        # override Input.update at the Ruby level.
        Input.asyncKeyState(vkey[0]) > 0 ? 1 : 0
      end
    end

    class GetAsyncKeyState
      PRESSED_BIT = (1 << 15)
      def call(vkey)
        Input.asyncKeyState(vkey[0])
      end
    end

    class GetKeyboardState
      PRESSED_BIT = 0x80
      def call(args)
        out_states = args[0]

        Scancodes::WIN32.each do |_name, val|
          pressed = Input.asyncKeyState(val) > 0

          out_states[val] = pressed ? PRESSED_BIT : 0
        end
        1
      end
    end

    # Translates a Windows virtual-key code into a hardware scancode
    # (or vice versa). Pokemon Essentials-based games call this with
    # uMapType = 0 (MAPVK_VK_TO_VSC) during text entry to build the
    # arguments for ToUnicode below.
    #
    # We lean on the existing WIN2SDL table: for every VK in WIN32
    # there's either a direct same-name entry in SDL's scancode
    # table, or an aliased entry via WIN2SDL. The returned value is
    # the SDL scancode, not the real Windows scancode - in practice
    # games just pass it straight into ToUnicode, which in turn
    # ignores the scancode field for character resolution, so the
    # exact encoding doesn't matter.
    class MapVirtualKey
      MAPVK_VK_TO_VSC   = 0
      MAPVK_VSC_TO_VK   = 1
      MAPVK_VK_TO_CHAR  = 2

      def call(args)
        code = args[0].to_i
        map_type = args[1].to_i

        case map_type
        when MAPVK_VK_TO_VSC
          vkey_name = Scancodes::WIN32INV[code]
          return 0 if vkey_name.nil?

          sdl_name = Scancodes::SDL.key?(vkey_name) ? vkey_name : Scancodes::WIN2SDL[vkey_name]
          Scancodes::SDL[sdl_name] || 0
        when MAPVK_VK_TO_CHAR
          # Uppercase letter conversion. The high bit of the
          # return value signals "dead key" on Windows. We
          # never set it.
          return code if code.between?(0x41, 0x5A)
          return code if code.between?(0x30, 0x39)

          0
        else
          # VSC_TO_VK and related variants aren't used by any
          # game we ship support for. Returning 0 is the
          # documented "no mapping" result.
          0
        end
      end
    end

    # Converts a virtual-key code + keyboard-state buffer into the
    # Unicode character(s) it produces on a US QWERTY layout. Only
    # supports the printable-ASCII range because that's all any
    # RPG Maker name-entry screen needs (a-z, A-Z, 0-9, space, and
    # a handful of punctuation). Non-printable keys return 0
    # (ToUnicode's documented "no translation" result).
    #
    # Return semantics:
    #   >0 : number of wide-chars written to the output buffer
    #    0 : no translation (e.g. Escape, F1, arrow keys)
    #   <0 : dead key (not produced here)
    class ToUnicode
      PRESSED_BIT = 0x80

      # Keyed by VK (Scancodes::WIN32[:SYMBOL]). Values are
      # [unshifted, shifted] characters. Layout is hardcoded to
      # US QWERTY, which is what every mkxp-z-supported game
      # assumes regardless of the actual host keyboard layout.
      #
      # Control keys (VK_BACK, VK_TAB, VK_RETURN, VK_ESCAPE)
      # intentionally have no entry: Pokemon Uranium's
      # name-entry appends whatever ToUnicode returns directly
      # to the name string without filtering control bytes,
      # so emitting \b here produces rectangle tofu in the
      # text field. Real Windows returns 0 for these anyway.
      US_LAYOUT = {
        0x20 => [' ', ' '], # SPACE
        0x30 => ['0', ')'],   # 0
        0x31 => ['1', '!'],   # 1
        0x32 => ['2', '@'],   # 2
        0x33 => ['3', '#'],   # 3
        0x34 => ['4', '$'],   # 4
        0x35 => ['5', '%'],   # 5
        0x36 => ['6', '^'],   # 6
        0x37 => ['7', '&'],   # 7
        0x38 => ['8', '*'],   # 8
        0x39 => ['9', '('],   # 9
        0xBA => [';', ':'],   # OEM_1 semicolon
        0xBB => ['=', '+'],   # OEM_PLUS
        0xBC => [',', '<'],   # OEM_COMMA
        0xBD => ['-', '_'],   # OEM_MINUS
        0xBE => ['.', '>'],   # OEM_PERIOD
        0xBF => ['/', '?'],   # OEM_2 slash
        0xC0 => ['`', '~'],   # OEM_3 grave
        0xDB => ['[', '{'],   # OEM_4 left bracket
        0xDC => ['\\', '|'],  # OEM_5 backslash
        0xDD => [']', '}'],   # OEM_6 right bracket
        0xDE => ["'", '"'] # OEM_7 apostrophe
      }.freeze

      # rubocop:disable Metrics/AbcSize, Metrics/PerceivedComplexity
      # `call` mirrors Win32 ToUnicode's signature. The per-vkey
      # branching is inherent to keyboard scan-code translation.
      def call(args)
        vkey = args[0].to_i
        _scancode = args[1].to_i
        state = args[2]
        out_buf = args[3]
        buf_size = args[4].to_i
        _flags = args[5].to_i

        return 0 if state.nil? || out_buf.nil? || buf_size < 1

        # Check shift / caps-lock flags inside the state
        # buffer. ToUnicode treats both 0xA0 / 0xA1 as "shift"
        # and 0x14 as caps-lock. The high bit of each byte is
        # the pressed flag.
        shift_pressed = (state[0x10].to_i & PRESSED_BIT) != 0 ||
                        (state[0xA0].to_i & PRESSED_BIT) != 0 ||
                        (state[0xA1].to_i & PRESSED_BIT) != 0
        caps_on = (state[0x14].to_i & 0x01) != 0

        ch = nil

        if vkey.between?(0x41, 0x5A)
          # Letter keys. Caps lock XOR shift decides case.
          upper = caps_on ^ shift_pressed
          ch = (vkey + (upper ? 0 : 32)).chr
        elsif US_LAYOUT.key?(vkey)
          ch = US_LAYOUT[vkey][shift_pressed ? 1 : 0]
        end

        return 0 if ch.nil?

        # The output buffer is a Ruby string holding a packed
        # LPWSTR - each wide char is 2 bytes, little-endian.
        # Write the UTF-16 code unit for `ch` (only BMP chars
        # are emitted here, so a single code unit suffices).
        #
        # `unpack1` is Ruby 2.4+. Use `unpack` + `.first`
        # for 1.8/1.9 compatibility. `String#unpack("U")`
        # works on every Ruby version we target.
        code = ch.unpack('U').first
        lo = code & 0xFF
        hi = (code >> 8) & 0xFF
        out_buf[0] = lo
        out_buf[1] = hi
        # Null-terminate when room allows so callers that
        # treat the buffer as a C wide-string don't read stale
        # bytes after the written char.
        if buf_size >= 2
          out_buf[2] = 0
          out_buf[3] = 0
        end
        1
      end
      # rubocop:enable Metrics/AbcSize, Metrics/PerceivedComplexity
    end

    class ShowCursor
      def initialize
        @cursor_count = 0
      end

      def call(args)
        if args[0] == 1
          @cursor_count += 1
        else
          @cursor_count -= 1
        end

        Graphics.show_cursor = @cursor_count >= 0
      end
    end

    class GetCursorPos
      def call(args)
        out = [Input.mouse_x, Input.mouse_y].pack('ll')
        memcpy_string(args[0], out)
        1
      end
    end

    class GetClientRect
      def call(args)
        return 0 unless args[0] == 42 && args[1].is_a?(String) && args[1].bytesize >= 16

        rect = [0, 0, 640, 480]
        begin
          rect[2] = Graphics.width
          rect[3] = Graphics.height
        rescue StandardError
          # Graphics not yet initialised. Fall back to 640x480.
        end
        memcpy_string(args[1], rect.pack('l4'))
        1
      end
    end

    # The game window has no borders and sits at the top left.
    class GetWindowRect < GetClientRect
    end

    # Full-screen scripts size the canvas from the screen once, and the
    # engine already scales the canvas to the window. A screen the size
    # of the game keeps the canvas the size of the game. Pokemon
    # Essentials v17 divides the screen by the game size plus its screen
    # border, and rounds down. A screen without the border gives a factor
    # below 1: 0.5 in v17, 0 in Pokemon Sage.
    class GetSystemMetrics
      SM_CXSCREEN = 0
      SM_CYSCREEN = 1

      def call(args)
        case args[0]
        when SM_CXSCREEN then Graphics.width + (2 * essentials_border[0])
        when SM_CYSCREEN then Graphics.height + (2 * essentials_border[1])
        else 0
        end
      end

      private

      def essentials_border
        return [0, 0] unless defined?(::BORDERWIDTH) && defined?(::BORDERHEIGHT)
        return [0, 0] unless $PokemonSystem.respond_to?(:border) && $PokemonSystem.border == 1
        return [0, 0] if defined?(::FULLSCREENBORDERCROP) && ::FULLSCREENBORDERCROP

        [::BORDERWIDTH, ::BORDERHEIGHT]
      end
    end

    class SystemParametersInfo
      SPI_GETWORKAREA = 0x30

      def call(args)
        return 0 unless args[0] == SPI_GETWORKAREA && args[2].is_a?(String) && args[2].bytesize >= 16

        memcpy_string(args[2], [0, 0, Graphics.width, Graphics.height].pack('l4'))
        1
      end
    end
    SystemParametersInfoA = SystemParametersInfo
    SystemParametersInfoW = SystemParametersInfo

    class ScreenToClient
      def call(_args)
        1
      end
    end

    class FindWindowA
      def call(args)
        if args[0] == 'RGSS Player' || args[1] == 'RGSS Player'
          42
        else
          0
        end
      end
    end

    # Some scripts (Vinemon's `Scene_Movie3` for instance)
    # request the unsuffixed `FindWindow` instead of the
    # A/W-tagged form. Win32 itself routes the unsuffixed name to
    # the ANSI variant via macro on Windows. Our constant lookup
    # is exact-match so we have to alias here, otherwise the
    # script falls through to TOLERATE_ERRORS and gets back 0,
    # breaking any later `hwnd == GetForegroundWindow.call` style
    # comparison.
    FindWindow = FindWindowA
    FindWindowW = FindWindowA

    class FindWindowEx
      def call(args)
        # FindWindowEx(parent, childAfter, className, windowName)
        # args[2] is className, args[3] is windowName
        if args[2] == 'RGSS Player' || args[3] == 'RGSS Player'
          42
        else
          0
        end
      end
    end

    # Alias for FindWindowExA (same as FindWindowEx)
    FindWindowExA = FindWindowEx
    FindWindowExW = FindWindowEx

    class GetForegroundWindow
      def call(_args)
        42
      end
    end

    class RegisterHotKey
      def call(_args)
        1
      end
    end

    class GetWindowThreadProcessId
      def call(args)
        # Write a fake process ID to output buffer
        memcpy_string(args[1], [1].pack('l')) if args[1].is_a?(String) && args[1].length >= 4
        1 # Return thread ID
      end
    end
  end

  module Kernel32
    class GetCurrentThreadId
      def call(_args)
        1
      end
    end

    # The command line of a player who opened Game.exe, with no
    # test-play flags.
    class GetCommandLine
      def call(_args)
        "\"#{File.join(Dir.pwd, 'Game.exe')}\""
      end
    end
    GetCommandLineA = GetCommandLine

    # RtlMoveMemory(destination, source, length).
    #
    # Games use this to dereference a pointer another Win32 call gave
    # them. Pokemon fan games with their own Winsock layer read the
    # `hostent` from gethostbyname this way, so resolve our synthetic
    # pointers here. A String source is copied directly, which is the
    # struct-to-buffer direction the same scripts use.
    #
    # Anything else leaves the buffer as the caller prepared it,
    # which matches what every caller got before this existed.
    class RtlMoveMemory
      def call(args)
        destination = args[0]
        source = args[1]
        length = args[2].to_i
        return 0 unless destination.is_a?(String) && length > 0

        data = if source.is_a?(String)
                 source[0, length]
               else
                 Win32API_Impl::FakeHeap.read(source, length)
               end
        return 0 if data.nil?

        memcpy_string(destination, data[0, destination.length].to_s)
        1
      end
    end

    # Ruby 1.8 has no code page tables, so these read every code page
    # as UTF-8. win32_wrap_encoding.rb replaces them on Ruby 1.9 and
    # later.
    def self.to_utf16(text, _codepage)
      units = []
      utf8_codes(text).each do |code|
        if code > 0xFFFF
          code -= 0x10000
          units << (0xD800 + (code >> 10)) << (0xDC00 + (code & 0x3FF))
        else
          units << code
        end
      end
      units.pack('v*')
    end

    # A byte that does not start a valid sequence becomes U+FFFD, and the
    # next byte starts a new character, as Win32 does.
    def self.utf8_codes(text)
      bytes = text.unpack('C*')
      codes = []
      i = 0
      while i < bytes.size
        lead = bytes[i]
        size = if lead < 0x80 then 1
               elsif lead >= 0xF0 then 4
               elsif lead >= 0xE0 then 3
               elsif lead >= 0xC2 then 2
               end
        code = size && utf8_code(bytes[i, size])
        if code
          codes << code
          i += size
        else
          codes << 0xFFFD
          i += 1
        end
      end
      codes
    end

    def self.utf8_code(char)
      code = char.pack('C*').unpack('U').first
      code unless code > 0x10FFFF || (code >= 0xD800 && code < 0xE000)
    rescue ArgumentError
      nil
    end

    def self.from_utf16(wide, _codepage)
      units = wide.unpack('v*')
      codes = []
      i = 0
      while i < units.size
        high = units[i]
        low = units[i + 1]
        if high >= 0xD800 && high < 0xDC00 && low && low >= 0xDC00 && low < 0xE000
          codes << (0x10000 + ((high - 0xD800) << 10) + (low - 0xDC00))
          i += 2
        else
          codes << (high >= 0xD800 && high < 0xE000 ? 0xFFFD : high)
          i += 1
        end
      end
      codes.pack('U*')
    end

    # A count of -1 reads up to the first null unit and keeps it, as
    # Win32 does.
    def self.unit_slice(buf, count, unit)
      return '' unless buf.is_a?(String)

      if count == -1
        bytes = buf.unpack('C*')
        size = 0
        while size + unit <= bytes.size
          size += unit
          break if bytes[size - unit, unit] == [0] * unit
        end
      else
        size = [count * unit, buf.bytesize].min
        size = 0 if size < 0
      end
      buf.unpack("a#{size}").first
    end

    # With no buffer or a size of 0, the caller asks how many bytes the
    # result needs.
    def self.write_buffer(dst, data, size)
      return data.bytesize unless dst.is_a?(String) && size > 0

      count = [data.bytesize, dst.bytesize, size].min
      if defined?(Encoding)
        # On Ruby 1.9 and later, dst[i] = b replaces the i-th character,
        # not the i-th byte.
        count.times { |i| dst.setbyte(i, data.getbyte(i)) }
      else
        memcpy_string(dst, data[0, count])
      end
      count
    end

    class MultiByteToWideChar
      def call(args)
        codepage, _flags, source, count, dst, dst_units = args
        text = Win32API_Impl::Kernel32.unit_slice(source, count.to_i, 1)
        wide = Win32API_Impl::Kernel32.to_utf16(text, codepage.to_i)
        Win32API_Impl::Kernel32.write_buffer(dst, wide, dst_units.to_i * 2) / 2
      end
    end
    MultiByteToWideCharA = MultiByteToWideChar

    class WideCharToMultiByte
      def call(args)
        codepage, _flags, source, count, dst, dst_size = args
        wide = Win32API_Impl::Kernel32.unit_slice(source, count.to_i, 2)
        text = Win32API_Impl::Kernel32.from_utf16(wide, codepage.to_i)
        Win32API_Impl::Kernel32.write_buffer(dst, text, dst_size.to_i)
      end
    end
    WideCharToMultiByteA = WideCharToMultiByte
  end

  # There is no Windows registry here, so a key lookup must fail the
  # way a missing key fails on Windows: with a non-zero error code.
  # The tolerate-unknown fallback in Win32API#call returns 0, which
  # reads as ERROR_SUCCESS and sends callers down paths built from
  # empty registry data. Essentials' MiniRegistry then reports an
  # empty system font folder, and the stock FontInstaller copies
  # fonts to "\<file>", fails silently, and asks the player to
  # install the fonts again on every launch.
  module Advapi32
    ERROR_FILE_NOT_FOUND = 2

    class RegOpenKeyExA
      def call(_args)
        ERROR_FILE_NOT_FOUND
      end
    end

    class RegOpenKeyExW < RegOpenKeyExA
    end

    class RegQueryValueExA
      def call(_args)
        ERROR_FILE_NOT_FOUND
      end
    end

    class RegQueryValueExW < RegQueryValueExA
    end

    class RegCloseKey
      def call(_args)
        0
      end
    end
  end

  # Nuri Yuri's RGSS Linker script raises LoadError unless this call
  # returns 1. Fusion Generation II stops at launch on it.
  module RGSSLinker
    class RGSSLinker_Initialize
      def call(_args)
        1
      end
    end
  end

  # Cross-call state for the MCI/AVI playback shim. Vinemon Sauce
  # Edition's title screen opens an AVI via Windows' MCI subsystem
  # (`mciSendString "open ... type AVIVideo alias X"` followed by
  # `play X notify`) and then sits in a Win32 message-pump loop
  # waiting for `MM_MCINOTIFY` (0x3B9) - "MCI device done playing".
  # Without a real MCI implementation the notification never lands
  # and the loop spins forever, allocating message buffers each
  # iteration (visible as 1 fps + steady memory growth in the
  # debug overlay).
  #
  # Can't decode AVI here, but we can short-circuit
  # the script's wait so it falls through to the title screen
  # without playing the intro. The shim counts `mciSendString`
  # calls. Once the script has issued at least an open + play
  # pair, the next `GetMessage` call returns a synthesized
  # MM_MCINOTIFY message and the message-pump loop breaks.
  #
  module MciState
    @@send_string_calls = 0
    @@playback_done_pending = false

    def self.observe_send_string
      @@send_string_calls += 1
      # 2 = open + play. The script enters its message pump
      # right after the play command, so this is when we arm
      # the fake notification.
      @@playback_done_pending = true if @@send_string_calls >= 2
    end

    def self.consume_playback_done
      result = @@playback_done_pending
      @@playback_done_pending = false
      result
    end
  end

  module Winmm
    # MciSendString shim. The script passes commands as
    # UTF-16LE-encoded byte buffers (Vinemon's
    # `Zeus::Encode.utf8_to_utf16`). We decode the relevant ones
    # and write canned responses into the output buffer so the
    # caller's mci_result parser produces values that exit its
    # video-pump loops cleanly. Anything we don't recognize is a
    # silent no-op (the caller treats `0` as success).
    class MciSendStringW
      # Map of command pattern → canned response text. Tested
      # against Vinemon's Scene_Movie3.rb commands. Extend as
      # new MCI-using games hit unhandled cases.
      RESPONSES = [
        # `where X source` returns "x y w h" in pixels. We
        # need non-zero w/h so the caller's
        # `ratio = w / h.to_f` doesn't divide by zero.
        [/^\s*where\s+\S+\s+source\b/i, '0 0 320 240'],
        # Length / position 0 + 0 makes the script's
        # `break if position >= length` fire on the first
        # iteration.
        [/^\s*status\s+\S+\s+length\b/i,          '0'],
        [/^\s*status\s+\S+\s+position\b/i,        '0'],
        # Some scripts also check `mode == 'stopped'`.
        [/^\s*status\s+\S+\s+mode\b/i,            'stopped'],
        [/^\s*status\s+\S+\s+window\s+handle\b/i, '42']
      ].freeze

      def call(args)
        Win32API_Impl::MciState.observe_send_string

        cmd = decode_utf16(args[0])
        # Avoid Ruby 2.3+ syntax / methods: we also build for
        # Ruby 1.9 native (RGSS3 multi-Ruby path).
        #   `&.` (safe navigation)       needs 2.3+
        #   `Regexp#match?` (bool)       needs 2.4+
        # Use the traditional `re =~ cmd` form (returns Integer
        # offset on match, nil on miss) which works on every
        # Ruby version we target.
        match = RESPONSES.find { |re, _| re =~ cmd }
        response = match ? match.last : nil
        write_response(args[1], response) if response

        # MCIERR_NOERROR. Real MCI returns specific error
        # codes for failures. The caller paths we hit only
        # distinguish `0 vs nonzero` and we want success.
        0
      end

      private

      def decode_utf16(buf)
        return '' unless buf.is_a?(String)

        Win32API_Impl::Kernel32.from_utf16(buf, 65_001).split("\0", 2).first || ''
      end

      def write_response(buf, text)
        return unless buf.is_a?(String)

        size = buf.bytesize
        wide = Win32API_Impl::Kernel32.to_utf16(text, 65_001)
        Win32API_Impl::Kernel32.write_buffer(buf, wide + ("\0" * size), size)
      end
    end
    MciSendStringA = MciSendStringW
    MciSendString  = MciSendStringW
  end

  # Extend User32 with the GetMessage shim. Defined inside this
  # `module User32` block so it sits alongside the existing
  # Keybd_event / GetKeyState / etc. stubs above.
  module User32
    # Pokemon-Essentials-derived games typically don't enter a
    # Win32 message loop at all - the engine's own event thread
    # handles input. The exception is scripts that use MCI for
    # media playback and need to pump messages until
    # MM_MCINOTIFY arrives. We fake that one notification when
    # `MciState.consume_playback_done` is set. Otherwise we
    # return 0 (the standard "WM_QUIT received" return that
    # breaks while-GetMessage idioms cleanly).
    class GetMessage
      MM_MCINOTIFY            = 0x3B9
      MCI_NOTIFY_SUCCESSFUL   = 0x1

      def call(args)
        if Win32API_Impl::MciState.consume_playback_done
          # Win32 MSG struct, 32-bit ABI (RGSS-era):
          #   hwnd    (4 bytes, offset 0)
          #   message (4 bytes, offset 4)
          #   wParam  (4 bytes, offset 8)
          #   lParam  (4 bytes, offset 12)
          #   time    (4 bytes, offset 16)
          #   pt.x    (4 bytes, offset 20)
          #   pt.y    (4 bytes, offset 24)
          # wParam = MCI_NOTIFY_SUCCESSFUL signals the
          # script that playback completed without error.
          msg = [0, MM_MCINOTIFY, MCI_NOTIFY_SUCCESSFUL,
                 0, 0, 0, 0].pack('L7')
          memcpy_string(args[0], msg)
          return 1
        end
        0
      end
    end
    GetMessageA = GetMessage
    GetMessageW = GetMessage
  end

  # wininet bridge for Berka's RGSS downloader (embedded by many
  # Pokemon fangames, often as "LUKA DOWNLOADER MODULE"). The script
  # opens a session at load time and aborts the whole script when
  # the handle is 0:
  #
  #   IOA = Win32API.new('wininet','InternetOpenA','plppl','l')
  #           .call('',0,'','',0)
  #   raise ... if IOA == 0
  #
  # At download time it opens a request per URL and pumps it from a
  # 1024-byte read loop, one call per frame:
  #
  #   @fs = IOU.call(IOA, url, nil, 0, 0, 0)
  #   r = IRF.call(@fs, buf, size, o=[n].pack('i!'))
  #   n = o.unpack('i!')[0]   # 0 bytes read = download complete
  #
  # Back the request functions with the engine's HTTPLite client:
  # InternetOpenUrl performs the whole GET and buffers the body;
  # InternetReadFile hands the body out in caller-sized chunks. The
  # fetch is synchronous, so the game blocks for the duration of
  # the transfer (bounded by the native client's 10s connect / 30s
  # read timeouts). With networking unavailable or disabled the
  # fetch fails, InternetOpenUrl returns 0, and the read loop
  # reports 0 bytes - the same shape a dead connection produces on
  # Windows, which these scripts already handle.
  module Wininet
    # Cross-call request state: handle -> buffered response.
    module Requests
      @@handles = {}
      @@next_handle = 0x100

      def self.open(response)
        add(buffer({}, response))
      end

      def self.add(state)
        handle = @@next_handle
        @@next_handle += 1
        @@handles[handle] = state
        handle
      end

      def self.buffer(state, response)
        body = response[:body].to_s.dup
        # Reads slice and count BYTES. On 1.9+ VMs the body may
        # arrive tagged UTF-8, where [] and length work in
        # characters. Retag as binary so both are byte-based.
        body.force_encoding('ASCII-8BIT') if body.respond_to?(:force_encoding)
        state[:body] = body
        state[:pos] = 0
        state[:status] = response[:status].to_i
        state[:response_headers] = response[:headers].is_a?(Hash) ? response[:headers] : {}
        state
      end

      def self.lookup(handle)
        @@handles[handle]
      end

      def self.close(handle)
        @@handles.delete(handle)
        1
      end
    end

    class InternetOpenA
      def call(_args)
        # Fake session handle. Any nonzero value passes the
        # scripts' load-time handle checks. Requests carry their
        # own state keyed by the request handle, so the session
        # handle is never dereferenced.
        1
      end
    end
    InternetOpenW = InternetOpenA
    InternetOpen  = InternetOpenA

    class InternetOpenUrl
      def call(args)
        url = args[1]
        return 0 unless defined?(HTTPLite) && url.is_a?(String) && !url.empty?

        response = begin
          # Third arg: follow redirects. The http:// URLs these
          # 2010s-era scripts embed commonly 301 to https now.
          HTTPLite.get(url, nil, true)
        rescue StandardError
          nil
        end
        return 0 unless response.is_a?(Hash)

        status = response[:status].to_i
        return 0 if status.zero?

        # HTTP error statuses still return a handle, like Windows:
        # the caller reads the error body and can query the status.
        Requests.open(response)
      end
    end
    InternetOpenUrlA = InternetOpenUrl
    InternetOpenUrlW = InternetOpenUrl

    # The request API of Essentials BES and its forks:
    #
    #   hConnect = ICA.call(hInternet, 'host', 80, nil, nil, 3, 0, 0)
    #   hRequest = HORA.call(hConnect, 'GET', 'path', nil, nil, nil, 0, 0)
    #   HARHA.call(hRequest, "User-Agent: RPG Maker XP", -1, flags)
    #   HSRA.call(hRequest, nil, 0, body, body.bytesize)
    #
    # then HttpQueryInfo and InternetReadFile on hRequest. The W
    # spellings take UTF-16 strings, so they stay on the fallback.
    INTERNET_DEFAULT_HTTPS_PORT = 443
    INTERNET_FLAG_SECURE = 0x00800000

    class InternetConnectA
      def call(args)
        server = args[1]
        return 0 unless server.is_a?(String) && !server.empty?

        Requests.add(:server => server, :port => args[2].to_i)
      end
    end
    InternetConnect = InternetConnectA

    class HttpOpenRequestA
      def call(args)
        connection = Requests.lookup(args[0])
        return 0 unless connection && connection[:server]

        verb = args[1].is_a?(String) && !args[1].empty? ? args[1].upcase : 'GET'
        return 0 unless %w[GET HEAD POST].include?(verb)

        path = args[2].is_a?(String) ? args[2] : ''
        Requests.add(:method => verb, :headers => {}, :url => url(connection, path, args[6].to_i))
      end

      private

      def url(connection, path, flags)
        port = connection[:port]
        secure = port == INTERNET_DEFAULT_HTTPS_PORT || (flags & INTERNET_FLAG_SECURE) != 0
        host = connection[:server]
        host = "#{host}:#{port}" unless port.zero? || port == (secure ? 443 : 80)
        path = "/#{path}" unless path[0, 1] == '/'
        "#{secure ? 'https' : 'http'}://#{host}#{path}"
      end
    end
    HttpOpenRequest = HttpOpenRequestA

    # Reads "Name: value" lines. A length of -1 means the text ends
    # at its first NUL.
    def self.merge_headers(headers, text, length)
      return unless text.is_a?(String)

      text = length > 0 ? text[0, length] : text.split("\0", 2)[0].to_s
      text.split(/\r?\n/).each do |line|
        name, value = line.split(':', 2)
        headers[name.strip] = value.strip if value
      end
    end

    class HttpAddRequestHeadersA
      def call(args)
        request = Requests.lookup(args[0])
        return 0 unless request && request[:headers]

        Wininet.merge_headers(request[:headers], args[1], args[2].to_i)
        1
      end
    end
    HttpAddRequestHeaders = HttpAddRequestHeadersA

    class HttpSendRequestA
      def call(args)
        request = Requests.lookup(args[0])
        return 0 unless defined?(HTTPLite) && request && request[:url]

        Wininet.merge_headers(request[:headers], args[1], args[2].to_i)
        body = args[3].is_a?(String) ? args[3][0, args[4].to_i] : ''
        response = begin
          fetch(request, body)
        rescue StandardError
          nil
        end
        return 0 unless response.is_a?(Hash) && response[:status].to_i != 0

        Requests.buffer(request, response)
        1
      end

      private

      # Windows follows redirects here too, so a script that still
      # connects to port 80 reaches a host that now 301s every
      # request to https.
      def fetch(request, body)
        url = request[:url]
        headers = request[:headers]
        return HTTPLite.get(url, headers, true) if request[:method] != 'POST'
        return HTTPLite.post(url, {}, headers, true) if body.empty?

        type = headers.keys.find { |name| name.downcase == 'content-type' }
        ctype = type ? headers.delete(type) : 'application/x-www-form-urlencoded'
        HTTPLite.post_body(url, body, ctype, headers)
      end
    end
    HttpSendRequest = HttpSendRequestA

    class InternetReadFile
      def call(args)
        request = Requests.lookup(args[0])
        buf = args[1]
        out = args[3]
        unless request && buf.is_a?(String)
          write_count(out, 0)
          return 0
        end

        want = args[2].to_i
        want = buf.length if want > buf.length
        want = 0 if want < 0
        chunk = request[:body][request[:pos], want] || ''
        request[:pos] += chunk.length
        memcpy_string(buf, chunk)
        write_count(out, chunk.length)
        1
      end

      private

      # The out-parameter is a packed 4-byte native int the caller
      # unpacks with 'i!'. iOS is little-endian, so 'V' produces
      # the same bytes on every VM.
      def write_count(out, count)
        memcpy_string(out, [count].pack('V')) if out.is_a?(String) && out.length >= 4
      end
    end

    class HttpQueryInfo
      HTTP_QUERY_CONTENT_LENGTH = 5
      HTTP_QUERY_STATUS_CODE = 19
      HTTP_QUERY_RAW_HEADERS_CRLF = 22
      HTTP_QUERY_FLAG_NUMBER = 0x20000000

      def call(args)
        request = Requests.lookup(args[0])
        buf = args[2]
        return 0 unless request && request[:body] && buf.is_a?(String)

        info = args[1].to_i
        value = case info & 0xFFFF
                when HTTP_QUERY_CONTENT_LENGTH then request[:body].length
                when HTTP_QUERY_STATUS_CODE then request[:status]
                when HTTP_QUERY_RAW_HEADERS_CRLF then raw_headers(request)
                else return 0
                end
        return 0 if value.is_a?(String) && (info & HTTP_QUERY_FLAG_NUMBER) != 0

        payload = if (info & HTTP_QUERY_FLAG_NUMBER).zero?
                    # Headers are text on Windows unless FLAG_NUMBER
                    # asks for a binary DWORD.
                    "#{value}\0"
                  else
                    [value].pack('V')
                  end
        # Windows fails with ERROR_INSUFFICIENT_BUFFER rather than
        # writing past the caller's buffer. Writing anyway would
        # raise IndexError inside the game script.
        return 0 if payload.length > buf.length

        memcpy_string(buf, payload)
        1
      end

      private

      # HTTPLite does not keep the reason phrase, so the status line
      # has only the code.
      def raw_headers(request)
        lines = ["HTTP/1.1 #{request[:status]}"]
        request[:response_headers].each { |name, value| lines << "#{name}: #{value}" }
        "#{lines.join("\r\n")}\r\n\r\n"
      end
    end
    HttpQueryInfoA = HttpQueryInfo
    HttpQueryInfoW = HttpQueryInfo

    class InternetCloseHandle
      def call(args)
        # Also succeeds for the fake session handle and for
        # already-closed handles. Callers only check for nonzero.
        Requests.close(args[0])
      end
    end

    class DeleteUrlCacheEntry
      def call(_args)
        # No cache exists, so every entry is already deleted.
        1
      end
    end
    DeleteUrlCacheEntryA = DeleteUrlCacheEntry
    DeleteUrlCacheEntryW = DeleteUrlCacheEntry
  end

  # urlmon bridge for one-shot Win32API downloaders:
  #
  #   UDF = Win32API.new('urlmon', 'URLDownloadToFileA', 'lppll', 'l')
  #   UDF.call(0, url, dest, 0, 0)  # 0 == S_OK
  #
  # Backed by the engine's streaming HTTPLite.download. The dest
  # path is game-relative (cwd is the game folder), matching where
  # these scripts expect their file on Windows. With networking
  # unavailable or disabled the call returns E_FAIL-shaped nonzero,
  # which the scripts already treat as a failed download.
  module Urlmon
    class URLDownloadToFile
      S_OK = 0
      E_FAIL = 1

      def call(args)
        url = args[1]
        dest = args[2]
        unless defined?(HTTPLite) &&
               url.is_a?(String) && !url.empty? &&
               dest.is_a?(String) && !dest.empty?
          return E_FAIL
        end

        response = begin
          HTTPLite.download(url, dest)
        rescue StandardError
          nil
        end

        status = response.is_a?(Hash) ? response[:status].to_i : 0
        status == 200 ? S_OK : E_FAIL
      end
    end
    URLDownloadToFileA = URLDownloadToFile
    URLDownloadToFileW = URLDownloadToFile
  end

  # Synthetic pointer heap.
  #
  # A few Win32 calls hand the caller a pointer and expect it to read
  # the memory back later through RtlMoveMemory. `gethostbyname` is
  # the one that matters here: it returns a `hostent *`. Ruby cannot
  # give out real addresses, so hand out synthetic ones and resolve
  # them from this table. Blocks live for the whole session, which is
  # fine because games call these once per connection.
  module FakeHeap
    BASE = 0x7F00_0000
    STRIDE = 0x1000

    @blocks = {}
    @next_address = BASE

    def self.alloc(bytes)
      address = @next_address
      @next_address += STRIDE
      @blocks[address] = bytes.to_s
      address
    end

    # Reads always succeed for a known block. Callers ask for a fixed
    # struct size (256 bytes for a host name, say) that can overrun
    # the real payload, so pad with NULs the way real memory reads
    # would return whatever follows.
    def self.read(address, length)
      block = @blocks[address]
      return nil if block.nil?

      data = block[0, length].to_s
      data + ("\0" * (length - data.length))
    end
  end

  # ws2_32 (Winsock) bridge.
  #
  # Pokemon fan games that predate Essentials' HTTP helpers talk to
  # their servers through a Ruby reimplementation of Winsock. Pokemon
  # Insurgence ships two of them and uses the ten imports below.
  #
  # Without this module every call fell through to the
  # TOLERATE_ERRORS branch and returned 0. In Winsock `connect()`
  # returns 0 for SUCCESS, so the game believed it had connected,
  # wrote into nothing, and then polled a socket that never became
  # readable. The result was an online menu that hung until the
  # player pressed cancel.
  #
  # Ruby's socket extension only initialises when the host grants the
  # game network access (ios/Dependencies/ruby18/extinit.c). When it
  # is absent every call here reports failure, so the game takes the
  # offline path it already has for an unreachable server.
  module Ws2_32
    AF_INET = 2
    SOCK_STREAM = 1
    SOCKET_ERROR = -1
    INVALID_SOCKET = -1

    # Capture the socket entry points while they are still pristine.
    # The games this module exists for reopen `Socket` and add their
    # own Winsock wrappers, so calling a reopened method later would
    # recurse straight back into this module.
    NATIVE = if defined?(::Socket) && defined?(::IO)
               begin
                 {
                   :klass => ::Socket,
                   :init => ::Socket.instance_method(:initialize),
                   :connect => ::Socket.instance_method(:connect_nonblock),
                   :pack => ::Socket.method(:pack_sockaddr_in),
                   :recv => ::Socket.instance_method(:recv),
                   :write => ::IO.instance_method(:write),
                   :close => ::IO.instance_method(:close),
                   :closed => ::IO.instance_method(:closed?),
                   :select => ::IO.method(:select)
                 }
               rescue StandardError
                 nil
               end
             end

    @sockets = {}
    @next_handle = 3
    @last_error = 0

    def self.available?
      !NATIVE.nil?
    end

    def self.last_error
      @last_error
    end

    # Winsock reports failure as -1 and keeps the code for a later
    # WSAGetLastError. These games look the code up in Errno, so
    # store the POSIX number rather than a Winsock one.
    def self.fail_with(errno)
      @last_error = errno
      SOCKET_ERROR
    end

    def self.entry_for(handle)
      @sockets[handle]
    end

    def self.io_for(handle)
      entry = @sockets[handle]
      entry && entry[:io]
    end

    # With no socket extension the host denied this game network
    # access, so report the errno airplane mode gives. Callers read it
    # back through WSAGetLastError and look it up in Errno, and a 0
    # there would leave them with "Success" or no match at all.
    def self.open_handle(domain)
      return fail_with(Errno::ENETDOWN::Errno) unless available?
      return fail_with(Errno::EAFNOSUPPORT::Errno) unless domain == AF_INET

      handle = @next_handle
      @next_handle += 1
      @sockets[handle] = { :io => nil }
      handle
    end

    def self.close_handle(handle)
      entry = @sockets.delete(handle)
      io = entry && entry[:io]
      return 0 if io.nil?

      begin
        NATIVE[:close].bind(io).call unless NATIVE[:closed].bind(io).call
      rescue StandardError
        nil
      end
      0
    end

    # Winsock sockaddr_in: family (2 bytes, host order), port (2
    # bytes, network order), then the four address bytes.
    def self.connect_handle(handle, sockaddr)
      entry = entry_for(handle)
      return fail_with(Errno::EBADF::Errno) if entry.nil?
      return fail_with(Errno::ENETDOWN::Errno) unless available?
      return fail_with(Errno::EINVAL::Errno) unless sockaddr.is_a?(String) && sockaddr.length >= 8

      port = sockaddr[2, 2].unpack('n')[0].to_i
      host = sockaddr[4, 4].unpack('C4').join('.')

      io = nil
      errno = nil
      begin
        io = connect_to(host, port)
      rescue SystemCallError => e
        errno = e.errno
      rescue StandardError
        errno = Errno::ECONNREFUSED::Errno
      end
      return fail_with(errno) if errno
      return fail_with(Errno::ETIMEDOUT::Errno) if io.nil?

      entry[:io] = io
      0
    end

    # Connect without blocking the RGSS thread for a full TCP
    # timeout. Ruby 1.8 schedules green threads, so a blocking
    # connect() stalls every thread, including the one drawing frames.
    #
    # The cap only applies when a server does not answer. A server
    # that answers completes in one round trip and never reaches it.
    # Five seconds still covers two lost SYN retransmits, and it
    # halves how long a dead server freezes the game.
    def self.connect_to(host, port, seconds = 5)
      io = NATIVE[:klass].allocate
      NATIVE[:init].bind(io).call(AF_INET, SOCK_STREAM, 0)
      address = NATIVE[:pack].call(port, host)

      begin
        NATIVE[:connect].bind(io).call(address)
      rescue Errno::EINPROGRESS, Errno::EALREADY
        if NATIVE[:select].call(nil, [io], nil, seconds).nil?
          NATIVE[:close].bind(io).call
          return nil
        end
        begin
          NATIVE[:connect].bind(io).call(address)
        rescue Errno::EISCONN
          # Already connected. Winsock calls that success.
        end
      end
      io
    end

    def self.send_on(handle, data, length)
      io = io_for(handle)
      return fail_with(Errno::ENOTCONN::Errno) if io.nil?

      payload = data.to_s
      payload = payload[0, length] if length > 0 && length < payload.length
      begin
        NATIVE[:write].bind(io).call(payload)
      rescue SystemCallError => e
        fail_with(e.errno)
      end
    end

    def self.recv_into(handle, buffer, length)
      io = io_for(handle)
      return fail_with(Errno::ENOTCONN::Errno) if io.nil?
      return 0 unless buffer.is_a?(String) && length > 0
      return 0 if NATIVE[:select].call([io], nil, nil, 0).nil?

      data = nil
      begin
        data = NATIVE[:recv].bind(io).call(length)
      rescue SystemCallError => e
        return fail_with(e.errno)
      end
      return 0 if data.nil? || data.empty?

      memcpy_string(buffer, data)
      data.length
    end

    # The caller packs an fd_set as a count followed by one handle,
    # and a timeval as seconds followed by microseconds.
    def self.select_on(readfds, timeval)
      return fail_with(Errno::EINVAL::Errno) unless readfds.is_a?(String) && readfds.length >= 8

      count, handle = readfds.unpack('ll')
      return 0 if count.to_i < 1

      io = io_for(handle)
      return 0 if io.nil?

      timeout = 0
      if timeval.is_a?(String) && timeval.length >= 8
        seconds, microseconds = timeval.unpack('ll')
        timeout = seconds.to_i + (microseconds.to_i / 1_000_000.0)
      end

      NATIVE[:select].call([io], nil, nil, timeout).nil? ? 0 : 1
    end

    # Builds a Win32 `hostent` in the synthetic heap and returns a
    # pointer to it. The 32-bit layout the callers unpack is h_name,
    # h_aliases, h_addrtype, h_length, h_addr_list.
    def self.hostent_for(name)
      return 0 unless available?

      host = name.to_s.split("\0")[0].to_s
      return 0 if host.empty?

      address = begin
        resolve(host)
      rescue StandardError
        nil
      end
      return 0 if address.nil?

      name_pointer = FakeHeap.alloc("#{host}\0")
      alias_pointer = FakeHeap.alloc([0].pack('L'))
      address_pointer = FakeHeap.alloc(address)
      list_pointer = FakeHeap.alloc([address_pointer, 0].pack('LL'))

      FakeHeap.alloc([name_pointer, alias_pointer].pack('LL') +
                     [AF_INET, address.length].pack('ss') +
                     [list_pointer].pack('L'))
    end

    def self.resolve(host)
      if host =~ /\A(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})\z/
        return [Regexp.last_match(1).to_i, Regexp.last_match(2).to_i,
                Regexp.last_match(3).to_i, Regexp.last_match(4).to_i].pack('C4')
      end

      # pack_sockaddr_in resolves the name and puts the four address
      # bytes at offset 4 of the BSD sockaddr_in it returns.
      NATIVE[:pack].call(0, host)[4, 4]
    end

    class Socket
      def call(args)
        Ws2_32.open_handle(args[0].to_i)
      end
    end

    class Connect
      def call(args)
        Ws2_32.connect_handle(args[0].to_i, args[1])
      end
    end

    class Send
      def call(args)
        Ws2_32.send_on(args[0].to_i, args[1], args[2].to_i)
      end
    end

    class Recv
      def call(args)
        Ws2_32.recv_into(args[0].to_i, args[1], args[2].to_i)
      end
    end

    class Select
      def call(args)
        Ws2_32.select_on(args[1], args[4])
      end
    end

    class Closesocket
      def call(args)
        Ws2_32.close_handle(args[0].to_i)
      end
    end

    class Gethostbyname
      def call(args)
        Ws2_32.hostent_for(args[0])
      end
    end

    # Clients never bind, and the options these games set (keepalive,
    # nodelay) do not change whether the connection works. Report
    # success so the callers' error checks stay quiet.
    class Bind
      def call(_args)
        0
      end
    end

    class Setsockopt
      def call(_args)
        0
      end
    end

    class WSAGetLastError
      def call(_args)
        Ws2_32.last_error
      end
    end
  end
end

def kappatalize(s)
  # Sanitize to a valid Ruby constant name: strip non-alphanumeric/underscore
  # chars (e.g. "RGSS Linker" -> "RGSSLinker") and ensure first char is uppercase.
  s = s.gsub(/[^A-Za-z0-9_]/, '')
  s[0] = s[0, 1].upcase unless s.empty?
  s
end

# `method_defined?` does not see private methods. Treat both public
# and private methods as "already defined" for our idempotency guards.
def mkxp_method_or_alias_defined?(klass, name)
  klass.method_defined?(name) || klass.private_method_defined?(name)
end

class Win32API
  NATIVE_ON_WINDOWS = true unless const_defined?('NATIVE_ON_WINDOWS')
  TOLERATE_ERRORS = true unless const_defined?('TOLERATE_ERRORS')
  LOG_NATIVE = false unless const_defined?('LOG_NATIVE')

  # mkxp-z-apple-mobile has no native Win32API implementation - the
  # MiniFFI binding is Windows-only and was dropped when the fork
  # narrowed to iOS/iPadOS/tvOS. The alias_method calls below check
  # whether a native :initialize / :call exists before capturing it,
  # so this file works both with and without the native binding.
  if mkxp_method_or_alias_defined?(self,
                                   :initialize) && !mkxp_method_or_alias_defined?(self,
                                                                                  :mkxp_native_initialize)
    alias mkxp_native_initialize initialize
  end
  def initialize(dll, func, *args)
    @dll = dll
    @func = func
    @called = false

    dll = kappatalize(dll.chomp('.dll'))
    func = kappatalize(func)

    if (!System.is_windows? || !NATIVE_ON_WINDOWS) && Win32API_Impl.const_defined?(dll)
      dll_impl = Win32API_Impl.const_get(dll)
      if dll_impl.const_defined?(func)
        @mkxp_wrap_impl = dll_impl.const_get(func).new
        return
      end
    end

    @mkxp_native_available = false
    return unless respond_to?(:mkxp_native_initialize)

    begin
      mkxp_native_initialize(@dll, @func, *args)
      @mkxp_native_available = true
      nil
    rescue StandardError
      # Native initialiser missing on this Win32API build. The
      # Ruby-only fallback path above is enough for compat.
    end
  end

  if mkxp_method_or_alias_defined?(self, :call) && !mkxp_method_or_alias_defined?(self, :mkxp_native_call)
    alias mkxp_native_call call
  end
  def call(*args)
    return @mkxp_wrap_impl.call(args) if @mkxp_wrap_impl

    if @mkxp_native_available
      System.puts("[Win32API] [#{@dll}:#{@func}] #{args}") if LOG_NATIVE
      return mkxp_native_call(*args)
    end

    raise "[Win32API] [#{@dll}:#{@func}] #{args}" unless TOLERATE_ERRORS

    System.puts("[Win32API] [#{@dll}:#{@func}] #{args}") unless @called
    @called = true
    0
  end
end
