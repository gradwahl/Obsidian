$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Net.Http
$candidateMap = Get-Content build/artwork_candidates.json -Raw | ConvertFrom-Json
$metadata = Get-Content build/commons-artwork.json -Raw | ConvertFrom-Json
$pages = @($metadata.query.pages.PSObject.Properties.Value)
$client = New-Object System.Net.Http.HttpClient
$client.DefaultRequestHeaders.UserAgent.ParseAdd('WoloDrive-Artwork/1.0 (https://github.com/ogrody420/wolodrive)')
$records = @()
$pending = @()
foreach ($entry in $candidateMap.PSObject.Properties) {
    $page = $pages | Where-Object { $_.title -eq ('File:' + $entry.Value) }
    if (!$page.imageinfo) { throw ('Missing Commons file: ' + $entry.Value) }
    $info = $page.imageinfo[0]
    $extension = [IO.Path]::GetExtension($page.title).ToLowerInvariant()
    if ($extension -eq '.svg') { $extension = '.png' }
    $path = 'assets/library/' + $entry.Name + $extension
    $directory = Split-Path $path
    New-Item -ItemType Directory -Path $directory -Force | Out-Null
    $url = $info.thumburl.Split('?')[0]
    $author = [Net.WebUtility]::HtmlDecode(($info.extmetadata.Artist.value -replace '<[^>]+>', ''))
    $record = [ordered]@{
        key=$entry.Name; path=$path; title=$page.title; source=$info.descriptionurl
        original=$info.url.Split('?')[0]; downloaded=$url; author=$author
        license=$info.extmetadata.LicenseShortName.value; license_url=$info.extmetadata.LicenseUrl.value
        changes='Wikimedia-generated thumbnail; no other changes.'
    }
    $records += [pscustomobject]$record
    if (!(Test-Path -LiteralPath $path)) {
        $pending += [pscustomobject]@{Path=$path; Task=$client.GetByteArrayAsync($url)}
    }
}
foreach ($download in $pending) {
    $bytes = $download.Task.GetAwaiter().GetResult()
    [IO.File]::WriteAllBytes((Join-Path $PWD $download.Path), $bytes)
    Write-Output ('Saved ' + $download.Path + ' (' + $bytes.Length + ' bytes)')
}
foreach ($record in $records) {
    $record | Add-Member -NotePropertyName sha256 -NotePropertyValue (Get-FileHash -LiteralPath $record.path -Algorithm SHA256).Hash.ToLowerInvariant()
}
$records | ConvertTo-Json -Depth 5 | Set-Content assets/library/SOURCES.json -Encoding utf8
$credits = @('# Library artwork', '', 'Bundled company logos and console hardware photos from Wikimedia Commons.', 'Each source page records authorship and licensing. Brand marks identify their respective owners.', 'Images are Wikimedia-generated thumbnails, with no other changes. Asset licenses apply to the images, independently of application code.', '', '| Asset | Author | License |', '| --- | --- | --- |')
foreach ($record in $records) {
    $author = $record.author -replace '\|','/' -replace '\r?\n',' '
    $license = $record.license
    if ($record.license_url) { $license = '[' + $license + '](' + $record.license_url + ')' }
    $credits += '| [' + $record.path.Replace('assets/library/', '') + '](' + $record.source + ') | ' + $author + ' | ' + $license + ' |'
}
$credits += @('', 'Exact download URLs and SHA-256 hashes are recorded in SOURCES.json.')
$credits | Set-Content assets/library/CREDITS.md -Encoding utf8
$client.Dispose()
