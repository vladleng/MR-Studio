param([string]$Threads)
$ErrorActionPreference='Stop'
if(!$Threads){$Threads=Read-Host 'CPU threads for BOTH backends (1, 2, 4, 8, 16)'}
if($Threads -notin @('1','2','4','8','16')){throw 'Choose exactly 1, 2, 4, 8 or 16'}
$exe=Join-Path $PSScriptRoot 'Moon River Studio JUCE.exe'
if(!(Test-Path -LiteralPath $exe)){throw 'Extract the entire runtime ZIP before starting the benchmark'}
$existing=@(Get-Process -ErrorAction SilentlyContinue | Where-Object {$_.ProcessName -eq 'Moon River Studio JUCE'})
if($existing.Count){throw 'Close MR Studio before selecting a benchmark thread count'}
$cpus=@(Get-CimInstance Win32_Processor)
$manifest=Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot 'SHA256.json') | ConvertFrom-Json
$folder=Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'MR Studio/SeparationBenchmarks'
New-Item -ItemType Directory -Force -Path $folder | Out-Null
$stamp=Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$report=Join-Path $folder "host-$stamp-$Threads-threads.json"
$env:MRS_SEPARATION_BENCHMARK_THREADS=$Threads
$affinity=(Get-Process -Id $PID).ProcessorAffinity.ToInt64()
$available=0
for($i=0;$i -lt 64;$i++){if(($affinity -band ([long]1 -shl $i)) -ne 0){$available++}}
@{
 version=$manifest.version;source=$manifest.source;configured_threads=[int]$Threads
 cpu_names=@($cpus | ForEach-Object {$_.Name})
 physical_cores=($cpus | Measure-Object NumberOfCores -Sum).Sum
 logical_processors=($cpus | Measure-Object NumberOfLogicalProcessors -Sum).Sum
 launcher_affinity_mask=$affinity;launcher_affinity_logical_processors=$available
 windows_job_cpu_quota='not measured; affinity alone does not establish CPU capacity'
 native_build='x64 portable CPU; ENGINE_ENABLE_NATIVE_CPU=OFF; see Actions log for ISA/BLAS flags'
 notes='Configured threads are not measured active cores. Compare CPU mode and HTDemucs6; separate SMT from physical cores. Worker wall includes startup/model load; repeated jobs are not warm inference sessions.'
} | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $report -Encoding UTF8
Write-Host "Both backends: $Threads configured CPU threads. Select CPU in the separation dialog."
Write-Host "Host report: $report"
Write-Host 'Use the same HTDemucs6 clip/range/chunk/stems; alternate backends and repeat each at least twice.'
Write-Host 'Close Studio and restart this launcher to change threads. Normal EXE launch retains the 1..4 default.'
$process=Start-Process -FilePath $exe -WorkingDirectory $PSScriptRoot -PassThru
$process.WaitForExit()
exit $process.ExitCode
