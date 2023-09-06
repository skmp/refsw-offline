/*
	This file is part of libswirl
*/
#include "license/bsd"


#pragma once

#include "build.h"

#if BUILD_COMPILER==COMPILER_VC
#define DECL_ALIGN(x) __declspec(align(x))
#else
#ifndef __forceinline
#define __forceinline inline
#endif
#define DECL_ALIGN(x) __attribute__((aligned(x)))
#ifndef _WIN32
#define __debugbreak
#endif
#endif


#if HOST_CPU == CPU_X86

	#if BUILD_COMPILER==COMPILER_VC
	#define DYNACALL  __fastcall
	#else
	//android defines fastcall as regparm(3), it doesn't work for us
	#undef fastcall
	#define DYNACALL __attribute__((fastcall))
	#endif
#else
	#define DYNACALL
#endif

#if BUILD_COMPILER==COMPILER_VC
#ifdef _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES
#undef _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES
#endif

#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 1

#ifdef _CRT_SECURE_NO_DEPRECATE
#undef _CRT_SECURE_NO_DEPRECATE
#endif

#define _CRT_SECURE_NO_DEPRECATE
//unnamed struncts/unions
#pragma warning( disable : 4201)

//unused parameters
#pragma warning( disable : 4100)
#endif



#if BUILD_COMPILER==COMPILER_VC
//SHUT UP M$ COMPILER !@#!@$#
#ifdef _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES
#undef _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES
#endif

#define _CRT_SECURE_CPP_OVERLOAD_STANDARD_NAMES 1

#ifdef _CRT_SECURE_NO_DEPRECATE
#undef _CRT_SECURE_NO_DEPRECATE
#endif
#define _CRT_SECURE_NO_DEPRECATE

//Do not complain when i use enum::member
#pragma warning( disable : 4482)

//unnamed struncts/unions
#pragma warning( disable : 4201)

//unused parameters
#pragma warning( disable : 4100)
#endif

#include <stdint.h>
#include <stddef.h>

//basic types
typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float f32;
typedef double f64;

#ifdef _M_X64
#undef X86
#define X64
#endif

typedef ptrdiff_t snat;
typedef size_t unat;

#ifdef X64
typedef u64 unat;
#endif

typedef char wchar;

#define EXPORT extern "C" __declspec(dllexport)



#ifndef CDECL
#define CDECL __cdecl
#endif




//intc function pointer and enums
enum HollyInterruptType
{
	holly_nrm = 0x0000,
	holly_ext = 0x0100,
	holly_err = 0x0200,
};

enum HollyInterruptID
{
		// asic9a /sh4 external holly normal [internal]
		holly_RENDER_DONE_vd = holly_nrm | 0,	//bit 0 = End of Render interrupt : Video
		holly_RENDER_DONE_isp = holly_nrm | 1,	//bit 1 = End of Render interrupt : ISP
		holly_RENDER_DONE = holly_nrm | 2,		//bit 2 = End of Render interrupt : TSP

		holly_SCANINT1 = holly_nrm | 3,			//bit 3 = V Blank-in interrupt
		holly_SCANINT2 = holly_nrm | 4,			//bit 4 = V Blank-out interrupt
		holly_HBLank = holly_nrm | 5,			//bit 5 = H Blank-in interrupt

		holly_YUV_DMA = holly_nrm | 6,			//bit 6 = End of Transferring interrupt : YUV
		holly_OPAQUE = holly_nrm | 7,			//bit 7 = End of Transferring interrupt : Opaque List
		holly_OPAQUEMOD = holly_nrm | 8,		//bit 8 = End of Transferring interrupt : Opaque Modifier Volume List

		holly_TRANS = holly_nrm | 9,			//bit 9 = End of Transferring interrupt : Translucent List
		holly_TRANSMOD = holly_nrm | 10,		//bit 10 = End of Transferring interrupt : Translucent Modifier Volume List
		holly_PVR_DMA = holly_nrm | 11,			//bit 11 = End of DMA interrupt : PVR-DMA
		holly_MAPLE_DMA = holly_nrm | 12,		//bit 12 = End of DMA interrupt : Maple-DMA

