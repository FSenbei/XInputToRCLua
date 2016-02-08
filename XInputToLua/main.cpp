/*
	RC��Lua��XInput���g�����߂�DLL

	KSP�̃^�C�g����ʌ�����RC����肽���Ȃ������A�����h���C�o��360�R���̃g���K�[�����E�ʁX�Ɏ擾���邱�Ƃ��ł��Ȃ����ƂɃL���āA������ӂ̃T���v������R�s�y�����肵�Č��悤���܂˂ō��ꂽ�u�c�B
	�����̊��œ����΂Ȃ�ł��悩�����B
	2014�N�쐬�H
*/
#define DLLEXP extern "C" __declspec(dllexport)
//#define RCEXP extern "C" static
#define RCEXP extern "C" __declspec(dllexport)

#define ARRAY_LENGTH(array) (sizeof(array) / sizeof(array[0])) // �z��̗v�f���̌v�Z

#include <stdio.h>
#include <Windows.h>
#include <XInput.h> // XInput API
#include <vector>

extern "C" { 
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
}

//---- XInput�֌W ----//
#define MAX_CONTROLLERS 4  // XInput ���F���ł���̂͂S�܂łȂ̂ŁA�Ƃ肠�����S
struct CONTROLER_STATE // XInput�̐ڑ���Ԃƃ{�^���A�X�e�B�b�N�ނ̏�Ԃ��i�[���邽�߂̍\���� SDK�̃T���v������̃R�s�y�̂��C�̖���
{
    XINPUT_STATE state;
    bool bConnected;
};

typedef DWORD ( __stdcall *XIGetStatePtr)( DWORD, XINPUT_STATE*); // XInputGetStateEx�̊i�[�Ɏg�p����֐��|�C���^�B�Ăяo���K�񖳂��Ɨ�������ۂ��H

