$bitness = $env:buildArch -eq 'X86' ? 32 : 64
New-Item -Path "dist\win\qView-Win$bitness" -ItemType Directory -ea 0
copy -R bin\* "dist\win\qView-Win$bitness"
iscc dist\win\qView$bitness.iss
copy dist\win\Output\* bin\