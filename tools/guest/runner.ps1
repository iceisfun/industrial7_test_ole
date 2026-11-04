# Guest-side runner: runs the listed test exes and frames every line on COM1.
#   OLET begin <mode-list>        OLET mode <adm|std|dx>
#   OLET start <exe>              OLET out <exe> | <line>
#   OLET rc <exe> <code|TIMEOUT> <secs>
#   OLET done                     (only when every exe was attempted)
# Parameters: -Payload <dir>  -Modes adm,std  -Timeout <secs per exe>  -Full (also send RUN lines)
param([string]$Payload, [string]$Modes = 'adm', [int]$Timeout = 600, [switch]$Full)
$ErrorActionPreference = 'Continue'
$port = New-Object System.IO.Ports.SerialPort 'COM1', 115200, 'None', 8, 'One'
$port.WriteTimeout = 30000
$port.Open()
function Send([string]$s) {
    $s = ($s -replace '[^\x20-\x7e]', '?')
    $port.Write($s + "`r`n")
}
$work = 'C:\oletw'
$bin = Join-Path $work 'bin'; $res = Join-Path $work 'results'; $cwd = Join-Path $work 'cwd'
New-Item -ItemType Directory -Force -Path $bin, $res, $cwd | Out-Null
Copy-Item -Recurse -Force (Join-Path $Payload 'bin\*') $bin
& icacls $work /grant 'Users:(OI)(CI)M' /T /C /Q | Out-Null
$list = @(Get-Content (Join-Path $Payload 'list.txt') | Where-Object { $_.Trim() })
$filter = ''
if (Test-Path (Join-Path $Payload 'filter.txt')) { $filter = (Get-Content (Join-Path $Payload 'filter.txt') -Raw).Trim() }
Send ("OLET begin modes=$Modes exes=$($list.Count) timeout=$Timeout")
$os = (Get-CimInstance Win32_OperatingSystem)
Send ("OLET os build=$($os.BuildNumber) version=$($os.Version) sku=$($os.OperatingSystemSKU) arch=$($os.OSArchitecture)")

foreach ($mode in ($Modes -split ',')) {
    Send "OLET mode $mode"
    # identity of the token this mode runs under (kept in the raw log only, not in committed summaries)
    $pq = if ($mode -eq 'std') { "`"$bin\olet_runas.exe`" `"$bin\olet_probe.exe`"" } else { "`"$bin\olet_probe.exe`"" }
    $pl = & $env:ComSpec /c "$pq 2>&1"
    foreach ($l in ($pl | Where-Object { $_ -like "PROBE *" })) { Send "OLET probe $mode | $l" }
    foreach ($exe in $list) {
        $exe = $exe.Trim()
        $path = Join-Path $bin "$exe.exe"
        $outf = Join-Path $res "$exe.$mode.txt"
        Send "OLET start $exe"
        if (-not (Test-Path $path)) { Send "OLET rc $exe MISSING 0"; continue }
        $xargs = if ($filter) { " $filter" } else { '' }
        # adm and dx run with the session's own elevated token; std uses the restricted-token launcher
        if ($mode -eq 'std') { $inner = "`"$bin\olet_runas.exe`" `"$path`"$xargs" } else { $inner = "`"$path`"$xargs" }
        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = "$env:ComSpec"
        $psi.Arguments = "/c `"$inner > `"$outf`" 2>&1`""
        $psi.WorkingDirectory = $cwd
        $psi.UseShellExecute = $false
        $psi.CreateNoWindow = $true
        $sw = [Diagnostics.Stopwatch]::StartNew()
        $p = [Diagnostics.Process]::Start($psi)
        $done = $p.WaitForExit($Timeout * 1000)
        $code = 'TIMEOUT'
        if ($done) { $code = $p.ExitCode } else { & taskkill /F /T /PID $p.Id 2>&1 | Out-Null; Start-Sleep -Milliseconds 500 }
        $secs = [math]::Round($sw.Elapsed.TotalSeconds, 1)
        if (Test-Path $outf) {
            $fs = [IO.File]::Open($outf, 'Open', 'Read', 'ReadWrite')
            $sr = New-Object IO.StreamReader($fs)
            while (($line = $sr.ReadLine()) -ne $null) {
                if (-not $Full -and $line.StartsWith('RUN ')) { continue }
                Send "OLET out $exe | $line"
            }
            $sr.Close()
        }
        Send "OLET rc $exe $code $secs"
    }
}
Send 'OLET done'
$port.Close()
exit 0
