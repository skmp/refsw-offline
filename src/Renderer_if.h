/*
	This file is part of libswirl
*/
#include "license/bsd"


#pragma once

#include "core_structs.h"

extern u32 VertexCount;
extern u32 FrameCount;


void rend_init_renderer(u8* vram);
void rend_term_renderer();
void rend_vblank();

void rend_start_render(u8* vram);
void rend_end_render();

void rend_set_fb_scale(float x,float y);
void rend_resize(int width, int height);
void rend_text_invl(vram_block* bl);

///////

struct Renderer;

struct rendererbackend_t {
	string slug;
	string desc;
	int priority;
	Renderer* (*create)(u8* vram);
};

struct Renderer
{
	rendererbackend_t backendInfo;	// gets filled by renderer_if

	virtual bool Init()=0;
	

	virtual bool RenderPVR()=0;
    virtual bool RenderFramebuffer() = 0;
	virtual bool RenderLastFrame() { return false; }

	virtual void Present()=0;

	virtual ~Renderer() { }
};

extern bool renderer_enabled;	// Signals the renderer thread to exit
extern bool renderer_changed;	// Signals the renderer thread to switch renderer

extern bool RegisterRendererBackend(const rendererbackend_t& backend);
vector<rendererbackend_t> rend_get_backends();