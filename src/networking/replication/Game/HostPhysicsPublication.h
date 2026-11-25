#pragma once

#include <cstdint>
#include <map>

namespace netgame::publication
{
	// Observes host sleep state and returns true only on an awake-to-asleep transition.
	// The first observation initializes the object's state without publishing an event.
	inline bool ObserveHostSleepTransition(std::map<uint32_t, bool>& sleepingByObject,
		uint32_t objectId, bool active)
	{
		const bool sleeping = !active;
		auto previous = sleepingByObject.find(objectId);
		const bool transitionedToSleep = sleeping && previous != sleepingByObject.end() && !previous->second;
		sleepingByObject[objectId] = sleeping;
		return transitionedToSleep;
	}
}
