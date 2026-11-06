#include "Synth.h"

namespace pg
{
// =============================================================================
// StepData
// =============================================================================
void StepData::reset()
{
    for (int l = 0; l < kLanes; ++l)
        for (int s = 0; s < kSteps; ++s)
        {
            // gentle default shapes so each lane does something when routed
            float v = 0.5f;
            if (l == 0) v = (float) (s % 16) / 15.0f;
            if (l == 1) v = (s % 4 == 0) ? 1.0f : 0.25f;
            if (l == 2) v = 0.5f + 0.5f * std::sin ((float) s * kTwoPi / 16.0f);
            if (l == 3) v = (float) ((s * 7) % 11) / 10.0f;
            seq[(size_t) (l * kSteps + s)].store (v);
            seqLock[(size_t) (l * kSteps + s)].store (false);
        }
    for (int s = 0; s < kSteps; ++s)
    {
        arpOn[(size_t) s].store (true);
        arpTie[(size_t) s].store (false);
        arpTranspose[(size_t) s].store (0);
        arpRatchet[(size_t) s].store (1);
        arpVel[(size_t) s].store (0.8f);
    }
}

juce::ValueTree StepData::toTree() const
{
    juce::ValueTree t ("Steps");
    for (int l = 0; l < kLanes; ++l)
    {
        juce::StringArray v, k;
        for (int s = 0; s < kSteps; ++s)
        {
            v.add (juce::String (seq[(size_t) (l * kSteps + s)].load(), 4));
            k.add (seqLock[(size_t) (l * kSteps + s)].load() ? "1" : "0");
        }
        juce::ValueTree lane ("Lane");
        lane.setProperty ("index", l, nullptr);
        lane.setProperty ("values", v.joinIntoString (","), nullptr);
        lane.setProperty ("locks", k.joinIntoString (""), nullptr);
        t.appendChild (lane, nullptr);
    }
    juce::StringArray a;
    for (int s = 0; s < kSteps; ++s)
        a.add (juce::String (arpOn[(size_t) s].load() ? 1 : 0) + ":" + juce::String (arpTranspose[(size_t) s].load()) + ":"
               + juce::String (arpVel[(size_t) s].load(), 3) + ":" + juce::String (arpTie[(size_t) s].load() ? 1 : 0) + ":"
               + juce::String (arpRatchet[(size_t) s].load()));
    juce::ValueTree arp ("Arp");
    arp.setProperty ("steps", a.joinIntoString (","), nullptr);
    t.appendChild (arp, nullptr);
    return t;
}

void StepData::fromTree (const juce::ValueTree& t)
{
    if (! t.isValid()) return;
    for (auto lane : t)
    {
        if (lane.hasType ("Lane"))
        {
            const int l = juce::jlimit (0, kLanes - 1, (int) lane.getProperty ("index"));
            auto vals = juce::StringArray::fromTokens (lane.getProperty ("values").toString(), ",", "");
            auto locks = lane.getProperty ("locks").toString();
            for (int s = 0; s < kSteps && s < vals.size(); ++s)
                seq[(size_t) (l * kSteps + s)].store (juce::jlimit (0.0f, 1.0f, vals[s].getFloatValue()));
            for (int s = 0; s < kSteps && s < locks.length(); ++s)
                seqLock[(size_t) (l * kSteps + s)].store (locks[s] == '1');
        }
        else if (lane.hasType ("Arp"))
        {
            auto steps = juce::StringArray::fromTokens (lane.getProperty ("steps").toString(), ",", "");
            for (int s = 0; s < kSteps && s < steps.size(); ++s)
            {
                auto f = juce::StringArray::fromTokens (steps[s], ":", "");
                if (f.size() < 5) continue;
                arpOn[(size_t) s].store (f[0].getIntValue() != 0);
                arpTranspose[(size_t) s].store (juce::jlimit (-24, 24, f[1].getIntValue()));
                arpVel[(size_t) s].store (juce::jlimit (0.0f, 1.0f, f[2].getFloatValue()));
                arpTie[(size_t) s].store (f[3].getIntValue() != 0);
                arpRatchet[(size_t) s].store (juce::jlimit (1, 4, f[4].getIntValue()));
            }
        }
    }
}

// =============================================================================
// Synth
// =============================================================================
void Synth::prepare (double sampleRate, int)
{
    sr = sampleRate;
    for (int i = 0; i < kMaxVoices; ++i)
        voices[(size_t) i].prepare (sr, i);
    for (int i = 0; i < 4; ++i) gLfo[i].rng.seed (0x1000u + (uint32_t) i);
    monoStack.clear();
    arpHeld.clear(); arpLatched.clear(); arpOffs.clear(); arpSounding.clear();
    internalBeat = 0.0;
    monoStack.reserve (128); arpHeld.reserve (128); arpLatched.reserve (128);
    arpOffs.reserve (256); arpSounding.reserve (128);
}

void Synth::allNotesOff()
{
    for (auto& v : voices) v.kill();
    monoStack.clear();
    arpHeld.clear(); arpLatched.clear(); arpOffs.clear(); arpSounding.clear();
}

void Synth::noteOn (int note, float vel)
{
    lastNote.store (note);
    noteOnCounter.fetch_add (1);
    const int mode = (int) p[voice_mode];
    if (mode == 0)
    {
        const int poly = juce::jlimit (1, kMaxVoices, (int) p[polyphony]);
        Voice* target = nullptr;
        // same note retriggers its own voice
        for (int i = 0; i < poly; ++i)
            if (voices[(size_t) i].isActive() && voices[(size_t) i].getNote() == note) { target = &voices[(size_t) i]; break; }
        if (target == nullptr)
            for (int i = 0; i < poly; ++i)
                if (! voices[(size_t) i].isActive()) { target = &voices[(size_t) i]; break; }
        if (target == nullptr)
        {
            // steal: oldest released voice, else oldest voice
            uint64_t best = UINT64_MAX;
            for (int pass = 0; pass < 2 && target == nullptr; ++pass)
                for (int i = 0; i < poly; ++i)
                {
                    auto& v = voices[(size_t) i];
                    if (pass == 0 && v.isHeld()) continue;
                    if (v.getAge() < best) { best = v.getAge(); target = &v; }
                }
        }
        // voices above the polyphony limit fade out
        for (int i = poly; i < kMaxVoices; ++i) voices[(size_t) i].release();
        target->start (note, vel, true, ctx);
    }
    else
    {
        monoStack.erase (std::remove (monoStack.begin(), monoStack.end(), note), monoStack.end());
        monoStack.push_back (note);
        auto& v = voices[0];
        const bool legato = mode == 2 && v.isActive() && v.isHeld();
        if (legato) v.moveTo (note, ctx);
        else v.start (note, vel, true, ctx);
    }
}

void Synth::noteOff (int note)
{
    const int mode = (int) p[voice_mode];
    if (mode == 0)
    {
        for (auto& v : voices)
            if (v.isActive() && v.isHeld() && v.getNote() == note) v.release();
        return;
    }
    monoStack.erase (std::remove (monoStack.begin(), monoStack.end(), note), monoStack.end());
    auto& v = voices[0];
    if (v.getNote() != note) return;
    if (monoStack.empty()) v.release();
    else if (mode == 2) v.moveTo (monoStack.back(), ctx);
    else v.start (monoStack.back(), 0.8f, true, ctx);
}

void Synth::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn()) noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff()) noteOff (m.getNoteNumber());
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        for (auto& v : voices) if (v.isActive()) v.release();
        monoStack.clear();
    }
}

