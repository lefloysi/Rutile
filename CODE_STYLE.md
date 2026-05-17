# Rutile Code Style

This document is normative. If code and this file disagree, update the code or update this file in
the same change. Do not leave tribal knowledge living only in chat.

- Always use braces for control flow, even for single-line bodies.
- Do not suppress unused-parameter warnings with `(void)name`; warnings are acceptable.
- Rutile API calls throw by writing thread-local error state. They do not return `rt_error` unless the function is explicitly an error query.
- Always check `rtError()` after Rutile calls that can throw.
- Destroy functions never throw.
- Destroying `RT_NULL_HANDLE` is defined behavior.
- Calling `rtExit()` without a successful `rtInit()` is defined behavior.
- Public standard API belongs in `rutile.h`, not backend-specific headers.
- Public extension API belongs in its extension header, such as `rt_ext_swapchain.h`, and must be
  wired through validation and logging layers when those layers wrap the extension.
- Public opaque handles use the `rt_` prefix style, such as `rt_buffer`, not `_h` suffixes.
- Use `RT_NULL_HANDLE` for null Rutile handles.
- Use Rutile primitive aliases in project code: `u08`, `u16`, `u32`, `u64`, `usize`, `uptr`,
  `i08`, `i16`, `i32`, `i64`, `f32`, and `f64`. Raw C/C++ primitive spellings are only for
  C strings, language-required signatures such as `int main`, and external API boundaries where
  the external function requires that exact type.
- Function declarations and definitions stay on one line when the whole signature is 100 characters
  or less, including the opening brace for definitions.
- Only wrap a function signature when the one-line form would exceed 100 characters.
- Declare local variables at the point where they become necessary. Do not place a dense stack of
  unrelated locals at the top of a function.
- In implementation files, public `rt*` functions go at the top; private/internal `rtvk_*` functions go below them.

## Public API Shape

- Public functions are operations, not setters for hidden future operations. If a call defines memory,
  shader linkage, swapchain extent, or another object state transition, make that transition explicit
  in the operation that owns it.
- The first argument of an object operation is the object being operated on. For example,
  `rtBufferData(buffer, ...)`, not `rtBufferData(queue, buffer, ...)`.
- Do not expose backend implementation controls such as queues for uploads unless the user is
  genuinely choosing queue behavior. If a backend can choose the upload queue safely, the backend
  chooses it.
- Do not expose CPU mapping for buffers. User code copies data through Rutile calls such as
  `rtBufferData`, `rtBufferSubdata`, and `rtBufferRead`. Backend code may map host-visible memory
  internally, but no public API may return a mapped pointer or require the user to manage map/unmap.
- Do not expose framebuffer internals through public getters. Framebuffers are render targets used by
  command buffers; users should not pull texture views back out of them through core API calls.
- Public object configuration that must be finalized before use gets an explicit finalize operation.
  Graphics programs must call `rtGraphicsProgramLink` after shader/layout setup and before binding.
- If changing configuration invalidates a finalized object, mark it unfinalized and require the user to
  finalize again. Do not silently keep using stale backend objects.

## Buffers

- `rtBufferData` defines buffer memory. Its signature includes mode, usage, size, and optional data.
  Do not add separate public `rtBufferMode` or `rtBufferUsage` calls.
- Buffer modes:
  - `RT_BUFFER_STATIC`: data is expected to change rarely. Backends may use staging and device-local
    memory. If work is submitted to upload the data, return the submission timepoint.
  - `RT_BUFFER_DYNAMIC`: data changes often. Backends should use host-visible storage and copy into it
    directly when that is appropriate. CPU-completed copies return a null/completed timepoint.
- Buffer usage flags describe how the buffer may be used, such as staging, vertex, index, uniform,
  storage, transfer source, and transfer destination.
- Staging usage means the buffer is host-visible transfer memory and is not a drawable/bindable
  render input by itself.
- `rtBufferSubdata` preserves the mode and usage chosen by the last `rtBufferData`.
- Binding commands must validate usage. For example, vertex binding requires vertex usage and must
  reject staging-only storage.

## Swapchains

- Swapchain resize is explicit API. The application is expected to call `rtSwapchainResize` when its
  window, surface, layer, or drawable size changes. If the application keeps rendering after a resize
  without calling it, behavior is allowed to fail because the swapchain extent is stale.
- Do not require a callback from Rutile itself. Platform helpers, examples, and applications may use a
  window-system callback, such as a GLFW framebuffer-size callback, to call `rtSwapchainResize`.