const WORD ButtonList[] = { // XInput�̃{�^�����X�g�B �g���K�[�̓A�i���O�����擾�ł��Ȃ��̂ŕʘg�B
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

std::vector<CONTROLER_STATE> ControllersState( MAX_CONTROLLERS); // ��L��CONTROLER_STATE�\���̂��ő�F�����̕������i�[����z��B
std::vector<CONTROLER_STATE> ControllersStatePrev( MAX_CONTROLLERS); // �O�t���[���̃R���g���[���[�̏�Ԃ��i�[����z��B
bool UseDeadZone = false;
#define DEFAULT_DEADZONE  ( 0.24f * FLOAT(0x7FFF) )  // Default to 24% of the +/- 32767 range.   This is a reasonable default value but can be altered if needed. ���̃f�b�h�]�[���N�\���� �o�J����Ȃ��́H ��ŏ������������B
SHORT InputDeadZone = (SHORT) DEFAULT_DEADZONE;

//---- ----//

//lua_State* L; // Lua�̏�Ԃ������i�[����ϐ��炵��
bool ActiveOnly=true;

//---- �֐��v���g�^�C�v ----//
HRESULT UpdateControllerState();
XIGetStatePtr InitXInputGetStateEx();
bool GetCurrentProcessWindowActive();
//---- ----//


BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	switch( fdwReason )
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
inline bool GetXInputButton( CONTROLER_STATE cont_state, INT ButtonNum ) // �{�^�����̎擾�B���̊֐��͂���DLL������Ăяo�����߂̃��m�ł���ALua����g�p������̂ł͂Ȃ��B
{
	WORD wButtons = cont_state.state.Gamepad.wButtons; // �{�^�����
	WORD ButtonState = 0; // �{�^����������Ă��邩�ǂ���

	if ( ButtonNum >= 0 && ButtonNum <= ARRAY_LENGTH( ButtonList )-1 ) // �������{�^�����X�g�͈͓̔����m�F �i C����� 0�I���W���Ȃ̂ŗv�f��-1�j
		ButtonState = wButtons & ButtonList[ ButtonNum ];
	else if ( ButtonNum == ARRAY_LENGTH( ButtonList ) ) // �g���K�[�͎擾���@���Ⴄ�̂ł���Ȃ��Ƃ�(�L�E�ցE`)	
		ButtonState = ( cont_state.state.Gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD ) ? 1:0 ; // �g���K�[�̓A�i���O�l�ŕԂ��Ă���̂Ŏ��͂Ńf�W�^����
	else if ( ButtonNum == ARRAY_LENGTH( ButtonList )+1 )
		ButtonState = ( cont_state.state.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD ) ? 1:0;
	else
		ButtonState = 0;

	// ���̂܂܂��Ǝg���Â炢�l�ŕԂ��Ă���̂ŁA Bool�ŕԂ��B
	if ( ButtonState > 0 )
		return true;
	else
		return false;
}

RCEXP int GetXInputButtonToLua( lua_State *L ) //XInput�̃{�^�������擾����֐� RC�Ō����Ƃ����_KEY����
{
	if ( GetCurrentProcessWindowActive() )
	{
		if ( lua_isnumber(L, 1) && lua_isnumber(L, 2) && lua_gettop(L)==2) //�^�ʂ�̈������K�v�ȕ����邩�m�F
		{
			if ( lua_tonumber(L, 1) >= 0 && lua_tonumber(L, 1) <= MAX_CONTROLLERS-1 ) // �擾����p�b�h�̔ԍ����͈͓����m�F
			{
				INT PadNum = (INT) lua_tonumber(L, 1);
				WORD ButtonState = (WORD) GetXInputButton( ControllersState[PadNum], (INT) lua_tonumber(L, 2) ); // �{�^����������Ă��邩�ǂ���
				lua_pushnumber( L, ButtonState );
			}
			else { lua_pushnumber( L, -2); } // �G���[����
		}
		else { lua_pushnumber( L, -1); } // �G���[����
	}
	else
		lua_pushnumber( L, 0 );

	return 1;
}
RCEXP int GetXInputButtonDownToLua( lua_State *L ) // RC�Ō����Ƃ����_KEYDOWN��XInput��
{
	if ( GetCurrentProcessWindowActive() )
	{
		if ( lua_isnumber(L, 1) && lua_isnumber(L, 2) && lua_gettop(L)==2) //�^�ʂ�̈������K�v�ȕ����邩�m�F
		{
			if ( lua_tonumber(L, 1) >= 0 && lua_tonumber(L, 1) <= MAX_CONTROLLERS-1 ) // �擾����p�b�h�̔ԍ����͈͓����m�F
			{
				INT PadNum = (INT) lua_tonumber(L, 1);
				WORD ButtonState = (WORD) GetXInputButton( ControllersState[PadNum], (INT) lua_tonumber(L, 2) ); // ���t���[���̃{�^���̏��
				WORD ButtonStatePrev = (WORD) GetXInputButton( ControllersStatePrev[PadNum], (INT) lua_tonumber(L, 2) ); // �O�t���[���̃{�^���̏��
				if ( ButtonStatePrev == 0 && ButtonState == 1 ) // ���݂̃t���[�������{�^����������Ă���ꍇ��1��Ԃ��B
					lua_pushnumber( L, 1 );
				else
					lua_pushnumber( L, 0 );
			}
			else { lua_pushnumber( L, -2); } // �G���[����
		}
		else { lua_pushnumber( L, -1); } // �G���[����
	}
	else
		lua_pushnumber( L, 0 );

	return 1;
}
RCEXP int GetXInputButtonUpToLua( lua_State *L ) // RC�Ō����Ƃ����_KEYUP��XInput��
{
	if ( GetCurrentProcessWindowActive() )
	{
		if ( lua_isnumber(L, 1) && lua_isnumber(L, 2) && lua_gettop(L)==2) //�^�ʂ�̈������K�v�ȕ����邩�m�F
		{
			if ( lua_tonumber(L, 1) >= 0 && lua_tonumber(L, 1) <= MAX_CONTROLLERS-1 ) // �擾����p�b�h�̔ԍ����͈͓����m�F
			{
				INT PadNum = (INT) lua_tonumber(L, 1);
				WORD ButtonState = (WORD) GetXInputButton( ControllersState[PadNum], (INT) lua_tonumber(L, 2) ); // ���t���[���̃{�^���̏��
				WORD ButtonStatePrev = (WORD) GetXInputButton( ControllersStatePrev[PadNum], (INT) lua_tonumber(L, 2) ); // �O�t���[���̃{�^���̏��
				if ( ButtonStatePrev == 1 && ButtonState == 0 ) // �O�̃t���[�������{�^����������Ă���ꍇ��1��Ԃ��B
					lua_pushnumber( L, 1 );
				else
					lua_pushnumber( L, 0 );
			}
			else { lua_pushnumber( L, -2); } // �G���[����
		}
		else { lua_pushnumber( L, -1); } // �G���[����
	}
	else
		lua_pushnumber( L, 0 );

	return 1;
}
RCEXP int GetXInputAnalogToLua( lua_State *L ) // RC�Ō����Ƃ����_ANALOG
{
	if ( GetCurrentProcessWindowActive() )
	{
		if ( lua_isnumber(L, 1) && lua_isnumber(L, 2) && lua_gettop(L)==2) //�^�ʂ�̈������K�v�ȕ����邩�m�F
		{
			if ( lua_tonumber(L, 1) >= 0 && lua_tonumber(L, 1) <= MAX_CONTROLLERS-1 ) // �擾����p�b�h�̔ԍ����͈͓����m�F
			{
				INT PadNum = (INT) lua_tonumber(L, 1); // �p�b�h�̔ԍ�
				XINPUT_GAMEPAD GamePad = ControllersState[PadNum].state.Gamepad; // ���̂܂܂��ƃN�\�������A�f�b�h�]�[�����g�����������A�Ƃ������Ƃŕʕϐ����B

				if ( UseDeadZone ) // �f�b�h�]�[���̎g�p���I���ɂȂ��Ă���ꍇ�̓f�b�h�]�[����K�p���Ă���switch���ցB
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
				switch ( (int) lua_tonumber(L, 2) ) // �����ɍ��킹�ăA�i���O�̒l��Ԃ�
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
RCEXP int XInputSetDeadZone( lua_State *L ) // �f�b�h�]�[�����g�p���邩�ǂ���
{
	if ( lua_isboolean(L,1) )
		UseDeadZone = lua_toboolean(L,1); // �����̃u�[���A�������̂܂ܓ˂�����
	if ( lua_isnumber(L,2) )
	{
		if ( lua_tonumber(L,2) >0 && lua_tonumber(L,2) < FLOAT(0x7FFF) ) // �Q�ڂ̈����̓f�b�h�]�[����臒l�̐ݒ�Ɏg��
			InputDeadZone = (SHORT) lua_tonumber(L,2); // 0�ȏォ�ő�l�ȉ��̏ꍇ�����ݒ�\
		else if ( lua_tonumber(L,2) < 0 ) 
			InputDeadZone = (SHORT) DEFAULT_DEADZONE; // ���̐�������ƃf�t�H���g�̒l�ɂȂ�܂���
	}
	lua_pushboolean(L, UseDeadZone);
	return 1;
}
RCEXP int XInputSetVibration( lua_State *L ) // �U���̐ݒ�
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
RCEXP int UpdateControllerStateToLua( lua_State *L ) //�R���g���[���[�̏�Ԃ��擾�i�X�V�j����
{
	if ( UpdateControllerState() == S_OK )
		lua_pushnumber( L, 1 );
	else
		lua_pushnumber( L, 0 );
    return 1;
}
RCEXP int XInputSetActiveOnly( lua_State *L ) // �E�B���h�E���A�N�e�B�u�ȂƂ������擾�����邩�ǂ����B
{
	if ( lua_isboolean(L,1) )
		ActiveOnly = lua_toboolean(L,1); // �����̃u�[���A�������̂܂ܓ˂�����
	lua_pushboolean(L, ActiveOnly);
	return 1;
}

RCEXP int GetXInputRawButton( lua_State *L ) // �r�b�g�}�X�N�|����O�̐��̃{�^������Ԃ� ����m�F�p
{
	//UpdateControllerState(); //�p�b�h���̍X�V
	lua_pushnumber( L, (lua_Number) ControllersState[1].state.Gamepad.wButtons );
	return 1;
}
RCEXP int GetXInputPacketNumber( lua_State *L ) // dwPacketNumber��Ԃ�����
{
	//UpdateControllerState(); //�p�b�h���̍X�V
	lua_pushnumber( L, (lua_Number) ControllersState[1].state.dwPacketNumber );
	return 1;
}
RCEXP int GetXInputConnected( lua_State *L ) // �R���g���[���[�̐ڑ���Ԃ̎擾
{
	if ( lua_isnumber(L, 1) )
	{
		if ( lua_tonumber(L, 1) >= 0 && lua_tonumber(L, 1) <= MAX_CONTROLLERS-1 )
			lua_pushnumber( L, (lua_Number) ControllersState[(INT)lua_tonumber(L,1)].bConnected );
	}
	return 1;
}

DLLEXP int RegFunc( lua_State *L ) //�֐���Lua�ɓo�^����
{
	//lua_open()�͂���Ȃ����ۂ��H
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

bool GetCurrentProcessWindowActive() // �Ăяo�����̃v���Z�X�i��RC�j�̃E�B���h�E���A�N�e�B�u���ǂ������擾����֐��B
{
	if ( ActiveOnly ) // ��A�N�e�B�u�ł��擾������ݒ�̂Ƃ��́A�����I��true�Ԃ�����N�\�d�l
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
XIGetStatePtr InitXInputGetStateEx() // XInputGetStateEx���������邽�߂̊֐�
{
	static XIGetStatePtr XInputGetStateEx; // �֐��ւ̃|�C���^
	if ( XInputGetStateEx == NULL)
	{
		HMODULE XInputLib = LoadLibrary( "xinput1_3.dll" );	
		if (XInputLib == NULL)
		{
			XInputLib = LoadLibrary( "xinput1_4.dll" ); // �ǂ߂Ȃ�������ʂ̂������Ă݂�
			if (XInputLib == NULL )
				MessageBox( NULL, "XInput��DLL���ǂݍ��߂Ȃ�����(�L�E�ցE�M)", "Error", MB_OK ); // ����ł��ǂ߂Ȃ�������x���o���Ƃ�
		}
		if (XInputLib != NULL )
			XInputGetStateEx = (XIGetStatePtr) GetProcAddress( XInputLib, (LPCSTR)100 ); //XInputGetStateEx�̏��� 100�͏����Ƃ������̂炵��
		else
			XInputGetStateEx = (XIGetStatePtr) &XInputGetState; // DLL���ǂ߂Ȃ������ꍇ�́A�ʏ�̊֐����g�p����B�������K�C�h�{�^���̎擾�͂ł��Ȃ��B
	}
	return XInputGetStateEx;
}

HRESULT UpdateControllerState() // �R���g���[���[�̏�Ԃ��擾�i�X�V�j����
{
	XIGetStatePtr XInputGetStateEx = InitXInputGetStateEx(); //�֐��ւ̃|�C���^
	std::copy( ControllersState.begin(), ControllersState.end(), ControllersStatePrev.begin() ); //�R���g���[���[�̏�Ԃ̔z����R�s�[
	DWORD dwResult;
	for( DWORD i = 0; i < MAX_CONTROLLERS; i++ )
	{
		dwResult = XInputGetStateEx( i, &ControllersState[i].state );

		if( dwResult == ERROR_SUCCESS ) //�G���[�f�������ڑ��Ƃ݂Ȃ��H
			ControllersState[i].bConnected = true;
		else
			ControllersState[i].bConnected = false;
	}
    return S_OK;
}