#include "AdvisorPanelComponent.h"
#include "UIStyle.h"

using namespace UIStyle;

//==============================================================================
// SuggestionCard

void AdvisorPanelComponent::SuggestionCard::setItem(const FeedbackItem& item)
{
    data = item;
    rebuildLayout(getWidth() > 0 ? getWidth() : 400);
}

void AdvisorPanelComponent::SuggestionCard::rebuildLayout(int width)
{
    if (width <= 0)
        return;

    juce::AttributedString s;
    s.append(data.category.toUpperCase() + "\n",
              juce::Font(small().withStyle("Bold")), kTextDim);
    s.append(data.headline + "\n",
              juce::Font(body().withStyle("Bold")), kTextPrimary);
    if (data.detail.isNotEmpty())
        s.append(data.detail + "\n", juce::Font(body()), kTextDim);
    if (data.fix.isNotEmpty())
        s.append("Try: " + data.fix, juce::Font(body()), kAccent);

    layout.createLayout(s, (float) juce::jmax(40, width - kPadding * 2 - 6));
}

int AdvisorPanelComponent::SuggestionCard::getRequiredHeight(int width)
{
    rebuildLayout(width);
    return (int) std::ceil(layout.getHeight()) + kPadding * 2;
}

void AdvisorPanelComponent::SuggestionCard::resized()
{
    rebuildLayout(getWidth());
}

void AdvisorPanelComponent::SuggestionCard::paint(juce::Graphics& g)
{
    auto stripe = data.severity == Severity::Critical ? kCrit
                : data.severity == Severity::Warning  ? kWarn
                                                        : kGood;

    g.setColour(kPanelAlt);
    g.fillRoundedRectangle(getLocalBounds().toFloat(), 6.0f);
    g.setColour(stripe);
    g.fillRoundedRectangle(0.0f, 0.0f, 4.0f, (float) getHeight(), 2.0f);

    layout.draw(g, juce::Rectangle<float>((float) kPadding + 6.0f, (float) kPadding,
                                           (float) getWidth() - kPadding * 2 - 6.0f,
                                           (float) getHeight() - kPadding * 2));
}

//==============================================================================
// ActionableCard

AdvisorPanelComponent::ActionableCard::ActionableCard()
{
    previewButton.setColour(juce::TextButton::buttonColourId,  kPanel);
    previewButton.setColour(juce::TextButton::textColourOffId, kGood);
    addAndMakeVisible(previewButton);

    rejectButton.setColour(juce::TextButton::buttonColourId,  kPanel);
    rejectButton.setColour(juce::TextButton::textColourOffId, kTextDim);
    addAndMakeVisible(rejectButton);

    toggleButton.setColour(juce::TextButton::buttonColourId,  kPanel);
    toggleButton.setColour(juce::TextButton::textColourOffId, kAccent);
    addChildComponent(toggleButton); // hidden until Previewing

    keepButton.setColour(juce::TextButton::buttonColourId,  kPanel);
    keepButton.setColour(juce::TextButton::textColourOffId, kGood);
    addChildComponent(keepButton);

    undoButton.setColour(juce::TextButton::buttonColourId,  kPanel);
    undoButton.setColour(juce::TextButton::textColourOffId, kTextDim);
    addChildComponent(undoButton);

    previewButton.onClick = [this]
    {
        if (state != State::Idle)
            return;
        if (applyChange)
            applyChange();
        hearingAfter = true;
        state = State::Previewing;
        if (onPreviewStarted)
            onPreviewStarted();
        toggleButton.setButtonText("Hear: Before");
        updateButtonVisibility();
    };
    rejectButton.onClick = [this]
    {
        if (onRejected)
            onRejected();
    };
    toggleButton.onClick = [this]
    {
        if (hearingAfter)
        {
            if (revertChange) revertChange();
            hearingAfter = false;
            toggleButton.setButtonText("Hear: After");
        }
        else
        {
            if (applyChange) applyChange();
            hearingAfter = true;
            toggleButton.setButtonText("Hear: Before");
        }
    };
    keepButton.onClick = [this]
    {
        if (!hearingAfter)
        {
            if (applyChange) applyChange();
            hearingAfter = true;
        }
        // Deliberately NOT calling onPreviewEnded() here - that would
        // restore playback suppression on the Advisor tab immediately,
        // silencing the exact change the user just committed to. Only
        // Undo (a real revert, back to just listening) should do that.
        enterAppliedState();
    };
    undoButton.onClick = [this]
    {
        if (hearingAfter && revertChange)
            revertChange();
        if (onPreviewEnded)
            onPreviewEnded();
        if (onRejected) // same dismissal path as Reject - owner removes the card
            onRejected();
    };
}