		holly_MAPLE_VBOI = holly_nrm | 13,		//bit 13 = Maple V blank over interrupt
		holly_GDROM_DMA = holly_nrm | 14,		//bit 14 = End of DMA interrupt : GD-DMA
		holly_SPU_DMA = holly_nrm | 15,			//bit 15 = End of DMA interrupt : AICA-DMA

		holly_EXT_DMA1 = holly_nrm | 16,		//bit 16 = End of DMA interrupt : Ext-DMA1(External 1)
		holly_EXT_DMA2 = holly_nrm | 17,		//bit 17 = End of DMA interrupt : Ext-DMA2(External 2)
		holly_DEV_DMA = holly_nrm | 18,			//bit 18 = End of DMA interrupt : Dev-DMA(Development tool DMA)

		holly_CH2_DMA = holly_nrm | 19,			//bit 19 = End of DMA interrupt : ch2-DMA
		holly_PVR_SortDMA = holly_nrm | 20,		//bit 20 = End of DMA interrupt : Sort-DMA (Transferring for alpha sorting)
		holly_PUNCHTHRU = holly_nrm | 21,		//bit 21 = End of Transferring interrupt : Punch Through List

		// asic9c/sh4 external holly external [EXTERNAL]
		holly_GDROM_CMD = holly_ext | 0x00,	//bit 0 = GD-ROM interrupt
		holly_SPU_IRQ = holly_ext | 0x01,	//bit 1 = AICA interrupt
		holly_EXP_8BIT = holly_ext | 0x02,	//bit 2 = Modem interrupt
		holly_EXP_PCI = holly_ext | 0x03,	//bit 3 = External Device interrupt

		// asic9b/sh4 external holly err only error [error]
		//missing quite a few ehh ?
		//bit 0 = RENDER : ISP out of Cache(Buffer over flow)
		//bit 1 = RENDER : Hazard Processing of Strip Buffer
		holly_PRIM_NOMEM = holly_err | 0x02,	//bit 2 = TA : ISP/TSP Parameter Overflow
		holly_MATR_NOMEM = holly_err | 0x03,	//bit 3 = TA : Object List Pointer Overflow
		holly_ILLEGAL_PARAM = holly_err | 0x04 //bit 4 = TA : Illegal Parameter
		//bit 5 = TA : FIFO Overflow
		//bit 6 = PVRIF : Illegal Address set
		//bit 7 = PVRIF : DMA over run
		//bit 8 = MAPLE : Illegal Address set
		//bit 9 = MAPLE : DMA over run
		//bit 10 = MAPLE : Write FIFO over flow
		//bit 11 = MAPLE : Illegal command
		//bit 12 = G1 : Illegal Address set
		//bit 13 = G1 : GD-DMA over run
		//bit 14 = G1 : ROM/FLASH access at GD-DMA
		//bit 15 = G2 : AICA-DMA Illegal Address set
		//bit 16 = G2 : Ext-DMA1 Illegal Address set
		//bit 17 = G2 : Ext-DMA2 Illegal Address set
		//bit 18 = G2 : Dev-DMA Illegal Address set
		//bit 19 = G2 : AICA-DMA over run
		//bit 20 = G2 : Ext-DMA1 over run
		//bit 21 = G2 : Ext-DMA2 over run
		//bit 22 = G2 : Dev-DMA over run
		//bit 23 = G2 : AICA-DMA Time out
		//bit 24 = G2 : Ext-DMA1 Time out
		//bit 25 = G2 : Ext-DMA2 Time out
		//bit 26 = G2 : Dev-DMA Time out
		//bit 27 = G2 : Time out in CPU accessing
};



struct vram_block
{
	u32 start;
	u32 end;
	u32 len;
	u32 type;

	void* userdata;
};

enum ndc_error_codes
{
	rv_ok = 0,			//no error
	rv_cli_finish=69,	//clean exit after -help or -version , should we just use rv_ok?