void Synth::updateGlobalMods (int numSamples, double beat, double beatsPerSample)
{
    const float secs = (float) numSamples / (float) sr;
    const bool playing = wasPlaying;

    for (int i = 0; i < 4; ++i)
    {
        const int base = lfo1_shape + i * (lfo2_shape - lfo1_shape);
        const int shape = (int) p[base];
        const bool sync = p[base + (lfo1_sync - lfo1_shape)] > 0.5f;
        const double div = choices::divisionInBeats ((int) p[base + (lfo1_div - lfo1_shape)]);
        if (sync && playing)
        {
            const double ph = beat / div + (double) p[base + (lfo1_phase - lfo1_shape)];
            const float newPhase = (float) (ph - std::floor (ph));
            float inc = newPhase - gLfo[i].phase;
            if (inc < 0.0f) inc += 1.0f;
            global.lfo[i] = gLfo[i].advance (shape, inc);
        }
        else
        {
            const float hz = sync ? (float) (global.bpm / 60.0 / div) : p[base + (lfo1_rate - lfo1_shape)];
            global.lfo[i] = gLfo[i].advance (shape, hz * secs);
        }
        global.lfoPhase[i] = gLfo[i].phase;
    }

    // chaos: two Lorenz attractors
    for (int c = 0; c < 2; ++c)
    {
        const float rate = c == 0 ? p[chaos1_rate] : p[chaos2_rate];
        const float smooth = c == 0 ? p[chaos1_smooth] : p[chaos2_smooth];
        float* s = lorenz[c];
        const int sub = 4;
        const float dt = juce::jmin (0.02f, rate * secs * 0.25f / (float) sub);
        for (int k = 0; k < sub; ++k)
        {
            const float dx = 10.0f * (s[1] - s[0]);
            const float dy = s[0] * (28.0f - s[2]) - s[1];
            const float dz = s[0] * s[1] - 2.6667f * s[2];
            s[0] += dx * dt; s[1] += dy * dt; s[2] += dz * dt;
        }
        const float target = clampf (s[0] / 20.0f, -1.0f, 1.0f);
        const float a = std::exp (-secs / (0.001f + smooth * smooth * 2.0f));
        chaosSmooth[c] = target + (chaosSmooth[c] - target) * a;
        global.chaos[c] = chaosSmooth[c];
    }

    global.macro[0] = p[macro1]; global.macro[1] = p[macro2];
    global.macro[2] = p[macro3]; global.macro[3] = p[macro4];

    // sequencer lanes
    for (int l = 0; l < 4; ++l)
    {
        const int base = seq1_on + l * (seq2_on - seq1_on);
        if (p[base] < 0.5f) { global.seq[l] = 0.0f; steps.seqPlayStep[(size_t) l].store (-1); continue; }
        const int len = juce::jlimit (1, 32, (int) p[base + (seq1_len - seq1_on)]);
        const double div = choices::divisionInBeats ((int) p[base + (seq1_div - seq1_on)]);
        const double swing = p[base + (seq1_swing - seq1_on)];
        const double pos = beat / div;
        long long k = (long long) std::floor (pos);
        if ((k & 1) && (pos - (double) k) < swing) --k;   // delayed odd steps
        const int step = (int) (((k % len) + len) % len);
        steps.seqPlayStep[(size_t) l].store (step);
        float v = steps.seq[(size_t) (l * StepData::kSteps + step)].load (std::memory_order_relaxed);
        if ((int) p[base + (seq1_mode - seq1_on)] == 1) v = v * 2.0f - 1.0f;
        const float glideAmt = p[base + (seq1_glide - seq1_on)];
        const float stepSecs = (float) (div * 60.0 / global.bpm);
        const float a = glideAmt <= 0.0f ? 0.0f : std::exp (-secs / (glideAmt * stepSecs * 0.5f + 1.0e-4f));
        seqSmooth[l] = v + (seqSmooth[l] - v) * a;
        global.seq[l] = seqSmooth[l];
    }
    juce::ignoreUnused (beatsPerSample);
}

