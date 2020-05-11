#include "Utils/MemoryMgr.h"
#include "Utils/Patterns.h"


void OnInitializeHook()
{
	using namespace Memory::VP;
	using namespace hook;

	// Don't sleep in a frame limiter
	// This affects Wonderful 101, Metal Gear Rising and who knows how many more games from Platinum Games
	auto sleep = pattern( "85 D2 7E 03" ).count(1);
	if ( sleep.size() == 1 )
	{
		Patch<uint8_t>( sleep.get_first<void>( 2 ), 0xEB ); // jle -> jmp
	}
}
