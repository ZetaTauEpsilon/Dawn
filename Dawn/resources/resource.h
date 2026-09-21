#pragma once

/** The first module-local RCDATA identifier owns the bundled default settings document. */
#define IDR_DEFAULT_SETTINGS 101
/** The next module-local RCDATA identifier embeds the required Dear ImGui MIT notice. */
#define IDR_IMGUI_LICENSE 102
/** The next module-local RCDATA identifier embeds the required Microsoft Detours notice. */
#define IDR_DETOURS_LICENSE 103
/** The next module-local RCDATA identifier holds the supplied Dawn logo, as a PNG. */
#define IDR_LOGO_SHEET 104

/** The four numeric fields of the version resource, in FILEVERSION order. */
#define DAWN_VER_MAJOR 1
#define DAWN_VER_MINOR 7
#define DAWN_VER_PATCH 1
#define DAWN_VER_BUILD 0
/** The same version as display text. Windows shows this string, not the four fields. */
#define DAWN_VER_STRING "1.7.1.0"
#define DAWN_DISPLAY_VERSION "v1.7.1"
