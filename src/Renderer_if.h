/*
	This file is part of libswirl
*/
#include "license/bsd"


#pragma once

#include "core_structs.h"

struct Renderer
{
	virtual bool Init()=0;
	

	virtual bool RenderPVR()=0;
    virtual bool RenderFramebuffer() = 0;
	virtual bool RenderLastFrame() { return false; }

	virtual void Present()=0;

	virtual ~Renderer() { }
};