void Synth::runArp (int numSamples, double beatStart, double bps, std::vector<std::pair<int, juce::MidiMessage>>& events)
{
    const bool on = p[arp_on] > 0.5f;
    const double beatEnd = beatStart + bps * numSamples;
    auto offsetOf = [&] (double b) { return juce::jlimit (0, numSamples - 1, (int) ((b - beatStart) / bps)); };

    if (! on)
    {
        if (arpWasOn)
        {
            for (int n : arpSounding) events.push_back ({ 0, juce::MidiMessage::noteOff (1, n) });
            arpSounding.clear(); arpOffs.clear(); arpLatched.clear(); arpRatchetOns.clear();
            steps.arpPlayStep.store (-1);
        }
        arpWasOn = false;
        return;
    }
    if (! arpWasOn) { arpNextBeat = beatStart; arpCounter = 0; arpStep = 0; arpNoteIndex = 0; }
    arpWasOn = true;

    const double div = choices::divisionInBeats ((int) p[arp_div]);
    const bool latch = p[arp_latch] > 0.5f;
    const auto& src = latch ? arpLatched : arpHeld;

    // re-sync with the host grid after a jump in the timeline
    if (wasPlaying && (arpNextBeat < beatStart - div || arpNextBeat > beatStart + 2.0 * div))
    {
        const double g = std::ceil (beatStart / div - 1.0e-9);
        arpStep = (int) ((long long) g % 2);
        arpNextBeat = g * div;
    }

    // emit pending note-offs and ratchet note-ons that fall in this block
    for (size_t i = 0; i < arpOffs.size();)
    {
        if (arpOffs[i].beat < beatEnd)
        {
            const int n = arpOffs[i].note;
            if (n >= 0)
            {
                events.push_back ({ offsetOf (arpOffs[i].beat), juce::MidiMessage::noteOff (1, n) });
                arpSounding.erase (std::remove (arpSounding.begin(), arpSounding.end(), n), arpSounding.end());
            }
            arpOffs.erase (arpOffs.begin() + (long) i);
        }
        else ++i;
    }

    if (src.empty())
    {
        if (! wasPlaying) arpNextBeat = beatEnd;   // the next press starts immediately
        arpCounter = 0;
        arpNoteIndex = 0;
        arpRatchetOns.clear();
        steps.arpPlayStep.store (-1);
        return;
    }

    const double swing = p[arp_swing];
    const int len = juce::jlimit (1, 32, (int) p[arp_len]);
    const int octs = juce::jlimit (1, 4, (int) p[arp_oct]);
    const int mode = (int) p[arp_mode];
    const float gate = p[arp_gate];

    while (true)
    {
        const double fire = arpNextBeat + ((arpStep & 1) ? swing * div : 0.0);
        if (fire >= beatEnd) break;

        const int s = arpCounter % len;
        steps.arpPlayStep.store (s);
        const bool stepOn = steps.arpOn[(size_t) s].load();
        const int transpose = steps.arpTranspose[(size_t) s].load();
        const float vel = steps.arpVel[(size_t) s].load();
        const bool tie = steps.arpTie[(size_t) s].load();
        const int ratchet = juce::jlimit (1, 4, steps.arpRatchet[(size_t) s].load());

        if (stepOn)
        {
            // build the note pool
            std::vector<int> base (src.begin(), src.end());
            if (mode != 4) { std::sort (base.begin(), base.end()); base.erase (std::unique (base.begin(), base.end()), base.end()); }
            std::vector<int> pool;
            for (int o = 0; o < octs; ++o)
                for (int n : base) pool.push_back (n + 12 * o);

            std::vector<int> chosen;
            const int count = (int) pool.size();
            if (mode == 6) chosen = pool;
            else if (count > 0)
            {
                int idx = 0;
                switch (mode)
                {
                    case 0: case 4: idx = arpCounterNote (count); break;
                    case 1: idx = count - 1 - arpCounterNote (count); break;
                    case 2: case 3:
                    {
                        const int period = juce::jmax (1, 2 * count - 2);
                        int k = arpCounterNote (period);
                        idx = k < count ? k : period - k;
                        if (mode == 3) idx = count - 1 - idx;
                        break;
                    }
                    default: idx = (int) (rng.uni() * (float) count); break;
                }
                chosen.push_back (pool[(size_t) juce::jlimit (0, count - 1, idx)]);
            }
            ++arpNoteIndex;

            const double sub = div / ratchet;
            for (int r = 0; r < ratchet; ++r)
            {
                const double t = fire + sub * r;
                const double offBeat = (tie && r == ratchet - 1) ? t + sub * 1.02 : t + sub * gate;
                for (int n : chosen)
                {
                    const int note = juce::jlimit (0, 127, n + transpose);
                    if (t < beatEnd)
                    {
                        // a new note-on of a sounding pitch: close it first
                        if (std::find (arpSounding.begin(), arpSounding.end(), note) != arpSounding.end())
                            events.push_back ({ offsetOf (t), juce::MidiMessage::noteOff (1, note) });
                        events.push_back ({ offsetOf (t), juce::MidiMessage::noteOn (1, note, juce::jlimit (0.05f, 1.0f, vel)) });
                        arpSounding.push_back (note);
                    }
                    else
                        arpRatchetOns.push_back ({ t, note, vel });
                    // remove any older pending off for this pitch, then schedule the new one
                    arpOffs.erase (std::remove_if (arpOffs.begin(), arpOffs.end(), [&] (const PendingOff& o) { return o.note == note && o.beat > t; }), arpOffs.end());
                    arpOffs.push_back ({ offBeat, note });
                }
            }
        }
        ++arpCounter;
        ++arpStep;
        arpNextBeat += div;
    }

    // ratchet notes that were scheduled past the previous block
    for (size_t i = 0; i < arpRatchetOns.size();)
    {
        auto& rr = arpRatchetOns[i];
        if (rr.beat < beatEnd)
        {
            events.push_back ({ offsetOf (rr.beat), juce::MidiMessage::noteOn (1, rr.note, juce::jlimit (0.05f, 1.0f, rr.vel)) });
            arpSounding.push_back (rr.note);
            arpRatchetOns.erase (arpRatchetOns.begin() + (long) i);
        }
        else ++i;
    }
    // offs that became due inside this block (short gates)
    for (size_t i = 0; i < arpOffs.size();)
    {
        if (arpOffs[i].beat < beatEnd)
        {
            events.push_back ({ offsetOf (arpOffs[i].beat), juce::MidiMessage::noteOff (1, arpOffs[i].note) });
            arpSounding.erase (std::remove (arpSounding.begin(), arpSounding.end(), arpOffs[i].note), arpSounding.end());
            arpOffs.erase (arpOffs.begin() + (long) i);
        }
        else ++i;
    }
}

