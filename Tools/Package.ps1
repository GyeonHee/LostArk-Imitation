# LoA 패키징 — Development 빌드 + 평가용 문서 복사 + 테스트 기록 제거
#   전체:        powershell -ExecutionPolicy Bypass -File Tools\Package.ps1
#   압축 직전:   powershell -ExecutionPolicy Bypass -File Tools\Package.ps1 -PrepareOnly
#                (패키징 없이 문서만 다시 복사하고 패키지 안의 저장·로그를 지움)
# Development 빌드는 저장 파일이 패키지 폴더 안(LoA\Saved)에 생겨서, 테스트 후 그대로 압축하면 내 기록이 같이 나간다.
param([switch]$PrepareOnly)

$ErrorActionPreference = 'Stop'

$Root     = Split-Path -Parent $PSScriptRoot
$Project  = Join-Path $Root 'LoA.uproject'
$Engine   = 'C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles'
$Archive  = Join-Path $Root 'Saved\Packaged'
$Package  = Join-Path $Archive 'Windows'

if (-not $PrepareOnly) {
    if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) {
        throw '에디터가 켜져 있습니다. 저장하고 닫은 뒤 다시 실행하세요.'
    }

    # 쿡은 에디터 모듈로 돌아가므로 에디터 대상부터 최신으로 맞춘다(Live Coding 패치는 쿡에 반영되지 않음)
    & (Join-Path $Engine 'Build.bat') LoAEditor Win64 Development $Project -NoUBTMakefiles
    if ($LASTEXITCODE -ne 0) { throw "에디터 빌드 실패 (exit $LASTEXITCODE)" }

    # 맵은 DefaultGame.ini의 +MapsToCook을 따른다
    & (Join-Path $Engine 'RunUAT.bat') BuildCookRun "-project=$Project" -noP4 -platform=Win64 `
        -clientconfig=Development -build -cook -stage -pak -iostore -compressed -prereqs `
        -archive "-archivedirectory=$Archive" -unattended -utf8output
    if ($LASTEXITCODE -ne 0) { throw "패키징 실패 (exit $LASTEXITCODE)" }
}

if (-not (Test-Path (Join-Path $Package 'LoA.exe'))) {
    throw "패키지를 찾지 못했습니다: $Package"
}

Copy-Item (Join-Path $Root 'Docs\패키지_README.txt') (Join-Path $Package 'README.txt') -Force
Copy-Item (Join-Path $Root 'Docs\테스트_명령어.txt') $Package -Force

# 테스트 때 생긴 저장·로그·설정을 지워 받는 사람이 초기 상태로 시작하게 한다
$Saved = Join-Path $Package 'LoA\Saved'
if (Test-Path $Saved) { Remove-Item $Saved -Recurse -Force }

$SizeGB = [math]::Round(((Get-ChildItem $Package -Recurse -File | Measure-Object Length -Sum).Sum) / 1GB, 2)
Write-Host "완료: $Package ($SizeGB GB)"
