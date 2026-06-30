# sandbox-bindgen.rb
#
# This file is part of mkxp.
#
# Copyright (C) 2025 - 2026 The mkxp-z authors
#
# mkxp is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# mkxp is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with mkxp.  If not, see <http://www.gnu.org/licenses/>.

################################################################################

# The name passed as the `-n`/`--module-name` flag to `wasm2c`
MODULE_NAME = 'ruby'

# The name of the `malloc()` binding defined in ruby-bindings.h
MALLOC_FUNC = 'mkxp_sandbox_malloc'

# The name of the `free()` binding defined in ruby-bindings.h
FREE_FUNC = 'mkxp_sandbox_free'

################################################################################

IGNORED_FUNCTIONS = Set[
  'rb_class_descendants',
  'rb_close_before_exec',
]

EXTRA_FUNCTIONS = [
  'mkxp_sandbox_pump_threads	tags.c	/^void mkxp_sandbox_pump_threads(void);$/;"	p	signature:(void)',
]

ARG_HANDLERS = {
  'VALUE' => { keep: true, primitive: :size },
  'ID' => { keep: true, primitive: :size },
  'int' => { primitive: :s32 },
  'unsigned int' => { primitive: :u32 },
  'long' => { primitive: :ssize },
  'unsigned long' => { primitive: :size },
  'long long' => { primitive: :s64 },
  'unsigned long long' => { primitive: :u64 },
  'float' => { primitive: :f32 },
  'double' => { primitive: :f64 },
  'bool' => { primitive: :bool },
  '_Bool' => { primitive: :bool },
  'const char *' => {
    keep: true,
    buf_size: 'std::strlen(ARG) + 1',
    serialize: "bind.strcpy(BUF, ARG);\n",
  },
  'const VALUE *' => {
    keep: true,
    condition: lambda { |func_name, args, arg_index| arg_index > 0 && args[arg_index - 1] == 'int' }, # Only handle arguments of type `const VALUE *` if the previous argument is of type `int`
    buf_size: 'PREV_ARG * sizeof(VALUE)',
    serialize: <<~HEREDOC
      bind.arycpy(BUF, ARG, PREV_ARG);
    HEREDOC
  },
  'volatile VALUE *' => {
    keep: true,
    buf_size: 'sizeof(VALUE)',
    serialize: <<~HEREDOC
      bind.ref<VALUE>(BUF) = *ARG;
    HEREDOC
  },
  'void *' => {
    condition: lambda { |func_name, args, arg_index| args[arg_index + 1] == 'const rb_data_type_t *' }, # Only handle arguments of type `void *` if the next argument is of type `const rb_data_type_t *`
    primitive: :ptr
  },
  'const rb_data_type_t *' => {
    keep: true,
    formatter: lambda { |name| "const struct bindings::rb_data_type &#{name}" },
  },
  'VALUE (*)()' => {
    keep: true,
    anyargs: true,
    formatter: lambda { |name| "VALUE (*#{name})(ANYARGS)" },
    declaration: 'VALUE (*)(ANYARGS)',
  },
  'rb_alloc_func_t' => {
    keep: true,
    func_ptr_args: [:value],
    func_ptr_rets: [:value],
    formatter: lambda { |name| "VALUE (*#{name})(VALUE)" },
    declaration: 'VALUE (*)(VALUE)',
  },
  'VALUE (*)(VALUE)' => {
    keep: true,
    func_ptr_args: [:value],
    func_ptr_rets: [:value],
    formatter: lambda { |name| "VALUE (*#{name})(VALUE)" },
    declaration: 'VALUE (*)(VALUE)',
  },
  'VALUE (*)(VALUE,VALUE)' => {
    keep: true,
    func_ptr_args: [:value, :value],
    func_ptr_rets: [:value],
    formatter: lambda { |name| "VALUE (*#{name})(VALUE, VALUE)" },
    declaration: 'VALUE (*)(VALUE, VALUE)',
  },
  'rb_block_call_func_t' => {
    keep: true,
    func_ptr_args: [:value, :value, :s32, :ptr, :value],
    func_ptr_rets: [:value],
    formatter: lambda { |name| "VALUE (*#{name})(VALUE, VALUE, int32_t, wasm_ptr_t, VALUE)" },
    declaration: 'VALUE (*)(VALUE, VALUE, int32_t, wasm_ptr_t, VALUE)',
  }
}

