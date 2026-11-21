#include "PluginEditor.h"

using juce::String;

namespace
{
    String u8 (const char* s) { return String::fromUTF8 (s); }

    String dbText (float v, int decimals = 1)
    {
        if (v <= -99.0f) return u8 ("\xe2\x80\x93");
        return (v > 0.04f ? "+" : "") + String (v, decimals);
    }

    String hzText (float f)
    {
        return f >= 1000.0f ? String (f / 1000.0f, f >= 10000.0f ? 1 : 2) + " kHz" : String (juce::roundToInt (f)) + " Hz";
    }

    void fillScreen (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setGradientFill (juce::ColourGradient (Pal::screenTop, 0, r.getY(), Pal::screenBottom, 0, r.getBottom(), false));
        g.fillRoundedRectangle (r, 8.0f);
    }

    void drawFreqGrid (juce::Graphics& g, juce::Rectangle<float> a, std::function<float (float)> freqToX, bool labels = true)
    {
        g.setFont (juce::FontOptions (10.0f));
        for (float f : { 30.0f, 50.0f, 100.0f, 200.0f, 300.0f, 500.0f, 1000.0f, 2000.0f, 3000.0f, 5000.0f, 10000.0f, 15000.0f })
        {
            const float x = freqToX (f);
            const int fi = (int) f;
            const bool major = fi == 100 || fi == 1000 || fi == 10000;
            g.setColour (juce::Colours::white.withAlpha (major ? 0.12f : 0.05f));
            g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
            if (labels)
            {
                g.setColour (Pal::screenText.withAlpha (0.7f));
                g.drawText (f >= 1000.0f ? String ((int) (f / 1000)) + "k" : String ((int) f), (int) x - 20, (int) a.getBottom() + 2, 40, 12, juce::Justification::centred);
            }
        }
    }

    const juce::Colour modeColour[3] = { juce::Colours::white, Pal::screenLine, Pal::toneLine };
}

//==============================================================================
void Page::paint (juce::Graphics& g)
{
    for (auto& gr : groups)
    {
        auto r = gr.area.toFloat();
        g.setColour (Pal::panel);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (Pal::border);
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
        g.setColour (Pal::dim);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (gr.title, gr.area.getX() + 14, gr.area.getY() + 8, gr.area.getWidth() - 28, 14, juce::Justification::centredLeft);
    }
    g.setFont (juce::FontOptions (12.0f));
    for (auto& n : notes)
    {
        g.setColour (Pal::dim);
        g.drawFittedText (n.second, n.first, juce::Justification::topLeft, 8, 1.0f);
    }
}

//==============================================================================
EqCurveView::EqCurveView (AuricMaster2Processor& p, bool inter) : proc (p), interactive (inter)
{
    setOpaque (true);
    spectrum.fill (-120.0f);
    if (interactive)
    {
        for (int i = 0; i < 4; ++i)
        {
            const auto n = "eq" + String (i + 1);
            auto make = [this] (const String& id, float& target)
            {
                auto a = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter (id), [this, &target] (float v) { target = v; repaint(); }, nullptr);
                a->sendInitialUpdate();
                return a;
            };
            nodes[(size_t) i].fAtt = make (n + "_freq", nodes[(size_t) i].freq);
            nodes[(size_t) i].gAtt = make (n + "_gain", nodes[(size_t) i].gain);
            nodes[(size_t) i].qAtt = make (n + "_q", nodes[(size_t) i].q);
        }
    }
}

juce::Rectangle<float> EqCurveView::plot() const
{
    return getLocalBounds().toFloat().reduced (1.0f).withTrimmedBottom (16.0f).withTrimmedLeft (interactive ? 30.0f : 8.0f).withTrimmedRight (8.0f).withTrimmedTop (8.0f);
}

float EqCurveView::freqToX (float f) const
{
    const auto a = plot();
    return a.getX() + a.getWidth() * std::log (juce::jlimit (20.0f, 20000.0f, f) / 20.0f) / std::log (1000.0f);
}

float EqCurveView::xToFreq (float x) const
{
    const auto a = plot();
    return juce::jlimit (20.0f, 20000.0f, 20.0f * std::pow (1000.0f, (x - a.getX()) / a.getWidth()));
}

float EqCurveView::gainToY (float gdb) const
{
    const auto a = plot();
    const float range = interactive ? 12.0f : 9.0f;
    return juce::jmap (juce::jlimit (-range * 1.1f, range * 1.1f, gdb), -range, range, a.getBottom() - 4.0f, a.getY() + 4.0f);
}

float EqCurveView::yToGain (float y) const
{
    const auto a = plot();
    return juce::jlimit (-12.0f, 12.0f, juce::jmap (y, a.getBottom() - 4.0f, a.getY() + 4.0f, -12.0f, 12.0f));
}

juce::Point<float> EqCurveView::nodePos (int i) const
{
    return { freqToX (nodes[(size_t) i].freq), gainToY (nodes[(size_t) i].gain) };
}

int EqCurveView::hitNode (juce::Point<float> pt) const
{
    if (! interactive) return -1;
    int best = -1;
    float bestD = 14.0f;
    for (int i = 0; i < 4; ++i)
    {
        const float d = nodePos (i).getDistanceFrom (pt);
        if (d < bestD) { bestD = d; best = i; }
    }
    return best;
}

void EqCurveView::refresh()
{
    proc.resonance.copyDisplay (spectrum, reduction);

    // the same filters as the processor (gains scaled by AMOUNT), evaluated at a display rate
    const double fs = 48000.0;
    const float amt = param ("amount") * 0.01f;
    Biquad lc1, lc2;
    const float lcF = param ("lowcut");
    const bool lcOn = lcF > 20.5f;
    if (lcOn) { lc1.setHighPass (fs, lcF, 0.5412); lc2.setHighPass (fs, lcF, 1.3066); }
    std::array<Biquad, 4> bm, bs;
    sideDiffers = false;
    for (int b = 0; b < 4; ++b)
    {
        const auto n = "eq" + String (b + 1);
        const float f = param ((n + "_freq").toRawUTF8()), g = param ((n + "_gain").toRawUTF8()) * amt, q = param ((n + "_q").toRawUTF8());
        const int mode = (int) param ((n + "_mode").toRawUTF8());
        const float gm = mode == 2 ? 0.0f : g, gsd = mode == 1 ? 0.0f : g;
        if (mode != 0 && std::abs (g) > 0.05f) sideDiffers = true;
        if (b == 0)      { bm[0].setLowShelf (fs, f, q, gm); bs[0].setLowShelf (fs, f, q, gsd); }
        else if (b == 3) { bm[3].setHighShelf (fs, f, q, gm); bs[3].setHighShelf (fs, f, q, gsd); }
        else             { bm[(size_t) b].setPeak (fs, f, q, gm); bs[(size_t) b].setPeak (fs, f, q, gsd); }
    }
    Biquad t1, t2, t3;
    t1.setLowShelf (fs, 100.0, 0.7, param ("tone_low") * amt);
    t2.setPeak (fs, 1200.0, 0.7, param ("tone_mid") * amt);
    t3.setHighShelf (fs, 10000.0, 0.7, param ("tone_high") * amt);

    for (int i = 0; i < points; ++i)
    {
        const double f = 20.0 * std::pow (1000.0, (double) i / (points - 1));
        double common = t1.magnitudeDb (fs, f) + t2.magnitudeDb (fs, f) + t3.magnitudeDb (fs, f);
        if (lcOn) common += lc1.magnitudeDb (fs, f) + lc2.magnitudeDb (fs, f);
        double m = common, s = common;
        for (int b = 0; b < 4; ++b) { m += bm[(size_t) b].magnitudeDb (fs, f); s += bs[(size_t) b].magnitudeDb (fs, f); }
        curveMid[(size_t) i] = (float) m;
        curveSide[(size_t) i] = (float) s;
    }
    repaint();
}

