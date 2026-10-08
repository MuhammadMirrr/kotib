@echo off
rem Windows tomonining sinov skripti. D: diskidan ishga tushiriladi.
d:
echo === 1. Sof mantiq testlari ===
kotib-testlar.exe
echo.
echo === 2. Yadro: WAV fayldan transkripsiya ===
rubai-cli.exe jfk.wav --model d:\models\ggml-rubaistt.bin
echo.
echo === 3. Media Foundation dekoderi (MP3) ===
rubai-cli.exe jfk.mp3 --model d:\models\ggml-rubaistt.bin
echo.
echo === 4. Studiya yoli: segmentlar + matn formatlash ===
rubai-cli.exe jfk.wav --segmentlar --model d:\models\ggml-rubaistt.bin
echo.
echo === 5. Tarjima (model bor boʻlsa) ===
rem Tarjima modeli %LOCALAPPDATA% dan qidiriladi, shuning uchun uni bir marta
rem koʻchirib qoʻyamiz. D: diskidan toʻgʻridan-toʻgʻri oʻqish ham ishlaydi,
rem lekin ilova aynan shu joyni qaraydi va sinov haqiqiy yoʻlni tekshirsin.
if exist d:\tarjima-model-33b\model.bin (
  if not exist "%LOCALAPPDATA%\Kotib\tarjima-model-33b\model.bin" (
    echo Tarjima modeli koʻchirilmoqda...
    xcopy /E /I /Q /Y d:\tarjima-model-33b "%LOCALAPPDATA%\Kotib\tarjima-model-33b" >nul
  )
  rubai-cli.exe --tarjima "Bugun havo juda yaxshi. Ertaga yomgʻir yogʻishi mumkin."
) else (
  echo Tarjima modeli diskda yoʻq — oʻtkazib yuborildi.
)
echo.
echo === Hammasi tugadi ===
