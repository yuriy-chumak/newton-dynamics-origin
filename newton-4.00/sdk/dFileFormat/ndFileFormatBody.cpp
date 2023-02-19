/* Copyright (c) <2003-2022> <Julio Jerez, Newton Game Dynamics>
* 
* This software is provided 'as-is', without any express or implied
* warranty. In no event will the authors be held liable for any damages
* arising from the use of this software.
* 
* Permission is granted to anyone to use this software for any purpose,
* including commercial applications, and to alter it and redistribute it
* freely, subject to the following restrictions:
* 
* 1. The origin of this software must not be misrepresented; you must not
* claim that you wrote the original software. If you use this software
* in a product, an acknowledgment in the product documentation would be
* appreciated but is not required.
* 
* 2. Altered source versions must be plainly marked as such, and must not be
* misrepresented as being the original software.
* 
* 3. This notice may not be removed or altered from any source distribution.
*/

#include "ndFileFormatStdafx.h"
#include "ndFileFormatBody.h"
#include "ndFileFormatNotify.h"

ndFileFormatBody::ndFileFormatBody()
	:ndFileFormatRegistrar(ndBody::StaticClassName())
{
}

ndFileFormatBody::ndFileFormatBody(const char* const className)
	:ndFileFormatRegistrar(className)
{
}

void ndFileFormatBody::SaveBody(nd::TiXmlElement* const parentNode, const ndBody* const body)
{
	nd::TiXmlElement* const classNode = xmlCreateClassNode(parentNode, "ndBodyClass", ndBody::StaticClassName());

	ndBodyNotify* const notity = body->GetNotifyCallback();
	ndFileFormatRegistrar* const handler = ndFileFormatRegistrar::GetHandler(notity->ClassName());
	ndAssert(handler);
	handler->SaveNotify(classNode, notity);

	xmlSaveParam(classNode, "matrix", body->GetMatrix());
	xmlSaveParam(classNode, "omega", body->GetOmega());
	xmlSaveParam(classNode, "velocity", body->GetVelocity());
	xmlSaveParam(classNode, "centerOfMass", body->GetCentreOfMass());
}