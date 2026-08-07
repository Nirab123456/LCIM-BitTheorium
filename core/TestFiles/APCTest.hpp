// #include <iostream>
// #include <iomanip>
// #include <vector>
// #include <array>
// #include <optional>
// #include <atomic>
// #include <chrono>
// #include <thread>
// #include <cstdint>
// #include <cstring>
// #include <algorithm>
// #include <string>

// #include "AdaptivePackedCellContainer/APCSegmentsCausalCordinator.hpp"


// using namespace BidirectionalInMemGraph;

// namespace
// {
//     /*
//         APC Growth-Oriented Tiny Predictive Neural Fabric Test

//         Purpose:
//             1. Force shared-chain growth.
//             2. Validate updated APC occupancy invariant:
//                    central(PUBLISHED/CLAIMED/FAULTY)
//                    ==
//                    sum(all region PUBLISHED/CLAIMED/FAULTY)
//             3. Validate that CONTROL_SLOT metacells remain counted.
//             4. Validate that payload exact counts match non-control region metadata.
//             5. Demonstrate a tiny multidirectional predictive neural flow.

//         Why this version grows:
//             The previous test consumed each cell quickly.
//             That kept every region below pressure.
//             This version publishes large bursts first, then drains later.

//         Neural flow:

//             Sensor.FF burst
//                 -> Comparator.FF

//             Predictor.FB burst
//                 -> Comparator.FB

//             Comparator:
//                 error = sensor - prediction
//                 state = prediction + 0.5 * error

//                 -> Integrator.STATE
//                 -> Integrator.ERROR

//             Integrator:
//                 motor = state + 0.25 * error
//                 feedback = 0.20 * error

//                 -> Motor.FF
//                 -> Predictor.FB

//             Final:
//                 drain Motor.FF
//                 drain Predictor.FB

//         Important:
//             This is not a speed benchmark.
//             This is a correctness/growth/invariant test.
//     */

//     constexpr size_t NODE_CAPACITY = 256;
//     constexpr uint32_t BURST_N = 320;
//     constexpr uint32_t MAX_RETRY = 20000;

//     struct ExactPayloadLocalityCount
//     {
//         uint32_t Idle = 0;
//         uint32_t Published = 0;
//         uint32_t Claimed = 0;
//         uint32_t Faulty = 0;

//         uint32_t Sum() const noexcept
//         {
//             return Idle + Published + Claimed + Faulty;
//         }

//         uint32_t Used() const noexcept
//         {
//             return Published + Claimed + Faulty;
//         }
//     };

//     struct OccupancyTriple
//     {
//         uint32_t Published = 0;
//         uint32_t Claimed = 0;
//         uint32_t Faulty = 0;

//         uint32_t Used() const noexcept
//         {
//             return Published + Claimed + Faulty;
//         }
//     };

//     struct NeuralTrace
//     {
//         uint32_t Index = 0;
//         float Sensor = 0.0f;
//         float Prediction = 0.0f;
//         float Error = 0.0f;
//         float State = 0.0f;
//         float Motor = 0.0f;
//         float Feedback = 0.0f;
//     };

//     template <typename To, typename From>
//     To BitCastPortable(const From& src) noexcept
//     {
//         static_assert(sizeof(To) == sizeof(From));
//         static_assert(std::is_trivially_copyable_v<To>);
//         static_assert(std::is_trivially_copyable_v<From>);

//         To out{};
//         std::memcpy(&out, &src, sizeof(To));
//         return out;
//     }

//     const char* RegionName(MacroColumnOfAPC r) noexcept
//     {
//         switch (r)
//         {
//             case MacroColumnOfAPC::NONE: return "NONE";
//             case MacroColumnOfAPC::FEEDFORWARD_MESSAGE: return "FF";
//             case MacroColumnOfAPC::FEEDBACKWARD_MESSAGE: return "FB";
//             case MacroColumnOfAPC::LATERAL_MESAGE: return "LATERAL";
//             case MacroColumnOfAPC::STATE_SLOT: return "STATE";
//             case MacroColumnOfAPC::ERROR_SLOT: return "ERROR";
//             case MacroColumnOfAPC::WEIGHTLESS_LOOKUP: return "EDGE";
//             case MacroColumnOfAPC::WEIGHT_SLOT: return "WEIGHT";
//             case MacroColumnOfAPC::META_HEADER: return "CONTROL";
//             case MacroColumnOfAPC::AUX_SLOT: return "AUX";
//             case MacroColumnOfAPC::HETEROGENOUS_PTR: return "HETERO_MEM";
//             case MacroColumnOfAPC::RAW_64BIT_MEMORY: return "LOCAL_PTR";
//             case MacroColumnOfAPC::PAIRED_POINTER_IN_MEMORY: return "DIST_PTR";
//             case MacroColumnOfAPC::FREE_SLOT: return "FREE";
//             case MacroColumnOfAPC::UNDEFINED: return "UNDEFINED";
//             case MacroColumnOfAPC::NULLNAN: return "NULLNAN";
//             default: return "UNKNOWN";
//         }
//     }

