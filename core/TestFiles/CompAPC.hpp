#pragma once

// Drop-in extension for the existing APCTest.hpp from LifeOfSilicon(2).h.
// It deliberately REUSES CurrentArchitectureThroughputComparison instead of
// creating a second competing test framework.
#include "APCTest.hpp"

#include <cmath>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <string>

namespace TestSpace1
{
    // =====================================================================
    // Comprehensive APC vs std::vector + linked-list benchmark extension
    // =====================================================================

    struct BenchmarkOptions
    {
        uint32_t WarmupRuns = 2u;
        uint32_t MeasuredRuns = 9u;
        uint32_t MixedStressRuns = 5u;
        bool PrintEveryRun = false;
        bool PrintCSV = true;
    };

    struct TimingSummary
    {
        bool Valid = false;
        int64_t MinUs = 0;
        int64_t MedianUs = 0;
        int64_t P95Us = 0;
        int64_t MaxUs = 0;
        double MeanUs = 0.0;
        double StdDevUs = 0.0;
    };

    struct MemorySummary
    {
        uint64_t PersistentBytes = 0u;
        uint64_t PayloadCapacityBytes = 0u;
        uint64_t LogicalPayloadBytes = 0u;
        uint64_t GraphAndObjectBytes = 0u;
        uint64_t ScratchBytes = 0u;
        uint64_t ReservedButUnusedBytes = 0u;
    };

    struct MixedStressResult
    {
        bool Ok = false;
        PipelineResult Pipeline{};
        TimedResult BranchMutation{};
        int64_t ElapsedUs = 0;
    };

    struct ComprehensiveRunSet
    {
        std::vector<int64_t> PipelineUs;
        std::vector<int64_t> ChaseUs;
        std::vector<int64_t> MutationUs;
        std::vector<int64_t> MixedUs;

        bool AllPipelineOk = true;
        bool AllChaseOk = true;
        bool AllMutationOk = true;
        bool AllMixedOk = true;
        bool AllPayloadEqual = true;
    };

    // ---------------------------------------------------------------------
    // Timing statistics
    // ---------------------------------------------------------------------

    static TimingSummary SummarizeTimings(std::vector<int64_t> samples)
    {
        TimingSummary out{};
        if (samples.empty())
        {
            return out;
        }

        std::sort(samples.begin(), samples.end());

        const size_t n = samples.size();
        out.MinUs = samples.front();
        out.MaxUs = samples.back();
        out.MedianUs = samples[n / 2u];

        const size_t p95_index = std::min<size_t>(
            n - 1u,
            static_cast<size_t>(std::ceil(0.95 * static_cast<double>(n))) - 1u
        );
        out.P95Us = samples[p95_index];

        const double sum = std::accumulate(
            samples.begin(), samples.end(), 0.0,
            [](double a, int64_t b) { return a + static_cast<double>(b); });
        out.MeanUs = sum / static_cast<double>(n);

        double variance = 0.0;
        for (const int64_t s : samples)
        {
            const double d = static_cast<double>(s) - out.MeanUs;
            variance += d * d;
        }
        variance /= static_cast<double>(n);
        out.StdDevUs = std::sqrt(variance);
        out.Valid = true;
        return out;
    }

    static double SafeRatio(double numerator, double denominator) noexcept
    {
        return denominator > 0.0 ? numerator / denominator : 0.0;
    }

    static double OpsPerSecond(uint64_t operations, int64_t elapsed_us) noexcept
    {
        if (elapsed_us <= 0)
        {
            return 0.0;
        }
        return static_cast<double>(operations) * 1'000'000.0 /
            static_cast<double>(elapsed_us);
    }

    // ---------------------------------------------------------------------
    // Fixtures. Two extra nodes form an independent side branch used by the
    // mixed test. This lets data sharing proceed while topology mutates
    // elsewhere, which is exactly the "do graph work without halting Fabric"
    // property we want to exercise without introducing a baseline data race.
    // ---------------------------------------------------------------------

    struct VectorLinkedFixture
    {
        VectorNode Sensor{1u, VALUE_COUNT};
        VectorNode Predictor{2u, VALUE_COUNT};
        VectorNode Comparator{3u, VALUE_COUNT};
        VectorNode Integrator{4u, VALUE_COUNT};
        VectorNode Motor{5u, VALUE_COUNT};
        VectorNode SideOwner{6u, VALUE_COUNT};
        VectorNode SideChild{7u, VALUE_COUNT};

        bool Build() noexcept
        {
            Sensor.Horizontal.OwnsRoot = true;
            Integrator.Horizontal.OwnsRoot = true;
            Predictor.Vertical.OwnsRoot = true;
            Comparator.Vertical.OwnsRoot = true;

            SideOwner.Horizontal.OwnsRoot = true;
            SideOwner.Vertical.OwnsRoot = true;

            return
                VectorLinkTwoAPC(Sensor, Integrator, Axis::HORIZONTAL, Inheritance::FIRST_CHILD) &&
                VectorLinkTwoAPC(Integrator, Motor, Axis::HORIZONTAL, Inheritance::FIRST_CHILD) &&
                VectorLinkTwoAPC(Predictor, Comparator, Axis::VERTICAL, Inheritance::FIRST_CHILD) &&
                VectorLinkTwoAPC(Comparator, Motor, Axis::VERTICAL, Inheritance::FIRST_CHILD) &&
                VectorLinkTwoAPC(SideOwner, SideChild, Axis::HORIZONTAL, Inheritance::FIRST_CHILD) &&
                VectorLinkTwoAPC(SideOwner, SideChild, Axis::VERTICAL, Inheritance::FIRST_CHILD);
        }
    };

