#include "SerumAutomation.h"
#include <ApplicationServices/ApplicationServices.h>
#include <unistd.h>
#import <Cocoa/Cocoa.h> // needed for direct NSWindow/NSEvent delivery in clickNextPreset() - see its own comment

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

        // Real NSWindow* for the window matching `hint` in THIS process's
        // own [NSApp windows] - same match semantics as
        // findWindowByTitleSubstring (AX), used here to get an actual
        // NSWindow for direct event delivery. Returns nil if not found -
        // never guessed/assumed.
        NSWindow* findNSWindowByTitleSubstring(const juce::String& hint)
        {
            NSApplication* nsApp = [NSApplication sharedApplication];
            for (NSWindow* w in nsApp.windows)
            {
                const juce::String title = juce::String::fromCFString((CFStringRef) (w.title ? w.title : @""));
                if (title.contains(hint))
                    return w;
            }
            return nil;
        }

        // Delivers `event` directly to `window` via -[NSWindow sendEvent:]
        // - AppKit's own event-dispatch entry point, which performs its
        // own hit-testing (via the content view's hitTest:) to route the
        // event to whatever view is actually at that point (established by
        // a real instrumented investigation to be VSTGUI_NSView, Serum 2's
        // own control-surface view). A plain in-process Objective-C method
        // call - never asks the window server "what's on screen at this
        // pixel", so it doesn't depend on this window being the physically
        // topmost thing on screen, unlike CGEventPost/CGEventPostToPid
        // (both tried first, real instrumented runs: events confirmed
        // dispatched, but Serum's state never changed either time).
        // -sendEvent: (not -mouseDown:/-mouseUp: called directly on
        // contentView) specifically because it performs AppKit's own
        // correct hit-test routing to the deepest view at the point - the
        // content view chain here is JUCEView -> JuceInnerNSView ->
        // VSTGUI_NSView, and calling -mouseDown: directly on contentView
        // would skip that routing and might never reach VSTGUI_NSView at
        // all.
        void sendEventDirectlyToWindow(NSWindow* window, NSEvent* event, const char* label)
        {
            juce::Logger::writeToLog(juce::String("  [diag] delivering ") + label
                + " via [NSWindow sendEvent:] directly to windowNumber=" + juce::String((int) window.windowNumber)
                + " title=\"" + juce::String::fromCFString((CFStringRef) window.title) + "\""
                + " - bypasses CGEventPost/CGEventPostToPid/HID tap/WindowServer screen hit-testing entirely");
            [window sendEvent:event];
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
        // system-wide frontmost application. juce::Process::makeForegroundProcess()
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
        // windows ends up frontmost). Raising again immediately before the
        // click reclaims it.
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
        juce::Logger::writeToLog("  [diag] exact click point=(" + juce::String((int) p.x) + "," + juce::String((int) p.y) + ")"
            + " (AbletonCopilot/Serum share one process - Serum is hosted in-process as a VST3 plugin,"
            + " not a separate PID - AbletonCopilot pid=" + juce::String((int) getpid()) + " IS Serum's pid here)");

        // Direct AppKit event delivery - investigated and verified before
        // implementing (a real AX-tree dump + NSWindow/view-hierarchy dump
        // found the real NSWindow "Serum 2 - <role>" and its VSTGUI_NSView
        // content view). Both CGEventPost (global) and CGEventPostToPid
        // (process-targeted) were tried and tested for real first - clicks
        // were confirmed dispatched, but Serum's state never changed
        // either time. A plain in-process Objective-C method call, which
        // never asks the window server what's on screen, was independently
        // verified twice (real instrumented runs, real Serum state
        // fingerprint changes both times) to actually work. Same
        // coordinate calculation, same activation/raise above, same
        // timings below - only the construction+delivery of the 5 events
        // changed from the original CGEventPost-based implementation.
        NSWindow* matchedWindow = findNSWindowByTitleSubstring(windowTitleHint);
        if (matchedWindow == nil)
        {
            juce::Logger::writeToLog("  [diag] direct-NSEvent delivery FAILED: no NSWindow in [NSApp windows] matched \""
                + windowTitleHint + "\" - cannot deliver directly, not falling back to any other mechanism");
            return false;
        }

        // Convert the existing screen/top-left click point `p` into the
        // matched window's own local (AppKit bottom-left-origin)
        // coordinate space, using the window's own -convertRectFromScreen:
        // (which uses its actual current frame internally) rather than
        // hand-rolled math against an assumed screen height.
        CGFloat mainScreenHeightForLog = 0;
        if (NSScreen* mainScreen = [NSScreen mainScreen])
            mainScreenHeightForLog = mainScreen.frame.size.height;
        const NSPoint screenPointBottomLeftOrigin = NSMakePoint(p.x, mainScreenHeightForLog - p.y);
        const NSRect  screenPointRect             = NSMakeRect(screenPointBottomLeftOrigin.x, screenPointBottomLeftOrigin.y, 0, 0);
        const NSPoint windowLocalPoint            = [matchedWindow convertRectFromScreen:screenPointRect].origin;

        juce::Logger::writeToLog("  [diag] matched NSWindow windowNumber=" + juce::String((int) matchedWindow.windowNumber)
            + " title=\"" + juce::String::fromCFString((CFStringRef) matchedWindow.title) + "\""
            + " isKeyWindow=" + (matchedWindow.isKeyWindow ? "yes" : "no")
            + " isMainWindow=" + (matchedWindow.isMainWindow ? "yes" : "no")
            + " isVisible=" + (matchedWindow.isVisible ? "yes" : "no")
            + " pid=" + juce::String((int) getpid()));
        juce::Logger::writeToLog("  [diag] original screen point (top-left origin)=(" + juce::String((int) p.x) + "," + juce::String((int) p.y) + ")"
            + " converted window-local point=(" + juce::String((int) windowLocalPoint.x) + "," + juce::String((int) windowLocalPoint.y) + ")");

        int nsEventCounter = 0;
        auto sendMouseEvent = [&](NSEventType type, const char* label)
        {
            NSEvent* event = [NSEvent mouseEventWithType:type
                                                  location:windowLocalPoint
                                             modifierFlags:0
                                                 timestamp:[[NSProcessInfo processInfo] systemUptime]
                                              windowNumber:matchedWindow.windowNumber
                                                   context:nil
                                               eventNumber:nsEventCounter++
                                                clickCount:1
                                                  pressure:(type == NSEventTypeLeftMouseDown ? 1.0f : 0.0f)];
            juce::Logger::writeToLog(juce::String("  [diag] event type=")
                + (type == NSEventTypeMouseMoved ? "MouseMoved" : type == NSEventTypeLeftMouseDown ? "LeftMouseDown" : "LeftMouseUp")
                + " label=" + label);
            sendEventDirectlyToWindow(matchedWindow, event, label);
        };

        sendMouseEvent(NSEventTypeMouseMoved, "mouseMoved");
        juce::Thread::sleep(100);

        // UNVERIFIED hypothesis, disclosed as such: one instrumented run's
        // z-order snapshot showed Serum 2's window genuinely ahead of every
        // other on-screen app at some point during that run, yet the click
        // still didn't register - consistent with a real, common macOS
        // behaviour where the FIRST mouse-down that also activates/focuses
        // a just-raised window is consumed as a "focus click" by AppKit
        // (NSWindow's acceptsFirstMouse defaults to NO) and never reaches
        // the control underneath. This dispatches one such "focus" down/up
        // before the real click below - same coordinates, same event
        // count, one extra press to absorb a swallowed first click. Kept
        // because it's cheap and plausible, not because it was separately
        // confirmed necessary on its own.
        sendMouseEvent(NSEventTypeLeftMouseDown, "focusMouseDown");
        juce::Thread::sleep(40);
        sendMouseEvent(NSEventTypeLeftMouseUp, "focusMouseUp");
        juce::Thread::sleep(150); // let the window fully settle as key/focused before the real click

        sendMouseEvent(NSEventTypeLeftMouseDown, "mouseDown");
        juce::Thread::sleep(40);

        sendMouseEvent(NSEventTypeLeftMouseUp, "mouseUp");
        juce::Thread::sleep(100);

        return true; // a click was dispatched - NOT proof the preset changed; callers must verify independently
    }
}
