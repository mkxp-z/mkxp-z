# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Mateo M. Additional terms apply: see ADDITIONAL-TERMS.md.

# Pokemon Essentials Graphics + movie-plugin compatibility aliases.
#
# JoiPlay bound `Graphics.poke_*` as distinct C++ entry points and
# then defined the regular `Graphics.width` / `resize_screen` etc.
# in Ruby as thin wrappers around them. Our engine already ships
# the regular names natively, so every fangame that calls the
# `poke_*` flavor (Uranium, Reborn, Rejuvenation, Insurgence,
# Infinite Fusion and most PE19-based games) crashes on iOS with
# `NoMethodError: undefined method poke_snap_to_bitmap`.
#
# Port the aliases in reverse: define `poke_*` as delegates to the
# real methods. Same for `mkxp_snap_to_bitmap` which a handful of
# older plugins hit, and for `zeus_play_movie` which wraps
# `Graphics.play_movie` for games that expect the Zeus video
# plugin API. Gate each on `respond_to?` + `method_defined?` so
# re-running this script (reload during development) or games
# that already define these names keep working.
#
# The delegates call the NATIVE methods snapshotted at preload
# (pokemon_compat.rb), not the live names. Games redefine the live
# names as wrappers over these compat names (Daybreak's
# snap_to_bitmap returns Graphics.mkxp_snap_to_bitmap), so a
# late-bound delegate closes a call loop that dies with
# SystemStackError at the first battle transition. The live-name
# fallback only covers a snapshot the preload could not take.

module Graphics
  class << self
    unless respond_to?(:__mkxp_graphics_native, true)
      define_method(:__mkxp_graphics_native) do |ivar, name, *args|
        native = Graphics.instance_variable_get(ivar)
        native ? native.call(*args) : Graphics.send(name, *args)
      end
      private :__mkxp_graphics_native
    end

    unless respond_to?(:poke_width)
      define_method(:poke_width) { __mkxp_graphics_native(:@__mkxp_native_width, :width) }
    end

    unless respond_to?(:poke_height)
      define_method(:poke_height) { __mkxp_graphics_native(:@__mkxp_native_height, :height) }
    end

    unless respond_to?(:poke_snap_to_bitmap)
      define_method(:poke_snap_to_bitmap) do
        __mkxp_graphics_native(:@__mkxp_native_snap_to_bitmap, :snap_to_bitmap)
      end
    end

    unless respond_to?(:mkxp_snap_to_bitmap)
      define_method(:mkxp_snap_to_bitmap) do
        __mkxp_graphics_native(:@__mkxp_native_snap_to_bitmap, :snap_to_bitmap)
      end
    end

    unless respond_to?(:poke_resize_screen)
      define_method(:poke_resize_screen) do |w, h|
        __mkxp_graphics_native(:@__mkxp_native_resize_screen, :resize_screen, w, h)
      end
    end

    # Legacy PE feature probe. Always true on mkxp-z. Some games
    # branch on this to decide whether to call resize_screen.
    define_method(:haveresizescreen) { true } unless respond_to?(:haveresizescreen)

    # Zeus video plugin ships as a separate DLL on Windows and
    # some fangames call it directly. Delegate to our native
    # play_movie, ignoring the extra cancellable/fit args which
    # we don't expose distinctly.
    unless respond_to?(:zeus_play_movie)
      define_method(:zeus_play_movie) do |filename, *_rest|
        __mkxp_graphics_native(:@__mkxp_native_play_movie, :play_movie, filename)
      end
    end

    # Many fangames replace Graphics.snap_to_bitmap with a version
    # that cannot work off Windows, and every replacement fails the
    # same way: it returns nil.
    #
    # Peter O.'s "Sprite Resizer" asks rubyscreen.dll to write a BMP
    # to ENV["TEMP"], then loads that file. The DLL never runs here,
    # so no file appears and the method returns nil. Essentials then
    # ships a fallback (PSystem_System) that hard-codes `return nil`
    # when the resizer is absent.
    #
    # Menus, `pbFadeOutIn` and every transition draw the captured
    # screen as their background. A nil capture leaves them blank,
    # which reads as "the menu never opened" (Pokemon Empyrean).
    #
    # Keep the game's version and fall back to the native capture
    # when it yields nothing. Games that wrap snap_to_bitmap for
    # their own reasons (Daybreak returns Graphics.mkxp_snap_to_bitmap)
    # return a real bitmap, so the wrapper passes their result on.
    game_snap = Graphics.method(:snap_to_bitmap) if Graphics.respond_to?(:snap_to_bitmap)
    native_snap = Graphics.instance_variable_get(:@__mkxp_native_snap_to_bitmap)
    if game_snap && native_snap && game_snap != native_snap
      define_method(:snap_to_bitmap) do
        captured = begin
          game_snap.call
        rescue StandardError
          nil
        end
        captured = native_snap.call if captured.nil?
        captured
      end
      MKXP.puts('[graphics-compat] snap_to_bitmap falls back to the native capture') if defined?(MKXP)
    end
  end