void EqCurveView::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto full = getLocalBounds().toFloat();
    const auto a = plot();
    g.fillAll (Pal::bg);
    fillScreen (g, full);
    drawFreqGrid (g, a, [this] (float f) { return freqToX (f); });

    // dB grid
    g.setFont (FontOptions (10.0f));
    const float range = interactive ? 12.0f : 9.0f;
    for (float d = -range; d <= range + 0.1f; d += (interactive ? 6.0f : 3.0f))
    {
        const float y = gainToY (d);
        g.setColour (Colours::white.withAlpha (std::abs (d) < 0.1f ? 0.18f : 0.05f));
        g.drawHorizontalLine ((int) y, a.getX(), a.getRight());
        if (interactive)
        {
            g.setColour (Pal::screenText.withAlpha (0.6f));
            g.drawText ((d > 0 ? "+" : "") + String ((int) d), (int) full.getX() + 2, (int) y - 6, 24, 12, Justification::centredRight);
        }
    }

    // spectrum backdrop (tilted, as in the resonance view)
    constexpr int N = ResonanceSuppressor::numDisplayPoints;
    auto specY = [&] (float db) { return jmap (jlimit (-84.0f, 6.0f, db), -84.0f, 6.0f, a.getBottom(), a.getY() + a.getHeight() * 0.15f); };
    Path spec;
    spec.startNewSubPath (a.getX(), a.getBottom());
    for (int i = 0; i < N; ++i)
        spec.lineTo (a.getX() + a.getWidth() * (float) i / (N - 1), specY (spectrum[(size_t) i]));
    spec.lineTo (a.getRight(), a.getBottom());
    spec.closeSubPath();
    g.setColour (Pal::screenLine.withAlpha (0.10f));
    g.fillPath (spec);
    g.setColour (Pal::screenLine.withAlpha (0.25f));
    g.strokePath (spec, PathStrokeType (1.0f));

    // EQ curves
    auto curvePath = [&] (const std::array<float, points>& c)
    {
        Path p;
        for (int i = 0; i < points; ++i)
        {
            const float x = a.getX() + a.getWidth() * (float) i / (points - 1);
            if (i == 0) p.startNewSubPath (x, gainToY (c[(size_t) i]));
            else p.lineTo (x, gainToY (c[(size_t) i]));
        }
        return p;
    };
    auto mid = curvePath (curveMid);
    Path fillP (mid);
    fillP.lineTo (a.getRight(), gainToY (0.0f));
    fillP.lineTo (a.getX(), gainToY (0.0f));
    fillP.closeSubPath();
    g.setColour (Pal::accent.withAlpha (0.28f));
    g.fillPath (fillP);
    if (sideDiffers)
    {
        g.setColour (Pal::toneLine.withAlpha (0.9f));
        g.strokePath (curvePath (curveSide), PathStrokeType (1.6f));
    }
    g.setColour (Colours::white.withAlpha (0.92f));
    g.strokePath (mid, PathStrokeType (2.2f));

    g.setFont (FontOptions (10.5f, Font::bold));
    g.setColour (Pal::screenText);
    g.drawText (interactive ? u8 ("EQ \xc2\xb7 MITTE") : u8 ("KLANGKURVE (EQ + KLANG, inkl. AMOUNT)"), (int) a.getX() + 8, (int) a.getY() + 4, 300, 14, Justification::centredLeft);
    if (sideDiffers)
    {
        g.setColour (Pal::toneLine);
        g.drawText ("SEITE", (int) a.getX() + (interactive ? 86 : 268), (int) a.getY() + 4, 60, 14, Justification::centredLeft);
    }

    if (! interactive) return;

    const char* bandNames[4] = { "LOW SHELF", "GLOCKE", "GLOCKE", "HIGH SHELF" };
    for (int i = 0; i < 4; ++i)
    {
        const auto pos = nodePos (i);
        const int mode = (int) param (("eq" + String (i + 1) + "_mode").toRawUTF8());
        const bool hot = i == hovered || i == dragging;
        const float r = hot ? 9.0f : 7.5f;
        g.setColour (modeColour[mode].withAlpha (std::abs (nodes[(size_t) i].gain) > 0.05f ? 1.0f : 0.55f));
        g.fillEllipse (pos.x - r, pos.y - r, 2 * r, 2 * r);
        g.setColour (Pal::screenBottom);
        g.setFont (FontOptions (10.5f, Font::bold));
        g.drawText (String (i + 1), Rectangle<float> (pos.x - r, pos.y - r, 2 * r, 2 * r), Justification::centred);
        if (hot)
        {
            const String modeName[3] = { "Stereo", "Mitte", "Seite" };
            const String t = String (bandNames[i]) + "  " + hzText (nodes[(size_t) i].freq) + "  " + dbText (nodes[(size_t) i].gain) + " dB  Q "
                             + String (nodes[(size_t) i].q, 2) + "  " + modeName[mode];
            const float tw = 250.0f;
            const float tx = jlimit (a.getX(), a.getRight() - tw, pos.x - tw * 0.5f);
            const float ty = pos.y > a.getCentreY() ? pos.y - 34.0f : pos.y + 14.0f;
            g.setColour (Pal::screenBottom.withAlpha (0.85f));
            g.fillRoundedRectangle (tx, ty, tw, 20.0f, 5.0f);
            g.setColour (Colours::white);
            g.drawText (t, Rectangle<float> (tx, ty, tw, 20.0f), Justification::centred);
        }
    }
}

void EqCurveView::mouseMove (const juce::MouseEvent& e)
{
    const int h = hitNode (e.position);
    if (h != hovered) { hovered = h; repaint(); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
}

void EqCurveView::mouseExit (const juce::MouseEvent&) { hovered = -1; repaint(); }

void EqCurveView::mouseDown (const juce::MouseEvent& e)
{
    dragging = hitNode (e.position);
    if (dragging < 0) return;
    nodes[(size_t) dragging].fAtt->beginGesture();
    nodes[(size_t) dragging].gAtt->beginGesture();
}

void EqCurveView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0) return;
    auto& n = nodes[(size_t) dragging];
    n.fAtt->setValueAsPartOfGesture (xToFreq (e.position.x));
    n.gAtt->setValueAsPartOfGesture (yToGain (e.position.y));
}

void EqCurveView::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0)
    {
        nodes[(size_t) dragging].fAtt->endGesture();
        nodes[(size_t) dragging].gAtt->endGesture();
    }
    dragging = -1;
    repaint();
}

void EqCurveView::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int h = hitNode (e.position);
    if (h >= 0) nodes[(size_t) h].gAtt->setValueAsCompleteGesture (0.0f);
}

void EqCurveView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int h = hitNode (e.position) >= 0 ? hitNode (e.position) : hovered;
    if (h < 0) return;
    auto& n = nodes[(size_t) h];
    n.qAtt->setValueAsCompleteGesture (juce::jlimit (0.3f, 6.0f, n.q * (w.deltaY > 0 ? 1.12f : 1.0f / 1.12f)));
}

//==============================================================================
DynamicsView::DynamicsView (AuricMaster2Processor& p) : proc (p)
{
    setOpaque (true);
    spectrum.fill (-120.0f);
    lowAtt = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter ("xover_low"), [this] (float v) { xLow = v; repaint(); }, nullptr);
    highAtt = std::make_unique<juce::ParameterAttachment> (*proc.apvts.getParameter ("xover_high"), [this] (float v) { xHigh = v; repaint(); }, nullptr);
    lowAtt->sendInitialUpdate();
    highAtt->sendInitialUpdate();
}

juce::Rectangle<float> DynamicsView::plot() const
{
    return getLocalBounds().toFloat().reduced (1.0f).withTrimmedBottom (34.0f).withTrimmedTop (28.0f).reduced (8.0f, 0.0f);
}

float DynamicsView::freqToX (float f) const
{
    const auto a = plot();
    return a.getX() + a.getWidth() * std::log (juce::jlimit (20.0f, 20000.0f, f) / 20.0f) / std::log (1000.0f);
}

float DynamicsView::xToFreq (float x) const
{
    const auto a = plot();
    return 20.0f * std::pow (1000.0f, juce::jlimit (0.0f, 1.0f, (x - a.getX()) / a.getWidth()));
}

int DynamicsView::hitLine (float x) const
{
    if (std::abs (x - freqToX (xLow)) < 8.0f) return 0;
    if (std::abs (x - freqToX (xHigh)) < 8.0f) return 1;
    return -1;
}

void DynamicsView::refresh()
{
    proc.resonance.copyDisplay (spectrum, reduction);
    const float now[4] = { proc.grLow.load(), proc.grMid.load(), proc.grHigh.load(), proc.grGlue.load() };
    for (int i = 0; i < 4; ++i)
    {
        const float v = juce::jlimit (0.0f, 24.0f, -now[i]);
        gr[(size_t) i] = v > gr[(size_t) i] ? v : gr[(size_t) i] * 0.85f + v * 0.15f;
        grHold[(size_t) i] = juce::jmax (gr[(size_t) i], grHold[(size_t) i] - 0.05f);
    }
    repaint();
}