void AdvisorPanelComponent::ActionableCard::setAction(const FeedbackItem& item,
                                                        std::function<void()> apply,
                                                        std::function<void()> revert)
{
    applyChange  = std::move(apply);
    revertChange = std::move(revert);
    setItem(item);
}

void AdvisorPanelComponent::ActionableCard::enterAppliedState()
{
    state = State::Applied;
    previewButton.setEnabled(false);
    previewButton.setButtonText("Applied");
    updateButtonVisibility();
}

void AdvisorPanelComponent::ActionableCard::keepDirectly()
{
    if (state == State::Applied)
        return;
    if (!hearingAfter)
    {
        if (applyChange) applyChange();
        hearingAfter = true;
    }
    // Same reasoning as keepButton.onClick - committing shouldn't silence
    // what was just applied.
    enterAppliedState();
}

void AdvisorPanelComponent::ActionableCard::updateButtonVisibility()
{
    const bool idle       = state == State::Idle;
    const bool previewing = state == State::Previewing;

    previewButton.setVisible(idle || state == State::Applied);
    rejectButton.setVisible(idle);
    toggleButton.setVisible(previewing);
    keepButton.setVisible(previewing);
    undoButton.setVisible(previewing);

    resized(); // button layout depends on state - reposition now that visibility changed
}

void AdvisorPanelComponent::ActionableCard::resized()
{
    SuggestionCard::resized();

    constexpr int kBtnH = 22, kMargin = 8;
    constexpr int kBtnW = 64, kToggleW = 96;

    if (state == State::Previewing)
    {
        undoButton.setBounds  (getWidth() - kBtnW - kMargin - kPadding,                        kPadding, kBtnW, kBtnH);
        keepButton.setBounds  (getWidth() - kBtnW * 2 - kMargin * 2 - kPadding,                 kPadding, kBtnW, kBtnH);
        toggleButton.setBounds(getWidth() - kBtnW * 2 - kToggleW - kMargin * 3 - kPadding,       kPadding, kToggleW, kBtnH);
    }
    else
    {
        previewButton.setBounds(getWidth() - kBtnW * 2 - kMargin * 2 - kPadding, kPadding, kBtnW, kBtnH);
        rejectButton.setBounds (getWidth() - kBtnW - kMargin - kPadding,          kPadding, kBtnW, kBtnH);
    }
}

//==============================================================================
// AdvisorPanelComponent

