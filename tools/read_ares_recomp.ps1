$source = @"
using System;
using System.Runtime.InteropServices;
using System.Text;

public class ConsoleReader {
    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool AttachConsole(uint dwProcessId);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool FreeConsole();

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern IntPtr GetStdHandle(int nStdHandle);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool GetConsoleScreenBufferInfo(IntPtr hConsoleOutput, out CONSOLE_SCREEN_BUFFER_INFO lpConsoleScreenBufferInfo);

    [DllImport("kernel32.dll", SetLastError = true)]
    public static extern bool ReadConsoleOutputCharacter(IntPtr hConsoleOutput, [Out] StringBuilder lpCharacter, uint nLength, COORD dwReadCoord, out uint lpNumberOfCharsRead);

    [StructLayout(LayoutKind.Sequential)]
    public struct COORD {
        public short X;
        public short Y;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct SMALL_RECT {
        public short Left;
        public short Top;
        public short Right;
        public short Bottom;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct CONSOLE_SCREEN_BUFFER_INFO {
        public COORD dwSize;
        public COORD dwCursorPosition;
        public ushort wAttributes;
        public SMALL_RECT srWindow;
        public COORD dwMaximumWindowSize;
    }

    public static string ReadBuffer(uint pid) {
        FreeConsole();
        if (!AttachConsole(pid)) {
            return "AttachConsole failed with error: " + Marshal.GetLastWin32Error();
        }

        IntPtr hOut = GetStdHandle(-11); // STD_OUTPUT_HANDLE
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (!GetConsoleScreenBufferInfo(hOut, out csbi)) {
            FreeConsole();
            return "GetConsoleScreenBufferInfo failed with error: " + Marshal.GetLastWin32Error();
        }

        int totalChars = csbi.dwSize.X * (csbi.dwCursorPosition.Y + 1);
        if (totalChars <= 0) totalChars = 80 * 25;
        var sb = new StringBuilder(totalChars);
        uint read = 0;
        ReadConsoleOutputCharacter(hOut, sb, (uint)totalChars, new COORD { X = 0, Y = 0 }, out read);

        FreeConsole();
        System.IO.File.WriteAllText("C:\\Desenvolvimento\\Nintendo 64\\GTA264\\GTA2 N64 Tech Demo\\ares_console_recomp.txt", sb.ToString());
        return sb.ToString();
    }
}
"@

Add-Type -TypeDefinition $source

$aresPath = "C:\Desenvolvimento\Nintendo 64\Veil of the Damned 64\deps\ares\ares-v148\ares.exe"
$workingDir = "C:\Desenvolvimento\Nintendo 64\Veil of the Damned 64\deps\ares\ares-v148"
$romPath = "C:\Desenvolvimento\Nintendo 64\GTA264\GTA2 N64 Tech Demo\gta2_recomp.z64"

Get-Process ares -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 400

$proc = Start-Process -FilePath $aresPath -WorkingDirectory $workingDir -ArgumentList "--terminal", "`"$romPath`"" -PassThru
Start-Sleep -Seconds 60

$output = [ConsoleReader]::ReadBuffer($proc.Id)
Write-Host "=== CONSOLE OUTPUT FROM ARES ==="
Write-Host $output
Write-Host "================================"

Stop-Process -Id $proc.Id -Force
