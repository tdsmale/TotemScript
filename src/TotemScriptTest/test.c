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
#define TOTEMSCRIPTCMD "TotemScriptCmd.exe"
#else
#define TOTEMSCRIPTCMD "time ./TotemScriptCmd"
#endif

#define TOTEMSCRIPTCMD_RUN(x) TOTEMSCRIPTCMD " " x

#define TEST(x) \
    printf("Next test: "TOTEMSCRIPTCMD_RUN(x)"\n"); \
    printf("Press the return key to run"); \
    getchar(); \
    system(TOTEMSCRIPTCMD_RUN(x));

int main(int argc, const char * argv[])
{
    TEST("");
    
    TEST("--version");
    TEST("-v");
    
    TEST("--help");
    TEST("-h");
    
    TEST("--file test.totem");
    TEST("-f test.totem");
    
    TEST("--string \"print(1 + 2);\"");
    TEST("-s \"print(1 + 2);\"");
    
    TEST("--file --dump test.totem");
    TEST("-f -d test.totem");
    
    TEST("-f benchmark_nbody.totem 50000000");
    TEST("-f benchmark_mandelbrot.totem 16000");
    
    return 0;
}