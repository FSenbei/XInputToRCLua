/*

*/
#define DLLEXP extern "C" __declspec(dllexport)
//#define RCEXP extern "C" static
#define RCEXP extern "C" __declspec(dllexport)

#define ARRAY_LENGTH(array) (sizeof(array) / sizeof(array[0])) // �z��̗v�f���̌v�Z

#include <stdio.h>
#include <Windows.h>
#include <vector>

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}


//lua_State* L; // Lua�̏�Ԃ������i�[����ϐ��炵��

//---- �֐��v���g�^�C�v ----//
//---- ----//


BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:  // ���[�h���ꂽ
		//MessageBox( NULL, "Load", "Test", MB_OK );
		break;
	case DLL_PROCESS_DETACH:  // �A�����[�h���ꂽ
		//MessageBox( NULL, "UnLoad", "Test", MB_OK );
		break;
	case DLL_THREAD_ATTACH:   // �X���b�h���쐬����
		//MessageBox( NULL, "Threead Attach", "Test", MB_OK );
		break;
	case DLL_THREAD_DETACH:   // �X���b�h���I������
		//MessageBox( NULL, "Threead Detach", "Test", MB_OK );
		break;
	}
	return TRUE;
}


RCEXP int TestFunc(lua_State *L) //  ����m�F�p
{
	static int test = 10;
	test = test + 1;
	lua_pushnumber(L, test );
	return 1;
}


DLLEXP int RegFunc(lua_State *L) //�֐���Lua�ɓo�^����
{
	//lua_open()�͂���Ȃ����ۂ��H
	//lua_State *L = lua_open();  /* create state */

	lua_register(L, "C_TestFunc", TestFunc);
	MessageBox(NULL, "(�L�E�ցE�M)", "Error", MB_OK);
	//lua_close(L);
	return 1;
}