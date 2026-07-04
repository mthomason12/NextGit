# Trace Instructions — CheapGlk vs NextGlk Comparison

## Prerequisites

Both builds compiled with trace points in place:

- `./rebuild-cheapglk.ps1` — builds Git + CheapGlk
- `./rebuild-nextgit.ps1` — builds Git + NextGlk

All commands below are PowerShell, run from the project root.

## Step 1: CheapGlk Trace

```
cd thirdparty/git
Remove-Item -Force nextgit.sav -ErrorAction SilentlyContinue
"save`nrestore`nquit`n" | ./git /path/to/your/game.ulx 2>../../trace-cheap.log
cd ../..
```

## Step 2: NextGlk Trace

After rebuilding with `./rebuild-nextgit.ps1`:

```
cd thirdparty/git
Remove-Item -Force nextgit.sav -ErrorAction SilentlyContinue
"save`nrestore`nquit`n" | ./git /path/to/your/game.ulx 2>../../trace-next.log
cd ../..
```

## Step 3: Filter and Diff

```
Select-String -Path trace-cheap.log -Pattern "TRACE|req_line_event" | ForEach-Object { $_.Line -replace '0x[0-9a-f]{6,}', 'ADDR' } > trace-cheap-filtered.log
Select-String -Path trace-next.log -Pattern "TRACE|req_line_event" | ForEach-Object { $_.Line -replace '0x[0-9a-f]{6,}', 'ADDR' } > trace-next-filtered.log
diff (Get-Content trace-cheap-filtered.log) (Get-Content trace-next-filtered.log)
```

(Note: `2>` in PowerShell 5 may not work for stderr. If you see an error about `2>`, use `cmd /c` or run the git command from a bash shell instead.)

Any line in the diff indicates a Glk call whose parameter or return value differs between CheapGlk and NextGlk.