void Synth::render (juce::AudioBuffer<float>& out, juce::MidiBuffer& midi, const float* params,
                    const SampleData* sample, const Wavetable* tableA, const Wavetable* tableB,
                    const juce::AudioPlayHead::PositionInfo* pos)
{
    p = params;
    ctx.p = params;
    ctx.sample = sample;
    ctx.table[0] = tableA;
    ctx.table[1] = tableB;
    ctx.global = &global;

    const int n = out.getNumSamples();
    // ---- clock ----
    bool playing = false;
    double ppq = 0.0;
    if (pos != nullptr)
    {
        if (auto b = pos->getBpm()) global.bpm = juce::jlimit (20.0, 400.0, *b);
        playing = pos->getIsPlaying();
        if (auto q = pos->getPpqPosition()) ppq = *q; else playing = false;
    }
    wasPlaying = playing;
    const double bps = global.bpm / 60.0 / sr;
    const double beatStart = playing ? ppq : internalBeat;
    internalBeat += bps * n;

    // ---- events: host MIDI (+ arpeggiator) ----
    std::vector<std::pair<int, juce::MidiMessage>> events;
    events.reserve (64);
    const bool arpOn = p[arp_on] > 0.5f;
    for (const auto meta : midi)
    {
        auto m = meta.getMessage();
        if (m.isPitchWheel()) { global.pitchBend = (float) (m.getPitchWheelValue() - 8192) / 8192.0f; continue; }
        if (m.isControllerOfType (1)) { global.modWheel = (float) m.getControllerValue() / 127.0f; continue; }
        if (m.isChannelPressure()) { global.aftertouch = (float) m.getChannelPressureValue() / 127.0f; continue; }
        if (m.isAftertouch()) { global.aftertouch = (float) m.getAfterTouchValue() / 127.0f; continue; }
        if (arpOn && (m.isNoteOn() || m.isNoteOff()))
        {
            const int note = m.getNoteNumber();
            if (m.isNoteOn())
            {
                if (arpHeld.empty())
                {
                    arpLatched.clear();                         // a fresh chord replaces the latched one
                    if (! playing) { arpNextBeat = beatStart + bps * meta.samplePosition; arpCounter = 0; arpStep = 0; }
                }
                arpHeld.push_back (note);
                if (std::find (arpLatched.begin(), arpLatched.end(), note) == arpLatched.end()) arpLatched.push_back (note);
            }
            else
                arpHeld.erase (std::remove (arpHeld.begin(), arpHeld.end(), note), arpHeld.end());
            continue;
        }
        events.push_back ({ meta.samplePosition, m });
    }
    if (! arpOn && ! arpHeld.empty()) { arpHeld.clear(); }
    runArp (n, beatStart, bps, events);
    std::stable_sort (events.begin(), events.end(), [] (const auto& a, const auto& b) { return a.first < b.first; });

    // ---- render in short chunks so global modulators stay smooth ----
    float* L = out.getWritePointer (0);
    float* R = out.getWritePointer (out.getNumChannels() > 1 ? 1 : 0);
    size_t ev = 0;
    int i = 0;
    while (i < n)
    {
        while (ev < events.size() && events[ev].first <= i) handleMidi (events[ev++].second);
        int end = juce::jmin (n, i + 32);
        if (ev < events.size()) end = juce::jmin (end, juce::jmax (i + 1, events[ev].first));
        updateGlobalMods (end - i, beatStart + bps * i, bps);

        // the newest voice feeds the grain display
        Voice* newest = nullptr;
        for (auto& v : voices)
            if (v.isActive() && (newest == nullptr || v.getAge() > newest->getAge())) newest = &v;
        int active = 0;
        for (auto& v : voices)
        {
            if (! v.isActive()) continue;
            ++active;
            ctx.viz = (&v == newest) ? &viz : nullptr;
            v.render (L + i, R + i, end - i, ctx);
        }
        activeVoices.store (active, std::memory_order_relaxed);
        if (active == 0) for (auto& sl : viz.pos) sl.store (-1.0f, std::memory_order_relaxed);
        i = end;
    }
    while (ev < events.size()) handleMidi (events[ev++].second);
}
} // namespace pg
