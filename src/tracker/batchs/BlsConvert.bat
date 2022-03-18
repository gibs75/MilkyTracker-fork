cd "%~d0%~p0\"
%~d0
BlsConvert.exe "%1" > ~temp.txt
"C:\Program Files (x86)\Notepad++\notepad++.exe" ~temp.txt
