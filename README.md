# UE5 Agent CVar 🎛️🤖

[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.5-313131?logo=unrealengine)](https://www.unrealengine.com/)
[![Editor Plugin](https://img.shields.io/badge/Type-Editor%20Plugin-blue)](#)
[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Anthropic Claude](https://img.shields.io/badge/AI-Anthropic%20Claude-d97706)](https://www.anthropic.com/)
[![Status](https://img.shields.io/badge/Status-Active-brightgreen)](#)

Find and tune Unreal Engine console variables (CVars) without leaving the editor — either by **describing your problem in plain English** and letting Claude pick the right CVars, or by **searching the catalog live** as you type. ✨

---

## What it does 🛠️

- 💬 **Ask AI** — type something like *"my shadows flicker on foliage at distance"* and get a ranked list of CVars with suggested values and a one-line reason for each.
- 🔎 **Manual search** — live substring match across all 6,600+ UE 5.5 CVars (name + description).
- ⚡ **Apply / Reset** — every result has an editable value field, an **Apply** button (sets the CVar instantly), and a **Reset to Default** button. Suggested values stay in the box so you can flip back and forth.
- 📜 **Two logs** — Agent Process log (Claude's prompts & responses) and CVar History log (every apply/reset). Both Ctrl+C-friendly.
- 🔐 **Per-project key storage** — your Anthropic API key is saved to `GEditorPerProjectIni`, never to source control.

---

## Install via GitHub 📦

1. Open your Unreal project folder. If it doesn't have a `Plugins/` folder, create one.
2. Clone this repo into it:
   ```
   cd YourProject/Plugins
   git clone https://github.com/LucidDelta/UE5AgentCVar.git
   ```
3. Right-click your `.uproject` file → **Generate Visual Studio project files**.
4. Open the project. When prompted to compile the new module, click **Yes**.
5. Once the editor opens, go to **Tools → UE5 Agent CVar**.

> 📝 Don't have a C++ project? Add an empty C++ class through **Tools → New C++ Class** first, then come back to step 1. (UE Editor needs a C++ pipeline to compile plugins.)

---

## First-run setup 🔑

1. Open the panel from **Tools → UE5 Agent CVar**.
2. Click the ⚙️ gear icon.
3. Paste your Anthropic API key. Get one at [console.anthropic.com](https://console.anthropic.com/).
4. Pick a model — `claude-haiku-4-5-…` is fast and cheap, perfect for this. 🐎

That's it. The key is saved per-project, so you only do this once.

---

## Usage 🎯

**AI mode**
1. Type a problem (e.g. *"reduce GPU memory used by virtual textures"*).
2. Set the result count (default 10, any positive integer).
3. Click **Submit** — recommended CVars appear as cards.

**Manual mode**
1. Type into the search box. Results update live, no button needed.
2. Edit the value, hit **Apply**.

---

## Requirements ✅

- Unreal Engine 5.5
- A C++ Unreal project (Blueprint-only projects can't compile plugins)
- An Anthropic API key

---

## License 📄

[MIT](LICENSE) — do whatever you want, just keep the notice.

---

## Built with [Claude Code](https://claude.com/claude-code) 🤝
