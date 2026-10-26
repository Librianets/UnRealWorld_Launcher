param([string]$Root, [ValidateSet('standard','lite')][string]$Edition='standard', [string]$CurrentVersion='0.25', [int]$ParentId=0, [ValidateSet('en','ru','fi')][string]$UILanguage='ru')
$ErrorActionPreference='Stop'
Set-StrictMode -Version 2
$script:FailAfterCopy=0
$script:ProgressForm=$null
function Pump { if($script:ProgressForm){[Windows.Forms.Application]::DoEvents()} }
function Text($en,$ru,$fi) { if($UILanguage -eq 'ru'){return $ru};if($UILanguage -eq 'fi'){return $fi};return $en }
function Hash($p) { (Get-FileHash -LiteralPath $p -Algorithm SHA256).Hash.ToLowerInvariant() }
function Allowed($name) {
 if($name -cmatch '^(URW_Launcher\.exe|URWTrace\.exe|URWTraceHook\.dll|URWTrace\.ini|URWUpdater\.ps1|Translation_README\.txt|Translation_HISTORY\.txt|THIRD_PARTY_LICENSES\.txt|CODE_LICENSE\.txt|LICENSE_SCOPE\.txt)$'){return $true}
 return $name -cmatch '^(rus_translate|fin_translate)/(catalog\.json|legacy_catalog\.json|runtime_grammar\.json|URW\.FNT|catalog\.html|translations\.csv|grammar\.json|provenance\.json\.gz|generated_files\.json|README\.txt|FONT_NOTES\.txt)$'
}
function Path-Safe($base,$name) {
 if($name -match '(^|[/\\])\.\.?([/\\]|$)' -or $name.Contains(':') -or $name.StartsWith('/') -or $name.StartsWith('\')){throw 'Unsafe relative path'}
 $full=[IO.Path]::GetFullPath((Join-Path $base $name));$prefix=[IO.Path]::GetFullPath($base).TrimEnd('\')+'\'
 if(-not $full.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw 'Path leaves installation'}
 $check=$full
 while($check.Length -ge $prefix.TrimEnd('\').Length){
  if(Test-Path -LiteralPath $check){if((Get-Item -Force -LiteralPath $check).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Linked installation paths are not supported'}}
  $check=[IO.Path]::GetDirectoryName($check);if(-not $check){break}
 }
 return $full
}
function Atomic-Copy($source,$dest) {
 [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($dest))|Out-Null
 $tmp=$dest+'.urw-update-tmp';if(Test-Path -LiteralPath $tmp){throw 'An update temporary file already exists'}
 try{[IO.File]::Copy($source,$tmp,$false);if(Test-Path -LiteralPath $dest){[IO.File]::Replace($tmp,$dest,[NullString]::Value)}else{[IO.File]::Move($tmp,$dest)}}finally{if(Test-Path -LiteralPath $tmp){[IO.File]::Delete($tmp)}}
}
function Check-Idle($base) {
 foreach($p in Get-Process -ErrorAction SilentlyContinue){
  if($p.ProcessName -notin @('urw','urw-original','URWTrace')){continue}
  try{$path=$p.Path}catch{throw 'Cannot check running game'}
  if(-not $path -or [IO.Path]::GetDirectoryName($path) -ieq $base){throw (Text 'Close the game first.' 'Сначала закройте игру.' 'Sulje peli ensin.')}
 }
}
function Lock-Update($base) {
 Check-Idle $base
 return [IO.FileStream]::new((Join-Path $base 'URWSteam.lock'),[IO.FileMode]::OpenOrCreate,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None,4096,[IO.FileOptions]::DeleteOnClose)
}
function Read-Package($zip,$expectedHash,$version,$edition,$dest) {
 if((Hash $zip) -cne $expectedHash){throw 'Archive SHA256 mismatch'}
 Add-Type -AssemblyName System.IO.Compression.FileSystem
 $a=[IO.Compression.ZipFile]::OpenRead($zip)
 try {
  $seen=@{};$total=0L
  foreach($e in $a.Entries){
   $name=$e.FullName
   if($seen.ContainsKey($name) -or ($name -cne 'update_manifest.json' -and -not (Allowed $name))){throw "Unexpected archive entry: $name"}
   $seen[$name]=$true;$total+=$e.Length
   if($e.Length -gt 536870912 -or $total -gt 1073741824){throw 'Package exceeds size limit'}
  }
  $entry=$a.GetEntry('update_manifest.json');if(-not $entry -or $entry.Length -gt 1048576){throw 'Update manifest missing or too large'}
  $reader=[IO.StreamReader]::new($entry.Open());try{$m=$reader.ReadToEnd()|ConvertFrom-Json}finally{$reader.Dispose()}
  if($m.schema -ne 1 -or $m.version -cne $version -or $m.edition -cne $edition -or $m.game_sha256 -cnotmatch '^[a-f0-9]{64}$'){throw 'Incompatible update manifest'}
  $files=@($m.files.PSObject.Properties)
  if($seen.Count -ne $files.Count+1){throw 'Manifest entry count mismatch'}
  foreach($f in $files){if(-not (Allowed $f.Name) -or -not $seen.ContainsKey($f.Name) -or $f.Value -cnotmatch '^[a-f0-9]{64}$'){throw 'Invalid file manifest'}}
  foreach($required in @('URW_Launcher.exe','URWTrace.exe','URWTraceHook.dll','URWTrace.ini','URWUpdater.ps1','rus_translate/catalog.json','fin_translate/catalog.json','rus_translate/runtime_grammar.json','fin_translate/runtime_grammar.json','rus_translate/legacy_catalog.json','fin_translate/legacy_catalog.json','rus_translate/URW.FNT','fin_translate/URW.FNT')){if(-not $seen.ContainsKey($required)){throw "Required file missing: $required"}}
  foreach($e in $a.Entries){Pump;$path=Path-Safe $dest $e.FullName;[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($path))|Out-Null;[IO.Compression.ZipFileExtensions]::ExtractToFile($e,$path,$false)}
  foreach($f in $files){if((Hash (Join-Path $dest $f.Name)) -cne $f.Value){throw "File checksum mismatch: $($f.Name)"}}
  return $m
 } finally {$a.Dispose()}
}
function Restore-Transaction($base,$journal) {
 $backup=Path-Safe $base 'URW_update_backup'
 foreach($f in $journal.files){
  if(-not(Allowed $f.name) -and $f.name -notin @('update_manifest.json','urw.exe','urw-original.exe','URWSteam.state')){throw 'Invalid recovery path'}
  $dest=Path-Safe $base $f.name
  if($f.existed){$src=Path-Safe $backup $f.name;if((Hash $src) -cne $f.sha256){throw 'Backup is incomplete'};Atomic-Copy $src $dest}
  elseif(Test-Path -LiteralPath $dest){[IO.File]::Delete($dest)}
 }
 [IO.File]::Delete((Join-Path $base 'URW_update_pending.json'))
}
function Install-Package($base,$stage,$manifest) {
 $base=[IO.Path]::GetFullPath($base).TrimEnd('\');$lock=Lock-Update $base
 try {
  $pending=Path-Safe $base 'URW_update_pending.json'
  if(Test-Path -LiteralPath $pending){throw 'Previous update needs recovery'}
  $stateFile=Path-Safe $base 'URWSteam.state';$managed=Test-Path -LiteralPath $stateFile
  $live=Path-Safe $base 'urw.exe';$original=Path-Safe $base 'urw-original.exe'
  if($managed){
   $state=@([IO.File]::ReadAllLines($stateFile));if($state.Count -ne 4 -or $state[0] -cne 'URWSteam1' -or $state[1] -cnotmatch '^[a-f0-9]{64}$' -or $state[3] -cnotmatch '^[a-f0-9]{64}$'){throw 'Steam installation record is invalid'}
   $oldLive=Hash $live;$own=$oldLive -ceq $state[1] -or $oldLive -ceq $state[2]
   if($own){if((Hash $original) -cne $manifest.game_sha256){throw 'Game version is not supported by this update'}}
   else {if($oldLive -cne $manifest.game_sha256){throw 'Updated game version is not supported by this package'};if((Hash $original) -cne $state[3]){throw 'Original backup does not match installation record'}}
  } elseif((Hash $live) -cne $manifest.game_sha256){throw 'Game version is not supported by this update'}
  $names=@($manifest.files.PSObject.Properties.Name)+@('update_manifest.json')
  # Existing settings always win. For Lite remove only our known editor files.
  if(Test-Path -LiteralPath (Join-Path $base 'URWTrace.ini')){$names=@($names|Where-Object {$_ -cne 'URWTrace.ini'})}
  $remove=@()
  if($manifest.edition -eq 'lite'){
   foreach($lang in @('rus_translate','fin_translate')){foreach($n in @('catalog.html','translations.csv','grammar.json','provenance.json.gz','generated_files.json','README.txt','FONT_NOTES.txt')){$remove+=($lang+'/'+$n)}}
  }
  $names=@($names+$remove|Select-Object -Unique)
  if($managed){$names+=@('urw.exe','URWSteam.state');if(-not $own){$names+='urw-original.exe'}}
  foreach($n in $names){$dest=Path-Safe $base $n;if(Test-Path -LiteralPath $dest){$f=[IO.File]::Open($dest,[IO.FileMode]::Open,[IO.FileAccess]::ReadWrite,[IO.FileShare]::None);$f.Dispose()}}
  $backup=Path-Safe $base 'URW_update_backup'
  if(Test-Path -LiteralPath $backup){
   $marker=Join-Path $backup 'backup_manifest.json';if(-not(Test-Path -LiteralPath $marker) -or (Get-Content -Raw -LiteralPath $marker|ConvertFrom-Json).kind -cne 'URWUpdateBackup1'){throw 'Backup directory is not owned by updater'}
   Remove-Item -LiteralPath $backup -Recurse -Force
  }
  [IO.Directory]::CreateDirectory($backup)|Out-Null;[IO.File]::WriteAllText((Join-Path $backup 'backup_manifest.json'),'{"kind":"URWUpdateBackup1","phase":"preparing"}');$records=@()
  foreach($n in $names){
   Pump
   $dest=Path-Safe $base $n;$exists=Test-Path -LiteralPath $dest;$hash=''
   if($exists){$target=Path-Safe $backup $n;[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target))|Out-Null;[IO.File]::Copy($dest,$target,$false);$hash=Hash $target;if($hash -cne (Hash $dest)){throw 'Backup checksum mismatch'}}
   $records+=@{name=$n;existed=$exists;sha256=$hash}
  }
  $journal=@{kind='URWUpdateBackup1';version=$manifest.version;files=$records};$json=$journal|ConvertTo-Json -Depth 6
  [IO.File]::WriteAllText((Join-Path $backup 'backup_manifest.json'),$json);[IO.File]::WriteAllText($pending,$json)
  try {
   $copied=0
   if($managed -and -not $own){[IO.File]::Copy($live,(Join-Path $stage 'new-original.exe'),$false)}
   foreach($n in $names){
    Pump
    if($n -in @('urw.exe','urw-original.exe','URWSteam.state')){continue}
    $dest=Path-Safe $base $n
    if($n -in $remove){if(Test-Path -LiteralPath $dest){[IO.File]::Delete($dest)}}else{Atomic-Copy (Join-Path $stage $n) $dest}
    $copied++;if($script:FailAfterCopy -gt 0 -and $copied -eq $script:FailAfterCopy){throw 'Injected test failure'}
   }
   if($managed){
    if(-not $own){Atomic-Copy (Join-Path $stage 'new-original.exe') $original}
    if((Hash $original) -cne $manifest.game_sha256){throw 'Original game changed during update'}
    $master=Join-Path $base 'URWTrace.exe';$hash=Hash $master
    # Same durable intent as SteamInstall: old and new loaders remain identifiable.
    $previous=if($own){$oldLive}else{'-'};$next="URWSteam1`n$hash`n$previous`n$($manifest.game_sha256)`n";$temp=Join-Path $stage 'next.state';[IO.File]::WriteAllText($temp,$next)
    Atomic-Copy $temp $stateFile;Atomic-Copy $master $live
   }
   [IO.File]::Delete($pending)
  } catch {Restore-Transaction $base $journal;throw}
 } finally {$lock.Dispose()}
}
function Recover-Pending($base) {
 $p=Path-Safe $base 'URW_update_pending.json';if(-not(Test-Path -LiteralPath $p)){return}
 $lock=Lock-Update $base
 try{$j=Get-Content -Raw -LiteralPath $p|ConvertFrom-Json;if($j.kind -cne 'URWUpdateBackup1'){throw 'Unknown recovery record'};Restore-Transaction $base $j}finally{$lock.Dispose()}
}
function Download($url,$path) {
 if($url -cnotmatch '^https://github\.com/Librianets/UnRealWorld_MultiTranslate/releases/download/hook-[0-9]+\.[0-9]+/(Hook_[0-9]+\.[0-9]+(_lite)?\.zip|SHA256SUMS\.txt)$'){throw 'Unexpected release URL'}
 $client=[Net.WebClient]::new();$client.Headers['User-Agent']='URW-Translation-Updater'
 try{$task=$client.DownloadFileTaskAsync([uri]$url,$path);$watch=[Diagnostics.Stopwatch]::StartNew()
  while(-not $task.IsCompleted){if($watch.Elapsed.TotalSeconds -gt 180){$client.CancelAsync();throw 'Download timed out'};Pump;Start-Sleep -Milliseconds 100}
  $task.GetAwaiter().GetResult()
 }finally{$client.Dispose()}
}
function Close-ParentLauncher($base,$pidValue) {
 if($pidValue -le 0){return $false}
 $parent=Get-Process -Id $pidValue -ErrorAction SilentlyContinue
 if(-not $parent){return $false}
 if($parent.Path -ine (Join-Path $base 'URW_Launcher.exe')){throw 'Launcher process changed'}
 $parent.CloseMainWindow()|Out-Null
 if(-not $parent.WaitForExit(10000)){throw 'Close the launcher before updating'}
 return $true
}
function Latest-Release {
 $client=[Net.WebClient]::new();$client.Headers['User-Agent']='URW-Translation-Updater';$client.Headers['Accept']='application/vnd.github+json'
 try{$task=$client.DownloadStringTaskAsync([uri]'https://api.github.com/repos/Librianets/UnRealWorld_MultiTranslate/releases/latest');$watch=[Diagnostics.Stopwatch]::StartNew()
  while(-not $task.IsCompleted){if($watch.Elapsed.TotalSeconds -gt 30){$client.CancelAsync();throw 'GitHub check timed out'};Pump;Start-Sleep -Milliseconds 100}
  return ($task.GetAwaiter().GetResult()|ConvertFrom-Json)
 }finally{$client.Dispose()}
}
function Main {
 Add-Type -AssemblyName System.Windows.Forms
 $Root=[IO.Path]::GetFullPath($Root).TrimEnd('\');$null=Path-Safe $Root 'URW_Launcher.exe'
 $temp=Join-Path ([IO.Path]::GetTempPath()) ('URWUpdate_'+[Guid]::NewGuid().ToString('N'));[IO.Directory]::CreateDirectory($temp)|Out-Null
 $form=[Windows.Forms.Form]::new();$form.Text='UnReal World';$form.Width=510;$form.Height=145;$form.ControlBox=$false;$form.StartPosition='CenterScreen'
 $label=[Windows.Forms.Label]::new();$label.Left=15;$label.Top=15;$label.Width=470;$label.Height=45
 $bar=[Windows.Forms.ProgressBar]::new();$bar.Left=15;$bar.Top=65;$bar.Width=465;$bar.Style='Marquee';$form.Controls.Add($label);$form.Controls.Add($bar)
 $script:ProgressForm=$form
 $closedParent=$false
 try {
  Check-Idle $Root
  if(Test-Path -LiteralPath (Join-Path $Root 'URW_update_pending.json')){$closedParent=Close-ParentLauncher $Root $ParentId}
  Recover-Pending $Root
  $label.Text=Text 'Checking GitHub Releases…' 'Проверка обновлений на GitHub…' 'Tarkistetaan GitHub-päivityksiä…';$form.Show();[Windows.Forms.Application]::DoEvents()
  [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
  $release=Latest-Release
  if($release.draft -or $release.prerelease -or $release.tag_name -cnotmatch '^hook-([0-9]+\.[0-9]+)$'){throw 'No supported stable release'}
  $version=$Matches[1];$currentEdition=if(Test-Path -LiteralPath (Join-Path $Root 'rus_translate/catalog.html')){'standard'}else{'lite'}
  if([version]$version -lt [version]$CurrentVersion -or ([version]$version -eq [version]$CurrentVersion -and $Edition -eq $currentEdition)){
   $form.Hide();[Windows.Forms.MessageBox]::Show((Text 'This edition is up to date.' 'Эта редакция уже обновлена.' 'Tämä versio on ajan tasalla.'),'UnReal World')|Out-Null;return
  }
  $name='Hook_'+$version+$(if($Edition -eq 'lite'){'_lite'})+'.zip';$assets=@($release.assets|Where-Object name -CEQ $name);$sums=@($release.assets|Where-Object name -CEQ 'SHA256SUMS.txt')
  if($assets.Count -ne 1 -or $sums.Count -ne 1){throw 'Required release assets are missing'}
  $form.Hide()
  $editionLabel=if($Edition -eq 'lite'){'Lite'}else{Text 'Standard' 'Обычная' 'Tavallinen'}
  $question=Text "Install $version ($editionLabel)? Settings will be kept. Replaced translations and editor files are saved in URW_update_backup (one previous update)." "Установить $version ($editionLabel)? Настройки сохранятся. Заменяемые переводы и файлы редактора будут сохранены в URW_update_backup (одно предыдущее обновление)." "Asennetaanko $version ($editionLabel)? Asetukset säilyvät. Korvattavat käännökset ja editoritiedostot tallennetaan kansioon URW_update_backup (yksi edellinen päivitys)."
  if([Windows.Forms.MessageBox]::Show($question,'UnReal World','YesNo','Question') -ne 'Yes'){return}
  $form.Show();$label.Text=Text 'Downloading and verifying the package…' 'Загрузка и проверка комплекта…' 'Ladataan ja tarkistetaan pakettia…';[Windows.Forms.Application]::DoEvents()
  $sumFile=Join-Path $temp 'SHA256SUMS.txt';Download $sums[0].browser_download_url $sumFile
  $lines=@(Get-Content -LiteralPath $sumFile|Where-Object {$_ -cmatch ('^[a-f0-9]{64}  '+[regex]::Escape($name)+'$')});if($lines.Count -ne 1){throw 'Archive checksum not found'}
  $zip=Join-Path $temp $name;Download $assets[0].browser_download_url $zip
  $stage=Join-Path $temp 'package';[IO.Directory]::CreateDirectory($stage)|Out-Null
  $manifest=Read-Package $zip ($lines[0].Substring(0,64)) $version $Edition $stage
  Check-Idle $Root
  if(-not $closedParent){$closedParent=Close-ParentLauncher $Root $ParentId}
  $label.Text=Text 'Installing…' 'Установка…' 'Asennetaan…';[Windows.Forms.Application]::DoEvents()
  Install-Package $Root $stage $manifest
  $form.Hide();[Windows.Forms.MessageBox]::Show((Text 'Updated. Steam loader and selected edition are ready.' 'Обновлено. Загрузчик Steam и выбранная редакция готовы.' 'Päivitetty. Steam-käynnistin ja valittu paketti ovat valmiita.'),'UnReal World')|Out-Null
 } catch {$form.Hide();[Windows.Forms.MessageBox]::Show($_.Exception.Message,'UnReal World','OK','Error')|Out-Null}
 finally {$script:ProgressForm=$null;$form.Dispose();Remove-Item -LiteralPath $temp -Recurse -Force -ErrorAction SilentlyContinue;if($closedParent){Start-Process -FilePath (Join-Path $Root 'URW_Launcher.exe') -WorkingDirectory $Root}}
}
if($MyInvocation.InvocationName -ne '.') { Main }
