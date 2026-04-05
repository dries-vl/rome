#!/usr/bin/env sh
: '
@echo off
setlocal EnableDelayedExpansion

for %%F in (data\*.png) do (
    set "name=%%~nF"
    set "suffix=!name:~-5!"

    if /I "!suffix!"=="_data" (
        set "out=static\win32\%%~nF.obj"
        if not exist "!out!" (
            call :build_raw "%%F" "%%~nF"
        ) else (
            rem rebuild policy here
            call :build_raw "%%F" "%%~nF"
        )
    ) else (
        set "out=static\win32\%%~nF.obj"
        if not exist "!out!" (
            call :build_ktx "%%F" "%%~nF"
        ) else (
            rem rebuild policy here
            call :build_ktx "%%F" "%%~nF"
        )
    )
)
goto :eof

:build_bc
set "fmt=BC1_UNORM"
if /I "%~2"=="font_atlas" set "fmt=BC3_UNORM"
if /I "%~2"=="height" set "fmt=BC4_UNORM"
if /I "%~2"=="noise" set "fmt=BC4_UNORM"
if /I "%~2"=="height_detail" set "fmt=BC4_UNORM"
if /I "%~2"=="normals" set "fmt=BC5_UNORM"

echo Building compressed %~1
texconv -y -m 0 -f %fmt% -o static\win32 "%~1"
if errorlevel 1 exit /b 1

objcopy -I binary -O elf64-x86-64 ^
    --rename-section .data=.rdata,alloc,load,readonly,data,contents ^
    "static/win32/%~2.dds" "static/win32/%~2.obj" ^
    --redefine-sym _binary_static_win32_%~2_dds_start=%~2 ^
    --redefine-sym _binary_static_win32_%~2_dds_end=%~2_end
if errorlevel 1 exit /b 1

del "static\win32\%~2.dds"
exit /b 0

:build_raw
echo Building raw %~1
magick "%~1" -channel R -separate -depth 8 "gray:static/win32/%~2.raw"
if errorlevel 1 exit /b 1

objcopy -I binary -O elf64-x86-64 ^
    --rename-section .data=.rdata,alloc,load,readonly,data,contents ^
    "static/win32/%~2.raw" "static/win32/%~2.obj" ^
    --redefine-sym _binary_static_win32_%~2_raw_start=%~2 ^
    --redefine-sym _binary_static_win32_%~2_raw_end=%~2_end
if errorlevel 1 exit /b 1

del "static\win32\%~2.raw"
exit /b 0
'

# Linux portion
for img in data/*.png; do
    [ -e "$img" ] || continue

    base="$(basename "$img" .png)"
    out="static/linux/${base}.o"

    case "$base" in
        *_data)
            kind=raw
            ;;
        *)
            kind=bc
            ;;
    esac

    if [ ! -e "$out" ] || [ "$img" -nt "$out" ]; then
        echo "Building $img"

        if [ "$kind" = raw ]; then
            raw="static/linux/${base}.raw"

            magick "$img" -channel R -separate -depth 8 "gray:${raw}" || exit 1

            objcopy \
                -I binary -O default \
                --rename-section .data=.rodata,alloc,load,readonly,data,contents \
                "$raw" "$out" \
                --redefine-sym "_binary_static_linux_${base}_raw_start=${base}" \
                --redefine-sym "_binary_static_linux_${base}_raw_end=${base}_end" \
                || exit 1

            rm "$raw"
        else
            dds="static/linux/${base}.dds"

            case "$base" in
                font_atlas)
                    fmt=BC3
                    ;;
                height|noise)
                    fmt=BC4
                    ;;
                normals,height_detail)
                    fmt=BC5
                    ;;
                *)
                    fmt=BC1
                    ;;
            esac

            CompressonatorCLI \
                -fd "$fmt" \
                "$img" "$dds" || exit 1

            objcopy \
                -I binary -O default \
                --rename-section .data=.rodata,alloc,load,readonly,data,contents \
                "$dds" "$out" \
                --redefine-sym "_binary_static_linux_${base}_dds_start=${base}" \
                --redefine-sym "_binary_static_linux_${base}_dds_end=${base}_end" \
                || exit 1

            rm "$dds"
        fi
    else
        echo "Skipping $img (up to date)"
    fi
done