end

# Some fangame plugins (e.g. Pokemon Void Character Select) assign
# a resolved filename string directly to Sprite#bitmap= and call
# .width / .height on the same path string. Coerce at the setter;
# Graphics/ path strings expose bitmap dimensions on demand.
module MkxpGraphicsPathString
  GRAPHICS_RE = %r{\AGraphics/}.freeze

  def width
    raise NoMethodError, "undefined method `width' for #{inspect}" unless self =~ GRAPHICS_RE

    Bitmap.new(self).width
  end

  def height
    raise NoMethodError, "undefined method `height' for #{inspect}" unless self =~ GRAPHICS_RE

    Bitmap.new(self).height
  end
end

# rubocop:disable Lint/SendWithMixinArgument -- include/prepend are private until Ruby 2.1
if String.respond_to?(:prepend, true)
  String.send(:prepend, MkxpGraphicsPathString) unless String.ancestors.include?(MkxpGraphicsPathString)
else
  # Module#prepend is Ruby 2.0+. width/height are new methods on
  # String, so include is equivalent on 1.8/1.9 VX/VX Ace games.
  String.send(:include, MkxpGraphicsPathString) unless String.ancestors.include?(MkxpGraphicsPathString)
end
# rubocop:enable Lint/SendWithMixinArgument

module MkxpBitmapPathCoercion
  def bitmap=(value)
    super(MkxpBitmapPathCoercion.coerce(value))
  end

  module_function

  def coerce(value)
    return nil if value.nil?
    return value unless value.is_a?(String)
    return nil if value.empty?

    Bitmap.new(value)
  end

  def patch(klass)
    return unless defined?(klass) && klass.is_a?(Class)

    if klass.respond_to?(:prepend, true)
      return if klass.ancestors.include?(MkxpBitmapPathCoercion)

      klass.send(:prepend, MkxpBitmapPathCoercion)
    else
      marker = :__mkxp_coerced_bitmap_set
      return if klass.method_defined?(marker)

      klass.class_eval do
        alias_method marker, :bitmap=
        define_method(:bitmap=) do |value|
          send(marker, MkxpBitmapPathCoercion.coerce(value))
        end
      end
    end
  end
end

MkxpBitmapPathCoercion.patch(Sprite) if defined?(Sprite)
MkxpBitmapPathCoercion.patch(Plane) if defined?(Plane)

module MkxpPokemonGraphicsCompat
  def self.patch_ball_animation_mixin
    return unless defined?(PokeBattle_BallAnimationMixin)
    return unless PokeBattle_BallAnimationMixin.method_defined?(:ballTracksHand)

    PokeBattle_BallAnimationMixin.module_eval do
      unless method_defined?(:__mkxp_ball_tracks_hand_with_bitmap)
        alias_method :__mkxp_ball_tracks_hand_with_bitmap, :ballTracksHand
      end

      # rubocop:disable Naming/MethodParameterName, Naming/VariableName -- PE mixin API
      def ballTracksHand(ball, traSprite, safariThrow = false)
        bitmap = traSprite.bitmap if traSprite
        return [-6, 202] if bitmap.nil?

        __mkxp_ball_tracks_hand_with_bitmap(ball, traSprite, safariThrow)
      end
      # rubocop:enable Naming/MethodParameterName, Naming/VariableName
    end
  end