void DynamicsView::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto full = getLocalBounds().toFloat();
    const auto a = plot();
    g.fillAll (Pal::bg);
    fillScreen (g, full);
    drawFreqGrid (g, a, [this] (float f) { return freqToX (f); });

    // spectrum backdrop
    constexpr int N = ResonanceSuppressor::numDisplayPoints;
    Path spec;
    spec.startNewSubPath (a.getX(), a.getBottom());
    for (int i = 0; i < N; ++i)
        spec.lineTo (a.getX() + a.getWidth() * (float) i / (N - 1),
                     jmap (jlimit (-84.0f, 6.0f, spectrum[(size_t) i]), -84.0f, 6.0f, a.getBottom(), a.getY() + a.getHeight() * 0.3f));
    spec.lineTo (a.getRight(), a.getBottom());
    spec.closeSubPath();
    g.setColour (Pal::screenLine.withAlpha (0.08f));
    g.fillPath (spec);

    const float xs[4] = { a.getX(), freqToX (xLow), freqToX (xHigh), a.getRight() };
    const String names[3] = { "TIEFEN", "MITTEN", u8 ("H\xc3\x96HEN") };
    const String ranges[3] = { "bis " + hzText (xLow), hzText (xLow) + u8 (" \xe2\x80\x93 ") + hzText (xHigh), "ab " + hzText (xHigh) };
    for (int b = 0; b < 3; ++b)
    {
        const auto band = Rectangle<float> (xs[b], a.getY(), xs[b + 1] - xs[b], a.getHeight()).reduced (3.0f, 0.0f);
        const float depth = jmin (1.0f, gr[(size_t) b] / 6.0f), hold = jmin (1.0f, grHold[(size_t) b] / 6.0f);
        g.setColour (Pal::accent.withAlpha (0.55f));
        g.fillRoundedRectangle (band.withHeight (jmax (2.0f, band.getHeight() * depth)), 4.0f);
        g.setColour (Colours::white.withAlpha (0.7f));
        g.fillRect (band.getX(), band.getY() + band.getHeight() * hold, band.getWidth(), 1.5f);
        g.setColour (Colours::white);
        g.setFont (FontOptions (12.0f, Font::bold));
        g.drawText (names[b] + "  " + String (-gr[(size_t) b], 1) + " dB", (int) full.getX() + (int) xs[b] + 4, (int) full.getY() + 6, (int) (xs[b + 1] - xs[b]) - 8, 16, Justification::centred);
        g.setColour (Pal::screenText.withAlpha (0.75f));
        g.setFont (FontOptions (10.5f));
        g.drawText (ranges[b], (int) xs[b] + 4, (int) a.getBottom() - 18, (int) (xs[b + 1] - xs[b]) - 8, 14, Justification::centred);
    }

    // crossover handles
    for (int i = 0; i < 2; ++i)
    {
        const float x = xs[i + 1];
        const bool hot = hovered == i || dragging == i;
        g.setColour (hot ? Pal::toneLine : Colours::white.withAlpha (0.6f));
        g.drawLine (x, a.getY(), x, a.getBottom(), hot ? 2.0f : 1.2f);
        g.fillEllipse (x - 6.0f, a.getCentreY() - 6.0f, 12.0f, 12.0f);
    }

    // band scale and glue bar
    g.setColour (Pal::screenText.withAlpha (0.55f));
    g.setFont (FontOptions (9.5f));
    g.drawText ("0 dB", (int) a.getRight() - 40, (int) a.getY() + 1, 38, 11, Justification::centredRight);
    g.drawText ("-6 dB", (int) a.getRight() - 40, (int) a.getBottom() - 12, 38, 11, Justification::centredRight);
    const float gd = jmin (1.0f, gr[3] / 6.0f);
    g.setColour (Pal::toneLine);
    g.setFont (FontOptions (10.5f, Font::bold));
    g.drawText ("GLUE " + String (-gr[3], 1) + " dB", (int) a.getX(), (int) full.getBottom() - 17, 100, 12, Justification::centredLeft);
    auto glueBar = Rectangle<float> (a.getX() + 104.0f, full.getBottom() - 14.0f, a.getWidth() - 104.0f, 6.0f);
    g.setColour (Colours::white.withAlpha (0.08f));
    g.fillRoundedRectangle (glueBar, 3.0f);
    g.setColour (Pal::toneLine.withAlpha (0.85f));
    g.fillRoundedRectangle (glueBar.withWidth (glueBar.getWidth() * gd), 3.0f);
}

void DynamicsView::mouseMove (const juce::MouseEvent& e)
{
    const int h = hitLine (e.position.x);
    if (h != hovered) { hovered = h; repaint(); }
    setMouseCursor (h >= 0 ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
}

void DynamicsView::mouseDown (const juce::MouseEvent& e)
{
    dragging = hitLine (e.position.x);
    if (dragging == 0) lowAtt->beginGesture();
    if (dragging == 1) highAtt->beginGesture();
}

void DynamicsView::mouseDrag (const juce::MouseEvent& e)
{
    const float f = xToFreq (e.position.x);
    if (dragging == 0) lowAtt->setValueAsPartOfGesture (juce::jlimit (60.0f, juce::jmin (400.0f, xHigh * 0.5f), f));
    if (dragging == 1) highAtt->setValueAsPartOfGesture (juce::jlimit (juce::jmax (1000.0f, xLow * 2.0f), 8000.0f, f));
}

void DynamicsView::mouseUp (const juce::MouseEvent&)
{
    if (dragging == 0) lowAtt->endGesture();
    if (dragging == 1) highAtt->endGesture();
    dragging = -1;
    repaint();
}

//==============================================================================
void ScopeView::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto full = getLocalBounds().toFloat();
    fillScreen (g, full);
    auto area = full.reduced (12.0f).withTrimmedBottom (40.0f);
    const float size = jmin (area.getWidth(), area.getHeight());
    const auto sq = Rectangle<float> (size, size).withCentre (area.getCentre());
    const auto c = sq.getCentre();
    const float rad = size * 0.5f;

    g.setColour (Colours::white.withAlpha (0.08f));
    g.drawEllipse (sq.reduced (1.0f), 1.0f);
    g.drawEllipse (sq.reduced (rad * 0.5f), 1.0f);
    g.drawLine (c.x, sq.getY(), c.x, sq.getBottom());
    g.drawLine (sq.getX(), c.y, sq.getRight(), c.y);
    const float d = rad * 0.7071f;
    g.drawLine (c.x - d, c.y - d, c.x + d, c.y + d);
    g.drawLine (c.x - d, c.y + d, c.x + d, c.y - d);
    g.setColour (Pal::screenText.withAlpha (0.8f));
    g.setFont (FontOptions (11.0f, Font::bold));
    g.drawText ("M", (int) c.x - 10, (int) sq.getY() - 2, 20, 14, Justification::centred);
    g.drawText ("L", (int) (c.x - d) - 16, (int) (c.y - d) - 14, 16, 14, Justification::centred);
    g.drawText ("R", (int) (c.x + d), (int) (c.y - d) - 14, 16, 14, Justification::centred);

    const int newest = proc.scopePos.load (std::memory_order_acquire);
    g.setColour (Pal::screenLine.withAlpha (0.55f));
    for (int k = 0; k < AuricMaster2Processor::scopeSize; ++k)
    {
        const int idx = (newest + k) % AuricMaster2Processor::scopeSize;
        const float l = proc.scope[(size_t) (2 * idx)].load (std::memory_order_relaxed);
        const float r = proc.scope[(size_t) (2 * idx + 1)].load (std::memory_order_relaxed);
        float x = (r - l) * 0.5f, y = (l + r) * 0.5f;   // mono full scale reaches the rim
        const float len = std::sqrt (x * x + y * y);
        if (len > 1.0f) { x /= len; y /= len; }
        g.fillRect (c.x + x * rad - 0.75f, c.y - y * rad - 0.75f, 1.5f, 1.5f);
    }

    // correlation
    auto bar = Rectangle<float> (full.getX() + 16.0f, full.getBottom() - 26.0f, full.getWidth() - 32.0f, 8.0f);
    g.setColour (Colours::white.withAlpha (0.1f));
    g.fillRoundedRectangle (bar, 4.0f);
    const float mx = bar.getX() + bar.getWidth() * (corrS + 1.0f) * 0.5f;
    g.setColour (corrS < 0.0f ? Pal::warn : Pal::good);
    g.fillRoundedRectangle (Rectangle<float> (jmin (mx, bar.getCentreX()), bar.getY(), std::abs (mx - bar.getCentreX()), bar.getHeight()), 4.0f);
    g.setColour (Colours::white);
    g.fillEllipse (mx - 5.0f, bar.getCentreY() - 5.0f, 10.0f, 10.0f);
    g.setColour (Pal::screenText);
    g.setFont (FontOptions (10.5f, Font::bold));
    g.drawText (u8 ("KORRELATION  ") + String (corrS, 2), bar.withY (bar.getY() - 17.0f).withHeight (14.0f), Justification::centred);
    g.setFont (FontOptions (10.0f));
    g.drawText ("-1", bar.withY (bar.getBottom() + 1.0f).withHeight (12.0f), Justification::centredLeft);
    g.drawText ("+1", bar.withY (bar.getBottom() + 1.0f).withHeight (12.0f), Justification::centredRight);
}

//==============================================================================
void LimiterView::refresh()
{
    st[(size_t) pos] = proc.outMeter.getShortTerm();
    lim[(size_t) pos] = proc.grLimitPeak.exchange (0.0f);
    clip[(size_t) pos] = proc.grClipPeak.exchange (0.0f);
    tp[(size_t) pos] = proc.outMeter.getTruePeakDb();
    pos = (pos + 1) % len;
    if (isShowing()) repaint();
}

