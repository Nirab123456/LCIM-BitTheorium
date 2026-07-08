#pragma once

#include <type_traits>
#include <cstring>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <random>
#include <optional>
#include <vector>
#include <algorithm>
#include <limits>
#include <cassert>
#include <cstddef>
#include <array>
#include <thread>
#include <stdexcept>
#include <memory>
#include <bit>
#include "Defination.h"

#if defined(_MSC_VER)
    #include <intrin.h>
#endif
// META16:
// [ attribute:2 | node_authority:2 | locality:2 | cell_mode:2 | cell_class:4 | mode_subclass:2 | dtype:2 ]
// shifts:
// attribute=14, node_authority=12, locality=10, cell_mode= 8, cell_class=4, mode_subclass=2, dtype=0


namespace PredictedAdaptedEncoding {
    using packed64_t = uint64_t;
    using val32_t    = uint32_t;
    using clk16_t    = uint16_t;
    using tag8_t     = uint8_t;
    using meta16_t   = uint16_t;



    static constexpr ::std::memory_order MoLoad_      = ::std::memory_order_acquire;
    static constexpr ::std::memory_order MoStoreSeq_  = ::std::memory_order_release;
    static constexpr ::std::memory_order MoStoreUnSeq_= ::std::memory_order_relaxed;
    static constexpr std::memory_order MoClaimSuccess = std::memory_order_acq_rel;
    static constexpr std::memory_order MoClaimFailure = MoLoad_;
    //will be removed
    static constexpr ::std::memory_order OnExchangeSuccess   = ::std::memory_order_acq_rel;
    static constexpr ::std::memory_order OnExchangeFailure   = ::std::memory_order_relaxed;
    //--

    static constexpr unsigned FAMILY_48_BIT_LEN = 48u;
    static constexpr unsigned VALBITS  = 32u;
    static constexpr unsigned LOW16_BIT_LEN  = 16u;
    static constexpr unsigned META16_B16  = 16u;
    static constexpr unsigned STBITS   = 8u;
    static constexpr unsigned TOTAL_LOW = 48u;

    static constexpr uint8_t DEFAULT_META16_INDEXING_LIMIT_2BIT = 3;

    static constexpr unsigned PRIO_LEN = 2u;
    static constexpr unsigned NODE_AUTH_LEN = 2u;
    static constexpr unsigned LOCALITY_LEN = 2u;// will be 2u
    static constexpr unsigned CELL_MODE_LEN = 2u;
    static constexpr unsigned CELL_CLASS_LEN = 4u;
    static constexpr unsigned SUB_CLASS_OF_CELL_MODE_LEN = 2u;
    static constexpr unsigned CELL_INTERNAL_DATA_TYPE_LEN = 2u;

    //shifts 
    static constexpr unsigned PCELL_DETATYPE_SHIFT = 0u;
    static constexpr unsigned SUBCLASS_SHIFT = PCELL_DETATYPE_SHIFT + CELL_INTERNAL_DATA_TYPE_LEN;
    static constexpr unsigned REGION_CLASS_SHIFT = SUBCLASS_SHIFT + SUB_CLASS_OF_CELL_MODE_LEN;
    static constexpr unsigned CELL_MODE_SHIFT = REGION_CLASS_SHIFT + CELL_CLASS_LEN;
    static constexpr unsigned LOCALITY_SHIFT = CELL_MODE_SHIFT + CELL_MODE_LEN;
    static constexpr unsigned OWNERSHIP_SHIFT = LOCALITY_SHIFT + LOCALITY_LEN;
    static constexpr unsigned ATTRIBUTE_SHIFT = OWNERSHIP_SHIFT + NODE_AUTH_LEN;
    static_assert(ATTRIBUTE_SHIFT + PRIO_LEN == META16_B16, "PNLTCOD must be 16 bits");
    //mask
    static constexpr tag8_t CELL_INTERNAL_DATA_TYPE_MASK = static_cast<tag8_t>((1u << CELL_INTERNAL_DATA_TYPE_LEN) - 1u);
    static constexpr tag8_t SUBCLASS_MASK = static_cast<tag8_t>((1u << SUB_CLASS_OF_CELL_MODE_LEN) - 1u);
    static constexpr tag8_t REGION_CLASS_MASK = static_cast<tag8_t>((1u << CELL_CLASS_LEN) - 1u);
    static constexpr tag8_t CELL_MODE_MASK = static_cast<tag8_t>((1u << CELL_MODE_LEN) - 1u);
    static constexpr tag8_t LOCALITY_MASK = static_cast<tag8_t>((1u << LOCALITY_LEN) - 1u);
    static constexpr tag8_t OWNERSHIP_MASK = static_cast<tag8_t>((1u << NODE_AUTH_LEN) - 1u);
    static constexpr tag8_t ATTRIBUTE_MASK = static_cast<tag8_t>((1u << PRIO_LEN) - 1u);
    