end

MkxpPokemonGraphicsCompat.patch_ball_animation_mixin

class Module
  # `include` is private on this Ruby, so its alias is private too,
  # and `method_defined?` does not see private methods. An RGSS
  # Reset re-runs this script. Without the private check the alias
  # re-targets the already-wrapped `include`, and the wrapper then
  # calls itself without end (SystemStackError on the reset path).
  unless method_defined?(:__mkxp_graphics_compat_include) ||
         private_method_defined?(:__mkxp_graphics_compat_include)
    alias __mkxp_graphics_compat_include include

    def include(*mods)
      ret = __mkxp_graphics_compat_include(*mods)
      if defined?(PokeBattle_BallAnimationMixin) && mods.include?(PokeBattle_BallAnimationMixin)
        MkxpPokemonGraphicsCompat.patch_ball_animation_mixin
      end
      ret
    end
  end
end

if defined?(GameData::TrainerType) && GameData::TrainerType.respond_to?(:check_file)
  module GameData
    class TrainerType
      class << self
        unless method_defined?(:__mkxp_case_sensitive_check_file)
          alias __mkxp_case_sensitive_check_file check_file
        end

        # rubocop:disable Metrics/AbcSize -- candidate path fan-out for case-insensitive trainer sprites
        def check_file(tr_type, path, optional_suffix = '', suffix = '')
          ret = __mkxp_case_sensitive_check_file(tr_type, path, optional_suffix, suffix)
          return ret if ret

          tr_type_data = try_get(tr_type)
          return nil if tr_type_data.nil?

          candidates = []
          if optional_suffix && !optional_suffix.empty?
            candidates << (path + tr_type_data.id.to_s + optional_suffix + suffix)
            if tr_type_data.respond_to?(:id_number)
              candidates << (path + format('%03d', tr_type_data.id_number) + optional_suffix + suffix)
            end
          end
          candidates << (path + tr_type_data.id.to_s + suffix)
          if tr_type_data.respond_to?(:id_number)
            candidates << (path + format('%03d', tr_type_data.id_number) + suffix)
          end

          candidates.each do |candidate|
            resolved = __mkxp_case_insensitive_bitmap(candidate)
            return resolved if resolved
          end
          nil
        end
        # rubocop:enable Metrics/AbcSize

        def __mkxp_case_insensitive_bitmap(candidate)
          dir = File.dirname(candidate)
          base = File.basename(candidate).downcase

          Dir.foreach(dir) do |entry|
            next if ['.', '..'].include?(entry)

            name = File.basename(entry, File.extname(entry)).downcase
            ext = File.extname(entry).downcase
            if name == base && ['.png', '.gif'].include?(ext)
              return File.join(dir, File.basename(entry, File.extname(entry)))
            end
          end
          nil
        rescue SystemCallError
          nil
        end
      end
    end
  end
end

# Zeus81 "Bitmap Export" compatibility.
#
# Many fangames ship Zeus81's Bitmap Export script. It adds
# `Bitmap#export`, `Bitmap#save`, `Bitmap#get_data`, Marshal support
# for Bitmap, and a `Graphics.snap_to_bitmap` replacement. The script
# reads and writes pixels through the Win32 call `RtlMoveMemory`. It
# walks the RGSS object header (`__id__*2+16`) to find the pixel
# buffer of a bitmap.
#
# That layout only exists in the Windows RGSS DLLs. Our engine keeps
# pixels in a GPU texture, so there is no such buffer. `RtlMoveMemory`
# is a no-op stub here (win32_wrap.rb), so the script silently reads
# zero bytes. Every export writes a fully transparent image, and the
# Win32 `snap_to_bitmap` returns an empty bitmap.
#
# Games that build sprites at run time break in a way that is hard to
# read: no error, no crash, only invisible graphics. Pokemon Empyrean
# composes the player character from clothing layers, saves the result
# to `Graphics/Characters`, then loads it back. The player and the
# gender-selection trainers were invisible. Menus that snapshot the
# screen for their background were invisible too.
#
# Our engine already exposes the same operations natively:
#   Bitmap#raw_data / #raw_data=  - RGBA pixels, top row first
#   Bitmap#to_file                - PNG, JPG or BMP by file extension
#   Graphics.snap_to_bitmap       - real screen capture
#
# Rebuild the Zeus methods on top of those. `get_data` and `set_data`
# keep the Windows pixel layout (BGRA, bottom row first) because game
# scripts that touch pixels expect it. `export` and `save` skip the
# pure-Ruby PNG encoder and call the native writer.
#
# The engine restores `Graphics.snap_to_bitmap` only when Zeus took it
# over. `Graphics::GetDIBits` exists only in the Win32 branch of the
# script, so it is a safe signal. Games that legitimately wrap
# `snap_to_bitmap` (Daybreak, Reborn) keep their own version.