void LimiterView::paint (juce::Graphics& g)
{
    using namespace juce;
    const auto full = getLocalBounds().toFloat();
    fillScreen (g, full);
    auto a = full.reduced (12.0f).withTrimmedLeft (34.0f).withTrimmedTop (18.0f);
    auto top = a.removeFromTop (a.getHeight() * 0.62f);
    a.removeFromTop (10.0f);
    auto bottom = a;

    auto ly = [&] (float l) { return jmap (jlimit (-36.0f, -2.0f, l), -36.0f, -2.0f, top.getBottom(), top.getY()); };
    g.setFont (FontOptions (10.0f));
    for (float l = -34.0f; l <= -2.0f; l += 4.0f)
    {
        g.setColour (Colours::white.withAlpha (0.06f));
        g.drawHorizontalLine ((int) ly (l), top.getX(), top.getRight());
        g.setColour (Pal::screenText.withAlpha (0.6f));
        g.drawText (String ((int) l), (int) full.getX() + 6, (int) ly (l) - 6, 34, 12, Justification::centredRight);
    }

    const int t = (int) proc.apvts.getRawParameterValue ("target")->load();
    if (t > 0)
    {
        const float y = ly (Targets::lufs (t));
        Path dash;
        dash.startNewSubPath (top.getX(), y);
        dash.lineTo (top.getRight(), y);
        const float pattern[2] = { 6.0f, 4.0f };
        PathStrokeType (1.4f).createDashedStroke (dash, dash, pattern, 2);
        g.setColour (Pal::toneLine);
        g.fillPath (dash);
        g.setFont (FontOptions (10.5f, Font::bold));
        g.drawText ("ZIEL " + String (Targets::lufs (t), 0) + " LUFS", (int) top.getRight() - 120, (int) y - 16, 118, 14, Justification::centredRight);
    }

    // short-term loudness (oldest left)
    Path line;
    bool started = false;
    for (int k = 0; k < len; ++k)
    {
        const float v = st[(size_t) ((pos + k) % len)];
        const float x = top.getX() + top.getWidth() * (float) k / (len - 1);
        if (v < -60.0f) { started = false; continue; }
        if (! started) { line.startNewSubPath (x, ly (v)); started = true; }
        else line.lineTo (x, ly (v));
    }
    g.setColour (Colours::white.withAlpha (0.92f));
    g.strokePath (line, PathStrokeType (2.0f));

    // gain reduction (limiter and clipper hang from the top of the lower area)
    auto gy = [&] (float db) { return bottom.getY() + bottom.getHeight() * jlimit (0.0f, 1.0f, -db / 12.0f); };
    g.setColour (Colours::white.withAlpha (0.08f));
    g.drawHorizontalLine ((int) bottom.getY(), bottom.getX(), bottom.getRight());
    g.setColour (Pal::screenText.withAlpha (0.6f));
    g.drawText ("0", (int) full.getX() + 6, (int) bottom.getY() - 6, 34, 12, Justification::centredRight);
    g.drawText ("-12", (int) full.getX() + 6, (int) bottom.getBottom() - 6, 34, 12, Justification::centredRight);
    const float colW = bottom.getWidth() / (float) len;
    for (int k = 0; k < len; ++k)
    {
        const int idx = (pos + k) % len;
        const float x = bottom.getX() + colW * (float) k;
        g.setColour (Pal::accent.withAlpha (0.8f));
        g.fillRect (x, bottom.getY(), colW + 0.5f, gy (lim[(size_t) idx]) - bottom.getY());
        g.setColour (Pal::toneLine.withAlpha (0.8f));
        g.fillRect (x, bottom.getY(), colW + 0.5f, (gy (clip[(size_t) idx]) - bottom.getY()) * 0.5f);
    }

    const int last = (pos + len - 1) % len;
    g.setFont (FontOptions (11.0f, Font::bold));
    g.setColour (Colours::white);
    g.drawText (u8 ("KURZZEIT-LAUTHEIT \xc2\xb7 20 s"), (int) top.getX(), (int) full.getY() + 6, 260, 14, Justification::centredLeft);
    g.setColour (Pal::screenLine);
    g.drawText ("LIMITER " + String (lim[(size_t) last], 1) + " dB", (int) bottom.getX() + 6, (int) bottom.getBottom() - 16, 150, 14, Justification::centredLeft);
    g.setColour (Pal::toneLine);
    g.drawText ("CLIPPER " + String (clip[(size_t) last], 1) + " dB", (int) bottom.getX() + 160, (int) bottom.getBottom() - 16, 150, 14, Justification::centredLeft);
}

//==============================================================================
LoudnessPanel::LoudnessPanel (AuricMaster2Processor& p) : proc (p)
{
    histOut.fill (-100.0f);
    histIn.fill (-100.0f);
    resetButton.setColour (juce::TextButton::buttonColourId, Pal::panel);
    resetButton.setColour (juce::TextButton::textColourOffId, Pal::accent);
    resetButton.setTooltip (u8 ("Messung neu starten (integrierte Lautheit, LRA, True-Peak-Maximum)"));
    resetButton.onClick = [this] { proc.inMeter.requestReset(); proc.outMeter.requestReset(); };
    addAndMakeVisible (resetButton);
}

void LoudnessPanel::resized()
{
    resetButton.setBounds (getWidth() - 76, 10, 62, 22);
}

void LoudnessPanel::refresh()
{
    int newest = 0;
    proc.outMeter.copyHistory (histOut, newest);
    int newestIn = 0;
    proc.inMeter.copyHistory (histIn, newestIn);
    // re-order so index 0 is the oldest
    std::array<float, LoudnessMeter::historySize> tmp;
    for (int k = 0; k < LoudnessMeter::historySize; ++k) tmp[(size_t) k] = histOut[(size_t) ((newest + 1 + k) % LoudnessMeter::historySize)];
    histOut = tmp;
    for (int k = 0; k < LoudnessMeter::historySize; ++k) tmp[(size_t) k] = histIn[(size_t) ((newestIn + 1 + k) % LoudnessMeter::historySize)];
    histIn = tmp;

    const float now[6] = { proc.grLow.load(), proc.grMid.load(), proc.grHigh.load(), proc.grGlue.load(), proc.grClip.load(), proc.grLimit.load() };
    for (int i = 0; i < 6; ++i)
    {
        const float v = juce::jlimit (0.0f, 24.0f, -now[i]);
        grS[(size_t) i] = v > grS[(size_t) i] ? v : grS[(size_t) i] * 0.85f + v * 0.15f;
    }
    corrS = corrS * 0.85f + proc.correlation.load() * 0.15f;
    repaint();
}

