/*
	RCのLuaでXInputを使うためのDLL

	KSPのタイトル画面見たらRCがやりたくなったが、公式ドライバの360コンのトリガーを左右別々に取得することができないことにキレて、そこら辺のサンプルからコピペしたりして見よう見まねで作られたブツ。
	自分の環境で動けばなんでもよかった。
	2014年作成？
*/
#define DLLEXP extern "C" __declspec(dllexport)
//#define RCEXP extern "C" static
#define RCEXP extern "C" __declspec(dllexport)

#define ARRAY_LENGTH(array) (sizeof(array) / sizeof(array[0])) // 配列の要素数の計算

#include <stdio.h>
#include <Windows.h>
#include <XInput.h> // XInput API
#include <vector>

extern "C" { 
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

//---- XInput関係 ----//
#define MAX_CONTROLLERS 4  // XInput が認識できるのは４つまでなので、とりあえず４
struct CONTROLER_STATE // XInputの接続状態とボタン、スティック類の状態を格納するための構造体 SDKのサンプルからのコピペのやる気の無さ
{
    XINPUT_STATE state;
    bool bConnected;
};

typedef DWORD ( __stdcall *XIGetStatePtr)( DWORD, XINPUT_STATE*); // XInputGetStateExの格納に使用する関数ポインタ。呼び出し規約無いと落ちるっぽい？

const WORD ButtonList[] = { // XInputのボタンリスト。 トリガーはアナログしか取得できないので別枠。
	XINPUT_GAMEPAD_A,
	XINPUT_GAMEPAD_B,
	XINPUT_GAMEPAD_X,
	XINPUT_GAMEPAD_Y,
	XINPUT_GAMEPAD_LEFT_SHOULDER,
	XINPUT_GAMEPAD_RIGHT_SHOULDER,
	XINPUT_GAMEPAD_BACK,
	XINPUT_GAMEPAD_START,
	XINPUT_GAMEPAD_LEFT_THUMB,
	XINPUT_GAMEPAD_RIGHT_THUMB,
	XINPUT_GAMEPAD_DPAD_UP,
	XINPUT_GAMEPAD_DPAD_DOWN,
	XINPUT_GAMEPAD_DPAD_LEFT,
	XINPUT_GAMEPAD_DPAD_RIGHT,
	0x0400,
};

std::vector<CONTROLER_STATE> ControllersState( MAX_CONTROLLERS); // 上記のCONTROLER_STATE構造体を最大認識数の分だけ格納する配列。
std::vector<CONTROLER_STATE> ControllersStatePrev( MAX_CONTROLLERS); // 前フレームのコントローラーの状態を格納する配列。
bool UseDeadZone = false;
#define DEFAULT_DEADZONE  ( 0.24f * FLOAT(0x7FFF) )  // Default to 24% of the +/- 32767 range.   This is a reasonable default value but can be altered if needed. このデッドゾーンクソすぎ バカじゃないの？ 後で書き直したい。
SHORT InputDeadZone = (SHORT) DEFAULT_DEADZONE;

//---- ----//

//lua_State* L; // Luaの状態だかを格納する変数らしい
bool ActiveOnly=true;

//---- 関数プロトタイプ ----//
HRESULT UpdateControllerState();
XIGetStatePtr InitXInputGetStateEx();
bool GetCurrentProcessWindowActive();
//---- ----//


BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	switch( fdwReason )
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
inline bool GetXInputButton( CONTROLER_STATE cont_state, INT ButtonNum ) // ボタン情報の取得。この関数はこのDLL内から呼び出すためのモノであり、Luaから使用するものではない。
{
	WORD wButtons = cont_state.state.Gamepad.wButtons; // ボタン情報
	WORD ButtonState = 0; // ボタンが押されているかどうか

	if ( ButtonNum >= 0 && ButtonNum <= ARRAY_LENGTH( ButtonList )-1 ) // 引数がボタンリストの範囲内か確認 （ C言語は 0オリジンなので要素数-1）
		ButtonState = wButtons & ButtonList[ ButtonNum ];
	else if ( ButtonNum == ARRAY_LENGTH( ButtonList ) ) // トリガーは取得方法が違うのでこんなことに(´・ω・`)	
		ButtonState = ( cont_state.state.Gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD ) ? 1:0 ; // トリガーはアナログ値で返ってくるので自力でデジタル化
	else if ( ButtonNum == ARRAY_LENGTH( ButtonList )+1 )
		ButtonState = ( cont_state.state.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD ) ? 1:0;
	else
		ButtonState = 0;

	// そのままだと使いづらい値で返ってくるので、 Boolで返す。
	if ( ButtonState > 0 )
		return true;
	else
		return false;
}

RCEXP int GetXInputButtonToLua( lua_State *L ) //XInputのボタン情報を取得する関数 RCで言うところの_KEY相当
{
	if ( GetCurrentProcessWindowActive() )
	{
		if ( lua_isnumber(L, 1) && lua_isnumber(L, 2) && lua_gettop(L)==2) //型通りの引数が必要な分あるか確認
		{
			if ( lua_tonumber(L, 1) >= 0 && lua_tonumber(L, 1) <= MAX_CONTROLLERS-1 ) // 取得するパッドの番号が範囲内か確認
			{
				INT PadNum = (INT) lua_tonumber(L, 1);
				WORD ButtonState = (WORD) GetXInputButton( ControllersState[PadNum], (INT) lua_tonumber(L, 2) ); // ボタンが押されているかどうか
				lua_pushnumber( L, ButtonState );
			}
			else { lua_pushnumber( L, -2); } // エラー代わり
		}
		else { lua_pushnumber( L, -1); } // エラー代わり
	}
	else
		lua_pushnumber( L, 0 );

	return 1;
}
RCEXP int GetXInputButtonDownToLua( lua_State *L ) // RCで言うところの_KEYDOWNのXInput版
{
	if ( GetCurrentProcessWindowActive() )
	{
		if ( lua_isnumber(L, 1) && lua_isnumber(L, 2) && lua_gettop(L)==2) //型通りの引数が必要な分あるか確認
		{
			if ( lua_tonumber(L, 1) >= 0 && lua_tonumber(L, 1) <= MAX_CONTROLLERS-1 ) // 取得するパッドの番号が範囲内か確認
			{
				INT PadNum = (INT) lua_tonumber(L, 1);
				WORD ButtonState = (WORD) GetXInputButton( ControllersState[PadNum], (INT) lua_tonumber(L, 2) ); // 現フレームのボタンの状態
				WORD ButtonStatePrev = (WORD) GetXInputButton( ControllersStatePrev[PadNum], (INT) lua_tonumber(L, 2) ); // 前フレームのボタンの状態
				if ( ButtonStatePrev == 0 && ButtonState == 1 ) // 現在のフレームだけボタンが押されている場合に1を返す。
					lua_pushnumber( L, 1 );
				else
					lua_pushnumber( L, 0 );
			}
			else { lua_pushnumber( L, -2); } // エラー代わり
		}
		else { lua_pushnumber( L, -1); } // エラー代わり
	}
	else
		lua_pushnumber( L, 0 );

	return 1;
}
RCEXP int GetXInputButtonUpToLua( lua_State *L ) // RCで言うところの_KEYUPのXInput版
{
	if ( GetCurrentProcessWindowActive() )
	{
		if ( lua_isnumber(L, 1) && lua_isnumber(L, 2) && lua_gettop(L)==2) //型通りの引数が必要な分あるか確認
		{
			if ( lua_tonumber(L, 1) >= 0 && lua_tonumber(L, 1) <= MAX_CONTROLLERS-1 ) // 取得するパッドの番号が範囲内か確認
			{
				INT PadNum = (INT) lua_tonumber(L, 1);
				WORD ButtonState = (WORD) GetXInputButton( ControllersState[PadNum], (INT) lua_tonumber(L, 2) ); // 現フレームのボタンの状態
				WORD ButtonStatePrev = (WORD) GetXInputButton( ControllersStatePrev[PadNum], (INT) lua_tonumber(L, 2) ); // 前フレームのボタンの状態
				if ( ButtonStatePrev == 1 && ButtonState == 0 ) // 前のフレームだけボタンが押されている場合に1を返す。
					lua_pushnumber( L, 1 );
				else
					lua_pushnumber( L, 0 );
			}
			else { lua_pushnumber( L, -2); } // エラー代わり
		}
		else { lua_pushnumber( L, -1); } // エラー代わり
	}
	else
		lua_pushnumber( L, 0 );

	return 1;
}
RCEXP int GetXInputAnalogToLua( lua_State *L ) // RCで言うところの_ANALOG
{
	if ( GetCurrentProcessWindowActive() )
	{
		if ( lua_isnumber(L, 1) && lua_isnumber(L, 2) && lua_gettop(L)==2) //型通りの引数が必要な分あるか確認
		{
			if ( lua_tonumber(L, 1) >= 0 && lua_tonumber(L, 1) <= MAX_CONTROLLERS-1 ) // 取得するパッドの番号が範囲内か確認
			{
				INT PadNum = (INT) lua_tonumber(L, 1); // パッドの番号
				XINPUT_GAMEPAD GamePad = ControllersState[PadNum].state.Gamepad; // そのままだとクソ長いし、デッドゾーンも使うかもだし、ということで別変数化。

				if ( UseDeadZone ) // デッドゾーンの使用がオンになっている場合はデッドゾーンを適用してからswitch文へ。
				{
					if ( GamePad.sThumbLX < InputDeadZone && GamePad.sThumbLX > -InputDeadZone )
						GamePad.sThumbLX = 0;
					if ( GamePad.sThumbLY < InputDeadZone && GamePad.sThumbLY > -InputDeadZone )
						GamePad.sThumbLY = 0;
					if ( GamePad.sThumbRX < InputDeadZone && GamePad.sThumbRX > -InputDeadZone )
						GamePad.sThumbRX = 0;
					if ( GamePad.sThumbRY < InputDeadZone && GamePad.sThumbRY > -InputDeadZone )
						GamePad.sThumbRY = 0;
				}
				switch ( (int) lua_tonumber(L, 2) ) // 引数に合わせてアナログの値を返す
				{
					case 0: lua_pushnumber( L, GamePad.sThumbLX ); break;
					case 1: lua_pushnumber( L, GamePad.sThumbLY ); break;
					case 2: lua_pushnumber( L, GamePad.sThumbRX ); break;
					case 3: lua_pushnumber( L, GamePad.sThumbRY ); break;
					case 4: lua_pushnumber( L, GamePad.bLeftTrigger ); break;
					case 5: lua_pushnumber( L, GamePad.bRightTrigger ); break;
					default: lua_pushnumber( L, 0 );
				}
			}
			else
				lua_pushnumber( L, -2 );
		}
		else
			lua_pushnumber( L, -1 );
	}
	else
		lua_pushnumber( L, 0 );

	return 1;
}
RCEXP int XInputSetDeadZone( lua_State *L ) // デッドゾーンを使用するかどうか
{
	if ( lua_isboolean(L,1) )
		UseDeadZone = lua_toboolean(L,1); // 引数のブーリアンをそのまま突っ込む
	if ( lua_isnumber(L,2) )
	{
		if ( lua_tonumber(L,2) >0 && lua_tonumber(L,2) < FLOAT(0x7FFF) ) // ２つ目の引数はデッドゾーンの閾値の設定に使う
			InputDeadZone = (SHORT) lua_tonumber(L,2); // 0以上かつ最大値以下の場合だけ設定可能
		else if ( lua_tonumber(L,2) < 0 ) 
			InputDeadZone = (SHORT) DEFAULT_DEADZONE; // 負の数を入れるとデフォルトの値になりまする
	}
	lua_pushboolean(L, UseDeadZone);
	return 1;
}
RCEXP int XInputSetVibration( lua_State *L ) // 振動の設定
{
	XINPUT_VIBRATION Vibration;
	if ( lua_isnumber(L, 1) && lua_isnumber(L, 2) && lua_isnumber(L, 3) && lua_gettop(L)==3)
	{
		Vibration.wLeftMotorSpeed = (WORD) lua_tonumber(L,2);
		Vibration.wRightMotorSpeed = (WORD) lua_tonumber(L,3);
		XInputSetState( (DWORD) lua_tonumber(L,1), &Vibration );
	}
	return 0;
}
RCEXP int UpdateControllerStateToLua( lua_State *L ) //コントローラーの状態を取得（更新）する
{
	if ( UpdateControllerState() == S_OK )
		lua_pushnumber( L, 1 );
	else
		lua_pushnumber( L, 0 );
    return 1;
}
RCEXP int XInputSetActiveOnly( lua_State *L ) // ウィンドウがアクティブなときだけ取得させるかどうか。
{
	if ( lua_isboolean(L,1) )
		ActiveOnly = lua_toboolean(L,1); // 引数のブーリアンをそのまま突っ込む
	lua_pushboolean(L, ActiveOnly);
	return 1;
}

RCEXP int GetXInputRawButton( lua_State *L ) // ビットマスク掛ける前の生のボタン情報を返す 動作確認用
{
	//UpdateControllerState(); //パッド情報の更新
	lua_pushnumber( L, (lua_Number) ControllersState[1].state.Gamepad.wButtons );
	return 1;
}
RCEXP int GetXInputPacketNumber( lua_State *L ) // dwPacketNumberを返すだけ
{
	//UpdateControllerState(); //パッド情報の更新
	lua_pushnumber( L, (lua_Number) ControllersState[1].state.dwPacketNumber );
	return 1;
}
RCEXP int GetXInputConnected( lua_State *L ) // コントローラーの接続状態の取得
{
	if ( lua_isnumber(L, 1) )
	{
		if ( lua_tonumber(L, 1) >= 0 && lua_tonumber(L, 1) <= MAX_CONTROLLERS-1 )
			lua_pushnumber( L, (lua_Number) ControllersState[(INT)lua_tonumber(L,1)].bConnected );
	}
	return 1;
}

DLLEXP int RegFunc( lua_State *L ) //関数をLuaに登録する
{
	//lua_open()はいらないっぽい？
	//lua_State *L = lua_open();  /* create state */
	
	lua_register( L, "XInputButton" , GetXInputButtonToLua);
	lua_register( L, "XInputButtonDown", GetXInputButtonDownToLua);
	lua_register( L, "XInputButtonUp", GetXInputButtonUpToLua);
	lua_register( L, "XInputAnalog", GetXInputAnalogToLua);
	lua_register( L, "XInputUpdateState", UpdateControllerStateToLua);
	lua_register( L, "XInputConnected", GetXInputConnected);
	lua_register( L, "XInputSetDeadZone", XInputSetDeadZone);
	lua_register( L, "XInputSetVibration", XInputSetVibration);
	lua_register( L, "XInputSetActiveOnly", XInputSetActiveOnly);

	lua_register( L, "XInputRawButton" , GetXInputRawButton);
	lua_register( L, "XInputPacketNumber", GetXInputPacketNumber);
	
	//lua_close(L);
	return 0;
}

bool GetCurrentProcessWindowActive() // 呼び出し元のプロセス（≒RC）のウィンドウがアクティブかどうかを取得する関数。
{
	if ( ActiveOnly ) // 非アクティブでも取得させる設定のときは、強制的にtrue返させるクソ仕様
	{
		DWORD ProccesID;
		GetWindowThreadProcessId( GetActiveWindow(), &ProccesID );
		if ( ProccesID ==GetCurrentProcessId() )
			return true;
		else
			return false;
	}
	else
		return true;

}
XIGetStatePtr InitXInputGetStateEx() // XInputGetStateExを召喚するための関数
{
	static XIGetStatePtr XInputGetStateEx; // 関数へのポインタ
	if ( XInputGetStateEx == NULL)
	{
		HMODULE XInputLib = LoadLibrary( "xinput1_3.dll" );	
		if (XInputLib == NULL)
		{
			XInputLib = LoadLibrary( "xinput1_4.dll" ); // 読めなかったら別のを試してみる
			if (XInputLib == NULL )
				MessageBox( NULL, "XInputのDLLが読み込めなかった(´・ω・｀)", "Error", MB_OK ); // それでも読めなかったら警告出しとく
		}
		if (XInputLib != NULL )
			XInputGetStateEx = (XIGetStatePtr) GetProcAddress( XInputLib, (LPCSTR)100 ); //XInputGetStateExの召喚 100は序数というものらしい
		else
			XInputGetStateEx = (XIGetStatePtr) &XInputGetState; // DLLが読めなかった場合は、通常の関数を使用する。もちろんガイドボタンの取得はできない。
	}
	return XInputGetStateEx;
}

HRESULT UpdateControllerState() // コントローラーの状態を取得（更新）する
{
	XIGetStatePtr XInputGetStateEx = InitXInputGetStateEx(); //関数へのポインタ
	std::copy( ControllersState.begin(), ControllersState.end(), ControllersStatePrev.begin() ); //コントローラーの状態の配列をコピー
	DWORD dwResult;
	for( DWORD i = 0; i < MAX_CONTROLLERS; i++ )
	{
		dwResult = XInputGetStateEx( i, &ControllersState[i].state );

		if( dwResult == ERROR_SUCCESS ) //エラー吐いたら非接続とみなす？
			ControllersState[i].bConnected = true;
		else
			ControllersState[i].bConnected = false;
	}
    return S_OK;
}