#include "SerumAutomation.h"
#include <ApplicationServices/ApplicationServices.h>
#include <unistd.h>

namespace SerumAutomation
{
    bool isAutomationAvailable()
    {
        return AXIsProcessTrusted();
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