void LoudnessPanel::paint (juce::Graphics& g)
{
    using namespace juce;
    auto r = getLocalBounds().toFloat();
    g.setColour (Pal::panel);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (Pal::border);
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
    const int W = getWidth();

    g.setColour (Pal::dim);
    g.setFont (FontOptions (11.0f, Font::bold));
    g.drawText (u8 ("AUSGANG \xc2\xb7 LAUTHEIT"), 16, 12, 200, 16, Justification::centredLeft);

    // integrated loudness
    const float integ = proc.outMeter.getIntegrated();
    g.setColour (Pal::deep);
    g.setFont (FontOptions (50.0f, Font::bold));
    g.drawText (integ > -70.0f ? String (integ, 1) : u8 ("\xe2\x80\x93"), 14, 32, 190, 58, Justification::centredRight);
    g.setColour (Pal::dim);
    g.setFont (FontOptions (13.0f, Font::bold));
    g.drawText ("LUFS", 210, 50, 80, 16, Justification::centredLeft);
    g.setFont (FontOptions (10.5f));
    g.drawText ("integriert", 210, 66, 80, 14, Justification::centredLeft);

    const int t = (int) proc.apvts.getRawParameterValue ("target")->load();
    String targetLine;
    Colour dot = Pal::dim;
    if (t > 0)
    {
        const bool ok = proc.targetReached.load();
        dot = ok ? Pal::good : Pal::accent;
        targetLine = "Ziel " + String (Targets::lufs (t), 0) + " LUFS  " + (ok ? u8 ("\xc2\xb7 erreicht") : u8 ("\xc2\xb7 regelt nach \xe2\x80\xa6"));
    }
    else targetLine = u8 ("Kein Ziel \xc2\xb7 Drive manuell");
    g.setColour (dot);
    g.fillEllipse (18.0f, 99.0f, 9.0f, 9.0f);
    g.setColour (Pal::text);
    g.setFont (FontOptions (12.5f, Font::bold));
    g.drawText (targetLine, 34, 95, W - 50, 18, Justification::centredLeft);

    // tiles
    const float ceiling = proc.apvts.getRawParameterValue ("ceiling")->load();
    const float tpMax = proc.outMeter.getTruePeakMaxDb();
    struct Tile { String label, value, unit; Colour colour; };
    const Tile tiles[4] = {
        { "MOMENTAN", dbText (proc.outMeter.getMomentary()), "LUFS", Pal::text },
        { "KURZZEIT", dbText (proc.outMeter.getShortTerm()), "LUFS", Pal::text },
        { "LRA", String (proc.outMeter.getLra(), 1), "LU", Pal::text },
        { "TRUE PEAK MAX", dbText (tpMax), "dBTP", tpMax > ceiling + 0.1f ? Pal::warn : Pal::text } };
    for (int i = 0; i < 4; ++i)
    {
        const auto tr = Rectangle<float> (14.0f + (float) (i % 2) * ((float) W - 28.0f) * 0.5f + (i % 2 ? 4.0f : 0.0f), 122.0f + (float) (i / 2) * 58.0f,
                                          ((float) W - 28.0f) * 0.5f - 4.0f, 52.0f);
        g.setColour (Pal::bg);
        g.fillRoundedRectangle (tr, 8.0f);
        g.setColour (Pal::dim);
        g.setFont (FontOptions (10.0f, Font::bold));
        g.drawText (tiles[i].label, tr.reduced (10.0f, 6.0f).withHeight (12.0f), Justification::centredLeft);
        g.setColour (tiles[i].colour);
        g.setFont (FontOptions (21.0f, Font::bold));
        g.drawText (tiles[i].value, tr.reduced (10.0f, 0.0f).withTrimmedTop (20.0f).withHeight (26.0f), Justification::centredLeft);
        g.setColour (Pal::dim);
        g.setFont (FontOptions (10.5f, Font::bold));
        g.drawText (tiles[i].unit, tr.reduced (10.0f, 0.0f).withTrimmedTop (26.0f).withHeight (20.0f), Justification::centredRight);
    }

    // history
    auto hist = Rectangle<float> (14.0f, 244.0f, (float) W - 28.0f, 118.0f);
    fillScreen (g, hist);
    auto ha = hist.reduced (8.0f, 10.0f).withTrimmedTop (8.0f);
    auto hy = [&] (float l) { return jmap (jlimit (-40.0f, 0.0f, l), -40.0f, 0.0f, ha.getBottom(), ha.getY()); };
    g.setFont (FontOptions (9.5f));
    for (float l = -36.0f; l <= -4.0f; l += 8.0f)
    {
        g.setColour (Colours::white.withAlpha (0.06f));
        g.drawHorizontalLine ((int) hy (l), ha.getX(), ha.getRight());
        g.setColour (Pal::screenText.withAlpha (0.5f));
        g.drawText (String ((int) l), (int) ha.getRight() - 24, (int) hy (l) - 11, 24, 10, Justification::centredRight);
    }
    if (t > 0)
    {
        g.setColour (Pal::toneLine.withAlpha (0.9f));
        const float y = hy (Targets::lufs (t));
        for (float x = ha.getX(); x < ha.getRight(); x += 9.0f) g.drawLine (x, y, jmin (ha.getRight(), x + 5.0f), y, 1.2f);
    }
    auto histPath = [&] (const std::array<float, LoudnessMeter::historySize>& h)
    {
        Path p;
        bool started = false;
        for (int k = 0; k < LoudnessMeter::historySize; ++k)
        {
            const float v = h[(size_t) k];
            const float x = ha.getX() + ha.getWidth() * (float) k / (LoudnessMeter::historySize - 1);
            if (v < -60.0f) { started = false; continue; }
            if (! started) { p.startNewSubPath (x, hy (v)); started = true; }
            else p.lineTo (x, hy (v));
        }
        return p;
    };
    g.setColour (Pal::screenText.withAlpha (0.45f));
    g.strokePath (histPath (histIn), PathStrokeType (1.2f));
    g.setColour (Colours::white);
    g.strokePath (histPath (histOut), PathStrokeType (1.8f));
    g.setColour (Pal::screenText);
    g.setFont (FontOptions (10.0f, Font::bold));
    g.drawText (u8 ("KURZZEIT \xc2\xb7 60 s"), (int) hist.getX() + 8, (int) hist.getY() + 4, 140, 12, Justification::centredLeft);
    g.setColour (Pal::screenText.withAlpha (0.6f));
    g.drawText ("Eingang", (int) hist.getX() + 110, (int) hist.getY() + 4, 60, 12, Justification::centredLeft);

    // input / drive
    const float inI = proc.inMeter.getIntegrated();
    g.setColour (Pal::dim);
    g.setFont (FontOptions (10.0f, Font::bold));
    g.drawText ("EINGANG", 16, 374, 120, 12, Justification::centredLeft);
    g.drawText ("DRIVE", W / 2 + 4, 374, 120, 12, Justification::centredLeft);
    g.setColour (Pal::text);
    g.setFont (FontOptions (16.0f, Font::bold));
    g.drawText ((inI > -70.0f ? String (inI, 1) : u8 ("\xe2\x80\x93")) + " LUFS", 16, 388, 140, 20, Justification::centredLeft);
    g.drawText (dbText (proc.driveDb.load()) + " dB", W / 2 + 4, 388, 140, 20, Justification::centredLeft);

    // correlation
    g.setColour (Pal::dim);
    g.setFont (FontOptions (10.0f, Font::bold));
    g.drawText ("KORRELATION", 16, 422, 140, 12, Justification::centredLeft);
    g.setColour (corrS < 0.0f ? Pal::warn : Pal::text);
    g.drawText (String (corrS, 2), W - 80, 422, 64, 12, Justification::centredRight);
    auto bar = Rectangle<float> (16.0f, 440.0f, (float) W - 32.0f, 8.0f);
    g.setColour (Pal::track);
    g.fillRoundedRectangle (bar, 4.0f);
    const float mx = bar.getX() + bar.getWidth() * (jlimit (-1.0f, 1.0f, corrS) + 1.0f) * 0.5f;
    g.setColour (corrS < 0.0f ? Pal::warn : Pal::good);
    g.fillRoundedRectangle (Rectangle<float> (jmin (mx, bar.getCentreX()), bar.getY(), std::abs (mx - bar.getCentreX()), bar.getHeight()), 4.0f);
    g.setColour (Pal::text);
    g.fillRect (bar.getCentreX() - 0.5f, bar.getY() - 2.0f, 1.0f, bar.getHeight() + 4.0f);

    // gain reduction
    g.setColour (Pal::dim);
    g.drawText ("GAIN-REDUKTION", 16, 466, 160, 12, Justification::centredLeft);
    const String names[6] = { "TIEFEN", "MITTEN", u8 ("H\xc3\x96HEN"), "GLUE", "CLIPPER", "LIMITER" };
    for (int i = 0; i < 6; ++i)
    {
        const float y = 486.0f + (float) i * 22.0f;
        g.setColour (Pal::dim);
        g.setFont (FontOptions (10.0f, Font::bold));
        g.drawText (names[i], 16, (int) y, 64, 14, Justification::centredLeft);
        auto b = Rectangle<float> (82.0f, y + 3.0f, (float) W - 82.0f - 70.0f, 8.0f);
        g.setColour (Pal::track);
        g.fillRoundedRectangle (b, 4.0f);
        g.setColour (i >= 4 ? Pal::deep : Pal::accent);
        g.fillRoundedRectangle (b.withWidth (b.getWidth() * jmin (1.0f, grS[(size_t) i] / 12.0f)), 4.0f);
        g.setColour (Pal::text);
        g.drawText (String (-grS[(size_t) i], 1) + " dB", W - 76, (int) y, 60, 14, Justification::centredRight);
    }
}

//==============================================================================
void TabButton::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    auto r = getLocalBounds().toFloat();
    const bool on = getToggleState();
    g.setColour (on ? Pal::deep : (highlighted ? Pal::text : Pal::dim));
    g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    g.drawText (getButtonText(), r.withTrimmedBottom (4.0f), juce::Justification::centred);
    if (on)
    {
        g.setColour (Pal::accent);
        g.fillRoundedRectangle (r.removeFromBottom (3.0f).reduced (r.getWidth() * 0.18f, 0.0f), 1.5f);
    }
}

void SnapButton::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool on = getToggleState();
    g.setColour (on ? Pal::accent : Pal::panel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on || highlighted ? Pal::accent : Pal::border);
    g.drawRoundedRectangle (r, 6.0f, 1.2f);
    g.setColour (on ? juce::Colours::white : Pal::text);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (getButtonText(), r, juce::Justification::centred);
    if (used && ! on)
    {
        g.setColour (Pal::accent);
        g.fillEllipse (r.getRight() - 8.0f, r.getY() + 4.0f, 4.0f, 4.0f);
    }
}