//     const char* PassFail(bool ok) noexcept
//     {
//         return ok ? "PASS" : "FAIL";
//     }

//     std::array<MacroColumnOfAPC, 14> TrackedRegionsNoNoneNoNan() noexcept
//     {
//         return {
//             MacroColumnOfAPC::FEEDFORWARD_MESSAGE,
//             MacroColumnOfAPC::FEEDBACKWARD_MESSAGE,
//             MacroColumnOfAPC::LATERAL_MESAGE,
//             MacroColumnOfAPC::STATE_SLOT,
//             MacroColumnOfAPC::ERROR_SLOT,
//             MacroColumnOfAPC::WEIGHTLESS_LOOKUP,
//             MacroColumnOfAPC::WEIGHT_SLOT,
//             MacroColumnOfAPC::META_HEADER,
//             MacroColumnOfAPC::AUX_SLOT,
//             MacroColumnOfAPC::HETEROGENOUS_PTR,
//             MacroColumnOfAPC::RAW_64BIT_MEMORY,
//             MacroColumnOfAPC::PAIRED_POINTER_IN_MEMORY,
//             MacroColumnOfAPC::FREE_SLOT,
//             MacroColumnOfAPC::UNDEFINED
//         };
//     }

//     std::array<MacroColumnOfAPC, 8> PrintedRegions() noexcept
//     {
//         return {
//             MacroColumnOfAPC::META_HEADER,
//             MacroColumnOfAPC::FEEDFORWARD_MESSAGE,
//             MacroColumnOfAPC::FEEDBACKWARD_MESSAGE,
//             MacroColumnOfAPC::LATERAL_MESAGE,
//             MacroColumnOfAPC::STATE_SLOT,
//             MacroColumnOfAPC::ERROR_SLOT,
//             MacroColumnOfAPC::AUX_SLOT,
//             MacroColumnOfAPC::UNDEFINED
//         };
//     }

//     uint64_t PackF32(
//         MasterClockConf& clock,
//         float value,
//         MacroColumnOfAPC page_class,
//         WildCardOfPackedCell attribute = WildCardOfPackedCell::PACKED_CELL,
//         PackedMode mode = PackedMode::VALUE32
//     )
//     {
//         const uint32_t bits = BitCastPortable<uint32_t>(value);

//         const clk16_t now_16 = clock.NowClock16();

//         if (mode == PackedMode::MODEL32 || mode == PackedMode::MODEL48)
//         {
//             return PackedCell64_t::MakeModeledAPCValidPackedCell(
//                 static_cast<ModelFamily>(mode),
//                 UNSIGNED_ZERO, page_class, LocalityPolicy::PUBLISHED,
//                 InternalDataTypePolicy::FLOAT,
//                 attribute,
//                 bits,
//                 now_16
//             );
//         }


//         return PackedCell64_t::MakeTypedAPCValidPackedCell(
//             static_cast<TypeFamily>(mode),
//             ContractOfConcurrency::CLAIMED_GURDED,
//             page_class,
//             LocalityPolicy::PUBLISHED,
//             InternalDataTypePolicy::FLOAT,
//             attribute,
//             bits,
//             now_16
//         );

//     }

//     float UnpackF32(uint64_t cell, float fallback = 0.0f)
//     {
//         const auto maybe = PackedCell64_t::ExtractAnyPackedValueX<float>(cell);
//         return maybe ? *maybe : fallback;
//     }

//     ExactPayloadLocalityCount CountExactLocalPayload(AdaptivePackedCellContainer& apc)
//     {
//         ExactPayloadLocalityCount out{};

//         if (!apc.IfAPCBranchValid())
//         {
//             return out;
//         }

//         for (size_t i = apc.PayloadBegin(); i < apc.GetTotalCapacityForThisAPC(); ++i)
//         {
//             const auto view = PackedCell64_t::GetAuthoritiveViewsForACell(cell);

//             if (!view.IsCellValid)
//             {
//                 ++out.Faulty;
//                 continue;
//             }

//             switch (view.LocalityOfCell)
//             {
//                 case LocalityPolicy::IDLE:
//                     ++out.Idle;
//                     break;

//                 case LocalityPolicy::PUBLISHED:
//                     ++out.Published;
//                     break;

