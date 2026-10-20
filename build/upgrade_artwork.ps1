$ErrorActionPreference='Stop'
$records=Get-Content assets/library/SOURCES.json -Raw|ConvertFrom-Json
$meta=Get-Content build/replacement-metadata.json -Raw|ConvertFrom-Json
$replacementNames=@{
 'sony-ps3'='Sony-PlayStation-3-CECHA01-wController-L.png'
 'microsoft-xboxone'='Microsoft-Xbox-One-Console-wKinect.png'
 'sega-megadrive'='Sega-Genesis-Mk2-6button.png'
 'sega-cd'='Sega-CD-Model2-Set.png'
 'atari-5200'='Atari-5200-Console-Set.png'
 'snk-pocket'='Neo-Geo-Pocket-Anthra-Left.png'
 'snk-pocketcolor'='Neo-Geo-Pocket-Color-Blue-Left.png'
}
$spela=Get-Content build/spela-photo-credits.json -Raw|ConvertFrom-Json
$spelaKeys=@{'nintendo-nes'='nes';'nintendo-3ds'='3ds';'snk-cd'='neocd'}
$generated=@{
 'sega-32x'='C:/Users/Callum/.codex/generated_images/01a1529b-34da-7410-a400-26cbe22e8975/exec-d39a6311-7580-4b64-b096-2a72782f8206.png'
 'nintendo-switch2'='C:/Users/Callum/.codex/generated_images/01a1529b-34da-7410-a400-26cbe22e8975/exec-02010f8b-69dd-4641-be9f-bd0eaa2d7d28.png'
}
foreach($record in $records){
 if(!$record.key.StartsWith('consoles/')){continue}
 $key=$record.key.Substring(9)
 $newPath='assets/library/consoles/'+$key+'.png'
 if($replacementNames.ContainsKey($key)){
  $page=$meta.query.pages.PSObject.Properties.Value|Where-Object{$_.title -eq ('File:'+$replacementNames[$key])}
  $info=$page.imageinfo[0]
  Copy-Item -LiteralPath ('build/clean-'+$replacementNames[$key]) -Destination $newPath -Force
  $record.title=$page.title; $record.source=$info.descriptionurl; $record.original=$info.url.Split('?')[0]
  $record.downloaded=$info.thumburl.Split('?')[0]; $record.author=[Net.WebUtility]::HtmlDecode(($info.extmetadata.Artist.value -replace '<[^>]+>',''))
  $record.license=$info.extmetadata.LicenseShortName.value; $record.license_url=$info.extmetadata.LicenseUrl.value
  $record.changes='Replaced with a source PNG containing native transparency; thumbnail unchanged.'
 }elseif($spelaKeys.ContainsKey($key)){
  $id=$spelaKeys[$key]; $credit=$spela.photos|Where-Object{$_.console -eq $id}
  Copy-Item -LiteralPath ('build/photo-'+$id+'.png') -Destination $newPath -Force
  $record.source=$credit.source; $record.author=$credit.author; $record.license=$credit.license
  $record.downloaded='https://raw.githubusercontent.com/mattias800/spela/master/server/internal/api/static/console-photos/'+$id+'.png'
  $record.changes='Prepared transparent PNG from Spela; see upstream CREDITS.json. No runtime background removal.'
 }elseif($generated.ContainsKey($key)){
  Copy-Item -LiteralPath $generated[$key] -Destination $newPath -Force
  $record.changes='Background extracted with OpenAI imagegen from the credited source photograph; hardware retained.'
 }
 if($replacementNames.ContainsKey($key) -or $spelaKeys.ContainsKey($key) -or $generated.ContainsKey($key)){
  if($record.path -ne $newPath){if(Test-Path -LiteralPath $record.path){Remove-Item -LiteralPath $record.path}}
  $record.path=$newPath
 }
 $record.sha256=(Get-FileHash -LiteralPath $record.path -Algorithm SHA256).Hash.ToLowerInvariant()
}
$map=Get-Content build/console_logos.json -Raw|ConvertFrom-Json
$tree=Invoke-RestMethod -Uri 'https://api.github.com/repos/HVR88/Monochrome-Gaming-Logos/git/trees/main?recursive=1'
New-Item -ItemType Directory -Path assets/library/logos -Force|Out-Null
foreach($entry in $map.PSObject.Properties){
 $path='assets/library/logos/'+$entry.Name+'.png'
 if($entry.Value -eq 'COMMONS'){
  $logoMeta=Get-Content build/switch2-logo-metadata.json -Raw|ConvertFrom-Json
  $page=$logoMeta.query.pages.PSObject.Properties.Value|Where-Object{$_.imageinfo}
  $info=$page.imageinfo[0];$url=$info.thumburl.Split('?')[0]
  $source=$info.descriptionurl; $author=[Net.WebUtility]::HtmlDecode(($info.extmetadata.Artist.value -replace '<[^>]+>',''))
  $license=$info.extmetadata.LicenseShortName.value;$licenseUrl=$info.extmetadata.LicenseUrl.value
 }else{
  $repoPath='png/'+$entry.Value+'.png'
  if(!($tree.tree|Where-Object{$_.path -eq $repoPath})){throw ('Unknown console logo '+$repoPath)}
  $url='https://raw.githubusercontent.com/HVR88/Monochrome-Gaming-Logos/main/'+$repoPath
  $source='https://github.com/HVR88/Monochrome-Gaming-Logos/blob/main/'+$repoPath
  $author='HVR88 / respective brand owner';$license='Identification artwork; source usage disclaimer';$licenseUrl='https://github.com/HVR88/Monochrome-Gaming-Logos#usage-disclaimer'
 }
 Invoke-WebRequest -Uri $url -OutFile $path
 $records += [pscustomobject]@{key=('logos/'+$entry.Name);path=$path;title=$entry.Value;source=$source;original=$url;downloaded=$url;author=$author;license=$license;license_url=$licenseUrl;changes='Unmodified transparent logo, fitted proportionally in the UI.';sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()}
}
$records|ConvertTo-Json -Depth 5|Set-Content assets/library/SOURCES.json -Encoding utf8
Write-Output ('Prepared '+$records.Count+' artwork records.')
