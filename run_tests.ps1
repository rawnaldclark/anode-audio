$env:JAVA_HOME = "C:\Program Files\Android\Android Studio\jbr"
$env:PATH = "$env:JAVA_HOME\bin;$env:PATH"
Set-Location "C:\Users\theno\Projects\guitaremulatorapp"
& .\gradlew.bat testDebugUnitTest --no-daemon 2>&1 | Out-File -FilePath "C:\Users\theno\Projects\guitaremulatorapp\test_output.txt" -Encoding utf8
