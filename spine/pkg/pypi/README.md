# logan-spine-mcp

mcp-name: io.github.DeusData/logan-spine-mcp

**Fast code intelligence engine for AI coding agents.** Indexes an average repository in milliseconds, the Linux kernel (28M LOC) in 3 minutes. Answers structural queries in under 1ms.

This Python wrapper downloads the selected `logan-spine-mcp` runtime set from [GitHub Releases](https://github.com/DeusData/logan-spine-mcp/releases) on first run and verifies it before publishing it in your OS cache directory. The set contains the native executable and authenticated integration asset, with the graph UI always embedded.

## Installation

```bash
pip install logan-spine-mcp
# or
pipx install logan-spine-mcp
```

There is one composition per platform: the graph UI ships in every build, so no variant selection is needed.

## Usage

```bash
logan-spine-mcp install   # configure your coding agents
logan-spine-mcp --help
```

## Supported platforms

| OS      | Architecture |
|---------|-------------|
| macOS   | arm64, amd64 |
| Linux   | arm64, amd64 |
| Windows | arm64, amd64 |

## Full documentation

See [github.com/DeusData/logan-spine-mcp](https://github.com/DeusData/logan-spine-mcp)
