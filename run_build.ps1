$env:JAVA_HOME = "C:\Program Files\Android\Android Studio\jbr"
$env:PATH = "$env:JAVA_HOME\bin;$env:PATH"
Set-Location "C:\Users\theno\Projects\guitaremulatorapp"
& .\gradlew.bat assembleDebug --no-daemon 2>&1 | Out-File -FilePath "C:\Users\theno\Projects\guitaremulatorapp\build_output.txt" -Encoding utf8
