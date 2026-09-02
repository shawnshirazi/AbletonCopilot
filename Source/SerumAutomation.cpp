#include "SerumAutomation.h"
#include <ApplicationServices/ApplicationServices.h>
#include <unistd.h>

namespace SerumAutomation
{
    bool isAutomationAvailable()
    {
        return AXIsProcessTrusted();
    }

    bool requestAutomationPermission()
    {
        if (AXIsProcessTrusted())
            return true;

        const void* keys[]   = { kAXTrustedCheckOptionPrompt };
        const void* values[] = { kCFBooleanTrue };
        CFDictionaryRef options = CFDictionaryCreate(nullptr, keys, values, 1,
                                                       &kCFTypeDictionaryKeyCallBacks,
                                                       &kCFTypeDictionaryValueCallBacks);
        const bool trusted = AXIsProcessTrustedWithOptions(options);
        CFRelease(options);
        return trusted;
    }

    namespace
    {
        // Caller owns the returned reference (CFRetain'd) - nullptr if
        // not found. Searches only THIS process's own windows (never a
        // different app/process) via AXUIElementCreateApplication(getpid()).
        AXUIElementRef findWindowByTitleSubstring(AXUIElementRef app, const juce::String& hint)
        {
            CFArrayRef windows = nullptr;
            if (AXUIElementCopyAttributeValue(app, kAXWindowsAttribute, (CFTypeRef*) &windows) != kAXErrorSuccess
                || windows == nullptr)
                return nullptr;

            AXUIElementRef found = nullptr;
            const CFIndex n = CFArrayGetCount(windows);
            for (CFIndex i = 0; i < n; ++i)
            {
                auto w = (AXUIElementRef) CFArrayGetValueAtIndex(windows, i);
                CFStringRef title = nullptr;
                if (AXUIElementCopyAttributeValue(w, kAXTitleAttribute, (CFTypeRef*) &title) == kAXErrorSuccess
                    && title != nullptr)
                {
                    const juce::String t = juce::String::fromCFString(title);
                    CFRelease(title);
                    if (t.contains(hint))
                    {
                        found = w;
                        CFRetain(found);
                        break;
                    }
                }
            }
            CFRelease(windows);
            return found;
        }
    }

    WindowBounds findOwnWindow(const juce::String& windowTitleHint)
    {
        WindowBounds result;
        if (!isAutomationAvailable())
            return result;

        AXUIElementRef app = AXUIElementCreateApplication(getpid());
        if (app == nullptr)
            return result;

        AXUIElementRef w = findWindowByTitleSubstring(app, windowTitleHint);
        if (w != nullptr)
        {
            AXValueRef posVal = nullptr;
            AXValueRef sizeVal = nullptr;
            CGPoint pos = CGPointZero;
            CGSize size = CGSizeZero;

            if (AXUIElementCopyAttributeValue(w, kAXPositionAttribute, (CFTypeRef*) &posVal) == kAXErrorSuccess && posVal != nullptr)
            {
                AXValueGetValue(posVal, (AXValueType) kAXValueCGPointType, &pos);
                CFRelease(posVal);
            }
            if (AXUIElementCopyAttributeValue(w, kAXSizeAttribute, (CFTypeRef*) &sizeVal) == kAXErrorSuccess && sizeVal != nullptr)
            {
                AXValueGetValue(sizeVal, (AXValueType) kAXValueCGSizeType, &size);
                CFRelease(sizeVal);
            }

            result.found = true;
            result.x = (float) pos.x;
            result.y = (float) pos.y;
            result.w = (float) size.width;
            result.h = (float) size.height;
            CFRelease(w);
        }
        CFRelease(app);
        return result;
    }