    struct APCFixture
    {
        VagueTemoraryPremativeFabric Fabric{};

        AdaptivePackedCellContainer Sensor{};
        AdaptivePackedCellContainer Predictor{};
        AdaptivePackedCellContainer Comparator{};
        AdaptivePackedCellContainer Integrator{};
        AdaptivePackedCellContainer Motor{};
        AdaptivePackedCellContainer SideOwner{};
        AdaptivePackedCellContainer SideChild{};

        uint32_t SensorSlot = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        uint32_t PredictorSlot = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        uint32_t ComparatorSlot = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        uint32_t IntegratorSlot = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        uint32_t MotorSlot = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        uint32_t SideOwnerSlot = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        uint32_t SideChildSlot = APCDataStructure::APC_INDEX_BOUND_SENTINAL;

        std::array<AdaptivePackedCellContainer*, FABRIC_SLOT_COUNT> SlotMap{};

        static bool ReadSlot(
            AdaptivePackedCellContainer& apc,
            uint32_t& out_slot) noexcept
        {
            uint64_t raw = FABRIC_CELL_SENTINAL;
            if (
                !apc.GetThisSlotIdx(raw) ||
                !APCDataStructure::IsValid32BitAPCUnit(raw))
            {
                return false;
            }
            out_slot = static_cast<uint32_t>(raw);
            return true;
        }

        bool Build() noexcept
        {
            SlotMap.fill(nullptr);

            if (!Fabric.InitializeFabricWithPtrTable(
                    FABRIC_SLOT_COUNT,
                    SLOT_WORDS,
                    CoreOfFabricCoordinator::DEFAULT_THREAD_TABLE_CAPACITY))
            {
                return false;
            }

            LayoutBoundsOrchestrator::LayoutSpanAndPercentageCarrier layout{};
            SchemaDefinition::InitialRegionalDtypeConf dtype{};
            SchemaDefinition::InitialRegionalProtocol protocol{};
            BuildFourRegionConfig(layout, dtype, protocol);

            if (
                !Fabric.CreateAPC(
                    Sensor, true, false, layout, dtype, protocol,
                    APCDataStructure::BRANCH_VERSION) ||
                !Fabric.CreateAPC(
                    Predictor, false, true, layout, dtype, protocol,
                    APCDataStructure::BRANCH_VERSION) ||
                !Fabric.CreateAPC(
                    Comparator, false, true, layout, dtype, protocol,
                    APCDataStructure::BRANCH_VERSION) ||
                !Fabric.CreateAPC(
                    Integrator, true, false, layout, dtype, protocol,
                    APCDataStructure::BRANCH_VERSION) ||
                !Fabric.CreateAPC(
                    Motor, false, false, layout, dtype, protocol,
                    APCDataStructure::BRANCH_VERSION) ||
                !Fabric.CreateAPC(
                    SideOwner, true, true, layout, dtype, protocol,
                    APCDataStructure::BRANCH_VERSION) ||
                !Fabric.CreateAPC(
                    SideChild, false, false, layout, dtype, protocol,
                    APCDataStructure::BRANCH_VERSION))
            {
                return false;
            }

            if (
                !ReadSlot(Sensor, SensorSlot) ||
                !ReadSlot(Predictor, PredictorSlot) ||
                !ReadSlot(Comparator, ComparatorSlot) ||
                !ReadSlot(Integrator, IntegratorSlot) ||
                !ReadSlot(Motor, MotorSlot) ||
                !ReadSlot(SideOwner, SideOwnerSlot) ||
                !ReadSlot(SideChild, SideChildSlot))
            {
                return false;
            }

            for (const uint32_t slot : {
                    SensorSlot, PredictorSlot, ComparatorSlot, IntegratorSlot,
                    MotorSlot, SideOwnerSlot, SideChildSlot })
            {
                if (slot >= SlotMap.size())
                {
                    return false;
                }
            }

            SlotMap[SensorSlot] = &Sensor;
            SlotMap[PredictorSlot] = &Predictor;
            SlotMap[ComparatorSlot] = &Comparator;
            SlotMap[IntegratorSlot] = &Integrator;
            SlotMap[MotorSlot] = &Motor;
            SlotMap[SideOwnerSlot] = &SideOwner;
            SlotMap[SideChildSlot] = &SideChild;

            return
                Sensor.AttachAnotherToMe(Integrator, Axis::HORIZONTAL, Inheritance::FIRST_CHILD) &&
                Integrator.AttachAnotherToMe(Motor, Axis::HORIZONTAL, Inheritance::FIRST_CHILD) &&
                Predictor.AttachAnotherToMe(Comparator, Axis::VERTICAL, Inheritance::FIRST_CHILD) &&
                Comparator.AttachAnotherToMe(Motor, Axis::VERTICAL, Inheritance::FIRST_CHILD) &&
                SideOwner.AttachMeToAnother(SideChild, Axis::HORIZONTAL, Inheritance::FIRST_CHILD) &&
                SideOwner.AttachMeToAnother(SideChild, Axis::VERTICAL, Inheritance::FIRST_CHILD);
        }
    };

