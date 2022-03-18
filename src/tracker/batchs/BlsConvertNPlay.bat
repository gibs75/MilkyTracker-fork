cd "%~d0%~p0"
%~d0
Blsconvert.exe "%1" > ~temp.txt
Blsplay.exe "%~d1%~p1%~n1.BLS"
