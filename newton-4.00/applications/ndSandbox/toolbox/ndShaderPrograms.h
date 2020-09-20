/* Copyright (c) <2009> <Newton Game Dynamics>
* 
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
* 
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely
*/

//********************************************************************
// Newton Game dynamics 
// copyright 2000-2004
// By Julio Jerez
// VC: 6.0
// 
//********************************************************************


#ifndef __SHADERS_PROGRAMS_H__
#define __SHADERS_PROGRAMS_H__

#include "ndSandboxStdafx.h"
#include "ndOpenGlUtil.h"

class ndShaderPrograms
{
	public:
	ndShaderPrograms();
	~ndShaderPrograms();

	bool CreateAllEffects();

	private:
	void LoadShaderCode (const char* const name, char* const buffer);
	GLuint CreateShaderEffect (const char* const vertexShader, const char* const pixelShader);

	public:
	GLuint m_solidColor;
	GLuint m_decalEffect;
	GLuint m_diffuseEffect;
	GLuint m_diffuseIntanceEffect;
	GLuint m_skinningDiffuseEffect;
	GLuint m_diffuseNoTextureEffect;
};

#endif