    // ---------------------------------------------------------------------
    // Topology validation
    // ---------------------------------------------------------------------

    static bool ValidateVectorTopology(VectorLinkedFixture& f) noexcept
    {
        return
            VectorOwnedFirstChild(f.Sensor, Axis::HORIZONTAL) == &f.Integrator &&
            VectorOwnedFirstChild(f.Integrator, Axis::HORIZONTAL) == &f.Motor &&
            VectorOwnedFirstChild(f.Predictor, Axis::VERTICAL) == &f.Comparator &&
            VectorOwnedFirstChild(f.Comparator, Axis::VERTICAL) == &f.Motor &&
            VectorOwnedFirstChild(f.SideOwner, Axis::HORIZONTAL) == &f.SideChild &&
            VectorOwnedFirstChild(f.SideOwner, Axis::VERTICAL) == &f.SideChild &&
            AxisState(f.Motor, Axis::HORIZONTAL).Owner == &f.Integrator &&
            AxisState(f.Motor, Axis::VERTICAL).Owner == &f.Comparator &&
            AxisState(f.SideChild, Axis::HORIZONTAL).Owner == &f.SideOwner &&
            AxisState(f.SideChild, Axis::VERTICAL).Owner == &f.SideOwner;
    }

    static bool ValidateAPCLocksReleased(APCFixture& f) noexcept
    {
        for (const uint32_t slot : {
                f.SensorSlot, f.PredictorSlot, f.ComparatorSlot,
                f.IntegratorSlot, f.MotorSlot,
                f.SideOwnerSlot, f.SideChildSlot })
        {
            InstallAxisToBuffer::GraphMutationValues values{};
            if (
                !f.Fabric.ReadGraphMutationFlags(slot, values) ||
                !InstallAxisToBuffer::IsIdentityGraphUnlocked(values.Flags))
            {
                return false;
            }
        }
        return true;
    }

    static bool ValidateAPCTopology(APCFixture& f) noexcept
    {
        uint32_t h1 = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        uint32_t h2 = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        uint32_t v1 = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        uint32_t v2 = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        uint32_t sh = APCDataStructure::APC_INDEX_BOUND_SENTINAL;
        uint32_t sv = APCDataStructure::APC_INDEX_BOUND_SENTINAL;

        return
            ResolveOwnedFirstChildSlot(
                f.Fabric, f.SensorSlot, Axis::HORIZONTAL, h1) &&
            ResolveOwnedFirstChildSlot(
                f.Fabric, h1, Axis::HORIZONTAL, h2) &&
            ResolveOwnedFirstChildSlot(
                f.Fabric, f.PredictorSlot, Axis::VERTICAL, v1) &&
            ResolveOwnedFirstChildSlot(
                f.Fabric, v1, Axis::VERTICAL, v2) &&
            ResolveOwnedFirstChildSlot(
                f.Fabric, f.SideOwnerSlot, Axis::HORIZONTAL, sh) &&
            ResolveOwnedFirstChildSlot(
                f.Fabric, f.SideOwnerSlot, Axis::VERTICAL, sv) &&
            h1 == f.IntegratorSlot &&
            h2 == f.MotorSlot &&
            v1 == f.ComparatorSlot &&
            v2 == f.MotorSlot &&
            sh == f.SideChildSlot &&
            sv == f.SideChildSlot &&
            ValidateAPCLocksReleased(f);
    }

    // ---------------------------------------------------------------------
    // Mixed data-plane/control-plane stress.
    // The main five-node pipeline executes while the independent side branch
    // performs H and V unlink/relink cycles. This is intentionally not a
    // same-edge transaction benchmark; it asks whether unrelated topology
    // mutation stalls or corrupts region dataflow.
    // ---------------------------------------------------------------------

    static MixedStressResult RunVectorMixedStress(VectorLinkedFixture& f)
    {
        MixedStressResult out{};
        const auto begin = Clock::now();

        std::thread pipeline_thread([&]
        {
            out.Pipeline = RunVectorPipeline(
                f.Sensor, f.Predictor, f.Comparator, f.Integrator, f.Motor);
        });

        out.BranchMutation = RunVectorDynamicBranchMutation(
            f.SideOwner, f.SideOwner, f.SideChild);

        pipeline_thread.join();
        const auto end = Clock::now();

        out.ElapsedUs = std::chrono::duration_cast<Microseconds>(end - begin).count();
        out.Ok =
            out.Pipeline.Ok &&
            out.BranchMutation.Ok &&
            ValidateVectorTopology(f);
        return out;
    }

