#include <Windows.h>
#include <chrono>
#include <cmath>
#include <gtest/gtest.h>
#include <numeric>
#include <thread>
#include <vector>

// Mock class to simulate FPS calculation logic from ArgumentDebuggerWindow
class FPSCalculator
{
  private:
    static constexpr int FPS_HISTORY_SIZE = 60;

    ULONGLONG last_update_time_ = 0;
    ULONGLONG last_frame_time_ = 0;
    ULONGLONG last_qr_update_time_ = 0;

    int fps_history_[FPS_HISTORY_SIZE] = {0};
    int fps_history_index_ = 0;
    int current_fps_ = 0;
    int synced_fps_ = 0;

  public:
    // Initialize timing
    void Initialize()
    {
        ULONGLONG current_time = GetTickCount64();
        last_update_time_ = current_time;
        last_frame_time_ = current_time;
        last_qr_update_time_ = current_time;

        // Initialize FPS history
        for (int i = 0; i < FPS_HISTORY_SIZE; ++i)
        {
            fps_history_[i] = 0;
        }
    }

    // Update FPS based on frame timing (similar to RenderFrame logic)
    void UpdateFrame()
    {
        ULONGLONG current_time = GetTickCount64();
        ULONGLONG elapsed = current_time - last_frame_time_;

        if (elapsed > 0)
        {
            // Calculate instantaneous FPS
            int instant_fps = static_cast<int>(1000.0f / elapsed);

            // Add to history for averaging
            fps_history_[fps_history_index_] = instant_fps;
            fps_history_index_ = (fps_history_index_ + 1) % FPS_HISTORY_SIZE;

            // Calculate average FPS
            int sum = 0;
            int count = 0;
            for (int i = 0; i < FPS_HISTORY_SIZE; ++i)
            {
                if (fps_history_[i] > 0)
                {
                    sum += fps_history_[i];
                    count++;
                }
            }

            if (count > 0)
            {
                current_fps_ = sum / count;
            }
        }

        last_frame_time_ = current_time;

        // Update synced FPS every 5 seconds (like QR code update)
        if (current_time - last_qr_update_time_ >= 5000)
        {
            synced_fps_ = current_fps_;
            last_qr_update_time_ = current_time;
        }
    }

    // Simulate frame rendering at specific FPS
    void SimulateFramesAtFPS(int target_fps, int duration_ms)
    {
        if (target_fps <= 0)
            return;

        int frame_time_ms = 1000 / target_fps;
        int frames = duration_ms / frame_time_ms;

        for (int i = 0; i < frames; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(frame_time_ms));
            UpdateFrame();
        }
    }

    // Getters for testing
    int GetCurrentFPS() const
    {
        return current_fps_;
    }
    int GetSyncedFPS() const
    {
        return synced_fps_;
    }
    int GetHistoryValue(int index) const
    {
        return (index >= 0 && index < FPS_HISTORY_SIZE) ? fps_history_[index] : 0;
    }

    // Reset for testing
    void Reset()
    {
        Initialize();
        fps_history_index_ = 0;
        current_fps_ = 0;
        synced_fps_ = 0;
    }
};

class FPSCalculationTest : public ::testing::Test
{
  protected:
    FPSCalculator fps_calc;

    void SetUp() override
    {
        fps_calc.Initialize();
    }
};

TEST_F(FPSCalculationTest, InitialState)
{
    EXPECT_EQ(fps_calc.GetCurrentFPS(), 0);
    EXPECT_EQ(fps_calc.GetSyncedFPS(), 0);

    // All history should be initialized to 0
    for (int i = 0; i < 60; ++i)
    {
        EXPECT_EQ(fps_calc.GetHistoryValue(i), 0);
    }
}

TEST_F(FPSCalculationTest, SingleFrameUpdate)
{
    // Wait a bit to ensure elapsed time > 0
    std::this_thread::sleep_for(std::chrono::milliseconds(20));

    fps_calc.UpdateFrame();

    // Should have calculated some FPS value
    EXPECT_GT(fps_calc.GetCurrentFPS(), 0);
}

TEST_F(FPSCalculationTest, Steady60FPS)
{
    fps_calc.SimulateFramesAtFPS(60, 2000); // 2 seconds at 60 FPS

    int current_fps = fps_calc.GetCurrentFPS();

    // Windows timing is not very accurate, allow 50% variance
    EXPECT_GT(current_fps, 30); // At least 30 FPS
    EXPECT_LT(current_fps, 90); // At most 90 FPS
}

TEST_F(FPSCalculationTest, Steady30FPS)
{
    fps_calc.SimulateFramesAtFPS(30, 2000); // 2 seconds at 30 FPS

    int current_fps = fps_calc.GetCurrentFPS();

    // Allow significant variance due to timing inaccuracy
    EXPECT_GT(current_fps, 15); // At least 15 FPS
    EXPECT_LT(current_fps, 45); // At most 45 FPS
}

TEST_F(FPSCalculationTest, Steady144FPS)
{
    fps_calc.SimulateFramesAtFPS(144, 1000); // 1 second at 144 FPS

    int current_fps = fps_calc.GetCurrentFPS();

    // High FPS is hardest to achieve accurately
    EXPECT_GT(current_fps, 50); // At least 50 FPS
    EXPECT_LT(current_fps, 200); // At most 200 FPS
}