	rv_error=-2,		//error
	rv_serror=-1,		//silent error , it has been reported to the user
};

//Simple struct to store window rect  ;)
//Top is 0,0 & numbers are in pixels.
//Only client size
struct NDC_WINDOW_RECT
{
	u32 width;
	u32 height;
};



////////////////////////////////////////////////////////////////////////////////////////////////////////////

//******************************************************
//*********************** PowerVR **********************
//******************************************************

void libCore_vramlock_Unlock_block  (vram_block* block);
void libCore_vramlock_Unlock_block_wb  (vram_block* block);
vram_block* libCore_vramlock_Lock(u32 start_offset,u32 end_offset,void* userdata);



//******************************************************
//************************ GDRom ***********************
//******************************************************
enum DiscType
{
	CdDA=0x00,
	CdRom=0x10,
	CdRom_XA=0x20,
	CdRom_Extra=0x30,
	CdRom_CDI=0x40,
	GdRom=0x80,

	NoDisk=0x1,			//These are a bit hacky .. but work for now ...
	Open=0x2,			//tray is open :)
	Busy=0x3			//busy -> needs to be automatically done by gdhost
};

enum DiskArea
{
	SingleDensity,
	DoubleDensity
};

enum DriveEvent
{
	DiskChange=1	//disk ejected/changed
};

//******************************************************
//************************ AICA ************************
//******************************************************
void libCore_CDDA_Sector(s16* sector);


//passed on AICA init call


//Ram/Regs are managed by plugin , exept RTC regs (managed by main emu)

//******************************************************
//******************** ARM Sound CPU *******************
//******************************************************

//******************************************************
//****************** Maple devices ******************
//******************************************************


enum MapleDeviceCreationFlags
{
	MDCF_None=0,
	MDCF_Hotplug=1
};

struct maple_subdevice_instance;
struct maple_device_instance;

//buffer_out_len and responce need to be filled w/ proper info by the plugin
//buffer_in must not be edited (its direct pointer on ram)
//output buffer must contain the frame data , the frame header is generated by the maple routing code
//typedef u32 FASTCALL MapleSubDeviceDMAFP(void* device_instance,u32 Command,u32* buffer_in,u32 buffer_in_len,u32* buffer_out,u32& buffer_out_len);
typedef u32 MapleDeviceDMAFP(void* device_instance,u32 Command,u32* buffer_in,u32 buffer_in_len,u32* buffer_out,u32& buffer_out_len);

struct maple_subdevice_instance
{
	//port
	u8 port;
	//user data
	void* data;
	//MapleDeviceDMA
	MapleDeviceDMAFP* dma;
	bool connected;
	u32 reserved;	//reserved for the emu , DO NOT EDIT
};
struct maple_device_instance
{
	//port
	u8 port;
	//user data
	void* data;
	//MapleDeviceDMA
	MapleDeviceDMAFP* dma;
	bool connected;

	maple_subdevice_instance subdevices[5];
};


//includes from CRT
#include <stdlib.h>
#include <stdio.h>

#if defined(TARGET_NACL32)
	int nacl_printf(const wchar* Text,...);
	#define printf nacl_printf
	#define puts(X) printf("%s\n", X)
#endif

#if HOST_OS == OS_DARWIN
int darw_printf(const wchar* Text,...);
#define printf darw_printf
#define puts(X) printf("%s\n", X)
#endif

//includes from c++rt
#include <vector>
#include <string>
#include <map>
using namespace std;

//used for asm-olny functions
#if COMPILER_VC==BUILD_COMPILER
#define naked   __declspec( naked )
#else
#define naked __attribute__((naked))
#endif

// NOTE: Always inline for macOS builds or it causes duplicate symbol linker errors
#if DEBUG && HOST_OS != OS_DARWIN
//force
#define INLINE
//sugest
#define SINLINE
#else
//force
#define INLINE __forceinline
//sugest
#define SINLINE __inline
#endif

