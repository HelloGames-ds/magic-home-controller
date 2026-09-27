# TODO

Known bugs, unfinished work and open questions. Updated for 1.2.2.

## Bugs

- [ ] **In-app update check removed, not fixed.** Pressing "Check for updates" crashed
      the app. The button, the tray entry, the auto-check and `UpdateChecker` itself
      were deleted in 1.2.2 rather than fixed, so there is currently **no way to
      update from inside the app**. New versions have to be downloaded manually.
      The crash was most likely in the reply lifecycle (`deleteLater()` on the
      `QNetworkReply` while a `finished` handler was still using it), but this was
      never confirmed. If the feature comes back, it needs a proper fix first.
- [ ] **`installer.iss` cannot download the VC++ runtime.** Both attempts failed:
      `DownloadTemporaryFile` does not exist in Inno Setup 6.7.3, and
      `URLDownloadToFile` from `urlmon.dll` crashes with an access violation
      because COM is not initialised at that point (`external` declarations for a
      parameterless `OleInitialize` are rejected by the compiler).
      Workaround in place: `vc_redist.x64.exe` is bundled again, which puts the
      installer back to ~37.5 MB. A WinHTTP-based download would bring it to
      ~20 MB.
- [ ] **`vc_redist.x64.exe` is never removed from an existing installation.**
      It survives upgrades in `C:\Program Files\Magic Home Controller` and cannot
      be deleted without elevation, so machines upgraded from 1.2.0 keep a stale
      copy even after the fix above.
- [ ] **Build fails when the source path contains non-ASCII characters.**
      The em dash in the project folder name (`elk_c++ — копия`) makes
      `cmcldeps.exe` fail on `app.rc`, so the resource never compiles. The project
      must be configured through the 8.3 short path (`ELK_C_~2`). Worth fixing in
      `CMakeLists.txt` by referencing the icon relatively instead of by absolute
      path.

## Not finished

- [ ] **QSS compaction not visually verified.** Slider grooves went 8px → 6px,
      handles 16px → 13px, value labels 12px → 11px with tighter padding. It
      compiles and installs, but nobody has looked at the result in Advanced mode
      with long labels.
- [ ] **`AmbiCanvas` → `CaptureCanvas` rename not started.** The class name does
      not match what it does any more. Low priority, but it touches the whole
      editor, so it needs its own change with its own build.
- [ ] **No regression pass after the editor module split.** `AmbiEditor.cpp` /
      `AmbiEditorPanels.cpp` / `AmbiEditorWidgets.h` were split in 1.2.1 and the
      files compile, but zone saving, curve persistence and layer properties have
      only been spot-checked.
- [ ] **Editor always opens in Simple mode.** Layer state and mode were restored
      on open at one point, then deliberately reverted. `modeTouched_` now only
      guards against losing layers. Remembering the last mode is still open.
- [ ] **Translations are not regenerated for 1.2.2.** The removed update strings
      are still in `magic_home_controller_ru.ts` / `_en.ts`, and the QSS-adjacent
      label changes are not in the catalogues.
- [ ] **README screenshots are from an older build.** They show the update check
      button that no longer exists.

## Known limitations (not bugs)

- The headless test harness in `%TEMP%\opencode` is unreliable and has corrupted
  working state before. Manual testing is the only trustworthy signal right now.
- The app is unsigned, so SmartScreen warns on first run.
- `MHC_SYMBOLS` is `OFF` by default; without it `crash.log` contains raw
  addresses instead of function names.
