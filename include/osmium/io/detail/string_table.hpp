#ifndef OSMIUM_IO_DETAIL_STRING_TABLE_HPP
#define OSMIUM_IO_DETAIL_STRING_TABLE_HPP

/*

This file is part of Osmium (http://osmcode.org/libosmium).

Copyright 2013-2015 Jochen Topf <jochen@topf.org> and others (see README).

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

#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <list>
#include <map>
#include <string>

#include <osmium/io/detail/pbf.hpp>

namespace osmium {

    namespace io {

        namespace detail {

            /**
             * class StringStore
             *
             * Storage of lots of strings (const char *). Memory is allocated in chunks.
             * If a string is added and there is no space in the current chunk, a new
             * chunk will be allocated. Strings added to the store must not be larger
             * than the chunk size.
             *
             * All memory is released when the destructor is called. There is no other way
             * to release all or part of the memory.
             *
             */
            class StringStore {

                size_t m_chunk_size;

                std::list<std::string> m_chunks;

                void add_chunk() {
                    m_chunks.push_front(std::string());
                    m_chunks.front().reserve(m_chunk_size);
                }

            public:

                StringStore(size_t chunk_size) :
                    m_chunk_size(chunk_size),
                    m_chunks() {
                    add_chunk();
                }

                void clear() noexcept {
                    m_chunks.erase(std::next(m_chunks.begin()), m_chunks.end());
                    m_chunks.front().clear();
                }

                /**
                 * Add a null terminated string to the store. This will
                 * automatically get more memory if we are out.
                 * Returns a pointer to the copy of the string we have
                 * allocated.
                 */
                const char* add(const char* string) {
                    size_t len = std::strlen(string) + 1;

                    assert(len <= m_chunk_size);

                    size_t chunk_len = m_chunks.front().size();
                    if (chunk_len + len > m_chunks.front().capacity()) {
                        add_chunk();
                        chunk_len = 0;
                    }

                    m_chunks.front().append(string);
                    m_chunks.front().append(1, '\0');

                    return m_chunks.front().c_str() + chunk_len;
                }

                class const_iterator : public std::iterator<std::forward_iterator_tag, const char*> {

                    typedef std::list<std::string>::const_iterator it_type;
                    it_type m_it;
                    const it_type m_last;
                    const char* m_pos;

                public:

                    const_iterator(it_type it, it_type last) :
                        m_it(it),
                        m_last(last),
                        m_pos(it == last ? nullptr : m_it->c_str()) {
                    }

                    const_iterator& operator++() {
                        assert(m_it != m_last);
                        auto last_pos = m_it->c_str() + m_it->size();
                        while (m_pos != last_pos && *m_pos) ++m_pos;
                        if (m_pos != last_pos) ++m_pos;
                        if (m_pos == last_pos) {
                            ++m_it;
                            if (m_it != m_last) {
                                m_pos = m_it->c_str();
                            } else {
                                m_pos = nullptr;
                            }
                        }
                        return *this;
                    }

                    const_iterator operator++(int) {
                        const_iterator tmp(*this);
                        operator++();
                        return tmp;
                    }

                    bool operator==(const const_iterator& rhs) const {
                        return m_it == rhs.m_it && m_pos == rhs.m_pos;
                    }

                    bool operator!=(const const_iterator& rhs) const {
                        return !(*this == rhs);
                    }

                    const char* operator*() const {
                        assert(m_it != m_last);
                        assert(m_pos != nullptr);
                        return m_pos;
                    }

                }; // class const_iterator

                const_iterator begin() const {
                    if (m_chunks.front().empty()) {
                        return end();
                    }
                    return const_iterator(m_chunks.begin(), m_chunks.end());
                }

                const_iterator end() const {
                    return const_iterator(m_chunks.end(), m_chunks.end());
                }

                // These functions get you some idea how much memory was
                // used.
                size_t get_chunk_size() const noexcept {
                    return m_chunk_size;
                }

                size_t get_chunk_count() const noexcept {
                    return m_chunks.size();
                }

                size_t get_used_bytes_in_last_chunk() const noexcept {
                    return m_chunks.front().size();
                }

            }; // class StringStore

            struct StrComp {

                bool operator()(const char* lhs, const char* rhs) const {
                    return strcmp(lhs, rhs) < 0;
                }

            }; // struct StrComp

            class StringTable {

                // This is the maximum number of entries in a string table.
                // This should never be reached in practice but we better
                // make sure it doesn't. If we had max_uncompressed_blob_size
                // many entries, we are sure they would never fit into a PBF
                // Blob.
                static constexpr const uint32_t max_entries = max_uncompressed_blob_size;

                StringStore m_strings;
                std::map<const char*, size_t, StrComp> m_index;
                uint32_t m_size;

            public:

                StringTable() :
                    m_strings(1024 * 1024),
                    m_index(),
                    m_size(0) {
                    m_strings.add("");
                }

                void clear() {
                    m_strings.clear();
                    m_index.clear();
                    m_size = 0;
                    m_strings.add("");
                }

                uint32_t size() const noexcept {
                    return m_size + 1;
                }

                uint32_t add(const char* s) {
                    auto f = m_index.find(s);
                    if (f != m_index.end()) {
                        return uint32_t(f->second);
                    }

                    const char* cs = m_strings.add(s);
                    m_index[cs] = ++m_size;

                    if (m_size > max_entries) {
                        throw osmium::pbf_error("string table has too many entries");
                    }

                    return m_size;
                }

                StringStore::const_iterator begin() const {
                    return m_strings.begin();
                }

                StringStore::const_iterator end() const {
                    return m_strings.end();
                }

            }; // class StringTable

        } // namespace detail

    } // namespace io

} // namespace osmium

#endif // OSMIUM_IO_DETAIL_STRING_TABLE_HPP