//no inline -- fixme
#if BUILD_COMPILER == COMPILER_VC
#define NOINLINE __declspec(noinline)
#else
#define NOINLINE __attribute__ ((noinline))
#endif

#if BUILD_COMPILER == COMPILER_VC
#define likely(x) x
#define unlikely(x) x
#else
#define likely(x)       __builtin_expect((x),1)
#define unlikely(x)       __builtin_expect((x),0)
#endif

//basic includes
#include "stdclass.h"

#ifndef RELEASE
#define EMUERROR(format, ...) printf("Error in %20s:%s:%d: " format "\n", \
		__FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)
//strlen(__FILE__) <= 20 ? __FILE__ : __FILE__ + strlen(__FILE__) - 20,
#else
#define EMUERROR(format, ...)
#endif
#define EMUERROR2 EMUERROR
#define EMUERROR3 EMUERROR
#define EMUERROR4 EMUERROR

#ifndef NO_MMU
#define _X_x_X_MMU_VER_STR "/mmu"
#else
#define _X_x_X_MMU_VER_STR ""
#endif


#if DC_PLATFORM==DC_PLATFORM_DREAMCAST
	#define VER_EMUNAME		"reicast"
#elif DC_PLATFORM==DC_PLATFORM_DEV_UNIT
	#define VER_EMUNAME		"reicast-DevKit-SET5.21"
#elif DC_PLATFORM==DC_PLATFORM_NAOMI
	#define VER_EMUNAME		"reicast-Naomi"
#elif DC_PLATFORM==DC_PLATFORM_ATOMISWAVE
	#define VER_EMUNAME		"reicast-AtomisWave"
#else
	#error unknown target platform
#endif


#define VER_FULLNAME	VER_EMUNAME " git" _X_x_X_MMU_VER_STR " (built " __DATE__ "@" __TIME__ ")"
#define VER_SHORTNAME	VER_EMUNAME " git" _X_x_X_MMU_VER_STR


void os_DebugBreak();
#define dbgbreak os_DebugBreak()

#if COMPILER_VC_OR_CLANG_WIN32
#pragma warning( disable : 4127 4996 /*4244*/)
#else
#define stricmp strcasecmp
#endif

#define verify(x) if((x)==false){ msgboxf("Verify Failed  : " #x "\n in %s -> %s : %d \n",MBX_ICONERROR,(__FUNCTION__),(__FILE__),__LINE__); dbgbreak;}
#define die(reason) { msgboxf("Fatal error : %s\n in %s -> %s : %d \n",MBX_ICONERROR,(reason),(__FUNCTION__),(__FILE__),__LINE__); dbgbreak;}

#define fverify verify


//will be removed sometime soon
//This shit needs to be moved to proper headers

enum SmcCheckEnum {
	NoCheck = -1,
	FullCheck = 0,
	FastCheck = 1,
	FaultCheck = 2,
	ValidationCheck = 3,
};

struct settings_t
{
	struct {
		bool UseReios;
	} bios;

	struct {
		string ElfFile;
	} reios;

	struct {
		bool isShown;
	} savepopup;

	struct
	{
		bool UseMipmaps;
		bool WideScreen;
		bool ShowFPS;
		bool RenderToTextureBuffer;
		int RenderToTextureUpscale;
		bool TranslucentPolygonDepthMask;
		bool ModifierVolumes;
		bool Clipping;
		int TextureUpscale;
		int MaxFilteredTextureSize;
		f32 ExtraDepthScale;
		bool CustomTextures;
		bool DumpTextures;
		int ScreenScaling;		// in percent. 50 means half the native resolution
		int ScreenStretching;	// in percent. 150 means stretch from 4/3 to 6/3
		bool Fog;
		bool FloatVMUs;
		bool Rotate90;			// Rotate the screen 90 deg CC
		int ScreenOrientation;		//Force Screen Orientation value here: 1=Force Portrait, 2=Force Landscape, 3=AutoRotate
	} rend;

	struct
	{
		bool Enable;
		bool idleskip;
		bool unstable_opt;
		bool safemode;
		bool disable_nvmem;
		SmcCheckEnum SmcCheckLevel;
		int ScpuEnable;
		int DspEnable;
	} dynarec;


