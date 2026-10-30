// SD-Client: ping display and connection diagnostics.
// Only real measurements are shown: nothing here changes or fakes the network latency.
#ifndef GAME_CLIENT_COMPONENTS_SD_PING_H
#define GAME_CLIENT_COMPONENTS_SD_PING_H

#include <game/client/component.h>

#include <cstdint>

class CSdPing : public CComponent
{
public:
	static constexpr int NUM_SAMPLES = 120; // ~30 s with 4 samples per second

	struct SStats
	{
		bool m_Valid;
		int m_Current;
		int m_Delta; // change compared to ~1 second ago
		int m_Min;
		int m_Max;
		float m_Average;
		float m_Jitter; // mean absolute difference between consecutive samples
		int m_PredictionTimeMs;
		int m_NumSamples;
		int m_NumHighPingEvents;
	};

	int Sizeof() const override { return sizeof(*this); }
	void OnReset() override;
	void OnRender() override;

	SStats Stats() const;
	int Sample(int Index) const { return m_aSamples[(m_Head + Index) % NUM_SAMPLES]; } // 0 = oldest
	int NumSamples() const { return m_NumSamples; }

private:
	int m_aSamples[NUM_SAMPLES] = {};
	int m_Head = 0;
	int m_NumSamples = 0;
	int64_t m_LastSampleTime = 0;
	int m_ConsecutiveHigh = 0;
	int64_t m_LastWarnTime = 0;
	int m_NumHighPingEvents = 0;
	bool m_WasHigh = false;

	void AddSample(int Ping);
	int LocalPing() const;
};

#endif