RET_HANDLERS = {
  'void' => { keep: true, primitive: :void },
  'VALUE' => { keep: true, primitive: :size },
  'ID' => { keep: true, primitive: :size },
  'int' => { primitive: :s32 },
  'unsigned int' => { primitive: :u32 },
  'long' => { primitive: :ssize },
  'unsigned long' => { primitive: :size },
  'long long' => { primitive: :s64 },
  'unsigned long long' => { primitive: :u64 },
  'float' => { primitive: :f32 },
  'double' => { primitive: :f64 },
  'bool' => { primitive: :bool },
  '_Bool' => { primitive: :bool },
  'char *' => { primitive: :ptr },
  'const char *' => { primitive: :ptr },
}

VAR_TYPE_TABLE = {
  ssize: 'wasm_ssize_t',
  size: 'wasm_size_t',
  ptr: 'wasm_ptr_t',
  s32: 'int32_t',
  u32: 'uint32_t',
  s64: 'int64_t',
  u64: 'uint64_t',
  f32: 'float',
  f64: 'double',
  void: 'void',
  value: 'VALUE',
  bool: 'bool',
}

FUNC_TYPE_TABLE = {
  ssize: 'WASM_RT_ISIZE',
  size: 'WASM_RT_ISIZE',
  value: 'WASM_RT_ISIZE',
  ptr: 'WASM_RT_ISIZE',
  s32: 'WASM_RT_I32',
  u32: 'WASM_RT_I32',
  s64: 'WASM_RT_I64',
  u64: 'WASM_RT_I64',
  f32: 'WASM_RT_F32',
  f64: 'WASI_RT_F64',
}

def convert_type_to_unsigned(type)
  if type == 'wasm_ssize_t'
    'wasm_size_t'
  elsif type == 'int8_t'
    'uint8_t'
  elsif type == 'int16_t'
    'uint16_t'
  elsif type == 'int32_t'
    'uint32_t'
  elsif type == 'int64_t'
    'uint64_t'
  else
    type
  end
end

################################################################################

CALL_TYPES = [
  [:void, [:value]], # dmark, dfree, dcompact
  [:size, [:value]], # dsize
  [:value, [:s32, :ptr, :value]], # rb_define_method with argc = -1
  [:value, [:value, :value, :s32, :ptr, :value]], # rb_block_call_func_t
]
for i in 0..16
  CALL_TYPES.append([:value, [:value] * (i + 1)]) # rb_define_method with argc = i
end

$call_type_hash_salt = 0

def call_type_hash(call_type)
  h = ([$call_type_hash_salt] + call_type).hash.to_s(36)
  if h.start_with?('-')
    h = h[1..]
  end
  h
end

while CALL_TYPES.map { |call_type| call_type_hash(call_type) }.uniq.length < CALL_TYPES.length
  $call_type_hash_salt += 1
end

