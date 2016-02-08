/*

*/
#define DLLEXP extern "C" __declspec(dllexport)
//#define RCEXP extern "C" static
#define RCEXP extern "C" __declspec(dllexport)

#define ARRAY_LENGTH(array) (sizeof(array) / sizeof(array[0])) // 配列の要素数の計算

#include <stdio.h>
#include <Windows.h>
#include <vector>

extern "C" {
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}


//lua_State* L; // Luaの状態だかを格納する変数らしい

//---- 関数プロトタイプ ----//
//---- ----//


BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:  // ロードされた
		//MessageBox( NULL, "Load", "Test", MB_OK );
		break;
	case DLL_PROCESS_DETACH:  // アンロードされた
		//MessageBox( NULL, "UnLoad", "Test", MB_OK );
		break;
	case DLL_THREAD_ATTACH:   // スレッドを作成した
		//MessageBox( NULL, "Threead Attach", "Test", MB_OK );
		break;
	case DLL_THREAD_DETACH:   // スレッドが終了した
		//MessageBox( NULL, "Threead Detach", "Test", MB_OK );
		break;
	}
	return TRUE;
}


RCEXP int TestFunc(lua_State *L) //  動作確認用
{
	static int test = 10;
	test = test + 1;
	lua_pushnumber(L, test );
	return 1;
}


DLLEXP int RegFunc(lua_State *L) //関数をLuaに登録する
{
	//lua_open()はいらないっぽい？
	//lua_State *L = lua_open();  /* create state */

	lua_register(L, "C_TestFunc", TestFunc);
	MessageBox(NULL, "(´・ω・｀)", "Error", MB_OK);
	//lua_close(L);
	return 1;
}