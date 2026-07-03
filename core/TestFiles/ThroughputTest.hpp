#include <iostream>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <algorithm>
#include <array>
#include <optional>

#include "AdaptivePackedCellContainer/AdaptivePackedCellContainer.hpp"
#include  "NeuromorphicTimeSpace/VagueTemoraryPremativeFabric.hpp"


using namespace PredictedAdaptedEncoding;

namespace 
{
    constexpr uint32_t VALUE_COUNT = 256u;
    constexpr uint32_t PRODUCER_COUNT = 2u;
    constexpr uint32_t FF_WORKER_COUNT = 3u;
    constexpr uint32_t FB_WORKER_COUNT = 2u;
    constexpr uint32_t FINAL_WORKER_COUNT = 1u;

    packed64_t ComposeClockedModel32FroAPC(
    val32_t provided_cell_value32,
    APCPagedNodeSegmentClasses page_class = APCPagedNodeSegmentClasses::UNDEFINED,
    AttributePolicy attribute = AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL,
    LocalityPolicy locality = LocalityPolicy::PUBLISHED,
    Model32Subclass sub_class = Model32Subclass::SELF_CLASS,
    InternalDataTypePolicy dtype = InternalDataTypePolicy::UnsignedPCellDataType,
    OwnershipPolicy ownership = OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER
)
{
    const clk16_t now_clock16 = Timer48::NowClock16();
    const meta16_t desired_meta16 = PackedCell64_t::MakeMeta16ForAnyOwnerAndItsClassModel_32t(
        ownership,
        static_cast<tag8_t>(page_class),
        sub_class, attribute, locality, dtype
    );
    return PackedCell64_t::Compose32BitFamilyPackedCell(provided_cell_value32, now_clock16, desired_meta16);
}

    struct GraphStats
    {
        std::atomic<uint64_t> SensorFFProduced{0};
        std::atomic<uint64_t> PredictorFBProduced{0};

        std::atomic<uint64_t> StateIntegrated{0};
        std::atomic<uint64_t> ErrorComputed{0};

        std::atomic<uint64_t> ForwardEmitted{0};
        std::atomic<uint64_t> FeedbackEmitted{0};

        std::atomic<uint64_t> FinalCollected{0};

        std::atomic<uint64_t> GrowFF{0};
        std::atomic<uint64_t> GrowFB{0};
        std::atomic<uint64_t> GrowSTATE{0};
        std::atomic<uint64_t> GrowERROR{0};

        std::atomic<uint64_t> Retry{0};
        std::atomic<uint64_t> TerminalFail{0};

        std::atomic<uint64_t> OlderFFObserved{0};
        std::atomic<uint64_t> OlderFBObserved{0};
    };

    struct ExactLocalityCount
    {
        uint32_t Idle{0};
        uint32_t Published{0};
        uint32_t Claimed{0};
        uint32_t Faulty{0};

        uint32_t Sum() const noexcept
        {
            return Idle + Published + Claimed + Faulty;
        }
    };

    static packed64_t PackU32(
        uint32_t value,
        APCPagedNodeSegmentClasses region,
        AttributePolicy attribute = AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL
    )
    {
        return ComposeClockedModel32FroAPC(
            value,
            region,
            attribute,
            LocalityPolicy::PUBLISHED,
            Model32Subclass::SELF_CLASS,
            InternalDataTypePolicy::UnsignedPCellDataType,
            OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER
        );
    }

    static packed64_t PackFloat32(
        float value,
        APCPagedNodeSegmentClasses region,
        AttributePolicy attribute = AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL
    )
    {
        const uint32_t bits = BitCastMaybe<uint32_t>(value);

        return ComposeClockedModel32FroAPC(
            bits,
            region,
            attribute,
            LocalityPolicy::PUBLISHED,
            Model32Subclass::SELF_CLASS,
            InternalDataTypePolicy::FloatPCellDataType,
            OwnershipPolicy::ADAPTIVE_PACKED_CELL_CONTAINER
        );
    }