//                 case LocalityPolicy::CLAIMED:
//                     ++out.Claimed;
//                     break;

//                 case LocalityPolicy::FAULTY:
//                 default:
//                     ++out.Faulty;
//                     break;
//             }
//         }

//         return out;
//     }

//     ExactPayloadLocalityCount CountExactChainPayload(APCSegmentsCausalCordinator& root)
//     {
//         ExactPayloadLocalityCount sum{};

//         AdaptivePackedCellContainer* current = &root;
//         uint32_t guard = 0;

//         while (current && guard++ < 1024)
//         {
//             const ExactPayloadLocalityCount local = CountExactLocalPayload(*current);

//             sum.Idle += local.Idle;
//             sum.Published += local.Published;
//             sum.Claimed += local.Claimed;
//             sum.Faulty += local.Faulty;

//             AdaptivePackedCellContainer* next = current->GetNextSharedSegment();

//             if (!next || next == current)
//             {
//                 break;
//             }

//             current = next;
//         }

//         return sum;
//     }

//     uint32_t ChainLength(APCSegmentsCausalCordinator& root)
//     {
//         uint32_t count = 0;

//         AdaptivePackedCellContainer* current = &root;
//         uint32_t guard = 0;

//         while (current && guard++ < 1024)
//         {
//             ++count;

//             AdaptivePackedCellContainer* next = current->GetNextSharedSegment();

//             if (!next || next == current)
//             {
//                 break;
//             }

//             current = next;
//         }

//         return count;
//     }

//     OccupancyTriple ReadCentral(AdaptivePackedCellContainer& apc)
//     {
//         OccupancyTriple out{};

//         out.Published = apc.ReadCentralAPCOccupancyOfALocality(
//             LocalityPolicy::PUBLISHED
//         );

//         out.Claimed = apc.ReadCentralAPCOccupancyOfALocality(
//             LocalityPolicy::CLAIMED
//         );

//         out.Faulty = apc.ReadCentralAPCOccupancyOfALocality(
//             LocalityPolicy::FAULTY
//         );

//         return out;
//     }

//     OccupancyTriple ReadRegion(
//         AdaptivePackedCellContainer& apc,
//         MacroColumnOfAPC region
//     )
//     {
//         OccupancyTriple out{};

//         out.Published = apc.ReadRegionOccupancyOfALocality(
//             LocalityPolicy::PUBLISHED,
//             region
//         );

//         out.Claimed = apc.ReadRegionOccupancyOfALocality(
//             LocalityPolicy::CLAIMED,
//             region
//         );

//         out.Faulty = apc.ReadRegionOccupancyOfALocality(
//             LocalityPolicy::FAULTY,
//             region
//         );

//         return out;
//     }

//     uint32_t LocalPayloadMetaUsed(AdaptivePackedCellContainer& apc)
//     {
//         uint32_t used = 0;

//         for (const MacroColumnOfAPC region : TrackedRegionsNoNoneNoNan())
//         {
//             if (region == MacroColumnOfAPC::META_HEADER)
//             {
//                 continue;
//             }

//             const OccupancyTriple r = ReadRegion(apc, region);
//             used += r.Used();
//         }

//         return used;
//     }

//     bool LocalStrongInvariant(AdaptivePackedCellContainer& apc)
//     {
//         const bool central_sum_ok = apc.ValidateAPCOccupancyInvarient();

//         const ExactPayloadLocalityCount exact = CountExactLocalPayload(apc);
//         const bool exact_sum_ok = exact.Sum() == apc.PayloadCapacityFromHeader();

//         const uint32_t payload_meta_used = LocalPayloadMetaUsed(apc);
//         const bool payload_meta_ok = exact.Used() == payload_meta_used;

//         return central_sum_ok && exact_sum_ok && payload_meta_ok;
//     }

//     bool ChainStrongInvariant(APCSegmentsCausalCordinator& root)
//     {
//         AdaptivePackedCellContainer* current = &root;
//         uint32_t guard = 0;

//         while (current && guard++ < 1024)
//         {
//             if (!LocalStrongInvariant(*current))
//             {
//                 return false;
//             }

//             AdaptivePackedCellContainer* next = current->GetNextSharedSegment();

//             if (!next || next == current)
//             {
//                 break;
//             }

//             current = next;
//         }

//         return true;
//     }

//     void PrintLocalSegmentLine(
//         AdaptivePackedCellContainer& seg,
//         const char* prefix
//     )
//     {
//         const OccupancyTriple central = ReadCentral(seg);
//         const OccupancyTriple control = ReadRegion(seg, MacroColumnOfAPC::META_HEADER);
//         const ExactPayloadLocalityCount exact = CountExactLocalPayload(seg);