    /// @brief HIGHEST_TRUTH of Packed Cell
    enum class LocalityPolicy : tag8_t
    {
        IDLE = 0,
        PUBLISHED = 1,
        CLAIMED = 2,
        FAULTY = 3,
        UNASSIGNED_UNUSED_NANNULL = 4
    };

    /// @brief HIGHEST_TRUTH of Packed Cell
    enum class OwnershipPolicy : tag8_t
    {
        ADAPTIVE_PACKED_CELL_CONTAINER = 0,
        NEUROMORPHIC_SPACE_TIME_FABRIC = 1,
        RESERVED_2 = 2,
        RESERVED_3 = 3,
        UNASSIGNED_UNUSED_NANNULL = 4
    };

    /// @brief HIGHEST_TRUTH of Packed Cell
    enum class InternalDataTypePolicy : tag8_t
    {
        CHAR = 0,
        INT = 1,
        FLOAT = 2,
        UNSIGNED = 3,
        UNASSIGNED_UNUSED_NANNULL = 4

    };

    /// @brief HIGHEST_TRUTH of Packed Cell / ALL MODEL CELLS ARE DEFAULT ->ContractOfConcurrency::CLAIMED_GURDED
    /// @param MODEL32 -> Model32Subclass
    /// @param MODEL48 -> Model48Subclass
    /// @param VALUE32 -> ContractOfConcurrency
    /// @param VALUE48 -> ContractOfConcurrency
    enum class PackedMode : tag8_t
    {
        MODEL32 = 0,
        VALUE32 = 1,
        MODEL48 = 2,
        VALUE48 = 3,
        UNASSIGNED_UNUSED_NANNULL = 4
    };

    enum class ModelFamily : tag8_t
    {
        MODEL32 = PackedMode::MODEL32,
        MODEL48 = PackedMode::MODEL48
    };

    enum class TypeFamily : tag8_t
    {
        VALUE32 = PackedMode::VALUE32,
        VALUE48 = PackedMode::VALUE48
    };

    /// @param RAW_PRIVATE Caller owns range/cell; init/shutdown/private APC segment. 
    /// @param BOUNDED_RETRY_CAS_NO_CLAIMED Atomic load/store whole 64-bit cell. Multiple writers are allowed only if last-writer-wins is acceptable.
    /// @param CLAIMED_GURDED Exclusive mutation. After claim, writer may raw-store companion cells, then publish with release store.
    /// @param LAST_WRITIER_WIN_NO_CAS_RMW For counters, cursors, epochs, clocks, version increments, occupancy deltas. No `CLAIMED` state needed.
    enum class ContractOfConcurrency : tag8_t
    {
        RAW_PRIVATE = 0,
        BOUNDED_RETRY_CAS_NO_CLAIMED = 1,
        CLAIMED_GURDED = 2,
        LAST_WRITIER_WIN_NO_CAS_RMW = 3,
        UNASSIGNED_UNUSED_NANNULL = 4
    };

