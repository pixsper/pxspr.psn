Remove-Item "bin\pxspr.psn" -Recurse -ErrorAction Ignore
Remove-Item "bin\pxspr.psn.zip" -Recurse -ErrorAction Ignore

Copy-Item -Path "docs" -Destination "bin\pxspr.psn\docs"  -Recurse
Copy-Item -Path "externals" -Destination "bin\pxspr.psn\externals"  -Recurse
Copy-Item -Path "help" -Destination "bin\pxspr.psn\help"  -Recurse
Copy-Item -Path "misc" -Destination "bin\pxspr.psn\misc"  -Recurse
Copy-Item -Path "package-info.json" -Destination "bin\pxspr.psn"
Copy-Item -Path "icon.png" -Destination "bin\pxspr.psn"
Copy-Item -Path "LICENSE" -Destination "bin\pxspr.psn"
Copy-Item -Path "README.md" -Destination "bin\pxspr.psn"

Compress-Archive -Path "bin\pxspr.psn" -DestinationPath "bin\pxspr.psn.zip"
Remove-Item "bin\pxspr.psn" -Recurse -ErrorAction Ignore