AdvisorPanelComponent::AdvisorPanelComponent()
{
    titleLabel.setText("Advisor - melodic techno listening companion", juce::dontSendNotification);
    titleLabel.setFont(title());
    titleLabel.setColour(juce::Label::textColourId, kTextPrimary);
    addAndMakeVisible(titleLabel);

    static const char* kSectionNames[] = { "Intro", "Build", "Drop", "Breakdown", "Outro" };
    for (int i = 0; i < MelodicTechnoTheory::kNumSongSections; ++i)
        sectionPicker.addItem(kSectionNames[i], i + 1);
    sectionPicker.setSelectedId(3, juce::dontSendNotification); // Drop - matches the prior full-mix assumption
    sectionPicker.setColour(juce::ComboBox::backgroundColourId, kPanel);
    sectionPicker.setColour(juce::ComboBox::textColourId,       kTextPrimary);
    sectionPicker.setColour(juce::ComboBox::outlineColourId,    kBorder);
    addAndMakeVisible(sectionPicker);

    analyzeButton.setColour(juce::TextButton::buttonColourId,  kPanel);
    analyzeButton.setColour(juce::TextButton::textColourOffId, kAccent);
    analyzeButton.onClick = [this] { if (onAnalyzeClicked) onAnalyzeClicked(); };
    addAndMakeVisible(analyzeButton);

    suggestEditsButton.setColour(juce::TextButton::buttonColourId,  kPanel);
    suggestEditsButton.setColour(juce::TextButton::textColourOffId, kAccent);
    suggestEditsButton.onClick = [this] { if (onSuggestMelodyEditsClicked) onSuggestMelodyEditsClicked(); };
    addAndMakeVisible(suggestEditsButton);

    arrangementButton.setColour(juce::TextButton::buttonColourId,  kPanel);
    arrangementButton.setColour(juce::TextButton::textColourOffId, kAccent);
    arrangementButton.onClick = [this] { if (onArrangementClicked) onArrangementClicked(); };
    addAndMakeVisible(arrangementButton);

    listeningStatusLabel.setFont(body().withStyle("Bold"));
    listeningStatusLabel.setColour(juce::Label::textColourId, kTextDim);
    listeningStatusLabel.setText("Not listening yet - press Play in Ableton", juce::dontSendNotification);
    addAndMakeVisible(listeningStatusLabel);

    keyBpmLabel.setFont(body());
    keyBpmLabel.setColour(juce::Label::textColourId, kTextPrimary);
    addAndMakeVisible(keyBpmLabel);

    statusLabel.setText("Play something in Ableton for a few seconds, then click Analyze Now - "
                         "or click Suggest Melody Edits any time to check the piano roll.",
                         juce::dontSendNotification);
    statusLabel.setFont(body());
    statusLabel.setColour(juce::Label::textColourId, kTextDim);
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(statusLabel);

    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);

    melodyEditsLabel.setText("Melody edit suggestions", juce::dontSendNotification);
    melodyEditsLabel.setFont(body().withStyle("Bold"));
    melodyEditsLabel.setColour(juce::Label::textColourId, kTextPrimary);
    melodyEditsLabel.setVisible(false);
    content.addAndMakeVisible(melodyEditsLabel);

    approveAllButton.setColour(juce::TextButton::buttonColourId,  kPanel);
    approveAllButton.setColour(juce::TextButton::textColourOffId, kGood);
    approveAllButton.setVisible(false);
    approveAllButton.onClick = [this]
    {
        // Copy first — each approve can synchronously trigger removal, which
        // would otherwise mutate melodyEditCards while we're iterating it.
        juce::Array<ActionableCard*> toApprove;
        for (auto* c : melodyEditCards)
            toApprove.add(c);
        for (auto* c : toApprove)
            c->keepDirectly(); // applies and locks in directly, skipping the manual preview/compare step
    };
    content.addAndMakeVisible(approveAllButton);

    analysisTweaksLabel.setText("Tweaks from Analysis", juce::dontSendNotification);
    analysisTweaksLabel.setFont(body().withStyle("Bold"));
    analysisTweaksLabel.setColour(juce::Label::textColourId, kTextPrimary);
    analysisTweaksLabel.setVisible(false);
    content.addAndMakeVisible(analysisTweaksLabel);

    analysisApproveAllButton.setColour(juce::TextButton::buttonColourId,  kPanel);
    analysisApproveAllButton.setColour(juce::TextButton::textColourOffId, kGood);
    analysisApproveAllButton.setVisible(false);
    analysisApproveAllButton.onClick = [this]
    {
        juce::Array<ActionableCard*> toApprove;
        for (auto* c : analysisTweakCards)
            toApprove.add(c);
        for (auto* c : toApprove)
            c->keepDirectly();
    };
    content.addAndMakeVisible(analysisApproveAllButton);

    arrangementLabel.setText("Arrangement", juce::dontSendNotification);
    arrangementLabel.setFont(body().withStyle("Bold"));
    arrangementLabel.setColour(juce::Label::textColourId, kTextPrimary);
    arrangementLabel.setVisible(false);
    content.addAndMakeVisible(arrangementLabel);

    arrangementApproveAllButton.setColour(juce::TextButton::buttonColourId,  kPanel);
    arrangementApproveAllButton.setColour(juce::TextButton::textColourOffId, kGood);
    arrangementApproveAllButton.setVisible(false);
    arrangementApproveAllButton.onClick = [this]
    {
        juce::Array<ActionableCard*> toApprove;
        for (auto* c : arrangementCards)
            toApprove.add(c);
        for (auto* c : toApprove)
            c->keepDirectly();
    };
    content.addAndMakeVisible(arrangementApproveAllButton);
}

MelodicTechnoTheory::SongSection AdvisorPanelComponent::getSelectedSection() const
{
    const int id = sectionPicker.getSelectedId(); // 1-based, matches addItem above
    const int idx = juce::jlimit(0, MelodicTechnoTheory::kNumSongSections - 1, id - 1);
    return (MelodicTechnoTheory::SongSection) idx;
}