HEADER_START = <<~HEREDOC
  #ifndef MKXP_SANDBOX_BINDGEN_H
  #define MKXP_SANDBOX_BINDGEN_H

  #include "binding-sandbox/binding-base.h"

  // Autogenerated by sandbox-bindgen.rb. Don't manually modify this file - modify sandbox-bindgen.rb instead!

  namespace mkxp_sandbox {
      struct bindings : binding_base {
          bindings(std::shared_ptr<struct w2c_#{MODULE_NAME}> m);

          struct rb_data_type {
              friend struct bindings;
              rb_data_type();
              wasm_ptr_t get() const;
              private:
              wasm_ptr_t ptr;
              rb_data_type(wasm_ptr_t ptr);
          };

          struct rb_data_type rb_data_type(const char *wrap_struct_name, void (*dmark)(wasm_ptr_t), void (*dfree)(wasm_ptr_t), wasm_size_t (*dsize)(wasm_ptr_t), void (*dcompact)(wasm_ptr_t), wasm_ptr_t parent, wasm_ptr_t data, wasm_size_t flags);

HEREDOC

HEADER_END = <<~HEREDOC
  }

  #endif // MKXP_SANDBOX_BINDGEN_H
HEREDOC

PRELUDE = <<~HEREDOC
  // Autogenerated by sandbox-bindgen.rb. Don't manually modify this file - modify sandbox-bindgen.rb instead!

  #include <cstdarg>
  #include "mkxp-sandbox-bindgen.h"
  #include "mkxp-polyfill.h"

  #define _SBINDGEN_SLOT(slot_index) (bind.ref<typename slot_type<(slot_index), slots>::type>(bind.stack_pointer() + slot_offset<(slot_index), slots>::value))

  #ifdef MKXPZ_SANDBOX_MEMORY64
  #  define WASM_RT_ISIZE WASM_RT_I64
  #else
  #  define WASM_RT_ISIZE WASM_RT_I32
  #endif

  using namespace mkxp_sandbox;

  bindings::rb_data_type::rb_data_type() : ptr(0) {}

  bindings::rb_data_type::rb_data_type(wasm_ptr_t ptr) : ptr(ptr) {}

  wasm_ptr_t bindings::rb_data_type::get() const {
      if (ptr == 0) std::abort();
      return ptr;
  }

  bindings::bindings(std::shared_ptr<struct w2c_#{MODULE_NAME}> m) : binding_base(m) {}


  //////////////////////////////////////////////////////////////////////////////
HEREDOC

POSTSCRIPT = <<~HEREDOC

  struct bindings::rb_data_type bindings::rb_data_type(const char *wrap_struct_name, void (*dmark)(wasm_ptr_t), void (*dfree)(wasm_ptr_t), wasm_size_t (*dsize)(wasm_ptr_t), void (*dcompact)(wasm_ptr_t), wasm_ptr_t parent, wasm_ptr_t data, wasm_size_t flags) {
      wasm_ptr_t buf;
      wasm_ptr_t str;

      buf = sandbox_malloc(9 * sizeof(wasm_ptr_t));
      if (buf == 0) {
          MKXPZ_THROW(std::bad_alloc());
      }
      str = sandbox_malloc(std::strlen(wrap_struct_name) + 1);
      if (str == 0) {
          sandbox_free(buf);
          MKXPZ_THROW(std::bad_alloc());
      }

      this->strcpy(str, wrap_struct_name);
      ref<wasm_ptr_t>(buf, 0) = str;

      ref<wasm_ptr_t>(buf, 1) = dmark == nullptr ? 0 : wasm_rt_push_funcref(&instance().w2c_T0, wasm_rt_funcref_t {
          wasm2c_#{MODULE_NAME}_get_func_type(1, 0, #{FUNC_TYPE_TABLE[:ptr]}),
          (wasm_rt_function_ptr_t)_sbindgen_call_#{call_type_hash([:void, [:value]])},
          {nullptr},
          (void *)dmark,
      });

      ref<wasm_ptr_t>(buf, 2) = dfree == nullptr ? 0 : wasm_rt_push_funcref(&instance().w2c_T0, wasm_rt_funcref_t {
          wasm2c_#{MODULE_NAME}_get_func_type(1, 0, #{FUNC_TYPE_TABLE[:ptr]}),
          (wasm_rt_function_ptr_t)_sbindgen_call_#{call_type_hash([:void, [:value]])},
          {nullptr},
          (void *)dfree,
      });

      ref<wasm_ptr_t>(buf, 3) = dsize == nullptr ? 0 : wasm_rt_push_funcref(&instance().w2c_T0, wasm_rt_funcref_t {
          wasm2c_#{MODULE_NAME}_get_func_type(1, 0, #{FUNC_TYPE_TABLE[:ptr]}, #{FUNC_TYPE_TABLE[:size]}),
          (wasm_rt_function_ptr_t)_sbindgen_call_#{call_type_hash([:size, [:value]])},
          {nullptr},
          (void *)dsize,
      });

      ref<wasm_ptr_t>(buf, 4) = dcompact == nullptr ? 0 : wasm_rt_push_funcref(&instance().w2c_T0, wasm_rt_funcref_t {
          wasm2c_#{MODULE_NAME}_get_func_type(1, 0, #{FUNC_TYPE_TABLE[:ptr]}),
          (wasm_rt_function_ptr_t)_sbindgen_call_#{call_type_hash([:void, [:value]])},
          {nullptr},
          (void *)dcompact,
      });

      ref<wasm_ptr_t>(buf, 5) = 0;
      ref<wasm_ptr_t>(buf, 6) = parent;
      ref<wasm_ptr_t>(buf, 7) = data;
      ref<wasm_ptr_t>(buf, 8) = flags;

      return buf;
  }

  //////////////////////////////////////////////////////////////////////////////


HEREDOC

################################################################################

declarations = []
coroutines = []
call_bindings = []
func_names = []
globals = []
consts = []

# Create a `_sbindgen_call_` function for each possible call signature of every function pointer that can be passed to the Ruby C API:
# - `void (*)(VALUE)` (for dmark, dfree and dcompact)
# - `size_t (*)(VALUE)` (for dsize)
# - `VALUE (*)(int, VALUE *, VALUE)` (for rb_define_method with argc = -1)
# - `VALUE (*)(void)` (for rb_define_method with argc = 0)
# - `VALUE (*)(VALUE)` (for rb_define_method with argc = 1)
# - `VALUE (*)(VALUE, VALUE)` (for rb_define_method with argc = 2 or argc = -2)
# - `VALUE (*)(VALUE, VALUE, VALUE)` (for rb_define_method with argc = 3)
# - Similarly for argc = 4 through argc = 16
for call_type in CALL_TYPES
  call_return_type = VAR_TYPE_TABLE[call_type[0]]
  call_arg_types = call_type[1].map { |t| VAR_TYPE_TABLE[t] }
  call_bindings.append(
    <<~HEREDOC
      static #{convert_type_to_unsigned(call_return_type)} _sbindgen_call_#{call_type_hash(call_type)}(#{(["#{call_return_type} (*func)(#{call_arg_types.join(', ')})"] + (0...call_arg_types.length).map { |i| "#{convert_type_to_unsigned(call_arg_types[i])} a#{i}" }).join(', ')}) {
          #{call_type[0] == :void ? '' : "return (#{convert_type_to_unsigned(call_return_type)})"}func(#{(0...call_arg_types.length).map { |i| "(#{call_arg_types[i]})a#{i}" }.join(', ')});
      }
    HEREDOC
  )
end

# Find all `RUBY_Q` values defined in the Ruby headers (e.g. `RUBY_Qnil`, `RUBY_Qfalse`)
File.readlines('tags', chomp: true).each do |line|
  line = line.split("\t")

  # Skip tags that are not enumerators (the values inside of an `enum` declaration)
  next unless line[3] == 'e'

  # Skip enumerators whose name does not consist of "RUBY_Q" followed by a lowercase Latin letter
  const_name = line[0]
  next unless const_name.match?(/^RUBY_Q[a-z]/)
  const_name = const_name[6..]

  # Extract the numerical value of this enumerator
  match = line[2].match(/(?<==) *(?:(?:[1-9][0-9]*)|(?:0x[0-9A-Fa-f]+))(?=[,;]?\$\/)/)
  next if match.nil?
  value = match[0]

  consts.append([const_name.upcase, value.strip.to_i(value.strip.start_with?('0x') ? 16 : 10)])
end

# Find all `rb_` global variable declarations in the Ruby headers
File.readlines('tags', chomp: true).each do |line|
  line = line.split("\t")

  # Skip tags that are not variable declarations
  next unless line[3] == 'x'

  # Skip all variable declarations where the variable name does not start with "rb_"
  global_name = line[0]
  next unless global_name.match?(/^rb_[a-z][A-Z]/)

  # Skip all variable declarations where the variable type is something other than `VALUE`
  signature = line[2]
  next unless signature.start_with?('/^extern VALUE ')

  globals.append(global_name)
end

# Find all `rb_` and `mkxp_` functions declared in the Ruby headers and generate bindings for them
(File.readlines('tags', chomp: true) + EXTRA_FUNCTIONS).each do |line|
  line = line.split("\t")

  # Skip tags that are not function declarations
  next unless line[3] == 'p'

  # Skip functions that do not begin with "rb_" or "mkxp_", that end with "_static" (our bindings are unable to handle functions that take static strings as arguments) or are in the list of functions that we want to exclude
  func_name = line[0]
  next unless func_name.start_with?('rb_') || func_name.start_with?('mkxp_')
  next if func_name.end_with?('_static')
  next if IGNORED_FUNCTIONS.include?(func_name)

  # Only bind functions whose return type matches one of the return types we have a handler for
  ret = line[2]
  next unless ret.start_with?('/^') && ret.include?('(')
  ret = ret.gsub(/\[\[.*\]\]/, '') # Remove attributes like [[noreturn]]
  ret = ret[2..].partition('(')[0].strip
  next unless ret.include?(' ') && ret.rpartition(' ')[2].end_with?(func_name)
  ret = ret[...-func_name.length].strip
  next unless RET_HANDLERS.include?(ret)

  # Only bind functions whose arguments all match an argument type we have a handler for
  args = line[4]
  next unless args.start_with?('signature:(') && args.end_with?(')')
  args = args[11...-1]
  args = args
    .gsub('VALUE,VALUE', '$').split(',').map { |arg| arg.gsub('$', 'VALUE,VALUE') } # Split into an array, where each element is one argument as a string, e.g. 'int argc' or 'char **argv' or 'void (*func)(int, void *)'
    .filter { |arg| arg != 'void' } # Remove arguments that are equal to 'void'
    .map { |arg| arg == '...' ? '...' : arg.match?(/\(\* \w+\)/) ? arg.gsub(/\(\* \w+\)/, '(*)') : arg.rpartition(' ')[0].strip } # Retrieve only the type of each argument, removing the variable name
  next unless (0...args.length).all? { |i| args[i] == '...' || (ARG_HANDLERS.include?(args[i]) && (ARG_HANDLERS[args[i]][:condition].nil? || ARG_HANDLERS[args[i]][:condition].call(func_name, args, i))) }

  coroutine_initializer = ''
  destructor = []
  transformed_args = Set[]
  num_slots = 0
  i = 0
  args.each_with_index do |arg, i|
    next if arg == '...'
    handler = arg == 'const VALUE *' && func_name.start_with?('rb_funcall') ? {primitive: :ptr} : ARG_HANDLERS[arg]

    # Generate bindings for converting the arguments
    if !handler[:func_ptr_args].nil? || handler[:anyargs]
      if handler[:anyargs]
        coroutine_initializer += <<~HEREDOC
          switch (a#{args.length - 1}) {
              case -1:
                  _SBINDGEN_SLOT(#{num_slots}) = wasm_rt_push_funcref(&bind.instance().w2c_T0, wasm_rt_funcref_t {
                      wasm2c_#{MODULE_NAME}_get_func_type(3, 1, #{FUNC_TYPE_TABLE[:s32]}, #{FUNC_TYPE_TABLE[:ptr]}, #{FUNC_TYPE_TABLE[:value]}, #{FUNC_TYPE_TABLE[:value]}),
                      (wasm_rt_function_ptr_t)_sbindgen_call_#{call_type_hash([:value, [:s32, :ptr, :value]])},
                      {nullptr},
                      (void *)a#{i},
                  });
                  break;
              case -2:
                  _SBINDGEN_SLOT(#{num_slots}) = wasm_rt_push_funcref(&bind.instance().w2c_T0, wasm_rt_funcref_t {
                      wasm2c_#{MODULE_NAME}_get_func_type(2, 1, #{FUNC_TYPE_TABLE[:value]}, #{FUNC_TYPE_TABLE[:value]}, #{FUNC_TYPE_TABLE[:value]}),
                      (wasm_rt_function_ptr_t)_sbindgen_call_#{call_type_hash([:value, [:value, :value]])},
                      {nullptr},
                      (void *)a#{i},
                  });
                  break;
        HEREDOC
        for j in 0..16
          case_str = <<~HEREDOC
            case #{j}:
                _SBINDGEN_SLOT(#{num_slots}) = wasm_rt_push_funcref(&bind.instance().w2c_T0, wasm_rt_funcref_t {
                    wasm2c_#{MODULE_NAME}_get_func_type(#{j + 1}, 1, #{([FUNC_TYPE_TABLE[:value]] * (j + 2)).join(', ')}),
                    (wasm_rt_function_ptr_t)_sbindgen_call_#{call_type_hash([:value, [:value] * (j + 1)])},
                    {nullptr},
                    (void *)a#{i},
                });
                break;
          HEREDOC
          coroutine_initializer += case_str.split("\n").map { |line| "    #{line}".rstrip }.join("\n") + "\n"
        end
        coroutine_initializer += <<~HEREDOC
              default:
                  std::abort();
          }
        HEREDOC
      else
        coroutine_initializer += <<~HEREDOC
          _SBINDGEN_SLOT(#{num_slots}) = wasm_rt_push_funcref(&bind.instance().w2c_T0, wasm_rt_funcref_t {
              wasm2c_#{MODULE_NAME}_get_func_type(#{handler[:func_ptr_args].length}, #{handler[:func_ptr_rets].length}#{handler[:func_ptr_args].empty? && handler[:func_ptr_rets].empty? ? '' : ', ' + (handler[:func_ptr_args] + handler[:func_ptr_rets]).map { |type| FUNC_TYPE_TABLE[type] }.join(', ')}),
              (wasm_rt_function_ptr_t)_sbindgen_call_#{call_type_hash([handler[:func_ptr_rets].empty? ? :void : handler[:func_ptr_rets][0], handler[:func_ptr_args]])},
              {nullptr},
              (void *)a#{i},
          });
        HEREDOC
      end
      coroutine_initializer += "\n"
      transformed_args.add(i)
      num_slots += 1
    elsif !handler[:buf_size].nil?
      coroutine_initializer += <<~HEREDOC
        {
            wasm_ptr_t p = bind.sandbox_malloc(#{handler[:buf_size].gsub('PREV_ARG', "a#{i - 1}").gsub('ARG', "a#{i}")});
            if (p == 0) MKXPZ_THROW(std::bad_alloc());
            _SBINDGEN_SLOT(#{num_slots}) = p;
        }
      HEREDOC
      coroutine_initializer += handler[:serialize].gsub('PREV_ARG', "a#{i - 1}").gsub('ARG', "a#{i}").gsub('BUF', "_SBINDGEN_SLOT(#{num_slots})")
      coroutine_initializer += "\n"
      transformed_args.add(i)
      num_slots += 1
    end

    i += 1
  end

  coroutine_vars = []

  # If this is a varargs function, manually generate bindings for getting the varargs based on the function name
  if !args.empty? && args[-1] == '...'
    case func_name
    when 'rb_funcall'
      coroutine_initializer += <<~HEREDOC
        {
            wasm_ptr_t fp = w2c_#{MODULE_NAME}_rb_wasm_get_stack_pointer(&bind.instance());
            wasm_ptr_t sp = fp - CEIL_WASMSTACKALIGN(a#{args.length - 2} * sizeof(VALUE));
            if (sp > fp) {
                MKXPZ_THROW(std::bad_alloc());
            }
            _SBINDGEN_SLOT(#{num_slots}) = sp;
            _SBINDGEN_SLOT(#{num_slots + 1}) = fp;
            w2c_#{MODULE_NAME}_rb_wasm_set_stack_pointer(&bind.instance(), sp);
            std::va_list a;
            va_start(a, a#{args.length - 2});
            for (long i = 0; i < a#{args.length - 2}; ++i) {
                bind.ref<VALUE>(sp, i) = va_arg(a, VALUE);
            }
            va_end(a);
        }
      HEREDOC
      coroutine_initializer += "\n"
      num_slots += 2
    when 'rb_rescue2'
      coroutine_initializer += <<~HEREDOC
        {
            std::va_list a, b;
            va_start(a, a#{args.length - 2});
            va_copy(b, a);
            wasm_size_t n = 0;
            do ++n; while (va_arg(b, VALUE));
            va_end(b);
            wasm_ptr_t fp = w2c_#{MODULE_NAME}_rb_wasm_get_stack_pointer(&bind.instance());
            wasm_ptr_t sp = fp - CEIL_WASMSTACKALIGN(n * sizeof(VALUE));
            if (sp > fp) {
                MKXPZ_THROW(std::bad_alloc());
            }
            _SBINDGEN_SLOT(#{num_slots}) = sp;
            _SBINDGEN_SLOT(#{num_slots + 1}) = fp;
            w2c_#{MODULE_NAME}_rb_wasm_set_stack_pointer(&bind.instance(), sp);
            for (wasm_size_t i = 0; i < n; ++i) {
                bind.ref<VALUE>(sp, i) = va_arg(a, VALUE);
            }
            va_end(a);
        }
      HEREDOC
      coroutine_initializer += "\n"
      num_slots += 2
    else
      next
    end
  end

  old_num_slots = num_slots
  num_slots = 0
  coroutine_initializer = (
      (0...args.length).map do |i|
      arg = args[i]
      if arg == '...'
        num_slots += 2
        "_SBINDGEN_SLOT(#{num_slots - 2}) = 0;\n_SBINDGEN_SLOT(#{num_slots - 1}) = 0;"
      elsif transformed_args.include?(i)
        num_slots += 1
        "_SBINDGEN_SLOT(#{num_slots - 1}) = 0;"
      else
        nil
      end
    end
      .filter { |line| line != nil }
      .join("\n") + "\n\n"
  )
    .lstrip + coroutine_initializer
  num_slots = old_num_slots

  ret_handler = RET_HANDLERS[ret]

  coroutine_ret = !RET_HANDLERS[ret][:keep] ? VAR_TYPE_TABLE[RET_HANDLERS[ret][:primitive]] : ret;

  coroutine_vars.append("#{coroutine_ret} r") if ret_handler[:primitive] != :void

  coroutine_args = (0...args.length).map do |i|
    handler = args[i] == 'const VALUE *' && func_name.start_with?('rb_funcall') ? {primitive: :ptr} : ARG_HANDLERS[args[i]]
    args[i] == '...' ? '...'
      : !handler[:formatter].nil? ? handler[:formatter].call("a#{i}")
      : !handler[:keep] ? "#{VAR_TYPE_TABLE[handler[:primitive]]} a#{i}"
      : "#{args[i]} a#{i}"
  end

  declaration_args = (0...args.length).map do |i|
    handler = args[i] == 'const VALUE *' && func_name.start_with?('rb_funcall') ? {primitive: :ptr} : ARG_HANDLERS[args[i]]
    args[i] == '...' ? '...'
      : !handler[:formatter].nil? ? handler[:formatter].call('')
      : !handler[:keep] ? "#{VAR_TYPE_TABLE[handler[:primitive]]}"
      : "#{args[i]}"
  end

  j = 0
  coroutine_inner = <<~HEREDOC
    #{ret_handler[:primitive] == :void ? '' : 'r = '}w2c_#{MODULE_NAME}_#{func_name}(#{(['&bind.instance()'] + (0...args.length).map do |i|
      if args[i] == '...' || transformed_args.include?(i)
        j += 1
        "_SBINDGEN_SLOT(#{j - 1})"
      else
        args[i] == 'const rb_data_type_t *' ? "a#{i}.get()" : "a#{i}"
      end
    end).join(', ')});
    if (w2c_#{MODULE_NAME}_asyncify_get_state(&bind.instance()) != 1) break;
    BOOST_ASIO_CORO_YIELD;
  HEREDOC

  old_num_slots = num_slots
  coroutine_destructor = <<~HEREDOC
    void #{func_name}::end() noexcept {
    #{(0...args.length)
      .map do |i|
        i = args.length - 1 - i
        arg = args[i]
        if arg == '...'
          num_slots -= 2
          "    if (_SBINDGEN_SLOT(#{num_slots + 1}) != 0) w2c_#{MODULE_NAME}_rb_wasm_set_stack_pointer(&bind.instance(), _SBINDGEN_SLOT(#{num_slots + 1}));"
        elsif transformed_args.include?(i)
          num_slots -= 1
          "    if (_SBINDGEN_SLOT(#{num_slots}) != 0) bind.sandbox_free(_SBINDGEN_SLOT(#{num_slots}));"
        else
          nil
        end
      end
      .filter { |line| line != nil }
      .join("\n")}
    }
  HEREDOC
  num_slots = old_num_slots

  coroutine_definition = <<~HEREDOC
    #{func_name}::#{func_name}(struct binding_base &b) : bind(b) {}
    #{coroutine_ret} #{func_name}::operator()(#{coroutine_args.join(', ')}) {#{coroutine_vars.empty? ? '' : (coroutine_vars.map { |var| "\n    #{var} = 0;" }.join + "\n")}
        BOOST_ASIO_CORO_REENTER (this) {
    #{coroutine_initializer.empty? ? '' : (coroutine_initializer.split("\n").map { |line| "        #{line}".rstrip }.join("\n") + "\n\n")}        for (;;) {
    #{coroutine_inner.split("\n").map { |line| "            #{line}" }.join("\n")}
            }
        }#{ret_handler[:primitive] == :void ? '' : "\n\n    return r;"}
    }#{coroutine_destructor.empty? ? '' : ("\n" + coroutine_destructor)}
  HEREDOC

  coroutine_declaration = <<~HEREDOC
    struct #{func_name} : boost::asio::coroutine {
        typedef decl_slots<#{(['wasm_ptr_t'] * num_slots).join(', ')}> slots;
        #{coroutine_ret} operator()(#{declaration_args.join(', ')});
        #{func_name}(struct binding_base &b);
        #{coroutine_destructor.empty? ? '' : "void end() noexcept;\n    "}private:
        struct binding_base &bind;
    };
  HEREDOC

  func_names.append(func_name)
  coroutines.append(coroutine_definition)
  declarations.append(coroutine_declaration)
end

File.open('mkxp-sandbox-bindgen.h', 'w') do |file|
  file.write(HEADER_START)
  for global_name in globals
    file.write("        inline VALUE #{global_name}() const noexcept { return ref<VALUE>(instance().w2c_#{global_name}); }\n")
  end
  file.write("    };")
  for declaration in declarations
    file.write("\n\n" + declaration.split("\n").map { |line| "    #{line}" }.join("\n").rstrip)
  end
  file.write("\n\n")
  for const in consts
    file.write("#define SANDBOX_#{const[0]} ((VALUE)#{const[1]}U)\n")
  end
  file.write(HEADER_END)
end
File.open('mkxp-sandbox-bindgen.cpp', 'w') do |file|
  file.write(PRELUDE)
  for call_binding in call_bindings
    file.write("\n\n")
    file.write(call_binding.rstrip + "\n")
  end
  for coroutine in coroutines
    file.write("\n\n")
    file.write(coroutine.rstrip + "\n")
  end
  file.write(POSTSCRIPT)
end