- Vulkan may also encounter `VK_ERROR_OUT_OF_DATE_KHR` or `VK_SUBOPTIMAL_KHR`, but those are not a
  replacement for the explicit resize API. They are error/recovery signals, not the primary resize
  protocol.
- DX12 has no direct equivalent to `VK_ERROR_OUT_OF_DATE_KHR` for window resizing. Resize is performed
  by releasing back-buffer references and calling `IDXGISwapChain::ResizeBuffers`.
- Metal-style backends should treat drawable size similarly: update the layer/view drawable size from
  the platform resize path, then recreate or reacquire drawable-dependent resources.
- Successful swapchain creation, resize, acquire, and present paths must not print backend lifecycle
  messages. The logging layer is responsible for API tracing.

## Textures And Views

- A texture owns the underlying image/resource and its layout/state.
- A texture view owns only view metadata/handles, such as `VkImageView` or descriptor handles, plus any
  lifetime reference needed to keep the viewed texture alive.
- Do not store the backend image/resource handle directly on a texture view when the texture already
  owns it. Access image/resource state through the owning texture.
- Texture views may cache immutable view properties such as format and extent for convenience.

## Vulkan Barriers

- Do not write compact `vkCmdPipelineBarrier` argument groups such as `0, NULL` or `1, &barrier` on
  one line. Put each argument on its own line so the memory, buffer, and image barrier counts/pointers
  are visually unambiguous.
- When transitioning a texture view, update layout/state on the owning texture, not on the view.

## Logging And Output

- Backend internals are quiet on success. Do not print routine lifecycle/progress messages such as
  resource node created/reused/destroyed, swapchain created/resized, graphics pipeline created, device
  created, queue count, adapter selected, or allocator created.
- Backend startup timing is allowed and should stay concise: one successful initialization timing line
  per backend startup is fine.
- Use the logging layer for API tracing. If a developer wants call timing or object lifecycle output,
  it belongs in a layer or an opt-in diagnostic path, not the default backend.
- Backend output is acceptable for externally produced validation/debug messages and for unusual
  diagnostic failures that are not already represented by Rutile error state.
- Prefer setting Rutile error state over printing and continuing.

## Layers

- Layers are part of the API surface. When a public core or extension function is added, removed, or
  changes signature, update all layer proc tables, exported wrappers, private wrapper declarations,
  and state resolution in the same change.
- A layer may only expose an extension wrapper when the next chain exposes that function. The loader
  handles this by resolving through the next chain first; do not bypass that behavior.
- The validation layer should catch obvious misuse before it reaches a backend: null handles, missing
  required pointers, zero extents/counts where the operation cannot make sense, invalid enum values,
  command calls outside recording, and extension calls with missing next functions.
- Validation drops invalid calls after printing a validation message. It should not paper over the
  call by inventing backend resources or silently changing parameters.
- Validation should return a completed/null timepoint or zero result when it drops a call that would
  otherwise return work or data.
- Validation state must be reset when `rtExit` tears down the underlying implementation.
- The logging layer is transparent. It records arguments, return values, elapsed time, and backend
  error state, but it should not reject or reinterpret valid calls.
- Logging layer wrappers for optional extension functions may guard a missing next function because
  extension resolution can be partial while a user is probing support.

## Function Ordering

Function declarations and definitions are ordered by category first, then by name inside each category.
Separate categories with one empty line.

Use these categories when they apply:
- Resource lifetime: `Create`, then `Destroy`.
- Recording/session pairs: `Begin`, then the matching `End`.
- State/configuration commands.
- Data/upload/copy commands.
- Resize/recreate commands.
- Submit/query/wait commands.
- Internal helpers and backend-only functions.

Within a category, sort functions alphabetically by their operation name unless a stronger pairing rule applies.
Pairing rules override alphabetical order:
- `Create` is immediately followed by `Destroy`.
- `Begin` is immediately followed by the matching `End`.
- `Acquire` is immediately followed by the matching `Present` when they form the swapchain frame pair.
- `Resize` belongs after lifetime and before acquire/present for swapchains.
- `Retain` is immediately followed by `Release`.
- `Init` is immediately followed by `Finish`.

The first function in a pair supplies the alphabetical sort key for the pair. For example, `Begin`/`End`
sorts as `Begin`, and `Create`/`Destroy` sorts as `Create`.

Example:

```c
RTVK_API rt_texture rtTextureCreate(void);
RTVK_API void rtTextureDestroy(rt_texture texture);

RTVK_API rt_timepoint rtTextureCopy(...);
RTVK_API rt_timepoint rtTextureData(...);
RTVK_API rt_timepoint rtTextureSubcopy(...);
RTVK_API rt_timepoint rtTextureSubdata(...);
```