//==============================================================================
MainView::MainView (AuricMaster2Processor& p)
    : proc (p), loudness (p), simpleCurve (p, false), eqCurve (p, true), dynView (p), scope (p), limView (p), clash (p)
{
    setOpaque (true);

    // header
    for (int i = 0; i < 4; ++i)
    {
        snaps[(size_t) i] = std::make_unique<SnapButton> (String::charToString ((juce::juce_wchar) ('A' + i)));
        snaps[(size_t) i]->setTooltip (u8 ("Snapshot ") + String::charToString ((juce::juce_wchar) ('A' + i))
                                       + u8 (": vier komplette Einstellungen zum schnellen Vergleichen (werden mit dem Projekt gespeichert)"));
        snaps[(size_t) i]->onClick = [this, i] { proc.selectSnapshot (i); };
        addAndMakeVisible (*snaps[(size_t) i]);
    }
    matchAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "compare", matchButton);
    originalAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "bypass", originalButton);
    matchButton.setTooltip (u8 ("Gleicht die Lautheit an: die lautere Version wird leiser gemacht, damit du fair vergleichst (nicht f\xc3\xbcr den Export)"));
    originalButton.setTooltip (u8 ("Original h\xc3\xb6ren (Bypass, latenzausgeglichen)"));
    addAndMakeVisible (matchButton);
    addAndMakeVisible (originalButton);

    setupCombo (genre, *this, "assist_profile", "GENRE", u8 ("Klangziel f\xc3\xbcr den Assistenten"));
    genre.label.setJustificationType (juce::Justification::centredRight);
    assistButton.setColour (juce::TextButton::buttonColourId, Pal::accent);
    assistButton.setColour (juce::TextButton::buttonOnColourId, Pal::deep);
    assistButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    assistButton.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    assistButton.setTooltip (u8 ("H\xc3\xb6rt 15 Sekunden zu und stellt EQ, Dynamik, Breite, Mono-Bass, Farbe, Resonanzen und das Lautheitsziel passend ein"));
    assistButton.onClick = [this] { proc.startAssistant(); };
    addAndMakeVisible (assistButton);

    undoButton.setColour (juce::TextButton::buttonColourId, Pal::panel);
    undoButton.setColour (juce::TextButton::textColourOffId, Pal::accent);
    undoButton.setTooltip (u8 ("Stellt alles auf den Stand vor dem Assistenten zur\xc3\xbc" "ck"));
    undoButton.onClick = [this] { proc.undoAssistant(); };
    addChildComponent (undoButton);

    const String tabNames[numPages] = { "EINFACH", "EQ", "DYNAMIK", "STEREO & FARBE", "LIMITER", "RESONANZ" };
    for (int i = 0; i < numPages; ++i)
    {
        tabs[(size_t) i] = std::make_unique<TabButton> (tabNames[i]);
        tabs[(size_t) i]->onClick = [this, i] { showPage (i); };
        addAndMakeVisible (*tabs[(size_t) i]);
        addChildComponent (pages[(size_t) i]);
    }
    addAndMakeVisible (loudness);

    // EINFACH
    auto& ps = pages[simple];
    ps.addAndMakeVisible (simpleCurve);
    setupKnob (toneLow, ps, "tone_low", "TIEFEN", u8 ("Breiter Bass-Shelf bei 100 Hz"));
    setupKnob (toneMid, ps, "tone_mid", "MITTEN", u8 ("Breite Glocke bei 1,2 kHz: Pr\xc3\xa4senz / W\xc3\xa4rme"));
    setupKnob (toneHigh, ps, "tone_high", u8 ("H\xc3\x96HEN"), u8 ("Breiter H\xc3\xb6hen-Shelf bei 10 kHz: Luft"));
    setupKnob (amount, ps, "amount", "AMOUNT", u8 ("Gesamtintensit\xc3\xa4t aller Klang-, Dynamik-, Breiten- und Farbstufen (100 % = wie eingestellt)"), true);
    setupKnob (punch, ps, "punch", "PUNCH", u8 ("Makro f\xc3\xbcr Multiband-Dynamik und Glue: mehr Druck und Dichte"));
    setupKnob (width, ps, "width", "BREITE", u8 ("Makro f\xc3\xbcr die Stereobreite von Mitten und H\xc3\xb6hen; der Bass bleibt mono"));
    setupCombo (targetS, ps, "target", "LAUTHEITSZIEL", u8 ("Automatik: regelt den Drive, bis die integrierte Lautheit dem Ziel entspricht"));
    setupCombo (limModeS, ps, "lim_mode", "LIMITER", u8 ("Charakter des Limiters: Transparent, Punchy oder Laut"));
    setupKnob (ceilingS, ps, "ceiling", "CEILING", u8 ("Maximaler True Peak am Ausgang (Streaming: -1 dBTP)"));
    setupDrive (driveS, ps);
    setupKnob (input, ps, "input", "INPUT", u8 ("Eingangspegel vor allen Stufen"));
    setupKnob (output, ps, "output", "OUTPUT", u8 ("Ausgangstrim nach dem Limiter (nur leiser, damit das Ceiling sicher bleibt)"));

    // EQ
    auto& pe = pages[eq];
    pe.addAndMakeVisible (eqCurve);
    setupKnob (lowCut, pe, "lowcut", "LOW CUT", u8 ("Hochpass gegen Rumpeln (24 dB/Okt.), ganz links = aus"));
    for (int b = 0; b < 4; ++b)
    {
        const auto n = "eq" + String (b + 1);
        setupCombo (eqMode[(size_t) b], pe, (n + "_mode").toRawUTF8(), "MODUS", u8 ("Stereo = beide Kan\xc3\xa4le, Mitte = nur Mono-Anteil, Seite = nur Stereo-Anteil"));
        setupKnob (eqGain[(size_t) b], pe, (n + "_gain").toRawUTF8(), "GAIN", u8 ("Anhebung / Absenkung"));
        setupKnob (eqFreq[(size_t) b], pe, (n + "_freq").toRawUTF8(), "FREQ", "Frequenz");
        setupKnob (eqQ[(size_t) b], pe, (n + "_q").toRawUTF8(), "Q", u8 ("G\xc3\xbcte: klein = breit, gro\xc3\x9f = schmal"));
    }

    // DYNAMIK
    auto& pd = pages[dyn];
    pd.addAndMakeVisible (dynView);
    setupKnob (mbLow, pd, "mb_low", "TIEFEN", u8 ("Kompression im Tiefenband (Schwelle folgt dem Songpegel)"));
    setupKnob (mbMid, pd, "mb_mid", "MITTEN", u8 ("Kompression im Mittenband"));
    setupKnob (mbHigh, pd, "mb_high", u8 ("H\xc3\x96HEN"), u8 ("Kompression im H\xc3\xb6henband"));
    setupKnob (mbLowGain, pd, "mb_low_gain", "PEGEL", u8 ("Pegel des Tiefenbands"));
    setupKnob (mbMidGain, pd, "mb_mid_gain", "PEGEL", u8 ("Pegel des Mittenbands"));
    setupKnob (mbHighGain, pd, "mb_high_gain", "PEGEL", u8 ("Pegel des H\xc3\xb6henbands"));
    setupKnob (xLow, pd, "xover_low", "CROSSOVER 1", u8 ("Trennfrequenz Tiefen / Mitten (Linkwitz-Riley, phasenrichtig)"));
    setupKnob (xHigh, pd, "xover_high", "CROSSOVER 2", u8 ("Trennfrequenz Mitten / H\xc3\xb6hen"));
    setupKnob (mbSpeed, pd, "mb_speed", "TEMPO", u8 ("Attack/Release der B\xc3\xa4nder: langsam = mehr Transienten, schnell = dichter"));
    setupKnob (glue, pd, "glue", "GLUE", u8 ("Sanfter Bus-Kompressor \xc3\xbc" "ber alles (Sidechain ohne Bass, pumpt nicht)"));
    setupKnob (glueSpeed, pd, "glue_speed", "GLUE-TEMPO", u8 ("Attack/Release des Glue-Kompressors"));
    setupKnob (punchD, pd, "punch", "PUNCH", u8 ("Makro f\xc3\xbcr alle Dynamikstufen"));

    // STEREO & FARBE
    auto& pst = pages[stereo];
    pst.addAndMakeVisible (scope);
    setupKnob (widthD, pst, "width", "BREITE", u8 ("Makro: skaliert Mitten- und H\xc3\xb6henbreite gemeinsam"));
    setupKnob (widthMid, pst, "width_mid", "MITTEN", u8 ("Stereobreite der Mitten"));
    setupKnob (widthHigh, pst, "width_high", u8 ("H\xc3\x96HEN"), u8 ("Stereobreite der H\xc3\xb6hen"));
    setupKnob (monoFreq, pst, "mono_freq", "MONO-BASS", u8 ("Unterhalb dieser Frequenz wird das Signal mono (ganz links = aus)"));
    setupKnob (satLow, pst, "sat_low", u8 ("W\xc3\x84RME"), u8 ("Bands\xc3\xa4ttigung mit Tape-Charakter f\xc3\xbcr Tiefen und Mitten"));
    setupKnob (satHigh, pst, "sat_high", "GLANZ", u8 ("Pegelabh\xc3\xa4ngiger Exciter f\xc3\xbcr die H\xc3\xb6hen"));

    // LIMITER
    auto& pl = pages[limit];
    pl.addAndMakeVisible (limView);
    setupCombo (targetL, pl, "target", "LAUTHEITSZIEL", u8 ("Automatik: regelt den Drive, bis die integrierte Lautheit dem Ziel entspricht"));
    setupDrive (driveL, pl);
    setupKnob (ceilingL, pl, "ceiling", "CEILING", u8 ("Maximaler True Peak am Ausgang (Streaming: -1 dBTP)"));
    setupKnob (clip, pl, "clip", "CLIPPER", u8 ("Weicher Clipper vor dem Limiter (4x Oversampling): nimmt schnelle Spitzen ab"));
    setupKnob (outputL, pl, "output", "OUTPUT", u8 ("Ausgangstrim nach dem Limiter"));
    setupCombo (limModeL, pl, "lim_mode", "CHARAKTER", u8 ("Transparent = langsam und sauber, Punchy = ausgewogen, Laut = schnell und dicht"));

    // RESONANZ
    auto& pr = pages[res];
    clash.setLarge (true);
    pr.addAndMakeVisible (clash);
    resOnAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "res_on", resOn);
    resOn.setTooltip (u8 ("Dynamische Resonanzkontrolle ein/aus"));
    pr.addAndMakeVisible (resOn);
    setupKnob (resDepth, pr, "res_depth", "TIEFE", u8 ("Wie stark st\xc3\xb6rende Resonanzen abgesenkt werden"));
    setupKnob (resSharp, pr, "res_sharp", u8 ("SCH\xc3\x84RFE"), u8 ("Schmal (nur Spitzen) bis breit"));
    setupKnob (resSens, pr, "res_sens", u8 ("SELEKTIVIT\xc3\x84T"), u8 ("Wie deutlich eine Resonanz herausragen muss"));
    setupKnob (resAttack, pr, "res_attack", "ATTACK", u8 ("Reaktionszeit"));
    setupKnob (resRelease, pr, "res_release", "RELEASE", u8 ("R\xc3\xbc" "ckstellzeit"));

    // group frames and notes
    ps.groups = { { { 0, 204, 250, 250 }, "KLANG" }, { { 266, 204, 268, 250 }, u8 ("ST\xc3\x84RKE") },
                  { { 550, 204, 250, 250 }, "CHARAKTER" }, { { 0, 468, 800, 122 }, u8 ("LAUTHEIT & AUSGANG") } };
    ps.notes = { { { 16, 368, 220, 80 }, u8 ("Breite Klangbalance: Shelf 100 Hz, Glocke 1,2 kHz, Shelf 10 kHz. Feinarbeit auf der Seite EQ.") },
                 { { 566, 368, 220, 80 }, u8 ("PUNCH steuert Multiband und Glue, BREITE die Stereobreite. Details unter DYNAMIK und STEREO.") } };
    pe.groups = { { { 0, 314, 112, 276 }, "LOW CUT" } };
    const String bandTitles[4] = { u8 ("BAND 1 \xc2\xb7 LOW SHELF"), u8 ("BAND 2 \xc2\xb7 GLOCKE"), u8 ("BAND 3 \xc2\xb7 GLOCKE"), u8 ("BAND 4 \xc2\xb7 HIGH SHELF") };
    for (int b = 0; b < 4; ++b) pe.groups.push_back ({ { 124 + b * 169, 314, 157, 276 }, bandTitles[b] });
    pe.notes = { { { 12, 470, 92, 110 }, u8 ("Entfernt Rumpeln unterhalb der Frequenz.") } };
    pd.groups = { { { 0, 264, 560, 326 }, "MULTIBAND-KOMPRESSION" }, { { 576, 264, 224, 326 }, "GLUE & PUNCH" } };
    pst.groups = { { { 0, 0, 300, 340 }, "VEKTORSKOP" }, { { 316, 0, 484, 340 }, "STEREOBREITE" }, { { 0, 356, 800, 234 }, u8 ("FARBE \xc2\xb7 S\xc3\x84TTIGUNG") } };
    pst.notes = { { { 336, 228, 446, 100 }, u8 ("BREITE skaliert Mitten und H\xc3\xb6hen gemeinsam. Unterhalb von MONO-BASS wird das Signal mono: sicher f\xc3\xbcr Club, Vinyl und Handy-Lautsprecher. Die Korrelation sollte \xc3\xbc" "ber 0 bleiben.") },
                  { { 340, 392, 440, 180 }, u8 ("W\xc3\x84RME: Bands\xc3\xa4ttigung mit Tape-Charakter f\xc3\xbcr Tiefen und Mitten. Bringt Dichte und Obert\xc3\xb6ne, damit der Bass auch auf kleinen Lautsprechern h\xc3\xb6rbar wird.\n\nGLANZ: pegelabh\xc3\xa4ngiger Exciter f\xc3\xbcr die H\xc3\xb6hen. F\xc3\xbcgt Luft hinzu, ohne Zischlaute zu verst\xc3\xa4rken.") } };
    pl.groups = { { { 0, 304, 392, 286 }, "LAUTHEITSZIEL" }, { { 408, 304, 392, 286 }, "LIMITER" } };
    pl.notes = { { { 16, 462, 360, 120 }, u8 ("Die Automatik misst die Lautheit nach ITU-R BS.1770 und regelt den Drive weich nach, bis das Ziel erreicht ist. Ceiling und True-Peak-Grenze werden dabei immer eingehalten.") },
                 { { 650, 470, 140, 110 }, u8 ("True-Peak-Limiter mit Look-ahead und 4x Oversampling.") } };
    pr.groups = { { { 0, 444, 800, 146 }, "RESONANZ-KONTROLLE" } };

    const int saved = juce::jlimit (0, numPages - 1, proc.uiPage.load());
    showPage (saved);
}

