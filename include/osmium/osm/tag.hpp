#ifndef OSMIUM_OSM_TAG_HPP
#define OSMIUM_OSM_TAG_HPP

/*

This file is part of Osmium (http://osmcode.org/osmium).

Copyright 2013 Jochen Topf <jochen@topf.org> and others (see README).

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
#include <cstring>

#include <osmium/memory/collection.hpp>

namespace osmium {

    class Tag : public osmium::memory::detail::ItemHelper {

        Tag(const Tag&) = delete;
        Tag(Tag&&) = delete;

        Tag& operator=(const Tag&) = delete;
        Tag& operator=(Tag&&) = delete;

        template <class TMember>
        friend class osmium::memory::CollectionIterator;

        template <typename T>
        static T* after_null(T* ptr) {
            return std::strchr(ptr, 0) + 1;
        }

        char* next() {
            return after_null(after_null(data()));
        }

        const char* next() const {
            return after_null(after_null(data()));
        }

    public:

        static constexpr item_type collection_type = item_type::tag_list;

        const char* key() const {
            return data();
        }

        const char* value() const {
            return after_null(data());
        }

    }; // class Tag

    class TagList : public osmium::memory::Collection<Tag> {

    public:

        static constexpr osmium::item_type itemtype = osmium::item_type::tag_list;

        TagList() :
            osmium::memory::Collection<Tag>() {
        }

        const char* get_value_by_key(const char* key) const {
            auto result = std::find_if(cbegin(), cend(), [key](const Tag& tag) {
                return !strcmp(tag.key(), key);
            });
            if (result == cend()) {
                return nullptr;
            } else {
                return result->value();
            }
        }

    }; // class TagList

    static_assert(sizeof(TagList) % osmium::memory::align_bytes == 0, "Class osmium::TagList has wrong size to be aligned properly!");

} // namespace osmium

#endif // OSMIUM_OSM_TAG_HPP
