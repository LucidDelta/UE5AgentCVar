# CLAUDE.md — UE5AgentCVar

Context for Claude (or any AI assistant) continuing work on this plugin. Read this before making non-trivial changes — it covers the architecture and the engine quirks that are easy to trip over.

---

## What this plugin is

A **UE 5.5 Editor-only** plugin that helps developers find and tune console variables (CVars) in two ways:

1. **AI mode** — plain-English problem description → Anthropic Claude → ranked CVar recommendations with suggested values, an editable value field per result, Apply, and Reset to Default.
2. **Manual mode** — live substring search over the engine's CVar catalog (~6,600 entries), no API call.

Scope is deliberately narrow: it does **not** execute Python, generate Blueprint graphs, or write gameplay code. It only finds CVars and applies values via `IConsoleManager`.

The plugin name is `UE5AgentCVar`, the .uplugin loads as `Editor` / `PostEngineInit`, and there are no Runtime modules.

---

## Module layout

```
UE5AgentCVar.uplugin
preprocess_cvars.py                 ← offline tool, generates Resources/cvars.json
Resources/
  cvars.json                        ← preprocessed engine CVar catalog (~6.6k entries)
  UE5AgentCVar_MenuIcon.svg
Source/UE5AgentCVar/
  UE5AgentCVar.Build.cs
  Public/UE5AgentCVar.h             ← IModuleInterface, tab spawner
  Private/
    UE5AgentCVar.cpp                ← module startup, menu, tab
    UE5AgentCVarStyle.h/.cpp        ← FSlateStyleSet for the menu icon
    SUE5AgentCVarPanel.h/.cpp       ← root Slate widget (most of the UI)
    FCVarDatabase.h/.cpp            ← cvars.json loader, manual + pre-filter search
    FCVarAgentRunner.h/.cpp         ← Anthropic HTTP layer
```

Build deps:
- Public: `Core, CoreUObject, Engine, Slate, SlateCore, InputCore`
- Private: `EditorStyle, EditorWidgets, LevelEditor, ToolMenus, UnrealEd, Projects, ApplicationCore, WorkspaceMenuStructure, EditorFramework, HTTP, Json, JsonUtilities`

Do **not** add `PythonScriptPlugin` — this plugin has no Python path.

---

## How a query flows

### AI mode
1. User types a prompt and clicks **Submit**.
2. `FCVarDatabase::FilterForPrompt` tokenises the prompt, scores every CVar by name / keyword / description hits, returns the top 150 indices.
3. `FCVarAgentRunner::SendRecommendation` POSTs to `https://api.anthropic.com/v1/messages` with the candidate slice formatted as `- name | default=… | section=… | description`. The system prompt instructs Claude to reply with a **raw JSON array** of `{name, suggested_value, reason}`.
4. The HTTP completion fires on the **game thread** — the panel mutates Slate directly, no `AsyncTask` needed.
5. Response text is run through `StripMarkdownFences` defensively (models occasionally emit ` ```json … ``` ` despite instructions), then parsed as a JSON array.
6. `RebuildAIResults` rebuilds the AI scroll box from scratch as cards: name, default, section, description, AI reason, an editable value field pre-filled with the AI's suggestion, **Apply**, **Reset to Default**.

### Manual mode
1. User types into the search box; `OnManualSearchChanged` fires on every keystroke.
2. `FCVarDatabase::Search` does substring matching over name + description with positional weighting (name hits rank above description hits; prefix matches rank highest). Caps at 50 results.
3. `RebuildManualResults` builds identical cards minus the AI reason row. The value box is pre-filled with the CVar's default.

### Apply / Reset
- `ApplyCVar` calls `IConsoleManager::Get().FindConsoleVariable(*Name)->Set(*Value, ECVF_SetByConsole)`. Never via `GEngine->Exec` — that path doesn't surface failures cleanly.
- `ResetCVar` does the same with the default value, but **deliberately does not change the value box text** so the user can flip Apply ↔ Reset to compare suggested vs default with two clicks.
- Every Apply/Reset is appended to the CVar History Log with a timestamp.

---

## CVar metadata pipeline (`preprocess_cvars.py`)

The Python script is offline-only — run it once when the engine version changes. It reads a tab-separated dump of every CVar, splits on section headers, and produces `Resources/cvars.json` with one entry per CVar containing `name, default, description, section, keywords`.

Keyword extraction uses two passes:
- **Name tokenisation:** split on `.`, `-`, `_`, then split each chunk on **CamelCase + digit boundaries**. So `r.ShadowCopyCustomData` becomes `["r", "shadow", "copy", "custom", "data"]` rather than `["r", "shadowcopycustomdata"]`. This dramatically improves match quality for natural-language prompts.
- **Description tokenisation:** word-boundary regex, drop short tokens, drop a curated stopword list. The stopword set covers regular English filler **plus UE-doc filler** (`whether, default, allows, useful, specific, works, abc, …` — `abc` comes from the boilerplate `"showflag.abc=0"` example shared by 215 ShowFlag descriptions).

Result is ~12 keywords per CVar, ~7,100 unique tokens across the catalog.

If you want to push ranking quality further, the obvious next step is **TF-IDF rarity weighting** — words like `enable`, `debug`, `cache` appear in hundreds of CVars and should score lower than rare terms. Compute IDF once during preprocessing, store per-token weights, weight matches by them in `FilterForPrompt`. We left this out of v1 because the simple frequency-counting filter already feeds Haiku a useful 150-entry slice.

