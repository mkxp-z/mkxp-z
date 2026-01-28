# Custom Shader Implementation for MKXP-Z

## Overview

This document describes the implementation of custom GLSL shader support for the Ruby `Shader` class and its integration with the `Viewport` class.

## Implementation Summary

### What Was Implemented

1. **CustomShader C++ Class** (`src/display/customshader.h` and `src/display/customshader.cpp`)
   - New disposable class that loads GLSL fragment shaders from external files
   - Wraps the internal `ShaderBase` class
   - Uses a built-in passthrough vertex shader
   - Loads fragment shader code from the file specified in the constructor

2. **Ruby Binding** (`binding/shader-binding.cpp`)
   - Exposes the `Shader` class to Ruby
   - Constructor: `Shader.new('path/to/shader.glsl')`
   - Provides `filename` method to get the shader file path
   - Implements `dispose` and `disposed?` methods (via Disposable)

3. **Viewport Integration**
   - Added `shader` property to the `Viewport` class
   - Modified `src/display/viewport.h` to include shader attribute
   - Modified `src/display/viewport.cpp` to store shader reference
   - Modified `binding/viewport-binding.cpp` to expose `shader=` method

4. **Type Registration**
   - Added `Shader` type to `binding/binding-types.h`
   - Registered shader binding in `binding/binding-mri.cpp`

## Usage Example (Ruby)

```ruby
# Create a custom shader from a GLSL fragment shader file
shader = Shader.new('shaders/sepia.glsl')

# Create a viewport
viewport = Viewport.new(0, 0, 640, 480)

# Apply the shader to the viewport
viewport.shader = shader

# The shader is now associated with the viewport
# (Note: Actual rendering integration would need to be implemented in the viewport rendering code)

# Clean up
shader.dispose
viewport.dispose
```

## Shader File Format

The shader file should contain a GLSL fragment shader. Example (`sepia.glsl`):

```glsl
#ifdef FRAGMENT_SHADER
varying vec2 v_texCoord;
uniform sampler2D texture;

void main() {
    vec4 color = texture2D(texture, v_texCoord);
    
    // Sepia tone effect
    float gray = dot(color.rgb, vec3(0.299, 0.587, 0.114));
    vec3 sepia = vec3(gray) * vec3(1.2, 1.0, 0.8);
    
    gl_FragColor = vec4(sepia, color.a);
}
#endif
```

## Current Limitations & TODO

### 1. **Build Configuration** ⚠️ REQUIRED
The new source files must be added to the Xcode project:
- `src/display/customshader.h`
- `src/display/customshader.cpp`
- `binding/shader-binding.cpp`

**Action Required:**
- Open `/Users/luka/dev/mkxp-z-ext/macos/mkxp-z.xcodeproj` in Xcode
- Add the three files above to the project
- Ensure they're included in the target's compile sources

### 2. **Viewport Rendering Integration** ⚠️ NOT YET IMPLEMENTED
The shader is stored in the viewport but **not yet applied during rendering**. To actually use the shader, you need to modify the viewport's `composite()` method in `src/display/viewport.cpp` to:
- Check if a custom shader is set (`p->shader != nullptr`)
- Bind the shader before rendering viewport contents
- Apply the shader's projection matrix
- Unbind after rendering

Example integration (to be added to `viewport.cpp:composite()`):
```cpp
void Viewport::composite()
{
    if (emptyFlashFlag)
        return;

    bool renderEffect = p->needsEffectRender(flashing);

    if (elements.getSize() == 0 && !renderEffect)
        return;

    /* Setup scissor */
    glState.scissorTest.pushSet(true);
    glState.scissorBox.pushSet(p->rect->toIntRect());

    // Apply custom shader if set
    if (p->shader && !p->shader->isDisposed())
    {
        ShaderBase *customShader = p->shader->getShader();
        customShader->bind();
        customShader->applyViewportProj();
    }

    Scene::composite();

    // Unbind shader
    if (p->shader && !p->shader->isDisposed())
    {
        Shader::unbind();
    }

    /* If any effects are visible, request parent Scene to
     * render them. */
    if (renderEffect)
        scene->requestViewportRender
                (p->color->norm, flashColor, p->tone->norm);

    glState.scissorBox.pop();
    glState.scissorTest.pop();
}
```

### 3. **Shader Uniform Support** (Future Enhancement)
Currently, the shader loads with no way to set custom uniforms from Ruby. To make shaders more flexible, you could:
- Add methods like `shader.set_uniform(name, value)` to the Ruby API
- Store uniform values in CustomShaderPrivate
- Apply them before rendering

### 4. **Multiple Shader File Support** (Future Enhancement)
Currently only loads fragment shaders. Could be extended to:
- Support separate vertex and fragment shader files
- Auto-detect file type by extension (.vert vs .frag)
- Support combined shader files with preprocessor directives

## Files Modified/Created

### Created:
- `src/display/customshader.h` - CustomShader class header
- `src/display/customshader.cpp` - CustomShader class implementation
- `binding/shader-binding.cpp` - Ruby binding for Shader class
- `SHADER_IMPLEMENTATION.md` - This documentation file

### Modified:
- `src/display/viewport.h` - Added shader attribute
- `src/display/viewport.cpp` - Added shader storage and getter/setter
- `binding/viewport-binding.cpp` - Added shader property binding
- `binding/binding-types.h` - Added Shader type declaration
- `binding/binding-mri.cpp` - Registered shader binding initialization

## Testing Steps

1. **Build the project** - Add files to Xcode and compile
2. **Create a test shader** - Create a simple GLSL fragment shader file
3. **Test Ruby integration:**
   ```ruby
   shader = Shader.new('test.glsl')
   puts shader.filename  # Should print 'test.glsl'
   
   viewport = Viewport.new(0, 0, 640, 480)
   viewport.shader = shader
   puts viewport.shader.filename  # Should print 'test.glsl'
   
   shader.dispose
   puts shader.disposed?  # Should print 'true'
   ```

## Notes

- The implementation follows MKXP-Z's existing patterns for disposable objects
- Shaders are reference-counted (not copied) when assigned to viewports
- The vertex shader is currently hardcoded as a simple passthrough
- Fragment shaders should use `v_texCoord` for texture coordinates
- The shader file must exist in the game's file system when loading