//         std::cout
//             << prefix
//             << "branch=" << seg.GetSlabSlotID()
//             << " logical=" << seg.GetLogicalId()
//             << " shared=" << seg.GetSharedId()
//             << " group=" << seg.ReadValuFromAPCMetaIndecies(HeaderIdentifierOfAPC::NODE_GROUP_SIZE)
//             << " cap=" << seg.GetTotalCapacityForThisAPC()
//             << " payload=" << seg.PayloadCapacityFromHeader()
//             << " ready=0x" << std::hex
//             << seg.ReadValuFromAPCMetaIndecies(HeaderIdentifierOfAPC::PAGED_NODE_READY_BIT)
//             << std::dec
//             << " central(pub=" << central.Published
//             << ",claim=" << central.Claimed
//             << ",fault=" << central.Faulty
//             << ") control(pub=" << control.Published
//             << ",claim=" << control.Claimed
//             << ",fault=" << control.Faulty
//             << ") exact_payload(pub=" << exact.Published
//             << ",claim=" << exact.Claimed
//             << ",fault=" << exact.Faulty
//             << ",idle=" << exact.Idle
//             << ") inv=" << PassFail(LocalStrongInvariant(seg))
//             << "\n";
//     }

//     void PrintRegionPressure(
//         APCSegmentsCausalCordinator& node,
//         MacroColumnOfAPC region
//     )
//     {
//         const uint32_t exact_chain_pub =
//             node.CountExactTotalChainOccupancy(region);

//         std::cout
//             << "    "
//             << std::left << std::setw(12) << RegionName(region)
//             << " exact_chain_pub=" << std::setw(5) << exact_chain_pub;

//         AdaptivePackedCellContainer* current = &node;
//         uint32_t seg_index = 0;
//         uint32_t guard = 0;

//         while (current && guard++ < 1024)
//         {
//             const OccupancyTriple local = ReadRegion(*current, region);

//             std::cout
//                 << " | s" << seg_index
//                 << "(p=" << local.Published
//                 << ",c=" << local.Claimed
//                 << ",f=" << local.Faulty
//                 << ")";

//             AdaptivePackedCellContainer* next = current->GetNextSharedSegment();

//             if (!next || next == current)
//             {
//                 break;
//             }

//             current = next;
//             ++seg_index;
//         }

//         std::cout << "\n";
//     }

//     void PrintNodeSnapshot(
//         const char* label,
//         APCSegmentsCausalCordinator& node,
//         bool print_segments = true
//     )
//     {
//         std::cout << "\n[" << label << "]\n";
//         std::cout
//             << "  chain_length=" << ChainLength(node)
//             << " chain_invariant=" << PassFail(ChainStrongInvariant(node))
//             << "\n";

//         const ExactPayloadLocalityCount chain_exact = CountExactChainPayload(node);

//         std::cout
//             << "  exact_chain_payload: idle=" << chain_exact.Idle
//             << " pub=" << chain_exact.Published
//             << " claim=" << chain_exact.Claimed
//             << " faulty=" << chain_exact.Faulty
//             << " used=" << chain_exact.Used()
//             << "\n";

//         std::cout << "  region chain pressure:\n";

//         for (const MacroColumnOfAPC region : PrintedRegions())
//         {
//             PrintRegionPressure(node, region);
//         }

//         if (print_segments)
//         {
//             std::cout << "  local segments:\n";

//             AdaptivePackedCellContainer* current = &node;
//             uint32_t index = 0;
//             uint32_t guard = 0;

//             while (current && guard++ < 4)
//             {
//                 std::string prefix = "    seg[" + std::to_string(index) + "] ";
//                 PrintLocalSegmentLine(*current, prefix.c_str());

//                 AdaptivePackedCellContainer* next = current->GetNextSharedSegment();

//                 if (!next || next == current)
//                 {
//                     break;
//                 }

//                 current = next;
//                 ++index;
//             }
//         }
//     }

//     bool PublishWithRetry(
//         APCSegmentsCausalCordinator& node,
//         MacroColumnOfAPC region,
//         uint64_t cell,
//         PackedCellContainerManager& manager,
//         std::atomic<uint64_t>& growth_counter,
//         const char* label
//     )
//     {
//         for (uint32_t attempt = 0; attempt < MAX_RETRY; ++attempt)
//         {
//             if (node.PublishCausal(region, cell, &growth_counter))
//             {
//                 return true;
//             }

//             manager.GetManagersAdaptiveBackoff().AutoBackoff();
//         }

//         std::cerr
//             << "[FATAL] publish failed: " << label
//             << " region=" << RegionName(region)
//             << "\n";

//         return false;
//     }

