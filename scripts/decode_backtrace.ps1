param(
  [Parameter(Mandatory = $true, Position = 0)]
  [string]$Backtrace,

  [Parameter(Position = 1)]
  [string]$ElfPath = ".pio/build/esp32s3mini_ota/firmware.elf",

  [Parameter(Position = 2)]
  [string]$ToolPath = "$env:USERPROFILE\.platformio\packages\toolchain-xtensa-esp-elf\bin\xtensa-esp32s3-elf-addr2line.exe"
)

$addresses = [regex]::Matches($Backtrace, '0x[0-9A-Fa-f]+') | ForEach-Object { $_.Value }

if (-not $addresses -or $addresses.Count -eq 0) {
  throw "Geen adressen gevonden. Plak een backtrace zoals: 0x4037EF71 -> 0x40377C96 -> ..."
}

if (-not (Test-Path $ElfPath)) {
  throw "ELF niet gevonden: $ElfPath"
}

if (-not (Test-Path $ToolPath)) {
  throw "addr2line tool niet gevonden: $ToolPath"
}

Write-Host "Decoding $($addresses.Count) address(es) against $ElfPath" -ForegroundColor Cyan
& $ToolPath -pfiaC -e $ElfPath @addresses