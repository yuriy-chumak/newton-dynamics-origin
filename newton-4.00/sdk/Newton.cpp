/* 
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

#include "Newton.h"

#if 1
	#if (defined (__MINGW32__) || defined (__MINGW64__))
		int main(int argc, char* argv[])
		{
			return 0;
		}
	#endif

	#ifdef _MSC_VER
		BOOL APIENTRY DllMain( HMODULE hModule,	DWORD  ul_reason_for_call, LPVOID lpReserved)
		{
			switch (ul_reason_for_call)
			{
				case DLL_THREAD_ATTACH:
				case DLL_PROCESS_ATTACH:
					// check for memory leaks
					#ifdef _DEBUG
						// Track all memory leaks at the operating system level.
						// make sure no Newton tool or utility leaves leaks behind.
						_CrtSetDbgFlag(_CRTDBG_LEAK_CHECK_DF | _CRTDBG_REPORT_FLAG);
					#endif

				case DLL_THREAD_DETACH:
				case DLL_PROCESS_DETACH:
				break;
			}
			return TRUE;
		}
	#endif
#endif

class NewtonWorld: public ndWorld
{

};

D_LIBRARY_EXPORT
NewtonWorld* NewtonWorldCreate() {
	return new NewtonWorld();
}

D_LIBRARY_EXPORT
void NewtonWorldDestroy(NewtonWorld* world)
{
	delete world;
}

D_LIBRARY_EXPORT
ndInt32 NewtonWorldGetEngineVersion(NewtonWorld* world)
{
	return world->GetEngineVersion();
}

D_LIBRARY_EXPORT
void NewtonWorldUpdate(NewtonWorld* world, ndFloat32 timestep)
{
	(void) world->Update(timestep);
}