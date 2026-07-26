// Implements independent single-client ownership with specification identities.
#include "firmware/application/ownership.hpp"

namespace firmware::application {

bool Ownership::claim_file(const HostIdentity& host) {
    if (file_owner_.has_value() && !is_file_owner(host)) {
        return false;
    }
    file_owner_ = FileOwner{host.transport, host.slot};
    return true;
}

bool Ownership::is_file_owner(const HostIdentity& host) const {
    return file_owner_.has_value() && file_owner_->transport == host.transport && file_owner_->slot == host.slot;
}

void Ownership::release_file() {
    file_owner_.reset();
}

bool Ownership::has_file_owner() const {
    return file_owner_.has_value();
}

bool Ownership::claim_play(const HostIdentity& host) {
    if (play_owner_.has_value() && !is_play_owner(host)) {
        return false;
    }
    play_owner_ = host;
    return true;
}

bool Ownership::is_play_owner(const HostIdentity& host) const {
    return play_owner_.has_value() && *play_owner_ == host;
}

void Ownership::release_play() {
    play_owner_.reset();
}

bool Ownership::has_play_owner() const {
    return play_owner_.has_value();
}

void Ownership::transport_disconnected(const HostIdentity& host) {
    if (is_play_owner(host)) {
        release_play();
    }
}

}  // namespace firmware::application
