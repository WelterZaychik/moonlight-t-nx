// Compile-only host shim. It does not model Horizon behavior or SDK ABI.
#pragma once
#include <cstddef>
#include <cstdint>
using u32 = uint32_t;
using Result = uint32_t;
#define R_SUCCEEDED(value) ((value) == 0)
#define R_FAILED(value) ((value) != 0)
enum AppletType { AppletType_Application, AppletType_SystemApplication };
struct SwkbdConfig {};
extern "C" {
void randomGet(void*, size_t);
AppletType appletGetAppletType();
Result nifmGetCurrentIpConfigInfo(u32*,u32*,u32*,u32*,u32*);
Result swkbdCreate(SwkbdConfig*,int);
void swkbdConfigMakePresetDefault(SwkbdConfig*);
void swkbdConfigSetHeaderText(SwkbdConfig*,const char*);
void swkbdConfigSetInitialText(SwkbdConfig*,const char*);
void swkbdConfigSetStringLenMax(SwkbdConfig*,u32);
void swkbdConfigSetPasswordFlag(SwkbdConfig*,u32);
Result swkbdShow(SwkbdConfig*,char*,size_t);
void swkbdClose(SwkbdConfig*);
}