    static bool PublishBudgeted(
        AdaptivePackedCellContainer& apc,
        APCPagedNodeSegmentClasses region,
        packed64_t cell,
        std::atomic<uint64_t>* grow_counter,
        GraphStats& stats,
        uint32_t budget = 4096
    )
    {
        for (uint32_t attempt = 0; attempt < budget; ++attempt)
        {
            if (apc.PublishCausal(apc, region, cell, grow_counter))
            {
                return true;
            }

            stats.Retry.fetch_add(1, std::memory_order_relaxed);
        }

        stats.TerminalFail.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    static ExactLocalityCount CountExactLocality(AdaptivePackedCellContainer& apc)
    {
        ExactLocalityCount out{};

        if (!apc.IfAPCBranchValid())
        {
            return out;
        }

        for (size_t i = apc.PayloadBegin(); i < apc.GetTotalCapacityForThisAPC(); ++i)
        {
            const packed64_t cell = apc.BackingPtr[i].load(MoLoad_);
            const auto view = PackedCell64_t::GetAuthoritiveViewsForACell(cell);

            if (!view.IsCellValid ||
                view.LocalityOfCell == LocalityPolicy::FAULTY)
            {
                ++out.Faulty;
                continue;
            }

            switch (view.LocalityOfCell)
            {
                case LocalityPolicy::IDLE:
                    ++out.Idle;
                    break;

                case LocalityPolicy::PUBLISHED:
                    ++out.Published;
                    break;

                case LocalityPolicy::CLAIMED:
                    ++out.Claimed;
                    break;

                case LocalityPolicy::FAULTY:
                default:
                    ++out.Faulty;
                    break;
            }
        }

        return out;
    }

    static uint16_t HeaderUsedSum(AdaptivePackedCellContainer& apc)
    {
        return apc.ReadTotalOccuPancyOfAnyPageClass();
    }

    static uint32_t HeaderLocalitySum(AdaptivePackedCellContainer& apc)
    {
        return
            apc.ReadCentralAPCOccupancyOfALocality(LocalityPolicy::IDLE) +
            apc.ReadCentralAPCOccupancyOfALocality(LocalityPolicy::PUBLISHED) +
            apc.ReadCentralAPCOccupancyOfALocality(LocalityPolicy::CLAIMED) +
            apc.ReadCentralAPCOccupancyOfALocality(LocalityPolicy::FAULTY);
    }
    

    static void PrintNode(
        const char* name,
        AdaptivePackedCellContainer& apc
    )
    {
        const ExactLocalityCount exact = CountExactLocality(apc);

        const uint32_t header_idle =
            apc.ReadCentralAPCOccupancyOfALocality(LocalityPolicy::IDLE);

        const uint32_t header_pub =
            apc.ReadCentralAPCOccupancyOfALocality(LocalityPolicy::PUBLISHED);

        const uint32_t header_claim =
            apc.ReadCentralAPCOccupancyOfALocality(LocalityPolicy::CLAIMED);

        const uint32_t header_fault =
            apc.ReadCentralAPCOccupancyOfALocality(LocalityPolicy::FAULTY);

        const uint32_t header_sum = HeaderLocalitySum(apc);
        const uint32_t header_used = HeaderUsedSum(apc);
        const uint32_t payload = static_cast<uint32_t>(apc.PayloadCapacityFromHeader());

        std::cout << "\n[" << name << "]\n";
        // std::cout << "  branch=" << apc.GetSlabSlotID()
        //           << " logical=" << apc.GetLogicalId()
        //           << " shared=" << apc.GetSharedId()
        //           << " group=" << apc.ReadValuFromAPCMetaIndecies(MetaIndexOfAPCNode::NODE_GROUP_SIZE)
        //           << "\n";

        std::cout << "  payload_capacity=" << payload
                  << " total_capacity=" << apc.GetTotalCapacityForThisAPC()
                  << " ready_bit=0x" << std::hex
                  << apc.ReadValuFromAPCMetaIndecies(MetaIndexOfAPCNode::PAGED_NODE_READY_BIT)
                  << std::dec
                  << "\n";

        std::cout << "  header locality: "
                << "idle=" << header_idle
                << " pub=" << header_pub
                << " claim=" << header_claim
                << " faulty=" << header_fault
                << " used=" << header_used
                << " sum=" << header_sum
                << " invariant=" << (header_sum == header_idle + header_pub + header_claim + header_fault ? "OK" : "BAD")
                << "\n";

        std::cout << "  region published-data pressure:\n";

        std::cout << "  clocks: "
                  << "accFF=" << apc.ReadLastAcceptedClok16ForThisSegment(APCPagedNodeSegmentClasses::FEEDFORWARD_MESSAGE)
                  << " emitFF=" << apc.ReadLastEmittedClok16ForThisSegment(APCPagedNodeSegmentClasses::FEEDFORWARD_MESSAGE)
                  << " accFB=" << apc.ReadLastAcceptedClok16ForThisSegment(APCPagedNodeSegmentClasses::FEEDBACKWARD_MESSAGE)
                  << " emitFB=" << apc.ReadLastEmittedClok16ForThisSegment(APCPagedNodeSegmentClasses::FEEDBACKWARD_MESSAGE)
                  << "\n";
    }
}

void ThroughputTest()
{
    std::ios::sync_with_stdio(true);
    std::cout.setf(std::ios::unitbuf);
    std::cerr.setf(std::ios::unitbuf);


    // Timer48 timer;

    APCGroupReserver::APCInitialIdentityStruct cfg;
    cfg.InitialMode = PackedMode::VALUE32;


    VagueTemoraryPremativeFabric silicon_local_fabric{};

    if (!silicon_local_fabric.InitializeFabricWithPtrTable(
            1024,
            MINIMUM_APC_CAPACITY,
            APCDataStructure::BRANCH_VERSION,
            DEFAULT_THREAD_TABLE_CAPACITY))
    {
        std::cerr << "Fabric initialization failed\n";
        return;
    }

    AdaptivePackedCellContainer Sensor;
    AdaptivePackedCellContainer Predictor;
    AdaptivePackedCellContainer Comparator;
    AdaptivePackedCellContainer Integrator;
    AdaptivePackedCellContainer Motor;

    const bool roots_ok =
        silicon_local_fabric.ConstructAnAPC_(Sensor, cfg).has_value() &&
        silicon_local_fabric.ConstructAnAPC_(Predictor, cfg).has_value() &&
        silicon_local_fabric.ConstructAnAPC_(Comparator, cfg).has_value() &&
        silicon_local_fabric.ConstructAnAPC_(Integrator, cfg).has_value() &&
        silicon_local_fabric.ConstructAnAPC_(Motor, cfg).has_value();
    if (!roots_ok)
    {
        return;
    }
    

    GraphStats stats;

    std::atomic<bool> producers_done{false};
    std::atomic<uint64_t> ff_consumed{0};
    std::atomic<uint64_t> fb_consumed{0};
    std::atomic<uint64_t> state_consumed{0};
    std::atomic<uint64_t> error_consumed{0};
    std::atomic<uint64_t> final_done{0};

    std::vector<float> collected;
    std::mutex collected_mutex;

    const auto start_time = std::chrono::steady_clock::now();

    std::vector<std::thread> producers;
    std::vector<std::thread> workers;

    for (uint32_t p = 0; p < PRODUCER_COUNT; ++p)
    {
        producers.emplace_back([&, p]()
        {

            for (uint32_t i = p + 1; i <= VALUE_COUNT; i += PRODUCER_COUNT)
            {
                const packed64_t ff =
                    PackU32(i, APCPagedNodeSegmentClasses::FEEDFORWARD_MESSAGE);

                const packed64_t fb =
                    PackU32(i + 1u, APCPagedNodeSegmentClasses::FEEDBACKWARD_MESSAGE);

                if (PublishBudgeted(
                        Sensor,
                        APCPagedNodeSegmentClasses::FEEDFORWARD_MESSAGE,
                        ff,
                        &stats.GrowFF,
                        stats))
                {
                    stats.SensorFFProduced.fetch_add(1);
                    stats.ForwardEmitted.fetch_add(1);
                }

                if (PublishBudgeted(
                        Predictor,
                        APCPagedNodeSegmentClasses::FEEDBACKWARD_MESSAGE,
                        fb,
                        &stats.GrowFB,
                        stats))
                {
                    stats.PredictorFBProduced.fetch_add(1);
                    stats.FeedbackEmitted.fetch_add(1);
                }
            }

        });
    }

    for (uint32_t w = 0; w < FF_WORKER_COUNT; ++w)
    {
        workers.emplace_back([&, w]()
        {
            size_t cursor = Sensor.PayloadBegin();

            while (ff_consumed.load(std::memory_order_acquire) < VALUE_COUNT)
            {
                auto maybe =
                    Sensor.ConsumeCausal(
                        Sensor,
                        APCPagedNodeSegmentClasses::FEEDFORWARD_MESSAGE,
                        cursor,
                        &stats.OlderFFObserved
                    );

                if (!maybe)
                {
                    continue;
                }

                const auto maybe_x =
                    PackedCell64_t::ExtractAnyPackedValueX<uint32_t>(*maybe);

                if (!maybe_x)
                {
                    continue;
                }

                const uint32_t state_value = maybe_x.value() + 1u;

                const packed64_t state_cell =
                    PackU32(
                        state_value,
                        APCPagedNodeSegmentClasses::STATE_SLOT,
                        AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL
                    );

                if (PublishBudgeted(
                        Integrator,
                        APCPagedNodeSegmentClasses::STATE_SLOT,
                        state_cell,
                        &stats.GrowSTATE,
                        stats))
                {
                    stats.StateIntegrated.fetch_add(1);
                    ff_consumed.fetch_add(1);
                }
            }

        });
    }

    for (uint32_t w = 0; w < FB_WORKER_COUNT; ++w)
    {
        workers.emplace_back([&, w]()
        {
            size_t cursor = Predictor.PayloadBegin();

            while (fb_consumed.load(std::memory_order_acquire) < VALUE_COUNT)
            {
                auto maybe =
                    Predictor.ConsumeCausal(
                        Predictor,
                        APCPagedNodeSegmentClasses::FEEDBACKWARD_MESSAGE,
                        cursor,
                        &stats.OlderFBObserved
                    );

                if (!maybe)
                {
                    continue;
                }

                const auto maybe_x =
                    PackedCell64_t::ExtractAnyPackedValueX<uint32_t>(*maybe);

                if (!maybe_x)
                {
                    continue;
                }

                const uint32_t error_value = 1u;

                const packed64_t error_cell =
                    PackU32(
                        error_value,
                        APCPagedNodeSegmentClasses::ERROR_SLOT,
                        AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL
                    );

                if (PublishBudgeted(
                        Comparator,
                        APCPagedNodeSegmentClasses::ERROR_SLOT,
                        error_cell,
                        &stats.GrowERROR,
                        stats))
                {
                    stats.ErrorComputed.fetch_add(1);
                    fb_consumed.fetch_add(1);
                }
            }

        });
    }

    for (uint32_t w = 0; w < FINAL_WORKER_COUNT; ++w)
    {
        workers.emplace_back([&, w]()
        {

            size_t state_cursor = Integrator.PayloadBegin();
            size_t error_cursor = Comparator.PayloadBegin();

            while (final_done.load(std::memory_order_acquire) < VALUE_COUNT)
            {
                auto maybe_state =
                    Integrator.ConsumeCausal(
                        Integrator,
                        APCPagedNodeSegmentClasses::STATE_SLOT,
                        state_cursor,
                        nullptr
                    );

                if (!maybe_state)
                {
                    continue;
                }

                const auto maybe_state_u32 =
                    PackedCell64_t::ExtractAnyPackedValueX<uint32_t>(*maybe_state);

                if (!maybe_state_u32)
                {
                    continue;
                }

                uint32_t error_adjust = 0u;

                auto maybe_error =
                    Comparator.ConsumeCausal(
                        Comparator,
                        APCPagedNodeSegmentClasses::ERROR_SLOT,
                        error_cursor,
                        nullptr
                    );

                if (maybe_error)
                {
                    const auto maybe_error_u32 =
                        PackedCell64_t::ExtractAnyPackedValueX<uint32_t>(*maybe_error);

                    if (maybe_error_u32)
                    {
                        error_adjust = maybe_error_u32.value();
                        error_consumed.fetch_add(1);
                    }
                }

                const float motor_value =
                    static_cast<float>(maybe_state_u32.value()) + 0.5f * static_cast<float>(error_adjust);

                const packed64_t motor_cell =
                    PackFloat32(
                        motor_value,
                        APCPagedNodeSegmentClasses::FEEDFORWARD_MESSAGE,
                        AttributePolicy::SELF_CONTAINED_DATA_OR_MODEL
                    );

                if (PublishBudgeted(
                        Motor,
                        APCPagedNodeSegmentClasses::FEEDFORWARD_MESSAGE,
                        motor_cell,
                        nullptr,
                        stats))
                {
                    state_consumed.fetch_add(1);
                }

                size_t motor_cursor = Motor.PayloadBegin();

                auto final_cell =
                    Motor.ConsumeCausal(
                        Motor,
                        APCPagedNodeSegmentClasses::FEEDFORWARD_MESSAGE,
                        motor_cursor,
                        nullptr
                    );

                if (final_cell)
                {
                    const auto maybe_float =
                        PackedCell64_t::ExtractAnyPackedValueX<float>(*final_cell);

                    if (maybe_float)
                    {
                        std::lock_guard<std::mutex> lock(collected_mutex);
                        collected.push_back(maybe_float.value());
                        stats.FinalCollected.fetch_add(1);
                        final_done.fetch_add(1);
                    }
                }
            }

        });
    }

    for (auto& t : producers)
    {
        t.join();
    }

    producers_done.store(true, std::memory_order_release);
    std::cout << "All producers joined\n";

    for (auto& t : workers)
    {
        t.join();
    }

    std::cout << "All workers joined\n";

    const auto end_time = std::chrono::steady_clock::now();
    const auto runtime_us =
        std::chrono::duration_cast<std::chrono::microseconds>(
            end_time - start_time
        ).count();

    std::sort(collected.begin(), collected.end());

    std::cout << "\n==== APC Strict Authoritative Occupancy / Bidirectional Causal Test ====\n";
    std::cout << "Runtime us             : " << runtime_us << "\n";
    std::cout << "Sensor FF produced     : " << stats.SensorFFProduced.load() << "\n";
    std::cout << "Predictor FB produced  : " << stats.PredictorFBProduced.load() << "\n";
    std::cout << "State integrated       : " << stats.StateIntegrated.load() << "\n";
    std::cout << "Error computed         : " << stats.ErrorComputed.load() << "\n";
    std::cout << "Forward emitted        : " << stats.ForwardEmitted.load() << "\n";
    std::cout << "Feedback emitted       : " << stats.FeedbackEmitted.load() << "\n";
    std::cout << "Final collected        : " << stats.FinalCollected.load() << "\n";
    std::cout << "Grow FF                : " << stats.GrowFF.load() << "\n";
    std::cout << "Grow FB                : " << stats.GrowFB.load() << "\n";
    std::cout << "Grow STATE             : " << stats.GrowSTATE.load() << "\n";
    std::cout << "Grow ERROR             : " << stats.GrowERROR.load() << "\n";
    std::cout << "Retry                  : " << stats.Retry.load() << "\n";
    std::cout << "Terminal fail          : " << stats.TerminalFail.load() << "\n";
    std::cout << "Older FF observed      : " << stats.OlderFFObserved.load() << "\n";
    std::cout << "Older FB observed      : " << stats.OlderFBObserved.load() << "\n";

    PrintNode("Sensor", Sensor);
    PrintNode("Predictor", Predictor);
    PrintNode("Comparator", Comparator);
    PrintNode("Integrator", Integrator);
    PrintNode("Motor", Motor);

    std::cout << "\nFirst 16 collected values:\n";
    for (size_t i = 0; i < std::min<size_t>(16, collected.size()); ++i)
    {
        std::cout << i << " -> " << collected[i] << "\n";
    }

    Motor.FreeAll();
    Integrator.FreeAll();
    Comparator.FreeAll();
    Predictor.FreeAll();
    Sensor.FreeAll();

}