//     std::optional<uint64_t> ConsumeWithRetry(
//         APCSegmentsCausalCordinator& node,
//         MacroColumnOfAPC region,
//         size_t& cursor,
//         PackedCellContainerManager& manager,
//         std::atomic<uint64_t>& older_counter,
//         const char* label
//     )
//     {
//         for (uint32_t attempt = 0; attempt < MAX_RETRY; ++attempt)
//         {
//             auto maybe = node.ConsumeCausal(
//                 region,
//                 cursor,
//                 &older_counter,
//                 false
//             );

//             if (maybe)
//             {
//                 return maybe;
//             }

//             manager.GetManagersAdaptiveBackoff().AutoBackoff();
//         }

//         std::cerr
//             << "[FATAL] consume failed: " << label
//             << " region=" << RegionName(region)
//             << "\n";

//         return std::nullopt;
//     }

//     void InitNode(
//         APCSegmentsCausalCordinator& node,
//         PackedCellContainerManager& manager,
//         const APCGroupReserver::APCInitialIdentityStruct& cfg,
//         uint32_t aux = 0
//     )
//     {
//         node.SetManagerForGlobalAPC(&manager);

//         node.InitAPCAsNode(
//             cfg.BranchMinChildCapacity,
//             cfg,
//             aux
//         );
//     }

//     bool PublishSensorBurst(
//         APCSegmentsCausalCordinator& sensor,
//         PackedCellContainerManager& manager,
//         MasterClockConf& clock,
//         std::atomic<uint64_t>& growth_counter
//     )
//     {
//         for (uint32_t i = 0; i < BURST_N; ++i)
//         {
//             const float value = 1.0f + static_cast<float>(i) * 0.03125f;

//             if (!PublishWithRetry(
//                     sensor,
//                     MacroColumnOfAPC::FEEDFORWARD_MESSAGE,
//                     PackF32(
//                         clock,
//                         value,
//                         MacroColumnOfAPC::FEEDFORWARD_MESSAGE,
//                         WildCardOfPackedCell::PACKED_CELL
//                     ),
//                     manager,
//                     growth_counter,
//                     "Sensor.FF burst"
//                 ))
//             {
//                 return false;
//             }
//         }

//         return true;
//     }

//     bool PublishPredictionBurst(
//         APCSegmentsCausalCordinator& predictor,
//         PackedCellContainerManager& manager,
//         MasterClockConf& clock,
//         std::atomic<uint64_t>& growth_counter
//     )
//     {
//         for (uint32_t i = 0; i < BURST_N; ++i)
//         {
//             const float prediction = 0.5f + static_cast<float>(i) * 0.015625f;

//             if (!PublishWithRetry(
//                     predictor,
//                     MacroColumnOfAPC::FEEDBACKWARD_MESSAGE,
//                     PackF32(
//                         clock,
//                         prediction,
//                         MacroColumnOfAPC::FEEDBACKWARD_MESSAGE,
//                         WildCardOfPackedCell::PACKED_CELL
//                     ),
//                     manager,
//                     growth_counter,
//                     "Predictor.FB burst"
//                 ))
//             {
//                 return false;
//             }
//         }

//         return true;
//     }
// }

// void APCTest()
// {
//     std::ios::sync_with_stdio(true);
//     std::cout.setf(std::ios::unitbuf);
//     std::cerr.setf(std::ios::unitbuf);

//     std::cout << "\n";
//     std::cout << "============================================================\n";
//     std::cout << " APC Growth-Oriented Predictive Neural Fabric Test\n";
//     std::cout << "============================================================\n";

//     auto& manager = PackedCellContainerManager::Instance();
//     manager.StartAPCManager();

//     Timer48 timer;
//     MasterClockConf clock(nullptr, timer);

//     APCGroupReserver::APCInitialIdentityStruct cfg;
//     cfg.InitialMode = PackedMode::MODEL32;
//     cfg.ProducerBlockSize = 4;
//     cfg.RegionSize = 8;
//     cfg.EnableBranching = true;
//     cfg.BranchSplitThresholdPercentage = 25;
//     cfg.BranchMaxDepth = 8;
//     cfg.BranchMinChildCapacity = NODE_CAPACITY;
//     cfg.NodeGroupSize = 1u;

//     APCSegmentsCausalCordinator Sensor;
//     APCSegmentsCausalCordinator Predictor;
//     APCSegmentsCausalCordinator Comparator;
//     APCSegmentsCausalCordinator Integrator;
//     APCSegmentsCausalCordinator Motor;

//     InitNode(
//         Sensor,
//         manager,
//         cfg
//     );

//     InitNode(
//         Predictor,
//         manager,
//         cfg
//     );

//     InitNode(
//         Comparator,
//         manager,
//         cfg
//     );

