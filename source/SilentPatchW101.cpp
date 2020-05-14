#include "Utils/MemoryMgr.h"
#include "Utils/Patterns.h"

namespace FixedFL
{
	static int64_t* lastFrameProcessTime;

	static int64_t (*orgGetProcessTime)();
	static int64_t GetProcessTime_FromLastFrame()
	{
		return orgGetProcessTime() - *lastFrameProcessTime;
	}
}


namespace AltEnterWindow
{
	static bool IsKeyJustDown(const uint32_t* keyArray, uint32_t key)
	{
		// No point in figuring out the exact meaning...
		return (keyArray[(key >> 5) + 6] & (0x80000000 >> (key & 0x1F))) != 0;
	}

	static bool IsKeyDown(const uint32_t* keyArray, uint32_t key)
	{
		// No point in figuring out the exact meaning...
		return (keyArray[key >> 5] & (0x80000000 >> (key & 0x1F))) != 0;
	}

	static BOOL __fastcall IsAltEnterDown_Wrap(const uint32_t* keyArray, void*, uint32_t /*key*/)
	{
		return IsKeyDown(keyArray, 0x9D) && IsKeyJustDown(keyArray, 0xA); // Alt + Enter
	}
}


void OnInitializeHook()
{
	using namespace Memory::VP;
	using namespace hook;

	// TODO: This entire project needs transactional patterns!

	// Don't sleep in a frame limiter
	// This affects Wonderful 101, Metal Gear Rising and who knows how many more games from Platinum Games
	auto sleep = pattern( "85 D2 7E 03" ).count(1);
	if ( sleep.size() == 1 )
	{
		Patch<uint8_t>( sleep.get_first<void>( 2 ), 0xEB ); // jle -> jmp
	}


	// Fixed frame limiter calculations
	// Now operating on the difference between the current and last frame instead of process time
	// so calculations don't lose precision over time
	{
		using namespace FixedFL;
	
		auto lastFrameTime = pattern( "A3 ? ? ? ? 89 15 ? ? ? ? E8 ? ? ? ? 8B C8" );
		auto getTimeSinceLastFrame = pattern( "E8 ? ? ? ? 8B C8 E8 ? ? ? ? F2 0F 11 44 24 08" ); // 3 hits, but we can ignore the last one
		auto subtractLastFrameTime = pattern( "2B 05 ? ? ? ? 8B 0D ? ? ? ? 3B C8" ); // 2 hits

		if ( lastFrameTime.count(1).size() == 1 && getTimeSinceLastFrame.count(2).size() == 2 && subtractLastFrameTime.count(2).size() == 2 )
		{
			lastFrameProcessTime = *lastFrameTime.get_first<int64_t*>( 1 );

			ReadCall( getTimeSinceLastFrame.get(0).get<void>(), orgGetProcessTime );
			getTimeSinceLastFrame.for_each_result([]( pattern_match match ) {
				InjectHook( match.get<void>(), GetProcessTime_FromLastFrame );
			});

			subtractLastFrameTime.for_each_result([]( pattern_match match ) {
				Nop( match.get<void>(), 6 );
			});
		}
	}


	// Remap Escape enabling windowed mode to Alt+Enter
	{
		using namespace AltEnterWindow;

		auto isEscapeDown = pattern( "E8 ? ? ? ? 85 C0 74 4E A1 ? ? ? ? 83 C0 FD" );
		if ( isEscapeDown.count(1).size() == 1 )
		{
			InjectHook( isEscapeDown.get_first<void>(), IsAltEnterDown_Wrap );
		}
	}
}
