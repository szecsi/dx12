# VR work status — handoff notes

Written for picking this work up in a fresh Claude Code session (e.g. on the
VR test machine). Everything referenced here is already committed
(`git log` as of this note: `e673d8b dxc`, `e345c54 dlls`, `080a2cb retam vr`).
`git pull` gets you all of it.

## What exists

Two new OpenXR-based projects, both `Egg::OpenXRApp` subclasses (not
`SimpleApp`/`ScriptedApp` — OpenXR has no DXGI swap chain, and device/adapter
creation must be driven by the XR runtime's required LUID, so these are a
separate app-class hierarchy from the desktop projects):

- **`g-VrTest`** — minimal proof-of-pipeline: a single spinning per-face-
  colored cube. Confirmed working on real hardware (HP Reverb G2). Use this
  as the reference for "is the basic pipeline/toolchain OK" if `g-RetamVr`
  ever breaks in a confusing way — if the cube test still works, the bug is
  retam-specific, not OpenXR-plumbing-specific.
- **`g-RetamVr`** — VR port of g-Retam's stroke-hatching pipeline. Renders
  one hardcoded knight chess piece (`Media/chess/knight.obj`, 0.6m tall,
  1.2m in front of the headset's starting pose, slowly spinning). **Builds
  clean but is untested on hardware** — every run so far was on a
  headset-less dev machine, which only exercises `xrCreateInstance` before
  failing at "no runtime found." None of the asset-loading or per-eye
  rendering code has executed yet.

Also: `Egg::OpenXRApp` (`Egg/OpenXR/OpenXRApp.{h,cpp}`) is the shared base
class both of the above derive from — it owns the XR session/swapchain
lifecycle and per-eye command list submission. Both projects reuse it as-is.

## Bugs found and fixed along the way (all in shared code, so already benefit `g-VrTest` too)

- **`XR_ERROR_API_VERSION_UNSUPPORTED`**: was requesting `XR_CURRENT_API_VERSION`
  (1.1.60 from the vendored headers) instead of `XR_MAKE_VERSION(1,0,0)`.
  WMR's runtime only supports core 1.0. Fixed in `OpenXRApp::InitXrInstance()`.
- **Transform convention bug**: `XrPoseToViewMatrix`/`XrFovToProjectionMatrix`
  were written for column-vector math (`v'=M*v`), but this engine uses
  row-vector convention throughout (`world*view*proj`, `mul(pos,wvp)` in
  HLSL). Fixed by transposing both results (`OpenXRApp.cpp`). This is why
  the cube was invisible ("greyness everywhere") before the fix — verify
  numerically before touching this kind of thing again: identity rotation +
  camera at `(0,0,-1)` should put a world-origin point at view-space `z=1`.
- **`CreateRenderTargetView(texture, nullptr, handle)`** on the XR swapchain
  images caused `DXGI_ERROR_DEVICE_REMOVED`. OpenXR compositors commonly
  hand back **typeless** swapchain textures (so the app can choose sRGB vs
  UNORM), and a null view-desc can't infer a format from a typeless
  resource. Fixed by passing an explicit `D3D12_RENDER_TARGET_VIEW_DESC`
  with `DXGI_FORMAT_R8G8B8A8_UNORM_SRGB` (`OpenXRApp.cpp`,
  `CreateSwapChainResources()`).
- **`dxcompiler.dll`/`dxil.dll` missing on other machines**: every project
  links `dxcompiler.lib`, and `Egg::Mesh::Material::SetVertexShader/
  SetPixelShader` calls `DxcCreateInstance` for shader reflection on every
  material built — so this isn't VR-specific, `g-Retam` needs it too. These
  two DLLs weren't vendored anywhere in the repo before; now they live in
  `Common/DXC/bin/` and get copied to `Bin/` by `UtilCopyDLL`'s post-build
  step, same as PhysX/assimp/OpenXR. If a fresh machine still hits "DLL not
  found," rebuild the whole solution once (not just one project) so
  `UtilCopyDLL` actually runs — a single-project MSBuild target build does
  NOT trigger it.
- **Missing `.vcxproj.user`**: without `LocalDebuggerWorkingDirectory =
  $(OutDir)`, F5-debugging from Visual Studio defaults to the project
  folder, and `Egg::Shader::LoadCso`'s relative paths (`Shaders/...`) then
  fail to find anything (they only resolve correctly when CWD = `Bin/`).
  Both new projects have this file now; if you ever add a third VR
  project, don't forget it.

## Known risk areas in `g-RetamVr` specifically (untested — expect an iteration loop like `g-VrTest` needed)

- **Knight scale/position** (0.6m tall, `(0,0,-1.2)`) is a first guess, not
  measured/verified.
- **`perFrameCb->ahead`** is derived from the eye pose quaternion
  (rotating local `+Z`) — this one actually matters, `retam256PS.hlsl`
  uses it in the stroke-density/LOD calculation, not just a hint.
- **Faithfully-replicated quirk, not a bug to "fix" if you notice it**:
  `retam256CollectPS.hlsl`'s `uvMask` texture binding is never actually
  correctly bound (root param 4 gets overwritten by the UAV table binding
  at param 3) — this is pre-existing behavior in desktop `RetamApp.h` too,
  harmless because the code path that would use it is commented out
  (`//if(distToShore < 0.00001) return...`). Ported as-is deliberately.
- **Scope cuts**: no native GUI (retam material constants are `RetamApp`'s
  hardcoded defaults, no live tuning), no `retamReCollect`/mien=3 debug
  pass (off by default on desktop too).

## If something fails on the test machine

- Report the **exact error text**, and whether it's a hex code (compare
  against `GetDeviceRemovedReason`/HRESULT values — the assistant found
  three different root causes this way already: `INVALID_CALL`,
  `API_VERSION_UNSUPPORTED`, and a plain E_INVALIDARG from a PSO desc).
- `g-VrTest`'s PSO-creation failure path pulls the D3D12 debug layer's
  actual validation message into the assert dialog via `ID3D12InfoQueue`
  (see `VrTestApp.h`) instead of just the generic HRESULT — if `g-RetamVr`
  hits a similar PSO/device error, the same technique is worth adding
  there too before guessing blind.
- Build/run commands used throughout this work (from `C:\Coding\dx12`):
  ```
  & "C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\MSBuild.exe" GraphGame.sln /t:g-RetamVr /p:Configuration=Debug /p:Platform=x64
  ```
  Run a full solution build (no `/t:`) at least once after a fresh clone/pull
  so `UtilCopyDLL` populates `Bin/` with everything (PhysX, assimp, OpenXR,
  DXC DLLs) before running anything.
