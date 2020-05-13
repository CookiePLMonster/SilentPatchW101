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
}
