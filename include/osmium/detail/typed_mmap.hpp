#ifndef OSMIUM_DETAIL_TYPED_MMAP_HPP
#define OSMIUM_DETAIL_TYPED_MMAP_HPP

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

#include <system_error>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace osmium {

    namespace detail {

        /**
         * This is a helper class for working with memory mapped files and
         * anonymous shared memory. It wraps the necessary system calls
         * adding:
         * - error checking: all functions throw exceptions where needed
         * - internal casts and size calculations allow use with user defined
         *   type T instead of void*
         *
         * This class only contains static functions. It should never be
         * instantiated.
         */
        template <typename T>
        class typed_mmap {

        public:

            /**
             * Create anonymous private memory mapping with enough space for size
             * objects of type T.
             *
             * Note that no constructor is called for any of the objects in this memory!
             *
             * @tparam T Type of objects we want to store in this memory
             * @param size Number of objects of type T that should fit into this memory
             * @return Pointer to mapped memory
             * @exception std::system_error If mmap(2) failed
             */
            static T* map(size_t size) {
                void* addr = ::mmap(nullptr, sizeof(T) * size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
                if (addr == MAP_FAILED) {
                    throw std::system_error(errno, std::system_category(), "mmap failed");
                }
#pragma GCC diagnostic pop
                return reinterpret_cast<T*>(addr);
            }

            /**
             * Create shared memory mapping of a file with enough space for size
             * objects of type T. The file must already have at least the
             * required size.
             *
             * Note that no constructor is called for any of the objects in this memory!
             *
             * @tparam T Type of objects we want to store in this memory
             * @param size Number of objects of type T that should fit into this memory
             * @param fd File descriptor
             * @param write True if data should be writable
             * @return Pointer to mapped memory
             * @exception std::system_error If mmap(2) failed
             */
            static T* map(size_t size, int fd, bool write = false) {
                int prot = PROT_READ;
                if (write) {
                    prot |= PROT_WRITE;
                }
                void* addr = ::mmap(nullptr, sizeof(T) * size, prot, MAP_SHARED, fd, 0);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
                if (addr == MAP_FAILED) {
                    throw std::system_error(errno, std::system_category(), "mmap failed");
                }
#pragma GCC diagnostic pop
                return reinterpret_cast<T*>(addr);
            }

// mremap(2) is only available on linux systems
#ifdef __linux__
            /**
             * Grow memory mapping created with map().
             *
             * Note that no constructor is called for any of the objects in this memory!
             *
             * @tparam T Type of objects stored in this memory
             * @param data Pointer to current mapping (as returned by typed_mmap())
             * @param old_size Number of objects currently stored in this memory
             * @param new_size Number of objects we want to have space for
             * @exception std::system_error If mremap(2) call failed
             */
            static T* remap(T* data, size_t old_size, size_t new_size) {
                void* addr = ::mremap(reinterpret_cast<void*>(data), sizeof(T) * old_size, sizeof(T) * new_size, MREMAP_MAYMOVE);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
                if (addr == MAP_FAILED) {
                    throw std::system_error(errno, std::system_category(), "mremap failed");
                }
#pragma GCC diagnostic pop
                return reinterpret_cast<T*>(addr);
            }
#endif

            /**
             * Release memory from map() call.
             *
             * Note that no destructor is called for the objects in this memory!
             *
             * @tparam T Type of objects stored in this memory
             * @param data Pointer to the data
             * @param size Number of objects of type T stored
             * @exception std::system_error If munmap(2) call failed
             */
            static void unmap(T* data, size_t size) {
                if (::munmap(reinterpret_cast<void*>(data), sizeof(T) * size) != 0) {
                    throw std::system_error(errno, std::system_category(), "munmap failed");
                }
            }

            /**
             * Get number of objects of type T that would fit into a file.
             *
             * @tparam T Type of objects stored in this file
             * @param fd File descriptor
             * @return Number of objects of type T in this file
             * @exception std::system_error If fstat(2) call failed
             * @exception std::length_error If size of the file isn't a multiple of sizeof(T)
             */
            static size_t file_size(int fd) {
                struct stat s;
                if (fstat(fd, &s) < 0) {
                    throw std::system_error(errno, std::system_category(), "fstat failed");
                }
                if (s.st_size % sizeof(T) != 0) {
                    throw std::length_error("file size has to be multiple of object size");
                }
                return s.st_size / sizeof(T);
            }

            /**
             * Grow file so there is enough space for at least new_size objects
             * of type T. If the file is large enough already, nothing is done.
             * The file is never shrunk.
             *
             * @tparam T Type of objects stored in this file
             * @param new_size Number of objects of type T that should fit into this file
             * @param fd File descriptor
             * @exception std::system_error If ftruncate(2) call failed
             */
            static void grow_file(size_t new_size, int fd) {
                if (file_size(fd) < new_size) {
                    if (::ftruncate(fd, sizeof(T) * new_size) < 0) {
                        throw std::system_error(errno, std::system_category(), "ftruncate failed");
                    }
                }
            }

        }; // class typed_mmap

    } // namespace detail

} // namespace osmium

#endif // OSMIUM_DETAIL_TYPED_MMAP_HPP
