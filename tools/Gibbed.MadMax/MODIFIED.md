# Modified copy

This folder is an altered copy of these projects. It is not the original software.

| Project | Upstream | Commit |
|---|---|---|
| Gibbed.MadMax | https://github.com/gibbed/Gibbed.MadMax | `75358e9` (2015-11-05) |
| Gibbed.IO (`Gibbed.IO/`) | https://github.com/gibbed/Gibbed.IO | `500198d` (2021-10-08) |
| NDesk.Options (`NDesk.Options/`) | https://github.com/gibbed/NDesk.Options | `5325b85` (2021-10-08) |

Changes, made so the tools build with the dotnet SDK:

- Every `*.csproj` in Gibbed.MadMax is converted to SDK-style and targets `net48`.
- Added `Directory.Build.props` (builds Gibbed.IO and NDesk.Options for `net48`).
- Added `Gibbed.IO/CompatUInt.cs`: `uint` overloads of `ReadToMemoryStream` and `ReadString`, because the new Gibbed.IO takes `int` only.
- `Gibbed.MadMax.FileFormats/SmallArchiveFile.cs`: casts `uint` lengths to `int` in the same two calls.

The original license is in `license.txt`.
