$candidateMap = Get-Content build/artwork_candidates.json -Raw | ConvertFrom-Json
$candidateTitles = ($candidateMap.PSObject.Properties | ForEach-Object { 'File:' + $_.Value }) -join '|'
$metadataUrl = 'https://commons.wikimedia.org/w/api.php?action=query&format=json&prop=imageinfo&iiprop=url%7Cextmetadata&iiurlwidth=600&redirects=1&titles=' + [Uri]::EscapeDataString($candidateTitles)
$artMetadata = Invoke-RestMethod -Uri $metadataUrl -Headers @{'User-Agent'='WoloDrive-Artwork/1.0 (https://github.com/ogrody420/wolodrive)'}
$artMetadata | ConvertTo-Json -Depth 30 | Set-Content build/commons-artwork.json -Encoding utf8
$artMetadata.query.pages.PSObject.Properties.Value | ForEach-Object {
    [pscustomobject]@{Title=$_.title; License=$_.imageinfo[0].extmetadata.LicenseShortName.value; Missing=($_.missing -ne $null)}
} | Sort-Object Missing,Title | Format-Table -AutoSize
