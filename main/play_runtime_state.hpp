// Declares the process-wide streamed-play session shared by host and controller tasks.
#pragma once

#include "firmware/application/play_session.hpp"

namespace firmware::target {

// Returns the single prepared play session used by all transport adapters.
firmware::application::PlaySession& shared_play_session();

}  // namespace firmware::target
