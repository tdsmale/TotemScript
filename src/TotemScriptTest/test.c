//
//  test
//  TotemScriptTest
//
//  Created by Timothy Smale on 20/07/2016.
//  Copyright (c) 2016 Timothy Smale. All rights reserved.
//

#include <stdio.h>
#include <string.h>
#include <TotemScript/totem.h>

#ifdef TOTEM_WIN
#include <TotemScriptTest/dirent.h>
#define TOTEMSCRIPTCMD ".\\TotemScriptCmd.exe"
#else
#include <dirent.h>
#define TOTEMSCRIPTCMD "time ./TotemScriptCmd"
#endif

int main(void)
{
    char buffer[PATH_MAX];
    const char *dir = "./tests";
    
    DIR *d = opendir(dir);
    if (d)
    {
        for (struct dirent *f = readdir(d); f; f = readdir(d))
        {
            if (strstr(f->d_name, ".totem"))
            {
                totem_snprintf(buffer, TOTEM_ARRAY_SIZE(buffer), TOTEMSCRIPTCMD " -a -f %s/%s", dir, f->d_name);
                fprintf(stdout, "##########\nNext test: %s\n\n", buffer);
                printf("\n\nPress the return key to run\n");
                getchar();
                int returnVal = system(buffer);
                if(returnVal != EXIT_SUCCESS)
                {
                    printf("\n\n%s: Test Failed %i\n", f->d_name, returnVal);
                    printf("\n\nPress the return key to continue\n");
                    getchar();
                }
                else
                {
                    printf("\n\n%s: Test Succeeded\n", f->d_name);
                }
            }
        }
    }
    
    closedir(d);
    
    printf("\nAll tests run - press the return key to exit\n");
    getchar();
    
    return 0;
}
