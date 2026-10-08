@echo off
rem VM'ni bir marta sozlaydi. Elevated cmd'dan ishga tushiring:
rem   d:\sozla.cmd
net session >nul 2>&1
if errorlevel 1 (
  echo XATO: administrator huquqi kerak.
  exit /b 1
)

echo === 1. virtio drayverlari ===
pnputil /add-driver d:\virtio\netkvm\netkvm.inf /install
pnputil /add-driver d:\virtio\vioserial\vioser.inf /install

echo.
echo === 2. Tarmoq kutilmoqda ===
timeout /t 10 /nobreak >nul
ipconfig | findstr /i "IPv4"
ping -n 3 8.8.8.8

echo.
echo === 3. Mehmon agenti (UTM execute/push/pull uchun) ===
msiexec /i d:\virtio\agent\qemu-ga-x86_64.msi /qn /norestart
sc query QEMU-GA

echo.
echo === 4. OpenSSH server ===
powershell -NoProfile -ExecutionPolicy Bypass -Command "Add-WindowsCapability -Online -Name OpenSSH.Server~~~~0.0.1.0"
sc config sshd start= auto
net start sshd

echo.
echo === 5. Kalit va qobiq ===
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
 "New-Item -ItemType Directory -Force C:\ProgramData\ssh | Out-Null;" ^
 "Set-Content -Path C:\ProgramData\ssh\administrators_authorized_keys -Value (Get-Content d:\kotib_vm_key.pub) -Encoding ascii;" ^
 "icacls C:\ProgramData\ssh\administrators_authorized_keys /inheritance:r /grant 'Administrators:F' /grant 'SYSTEM:F' | Out-Null;" ^
 "New-ItemProperty -Path 'HKLM:\SOFTWARE\OpenSSH' -Name DefaultShell -Value 'C:\Windows\System32\cmd.exe' -PropertyType String -Force | Out-Null"
netsh advfirewall firewall add rule name=sshd dir=in action=allow protocol=TCP localport=22

echo.
echo === Sozlash tugadi ===
sc query sshd | findstr STATE