//     InitNode(
//         Integrator,
//         manager,
//         cfg
//     );

//     InitNode(
//         Motor,
//         manager,
//         cfg
//     );

//     std::atomic<uint64_t> grow_sensor{0};
//     std::atomic<uint64_t> grow_predictor{0};
//     std::atomic<uint64_t> grow_comparator{0};
//     std::atomic<uint64_t> grow_integrator{0};
//     std::atomic<uint64_t> grow_motor{0};
//     std::atomic<uint64_t> older_counter{0};

//     size_t sensor_ff_cursor = Sensor.PayloadBegin();
//     size_t predictor_fb_cursor = Predictor.PayloadBegin();
//     size_t comparator_ff_cursor = Comparator.PayloadBegin();
//     size_t comparator_fb_cursor = Comparator.PayloadBegin();
//     size_t integrator_state_cursor = Integrator.PayloadBegin();
//     size_t integrator_error_cursor = Integrator.PayloadBegin();
//     size_t motor_ff_cursor = Motor.PayloadBegin();
//     size_t predictor_feedback_cursor = Predictor.PayloadBegin();

//     std::vector<NeuralTrace> traces;
//     traces.reserve(BURST_N);

//     bool ok = true;

//     const auto start = std::chrono::steady_clock::now();

//     std::cout << "\nInitial snapshots:\n";
//     PrintNodeSnapshot("Sensor initial", Sensor, false);
//     PrintNodeSnapshot("Predictor initial", Predictor, false);
//     PrintNodeSnapshot("Comparator initial", Comparator, false);
//     PrintNodeSnapshot("Integrator initial", Integrator, false);
//     PrintNodeSnapshot("Motor initial", Motor, false);

//     std::cout << "\n============================================================\n";
//     std::cout << "Phase 1: burst-fill Sensor.FF and Predictor.FB\n";
//     std::cout << "============================================================\n";

//     ok = ok && PublishSensorBurst(Sensor, manager, clock, grow_sensor);
//     ok = ok && PublishPredictionBurst(Predictor, manager, clock, grow_predictor);

//     PrintNodeSnapshot("Sensor after FF burst", Sensor, true);
//     PrintNodeSnapshot("Predictor after initial FB burst", Predictor, true);

//     std::cout << "\n============================================================\n";
//     std::cout << "Phase 2: move bursts into Comparator.FF and Comparator.FB\n";
//     std::cout << "============================================================\n";

//     for (uint32_t i = 0; i < BURST_N && ok; ++i)
//     {
//         auto sensor_cell = ConsumeWithRetry(
//             Sensor,
//             MacroColumnOfAPC::FEEDFORWARD_MESSAGE,
//             sensor_ff_cursor,
//             manager,
//             older_counter,
//             "Sensor drain FF"
//         );

//         auto prediction_cell = ConsumeWithRetry(
//             Predictor,
//             MacroColumnOfAPC::FEEDBACKWARD_MESSAGE,
//             predictor_fb_cursor,
//             manager,
//             older_counter,
//             "Predictor drain initial FB"
//         );

//         if (!sensor_cell || !prediction_cell)
//         {
//             ok = false;
//             break;
//         }

//         ok = ok && PublishWithRetry(
//             Comparator,
//             MacroColumnOfAPC::FEEDFORWARD_MESSAGE,
//             *sensor_cell,
//             manager,
//             grow_comparator,
//             "Comparator receive FF"
//         );

//         ok = ok && PublishWithRetry(
//             Comparator,
//             MacroColumnOfAPC::FEEDBACKWARD_MESSAGE,
//             *prediction_cell,
//             manager,
//             grow_comparator,
//             "Comparator receive FB"
//         );
//     }

//     PrintNodeSnapshot("Sensor after drain", Sensor, true);
//     PrintNodeSnapshot("Predictor after initial prediction drain", Predictor, true);
//     PrintNodeSnapshot("Comparator after FF+FB burst", Comparator, true);

//     std::cout << "\n============================================================\n";
//     std::cout << "Phase 3: Comparator computes ERROR and STATE into Integrator\n";
//     std::cout << "============================================================\n";

//     for (uint32_t i = 0; i < BURST_N && ok; ++i)
//     {
//         auto ff = ConsumeWithRetry(
//             Comparator,
//             MacroColumnOfAPC::FEEDFORWARD_MESSAGE,
//             comparator_ff_cursor,
//             manager,
//             older_counter,
//             "Comparator consume FF"
//         );

//         auto fb = ConsumeWithRetry(
//             Comparator,
//             MacroColumnOfAPC::FEEDBACKWARD_MESSAGE,
//             comparator_fb_cursor,
//             manager,
//             older_counter,
//             "Comparator consume FB"
//         );