void MainView::setupKnob (Knob& k, juce::Component& parent, const char* id, const String& name, const String& tip, bool big)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (big ? juce::Slider::NoTextBox : juce::Slider::TextBoxBelow, false, 84, 16);
    if (big) k.slider.getProperties().set ("big", true);
    k.slider.setTooltip (tip);
    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    k.label.setInterceptsMouseClicks (false, false);
    parent.addAndMakeVisible (k.slider);
    parent.addAndMakeVisible (k.label);
    k.att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, id, k.slider);
    if (auto* prm = proc.apvts.getParameter (id))
        k.slider.setDoubleClickReturnValue (true, prm->convertFrom0to1 (prm->getDefaultValue()));
}

void MainView::setupCombo (Combo& c, juce::Component& parent, const char* id, const String& name, const String& tip)
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id)))
        c.box.addItemList (choice->choices, 1);
    c.box.setTooltip (tip);
    c.label.setText (name, juce::dontSendNotification);
    c.label.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    c.label.setInterceptsMouseClicks (false, false);
    parent.addAndMakeVisible (c.box);
    parent.addAndMakeVisible (c.label);
    c.att = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, id, c.box);
}

void MainView::setupDrive (DriveDial& d, juce::Component& parent)
{
    setupKnob (d.manual, parent, "drive", "DRIVE", u8 ("Verst\xc3\xa4rkung vor Clipper und Limiter (nur ohne Lautheitsziel)"));
    d.autoDial.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    d.autoDial.setRange (0.0, 24.0, 0.1);
    d.autoDial.setTextBoxStyle (juce::Slider::TextBoxBelow, true, 84, 16);
    d.autoDial.textFromValueFunction = [] (double v) { return dbText ((float) v) + " dB"; };
    d.autoDial.getProperties().set ("auto", true);
    d.autoDial.setInterceptsMouseClicks (false, false);
    d.autoLabel.setText ("DRIVE", juce::dontSendNotification);
    d.autoLabel.setJustificationType (juce::Justification::centred);
    d.autoLabel.setFont (juce::FontOptions (11.5f, juce::Font::bold));
    d.autoLabel.setColour (juce::Label::textColourId, Pal::accent);
    parent.addChildComponent (d.autoDial);
    parent.addChildComponent (d.autoLabel);
}

void MainView::placeKnob (Knob& k, juce::Rectangle<int> r)
{
    k.label.setBounds (r.removeFromTop (16));
    k.slider.setBounds (r);
}

void MainView::placeCombo (Combo& c, juce::Rectangle<int> r)
{
    c.label.setBounds (r.removeFromTop (18));
    c.box.setBounds (r.removeFromTop (28));
}

void MainView::placeDrive (DriveDial& d, juce::Rectangle<int> r)
{
    placeKnob (d.manual, r);
    d.autoLabel.setBounds (r.removeFromTop (16));
    d.autoDial.setBounds (r);
}

void MainView::updateDrive (DriveDial& d)
{
    const bool automatic = proc.apvts.getRawParameterValue ("target")->load() > 0.5f;
    d.manual.slider.setVisible (! automatic);
    d.manual.label.setVisible (! automatic);
    d.autoDial.setVisible (automatic);
    d.autoLabel.setVisible (automatic);
    if (automatic) d.autoDial.setValue (proc.driveDb.load(), juce::dontSendNotification);
}

void MainView::showPage (int index)
{
    currentPage = juce::jlimit (0, numPages - 1, index);
    proc.uiPage.store (currentPage);
    for (int i = 0; i < numPages; ++i)
    {
        pages[(size_t) i].setVisible (i == currentPage);
        tabs[(size_t) i]->setToggleState (i == currentPage, juce::dontSendNotification);
    }
}

void MainView::paint (juce::Graphics& g)
{
    using namespace juce;
    g.fillAll (Pal::bg);
    // header
    g.setColour (Pal::panel);
    g.fillRect (0, 0, designWidth, 60);
    g.setColour (Pal::border);
    g.drawHorizontalLine (60, 0.0f, (float) designWidth);
    g.setColour (Pal::deep);
    g.setFont (FontOptions (24.0f, Font::bold));
    g.drawText ("AURIC", 20, 10, 90, 28, Justification::centredLeft);
    g.setColour (Pal::accent);
    g.drawText ("MASTER 2", 104, 10, 140, 28, Justification::centredLeft);
    g.setColour (Pal::dim);
    g.setFont (FontOptions (10.5f, Font::bold));
    g.drawText ("MASTERING SUITE", 21, 37, 200, 14, Justification::centredLeft);
    g.drawText ("SNAPSHOT", 301, 6, 120, 12, Justification::centredLeft);

    // tab bar line
    g.setColour (Pal::border);
    g.drawHorizontalLine (102, 16.0f, 816.0f);

    // footer
    g.setColour (Pal::text);
    g.setFont (FontOptions (12.0f));
    g.drawText (status, 18, 708, undoButton.isVisible() ? 870 : 1000, 24, Justification::centredLeft);
    g.setColour (Pal::dim);
    g.setFont (FontOptions (10.5f));
    g.drawText (u8 ("v2.0  \xc2\xb7  BS.1770-4 / EBU R128"), 900, 708, 244, 24, Justification::centredRight);
}

void MainView::resized()
{
    for (int i = 0; i < 4; ++i) snaps[(size_t) i]->setBounds (300 + i * 38, 22, 32, 28);
    matchButton.setBounds (470, 16, 120, 30);
    originalButton.setBounds (598, 16, 104, 30);
    genre.label.setBounds (716, 16, 54, 30);
    genre.box.setBounds (776, 16, 150, 30);
    assistButton.setBounds (940, 13, 204, 36);
    for (int i = 0; i < numPages; ++i) tabs[(size_t) i]->setBounds (16 + i * 133, 68, 133, 34);
    for (auto& pg : pages) pg.setBounds (16, 110, 800, 590);
    loudness.setBounds (832, 68, 312, 632);
    undoButton.setBounds (890, 707, 116, 26);

    layoutSimple();
    layoutEq();
    layoutDyn();
    layoutStereo();
    layoutLimit();
    layoutRes();
}