    static MixedStressResult RunAPCMixedStress(APCFixture& f)
    {
        MixedStressResult out{};
        const auto begin = Clock::now();

        std::thread pipeline_thread([&]
        {
            out.Pipeline = RunAPCPipeline(
                f.Fabric,
                f.Sensor,
                f.Predictor,
                f.Comparator,
                f.Integrator,
                f.Motor,
                f.SensorSlot,
                f.PredictorSlot,
                f.ComparatorSlot,
                f.IntegratorSlot,
                f.MotorSlot,
                f.SlotMap);
        });

        out.BranchMutation = RunAPCDynamicBranchMutation(
            f.Fabric,
            f.SideOwnerSlot,
            f.SideOwnerSlot,
            f.SideChildSlot,
            f.SideOwner,
            f.SideOwner,
            f.SideChild
        );

        pipeline_thread.join();
        const auto end = Clock::now();

        out.ElapsedUs = std::chrono::duration_cast<Microseconds>(end - begin).count();
        out.Ok =
            out.Pipeline.Ok &&
            out.BranchMutation.Ok &&
            ValidateAPCTopology(f);
        return out;
    }

    // ---------------------------------------------------------------------
    // Memory accounting.
    // This is structural memory, not process RSS. That is intentional:
    // process RSS is allocator/OS/noise dependent and obscures what each
    // representation actually reserves.
    // ---------------------------------------------------------------------

    static uint64_t VectorNodeHeapBytes(const VectorNode& n) noexcept
    {
        return
            static_cast<uint64_t>(n.FeedForward.capacity()) * sizeof(uint64_t) +
            static_cast<uint64_t>(n.FeedBackward.capacity()) * sizeof(uint64_t) +
            static_cast<uint64_t>(n.State.capacity()) * sizeof(uint64_t) +
            static_cast<uint64_t>(n.Error.capacity()) * sizeof(uint64_t);
    }

    static MemorySummary MeasureVectorMemory(const VectorLinkedFixture& f) noexcept
    {
        MemorySummary m{};

        const std::array<const VectorNode*, 7> nodes{
            &f.Sensor, &f.Predictor, &f.Comparator, &f.Integrator,
            &f.Motor, &f.SideOwner, &f.SideChild
        };

        uint64_t heap_payload = 0u;
        for (const VectorNode* n : nodes)
        {
            heap_payload += VectorNodeHeapBytes(*n);
        }

        const uint64_t object_bytes =
            static_cast<uint64_t>(nodes.size()) * sizeof(VectorNode);

        // Main pipeline semantically consumes five VALUE_COUNT-sized streams:
        // sensor FF, predictor FB, integrator STATE, comparator ERROR, motor FF.
        const uint64_t logical_payload =
            5ull * VALUE_COUNT * sizeof(uint64_t);

        m.PersistentBytes = heap_payload + object_bytes;
        m.PayloadCapacityBytes = heap_payload;
        m.LogicalPayloadBytes = logical_payload;
        m.GraphAndObjectBytes = object_bytes;
        m.ScratchBytes = 2ull * VALUE_COUNT * sizeof(uint64_t); // ready arrays
        m.ReservedButUnusedBytes =
            m.PayloadCapacityBytes > logical_payload
                ? m.PayloadCapacityBytes - logical_payload
                : 0u;
        return m;
    }

    static MemorySummary MeasureAPCMemory(APCFixture& f) noexcept
    {
        MemorySummary m{};

        using FMI = CoreOfFabricCoordinator::FabricMetaIndicies;
        uint64_t total_cells = 0u;

        if (!f.Fabric.ReadAFabricU64Directly(
                static_cast<size_t>(FMI::TOTAL_CELLS),
                total_cells))
        {
            return m;
        }

        const uint64_t slab_bytes = total_cells * sizeof(uint64_t);
        const uint64_t host_apc_objects =
            7ull * sizeof(AdaptivePackedCellContainer);
        const uint64_t runtime_ptr_table =
            static_cast<uint64_t>(FABRIC_SLOT_COUNT) *
            sizeof(std::atomic<AdaptivePackedCellContainer*>);

        const uint64_t active_segment_capacity =
            7ull * SLOT_WORDS * sizeof(uint64_t);
        const uint64_t active_payload_capacity =
            7ull *
            (SLOT_WORDS - static_cast<uint32_t>(APCDataStructure::METACELL_COUNT)) *
            sizeof(uint64_t);
        const uint64_t logical_payload =
            5ull * VALUE_COUNT * sizeof(uint64_t);

        m.PersistentBytes = slab_bytes + host_apc_objects + runtime_ptr_table;
        m.PayloadCapacityBytes = active_payload_capacity;
        m.LogicalPayloadBytes = logical_payload;
        m.GraphAndObjectBytes =
            (slab_bytes > active_segment_capacity ? slab_bytes - active_segment_capacity : 0u) +
            host_apc_objects + runtime_ptr_table +
            7ull * static_cast<uint64_t>(APCDataStructure::METACELL_COUNT) * sizeof(uint64_t);
        m.ScratchBytes = 2ull * VALUE_COUNT * sizeof(uint64_t); // same ready arrays
        m.ReservedButUnusedBytes =
            m.PayloadCapacityBytes > logical_payload
                ? m.PayloadCapacityBytes - logical_payload
                : 0u;
        return m;
    }

    // ---------------------------------------------------------------------
    // Human-readable reporting
    // ---------------------------------------------------------------------

    static void PrintLine(char ch = '-', size_t width = 88u)
    {
        for (size_t i = 0u; i < width; ++i)
        {
            std::cout << ch;
        }
        std::cout << '\n';
    }

