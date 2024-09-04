#pragma once

#ifdef NTDDI_VERSION
#undef NTDDI_VERSION
#endif
#define NTDDI_VERSION   0x0A000000

#ifdef WINVER
#undef WINVER
#endif
#define WINVER          0x0A00

#ifdef _WIN32_WINNT
#undef _WIN32_WINNT
#endif
#define _WIN32_WINNT    0x0A00
