// Temporary standalone entry point for macOS/Linux.
// Currently only exercises a tiny portion of the code to validate build.

#include "../platform_shim.h"
#include <cstdio>

int main()
{
    printf("AVS standalone scaffold running. (Rendering loop not yet implemented)\n");
    // Future: initialize audio capture, preset loading, render loop.
    return 0;
}