void AdvisorPanelComponent::setBusy(bool busy)
{
    analyzeButton.setEnabled(!busy);
    analyzeButton.setButtonText(busy ? "Analyzing..." : "Analyze Now");
}

void AdvisorPanelComponent::setListeningStatus(bool isPlaying, int bufferedSecs)
{
    if (isPlaying)
    {
        listeningStatusLabel.setText(juce::String(juce::CharPointer_UTF8("\xe2\x97\x8f")) + " Listening... "
                                          + juce::String(bufferedSecs) + "s",
                                      juce::dontSendNotification);
        listeningStatusLabel.setColour(juce::Label::textColourId, kAccent);
    }
    else if (bufferedSecs > 0)
    {
        listeningStatusLabel.setText("Ready - " + juce::String(bufferedSecs) + "s captured, click Analyze Now",
                                      juce::dontSendNotification);
        listeningStatusLabel.setColour(juce::Label::textColourId, kGood);
    }
    else
    {
        listeningStatusLabel.setText("Not listening yet - press Play in Ableton", juce::dontSendNotification);
        listeningStatusLabel.setColour(juce::Label::textColourId, kTextDim);
    }
}

void AdvisorPanelComponent::setStatus(const juce::String& text)
{
    statusLabel.setText(text, juce::dontSendNotification);
    statusLabel.setVisible(true);
}

void AdvisorPanelComponent::setResult(const juce::String& keyBpmText, const std::vector<FeedbackItem>& items)
{
    keyBpmLabel.setText(keyBpmText, juce::dontSendNotification);
    statusLabel.setVisible(items.empty());
    if (items.empty())
        statusLabel.setText("No suggestions this time - try analyzing again.", juce::dontSendNotification);

    cards.clear();
    for (auto& item : items)
    {
        auto* card = cards.add(new SuggestionCard());
        card->setItem(item);
        content.addAndMakeVisible(card);
    }
    layoutContent();
}

void AdvisorPanelComponent::setMelodyEditSuggestions(const std::vector<ActionableFeedback>& newCards)
{
    melodyEditCards.clear();

    melodyEditsLabel.setVisible(!newCards.empty());
    approveAllButton.setVisible(!newCards.empty());

    if (newCards.empty())
        setStatus("No melody edit suggestions - the piano roll already checks out against the current key.");

    for (auto& mc : newCards)
    {
        auto* card = melodyEditCards.add(new ActionableCard());
        card->setAction(mc.item, mc.applyChange, mc.revertChange);
        card->onPreviewStarted = [this] { if (onPreviewStarted) onPreviewStarted(); };
        card->onPreviewEnded   = [this] { if (onPreviewEnded) onPreviewEnded(); };
        // Deferred: this runs from the card's own Reject button click, so
        // the card (which owns that button) must not be deleted until the
        // click handler has fully returned.
        card->onRejected = [this, card] { juce::MessageManager::callAsync([this, card] { removeMelodyEditCard(card); }); };
        content.addAndMakeVisible(card);
    }
    layoutContent();
}

void AdvisorPanelComponent::removeMelodyEditCard(ActionableCard* card)
{
    melodyEditCards.removeObject(card);
    melodyEditsLabel.setVisible(!melodyEditCards.isEmpty());
    approveAllButton.setVisible(!melodyEditCards.isEmpty());
    layoutContent();
}

void AdvisorPanelComponent::setAnalysisTweaks(const std::vector<ActionableFeedback>& newCards)
{
    analysisTweakCards.clear();

    analysisTweaksLabel.setVisible(!newCards.empty());
    analysisApproveAllButton.setVisible(!newCards.empty());

    for (auto& mc : newCards)
    {
        auto* card = analysisTweakCards.add(new ActionableCard());
        card->setAction(mc.item, mc.applyChange, mc.revertChange);
        card->onPreviewStarted = [this] { if (onPreviewStarted) onPreviewStarted(); };
        card->onPreviewEnded   = [this] { if (onPreviewEnded) onPreviewEnded(); };
        card->onRejected = [this, card] { juce::MessageManager::callAsync([this, card] { removeAnalysisTweakCard(card); }); };
        content.addAndMakeVisible(card);
    }
    layoutContent();
}

