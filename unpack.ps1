$b64 = Get-Content -Path "monomachine_source_base64.txt" -Raw
$cleanB64 = $b64 -replace '\s', ''
$bytes = [System.Convert]::FromBase64String($cleanB64)
[System.IO.File]::WriteAllBytes("monomachine_full_source.zip", $bytes)
Expand-Archive -Path "monomachine_full_source.zip" -DestinationPath "." -Force
Write-Host "Successfully extracted all Monomachine source files!"