    enum class Model32Subclass : tag8_t
    {
        SELF_CLASS = 0,
        LOW_OF_PAIRED_VERSIONED_CELL = 1,
        HIGH_OF_PAIRED_VERSIONED_CELL = 2,
        UNCLOCKED_1x8_PLUS_2x4 = 3,
        UNASSIGNED_UNUSED_NANNULL = 4
    };

    enum class Model48Subclass : tag8_t
    {
        SELF_CLASS = 0,
        PURE_TIMER_48 = 1,
        SUBDIVISION16x3_INTERNAL_CELL_MODEL  = 2,
        FOUR_SUBDIVISION_2x16_AND_2x8 = 3,
        UNASSIGNED_UNUSED_NANNULL = 4
    };

    /// @brief Describs Attribute OF: Packed Cell & Why it exist
    /// @param SELF_CONTAINED_DATA_OR_MODEL VALUE OR: MODEL -> Itself Carry The Whole Message
    /// @param DEPENDENT_OR_INSTRUCTION_CELL VALUE OR: MODEL -> Describs ANY: Kind OF: Instruction TO: Closeby PackedCell
    /// @param INSTRUCTION_RAW64_NEXT INSTRUCTIONS: If Cells Are Raw64 how TO: Read and Write Them & SEQUENTIAL: N * INSTRUCTION_RAW64_NEXT (N  AMOUNT: Can be used to describe the Meta)
    /// @param INSTRUCTION_RAW64_EOF INSTRUCTIONS: Defines Its the end Raw64 and LATER ON: Cells Are Packed Cell
    enum class AttributePolicy : tag8_t
    {
        SELF_CONTAINED_DATA_OR_MODEL = 0,
        DEPENDENT_OR_INSTRUCTION_CELL = 1,
        INSTRUCTION_RAW64_NEXT = 2,
        INSTRUCTION_RAW64_EOF = 3,
        UNASSIGNED_UNUSED_NANNULL = 4
    };

    /// @brief Name Of Each Segment On APC
    /// @param NONE Lower guard prevents a Packed Cell to be ever VALID: 0
    /// @param NULLNAN Uppper Guard Prevents Pack-ed Cell to be VALID: UINT64_MAX
    enum class APCPagedNodeSegmentClasses : tag8_t
    {
        NONE = 0x0,
        FEEDFORWARD_MESSAGE  = 0x1,
        FEEDBACKWARD_MESSAGE = 0x2,
        LATERAL_MESAGE = 0x3,
        STATE_SLOT = 0x4,
        ERROR_SLOT = 0x5,
        EDGE_DESCRIPTOR = 0x6,
        WEIGHT_SLOT = 0x7,
        AUX_SLOT = 0x8,
        HETEROGENOUS_RAW_MEMORY = 0x9,
        RAW_64BIT_MEMORY = 0xA,
        PAIRED_POINTER_IN_MEMORY = 0xB,
        FREE_SLOT     = 0xC,
        UNDEFINED = 0xD,
        META_HEADER = 0xE,
        NULLNAN     = 0xF
    };

