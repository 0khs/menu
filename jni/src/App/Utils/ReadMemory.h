#include <sys/mman.h>
#include "SysCall.h"
#define PI 3.14159265358979323846f
#define RAD2DEG(x) ((float)(x) * (float)(180.f / PI))
#define DEG2RAD(x) ((float)(x) * (float)(PI / 180.f))
uintptr_t GameBase;
uintptr_t GetGameModule(const char* module_name )
{
    FILE *fp;
    uintptr_t addr = 0;
    char *pch;
    char filename[32];
    char line[1024];
    snprintf(filename, sizeof(filename), "/proc/%d/maps", getpid());
    fp = fopen(filename, "r");
    if (fp != NULL)
    {
        while (fgets(line, sizeof(line), fp))
        {
            if (strstr(line, module_name))
            {
                pch = strtok(line, "-");
                addr = strtoul(pch, NULL, 16);
                if (addr == 0x8000)
                {
                    addr = 0;
                }
                break;
            }
        }
        fclose(fp);
    }
    return addr;
}