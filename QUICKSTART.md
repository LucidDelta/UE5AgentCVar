# Quickstart 🚀

Get the plugin running in five minutes.

## 1. Drop it in your project 📂

```
cd YourUnrealProject/Plugins
git clone https://github.com/LucidDelta/UE5AgentCVar.git
```

If `Plugins/` doesn't exist, create it.

## 2. Generate project files 🔨

Right-click `YourProject.uproject` → **Generate Visual Studio project files**.

> 💡 Blueprint-only project? Add any empty C++ class first via **Tools → New C++ Class** so Unreal sets up a build pipeline.

## 3. Compile 🛠️

Open the `.uproject`. Unreal will prompt to build the new module — click **Yes** and wait. First build takes a minute or two.

## 4. Open the panel 🪟

In the editor: **Tools → UE5 Agent CVar**.

## 5. Add your API key 🔑

1. Click the ⚙️ gear icon at the top of the panel.
2. Paste your Anthropic API key. Get one at [console.anthropic.com](https://console.anthropic.com/).
3. Pick a model. `claude-haiku-4-5-…` is the cheap/fast default.

Saved per-project, never committed to source control. ✅

## 6. Try it 🎯

**AI:**
> "my level streaming hitches when I drive across the world"

Click **Submit**. Read the suggestions, edit any value, click **Apply**.

**Manual:**
> Type `r.shadow` in the search box.

Results update live. Edit, Apply, done.

---

Need more detail or troubleshooting? See [README.md](README.md). 📘
