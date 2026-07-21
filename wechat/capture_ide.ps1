Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

# Find IDE window
Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public class Win32 {
    [DllImport("user32.dll")]
    public static extern bool GetWindowRect(IntPtr hwnd, out RECT lpRect);
    [DllImport("user32.dll")]
    public static extern bool SetForegroundWindow(IntPtr hwnd);
    [DllImport("user32.dll")]
    public static extern bool ShowWindow(IntPtr hwnd, int nCmdShow);
    [DllImport("user32.dll")]
    public static extern IntPtr FindWindow(string lpClassName, string lpWindowName);
    [StructLayout(LayoutKind.Sequential)]
    public struct RECT {
        public int Left, Top, Right, Bottom;
    }
}
"@

# Find IDE window
$ideHwnd = [IntPtr]::Zero
Add-Type -AssemblyName UIAutomationClient
$procs = Get-Process wechatdevtools -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowTitle -ne "" }
foreach ($p in $procs) {
    if ($p.MainWindowTitle -match "2048") {
        $ideHwnd = $p.MainWindowHandle
        Write-Host "Found IDE window: PID=$($p.Id) Title=$($p.MainWindowTitle) Handle=$ideHwnd"
        break
    }
}

if ($ideHwnd -eq [IntPtr]::Zero) {
    Write-Host "No IDE window found"
    exit 1
}

# Get window rect
$rect = New-Object Win32+RECT
[Win32]::GetWindowRect($ideHwnd, [ref]$rect) | Out-Null
Write-Host "Window rect: L=$($rect.Left) T=$($rect.Top) R=$($rect.Right) B=$($rect.Bottom)"

# Bring to foreground
[Win32]::ShowWindow($ideHwnd, 9) | Out-Null  # SW_RESTORE
Start-Sleep -Milliseconds 500
[Win32]::SetForegroundWindow($ideHwnd) | Out-Null
Start-Sleep -Milliseconds 500

# Capture window
$width = $rect.Right - $rect.Left
$height = $rect.Bottom - $rect.Top
if ($width -le 0 -or $height -le 0) {
    Write-Host "Invalid window size: ${width}x${height}"
    exit 1
}

$bmp = New-Object System.Drawing.Bitmap $width, $height
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($rect.Left, $rect.Top, 0, 0, $bmp.Size)
$out = "C:\Users\Administrator\AppData\Roaming\TRAE SOLO CN\ModularData\ai-agent\work-mode-projects\6a47e7225801ac16b95705a6\godot4_7_mono\wechat\ide_state_now.png"
$bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
Write-Host "Saved: $out (${width}x${height})"
