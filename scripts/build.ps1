$ErrorActionPreference = "Stop"

function Import-EspIdfEnvironment {
    if (Get-Command idf.py -ErrorAction SilentlyContinue) {
        return
    }

    $candidates = @()
    if ($env:IDF_PATH) {
        $candidates += $env:IDF_PATH
    }
    $candidates += Join-Path $HOME "esp\esp-idf"
    $candidates += Join-Path $HOME "esp-idf"
    $candidates += "C:\Espressif\frameworks\esp-idf"

    foreach ($path in $candidates) {
        $export = Join-Path $path "export.ps1"
        if (Test-Path $export) {
            . $export
            return
        }
    }
}

Import-EspIdfEnvironment

if (-not (Get-Command idf.py -ErrorAction SilentlyContinue)) {
    throw "未找到 idf.py。请先打开 ESP-IDF PowerShell，或设置 IDF_PATH 后重试。"
}

idf.py set-target esp32
idf.py build

