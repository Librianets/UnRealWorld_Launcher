$ErrorActionPreference='Stop'
. "$PSScriptRoot\..\URWUpdater.ps1"
$work=Join-Path $env:TEMP ('URWUpdaterTests_'+[guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($work)|Out-Null
$checks=0
function Assert($ok,$label){if(-not $ok){throw "FAIL: $label"};$script:checks++;Write-Output "PASS: $label"}
function File($p,$s){[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($p))|Out-Null;[IO.File]::WriteAllText($p,$s)}
function Fixture($name,$managed,$edition='standard'){
 $base=Join-Path $work $name;$stage=Join-Path $work ($name+'_stage');[IO.Directory]::CreateDirectory($base)|Out-Null;[IO.Directory]::CreateDirectory($stage)|Out-Null
 File "$base\urw.exe" 'clean game';$game=Hash "$base\urw.exe"
 File "$base\URWTrace.exe" 'old loader';File "$base\URW_Launcher.exe" 'old GUI';File "$base\URWTrace.ini" 'keep my settings';File "$base\rus_translate\catalog.json" 'my translation';File "$base\rus_translate\translations.csv" 'my edited CSV';File "$base\rus_translate\personal.txt" 'leave this file'
 if($managed){[IO.File]::Copy("$base\urw.exe","$base\urw-original.exe");[IO.File]::Copy("$base\URWTrace.exe","$base\urw.exe",$true);$hash=Hash "$base\urw.exe";File "$base\URWSteam.state" "URWSteam1`n$hash`n-`n$game`n"}
 $files=[ordered]@{}
 $names=@('URW_Launcher.exe','URWTrace.exe','URWTraceHook.dll','URWTrace.ini','URWUpdater.ps1')
 foreach($lang in @('rus_translate','fin_translate')){$names+=@('catalog.json','runtime_grammar.json','legacy_catalog.json','URW.FNT')|ForEach-Object{"$lang/$_"};if($edition -eq 'standard'){$names+="$lang/catalog.html";$names+="$lang/translations.csv"}}
 foreach($n in $names){File (Join-Path $stage $n) ('new '+$n);$files[$n]=Hash (Join-Path $stage $n)}
 $m=[pscustomobject]@{schema=1;version='0.26';edition=$edition;game_sha256=$game;files=[pscustomobject]$files}
 File "$stage\update_manifest.json" ($m|ConvertTo-Json -Depth 6)
 return @{base=$base;stage=$stage;manifest=$m}
}
try {
 foreach($managed in @($false,$true)){
  $f=Fixture "upgrade_$managed" $managed;Install-Package $f.base $f.stage $f.manifest
  Assert ((Get-Content -Raw "$($f.base)\URWTrace.ini") -ceq 'keep my settings') 'INI preserved'
  Assert ((Get-Content -Raw "$($f.base)\URW_update_backup\rus_translate\catalog.json") -ceq 'my translation') 'edited translation backed up'
  Assert ((Get-Content -Raw "$($f.base)\rus_translate\personal.txt") -ceq 'leave this file') 'unrelated file preserved'
  if($managed){Assert ((Hash "$($f.base)\urw.exe") -ceq (Hash "$($f.base)\URWTrace.exe")) 'Steam loader refreshed';Assert ((Hash "$($f.base)\urw-original.exe") -ceq $f.manifest.game_sha256) 'original game unchanged'}
  else{Assert ((Hash "$($f.base)\urw.exe") -ceq $f.manifest.game_sha256) 'unmanaged original unchanged'}
 }
 $f=Fixture 'switch_lite' $true 'lite';Install-Package $f.base $f.stage $f.manifest
 Assert (-not(Test-Path "$($f.base)\rus_translate\translations.csv")) 'Lite removes editor CSV'
 Assert (Test-Path "$($f.base)\URW_update_backup\rus_translate\translations.csv") 'removed CSV backed up'
 Assert (Test-Path "$($f.base)\rus_translate\personal.txt") 'Lite keeps foreign file'
 $next=Fixture 'switch_standard' $false;Install-Package $f.base $next.stage $next.manifest
 Assert (Test-Path "$($f.base)\rus_translate\catalog.html") 'Lite to Standard installs editor at same version'
 $f=Fixture 'rollback' $true;$before=Hash "$($f.base)\urw.exe";$script:FailAfterCopy=2;$threw=$false
 try{Install-Package $f.base $f.stage $f.manifest}catch{$threw=$true};$script:FailAfterCopy=0
 Assert $threw 'injected mid-copy failure reported';Assert ((Get-Content -Raw "$($f.base)\URW_Launcher.exe") -ceq 'old GUI') 'rollback restores GUI';Assert ((Hash "$($f.base)\urw.exe") -ceq $before) 'rollback keeps old Steam loader';Assert (-not(Test-Path "$($f.base)\URW_update_pending.json")) 'rollback clears journal'
 $f=Fixture 'unsupported' $false;File "$($f.base)\urw.exe" 'another version';$threw=$false;try{Install-Package $f.base $f.stage $f.manifest}catch{$threw=$true};Assert $threw 'unsupported game refused';Assert ((Get-Content -Raw "$($f.base)\URW_Launcher.exe") -ceq 'old GUI') 'unsupported refusal makes no changes'
 $f=Fixture 'lock' $true;$lock=Lock-Update $f.base;$threw=$false;try{Install-Package $f.base $f.stage $f.manifest}catch{$threw=$true}finally{$lock.Dispose()};Assert $threw 'concurrent operation refused'
 $f=Fixture 'steam_updated' $true;File "$($f.base)\urw.exe" 'new Steam game';$f.manifest.game_sha256=Hash "$($f.base)\urw.exe";Install-Package $f.base $f.stage $f.manifest
 Assert ((Get-Content -Raw "$($f.base)\urw-original.exe") -ceq 'new Steam game') 'new Steam original kept';Assert ((Hash "$($f.base)\urw.exe") -ceq (Hash "$($f.base)\URWTrace.exe")) 'updated Steam game gets new loader';Assert ((Get-Content -Raw "$($f.base)\URW_update_backup\urw-original.exe") -ceq 'clean game') 'previous original preserved in backup'
 $f=Fixture 'zip' $false;Add-Type -AssemblyName System.IO.Compression.FileSystem;$zip=Join-Path $work 'package.zip';[IO.Compression.ZipFile]::CreateFromDirectory($f.stage,$zip);$dest=Join-Path $work 'extracted';[IO.Directory]::CreateDirectory($dest)|Out-Null
 # .NET CreateFromDirectory uses backslashes on Windows. Our public ZIPs use slash.
 $a=[IO.Compression.ZipFile]::Open($zip,[IO.Compression.ZipArchiveMode]::Update);foreach($e in @($a.Entries)){if($e.FullName.Contains('\')){$stream=$e.Open();$mem=[IO.MemoryStream]::new();$stream.CopyTo($mem);$stream.Dispose();$n=$e.FullName.Replace('\','/');$e.Delete();$new=$a.CreateEntry($n);$out=$new.Open();$mem.Position=0;$mem.CopyTo($out);$out.Dispose();$mem.Dispose()}};$a.Dispose()
 $m=Read-Package $zip (Hash $zip) '0.26' 'standard' $dest;Assert ($m.version -eq '0.26') 'valid package verified/extracted'
 $threw=$false;try{Read-Package $zip ('0'*64) '0.26' 'standard' $dest}catch{$threw=$true};Assert $threw 'bad ZIP checksum refused'
 $a=[IO.Compression.ZipFile]::Open($zip,[IO.Compression.ZipArchiveMode]::Update);$a.CreateEntry('../outside.txt')|Out-Null;$a.Dispose();$threw=$false
 try{Read-Package $zip (Hash $zip) '0.26' 'standard' (Join-Path $work 'bad')}catch{$threw=$true};Assert $threw 'ZIP traversal refused';Assert (-not(Test-Path (Join-Path $work 'outside.txt'))) 'no traversal output'
 $f=Fixture 'recovery' $false;Install-Package $f.base $f.stage $f.manifest;[IO.File]::Copy("$($f.base)\URW_update_backup\backup_manifest.json","$($f.base)\URW_update_pending.json");Recover-Pending $f.base
 Assert ((Get-Content -Raw "$($f.base)\URW_Launcher.exe") -ceq 'old GUI') 'pending transaction recovers backup'
 Write-Output "TOTAL: $checks passed"
} finally {Remove-Item -LiteralPath $work -Recurse -Force}
