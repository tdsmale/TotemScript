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
#define TOTEMSCRIPTCMD "TotemScriptCmd.exe"
#else
#define TOTEMSCRIPTCMD "time ./TotemScriptCmd"
#endif

int main(int argc, const char * argv[])
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
                totem_snprintf(buffer, TOTEM_ARRAY_SIZE(buffer), TOTEMSCRIPTCMD " -f %s/%s", dir, f->d_name);
                fprintf(stdout, "\n\n##########\nNext test: %s\n\n", buffer);
                printf("Press the return key to run\n\n");
                getchar();
                system(buffer);
            }
        }
    }
    
    closedir(d);
    
    printf("All tests run - press the return key to exit\n\n");
    getchar();
    
    return 0;
}