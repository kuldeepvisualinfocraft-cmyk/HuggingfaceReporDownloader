# Hugging Face Repo Downloader

A standalone C++17 CLI tool that downloads entire Hugging Face repositories (models or datasets) by accepting a repo URL. No external dependencies — uses `curl.exe` (ships with Windows 10/11) for HTTP and Windows ANSI console for colored output.

## Features

- **Menu-based UI** — interactive console with colored prompts
- **Auto-fetches file list** via the Hugging Face API (`/api/models/...` or `/api/datasets/...`)
- **Full repo download** — creates a folder with the repo name and downloads every file
- **Progress feedback** — per-file progress bars via curl's `-#` flag
- **Output directory control** — change where repos are saved
- **Overwrite protection** — prompts before overwriting existing folders
- **Resilient** — continues downloading remaining files if one fails, reports failures at the end
- **Self-contained** — statically linked binary, zero runtime dependencies

## Usage

```
  ┌─────────────────────────────────────────────┐
  │  Main Menu                                  │
  │  [1] Enter Hugging Face URL                 │
  │  [2] Start Download                         │
  │  [3] Change Output Directory                │
  │  [4] Exit                                   │
  └─────────────────────────────────────────────┘
```

1. Run `hf_downloader.exe`
2. Select **1**, paste a URL (e.g. `https://huggingface.co/bert-base-uncased`)
3. The tool fetches and displays the file list
4. Select **2** to download all files into `./{repo_name}/`
5. Select **3** to change the output directory

## Supported URL Formats

- `https://huggingface.co/owner/repo`
- `https://huggingface.co/datasets/owner/repo`
- `owner/repo` (short form)

## Build from Source

Requires **MinGW-w64 g++** (or any C++17 compiler) and **curl.exe** (bundled with Windows 10/11).

```
g++ -std=c++17 -O2 -o hf_downloader.exe main.cpp -static
```

Or double-click `build.bat`.

## How It Works

1. **Parse URL** → extracts owner, repo name, and type (model/dataset)
2. **API Call** → `curl.exe` fetches `https://huggingface.co/api/{type}/{owner}/{repo}`
3. **JSON Parsing** → custom minimal recursive-descent parser extracts `siblings[].rfilename`
4. **Download** → iterates each file via `curl.exe -L -# -o` from `https://huggingface.co/{owner}/{repo}/resolve/main/{file}`

## Technical Notes

- The JSON parser is hand-written (~150 lines) — handles strings, numbers, objects, arrays, booleans, and null. No external library needed.
- ANSI escape sequences are enabled via `ENABLE_VIRTUAL_TERMINAL_PROCESSING` on Windows.
- The binary is ~3.5 MB when statically linked with MinGW.
