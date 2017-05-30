#pragma once

/**
 * \file coarray.hpp
 *
 * This header provides an implementation of a coarray, which is syntactically
 * similar to the coarrays defined in Co-array Fortran.
 */

#include <cstddef>
#include <vector>

#include "array.hpp"
#include "communication.hpp"

namespace bulk {

/**
 * Distributed array with easy element access, loosely based on the behaviour
 * of Co-Array Fortran.
 *
 * Coarrays provide a convenient way to share data across processors. Instead
 * of manually sending and receiving data elements, coarrays model distributed
 * data as a 2-dimensional array, where the first dimension is over the
 * processors, and the second dimension is over local 1-dimensional array
 * indices.
 *
 * It can be used as follows:
 * \code{.cpp}
 *     auto xs = bulk::coarray<int>(world, 10);
 *     // set the 5th element on the 1st processor to 4
 *     xs(1)[5] = 4;
 *     // set the 3rd element on the local processor to 2
 *     xs[3] = 2;
 * \endcode
 */
template <typename T>
class coarray {
  public:
    struct slice {
        int first;
        int last;
    };

    class slice_writer {
      public:
        /**
         * Assign a value to a slice of a remote image.
         *
         * \param value the new value of each element in the slice
         */
        void operator=(T value) {
            for (int i = s_.first; i < s_.last; ++i) {
                parent_.put(t_, i, value);
            }
        }

        /**
         * Assign values to a slice of a remote image.
         *
         * \param values the new values of in the slice
         *
         * Note: when sizes don't match, the rule is
         *     array[{2,10}] = {3,2,4};
         * will fill the array as
         *     {x,x,3,2,4,3,2,4,3,2,x,x};
         */
        void operator=(const std::vector<T>& values) {
            parent_.put(t_, s_, values);
        }

        auto get() {
            // FIXME implement
        }

      private:
        friend coarray<T>;

        slice_writer(bulk::world& world, coarray<T>& parent, int t, slice s)
            : world_(world), parent_(parent), t_(t), s_(s) {}

        bulk::world& world_;
        coarray<T>& parent_;
        int t_;
        slice s_;
    };

    class writer {
      public:
        /**
         * Assign a value to a remote image element.
         *
         * \param value the new value of the element
         */
        void operator=(T value) { parent_.put(t_, i_, value); }

        auto get() { return parent_.get(t_, i_); }

      private:
        friend coarray<T>;

        writer(bulk::world& world, coarray<T>& parent, int t, int i)
            : world_(world), parent_(parent), t_(t), i_(i) {}

        bulk::world& world_;
        coarray<T>& parent_;
        int t_;
        int i_;
    };

    class image {
      public:
        image(bulk::world& world, coarray<T>& parent, int t)
            : world_(world), parent_(parent), t_(t) {}

        /**
         * Obtain a writer to the remote element.
         *
         * \param i the index of the remote element
         *
         * \returns an object that can be used to write to the remote element
         */
        writer operator[](int i) { return writer(world_, parent_, t_, i); }

        slice_writer operator[](slice s) {
            return slice_writer(world_, parent_, t_, s);
        }

      private:
        bulk::world& world_;
        coarray<T>& parent_;
        int t_;
    };

    /**
     * Initialize and registers the coarray with the world
     *
     * \param world the distributed layer in which the array is defined.
     * \param local_size the size of the local array
     */
    template <
        typename = std::enable_if_t<std::is_trivially_constructible<T>::value>>
    coarray(bulk::world& world, int local_size)
        : world_(world), data_(world_, local_size) {}

    /**
     * Initialize and registers the coarray with the world
     *
     * \param world the distributed layer in which the array is defined.
     * \param local_size the size of the local array
     * \param default_value the initial value of each local element
     */
    coarray(bulk::world& world, int local_size, T default_value)
        : world_(world), data_(world_, local_size) {
        for (int i = 0; i < local_size; ++i) {
            data_[i] = default_value;
        }
    }

    coarray(coarray&& other) : world_(other.world()), data_(std::move(data_)) {}

    /**
     * Retrieve the coarray image with index t
     *
     * \param t index of the target image
     *
     * \returns the coarray image with index t
     */
    image operator()(int t) { return image(world_, *this, t); }

    /**
     * Access the `i`th element of the local coarray image
     *
     * \param i index of the element
     * \returns reference to the i-th element of the local image
     */
    T& operator[](int i) { return data_[i]; }
    const T& operator[](int i) const { return data_[i]; }

    /**
     * Get a reference to the world of the coarray.
     *
     * \returns a reference to the world of the coarray
     */
    bulk::world& world() { return world_; }

    /**
     * Get an iterator to the beginning of the local image of the coarray.
     *
     * \returns a pointer to the first element of the local data.
     */
    T* begin() { return data_.begin(); }

    /**
     * Get an iterator to the end of the local image of the coarray.
     *
     * \returns a pointer beyond the last element of the local data.
     */
    T* end() { return data_.end(); }

    /**
     * Put the value `value` into element `idx` on processor `t`.
     */
    void put(int t, int idx, T value) { data_.put(t, &value, idx, 1); }

    /**
     * Put a range of data in a remote image.
     *
     * Note: when sizes don't match, the rule is
     *     array[{2,10}] = {3,2,4};
     * will fill the array as
     *     {x,x,3,2,4,3,2,4,3,2,x,x};
     */
    void put(int processor, slice s, const std::vector<T> values) {
        for (int i = s.first;; i += values.size()) {
            int remaining = s.last - i;
            if (remaining < 0)
                break;
            if (remaining > (int)values.size())
                remaining = values.size();
            data_.put(processor, values.data(), i, (int)remaining);
        }
    }

    /**
     * Put a range of data in a remote image.
     */
    template <typename FwdIterator>
    void put(int processor, FwdIterator first, FwdIterator last,
             int offset = 0) {
        data_.put(processor, first, last, offset);
    }

    /**
     * Get a future to the value of element `idx` on processor `t`.
     */
    auto get(int t, int idx) { return data_.get(t, idx, 1); }

    /**
     * Get the size of the coarray.
     */
    std::size_t size() const { return data_.size(); }

    /**
     * Check if the coarray is empty.
     */
    bool empty() const { return size() == 0; }

  private:
    friend image;
    friend writer;

    bulk::world& world_;
    array<T> data_;

    array<T>& data() { return data_; }
};

} // namespace bulk