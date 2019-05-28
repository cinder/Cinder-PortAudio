if( ! (Test-Path "$PSScriptRoot\ASIOSDK2.3.2.zip" -PathType Leaf) ) {
    Write-Host "Downloading ASIO SDK 2.3.2"
    iwr -outf "$PSScriptRoot\ASIOSDK2.3.2.zip" http://www.steinberg.net/sdk_downloads/ASIOSDK2.3.2.zip
}

if( ! (Test-Path "$PSScriptRoot\ASIOSDK" -PathType Container) ) {
    Write-Host "Unzipping to lib\ASIOSDK"
    Expand-Archive "$PSScriptRoot\ASIOSDK2.3.2.zip" "$PSScriptRoot\temp"
    Move-Item $PSScriptRoot\temp\ASIOSDK2.3.2 $PSScriptRoot\ASIOSDK
    Remove-Item $PSScriptRoot\temp
	Remove-Item $PSScriptRoot\ASIOSDK2.3.2.zip
}

$asiolistPath = "$PSScriptRoot\ASIOSDK\host\pc\asiolist.cpp"
if( !( Select-String -Pattern "#undef UNICODE" -Path $asiolistPath -Quiet ) ) {
    Write-Host "Prepending `"#undef UNICODE`" to asiolist.cpp"
    ((Get-Content $asiolistPath -Raw) -replace "#include <windows.h>", "#undef UNICODE`r`n#include <windows.h>") | Set-Content -Path $asiolistPath
}

Write-Host "ASIO SDK downloaded and setup"