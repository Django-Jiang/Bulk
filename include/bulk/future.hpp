#pragma once

#include <bulk/world.hpp>
#include <memory>

/**
 * \file future.hpp
 *
 * This header defines a future object, which is used to refer to remote values
 * that are not yet known.
 */

namespace bulk {

/**
 * Represents a value that will become known in the upcoming superstep
 */
template <typename T>
class future {
  public:
    /**
     * Initialize the future.
     */
    future(bulk::world& world) : world_(world) {
        buffer_ = std::make_unique<T>();
    }

    /**
     * Deconstruct the future.
     */
    ~future() {}

    future(const future<T>& other) = delete;
    void operator=(const future<T>& other) = delete;

    future(future<T>&& other) : world_(other.world_) {
        *this = std::move(other);
    }

    /**
     * Move a future.
     */
    void operator=(future<T>&& other) {
        buffer_ = std::move(other.buffer_);
    }

    /**
     * Get a reference to the value held by the future.
     *
     * \returns a reference to the value
     */
    T& value() { return *buffer_.get(); }

    /**
     * Get a reference to the world of the future.
     *
     * \returns a reference to the world of the future
     */
    bulk::world& world() { return world_; }

  private:
    std::unique_ptr<T> buffer_;
    bulk::world& world_;
};

} // namespace bulk