TEST_F(FPSCalculationTest, SyncedFPSUpdatesEvery5Seconds)
{
    // Reset to ensure clean state
    fps_calc.Reset();
    
    // Initial synced FPS should be 0
    EXPECT_EQ(fps_calc.GetSyncedFPS(), 0);

    // Simulate 2 seconds at 60 FPS (well under 5 second threshold)
    fps_calc.SimulateFramesAtFPS(60, 2000);

    // Synced FPS should still be 0 (not 5 seconds yet)
    EXPECT_EQ(fps_calc.GetSyncedFPS(), 0);

    // Simulate 4 more seconds (total 6 seconds, well past 5 second threshold)
    fps_calc.SimulateFramesAtFPS(60, 4000);

    // Now synced FPS should be updated (timing may be inaccurate, so be lenient)
    EXPECT_GT(fps_calc.GetSyncedFPS(), 10); // Should be some reasonable value
}

TEST_F(FPSCalculationTest, FPSHistoryAveraging)
{
    // Start with steady 60 FPS
    fps_calc.SimulateFramesAtFPS(60, 1000);
    int fps_60 = fps_calc.GetCurrentFPS();

    // Switch to 30 FPS
    fps_calc.SimulateFramesAtFPS(30, 1000);
    int fps_after_switch = fps_calc.GetCurrentFPS();

    // FPS should be lower than initial due to averaging
    EXPECT_GT(fps_after_switch, 15); // At least 15 FPS
    EXPECT_LT(fps_after_switch, fps_60); // Less than initial FPS
}

TEST_F(FPSCalculationTest, VariableFPS)
{
    // Simulate variable frame times
    for (int i = 0; i < 100; ++i)
    {
        // Alternate between fast and slow frames
        int sleep_time = (i % 2 == 0) ? 16 : 33; // ~60fps and ~30fps
        std::this_thread::sleep_for(std::chrono::milliseconds(sleep_time));
        fps_calc.UpdateFrame();
    }

    int average_fps = fps_calc.GetCurrentFPS();

    // Should be somewhere between extremes but timing is unpredictable
    EXPECT_GT(average_fps, 15);
    EXPECT_LT(average_fps, 100);
}

TEST_F(FPSCalculationTest, VeryHighFPS)
{
    // Simulate very high FPS (minimal sleep)
    for (int i = 0; i < 200; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        fps_calc.UpdateFrame();
    }

    int high_fps = fps_calc.GetCurrentFPS();

    // Should be high (but Windows Sleep rarely allows >100 FPS)
    EXPECT_GT(high_fps, 50);
}

TEST_F(FPSCalculationTest, VeryLowFPS)
{
    // Simulate very low FPS
    for (int i = 0; i < 10; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(200)); // 5 FPS
        fps_calc.UpdateFrame();
    }

    int low_fps = fps_calc.GetCurrentFPS();

    // Should be around 5 FPS
    EXPECT_NEAR(low_fps, 5, 2);
}

TEST_F(FPSCalculationTest, FPSRecoveryAfterStall)
{
    // Start with steady 60 FPS
    fps_calc.SimulateFramesAtFPS(60, 1000);
    int initial_fps = fps_calc.GetCurrentFPS();
    EXPECT_GT(initial_fps, 20); // Should have reasonable FPS

    // Simulate a stall (long frame)
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    fps_calc.UpdateFrame();

    // FPS might drop or stay similar due to averaging
    int stalled_fps = fps_calc.GetCurrentFPS();
    EXPECT_GE(stalled_fps, 0); // Should be valid FPS

    // Resume normal 60 FPS
    fps_calc.SimulateFramesAtFPS(60, 2000);

    // FPS should recover
    int recovered_fps = fps_calc.GetCurrentFPS();
    EXPECT_GT(recovered_fps, 15); // Should have recovered
}

TEST_F(FPSCalculationTest, ZeroElapsedTimeHandling)
{
    // Two updates in rapid succession
    fps_calc.UpdateFrame();
    fps_calc.UpdateFrame(); // Might have 0 elapsed time

    // Should not crash or produce invalid values
    int fps = fps_calc.GetCurrentFPS();
    EXPECT_GE(fps, 0);
}

TEST_F(FPSCalculationTest, LongRunningStability)
{
    // Simulate long running at steady FPS
    fps_calc.SimulateFramesAtFPS(60, 10000); // 10 seconds

    int final_fps = fps_calc.GetCurrentFPS();

    // Should remain stable
    EXPECT_GT(final_fps, 20); // At least 20 FPS
    EXPECT_LT(final_fps, 100); // At most 100 FPS
}

TEST_F(FPSCalculationTest, MultipleSyncedFPSUpdates)
{
    // First update at 5 seconds
    fps_calc.SimulateFramesAtFPS(30, 5500);
    int first_sync = fps_calc.GetSyncedFPS();
    EXPECT_GT(first_sync, 10); // At least 10 FPS

    // Change to 60 FPS
    fps_calc.SimulateFramesAtFPS(60, 5000); // Another 5 seconds
    int second_sync = fps_calc.GetSyncedFPS();

    // Synced FPS should update to new value
    EXPECT_GT(second_sync, 15); // Should be reasonable FPS
}

TEST_F(FPSCalculationTest, ResetFunctionality)
{
    // Set up some state
    fps_calc.SimulateFramesAtFPS(120, 2000);
    EXPECT_GT(fps_calc.GetCurrentFPS(), 0);

    // Reset
    fps_calc.Reset();

    // Should be back to initial state
    EXPECT_EQ(fps_calc.GetCurrentFPS(), 0);
    EXPECT_EQ(fps_calc.GetSyncedFPS(), 0);
}