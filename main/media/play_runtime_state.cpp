// Owns the process-wide streamed-play session instance.
#include "play_runtime_state.hpp"

namespace firmware::target {
namespace {
firmware::application::PlaySession play_session;
}  // namespace

firmware::application::PlaySession& shared_play_session() {
    return play_session;
}

}  // namespace firmware::target
