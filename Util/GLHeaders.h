//////////////////////////////////////////////////////////////////////////////////
//	This file is part of the continued Journey MMORPG client					//
//																				//
//	This program is free software: you can redistribute it and/or modify		//
//	it under the terms of the GNU Affero General Public License as published by	//
//	the Free Software Foundation, either version 3 of the License, or			//
//	(at your option) any later version.											//
//////////////////////////////////////////////////////////////////////////////////
//
// One place to pull in the GL headers, so the platform choice is stated once
// instead of repeated at every include site.
//
// Desktop and Switch load desktop OpenGL through glad. Android uses GLES2
// directly - the NDK ships the headers and the driver, so no loader is needed.
//
// The client's renderer only uses calls that exist in both (VBOs, shaders,
// textures, blending), which is why this substitution works at all.
//
#pragma once

#if defined(PLATFORM_ANDROID)
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#else
#include <glad/glad.h>
#endif