//         if (!ff || !fb)
//         {
//             ok = false;
//             break;
//         }

//         const float sensor_value = UnpackF32(*ff);
//         const float prediction = UnpackF32(*fb);

//         const float error = sensor_value - prediction;
//         const float state = prediction + 0.50f * error;

//         NeuralTrace t{};
//         t.Index = i;
//         t.Sensor = sensor_value;
//         t.Prediction = prediction;
//         t.Error = error;
//         t.State = state;
//         traces.push_back(t);

//         ok = ok && PublishWithRetry(
//             Integrator,
//             MacroColumnOfAPC::STATE_SLOT,
//             PackF32(
//                 clock,
//                 state,
//                 MacroColumnOfAPC::STATE_SLOT,
//                 WildCardOfPackedCell::PACKED_CELL
//             ),
//             manager,
//             grow_integrator,
//             "Integrator receive STATE"
//         );

//         ok = ok && PublishWithRetry(
//             Integrator,
//             MacroColumnOfAPC::ERROR_SLOT,
//             PackF32(
//                 clock,
//                 error,
//                 MacroColumnOfAPC::ERROR_SLOT,
//                 WildCardOfPackedCell::PACKED_CELL
//             ),
//             manager,
//             grow_integrator,
//             "Integrator receive ERROR"
//         );
//     }

//     PrintNodeSnapshot("Comparator after drain", Comparator, true);
//     PrintNodeSnapshot("Integrator after STATE+ERROR burst", Integrator, true);

//     std::cout << "\n============================================================\n";
//     std::cout << "Phase 4: Integrator emits Motor.FF and Predictor.FB feedback\n";
//     std::cout << "============================================================\n";

//     for (uint32_t i = 0; i < BURST_N && ok; ++i)
//     {
//         auto state_cell = ConsumeWithRetry(
//             Integrator,
//             MacroColumnOfAPC::STATE_SLOT,
//             integrator_state_cursor,
//             manager,
//             older_counter,
//             "Integrator consume STATE"
//         );

//         auto error_cell = ConsumeWithRetry(
//             Integrator,
//             MacroColumnOfAPC::ERROR_SLOT,
//             integrator_error_cursor,
//             manager,
//             older_counter,
//             "Integrator consume ERROR"
//         );

//         if (!state_cell || !error_cell)
//         {
//             ok = false;
//             break;
//         }

//         const float state = UnpackF32(*state_cell);
//         const float error = UnpackF32(*error_cell);

//         const float motor = state + 0.25f * error;
//         const float feedback = 0.20f * error;

//         if (i < traces.size())
//         {
//             traces[i].Motor = motor;
//             traces[i].Feedback = feedback;
//         }

//         ok = ok && PublishWithRetry(
//             Motor,
//             MacroColumnOfAPC::FEEDFORWARD_MESSAGE,
//             PackF32(
//                 clock,
//                 motor,
//                 MacroColumnOfAPC::FEEDFORWARD_MESSAGE,
//                 WildCardOfPackedCell::PACKED_CELL
//             ),
//             manager,
//             grow_motor,
//             "Motor receive FF"
//         );

//         ok = ok && PublishWithRetry(
//             Predictor,
//             MacroColumnOfAPC::FEEDBACKWARD_MESSAGE,
//             PackF32(
//                 clock,
//                 feedback,
//                 MacroColumnOfAPC::FEEDBACKWARD_MESSAGE,
//                 WildCardOfPackedCell::PACKED_CELL
//             ),
//             manager,
//             grow_predictor,
//             "Predictor receive feedback"
//         );
//     }

//     PrintNodeSnapshot("Integrator after drain", Integrator, true);
//     PrintNodeSnapshot("Motor after motor burst", Motor, true);
//     PrintNodeSnapshot("Predictor after feedback burst", Predictor, true);

//     std::cout << "\n============================================================\n";
//     std::cout << "Phase 5: final drain Motor.FF and Predictor.FB\n";
//     std::cout << "============================================================\n";

//     std::vector<float> motor_outputs;
//     std::vector<float> feedback_outputs;
//     motor_outputs.reserve(BURST_N);
//     feedback_outputs.reserve(BURST_N);

//     for (uint32_t i = 0; i < BURST_N && ok; ++i)
//     {
//         auto motor_cell = ConsumeWithRetry(
//             Motor,
//             MacroColumnOfAPC::FEEDFORWARD_MESSAGE,
//             motor_ff_cursor,
//             manager,
//             older_counter,
//             "Motor final drain FF"
//         );

