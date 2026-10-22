param([string]$PythonExecutable = 'python')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$testOutput = Join-Path $projectRoot 'build/HostTests'
New-Item -ItemType Directory -Path $testOutput -Force | Out-Null
$nativeCompiler = (Get-Command gcc -ErrorAction Stop).Source
Push-Location $projectRoot
try {
    $flags = @('-std=c11','-Wall','-Wextra','-Wpedantic','-Werror','-fanalyzer',
               '-I','tests/stubs','-I','Core/Inc')
    $basicTest = Join-Path $testOutput 'basic_calc_test.exe'
    & $nativeCompiler @flags Core/Src/basic_calc.c Core/Src/complex_math.c tests/basic_calc_test.c -lm -o $basicTest
    if ($LASTEXITCODE -ne 0) { throw 'Arithmetic test build failed' }
    & $basicTest
    if ($LASTEXITCODE -ne 0) { throw 'Arithmetic tests failed' }
    $complexTest = Join-Path $testOutput 'complex_calc_test.exe'
    & $nativeCompiler @flags Core/Src/basic_calc.c Core/Src/complex_math.c tests/complex_calc_test.c -lm -o $complexTest
    if ($LASTEXITCODE -ne 0) { throw 'Complex test build failed' }
    & $complexTest
    if ($LASTEXITCODE -ne 0) { throw 'Complex tests failed' }
    $appTest = Join-Path $testOutput 'calculator_app_test.exe'
    $sources = @('Core/Src/basic_calc.c','Core/Src/complex_math.c','Core/Src/input_text.c','Core/Src/touch_keys.c',
                 'Core/Src/calculator_ui.c','Core/Src/usb_display.c','tests/calculator_app_test.c')
    & $nativeCompiler @flags @sources -lm -o $appTest
    if ($LASTEXITCODE -ne 0) { throw 'Application test build failed' }
    & $appTest
    if ($LASTEXITCODE -ne 0) { throw 'Application tests failed' }
    $nativeDll = Join-Path $testOutput 'basic_calc_native.dll'
    & $nativeCompiler -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -shared -I Core/Inc Core/Src/basic_calc.c Core/Src/complex_math.c -o $nativeDll
    if ($LASTEXITCODE -ne 0) { throw 'Native arithmetic library build failed' }
    $pythonRunner = Get-Command $PythonExecutable -ErrorAction Stop
    & $pythonRunner.Source tests/basic_calc_random_test.py $nativeDll
    if ($LASTEXITCODE -ne 0) { throw 'Random arithmetic tests failed' }
    & $pythonRunner.Source tests/complex_calc_random_test.py $nativeDll
    if ($LASTEXITCODE -ne 0) { throw 'Random complex tests failed' }
} finally {
    Pop-Location
}