if defined?(Bitmap) &&
   Bitmap.method_defined?(:raw_data) &&
   Bitmap.method_defined?(:to_file) &&
   Bitmap.method_defined?(:last_row_address)

  class Bitmap
    # Windows keeps bitmap pixels as BGRA with the bottom row first.
    # Our engine returns RGBA with the top row first. Convert both
    # ways so scripts see the layout they were written against.
    def __mkxp_swap_layout(data, row_bytes)
      rows = []
      y = (data.size / row_bytes) - 1
      while y >= 0
        rows << data[y * row_bytes, row_bytes]
        y -= 1
      end
      out = rows.join
      i = 0
      len = out.size
      while i < len
        out[i, 3] = out[i, 3].reverse
        i += 4
      end
      out
    end
    private :__mkxp_swap_layout

    # rubocop:disable Naming/AccessorMethodName -- names come from the Zeus81 API
    def get_data
      __mkxp_swap_layout(raw_data, width * 4)
    end

    def set_data(data)
      self.raw_data = __mkxp_swap_layout(data, width * 4)
      data
    end

    # Zeus hands out a fake String that points straight at the RGSS
    # pixel buffer, then frees the pointer. We give a plain copy and
    # a `free` that does nothing.
    def get_data_ptr
      data = get_data
      def data.free; end

      return data unless block_given?

      begin
        yield data
      ensure
        data.free
      end
    end
    # rubocop:enable Naming/AccessorMethodName

    # The address is meaningless here. Return 0 so any leftover
    # RtlMoveMemory call stays harmless.
    def last_row_address
      0
    end

    def export(filename, &on_finish)
      format = File.extname(filename).downcase
      case format
      when '.png', '.bmp', '.jpg', '.jpeg'
        to_file(filename)
      when ''
        filename = "#{filename}.png"
        to_file(filename)
      else
        print("Export format '#{format}' not supported.")
        return nil
      end
      on_finish.call if on_finish
      filename
    end

    def save(filename, &on_finish)
      export(filename, &on_finish)
    end

    # Games call this when `$MKXP` is set, and `export` on Windows.
    def save_to_file(filename)
      export(filename)
    end

    def export_png(filename, &on_finish)
      to_file(filename)
      on_finish.call if on_finish
      filename
    end

    def export_bmp(filename)
      to_file(filename)
      filename
    end
  end

  module Graphics
    class << self
      native_snap = Graphics.instance_variable_get(:@__mkxp_native_snap_to_bitmap)
      if native_snap && Graphics.const_defined?(:GetDIBits)
        define_method(:snap_to_bitmap) { native_snap.call }
      end

      if method_defined?(:export)
        def export(filename = nil)
          filename ||= Time.now.strftime("snapshot %Y-%m-%d %Hh%Mm%Ss #{frame_count}")
          bitmap = snap_to_bitmap
          return nil if bitmap.nil?

          begin
            bitmap.export(filename)
          ensure
            bitmap.dispose
          end
        end

        alias save export
        alias snapshot export
      end
    end
  end

  MKXP.puts('[bitmap-export] Zeus81 Bitmap Export rebuilt on native pixel access') if defined?(MKXP)
end