    static void PrintTimingSummary(
        const char* label,
        const TimingSummary& s,
        uint64_t operations)
    {
        if (!s.Valid)
        {
            std::cout << std::left << std::setw(28) << label << " no samples\n";
            return;
        }

        std::cout
            << std::left << std::setw(28) << label
            << " median=" << std::right << std::setw(9) << s.MedianUs << " us"
            << "  mean=" << std::setw(11) << std::fixed << std::setprecision(1) << s.MeanUs
            << "  p95=" << std::setw(9) << s.P95Us
            << "  min/max=" << s.MinUs << '/' << s.MaxUs
            << "  sd=" << std::setprecision(1) << s.StdDevUs;

        if (operations != 0u)
        {
            std::cout
                << "  throughput="
                << std::setprecision(2)
                << OpsPerSecond(operations, s.MedianUs)
                << " ops/s";
        }
        std::cout << '\n';
    }

    static void PrintMemorySummary(const char* label, const MemorySummary& m)
    {
        auto KiB = [](uint64_t b) { return static_cast<double>(b) / 1024.0; };
        const double utilization =
            m.PayloadCapacityBytes != 0u
                ? 100.0 * static_cast<double>(m.LogicalPayloadBytes) /
                    static_cast<double>(m.PayloadCapacityBytes)
                : 0.0;

        std::cout
            << label << '\n'
            << "  persistent structural memory : " << std::fixed << std::setprecision(2)
            << KiB(m.PersistentBytes) << " KiB\n"
            << "  payload capacity             : " << KiB(m.PayloadCapacityBytes) << " KiB\n"
            << "  logical payload used by test : " << KiB(m.LogicalPayloadBytes) << " KiB\n"
            << "  payload utilization          : " << utilization << "%\n"
            << "  graph/object/control estimate: " << KiB(m.GraphAndObjectBytes) << " KiB\n"
            << "  transient ready scratch      : " << KiB(m.ScratchBytes) << " KiB\n"
            << "  reserved payload not used    : " << KiB(m.ReservedButUnusedBytes) << " KiB\n";
    }

    // ---------------------------------------------------------------------
    // Repeated benchmark runner
    // ---------------------------------------------------------------------

    static bool RunWarmups(VectorLinkedFixture& v, APCFixture& a, uint32_t count)
    {
        for (uint32_t i = 0u; i < count; ++i)
        {
            const PipelineResult vp = RunVectorPipeline(
                v.Sensor, v.Predictor, v.Comparator, v.Integrator, v.Motor);
            const PipelineResult ap = RunAPCPipeline(
                a.Fabric,
                a.Sensor, a.Predictor, a.Comparator, a.Integrator, a.Motor,
                a.SensorSlot, a.PredictorSlot, a.ComparatorSlot,
                a.IntegratorSlot, a.MotorSlot, a.SlotMap);

            if (!vp.Ok || !ap.Ok)
            {
                return false;
            }
        }
        return true;
    }