void MainView::layoutSimple()
{
    simpleCurve.setBounds (0, 0, 800, 190);
    placeKnob (toneLow, { 8, 232, 78, 120 });
    placeKnob (toneMid, { 86, 232, 78, 120 });
    placeKnob (toneHigh, { 164, 232, 78, 120 });
    placeKnob (amount, { 290, 226, 220, 222 });
    placeKnob (punch, { 562, 228, 110, 134 });
    placeKnob (width, { 678, 228, 110, 134 });
    placeCombo (targetS, { 16, 500, 210, 46 });
    placeCombo (limModeS, { 240, 500, 140, 46 });
    placeKnob (ceilingS, { 398, 492, 92, 94 });
    placeDrive (driveS, { 496, 492, 92, 94 });
    placeKnob (input, { 600, 492, 92, 94 });
    placeKnob (output, { 698, 492, 92, 94 });
}

void MainView::layoutEq()
{
    eqCurve.setBounds (0, 0, 800, 300);
    placeKnob (lowCut, { 8, 340, 96, 112 });
    for (int b = 0; b < 4; ++b)
    {
        const int x = 124 + b * 169;
        placeCombo (eqMode[(size_t) b], { x + 12, 338, 133, 46 });
        placeKnob (eqGain[(size_t) b], { x + 24, 390, 109, 102 });
        placeKnob (eqFreq[(size_t) b], { x + 4, 494, 76, 92 });
        placeKnob (eqQ[(size_t) b], { x + 78, 494, 76, 92 });
    }
}

void MainView::layoutDyn()
{
    dynView.setBounds (0, 0, 800, 250);
    Knob* amounts[3] = { &mbLow, &mbMid, &mbHigh };
    Knob* gains[3] = { &mbLowGain, &mbMidGain, &mbHighGain };
    Knob* bottom[3] = { &xLow, &xHigh, &mbSpeed };
    for (int c = 0; c < 3; ++c)
    {
        const int x = 10 + c * 182;
        placeKnob (*amounts[c], { x + 36, 290, 110, 118 });
        placeKnob (*gains[c], { x + 50, 410, 82, 88 });
        placeKnob (*bottom[c], { x + 50, 500, 82, 86 });
    }
    placeKnob (glue, { 590, 290, 96, 112 });
    placeKnob (glueSpeed, { 692, 290, 96, 112 });
    placeKnob (punchD, { 630, 420, 116, 136 });
}

void MainView::layoutStereo()
{
    scope.setBounds (10, 28, 280, 302);
    placeKnob (widthD, { 328, 36, 112, 170 });
    placeKnob (widthMid, { 442, 36, 112, 170 });
    placeKnob (widthHigh, { 556, 36, 112, 170 });
    placeKnob (monoFreq, { 670, 36, 112, 170 });
    placeKnob (satLow, { 30, 392, 130, 160 });
    placeKnob (satHigh, { 180, 392, 130, 160 });
}

void MainView::layoutLimit()
{
    limView.setBounds (0, 0, 800, 290);
    placeCombo (targetL, { 16, 340, 220, 46 });
    placeDrive (driveL, { 254, 332, 120, 124 });
    placeKnob (ceilingL, { 424, 332, 112, 124 });
    placeKnob (clip, { 544, 332, 112, 124 });
    placeKnob (outputL, { 664, 332, 112, 124 });
    placeCombo (limModeL, { 424, 476, 200, 46 });
}

void MainView::layoutRes()
{
    clash.setBounds (0, 0, 800, 430);
    resOn.setBounds (20, 500, 92, 32);
    placeKnob (resDepth, { 140, 466, 110, 116 });
    placeKnob (resSharp, { 266, 466, 110, 116 });
    placeKnob (resSens, { 392, 466, 110, 116 });
    placeKnob (resAttack, { 518, 466, 110, 116 });
    placeKnob (resRelease, { 644, 466, 110, 116 });
}

void MainView::tick (int frame)
{
    loudness.refresh();
    limView.refresh();
    switch (currentPage)
    {
        case simple: simpleCurve.refresh(); break;
        case eq:     eqCurve.refresh(); break;
        case dyn:    dynView.refresh(); break;
        case stereo: scope.refresh(); break;
        case res:    clash.refresh(); break;
        default: break;
    }
    updateDrive (driveS);
    updateDrive (driveL);

    for (int i = 0; i < 4; ++i)
    {
        auto& b = *snaps[(size_t) i];
        const bool on = proc.getActiveSnapshot() == i, used = proc.snapshotUsed (i);
        if (b.getToggleState() != on || b.used != used)
        {
            b.setToggleState (on, juce::dontSendNotification);
            b.used = used;
            b.repaint();
        }
    }

    // assistant state and footer
    String newStatus;
    bool showUndo = false;
    if (proc.isAutoLearning())
    {
        const float prog = juce::jlimit (0.0f, 1.0f, proc.getAutoProgress());
        assistButton.setButtonText (u8 ("H\xc3\x96RT ZU \xe2\x80\xa6 ") + String (juce::roundToInt (prog * 100.0f)) + " %");
        assistButton.setToggleState (true, juce::dontSendNotification);
        const int left = juce::roundToInt ((1.0f - prog) * AuricMaster2Processor::assistSeconds);
        newStatus = u8 ("ASSISTENT h\xc3\xb6rt zu: noch ") + String (left) + u8 (" s. Spiele einen typischen, vollen Teil des Songs ab (z. B. den Refrain).");
    }
    else
    {
        assistButton.setButtonText ("ASSISTENT");
        assistButton.setToggleState (false, juce::dontSendNotification);
        const bool recent = proc.getAutoResult() != nullptr && juce::Time::getMillisecondCounter() - proc.getAutoResultTime() < 60000;
        if (recent && proc.getAssistSummary().isNotEmpty())
        {
            newStatus = u8 ("ASSISTENT (") + proc.apvts.getParameter ("assist_profile")->getCurrentValueAsText() + "): " + proc.getAssistSummary();
            showUndo = proc.canUndoAssistant();
        }
        else
        {
            const char* hints[numPages] = {
                "Tipp: Starte mit dem ASSISTENTEN, dann AMOUNT und KLANG nach Geh\xc3\xb6r. GLEICH LAUT vergleicht fair mit dem Original.",
                "Punkte ziehen = Frequenz und Gain  \xc2\xb7  Mausrad = G\xc3\xbcte (Q)  \xc2\xb7  Doppelklick = Band zur\xc3\xbc" "cksetzen",
                "Linien ziehen = Crossover verschieben  \xc2\xb7  Balken = aktuelle Gain-Reduktion je Band, unten der Glue-Kompressor",
                "Vektorskop: senkrecht = mono, breit = stereo. Korrelation unter 0 bedeutet Phasenprobleme.",
                "Oben die Kurzzeit-Lautheit der letzten 20 s, unten die Arbeit von Limiter (blau) und Clipper (gelb).",
                "Knoten ziehen = Frequenz und St\xc3\xa4rke  \xc2\xb7  Mausrad = Breite  \xc2\xb7  Doppelklick = zur\xc3\xbc" "cksetzen" };
            newStatus = u8 (hints[currentPage]);
        }
    }
    if (undoButton.isVisible() != showUndo) { undoButton.setVisible (showUndo); repaint (0, 704, designWidth, 36); }
    if (newStatus != status) { status = newStatus; repaint (0, 704, designWidth, 36); }
    juce::ignoreUnused (frame);
}

//==============================================================================
AuricMaster2Editor::AuricMaster2Editor (AuricMaster2Processor& p)
    : AudioProcessorEditor (&p), view (p)
{
    const int savedWidth = p.uiWidth.load();
    addAndMakeVisible (view);
    setLookAndFeel (&lnf);   // after adding the view, so every child picks up the colours
    view.setBounds (0, 0, MainView::designWidth, MainView::designHeight);
    setResizable (true, true);
    setResizeLimits (MainView::designWidth * 6 / 10, MainView::designHeight * 6 / 10,
                     MainView::designWidth * 16 / 10, MainView::designHeight * 16 / 10);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) MainView::designWidth / (double) MainView::designHeight);
    const int w = savedWidth > 0 ? juce::jlimit (MainView::designWidth * 6 / 10, MainView::designWidth * 16 / 10, savedWidth)
                                 : MainView::designWidth * 9 / 10;
    setSize (w, w * MainView::designHeight / MainView::designWidth);
    sizeRestored = true;
    startTimerHz (30);
}

AuricMaster2Editor::~AuricMaster2Editor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void AuricMaster2Editor::resized()
{
    const float scale = (float) getWidth() / (float) MainView::designWidth;
    view.setTransform (juce::AffineTransform::scale (scale));
    if (sizeRestored)
        if (auto* p = dynamic_cast<AuricMaster2Processor*> (getAudioProcessor()))
            p->uiWidth.store (getWidth());
}

void AuricMaster2Editor::timerCallback()
{
    view.tick (frame++);
}
