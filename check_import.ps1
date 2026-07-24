$WS = "C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6"
Write-Host "=== .mono import files ==="
Get-ChildItem "$WS\test_project\.godot\import" -Filter "*mono*" -ErrorAction SilentlyContinue
Write-Host "=== GodotSharp import files ==="
Get-ChildItem "$WS\test_project\.godot\import" -Filter "*GodotSharp*" -ErrorAction SilentlyContinue
Write-Host "=== CSharp import files ==="
Get-ChildItem "$WS\test_project\.godot\import" -Filter "*CSharp*" -ErrorAction SilentlyContinue
Write-Host "=== .mono dir contents ==="
Get-ChildItem "$WS\test_project\.mono" -Recurse -Filter "*.dll" -ErrorAction SilentlyContinue | Select-Object FullName, Length
