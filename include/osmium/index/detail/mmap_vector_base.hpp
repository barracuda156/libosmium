#ifndef OSMIUM_INDEX_DETAIL_MMAP_VECTOR_BASE_HPP
#define OSMIUM_INDEX_DETAIL_MMAP_VECTOR_BASE_HPP

/*

This file is part of Osmium (http://osmcode.org/libosmium).

Copyright 2013-2016 Jochen Topf <jochen@topf.org> and others (see README).

Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

*/

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <new> // IWYU pragma: keep
#include <stdexcept>

#include <osmium/index/index.hpp>
#include <osmium/util/memory_mapping.hpp>

namespace osmium {

    namespace detail {

        constexpr size_t mmap_vector_size_increment = 1024 * 1024;

        /**
         * This is a base class for implementing classes that look like
         * STL vector but use mmap internally. Do not use this class itself,
         * use the derived classes mmap_vector_anon or mmap_vector_file.
         */
        template <typename T>
        class mmap_vector_base {

        protected:

            size_t m_size;
            osmium::util::TypedMemoryMapping<T> m_mapping;

        public:

            mmap_vector_base(int fd, size_t capacity, size_t size = 0) :
                m_size(size),
                m_mapping(capacity, osmium::util::MemoryMapping::mapping_mode::write_shared, fd) {
                assert(size <= capacity);
                std::fill(data() + size, data() + capacity, osmium::index::empty_value<T>());
                shrink_to_fit();
            }

            explicit mmap_vector_base(size_t capacity = mmap_vector_size_increment) :
                m_size(0),
                m_mapping(capacity) {
                std::fill_n(data(), capacity, osmium::index::empty_value<T>());
            }

            ~mmap_vector_base() noexcept = default;

            using value_type      = T;
            using pointer         = value_type*;
            using const_pointer   = const value_type*;
            using reference       = value_type&;
            using const_reference = const value_type&;
            using iterator        = value_type*;
            using const_iterator  = const value_type*;

            void close() {
                m_mapping.unmap();
            }

            size_t capacity() const noexcept {
                return m_mapping.size();
            }

            size_t size() const noexcept {
                return m_size;
            }

            bool empty() const noexcept {
                return m_size == 0;
            }

            const_pointer data() const {
                return m_mapping.begin();
            }

            pointer data() {
                return m_mapping.begin();
            }

            const_reference operator[](size_t n) const {
                assert(n < m_size);
                return data()[n];
            }

            reference operator[](size_t n) {
                assert(n < m_size);
                return data()[n];
            }

            value_type at(size_t n) const {
                if (n >= m_size) {
                    throw std::out_of_range{"out of range"};
                }
                return data()[n];
            }

            void clear() noexcept {
                m_size = 0;
            }

            void shrink_to_fit() {
                while (m_size > 0 && data()[m_size - 1] == osmium::index::empty_value<value_type>()) {
                    --m_size;
                }
            }

            void push_back(const_reference value) {
                resize(m_size + 1);
                data()[m_size - 1] = value;
            }

            void reserve(size_t new_capacity) {
                if (new_capacity > capacity()) {
                    const size_t old_capacity = capacity();
                    m_mapping.resize(new_capacity);
                    std::fill(data() + old_capacity, data() + new_capacity, osmium::index::empty_value<value_type>());
                }
            }

            void resize(size_t new_size) {
                if (new_size > capacity()) {
                    reserve(new_size + osmium::detail::mmap_vector_size_increment);
                }
                m_size = new_size;
            }

            iterator begin() noexcept {
                return data();
            }

            iterator end() noexcept {
                return data() + m_size;
            }

            const_iterator begin() const noexcept {
                return data();
            }

            const_iterator end() const noexcept {
                return data() + m_size;
            }

            const_iterator cbegin() const noexcept {
                return data();
            }

            const_iterator cend() const noexcept {
                return data() + m_size;
            }

        }; // class mmap_vector_base

    } // namespace detail

} // namespace osmium

#endif // OSMIUM_INDEX_DETAIL_MMAP_VECTOR_BASE_HPP