    static bool CollectMeasuredRuns(
        VectorLinkedFixture& v,
        APCFixture& a,
        const BenchmarkOptions& options,
        ComprehensiveRunSet& vector_runs,
        ComprehensiveRunSet& apc_runs)
    {
        for (uint32_t run = 0u; run < options.MeasuredRuns; ++run)
        {
            PipelineResult vp{};
            PipelineResult ap{};

            // Alternate ordering to reduce systematic thermal/order bias.
            if ((run & 1u) == 0u)
            {
                vp = RunVectorPipeline(v.Sensor, v.Predictor, v.Comparator, v.Integrator, v.Motor);
                ap = RunAPCPipeline(
                    a.Fabric,
                    a.Sensor, a.Predictor, a.Comparator, a.Integrator, a.Motor,
                    a.SensorSlot, a.PredictorSlot, a.ComparatorSlot,
                    a.IntegratorSlot, a.MotorSlot, a.SlotMap);
            }
            else
            {
                ap = RunAPCPipeline(
                    a.Fabric,
                    a.Sensor, a.Predictor, a.Comparator, a.Integrator, a.Motor,
                    a.SensorSlot, a.PredictorSlot, a.ComparatorSlot,
                    a.IntegratorSlot, a.MotorSlot, a.SlotMap);
                vp = RunVectorPipeline(v.Sensor, v.Predictor, v.Comparator, v.Integrator, v.Motor);
            }

            vector_runs.PipelineUs.push_back(vp.ElapsedUs);
            apc_runs.PipelineUs.push_back(ap.ElapsedUs);
            vector_runs.AllPipelineOk &= vp.Ok;
            apc_runs.AllPipelineOk &= ap.Ok;

            const bool payload_equal = ComparePipelinePayloads(
                v.Integrator, v.Comparator, v.Motor,
                a.Integrator, a.Comparator, a.Motor);
            vector_runs.AllPayloadEqual &= payload_equal;
            apc_runs.AllPayloadEqual &= payload_equal;

            const TimedResult vc = RunVectorPointerChase(v.Sensor, v.Predictor, v.Motor);
            const TimedResult ac = RunAPCSlotChase(
                a.Fabric, a.SensorSlot, a.PredictorSlot, a.MotorSlot);

            vector_runs.ChaseUs.push_back(vc.ElapsedUs);
            apc_runs.ChaseUs.push_back(ac.ElapsedUs);
            vector_runs.AllChaseOk &= vc.Ok;
            apc_runs.AllChaseOk &= ac.Ok;

            const TimedResult vm = RunVectorDynamicBranchMutation(
                v.Integrator, v.Comparator, v.Motor);
            const TimedResult am = RunAPCDynamicBranchMutation(
                a.Fabric, a.IntegratorSlot, a.ComparatorSlot, a.MotorSlot,
                a.Integrator, a.Comparator, a.Motor
            );

            vector_runs.MutationUs.push_back(vm.ElapsedUs);
            apc_runs.MutationUs.push_back(am.ElapsedUs);
            vector_runs.AllMutationOk &= vm.Ok;
            apc_runs.AllMutationOk &= am.Ok;

            if (options.PrintEveryRun)
            {
                std::cout
                    << "run " << run
                    << " pipeline(us) vector/apc=" << vp.ElapsedUs << '/' << ap.ElapsedUs
                    << " chase=" << vc.ElapsedUs << '/' << ac.ElapsedUs
                    << " mutation=" << vm.ElapsedUs << '/' << am.ElapsedUs
                    << " payload=" << (payload_equal ? "OK" : "BAD")
                    << '\n';
            }
        }

        for (uint32_t run = 0u; run < options.MixedStressRuns; ++run)
        {
            MixedStressResult vm{};
            MixedStressResult am{};

            if ((run & 1u) == 0u)
            {
                vm = RunVectorMixedStress(v);
                am = RunAPCMixedStress(a);
            }
            else
            {
                am = RunAPCMixedStress(a);
                vm = RunVectorMixedStress(v);
            }

            vector_runs.MixedUs.push_back(vm.ElapsedUs);
            apc_runs.MixedUs.push_back(am.ElapsedUs);
            vector_runs.AllMixedOk &= vm.Ok;
            apc_runs.AllMixedOk &= am.Ok;

            const bool payload_equal = ComparePipelinePayloads(
                v.Integrator, v.Comparator, v.Motor,
                a.Integrator, a.Comparator, a.Motor);
            vector_runs.AllPayloadEqual &= payload_equal;
            apc_runs.AllPayloadEqual &= payload_equal;
        }

        return
            vector_runs.AllPipelineOk &&
            apc_runs.AllPipelineOk &&
            vector_runs.AllChaseOk &&
            apc_runs.AllChaseOk &&
            vector_runs.AllMutationOk &&
            apc_runs.AllMutationOk &&
            vector_runs.AllMixedOk &&
            apc_runs.AllMixedOk &&
            vector_runs.AllPayloadEqual &&
            apc_runs.AllPayloadEqual;
    }

    static void PrintCSVSummary(
        const ComprehensiveRunSet& v,
        const ComprehensiveRunSet& a,
        const MemorySummary& vm,
        const MemorySummary& am)
    {
        const TimingSummary vp = SummarizeTimings(v.PipelineUs);
        const TimingSummary ap = SummarizeTimings(a.PipelineUs);
        const TimingSummary vc = SummarizeTimings(v.ChaseUs);
        const TimingSummary ac = SummarizeTimings(a.ChaseUs);
        const TimingSummary vb = SummarizeTimings(v.MutationUs);
        const TimingSummary ab = SummarizeTimings(a.MutationUs);
        const TimingSummary vx = SummarizeTimings(v.MixedUs);
        const TimingSummary ax = SummarizeTimings(a.MixedUs);

        std::cout
            << "\nCSV_SUMMARY\n"
            << "system,pipeline_median_us,chase_median_us,mutation_median_us,mixed_median_us,persistent_bytes,payload_capacity_bytes,logical_payload_bytes\n"
            << "vector_linked," << vp.MedianUs << ',' << vc.MedianUs << ',' << vb.MedianUs << ',' << vx.MedianUs << ','
            << vm.PersistentBytes << ',' << vm.PayloadCapacityBytes << ',' << vm.LogicalPayloadBytes << '\n'
            << "apc_fabric," << ap.MedianUs << ',' << ac.MedianUs << ',' << ab.MedianUs << ',' << ax.MedianUs << ','
            << am.PersistentBytes << ',' << am.PayloadCapacityBytes << ',' << am.LogicalPayloadBytes << '\n';
    }