void AdvisorPanelComponent::removeAnalysisTweakCard(ActionableCard* card)
{
    analysisTweakCards.removeObject(card);
    analysisTweaksLabel.setVisible(!analysisTweakCards.isEmpty());
    analysisApproveAllButton.setVisible(!analysisTweakCards.isEmpty());
    layoutContent();
}

void AdvisorPanelComponent::setArrangementSuggestions(const std::vector<ActionableFeedback>& newCards)
{
    arrangementCards.clear();

    arrangementLabel.setVisible(!newCards.empty());
    arrangementApproveAllButton.setVisible(!newCards.empty());

    for (auto& mc : newCards)
    {
        auto* card = arrangementCards.add(new ActionableCard());
        card->setAction(mc.item, mc.applyChange, mc.revertChange);
        card->onPreviewStarted = [this] { if (onPreviewStarted) onPreviewStarted(); };
        card->onPreviewEnded   = [this] { if (onPreviewEnded) onPreviewEnded(); };
        card->onRejected = [this, card] { juce::MessageManager::callAsync([this, card] { removeArrangementCard(card); }); };
        content.addAndMakeVisible(card);
    }
    layoutContent();
}

void AdvisorPanelComponent::removeArrangementCard(ActionableCard* card)
{
    arrangementCards.removeObject(card);
    arrangementLabel.setVisible(!arrangementCards.isEmpty());
    arrangementApproveAllButton.setVisible(!arrangementCards.isEmpty());
    layoutContent();
}

void AdvisorPanelComponent::layoutContent()
{
    const int width = juce::jmax(200, viewport.getWidth() - viewport.getScrollBarThickness() - 4);
    int y = 0;
    constexpr int kGap = 8;

    for (auto* card : cards)
    {
        const int h = card->getRequiredHeight(width);
        card->setBounds(0, y, width, h);
        y += h + kGap;
    }

    if (!melodyEditCards.isEmpty())
    {
        y += 6;
        auto row = juce::Rectangle<int>(0, y, width, 24);
        melodyEditsLabel.setBounds(row.removeFromLeft(width - 110));
        approveAllButton.setBounds(row.removeFromRight(100));
        y += 24 + 6;

        for (auto* card : melodyEditCards)
        {
            const int h = juce::jmax(card->getRequiredHeight(width), 56);
            card->setBounds(0, y, width, h);
            y += h + kGap;
        }
    }

    if (!analysisTweakCards.isEmpty())
    {
        y += 6;
        auto row = juce::Rectangle<int>(0, y, width, 24);
        analysisTweaksLabel.setBounds(row.removeFromLeft(width - 110));
        analysisApproveAllButton.setBounds(row.removeFromRight(100));
        y += 24 + 6;

        for (auto* card : analysisTweakCards)
        {
            const int h = juce::jmax(card->getRequiredHeight(width), 56);
            card->setBounds(0, y, width, h);
            y += h + kGap;
        }
    }

    if (!arrangementCards.isEmpty())
    {
        y += 6;
        auto row = juce::Rectangle<int>(0, y, width, 24);
        arrangementLabel.setBounds(row.removeFromLeft(width - 110));
        arrangementApproveAllButton.setBounds(row.removeFromRight(100));
        y += 24 + 6;

        for (auto* card : arrangementCards)
        {
            const int h = juce::jmax(card->getRequiredHeight(width), 56);
            card->setBounds(0, y, width, h);
            y += h + kGap;
        }
    }

    content.setSize(width, y);
}

void AdvisorPanelComponent::resized()
{
    auto area = getLocalBounds().reduced(16);

    auto top = area.removeFromTop(30);
    analyzeButton.setBounds(top.removeFromRight(110));
    top.removeFromRight(8);
    sectionPicker.setBounds(top.removeFromRight(110));
    titleLabel.setBounds(top);

    area.removeFromTop(6);
    auto instantRow = area.removeFromTop(26);
    arrangementButton.setBounds(instantRow.removeFromRight(150));
    instantRow.removeFromRight(8);
    suggestEditsButton.setBounds(instantRow.removeFromRight(150));

    area.removeFromTop(6);
    listeningStatusLabel.setBounds(area.removeFromTop(20));
    keyBpmLabel.setBounds(area.removeFromTop(22));
    statusLabel.setBounds(area.removeFromTop(22));
    area.removeFromTop(8);

    viewport.setBounds(area);
    layoutContent();
}

void AdvisorPanelComponent::paint(juce::Graphics& g)
{
    g.fillAll(kBg);
}