---

## Engine gotchas / Slate quirks

These are the ones we tripped on or sidestepped while building. Don't "clean them up" without checking why they're written this way.

### `WeakThis` shadow warning
`SCompoundWidget` (via `TSharedFromThis`) has a member named `WeakThis`. Naming a local `TWeakPtr<…> WeakThis = SharedThis(this);` raises C4458 → error. We use `WeakPanel`.

### `LogPath` shadow warning
`EngineLogs.h` exposes a global `LogPath`. Local variables named `LogPath` in `.cpp` files including it raise C4459. We use `AgentLogPath` and `HistoryLogPath`.

### `SComboBox` refresh
`SComboBox` exposes `RefreshOptions()`. **Don't** call `RequestListRefresh()`.

### `SThrobber` include
UE 5.5: `#include "Widgets/Images/SThrobber.h"`. Older docs put it under `Widgets/Notifications/` — wrong path now.

### `SSpinBox<int32>` callback signature
The max-results control is `SSpinBox<int32>`, not `SSlider`. Its `OnValueChanged` takes `int32`, not `float`. If you copy code from a slider example you will get C2665 here.

### Menu duplicate-entry trap
The nomad tab spawner auto-registers itself in the Tools menu via UE's dynamic `TabManagerSection`. If we **also** add a manual entry (which we do, so it lands next to sibling AI plugins), we get **two** entries unless the spawner is set to `.SetMenuType(ETabSpawnerMenuType::Hidden)`. That call is load-bearing — don't remove it.

### Live-coding menu duplicates
On hot reload, prior menu registrations stick around and produce duplicate entries. We call `UnregisterOwnerByName(TEXT("UE5AgentCVarMenuOwner"))` at the **top** of `RegisterMenus()`. The owner FName is stable (not `this`) so it survives module reload.

### Read-only log widget
The two log panels use `SMultiLineEditableText` with `IsReadOnly(true)` inside `SScrollBox`. **Not** `STextBlock` — that's not selectable, so Ctrl+C breaks. Don't "simplify" this.

### SVG icon rules
`FSlateVectorImageBrush` parses via nanosvg, which is picky:
- Root `<svg>` must have explicit `width` and `height` attributes.
- Path `d` attributes work most reliably on a single line.
- `fill-rule="evenodd"` is supported.

If you need another icon, consider `FAppStyle::GetBrush("Icons.…")` first — there's a large library of engine brushes and using one means no SVG to maintain.

---

## Anthropic API

- Endpoint: `https://api.anthropic.com/v1/messages`
- Headers: `x-api-key`, `anthropic-version: 2023-06-01`, `content-type: application/json`
- Body shape: `{ model, max_tokens, system: "<system prompt>", messages: [{role: "user", content: …}] }`
- Response: parse `content[0].text` (we sum text fragments across the array to be safe), strip fences, parse the result as a JSON array.
- HTTP callbacks fire on the game thread; direct Slate mutation in the lambda is safe.

The system prompt explicitly instructs **raw JSON array, no markdown, no preamble**, with required fields `name`, `suggested_value`, `reason`. We still defensively strip fences before parsing.

The model dropdown lists Haiku, Sonnet, and Opus IDs as static options. We don't fetch the live model list — the cost (extra round-trip per panel open) didn't justify the benefit. If a new model ID ships, add it to `GetDefaultModelOptions` in `SUE5AgentCVarPanel.cpp`.

---

## Settings persistence

Stored in `GEditorPerProjectIni` under `[UE5AgentCVar]`:
- `AnthropicAPIKey` — entered through the gear panel, password-style field
- `HaikuModel` — currently selected model ID

Loaded in `Construct` (via `LoadSettings`), saved on every change (via `SaveSettings`). Per-project, per-user. Never committed to source control.

---

## Logs

Two distinct log files in `<Project>/Saved/UE5AgentCVar/`:
- `agent.log` — Claude reasoning steps: every prompt, response, error
- `history.log` — every Apply / Reset action with old → new values

Each has its own collapsible viewer in the panel and a Clear button. `AppendAgentLog` and `AppendHistoryLog` append to the file and refresh the on-screen viewer if it's currently expanded.

---

## What NOT to do

- Don't add Python execution. Out of scope; the sibling UE5AgentPython plugin handles that.
- Don't add other AI providers. Anthropic-only — keeps the system prompt and response shape simple.
- Don't change Reset to Default to also overwrite the value box text. The current behavior — silent CVar reset, value box untouched — is intentional so users can flip Apply ↔ Reset cheaply.
- Don't remove `.SetMenuType(ETabSpawnerMenuType::Hidden)` or the `UnregisterOwnerByName` call.
- Don't replace either log widget with `STextBlock`.
- Don't hard-code `C:\Users\…` paths. Use `IPluginManager::Get().FindPlugin(...)->GetBaseDir()` for plugin resources and `FPaths::ProjectSavedDir()` for logs.
- Don't commit `Saved/` or `Binaries/`. The API key lives in `Saved/`.

## What's fair game

- Tightening the system prompt for specific CVar tuning patterns.
- TF-IDF or embedding-based candidate filtering (replace `FilterForPrompt`).
- Result filtering / sorting in the panel (by section, alphabetical, etc.).
- Exporting recommendations as a `DefaultEngine.ini` snippet.
- Re-running `preprocess_cvars.py` against a newer engine version's CVar dump.
- Streaming the API response if you want incremental result display.
