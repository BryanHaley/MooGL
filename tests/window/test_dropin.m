/*
 * Copyright (C) The MooGL Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * test_dropin.m
 *
 * Built against Apple's OpenGL.framework, then given MooGL.framework by
 * tools/moogl-dropin. An NSOpenGLView, a plain view given a context with
 * setView:, and a dlopen of Apple's libGL the way SDL does it must all end
 * up in MooGL, and what they draw must reach the screen.
 */

#define GL_SILENCE_DEPRECATION
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#import <AppKit/AppKit.h>
#import <ImageIO/ImageIO.h>
#import <OpenGL/gl.h>
#include <dlfcn.h>

static int failures;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; fprintf(stderr, "FAIL: " __VA_ARGS__); fprintf(stderr, "\n"); } } while (0)

@interface RedView : NSOpenGLView
@end

@implementation RedView
- (void)drawRect:(NSRect)rect
{
    glClearColor(1, 0, 0, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    [[self openGLContext] flushBuffer];
}
@end

static NSOpenGLPixelFormat *pixelFormat(NSOpenGLPixelFormatAttribute profile)
{
    NSOpenGLPixelFormatAttribute a[] = { NSOpenGLPFADoubleBuffer, NSOpenGLPFAOpenGLProfile, profile, 0 };
    return [[NSOpenGLPixelFormat alloc] initWithAttributes: a];
}

static NSOpenGLPixelFormat *singleBuffered(void)
{
    NSOpenGLPixelFormatAttribute a[] = { NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersionLegacy, 0 };
    return [[NSOpenGLPixelFormat alloc] initWithAttributes: a];
}

static NSWindow *window(NSRect frame)
{
    NSWindow *w = [[NSWindow alloc] initWithContentRect: frame styleMask: NSWindowStyleMaskTitled
                                                backing: NSBackingStoreBuffered defer: NO];
    [w setReleasedWhenClosed: NO];
    return w;
}

// the middle pixel of a window as it is on screen, BGRA
static uint32_t centrePixel(NSWindow *w)
{
    NSString *path = [NSTemporaryDirectory() stringByAppendingPathComponent:
                      [NSString stringWithFormat: @"moogl_dropin_%d.png", getpid()]];
    NSString *cmd = [NSString stringWithFormat: @"screencapture -x -o -l%ld '%@'", (long)[w windowNumber], path];
    uint32_t pixel = 0;

    if (system([cmd UTF8String]) != 0)
        return 0;

    CGImageSourceRef src = CGImageSourceCreateWithURL((__bridge CFURLRef)[NSURL fileURLWithPath: path], NULL);
    CGImageRef img = src ? CGImageSourceCreateImageAtIndex(src, 0, NULL) : NULL;

    if (img)
    {
        size_t width = CGImageGetWidth(img), height = CGImageGetHeight(img);
        CGContextRef cg = CGBitmapContextCreate(&pixel, 1, 1, 8, 4, CGImageGetColorSpace(img),
                                                kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);
        CGContextDrawImage(cg, CGRectMake(-(CGFloat)(width / 2), -(CGFloat)(height / 2), width, height), img);
        CGContextRelease(cg);
        CGImageRelease(img);
    }
    if (src)
        CFRelease(src);
    unlink([path fileSystemRepresentation]);
    return pixel;
}

static void check(NSWindow *w1, NSWindow *w2, NSWindow *w3)
{
    uint32_t p1 = centrePixel(w1), p2 = centrePixel(w2), p3 = centrePixel(w3);

    CHECK((p1 & 0xffffff) == 0xff0000, "NSOpenGLView shows %06x, not red", p1 & 0xffffff);
    CHECK((p2 & 0xffffff) == 0x00ff00, "setView: view shows %06x, not green", p2 & 0xffffff);
    CHECK((p3 & 0xffffff) == 0x0000ff, "single buffered view shows %06x after glFlush, not blue", p3 & 0xffffff);

    // SDL finds GL this way; it must get MooGL's functions
    void *gl = dlopen("/System/Library/Frameworks/OpenGL.framework/Libraries/libGL.dylib", RTLD_LAZY);
    CHECK(gl && dlsym(gl, "glGetString") == (void *)glGetString, "dlopen of libGL found Apple's");

    printf(failures ? "failed\n" : "passed\n");
    fflush(stdout);
    _exit(failures ? 1 : 0);
}

int main(void)
{
    @autoreleasepool {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy: NSApplicationActivationPolicyRegular];

        // an NSOpenGLView, legacy profile as most old apps ask for
        NSWindow *w1 = window(NSMakeRect(100, 100, 200, 160));
        RedView *red = [[RedView alloc] initWithFrame: NSMakeRect(0, 0, 200, 160)
                                          pixelFormat: pixelFormat(NSOpenGLProfileVersionLegacy)];
        [w1 setContentView: red];
        [w1 makeKeyAndOrderFront: nil];

        // a plain view given a core context with setView:
        NSWindow *w2 = window(NSMakeRect(340, 100, 200, 160));
        NSView *plain = [[NSView alloc] initWithFrame: NSMakeRect(0, 0, 200, 160)];
        NSOpenGLContext *ctx = [[NSOpenGLContext alloc] initWithFormat: pixelFormat(NSOpenGLProfileVersion4_1Core)
                                                          shareContext: nil];
        [w2 setContentView: plain];
        [w2 makeKeyAndOrderFront: nil];

        // single buffered, as Mini vMac is: glFlush is what shows the frame
        NSWindow *w3 = window(NSMakeRect(580, 100, 200, 160));
        NSView *single = [[NSView alloc] initWithFrame: NSMakeRect(0, 0, 200, 160)];
        NSOpenGLContext *ctx3 = [[NSOpenGLContext alloc] initWithFormat: singleBuffered() shareContext: nil];
        [w3 setContentView: single];
        [w3 makeKeyAndOrderFront: nil];

        [NSTimer scheduledTimerWithTimeInterval: 0.3 repeats: NO block: ^(NSTimer *t) {
            [ctx setView: plain];
            [ctx makeCurrentContext];
            const char *renderer = (const char *)glGetString(GL_RENDERER);
            CHECK(renderer && strstr(renderer, "MGL"), "renderer is %s, not MooGL's", renderer ? renderer : "none");
            glClearColor(0, 1, 0, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            [ctx flushBuffer];

            [ctx3 setView: single];
            [ctx3 makeCurrentContext];
            glClearColor(0, 0, 1, 1);
            glClear(GL_COLOR_BUFFER_BIT);
            glFlush();

            // a first run may spend seconds building shaders, so this counts from the last frame
            [NSTimer scheduledTimerWithTimeInterval: 1.0 repeats: NO block: ^(NSTimer *t) { check(w1, w2, w3); }];
        }];

        [NSApp run];
    }
    return 1;
}

