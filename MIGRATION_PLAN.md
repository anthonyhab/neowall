# NeoWall Modular Architecture Migration Plan

## Current Situation

We have **two parallel implementations**:

### Old Monolithic System (Currently Active)
```
src/output.c          - ~1500 lines, manages outputs + rendering
src/render.c          - ~1500 lines, rendering + transitions + textures
include/neowall.h     - struct output_state (lines 105-210)
```

**Used by:**
- `src/compositor/backends/wayland/wayland_core.c`
- `src/eventloop.c`
- `src/config.c`
- `src/egl/egl_core.c`
- All GLES version implementations

### New Modular System (Completed, Not Integrated)
```
src/output/
├── output.h          - Clean output management API
└── output.c          - Output lifecycle and config

src/renderer/
├── renderer.h        - Pure rendering primitives
├── renderer.c        - Core rendering logic
├── textures/         - Texture management
│   ├── textures.h
│   └── textures.c
└── transitions/      - Transition effects
    ├── transitions.h
    ├── transitions.c
    ├── fade.c
    ├── slide.c
    ├── glitch.c
    └── pixelate.c
```

## Architecture Differences

### Old `struct output_state` (include/neowall.h)
- **210 lines** - massive struct with everything
- Contains: Wayland objects, EGL state, textures, shaders, uniforms, cache, FPS, config
- Tightly coupled - output + render + shader + texture all in one
- Used by entire codebase

### New `struct output_state` (src/output/output.h)
- **~30 lines** - clean separation
- Contains: Wayland objects, geometry, config, texture IDs
- Decoupled - delegates to renderer for actual rendering
- Not yet used by codebase

## Migration Strategy

### ❌ BAD: Big Bang Rewrite
- Replace all at once
- HIGH RISK - breaks everything
- Hard to test incrementally
- Difficult to roll back

### ✅ GOOD: Gradual Adapter Pattern

Keep old API, delegate to new implementation internally.

## Phase 1: Adapter Layer (This Phase)

### Step 1.1: Create Bridge Headers
Create `src/legacy_bridge.h` that maps old API to new:

```c
// Bridge old render.c API to new renderer
static inline bool render_frame(struct output_state *output) {
    transition_params_t params = {
        .prev_texture = output->next_texture,
        .current_texture = output->texture,
        .width = output->width,
        .height = output->height,
        .progress = output->transition_progress,
        .frame_count = output->frames_rendered
    };
    
    // Use new renderer
    // renderer_begin_frame(...);
    // renderer_render_frame(...);
    // renderer_end_frame(...);
    return true;
}
```

### Step 1.2: Keep Old output_state, Add New Context
Don't modify the old struct yet. Instead, add a pointer to new context:

```c
struct output_state {
    // ... all existing fields ...
    
    /* NEW: Bridge to modular system */
    renderer_context_t *renderer_ctx;    // New renderer
    struct output_config *new_output;    // New output module (if needed)
};
```

### Step 1.3: Implement Adapters in old files
Update `src/output.c` and `src/render.c` to delegate:

```c
// In src/render.c
bool render_init_output(struct output_state *output) {
    // Create new renderer context
    renderer_config_t config = {
        .egl_display = output->state->egl_display,
        .egl_context = output->state->egl_context,
        .gl_version = RENDERER_GLES_2_0,
        .enable_debug = true
    };
    
    output->renderer_ctx = renderer_create(&config);
    
    // ... keep old initialization for compatibility ...
    
    return output->renderer_ctx != NULL;
}

bool render_frame(struct output_state *output) {
    if (output->renderer_ctx) {
        // Use NEW rendering path
        return render_frame_new(output);
    } else {
        // Fallback to OLD rendering (legacy)
        return render_frame_legacy(output);
    }
}
```

### Step 1.4: Feature Flag
Add compile-time flag to switch between old/new:

```c
#ifdef USE_NEW_RENDERER
    return render_frame_new(output);
#else
    return render_frame_old(output);
#endif
```

## Phase 2: Gradual Migration

### Step 2.1: Migrate Functions One by One
- Start with simplest: `render_create_texture` → `renderer_texture_from_memory`
- Then: `render_destroy_texture` → `renderer_texture_destroy`
- Finally: Complex rendering loops

### Step 2.2: Update Call Sites (Low Risk Areas First)
1. Test harnesses and examples
2. Less-used features (pixelate transition)
3. Core rendering last

### Step 2.3: Parallel Testing
- Run old and new side-by-side
- Compare outputs
- Measure performance
- Fix discrepancies

## Phase 3: Cutover

### Step 3.1: Switch Default
```c
#ifndef USE_OLD_RENDERER  // Flip the default
    return render_frame_new(output);
#else
    return render_frame_old(output);
#endif
```

### Step 3.2: Deprecation Period
- Mark old functions as deprecated
- Update documentation
- Give users time to test

### Step 3.3: Remove Old Code
- Delete `src/output.c` (old)
- Delete `src/render.c` (old)
- Clean up includes
- Update Makefile

## Phase 4: Cleanup

### Step 4.1: Simplify output_state
Remove all renderer-specific fields:
- Shader programs, uniforms, VBOs → moved to renderer
- Texture management → moved to renderer
- Keep only output geometry, config

### Step 4.2: Update Public API
Expose new clean API in `include/neowall.h`

## Detailed Task List

### Immediate (Phase 1)
- [ ] Create `src/legacy_bridge.h`
- [ ] Add `renderer_ctx` pointer to old `output_state`
- [ ] Implement `render_frame_new()` using new renderer
- [ ] Implement `render_init_output_new()` 
- [ ] Add `USE_NEW_RENDERER` compile flag
- [ ] Test: Compile with both flags, ensure no regressions

### Short-term (Phase 2)
- [ ] Migrate texture functions
- [ ] Migrate transition rendering
- [ ] Migrate shader loading
- [ ] Update test suite
- [ ] Performance benchmarks

### Medium-term (Phase 3)
- [ ] Flip default to new renderer
- [ ] Beta testing period
- [ ] Fix reported issues
- [ ] Documentation updates

### Long-term (Phase 4)
- [ ] Remove old code
- [ ] Simplify structs
- [ ] API v2.0 release

## Risk Mitigation

### Testing Strategy
1. Unit tests for new modules (already done)
2. Integration tests with adapter layer
3. Visual regression tests (screenshot comparison)
4. Performance tests (FPS, memory)
5. Multi-monitor tests

### Rollback Plan
If new system has issues:
1. Flip compile flag back to old
2. Rebuild
3. Deploy
4. Fix issues offline

### Communication
- Document migration in CHANGELOG
- Provide migration guide for users
- Clear deprecation warnings

## Success Criteria

✅ **Phase 1 Complete when:**
- Old API still works
- New renderer can be enabled via flag
- Both paths tested and working

✅ **Phase 2 Complete when:**
- All rendering goes through new system
- Old code paths are fallbacks only
- Performance is equal or better

✅ **Phase 3 Complete when:**
- New renderer is default
- Old code is deprecated
- No regressions reported

✅ **Phase 4 Complete when:**
- Old code removed
- Codebase is 50% smaller
- API is clean and maintainable

## Current Status

- ✅ New modular system implemented
- ✅ Renderer fully decoupled from output
- ✅ All transitions completed
- ⏳ **NEXT: Implement Phase 1 adapter layer**

## Notes

- **Don't rush** - each phase should be stable before moving on
- **Test heavily** - rendering bugs are very visible to users
- **Measure everything** - performance regressions are unacceptable
- **Keep backward compatibility** - users should not notice the migration