    /// @brief Name Of Each Segment On Fabric
    /// @param NONE It is the lower guard prevents a Packed Cell to be ever 0
    /// @param GLOBAL_AND_CONFIG USED:FOR: Everything else after @param THREAD_TABLE
    /// @param RECORD_BOOK_OF_TABLE_SEGMENT_CLASSES STORES:All Begin & End Pair of indicies for every class of FabricTableSegmentClasses
    /// @param APC_HANDLE_DESCRIPTOR HOLDS:Each APC x RECORD:APCDescriptorCellType -> DESCRIBS: Initial Fundamental Meta for An APC When Created 
    /// @param BRANCH_HASH
    /// @param LOGICAL_HASH
    /// @param SHARED_HASH
    /// @param GENERIC_CONTROL USED:FOR: first 96 FabricMetaIndicies
    /// @param NULLNAN Uppper Guard Prevents Pack-ed Cell to be VALID: UINT64_MAX
    enum class FabricTableSegmentClasses : tag8_t
    {
        NONE = 0,
        GLOBAL_AND_CONFIG = 1,
        RECORD_BOOK_OF_TABLE_SEGMENT_CLASSES = 2,
        APC_HANDLE_DESCRIPTOR = 3,
        BRANCH_HASH = 4,
        LOGICAL_HASH = 5,
        SHARED_HASH = 6,
        EDGE_TABLE = 7,
        FREE_APC_LIST = 8,
        READY_QUEUE = 9,
        WORK_QUEUE = 10,
        DEVICE_VIEW_TABLE = 11,
        THREAD_TABLE  = 12,
        SEGMENT_POOL = 13,
        GENERIC_CONTROL = 14,
        NULLNAN = 15,
    };

    using OriginOfRecord = FabricTableSegmentClasses;


    static  constexpr packed64_t MaskLowNBits(unsigned n) noexcept
    {
        if (n == UNSIGNED_ZERO) return packed64_t(0);
        if (n >= BIT_LENGTH_OF_A_PACKED_CELL) return ~packed64_t(0);
        // produce low-n ones without shifting by >= width
        return ((packed64_t(1) << n) - 1u);                  
    }

    template<typename PCDT>
    struct PackedCellTypeBridge
    {
        static_assert(std::is_trivially_copyable_v<PCDT>, "Packed Cell allowes Trivially copyable 32/48 bit unsigned, int, float, char");
        using Decayed = std::remove_cv_t<std::remove_reference_t<PCDT>>;
        static constexpr bool IS_CHAR_LIKE = std::is_same_v<Decayed, char> || std::is_same_v<Decayed, signed char> || std::is_same_v<Decayed, unsigned char>;
        static constexpr bool IS_FLOAT_LIKE = std::is_floating_point_v<Decayed>;
        static constexpr bool IS_SIGNED_LIKE = std::is_integral_v<Decayed> && std::is_signed_v<Decayed> && !IS_CHAR_LIKE;
        static constexpr bool IS_UNSIGNED_LIKE = std::is_integral_v<Decayed> && std::is_unsigned_v<Decayed> && !IS_CHAR_LIKE;


        static constexpr InternalDataTypePolicy DType  = 
            IS_FLOAT_LIKE           ? InternalDataTypePolicy::FLOAT    :
            IS_SIGNED_LIKE          ? InternalDataTypePolicy::INT      :
            IS_UNSIGNED_LIKE        ? InternalDataTypePolicy::UNSIGNED :
            IS_CHAR_LIKE            ? InternalDataTypePolicy::CHAR     :
                                    InternalDataTypePolicy::UNSIGNED   ;
        static constexpr bool FITS_MODE_32 = (sizeof(Decayed) <= sizeof(val32_t));
        static constexpr bool FITS_MODE_48 = (sizeof(Decayed) <= SIZE_OF_MODE_48);
        static constexpr bool IS_SUPPORTED_TYPE = IS_FLOAT_LIKE || IS_SIGNED_LIKE || IS_UNSIGNED_LIKE || IS_CHAR_LIKE;

    };


    template<typename PCDT>
     constexpr InternalDataTypePolicy BridgeOfPackedCellDataType_v = PackedCellTypeBridge<PCDT>::DType;


    template <typename To, typename From>
     To BitCastMaybe(const From& from_address)
    {
        To out;
        if constexpr (sizeof(To) == sizeof(From))
        {
            return std::bit_cast<To>(from_address);
        }
        else if constexpr (sizeof(To) < sizeof(From))
        {
            std::memcpy(&out, &from_address, sizeof(To));
            return out;
        }
        else
        {
            std::memset(&out, 0, sizeof(To));
            std::memcpy(&out, &from_address, sizeof(From));
            return out;
        }
    }


}