//         auto feedback_cell = ConsumeWithRetry(
//             Predictor,
//             MacroColumnOfAPC::FEEDBACKWARD_MESSAGE,
//             predictor_feedback_cursor,
//             manager,
//             older_counter,
//             "Predictor final drain feedback"
//         );

//         if (!motor_cell || !feedback_cell)
//         {
//             ok = false;
//             break;
//         }

//         motor_outputs.push_back(UnpackF32(*motor_cell));
//         feedback_outputs.push_back(UnpackF32(*feedback_cell));
//     }
//     const size_t sample_count = std::min<size_t>(traces.size(), 8);
    
//     PrintNodeSnapshot("Motor after final drain", Motor, true);
//     PrintNodeSnapshot("Predictor after final feedback drain", Predictor, true);

//     const auto end = std::chrono::steady_clock::now();

//     const uint64_t runtime_us =
//         static_cast<uint64_t>(
//             std::chrono::duration_cast<std::chrono::microseconds>(end - start).count()
//         );

//     const bool final_sensor_ok = ChainStrongInvariant(Sensor);
//     const bool final_predictor_ok = ChainStrongInvariant(Predictor);
//     const bool final_comparator_ok = ChainStrongInvariant(Comparator);
//     const bool final_integrator_ok = ChainStrongInvariant(Integrator);
//     const bool final_motor_ok = ChainStrongInvariant(Motor);

//     const bool final_ok =
//         ok &&
//         final_sensor_ok &&
//         final_predictor_ok &&
//         final_comparator_ok &&
//         final_integrator_ok &&
//         final_motor_ok;

//     std::cout << "\n============================================================\n";
//     std::cout << "Trace sample\n";
//     std::cout << "============================================================\n";

        
//     std::cout
//         << std::left
//         << std::setw(8) << "i"
//         << std::setw(12) << "sensor"
//         << std::setw(12) << "pred"
//         << std::setw(12) << "error"
//         << std::setw(12) << "state"
//         << std::setw(12) << "motor"
//         << std::setw(12) << "feedback"
//         << "\n";


//     for (size_t i = 0; i < sample_count; ++i)
//     {
//         const NeuralTrace& t = traces[i];

//         std::cout
//             << std::left
//             << std::setw(8) << t.Index
//             << std::setw(12) << std::fixed << std::setprecision(4) << t.Sensor
//             << std::setw(12) << t.Prediction
//             << std::setw(12) << t.Error
//             << std::setw(12) << t.State
//             << std::setw(12) << t.Motor
//             << std::setw(12) << t.Feedback
//             << "\n";
//     }

//     std::cout << "\n============================================================\n";
//     std::cout << "Growth summary\n";
//     std::cout << "============================================================\n";

//     std::cout << "runtime_us              : " << runtime_us << "\n";
//     std::cout << "BURST_N                 : " << BURST_N << "\n";
//     std::cout << "NODE_CAPACITY           : " << NODE_CAPACITY << "\n";
//     std::cout << "sensor_chain_length     : " << ChainLength(Sensor) << "\n";
//     std::cout << "predictor_chain_length  : " << ChainLength(Predictor) << "\n";
//     std::cout << "comparator_chain_length : " << ChainLength(Comparator) << "\n";
//     std::cout << "integrator_chain_length : " << ChainLength(Integrator) << "\n";
//     std::cout << "motor_chain_length      : " << ChainLength(Motor) << "\n";
//     std::cout << "grow_sensor             : " << grow_sensor.load() << "\n";
//     std::cout << "grow_predictor          : " << grow_predictor.load() << "\n";
//     std::cout << "grow_comparator         : " << grow_comparator.load() << "\n";
//     std::cout << "grow_integrator         : " << grow_integrator.load() << "\n";
//     std::cout << "grow_motor              : " << grow_motor.load() << "\n";
//     std::cout << "older_cells_observed    : " << older_counter.load() << "\n";
//     std::cout << "motor_outputs           : " << motor_outputs.size() << "\n";
//     std::cout << "feedback_outputs        : " << feedback_outputs.size() << "\n";

//     std::cout << "\n============================================================\n";
//     std::cout << "Final invariant summary\n";
//     std::cout << "============================================================\n";

//     std::cout << "Sensor     : " << PassFail(final_sensor_ok) << "\n";
//     std::cout << "Predictor  : " << PassFail(final_predictor_ok) << "\n";
//     std::cout << "Comparator : " << PassFail(final_comparator_ok) << "\n";
//     std::cout << "Integrator : " << PassFail(final_integrator_ok) << "\n";
//     std::cout << "Motor      : " << PassFail(final_motor_ok) << "\n";
//     std::cout << "overall    : " << PassFail(final_ok) << "\n";

//     manager.StopAPCManager();
    
// }