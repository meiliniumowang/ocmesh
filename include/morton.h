// -*- C++ -*-
/*
 * Copyright 2014 Nicola Gigante
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef OCMESH_MORTON_H
#define OCMESH_MORTON_H

#include <cassert>
#include <cstdint>
#include <cmath>
#include <glm/glm.hpp>

#include <std14/utility>

namespace ocmesh {
namespace details {
   
    /*
     * Morton encoding calculation
     *
     * Voxels in the linear octree are stored in a specific order, that is
     * equivalent to a pre-order traversal of the tree, which spatially
     * corresponds to a space-filling path that is often called Z-order or,
     * recursively, Morton order.
     *
     * The morton code of a 3D coordinate vector is obtained by interleaving
     * the bits of the coordinates, e.g. if the coordinates are:
     *
     * x = xxxx, y = yyyy, z = zzzz
     *
     * the Morton code is zyxzyxzyx
     *
     * Note that the relative order of each coordinate in the interleave is
     * arbitrary, and corresponds to the spatial order that we want to follow.
     *
     * The order matters, as it has effects on the optimal order of traversal of
     * the octree.
     *
     * This order is hardcoded once and for all in the 
     * following enum declaration:
     */
    enum class coordinate_t : uint8_t {
        x = 0,
        y = 1,
        z = 2
    };
    
    
    /* 
     * The interleaving can be achieved with a constant number of
     * magic bitwise operations. The fastest implementation available is
     * based on the pre-computation of a table of bitmasks for 8 bit
     * values, that are composed to interleave our 16bits words.
     *
     * Note that this is faster than computing it on the fly
     * only if we suppose that a lot of morton codes will be calculated in
     * morton order, as to efficiently use the cache lines for the table data.
     * This is the case in our code.
     *
     * Thanks to the C++11 constexpr keyword, we can tell the compiler
     * to precompute the table and statically put the results directly
     * as data into the executable.
     *
     * The interleave() function is what computes the single elements
     * of the table. The code in the morton() function then uses the
     * facilities from static_table.h to fill the tables.
     *
     * C++11 constexpr functions must be written in purely functional
     * style, so the implementation of split() below is the
     * constexpr-enabled version of this code, which would be usable
     * as is with C++14 constexpr:
     *
     *    constexpr uint32_t interleave(uint8_t byte)
     *    {
     *       uint32_t x = byte;
     *
     *       x = (x | x << 16) & 0xFF0000FF;
     *       x = (x | x <<  8) & 0x0F00F00F;
     *       x = (x | x <<  4) & 0xC30C30C3;
     *       x = (x | x <<  2) & 0x49249249;
     *
     *       return x;
     *    }
     *
     */
    constexpr uint32_t masks[] = {
        0,
        0x49249249, 0xC30C30C3,
        0x0F00F00F, 0xFF0000FF
    };

    constexpr uint32_t interleave(uint32_t x, int level) {
        return level == 0 ? x :
            interleave((x | x << (1 << level)) & masks[level], level - 1);
    }

    constexpr uint32_t interleave(uint8_t x) {
        return interleave(uint32_t(x), 4);
    }
    
    /*
     * This function computes the complete interleaving of a coordinate 
     * component. The component is shifted according to the template argument.
     */
    template<coordinate_t S, size_t ...Idx>
    uint64_t morton(uint32_t value, std14::index_sequence<Idx...>)
    {
        static constexpr
        uint32_t table[] = { interleave(uint8_t(Idx)) << uint8_t(S) ... };
        
        uint8_t low    = value       & 0xFF,
                middle = value >> 8  & 0xFF,
                high   = value >> 16 & 0xFF;
        
        return uint64_t(table[high])   << 48 |
               uint64_t(table[middle]) << 24 |
               uint64_t(table[low]);
    }
    
    template<coordinate_t S = coordinate_t::x>
    uint64_t morton(uint32_t value) {
        return morton<S>(value, std14::make_index_sequence<256>());
    }
    
    /*
     * Note that into a 64bit words, one can encode a 3D vector of 21 bits for
     * each component. In the voxel class in voxel.h, coordinate components
     * have a smaller width, for other reasons, but the code in this section
     * is general and this function is independent of the choices
     * made in voxel.h, so it packs all the 21 bits of each coordinate.
     */
    uint64_t morton(glm::u32vec3 coordinates)
    {
        return morton< coordinate_t::x >(coordinates.x) |
               morton< coordinate_t::y >(coordinates.y) |
               morton< coordinate_t::z >(coordinates.z);
    }

    /*
     * Unpacks one component of a morton encoded 3D vector.
     */
    template<coordinate_t S = coordinate_t::x>
    uint32_t unmorton(uint64_t x)
    {
        x = x >> uint8_t(S); // Shift to select the component
        
        x =  x              & 0x9249249249249249;
        x = (x | (x >>  2)) & 0x30C30C30C30C30C3;
        x = (x | (x >>  4)) & 0xF00F00F00F00F00F;
        x = (x | (x >>  8)) & 0x00FF0000FF0000FF;
        x = (x | (x >> 16)) & 0xFFFF00000000FFFF;
        x = (x | (x >> 32)) & 0x00000000FFFFFFFF;
        
        return uint32_t(x); // The result surely fits into 32 bits.
    }

    /*
     * Unpacks a morton encoded 3D coordinate vector
     */
    glm::u32vec3 unmorton(uint64_t m)
    {
        uint32_t x = unmorton< coordinate_t::x >(m),
                 y = unmorton< coordinate_t::y >(m),
                 z = unmorton< coordinate_t::z >(m);
        
        return { x, y, z };
    }

} // namespace details

} // namespace ocmesh


#endif