    inline int RunComprehensiveArchitectureComparison(
        const BenchmarkOptions& options = BenchmarkOptions{})
    {
        std::cout << "\n";
        PrintLine('=');
        std::cout << "APC/FABRIC vs std::vector + LINKED-LIST COMPREHENSIVE ARCHITECTURE TEST\n";
        PrintLine('=');
        std::cout
            << "Purpose: compare identical logical work while separating data-plane,\n"
            << "control-plane, dynamic topology, and mixed-interference costs.\n\n"
            << "Main topology:\n"
            << "  H: SENSOR -> INTEGRATOR -> MOTOR\n"
            << "  V: PREDICTOR -> COMPARATOR -> MOTOR\n"
            << "Side topology used for concurrent churn:\n"
            << "  H: SIDE_OWNER -> SIDE_CHILD\n"
            << "  V: SIDE_OWNER -> SIDE_CHILD\n\n"
            << "Regions per APC/node: FF, FB, STATE, ERROR\n"
            << "FF/FB protocol       : IMMUTABLE_SNAPSHOT\n"
            << "STATE/ERROR protocol : PRIVATE_REGION + external release/acquire ready flag\n"
            << "Values               : " << VALUE_COUNT << '\n'
            << "Pipeline workers     : "
            << (PRODUCER_COUNT + FF_WORKER_COUNT + FB_WORKER_COUNT + FINAL_WORKER_COUNT) << '\n'
            << "Graph chase rounds   : " << CHASE_ROUNDS << '\n'
            << "Branch rounds/axis   : " << DYNAMIC_BRANCH_ROUNDS << '\n'
            << "Warmups               : " << options.WarmupRuns << '\n'
            << "Measured runs         : " << options.MeasuredRuns << '\n'
            << "Mixed stress runs     : " << options.MixedStressRuns << "\n\n";

        // --------------------------------------------------------------
        // Construction timing
        // --------------------------------------------------------------
        const auto vector_build_begin = Clock::now();
        auto vector_fixture = std::make_unique<VectorLinkedFixture>();
        const bool vector_build_ok = vector_fixture && vector_fixture->Build();
        const auto vector_build_end = Clock::now();

        const auto apc_build_begin = Clock::now();
        auto apc_fixture = std::make_unique<APCFixture>();
        const bool apc_build_ok = apc_fixture && apc_fixture->Build();
        const auto apc_build_end = Clock::now();

        const int64_t vector_build_us = std::chrono::duration_cast<Microseconds>(
            vector_build_end - vector_build_begin).count();
        const int64_t apc_build_us = std::chrono::duration_cast<Microseconds>(
            apc_build_end - apc_build_begin).count();

        if (!vector_build_ok || !apc_build_ok)
        {
            std::cout
                << "construction vector/APC : "
                << (vector_build_ok ? "PASS" : "FAIL") << '/'
                << (apc_build_ok ? "PASS" : "FAIL") << '\n';
            return 1;
        }

        const bool initial_vector_topology = ValidateVectorTopology(*vector_fixture);
        const bool initial_apc_topology = ValidateAPCTopology(*apc_fixture);

        std::cout
            << "CONSTRUCTION\n"
            << "  vector + linked-list : " << (vector_build_ok ? "PASS" : "FAIL")
            << "  " << vector_build_us << " us\n"
            << "  APC/Fabric            : " << (apc_build_ok ? "PASS" : "FAIL")
            << "  " << apc_build_us << " us\n"
            << "  vector topology       : " << (initial_vector_topology ? "PASS" : "FAIL") << '\n'
            << "  APC topology + locks  : " << (initial_apc_topology ? "PASS" : "FAIL") << "\n\n";

        if (!initial_vector_topology || !initial_apc_topology)
        {
            return 2;
        }

        // --------------------------------------------------------------
        // Warmup and measured execution
        // --------------------------------------------------------------
        if (!RunWarmups(*vector_fixture, *apc_fixture, options.WarmupRuns))
        {
            std::cout << "warmup : FAIL\n";
            return 3;
        }

        ComprehensiveRunSet vector_runs{};
        ComprehensiveRunSet apc_runs{};

        const bool measured_ok = CollectMeasuredRuns(
            *vector_fixture,
            *apc_fixture,
            options,
            vector_runs,
            apc_runs);

        const TimingSummary vector_pipeline = SummarizeTimings(vector_runs.PipelineUs);
        const TimingSummary apc_pipeline = SummarizeTimings(apc_runs.PipelineUs);
        const TimingSummary vector_chase = SummarizeTimings(vector_runs.ChaseUs);
        const TimingSummary apc_chase = SummarizeTimings(apc_runs.ChaseUs);
        const TimingSummary vector_mutation = SummarizeTimings(vector_runs.MutationUs);
        const TimingSummary apc_mutation = SummarizeTimings(apc_runs.MutationUs);
        const TimingSummary vector_mixed = SummarizeTimings(vector_runs.MixedUs);
        const TimingSummary apc_mixed = SummarizeTimings(apc_runs.MixedUs);

        PrintLine();
        std::cout << "DATA-PLANE: region synchronization + sharing\n";
        PrintTimingSummary("vector pipeline", vector_pipeline, VALUE_COUNT);
        PrintTimingSummary("APC pipeline", apc_pipeline, VALUE_COUNT);
        std::cout
            << "  APC/vector median ratio : " << std::fixed << std::setprecision(3)
            << SafeRatio(static_cast<double>(apc_pipeline.MedianUs), static_cast<double>(vector_pipeline.MedianUs)) << "x\n"
            << "  payload equality        : "
            << (vector_runs.AllPayloadEqual && apc_runs.AllPayloadEqual ? "PASS" : "FAIL") << "\n\n";

        std::cout << "CONTROL-PLANE: two-level H + two-level V traversal\n";
        PrintTimingSummary(
            "vector linked chase",
            vector_chase,
            static_cast<uint64_t>(CHASE_ROUNDS) * 4ull);
        PrintTimingSummary(
            "APC identity chase",
            apc_chase,
            static_cast<uint64_t>(CHASE_ROUNDS) * 4ull);
        std::cout
            << "  APC/vector median ratio : " << std::fixed << std::setprecision(3)
            << SafeRatio(static_cast<double>(apc_chase.MedianUs), static_cast<double>(vector_chase.MedianUs)) << "x\n\n";

        std::cout << "CONTROL-PLANE: concurrent H + V unlink/relink\n";
        const uint64_t mutation_calls =
            static_cast<uint64_t>(DYNAMIC_BRANCH_ROUNDS) * 4ull;
        PrintTimingSummary("vector branch mutation", vector_mutation, mutation_calls);
        PrintTimingSummary("APC branch mutation", apc_mutation, mutation_calls);
        std::cout
            << "  primitive operations/run: " << mutation_calls
            << " (H unlink+link + V unlink+link)\n"
            << "  APC/vector median ratio : " << std::fixed << std::setprecision(3)
            << SafeRatio(static_cast<double>(apc_mutation.MedianUs), static_cast<double>(vector_mutation.MedianUs)) << "x\n\n";

        std::cout << "MIXED STRESS: dataflow while unrelated H+V branches mutate\n";
        PrintTimingSummary("vector mixed", vector_mixed, VALUE_COUNT + mutation_calls);
        PrintTimingSummary("APC mixed", apc_mixed, VALUE_COUNT + mutation_calls);
        std::cout
            << "  vector mixed correctness : " << (vector_runs.AllMixedOk ? "PASS" : "FAIL") << '\n'
            << "  APC mixed correctness    : " << (apc_runs.AllMixedOk ? "PASS" : "FAIL") << '\n'
            << "  APC locks released       : "
            << (ValidateAPCLocksReleased(*apc_fixture) ? "PASS" : "FAIL") << "\n\n";

        // --------------------------------------------------------------
        // Memory
        // --------------------------------------------------------------
        const MemorySummary vector_memory = MeasureVectorMemory(*vector_fixture);
        const MemorySummary apc_memory = MeasureAPCMemory(*apc_fixture);

        PrintLine();
        std::cout << "STRUCTURAL MEMORY ACCOUNTING\n";
        std::cout
            << "This is deterministic representation memory, not process RSS.\n"
            << "APC's slab reserves all " << FABRIC_SLOT_COUNT << " slots; the vector baseline\n"
            << "allocates only the seven nodes used by this fixture. Report both the raw\n"
            << "numbers and utilization; do not call the raw ratio an allocator-independent result.\n\n";
        PrintMemorySummary("vector + linked-list", vector_memory);
        std::cout << '\n';
        PrintMemorySummary("APC/Fabric", apc_memory);
        std::cout
            << "\n  APC/vector persistent ratio : " << std::fixed << std::setprecision(3)
            << SafeRatio(static_cast<double>(apc_memory.PersistentBytes), static_cast<double>(vector_memory.PersistentBytes)) << "x\n";

        // --------------------------------------------------------------
        // Complexity and interpretation
        // --------------------------------------------------------------
        std::cout << '\n';

        PrintLine();
        std::cout << "CORRECTNESS MATRIX\n"
            << "  vector pipeline all runs      : " << (vector_runs.AllPipelineOk ? "PASS" : "FAIL") << '\n'
            << "  APC pipeline all runs         : " << (apc_runs.AllPipelineOk ? "PASS" : "FAIL") << '\n'
            << "  payload equivalence           : " << (vector_runs.AllPayloadEqual ? "PASS" : "FAIL") << '\n'
            << "  vector chase all runs         : " << (vector_runs.AllChaseOk ? "PASS" : "FAIL") << '\n'
            << "  APC chase all runs            : " << (apc_runs.AllChaseOk ? "PASS" : "FAIL") << '\n'
            << "  vector branch all runs        : " << (vector_runs.AllMutationOk ? "PASS" : "FAIL") << '\n'
            << "  APC branch all runs           : " << (apc_runs.AllMutationOk ? "PASS" : "FAIL") << '\n'
            << "  vector mixed all runs         : " << (vector_runs.AllMixedOk ? "PASS" : "FAIL") << '\n'
            << "  APC mixed all runs            : " << (apc_runs.AllMixedOk ? "PASS" : "FAIL") << '\n'
            << "  final vector topology         : " << (ValidateVectorTopology(*vector_fixture) ? "PASS" : "FAIL") << '\n'
            << "  final APC topology            : " << (ValidateAPCTopology(*apc_fixture) ? "PASS" : "FAIL") << '\n'
            << "  final APC graph locks         : " << (ValidateAPCLocksReleased(*apc_fixture) ? "PASS" : "FAIL") << '\n';

        const bool final_ok =
            measured_ok &&
            ValidateVectorTopology(*vector_fixture) &&
            ValidateAPCTopology(*apc_fixture) &&
            ValidateAPCLocksReleased(*apc_fixture);

        if (options.PrintCSV)
        {
            PrintCSVSummary(
                vector_runs,
                apc_runs,
                vector_memory,
                apc_memory);
        }

        PrintLine('=');
        std::cout << "OVERALL COMPREHENSIVE ARCHITECTURE TEST : "
            << (final_ok ? "PASS" : "FAIL") << '\n';
        PrintLine('=');

        return final_ok ? 0 : 4;
    }
}