	struct
	{
		u32 run_counts;
	} profile;

	struct
	{
		u32 cable;			// 0 -> VGA, 1 -> VGA, 2 -> RGB, 3 -> TV
		u32 region;			// 0 -> JP, 1 -> USA, 2 -> EU, 3 -> default
		u32 broadcast;		// 0 -> NTSC, 1 -> PAL, 2 -> PAL/M, 3 -> PAL/N, 4 -> default
		u32 language;		// 0 -> JP, 1 -> EN, 2 -> DE, 3 -> FR, 4 -> SP, 5 -> IT, 6 -> default
		std::vector<std::string> ContentPath;
		bool FullMMU;
	} dreamcast;

	struct
	{
		u32 HW_mixing;		//(0) -> SW , 1 -> HW , 2 -> Auto
		u32 BufferSize;		//In samples ,*4 for bytes (1024)
		bool LimitFPS;		// defaults to true
		u32 GlobalFocus;	//0 -> only hwnd , (1) -> Global
		u32 BufferCount;	//BufferCount+2 buffers used , max 60 , default 0
		u32 CDDAMute;
		u32 GlobalMute;
		u32 DSPEnabled;		//0 -> no, 1 -> yes
		bool OldSyncronousDma;		// 1 -> sync dma (old behavior), 0 -> async dma (fixes some games, partial implementation)
		bool NoBatch;
		bool NoSound;
	} aica;

	struct{
		std::string backend;

		// slug<<key, value>>
		std::map<std::string, std::map<std::string, std::string>> options;
	} audio;


#if USE_OMX
	struct
	{
		u32 Audio_Latency;
		bool Audio_HDMI;
	} omx;
#endif

#if SUPPORT_DISPMANX
	struct
	{
		u32 Width;
		u32 Height;
		bool Keep_Aspect;
	} dispmanx;
#endif

	struct
	{
		bool PatchRegion;
		bool LoadDefaultImage;
		char DefaultImage[512];
		char LastImage[512];
	} imgread;

	struct
	{
		u32 ta_skip;
		string backend;

		u32 MaxThreads;
		bool SynchronousRender;
		bool ForceGLES2;
	} pvr;

	struct {
		bool SerialConsole;
		bool VirtualSerialPort;
		string VirtualSerialPortFile;
	} debug;

	struct {
		bool OpenGlChecks;
	} validate;

	struct {
		u32 MouseSensitivity;
		u32 JammaSetup;			// 0: standard, 1: 4-players, 2: rotary encoders, 3: Sega Marine Fishing,
								// 4: dual I/O boards (4P), 5: Namco JYU board (Ninja Assault)
		int maple_devices[4];
		int maple_expansion_devices[4][2];
		int VirtualGamepadVibration;
	} input;


	struct {
		bool HideCallToAction;
	} social;

	struct {
		bool HideHomebrew;
		bool ShowArchiveOrg;
	} cloudroms;

};

extern settings_t settings;

void InitSettings();
void LoadSettings(bool game_specific);
void SaveSettings();

extern u32 patchRB;

inline bool is_s8(u32 v) { return (s8)v==(s32)v; }
inline bool is_u8(u32 v) { return (u8)v==(s32)v; }
inline bool is_s16(u32 v) { return (s16)v==(s32)v; }
inline bool is_u16(u32 v) { return (u16)v==(u32)v; }

#define verifyc(x) verify(!FAILED(x))

static inline void do_nada(...) { }

#ifdef _ANDROID
#include <android/log.h>

#ifdef printf
#undef printf
#endif

#ifdef puts
#undef puts
#endif

#define LOG_TAG   "reicast"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,LOG_TAG,__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,LOG_TAG,__VA_ARGS__)

#define puts      LOGI
#define printf    LOGI
#define putinf    LOGI
#endif




struct OnLoad
{
	typedef void OnLoadFP();
	OnLoad(OnLoadFP* fp) { fp(); }
};

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

