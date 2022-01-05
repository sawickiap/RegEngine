$compilerPath = 'c:\Program Files (x86)\Windows Kits\10\bin\10.0.20348.0\x64\fxc.exe'
&$compilerPath /T vs_5_0 /E main /O3 /Fo VS VS.hlsl
&$compilerPath /T ps_5_0 /E main /O3 /Fo PS PS.hlsl