    bool clickNextPreset(const juce::String& windowTitleHint)
    {
        const auto bounds = findOwnWindow(windowTitleHint);
        if (!bounds.found || bounds.w <= 0.0f || bounds.h <= 0.0f)
            return false;

        // Activate THIS process as the system-wide frontmost application,
        // THEN raise the target window within it. Proven necessary by a
        // real instrumented run: kAXRaiseAction only reorders windows
        // inside our own app's z-order - it does NOT make our app the
        // system-wide frontmost application. CGEventPost's kCGHIDEventTap
        // dispatches a real, global HID-level click that the window server
        // routes to whatever is ACTUALLY topmost on screen at that point,
        // across every app - not just topmost among our own windows. A
        // diagnostic screenshot (SoundLibraryLearner.cpp's before/after
        // capture) showed a completely unrelated foreground window sitting
        // on top of Serum 2's window at the click coordinates while our
        // app was raised-but-not-frontmost; the click landed there and
        // Serum 2's state never changed. juce::Process::makeForegroundProcess()
        // wraps [NSApp activateIgnoringOtherApps:YES] - a no-op if this
        // process is already frontmost, so safe to call unconditionally.
        juce::Process::makeForegroundProcess();
        juce::Logger::writeToLog("  [diag] isForegroundProcess after makeForegroundProcess()="
            + juce::String(juce::Process::isForegroundProcess() ? "yes" : "no"));

        // Diagnostic-only: enumerate every one of THIS process's own AX
        // windows (title + real position/size) - proves exactly how many
        // real windows exist and where, rather than inferring it from a
        // single title-substring lookup.
        if (AXUIElementRef diagApp = AXUIElementCreateApplication(getpid()))
        {
            CFArrayRef diagWindows = nullptr;
            if (AXUIElementCopyAttributeValue(diagApp, kAXWindowsAttribute, (CFTypeRef*) &diagWindows) == kAXErrorSuccess
                && diagWindows != nullptr)
            {
                const CFIndex n = CFArrayGetCount(diagWindows);
                juce::Logger::writeToLog("  [diag] own AX window count=" + juce::String((int) n));
                for (CFIndex i = 0; i < n; ++i)
                {
                    auto w = (AXUIElementRef) CFArrayGetValueAtIndex(diagWindows, i);
                    CFStringRef title = nullptr;
                    juce::String titleStr = "<no title>";
                    if (AXUIElementCopyAttributeValue(w, kAXTitleAttribute, (CFTypeRef*) &title) == kAXErrorSuccess && title != nullptr)
                    {
                        titleStr = juce::String::fromCFString(title);
                        CFRelease(title);
                    }
                    AXValueRef posVal = nullptr; CGPoint pos = CGPointZero;
                    AXValueRef sizeVal = nullptr; CGSize size = CGSizeZero;
                    if (AXUIElementCopyAttributeValue(w, kAXPositionAttribute, (CFTypeRef*) &posVal) == kAXErrorSuccess && posVal != nullptr)
                    { AXValueGetValue(posVal, (AXValueType) kAXValueCGPointType, &pos); CFRelease(posVal); }
                    if (AXUIElementCopyAttributeValue(w, kAXSizeAttribute, (CFTypeRef*) &sizeVal) == kAXErrorSuccess && sizeVal != nullptr)
                    { AXValueGetValue(sizeVal, (AXValueType) kAXValueCGSizeType, &size); CFRelease(sizeVal); }
                    juce::Logger::writeToLog("  [diag]   window[" + juce::String((int) i) + "] title=\"" + titleStr
                        + "\" pos=(" + juce::String((int) pos.x) + "," + juce::String((int) pos.y)
                        + ") size=(" + juce::String((int) size.width) + "x" + juce::String((int) size.height) + ")");
                }
                CFRelease(diagWindows);
            }
            CFRelease(diagApp);
        }

        // Raised TWICE, once before and once after the settle sleep: a real
        // instrumented run showed the FIRST raise losing the race to this
        // same app's own main editor window re-claiming key/front status as
        // part of activation settling (both windows belong to this one
        // process, so activating it doesn't by itself decide WHICH of our
        // windows ends up frontmost) - a diagnostic screenshot taken right
        // at click time showed the main editor window on top instead of
        // Serum 2. Raising again immediately before the click reclaims it.
        auto raiseTargetWindow = [&]
        {
            if (AXUIElementRef app = AXUIElementCreateApplication(getpid()))
            {
                if (AXUIElementRef w = findWindowByTitleSubstring(app, windowTitleHint))
                {
                    AXUIElementPerformAction(w, kAXRaiseAction);
                    CFRelease(w);
                }
                CFRelease(app);
            }
        };
        raiseTargetWindow();
        juce::Thread::sleep(120); // let the window server finish reordering/activating before the click
        raiseTargetWindow();
        juce::Thread::sleep(60);

        // Fraction measured empirically against a real, live Serum 2
        // session this project ran directly (window 1190x772 at screen
        // origin (161,75); the real next-preset arrow was located at real
        // screen point (1126,123), i.e. window-relative fraction
        // ((1126-161)/1190, (123-75)/772) = (0.811, 0.062)) - recorded
        // here as the measured basis, not an arbitrary guess. Still a
        // real, disclosed limitation: only correct if Serum 2's own
        // internal preset-bar layout proportions stay the same across
        // window sizes/versions, which has not been separately verified.
        constexpr float kNextPresetFracX = 0.811f;
        constexpr float kNextPresetFracY = 0.062f;

        const CGPoint p = CGPointMake(bounds.x + bounds.w * kNextPresetFracX,
                                       bounds.y + bounds.h * kNextPresetFracY);

        CGEventRef move = CGEventCreateMouseEvent(nullptr, kCGEventMouseMoved, p, kCGMouseButtonLeft);
        CGEventPost(kCGHIDEventTap, move);
        CFRelease(move);
        juce::Thread::sleep(100);

        // UNVERIFIED hypothesis, disclosed as such: one instrumented run's
        // CGWindowListCopyWindowInfo z-order snapshot showed Serum 2's
        // window genuinely ahead of every other on-screen app at some
        // point during that run, yet the click still didn't register -
        // consistent with a real, common macOS behaviour where the FIRST
        // mouse-down that also activates/focuses a just-raised window is
        // consumed as a "focus click" by AppKit (NSWindow's
        // acceptsFirstMouse defaults to NO) and never reaches the control
        // underneath. This dispatches one such "focus" down/up before the
        // real click below - same mechanism (CGEventPost/kCGHIDEventTap),
        // same coordinates, one extra press to absorb a swallowed first
        // click. Kept because it's cheap and plausible, NOT because it was
        // confirmed to fix the remaining failure - see SoundLibraryLearner
        // diag/test_log.txt from the actual verification run for the real,
        // still-unresolved result this pass ended on.
        CGEventRef focusDown = CGEventCreateMouseEvent(nullptr, kCGEventLeftMouseDown, p, kCGMouseButtonLeft);
        CGEventPost(kCGHIDEventTap, focusDown);
        CFRelease(focusDown);
        juce::Thread::sleep(40);
        CGEventRef focusUp = CGEventCreateMouseEvent(nullptr, kCGEventLeftMouseUp, p, kCGMouseButtonLeft);
        CGEventPost(kCGHIDEventTap, focusUp);
        CFRelease(focusUp);
        juce::Thread::sleep(150); // let the window fully settle as key/focused before the real click

        CGEventRef down = CGEventCreateMouseEvent(nullptr, kCGEventLeftMouseDown, p, kCGMouseButtonLeft);
        CGEventPost(kCGHIDEventTap, down);
        CFRelease(down);
        juce::Thread::sleep(40);

        CGEventRef up = CGEventCreateMouseEvent(nullptr, kCGEventLeftMouseUp, p, kCGMouseButtonLeft);
        CGEventPost(kCGHIDEventTap, up);
        CFRelease(up);
        juce::Thread::sleep(100);

        return true; // a click was dispatched - NOT proof the preset changed; callers must verify independently
    }
}
