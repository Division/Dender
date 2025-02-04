//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard 
//

cbuffer CB1 : register(b0)
{
    // Offset into counter buffer where this list's item count is stored
    uint CounterOffset;

    // A sort key that will end up at the end of the list; to be used to pad
    // lists in LDS (always 2048 items).
    //   Descending:  0x00000000
    //   Ascending:   0xffffffff
    // Also used by the ShouldSwap() function to invert ordering.
    uint NullItem;
}

uint InsertZeroBit(uint Value, uint BitIdx)
{
    uint Mask = BitIdx - 1;
    return (Value & ~Mask) << 1 | (Value & Mask);
}

// Takes Value and widens it by one bit at the location of the bit
// in the mask.  A one is inserted in the space.  OneBitMask must
// have one and only one bit set.
uint InsertOneBit( uint Value, uint OneBitMask )
{
    uint Mask = OneBitMask - 1;
    return (Value & ~Mask) << 1 | (Value & Mask) | OneBitMask;
}

// Determines if two sort keys should be swapped in the list.  NullItem is
// either 0 or 0xffffffff.  XOR with the NullItem will either invert the bits
// (effectively a negation) or leave the bits alone.  When the the NullItem is
// 0, we are sorting descending, so when A < B, they should swap.  For an
// ascending sort, ~A < ~B should swap.
bool ShouldSwap(uint A, uint B)
{
    return (A ^ NullItem) < (B ^ NullItem);
}

// Same as above, but only compares the upper 32-bit word.
bool ShouldSwap(uint2 A, uint2 B)
{
    return (A.y ^ NullItem) < (B.y ^ NullItem);
}
