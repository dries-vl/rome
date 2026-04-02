#!/usr/bin/env sh
: '
@echo off
tcc main.c platform/win32.c static/win32/*.obj -luser32 -lgdi32 -lucrtbase -lvulkan-1 -I"%VULKAN_SDK%\Include" -L"%VULKAN_SDK%\Lib" -bench -w -run
goto :eof
'
tcc main.c platform/wayland.c static/linux/*.o -lwayland-client -lxkbcommon -lvulkan -lm -L. -run
