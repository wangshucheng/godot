$WS = "C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6"
Copy-Item "$WS\test_project\.mono\temp\bin\Release\CSharpTestProject.dll" "$WS\test_project\.mono\assemblies\CSharpTestProject.dll" -Force
Write-Host "After copy:"
Get-ChildItem "$WS\test_project\.mono\assemblies"
