cl.exe /c /EHsc /I C:\glew-2.1.0\include /I "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.4\include" source.cpp
link.exe source.obj user32.lib gdi32.lib /LIBPATH:"C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.4\lib\x64" /LIBPATH:"C:\glew-2.1.0\lib\Release\x64" /SUBSYSTEM:WINDOWS

