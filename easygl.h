#ifndef EASYGL_H
#define EASYGL_H 20260927

/* =====================================================================
 * easygl.h - EasyX compatible drawing library implemented on OpenGL.
 *
 * Target   : Windows / MinGW GCC 4.9.2+ and MSVC 2015+.
 *            C flavour: C11 (-std=c11 / gnu11 / default).  C99 and C89 are
 *            not supported - see "Which C dialect" below.
 *            C++ flavour: C++11 or later, where the overloads are real.
 * Standard : strict C11. No STL, no references, no overloading, no default
 *            arguments - every EasyX "default argument" is emulated with a
 *            variadic (and, for TCHAR strings, _Generic) dispatch macro, so
 *            user code looks exactly like EasyX.
 * Encoding : this file is pure ASCII on purpose.
 *
 * Version  : EASYGL_H / EASYGL_VER / EASYGL_VERSION  (EasyX calls its own
 *            version macro EASYX_VER; here the version is carried by the
 *            include-guard macro EASYGL_H as requested.)
 *
 * Text     : every text API comes in an A (byte string) and a W (UTF-16)
 *            flavour. The A flavour decodes its input with UTF-8 first and
 *            falls back to the active ANSI code page (GBK/Big5/...), which
 *            is what fixes double byte characters such as Chinese: the old
 *            implementation baked one glyph per *byte* and produced two
 *            nonsense glyphs per Han character.
 *
 * Single translation unit: every helper is static, so include this file in
 * exactly one .c file (same rule as the original easygl.h).
 *
 * Revision 20260925 (bug fix pass)
 *   - InputBox() accepts the full EasyX signature again:
 *     InputBox(out, nMaxCount, prompt, title, pDefault, width, height,
 *              bHideCancelBtn) - eight arguments, matching EasyX exactly.
 *     Only five were dispatchable, so a call that passed pDefault / width /
 *     height / bHideCancelBtn did not compile.  The four trailing parameters
 *     are now honoured: pDefault
 *     pre-fills the edit box SELECTED, the way a rename field or a browser's
 *     address bar does it, so the first keystroke replaces the default
 *     outright instead of being appended to it (caret at the end) or
 *     landing in the middle and producing a mixture of the two.  Accept it
 *     with Enter, or type over it.  Keyboard messages still in flight are
 *     drained before the dialog's loop starts (gxIbDrainKeys()), and every
 *     key that is still physically down is muted until it comes back up
 *     (gxIbNoteHeld()) - so a "press P to rename" trigger, seen through
 *     GetAsyncKeyState() before its own WM_CHAR is dispatched, can neither
 *     land in the box nor auto repeat into it.  That is what made the same
 *     key press show the default on one run and "p" on the next.  width
 *     and height size the dialog (they are the WINDOW size, like EasyX, so
 *     the frame is subtracted first), and bHideCancelBtn hides the Cancel
 *     button - true, EasyX's default, leaves a single OK, so the two to five
 *     argument forms now show one button, and the default title is
 *     "InputBox".  (Older EasyX headers spell this argument bOnlyOK; it is
 *     the same value, described from the other end.)
 *     nMaxCount is enforced on the edit control with EM_LIMITTEXT, and a
 *     wide result that is too long for the caller's byte buffer is cut back
 *     a whole character at a time so a DBCS tail is never split.
 *     The prompt is measured (DT_CALCRECT + DT_WORDBREAK) instead of being
 *     clipped to a hard coded 34 px band, and a long one grows the dialog.
 *   - initgraph() calls DisableProcessWindowsGhosting(), so a window that is
 *     busy or waiting is not greyed out and labelled "not responding".
 *     That state is a timeout, not a diagnosis: DWM sends the window a
 *     WM_NULL and, if the answer is not back within a few seconds, it
 *     freezes the frame and rewrites the title bar.  Nothing else changes -
 *     the window still gets its messages and still draws.  Define
 *     GX_ALLOW_GHOSTING to keep the Windows default.
 *   - SetWorkingImage() with no argument is accepted again: it clears the
 *     working IMAGE and goes back to the window, which is what EasyX does.
 *     The implementation is renamed gxSetWorkingImage() because in C the
 *     public name has to be a dispatch macro, and a macro cannot be defined
 *     with the same name as a function it replaces - it would expand inside
 *     its own definition.  C++ keeps real overloads.  SetWorkingImage(NULL)
 *     counts as a single argument that expands to a parenthesised
 *     expression, which the argument counter cannot tell from "none", so
 *     the zero argument macro is variadic to absorb it.
 *   - getmessage(&msg, filter) works: the two argument form used to throw
 *     the ADDRESS away and wait on (BYTE)&msg, so getmessage(&msg, EX_KEY)
 *     never woke on a key.  The pointer now selects a filtered wait and the
 *     message is written into the caller's struct, as EasyX does.  A plain
 *     integer first argument keeps its old meaning (the filter itself).
 *   - optional: define GX_EASYX_HWND and declare "HWND hwnd;" at file scope
 *     and initgraph() / closegraph() keep it in step with the window, the
 *     way EasyX does.  It cannot be assigned unconditionally because the
 *     host declares hwnd AFTER including this header, so at the point
 *     initgraph() is compiled the name does not exist yet.  GetHWnd()
 *     always works and needs no macro.
 *   - new: strokepolylinef() fills a ribbon of a given width along a path in
 *     one pass, with setstrokecap() / setstrokejoin() choosing BUTT, ROUND or
 *     SQUARE ends and MITER, ROUND or BEVEL bends.  Unlike
 *     setlinestyle(PS_SOLID, thickness), which draws every segment as its own
 *     quad, one ribbon covers each pixel once, so a translucent fill stays
 *     even however sharply the path doubles back.  The outline self
 *     intersects, so it is filled under WINDING and the caller's fill mode is
 *     restored afterwards.
 *   - strokepolygon() / strokepolygonf() / fillstrokepolygon() /
 *     fillstrokepolygonf() (and the closed form used by all of them) now
 *     round EVERY corner of a closed figure.  The loop used to append only
 *     pts[0] to close the path, which left the corner at pts[0] as the
 *     junction of the head and the tail - and gxStrokeOutline() only rounds
 *     the corners that sit strictly inside the point list, so a closed
 *     figure came out with one sharp, unjoined corner while every other one
 *     was round (a five pointed star showed four round tips and one cut
 *     off).  The path now carries one segment past the start, which turns
 *     pts[0] into an interior corner; the first segment is walked twice and
 *     the outline is one polygon filled once under WINDING, so the overlap
 *     is covered once and a translucent ribbon keeps an even tone.
 *   - stroke*() paints with the LINE colour, like every other stroking call
 *     does: strokepolyline / strokepolylinef / strokepolygon / strokepolygonf
 *     take setlinecolor(), and fillstrokepolygon() fills with setfillcolor()
 *     and then rims with setlinecolor().  They used to take the fill colour,
 *     which disagreed with polygon() / polyline(); code that set the fill
 *     colour for a stroke*() call has to set the line colour instead.
 *   - rotateimage: a positive angle now rotates clockwise like EasyX / GDI,
 *     self rotation (dst == src) no longer destroys the source, the blit no
 *     longer inherits the current setrop2() mode, "highquality" really picks
 *     LINEAR filtering and the working image target is refreshed after the
 *     implicit Resize().
 *   - setrop2: R2_MASKPENNOT / R2_MASKNOTPEN and R2_MERGEPENNOT /
 *     R2_MERGENOTPEN are no longer swapped, the draw command that is still
 *     open is closed when the mode changes, and glyphs keep their shape
 *     (alpha tested) instead of turning into solid blocks.
 *   - putimage: SRCERASE / NOTSRCERASE map to the right ROP2 codes and the
 *     source rectangle is clipped to the image.
 *   - IMAGE: Resize() and loadimage(..., w, h) keep a valid magic and a valid
 *     render target, and never delete a texture that is still queued.
 *   - misc: CS_DBLCLKS, NOCLOSE, SHOWCONSOLE and INIT_RENDERMANUAL are
 *     honoured, setrendermode(RENDER_AUTO) actually presents.
 *   - setorigin / setaspectratio / setcliprgn / clearcliprgn now flush the
 *     pending batch first: vertices are queued in logical space and the
 *     projection / scissor box is applied when the batch is drawn, so
 *     primitives that were queued before the change used to jump.
 *   - clip region: the box is clamped on all four sides (a region that lies
 *     completely outside the target produced a negative glScissor() width,
 *     i.e. GL_INVALID_VALUE), and the exclusive right/bottom RECT returned
 *     by GetRgnBox() is no longer inflated by one pixel.
 *   - gxPresent() used the *current* target size for the viewport, so drawing
 *     into a smaller IMAGE and presenting showed a squeezed sub rectangle.
 *   - getimage(dst, x, y, w, h) passed the LOGICAL rectangle straight to the
 *     framebuffer reader, so with setaspectratio() / setorigin() it grabbed a
 *     completely different region than the one that was asked for.
 *   - getpixel() / gxDevToLogX/Y / floodfill truncated towards zero instead
 *     of flooring, so negative coordinates read the wrong pixel (or a pixel
 *     that should have been reported as out of range).
 *   - setaspectratio() accepted NaN / non positive values and poisoned the
 *     projection matrix with NaN (nothing drew any more).
 *   - gxPolyFill() fanned triangles from the centroid: wrong for concave
 *     polygons, and setpolyfillmode() had no effect at all.  Replaced by a
 *     scanline fill that honours ALTERNATE and WINDING.
 *   - PS_DASH / PS_DOT / PS_DASHDOT / PS_DASHDOTDOT / PS_USERSTYLE were
 *     stored but never used, so every dashed pen drew a solid line.  Dash
 *     patterns now follow the GDI system styles, scale with the pen width
 *     and keep their phase across the segments of one figure.
 *   - The hatch / pattern brush was tiled in DEVICE space (gl_FragCoord),
 *     so it neither followed setorigin() nor scaled with setaspectratio(),
 *     and BS_PATTERN did not tile at all (IMAGE textures are CLAMP).  It is
 *     now anchored in logical space and tiled with REPEAT.
 *   - g_patSx/g_patSy were dead and produced -Wunused warnings.
 *     gxInitScreenScale() was dead too; it is kept, and exposed as
 *     getinitscreenscale(), because it is wanted as a rough "how big is
 *     this display" figure - see the note there about it being a different
 *     thing from getdpi().
 *   - BS_PATTERN now tiles: the draw switches the shared IMAGE texture to
 *     REPEAT and back (hatch textures stay REPEAT, IMAGE textures are
 *     CLAMP for putimage()).
 *   - Default render mode was RENDER_MANUAL, so gxAutoPresent() never ran
 *     and a program that does not call BeginBatchDraw() / FlushBatchDraw()
 *     showed an empty window: the canvas was drawn but never presented.
 *     EasyX starts in RENDER_AUTO (a primitive becomes visible as it is
 *     drawn) and that is now the default here as well; INIT_RENDERMANUAL
 *     selects the manual mode, and graphdefaults() no longer resets the
 *     mode because EasyX treats it as a property of the window.
 *   - The auto present throttle (8 ms) used to DROP the frame it skipped, so
 *     the last primitive of a burst stayed invisible until the next event.
 *     The frame is now owed to gxPump(), which shows it before the caller
 *     blocks in getmessage() / getmessage(filter).
 *   - getmessage() did not block: it returned an all zero ExMessage when the
 *     queue was empty, so "while (1) { getmessage(); draw(); }" spun at full
 *     CPU speed and any frame counter in it ran away.  EasyX blocks until a
 *     message arrives, and so does this one now (WaitMessage() + pump).
 *     peekmessage() keeps its non blocking behaviour, and GetMouseMsg() /
 *     getmessage(MOUSEMSG*) block as well for the same reason.
 *   - GetImageBuffer() cost a full canvas round trip per call: glReadPixels
 *     (a GPU sync point) plus two extra full image passes plus two 900 kB
 *     mallocs.  EasyX hands back a pointer into real memory, so a program
 *     that repaints every pixel was several times slower here.  Reduced by:
 *       * fusing the alpha pass into the row flip (measured ~2x on that
 *         part: the row stays in cache instead of being walked twice);
 *       * reusing a scratch buffer instead of malloc / free per frame;
 *       * setimagebuffermode(GX_IMGBUF_DISCARD), which skips the read back
 *         entirely for callers that overwrite every pixel.
 *   - gradtriangle(): an easygl extension - a triangle with its own colour
 *     at each corner.  Three vertices on the GPU, where the pixel by pixel
 *     route costs a whole canvas read back plus upload.
 *   - initgraph() now hides the console unless EX_SHOWCONSOLE is given.
 *     A MinGW build without -mwindows is a /SUBSYSTEM:CONSOLE program, so a
 *     black cmd window sat behind the graphics window for the whole run.
 *     EasyX only keeps the console when the flag asks for it, and so does
 *     this now.  The window is hidden, not freed - ShowWindow(SW_HIDE), not
 *     FreeConsole() - so printf() / getch() keep working, and closegraph()
 *     brings it back.  showconsole() / hideconsole() are the manual form.
 *   - Alpha channel, carried by COLORREF itself.  EasyX leaves the top byte
 *     of a 0x00BBGGRR colour unused, so it is promoted to alpha:
 *
 *         setfillcolor(ARGB(128, 255, 0, 0));  solidcircle(...);   50%
 *         setfillcolor(RGB(255, 0, 0));        solidcircle(...);   opaque
 *
 *     The byte is a TRANSPARENCY:
 *
 *        0x00 = fully OPAQUE      0x80 = half     0xFF = INVISIBLE
 *
 *     so RGB(), which builds 0x00BBGGRR, is already fully opaque and needs
 *     no redefinition - every existing EasyX colour and program is correct
 *     as it stands.  The old convention read 0 as "no alpha given" and
 *     therefore also opaque, which meant "fully transparent" could not be
 *     expressed at all; that is what changed.  ARGB(0, r, g, b) is now
 *     opaque and ARGB(255, r, g, b) is invisible.
 *
 *     GL's own alpha means the opposite (coverage: 1 = solid), so the value
 *     is inverted in gxAlphaOf() - the single place the two conventions
 *     meet.  setalpha() / getalpha() speak transparency too, so 0 means
 *     solid and 255 means invisible.
 *
 *     GetImageBuffer() gives back 0x00BBGGRR like EasyX, so a pixel read
 *     from it is opaque and can be drawn straight back out.
 *
 *     Geometry and text carry alpha PER VERTEX (gxV() writes it into the
 *     vertex colour), so gradtriangle() fades each corner independently and
 *     the GPU interpolates - no extra cost, no batch split.  Textures are
 *     the exception: putimage() and the patterned brushes replace vColor
 *     with the texture outright (uUseTex 2 and 4), so for those the level
 *     comes from the per command uAlpha uniform, i.e. from setalpha().
 *
 *     The opaque path is unchanged apart from one byte test per vertex:
 *     an alpha byte of 0 or 255 writes 1.f, which is exactly what the old
 *     code hard coded, so no batch is split and no extra state is set.
 *
 *     Two caveats.  Alpha is only honoured in R2_COPYPEN - the other ROP
 *     modes run with blending off and an alpha test at 0.5, so there a
 *     level below 128 discards the primitive instead of fading it.  And
 *     ARGB(0, r, g, b) is opaque, not invisible (0 has to mean "no alpha"
 *     for RGB() to keep working); use setalpha(0) to hide everything.
 *   - MSAA anti aliasing, off by default: setaasamples(4) before or after
 *     initgraph().  This is the smallest change that smooths everything at
 *     once, because it happens below the renderer: the canvas framebuffer is
 *     rebuilt with a multisampled renderbuffer as its colour attachment, the
 *     GPU resolves on every edge, and gxMsaaResolve() folds the samples back
 *     into the plain texture with glBlitFramebuffer() before anything reads
 *     it (gxPresent, getpixel, getimage, GetImageBuffer).  No drawing code,
 *     no vertex data and no shader changes - geometry, text, images and
 *     patterns all get smoothed for free.
 *
 *     It needs GL 3.0 (or ARB_framebuffer_object, or the older
 *     EXT_framebuffer_multisample + EXT_framebuffer_blit pair; all three
 *     spellings are looked up).  Anything that is missing, or a sample count
 *     the driver refuses, silently falls back to no anti aliasing rather
 *     than failing.
 *   - setglversion(major, minor) asks for a specific OpenGL context version.
 *     It must be called before initgraph(), and it always asks for the
 *     COMPATIBILITY profile: this library's shaders are #version 120 and use
 *     varying / texture2D / gl_FragColor, and it never binds a VAO - all of
 *     which a core profile rejects, so a core context would draw nothing.
 *     If the requested version is refused the creation falls back to 3.3 and
 *     then to whatever wglCreateContext() gives, so it never fails.
 *     Requesting a newer version does NOT make drawing faster - see the note
 *     above the function.
 *   - getvendorinfo() / getrendererinfo() / getglver() / getglslver() wrap
 *     glGetString(), and getadapterinfo() / getdisplaymode() wrap
 *     EnumDisplayDevicesA() / EnumDisplaySettingsA().  All return "n/a"
 *     instead of NULL when there is no context yet, so they are safe to call
 *     at any time.  EasyX has no equivalents.
 *   - Lightweight extensions (see sections 11c, 16b, 17a and the window
 *     helpers): gradrectangle() (four corner colours, two triangles, the
 *     GPU interpolates - the rectangle form of gradtriangle());
 *     setblendmode() / getblendmode() with GX_BLEND_ALPHA / ADD / SUB /
 *     MUL / SCREEN / NONE for glow, shadow and tint effects, which EasyX
 *     cannot do at all; getfps() / getframetime() / settargetfps() so a
 *     loop can cap itself instead of spinning a core when vsync is off;
 *     setimagefilter() to switch IMAGE textures between NEAREST (crisp
 *     1:1, the old behaviour and still the default) and LINEAR (smooth
 *     scaled sprites) - this covers what MSAA cannot, because MSAA only
 *     touches the edges of geometry and does nothing for the inside of a
 *     texture; flipimage() / mirrorimage() (one flipped quad, no CPU
 *     pixels, works in place); and hidecursor() / showcursor() /
 *     setwindowalpha() / setwindowtopmost() plus their getters, which are
 *     plain Win32 one liners EasyX programs otherwise have to reach for
 *     windows.h themselves.
 *   - The blend mode is recorded per draw command (GLCmd.blend) like the
 *     ROP and the alpha, so changing it closes the open batch instead of
 *     silently repainting primitives that were queued earlier.  All modes
 *     need blending on, so they are ignored outside R2_COPYPEN, exactly
 *     like alpha.
 *   - gxPresent() only set uUseTex, so the uAlpha / uNoBlend uniforms still
 *     held whatever the last command left.  A primitive drawn with a
 *     non COPYPEN rop at an alpha below 128 therefore made the next
 *     present discard the whole frame (uNoBlend 1 + c.a < 0.5): black or
 *     frozen window.  The blit now forces both uniforms.
 *   - cleardevice() cleared to hard coded black instead of the current
 *     background colour, so setbkcolor(WHITE); cleardevice(); stayed
 *     black - and disagreed with the clear*() family, which always used
 *     g_gx_bkColor.  It now clears with g_gx_bkColor like EasyX.
 *   - gxImageAlloc() leaked the texture when framebuffer creation failed:
 *     the early return left img->tex set while the magic (the only thing
 *     that makes gxImageDestroy() free it) was still zero.
 *   - EasyX compatibility pass (see section 18b): setaspectratio() accepts
 *     the negative factors EasyX allows (setaspectratio(1, -1)), initgraph()
 *     returns HWND and understands the EX_* flags, drawtext() returns the
 *     text height, and the missing EasyX APIs were added: getaspectratio,
 *     graphdefaults, the clear* family, polybezier, settextstyle,
 *     gettextstyle, GetImageBuffer, setcapture / releasecapture,
 *     flushmessage(filter), PeekMouseMsg, FlushBatchDraw(rect),
 *     GetEasyXVer, the colour model helpers and the graphics.h leftovers
 *     (bar, bar3d, drawpoly, fillpoly, getcolor, setcolor, getmaxx,
 *     getmaxy, setwritemode).  The argument counting macro used by the C11
 *     dispatch only worked up to 8 arguments and silently broke on more.
 *   - Newly allocated storage is UNDEFINED, so a window that had only just
 *     been created showed whatever that memory last held - speckles that
 *     stayed until the program called cleardevice().  glTexImage2D(..., NULL)
 *     and glRenderbufferStorage*() both leave the contents undefined, so
 *     initgraph() and every window resize now clear the canvas to g_gx_bkColor
 *     (after gxApplyWindowState() / gxMsaaCreate(), because with MSAA on it
 *     is the multisample renderbuffer that is drawn), and gxImageAlloc()
 *     clears a fresh IMAGE to opaque black.  EasyX hands both over already
 *     in the background colour.
 *   - cleardevice() cleared to hard coded black instead of g_gx_bkColor.
 *   - setwinsize() and getwinsize() disagreed about units: the setter takes
 *     logical units and scales them by the DPI factor, while the getter
 *     reported raw device pixels, so setwinsize(1024, 768) followed by
 *     getwinsize() came back as 1536x1152 on a 150% display.  getwinsize()
 *     is now the inverse of setwinsize() and reports logical units at any
 *     DPI; getwindevsize() is the new device pixel form.
 *   - Performance pass on the hot path (nothing about it changes what is
 *     drawn - the batching model, the number of draw calls and the vertex
 *     data are exactly as they were):
 *       * gxFlush() re-allocated the vertex buffer with glBufferData() every
 *         single time.  That call does not just copy, it allocates, and the
 *         driver may either really reallocate or stall until the GPU is done
 *         with the old store.  The capacity is now remembered and the common
 *         case - the same buffer reused frame after frame - goes through
 *         glBufferSubData(), which only copies.  The capacity grows by
 *         doubling, so a frame that keeps getting slightly bigger does not
 *         reallocate on every one of them.
 *       * Per command GL state is cached: uniforms, the bound texture and
 *         the ROP are only set when they differ from what is in force.  A
 *         frame of plain fills repeats the same values for every command,
 *         and glUniform*() / glBindTexture() are not free at thousands of
 *         calls per frame.  The first command of every flush always sets
 *         everything, because GL state is global and gxPresent(),
 *         gxClearFbo() and rotateimage() all touch it in between.  The
 *         texture filter is cached per bound texture - it belongs to the
 *         texture, so binding a different one invalidates it (without that,
 *         two putimage() calls on one image with different filters would
 *         have kept the first one's).
 *       * The byte to float colour conversion in gxV() was switched to a
 *         multiply by 1/255 and then switched back: measured on 200000 quads
 *         the multiply was 2% SLOWER, so it divides by 255 again.
 *       * Quads are now 4 vertices plus 6 indices instead of 6 vertices.
 *         Rectangle fills, putimage() and text are all quads, and a tile
 *         map is nothing but quads, so this removes a third of the vertex
 *         work in the cases that dominate.  The index pattern is
 *         precomputed once (it only depends on the absolute vertex number)
 *         and only the part that is new is uploaded.  A batch cannot mix
 *         quads with the fans and strips of the circle and polygon code, so
 *         gxV() and gxQuad*() each close the batch before switching, and
 *         every batch starts on a multiple of 4 vertices - at most three
 *         padding vertices, which no command covers, so they are never
 *         drawn.  Expanding a command through the index pattern gives
 *         exactly the triangles the old layout did.
 *       * The automatic flush threshold was lowered to 50000 and then put
 *         back to 300000: a 120000 vertex frame uploads once at 300000 and
 *         three times at 50000, and smaller uploads are not cheaper here.
 *         splitting the batch earlier costs nothing because it is replayed
 *         in one go once it is flushed anyway.
 * ===================================================================== */

/*======================================================================
 *  Colour model
 *====================================================================*/
/* A colour is a 32 bit COLORREF, exactly as in EasyX:
 *
 *       0xAABBGGRR, which in memory on a little endian machine runs
 *       RR GG BB AA from the lowest address upwards.
 *
 *   - The bit layout is what every macro below uses:
 *
 *         bits  0..7  = red      (the LOW byte)
 *         bits  8..15 = green
 *         bits 16..23 = blue
 *         bits 24..31 = alpha    (the HIGH byte)
 *
 *     So the low byte is R and the high byte is A.  Written the other way
 *     round that is 0xAARRGGBB, the spelling most other libraries use,
 *     where the FIRST byte of the literal is alpha and the last is blue.
 *     RGB(r,g,b), GetRValue(), GetGValue() and GetBValue() are the
 *     ordinary Win32 macros and keep working unchanged.
 *
 *   - Bits 24..31 are the alpha channel, and it is a TRANSPARENCY:
 *
 *         0x00 = fully opaque      0x80 = half      0xFF = invisible
 *
 *     RGB() builds 0x00BBGGRR, so it yields an opaque colour with no extra
 *     work.  ARGB(a,r,g,b) and RGBA(r,g,b,a) build one explicitly, and
 *     GetAValue() reads the byte back.
 *
 *   - BGR(c) swaps red and blue and keeps the alpha byte intact.
 *
 *   - HSLtoHSV() / HSVtoHSL() convert between the two colour models, and
 *     HSLtoRGB() / RGBtoHSL() (and the HSV pair) go to and from a COLORREF.
 *
 * OpenGL means the opposite: its alpha is COVERAGE, where 1.0 is solid.
 * gxAlphaOf() converts with (255 - a) / 255, and that is the single place
 * the two conventions meet.  setalpha() and setwindowalpha() apply the same
 * inversion to their argument, so every "alpha" in this library - a colour
 * byte, a setalpha() level, a window opacity - is a transparency.
 */

#define EASYGL_VER      20260927
#define EASYGL_VERSION  "20260927"

#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS 1
#endif

/* Several MinGW installs - and MSVC projects that set nothing at all - leave
 * WINVER / _WIN32_WINNT at their 0x0400 default, which hides everything that
 * arrived with Windows 2000: WM_UNICHAR, WS_EX_LAYERED, LWA_ALPHA and the
 * layered window functions.  This header uses all of them, so raise both to
 * 0x0501 (XP) when the user has not asked for something newer.  A value the
 * user did supply is honoured and never lowered.  Without this, switching to
 * an older GCC turns into "WM_UNICHAR undeclared" at gxMsgIsType(). */
#ifndef WINVER
#define WINVER 0x0501
#elif WINVER < 0x0501
#undef WINVER
#define WINVER 0x0501
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#elif _WIN32_WINNT < 0x0501
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif

#include <windows.h>
#include <GL/gl.h>
#include <conio.h>

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* Belt and braces for the block above.  It can only help when this file is
 * the one that pulls in <windows.h>: if the user's own code includes it
 * first, with a low WINVER, windows.h has already been read and refuses to be
 * read again, so the prototypes and constants stay hidden.  Redefine the few
 * identifiers this header actually uses that are gated on WINVER >= 0x0500.
 * Each is guarded, so a correctly configured SDK is left completely alone.
 * (WM_UNICHAR doubles as WM_IME_CHAR, which is why the value is 0x0109; the
 * two are the same message in every SDK that declares them.) */
#ifndef WM_UNICHAR
#define WM_UNICHAR 0x0109
#endif
#ifndef UNICODE_NOCHAR
#define UNICODE_NOCHAR 0xFFFF
#endif
#ifndef WS_EX_LAYERED
#define WS_EX_LAYERED 0x00080000
#endif
#ifndef LWA_COLORKEY
#define LWA_COLORKEY 0x00000001
#endif
#ifndef LWA_ALPHA
#define LWA_ALPHA 0x00000002
#endif

/*======================================================================
 * Which C dialect, and what the EasyX style overloads may use
 *====================================================================*/
/* EasyX is a C++ library: outtextxy(x, y, "s") and outtextxy(x, y, L"s") are
 * two overloads and putimage() takes anywhere from three to eight arguments.
 * Neither overloading nor default arguments exist in C, so this header
 * emulates them - a variadic macro counts the arguments and picks an
 * implementation, and string arguments are routed to the A or the W entry
 * point.
 *
 * The C flavour of this header is C11, unconditionally.  The two things that
 * need it are _Generic, which is how a string argument is told apart from a
 * wide string (and a single char from a string), and _Static_assert, which
 * checks the layouts the polygon cores rely on.  Both are C11 and neither
 * has a substitute:
 *
 *   - A sizeof test cannot route strings.  ExMessage and MOUSEMSG are both
 *     24 bytes, and 'A' is an int with no elements at all, so
 *     outtextxy(x, y, 'A') and peekmessage(&mm) on a MOUSEMSG would both be
 *     unresolvable.
 *   - __builtin_choose_expr is no help either: it still type checks the
 *     branch it discards, so the wrong arm warns.
 *
 * C99 is therefore not supported.  Neither is C89 - variadic macros do not
 * exist before C99, so the default arguments that make putimage(x, y, &img)
 * work cannot be emulated at all, and the header additionally uses inline,
 * <stdbool.h>, long long, // comments, declarations after statements and
 * for loop declarations throughout.
 *
 * Build with -std=c11 (or gnu11, or the default, which is a gnu mode).
 * MinGW GCC 4.9+ and MSVC 2015+ (/std:c11) are both fine.
 *
 * GX_HAS_GENERIC is 1 and is only reported, not tested - it used to switch
 * to a sizeof based fallback, and that fallback is gone.  GX_NO_GENERIC
 * forces 0 for the test build, which is how the no _Generic path used to be
 * exercised; nothing in the header depends on it any more. */
#if defined(GX_NO_GENERIC)
#  define GX_HAS_GENERIC 0      /* test build only: exercises no _Generic */
#else
#  define GX_HAS_GENERIC 1      /* C11 - always available */
#endif

/* `inline` is C11, but MSVC only learned it in C mode with VS2015. */
#ifndef GX_INLINE
#  if defined(_MSC_VER) && (_MSC_VER < 1900) && !defined(__cplusplus)
#    define GX_INLINE static __inline
#  else
#    define GX_INLINE static inline
#  endif
#endif

/* Silences -Wunused-function / -Wunused-variable for helpers that exist for
 * the host program only.  MinGW gcc 4.9 understands __attribute__. */
#if defined(__GNUC__) || defined(__clang__)
#  define GX_UNUSED __attribute__((unused))
#else
#  define GX_UNUSED
#endif

/* EasyX sources are byte oriented: force the ANSI flavour of LOGFONT. */
#define LOGFONT LOGFONTA

/*---------------------------- EasyX colors ----------------------------*/
#ifndef BLACK
/* The EasyX colour constants, exactly as EasyX defines them.  Their alpha
 * byte is 0, which under this convention (see section 2b) means FULLY
 * OPAQUE - so they need no change at all. */
#define BLACK        0
#define BLUE         0xAA0000
#define GREEN        0x00AA00
#define CYAN         0xAAAA00
#define RED          0x0000AA
#define MAGENTA      0xAA00AA
#define BROWN        0x0055AA
#define LIGHTGRAY    0xAAAAAA
#define DARKGRAY     0x555555
#define LIGHTBLUE    0xFF5555
#define LIGHTGREEN   0x55FF55
#define LIGHTCYAN    0xFFFF55
#define LIGHTRED     0x5555FF
#define LIGHTMAGENTA 0xFF55FF
#define YELLOW       0x55FFFF
#define WHITE        0xFFFFFF
#endif

/* EasyX: BGR(c) swaps the red and blue of a COLORREF, which is what a
 * pixel read out of the display buffer (0x00rrggbb) needs to become a
 * COLORREF (0x00bbggrr).  The top byte is carried through so a colour
 * built with ARGB() keeps its alpha - the plain three term version would
 * silently turn it opaque. */
#ifndef BGR
#define BGR(c)  ((((DWORD)(c) & 0xFF) << 16) | ((DWORD)(c) & 0x0000FF00) | \
                 (((DWORD)(c) >> 16) & 0xFF) | ((DWORD)(c) & 0xFF000000u))
#endif

/* Win32 ternary raster operations (used by putimage) */
#ifndef SRCCOPY
#define SRCCOPY      ((DWORD)0x00CC0020)
#define SRCPAINT     ((DWORD)0x00EE0086)
#define SRCAND       ((DWORD)0x008800C6)
#define SRCINVERT    ((DWORD)0x00660046)
#define SRCERASE     ((DWORD)0x00440328)
#define NOTSRCCOPY   ((DWORD)0x00330008)
#define NOTSRCERASE  ((DWORD)0x001100A6)
#define MERGECOPY    ((DWORD)0x00C000CA)
#define MERGEPAINT   ((DWORD)0x00BB0226)
#define PATCOPY      ((DWORD)0x00F00021)
#define PATPAINT     ((DWORD)0x00FB0A09)
#define PATINVERT    ((DWORD)0x005A0049)
#define DSTINVERT    ((DWORD)0x00550009)
#define BLACKNESS    ((DWORD)0x00000042)
#define WHITENESS    ((DWORD)0x00FF0062)
#endif

/* EasyX mouse / message filters */
#ifndef EM_MOUSE
#define EM_MOUSE  0x01
#define EM_KEY    0x02
#define EM_CHAR   0x04
#define EM_WINDOW 0x08
/* EasyX spells the message filters EX_*; same numbers, so both names work
 * as the filter argument of peekmessage() / getmessage() / flushmessage(). */
#define EX_MOUSE  EM_MOUSE
#define EX_KEY    EM_KEY
#define EX_CHAR   EM_CHAR
#define EX_WINDOW EM_WINDOW
#endif

/* initgraph() flags (EasyX compatible values) */
#ifndef INIT_DEFAULT
/* initgraph() flags.
 *
 * EasyX names and numbers these EX_SHOWCONSOLE=1, EX_NOCLOSE=2,
 * EX_NOMINIMIZE=4, EX_DBLCLKS=8, so those are the values accepted here -
 * previously bit 1 meant "no border" and bit 2 "with logo", which made
 * initgraph(640, 480, EX_SHOWCONSOLE) produce a borderless window instead
 * of a console.  The library specific extras moved above 0x100 so both
 * sets can be combined in one call. */
#define EX_SHOWCONSOLE    0x0001
#define EX_NOCLOSE        0x0002
#define EX_NOMINIMIZE     0x0004
#define EX_DBLCLKS        0x0008
/* graphics.h aliases for the same four values */
#define EW_SHOWCONSOLE    EX_SHOWCONSOLE
#define EW_NOCLOSE        EX_NOCLOSE
#define EW_NOMINIMIZE     EX_NOMINIMIZE
#define EW_DBLCLKS        EX_DBLCLKS
#define SHOWCONSOLE       EX_SHOWCONSOLE
#define NOCLOSE           EX_NOCLOSE
#define NOMINIMIZE        EX_NOMINIMIZE
#define DBLCLKS           EX_DBLCLKS
/* easygl extensions, above the EasyX range */
#define INIT_DEFAULT      0x0000
#define INIT_NOBORDER     0x0100
#define INIT_WITHLOGO     0x0200
#define INIT_HIDE         0x0400
#define INIT_MINIMIZE     0x0800
#define INIT_RENDERMANUAL 0x1000
#define INIT_NOFORCEEXIT  0x2000
/* BGI setwritemode() */
#define COPY_PUT          0
#define XOR_PUT           1
#endif

/* render modes (EasyX 2022) */
/*======================================================================
 *  2b. Alpha channel convention
 *====================================================================*/
/* EasyX's COLORREF is 0x00BBGGRR and never touches the top byte, so it is
 * promoted to an alpha channel: 0xAABBGGRR.
 *
 * The byte is a TRANSPARENCY:
 *
 *   0    = fully OPAQUE       ARGB(0, r, g, b), and every RGB() colour
 *   1..254 = that much see-through
 *   255  = fully TRANSPARENT  ARGB(255, r, g, b), i.e. invisible
 *
 * The old convention read 0 as "no alpha given" - which was also opaque,
 * but meant "fully transparent" could not be expressed at all.  That is
 * what changed: only the top of the range (255) behaves differently now.
 *
 * RGB() is left alone because it builds 0x00BBGGRR, and 0 is exactly
 * "fully opaque" under this convention - no redefinition, no compatibility
 * switch.
 *
 * GL's alpha means the opposite (coverage, 1 = solid), so gxAlphaOf()
 * inverts it.  That is the only place the two conventions meet.
 *
 * GetImageBuffer() gives 0x00BBGGRR, so a pixel read from it is opaque and
 * can be drawn straight back out. */
typedef DWORD ACOLORREF;

/* The plain three component form, with no alpha.  Used by the constructors
 * below so that ARGB() / RGBA() do not depend on how RGB() is spelled. */
#define GX_RGB3(r, g, b)                                                     \
    ((DWORD)(BYTE)(r) | ((DWORD)(BYTE)(g) << 8) | ((DWORD)(BYTE)(b) << 16))

/* RGB() is left completely alone.  It builds 0x00BBGGRR, whose alpha byte
 * is 0 - and under this convention 0 means FULLY OPAQUE.  So every existing
 * EasyX colour and every existing EasyX program is already correct, with no
 * redefinition and no compatibility switch at all. */
#ifndef ARGB
#define ARGB(a, r, g, b)                                                     \
    ((ACOLORREF)((((DWORD)(BYTE)(a)) << 24) | GX_RGB3((r), (g), (b))))
#endif
#ifndef RGBA
#define RGBA(r, g, b, a)                                                     \
    ((ACOLORREF)((((DWORD)(BYTE)(a)) << 24) | GX_RGB3((r), (g), (b))))
#endif
#ifndef GetAValue
#define GetAValue(c) ((BYTE)(((DWORD)(c)) >> 24))
#endif
/* The alpha byte is a TRANSPARENCY, so: opaque is 0, invisible is 255. */
#define ALPHA_OPAQUE      ((BYTE)0)
#define ALPHA_TRANSPARENT ((BYTE)255)

#ifndef RENDER_AUTO
#define RENDER_AUTO    0
#define RENDER_MANUAL  1
#endif

/* EasyX style / mode constants (also used by the renderer internals) */
#ifndef BS_SOLID
#define BS_SOLID 0
#define BS_NULL  1
#define BS_HATCHED 2
#define BS_PATTERN 3
#define BS_DIBPATTERN 5
#endif
#ifndef HS_HORIZONTAL
#define HS_HORIZONTAL 0
#define HS_VERTICAL   1
#define HS_FDIAGONAL  2
#define HS_BDIAGONAL  3
#define HS_CROSS      4
#define HS_DIAGCROSS  5
#endif
#ifndef PS_SOLID
#define PS_SOLID 0
#define PS_DASH 1
#define PS_DOT 2
#define PS_DASHDOT 3
#define PS_DASHDOTDOT 4
#define PS_NULL 5
#define PS_USERSTYLE 7
#endif
#ifndef ALTERNATE
#define ALTERNATE 1
#define WINDING 2
#endif
#ifndef TRANSPARENT
#define TRANSPARENT 1
#define OPAQUE 2
#endif
#ifndef R2_BLACK
#define R2_BLACK 1
#define R2_NOTMERGEPEN 2
#define R2_MASKNOTPEN 3
#define R2_NOTCOPYPEN 4
#define R2_MASKPENNOT 5
#define R2_NOT 6
#define R2_XORPEN 7
#define R2_NOTMASKPEN 8
#define R2_MASKPEN 9
#define R2_NOTXORPEN 10
#define R2_NOP 11
#define R2_MERGENOTPEN 12
#define R2_COPYPEN 13
#define R2_MERGEPENNOT 14
#define R2_MERGEPEN 15
#define R2_WHITE 16
#endif
#ifndef FLOODFILLBORDER
#define FLOODFILLBORDER 0
#define FLOODFILLSURFACE 1
#endif

/*======================================================================
 *  1. OpenGL entry points that are not in the ancient GL/gl.h
 *====================================================================*/
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER          0x8892
#endif
/* Every constant below gets its own guard, for the same reason as the
 * GL_VENDOR block further down: grouping them under a single #ifndef meant a
 * system gl.h declaring VBOs but not, say, GL_STATIC_DRAW lost the whole
 * block.  GL_STATIC_DRAW in particular is needed by gxmesh_setup(). */
#ifndef GL_DYNAMIC_DRAW
#define GL_DYNAMIC_DRAW          0x88E8
#endif
#ifndef GL_STATIC_DRAW
#define GL_STATIC_DRAW           0x88E4
#endif
#ifndef GL_STREAM_DRAW
#define GL_STREAM_DRAW           0x88E0
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER       0x8B30
#endif
#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER         0x8B31
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS        0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS           0x8B82
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0              0x84C0
#endif
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER           0x8D40
#endif
#ifndef GL_COLOR_ATTACHMENT0
#define GL_COLOR_ATTACHMENT0     0x8CE0
#endif
#ifndef GL_FRAMEBUFFER_COMPLETE
#define GL_FRAMEBUFFER_COMPLETE  0x8CD5
#endif
/* glGetString() selectors.  GL_VENDOR / GL_RENDERER / GL_VERSION come from
 * GL 1.0, GL_SHADING_LANGUAGE_VERSION from GL 2.0.
 * Each gets its own guard: the system GL/gl.h already defines the 1.0 ones,
 * so grouping them under a single #ifndef skipped the whole block and lost
 * GL_SHADING_LANGUAGE_VERSION, which that header does not have. */
#ifndef GL_VENDOR
#define GL_VENDOR                     0x1F00
#endif
#ifndef GL_RENDERER
#define GL_RENDERER                   0x1F01
#endif
#ifndef GL_VERSION
#define GL_VERSION                    0x1F02
#endif
#ifndef GL_EXTENSIONS
#define GL_EXTENSIONS                 0x1F03
#endif
#ifndef GL_SHADING_LANGUAGE_VERSION
#define GL_SHADING_LANGUAGE_VERSION   0x8B8C
#endif
#ifndef ENUM_CURRENT_SETTINGS
#define ENUM_CURRENT_SETTINGS ((DWORD)-1)
#endif
#ifndef GL_RENDERBUFFER
#define GL_RENDERBUFFER          0x8D41
#define GL_READ_FRAMEBUFFER      0x8CA8
#define GL_DRAW_FRAMEBUFFER      0x8CA9
#define GL_RGBA8                 0x8058
#define GL_MAX_SAMPLES           0x8D57
#endif
#ifndef GL_COLOR_LOGIC_OP
#define GL_COLOR_LOGIC_OP        0x0BF2
#endif
#ifndef GL_SCISSOR_TEST
#define GL_SCISSOR_TEST          0x0C11
#endif
#ifndef GL_PACK_ALIGNMENT
#define GL_PACK_ALIGNMENT        0x0D05
#endif
#ifndef GL_UNPACK_ALIGNMENT
#define GL_UNPACK_ALIGNMENT      0x0CF5
#endif
#ifndef GL_LOGIC_OP
#define GL_LOGIC_OP              GL_COLOR_LOGIC_OP
#endif
/* GL_TEXTURE1 carries the second image of miximagei(); every other draw
 * reads unit 0.  MinGW ships a GL 1.1 gl.h, which has GL_TEXTURE0 but not
 * this one (it arrived with GL 1.3), hence the fallback. */
#ifndef GL_TEXTURE1
#define GL_TEXTURE1              0x84C1
#endif
/* GL_UNSIGNED_INT is GL 1.1, GL_ELEMENT_ARRAY_BUFFER is 1.5 - MinGW ships a
 * 1.1 gl.h, so the latter (at least) is missing there. */
#ifndef GL_UNSIGNED_INT
#define GL_UNSIGNED_INT          0x1405
#endif
#ifndef GL_ELEMENT_ARRAY_BUFFER
#define GL_ELEMENT_ARRAY_BUFFER  0x8893
#endif


#define DECLGL(ret, name, ...) \
    typedef ret (APIENTRY *name##_PROC)(__VA_ARGS__); \
    static name##_PROC name = 0;
#define LOADGL(name) name = (name##_PROC)wglGetProcAddress(#name)

DECLGL(void, glGenBuffers, GLsizei, GLuint*)
DECLGL(void, glBindBuffer, GLenum, GLuint)
DECLGL(void, glBufferData, GLenum, GLsizei, const void*, GLenum)
/* GLintptr / GLsizeiptr arrived with GL 1.5, and MinGW ships a GL 1.1 gl.h
 * that has neither, so glBufferSubData() cannot be declared with them.
 *
 * They cannot simply be defined here either: a typedef is not a macro, so
 * #ifndef does not see it, and a real gl.h that does declare them would then
 * hit "conflicting types".  Hence the private names below.
 *
 * The ABIs are identical - wherever GL does define these they are ptrdiff_t
 * and size_t - so the prototype matches the real function exactly. */
typedef ptrdiff_t GXGLintptr;
typedef size_t    GXGLsizeiptr;
DECLGL(void, glBufferSubData, GLenum, GXGLintptr, GXGLsizeiptr, const void*)
DECLGL(void, glDeleteBuffers, GLsizei, const GLuint*)
DECLGL(GLuint, glCreateShader, GLenum)
DECLGL(void, glShaderSource, GLuint, GLsizei, const char* const*, const GLint*)
DECLGL(void, glCompileShader, GLuint)
DECLGL(void, glGetShaderiv, GLuint, GLenum, GLint*)
DECLGL(void, glGetShaderInfoLog, GLuint, GLsizei, GLsizei*, char*)
DECLGL(void, glDeleteShader, GLuint)
DECLGL(GLuint, glCreateProgram, void)
DECLGL(void, glAttachShader, GLuint, GLuint)
DECLGL(void, glBindAttribLocation, GLuint, GLuint, const char*)
DECLGL(void, glLinkProgram, GLuint)
DECLGL(void, glGetProgramiv, GLuint, GLenum, GLint*)
DECLGL(void, glGetProgramInfoLog, GLuint, GLsizei, GLsizei*, char*)
DECLGL(void, glUseProgram, GLuint)
DECLGL(void, glDeleteProgram, GLuint)
DECLGL(GLint, glGetUniformLocation, GLuint, const char*)
DECLGL(void, glUniform1i, GLint, GLint)
DECLGL(void, glUniform1f, GLint, GLfloat)
DECLGL(void, glUniform2f, GLint, GLfloat, GLfloat)
DECLGL(void, glUniform3f, GLint, GLfloat, GLfloat, GLfloat)
DECLGL(void, glUniform4f, GLint, GLfloat, GLfloat, GLfloat, GLfloat)
DECLGL(void, glUniformMatrix4fv, GLint, GLsizei, GLboolean, const GLfloat*)
DECLGL(void, glGenVertexArrays, GLsizei, GLuint*)
DECLGL(void, glBindVertexArray, GLuint)
DECLGL(void, glDeleteVertexArrays, GLsizei, const GLuint*)
DECLGL(void, glGetVertexAttribiv, GLuint, GLenum, GLint*)
DECLGL(void, glEnableVertexAttribArray, GLuint)
DECLGL(void, glVertexAttribPointer, GLuint, GLint, GLenum, GLboolean, GLsizei, const void*)
DECLGL(void, glActiveTexture, GLenum)
DECLGL(void, glGenFramebuffers, GLsizei, GLuint*)
DECLGL(void, glBindFramebuffer, GLenum, GLuint)
DECLGL(void, glFramebufferTexture2D, GLenum, GLenum, GLenum, GLuint, GLint)
DECLGL(void, glGenRenderbuffers, GLsizei, GLuint*)
DECLGL(void, glBindRenderbuffer, GLenum, GLuint)
DECLGL(void, glDeleteRenderbuffers, GLsizei, const GLuint*)
DECLGL(void, glRenderbufferStorage, GLenum, GLenum, GLsizei, GLsizei)
DECLGL(void, glRenderbufferStorageMultisample, GLenum, GLsizei, GLenum, GLsizei, GLsizei)
DECLGL(void, glFramebufferRenderbuffer, GLenum, GLenum, GLenum, GLuint)
DECLGL(void, glBlitFramebuffer, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLint, GLbitfield, GLenum)
DECLGL(GLenum, glCheckFramebufferStatus, GLenum)
DECLGL(void, glDeleteFramebuffers, GLsizei, const GLuint*)

/* glGetString() must NOT go through DECLGL.  The system GL/gl.h already
 * declares it statically (it is a GL 1.0 function and therefore exported by
 * opengl32.dll), so declaring a function pointer of the same name is
 * "redeclared as a different kind of symbol".  Own pointer, own name. */
typedef const unsigned char* (APIENTRY *PFNGLGETSTRING)(GLenum);
static PFNGLGETSTRING gxGetString = 0;

/* glBlendEquation() is GL 1.2+, so MinGW's GL 1.1 gl.h does not declare it
 * and calling it outright does not compile.  It gets its own pointer rather
 * than a DECLGL() entry, because some SDKs *do* declare it and then the
 * static pointer would clash with the real declaration - exactly the
 * glGetString() trap above.  Declared up here because gxLoadGL() loads it. */
typedef void (APIENTRY *PFNGLBLENDEQUATION)(GLenum);
static PFNGLBLENDEQUATION gxBlendEquation = 0;

typedef HGLRC (WINAPI *PFN_WGLCREATECTXATTRIBS)(HDC, HGLRC, const int*);
#define WGL_CONTEXT_MAJOR_VERSION_ARB            0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB            0x2092
#define WGL_CONTEXT_FLAGS_ARB                    0x2094
#define WGL_CONTEXT_PROFILE_MASK_ARB             0x9126
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002

static void gxLoadGL(void) {
    LOADGL(glGenBuffers);            LOADGL(glBindBuffer);
    LOADGL(glBufferData);            LOADGL(glDeleteBuffers);
    LOADGL(glBufferSubData);
    LOADGL(glCreateShader);          LOADGL(glShaderSource);
    LOADGL(glCompileShader);         LOADGL(glGetShaderiv);
    LOADGL(glGetShaderInfoLog);      LOADGL(glDeleteShader);
    LOADGL(glCreateProgram);         LOADGL(glAttachShader);
    LOADGL(glBindAttribLocation);    LOADGL(glLinkProgram);
    LOADGL(glGetProgramiv);          LOADGL(glGetProgramInfoLog);
    LOADGL(glUseProgram);            LOADGL(glDeleteProgram);
    LOADGL(glGetUniformLocation);    LOADGL(glUniform1i);           LOADGL(glUniform1f);
    LOADGL(glUniform2f);             LOADGL(glUniformMatrix4fv);
    LOADGL(glUniform4f);
    LOADGL(glUniform3f);
    LOADGL(glEnableVertexAttribArray); LOADGL(glVertexAttribPointer);
    LOADGL(glActiveTexture);         LOADGL(glGenFramebuffers);
    LOADGL(glBindFramebuffer);       LOADGL(glFramebufferTexture2D);
    LOADGL(glCheckFramebufferStatus); LOADGL(glDeleteFramebuffers);
    LOADGL(glGenVertexArrays);
    LOADGL(glBindVertexArray);
    LOADGL(glDeleteVertexArrays);
    LOADGL(glGetVertexAttribiv);
    LOADGL(glRenderbufferStorage);
    LOADGL(glGenRenderbuffers);      LOADGL(glBindRenderbuffer);
    LOADGL(glDeleteRenderbuffers);   LOADGL(glFramebufferRenderbuffer);
    /* Three spellings exist for these two: GL 3.0 / ARB_framebuffer_object
     * use the plain names, older drivers only export the EXT ones. */
    glRenderbufferStorageMultisample =
        (glRenderbufferStorageMultisample_PROC)
            wglGetProcAddress("glRenderbufferStorageMultisample");
    if (!glRenderbufferStorageMultisample)
        glRenderbufferStorageMultisample =
            (glRenderbufferStorageMultisample_PROC)
                wglGetProcAddress("glRenderbufferStorageMultisampleEXT");
    glBlitFramebuffer =
        (glBlitFramebuffer_PROC)wglGetProcAddress("glBlitFramebuffer");
    if (!glBlitFramebuffer)
        glBlitFramebuffer =
            (glBlitFramebuffer_PROC)wglGetProcAddress("glBlitFramebufferEXT");
    /* glGetString() is a GL 1.0 entry point, and wglGetProcAddress() only
     * covers 1.2 and up, so it legitimately returns NULL here.  The 1.0/1.1
     * core functions are exported by opengl32.dll itself. */
    gxGetString = (PFNGLGETSTRING)wglGetProcAddress("glGetString");
    /* GL 1.2+; wglGetProcAddress first, then the dll export, same as
     * glGetString above.  A driver without it simply loses GX_BLEND_SUB. */
    gxBlendEquation =
        (PFNGLBLENDEQUATION)wglGetProcAddress("glBlendEquation");
    if (!gxGetString) {
        HMODULE hGL = GetModuleHandleA("opengl32.dll");
        if (!hGL) hGL = LoadLibraryA("opengl32.dll");
        if (hGL)
            gxGetString = (PFNGLGETSTRING)GetProcAddress(hGL, "glGetString");
    }
}

/*======================================================================
 *  2. Minimal containers (replacing std::vector / std::map)
 *====================================================================*/
#define GX_DEFINE_ARRAY(NAME, TYPE)                                          \
typedef struct NAME { TYPE* data; size_t size; size_t cap; } NAME;           \
GX_INLINE void NAME##_clear(NAME* v) { v->size = 0; }                        \
GX_INLINE void NAME##_free(NAME* v) {                                        \
    free(v->data); v->data = NULL; v->size = 0; v->cap = 0;                   \
}                                                                             \
GX_INLINE void NAME##_reserve(NAME* v, size_t n) {                            \
    size_t c; TYPE* p;                                                        \
    if (n <= v->cap) return;                                                  \
    c = (v->cap == 0) ? 64 : v->cap;                                          \
    while (c < n) {                                                           \
        if (c > ((size_t)-1) / 2) { c = n; break; }                           \
        c *= 2;                                                               \
    }                                                                         \
    p = (TYPE*)realloc(v->data, c * sizeof(TYPE));                            \
    if (!p) {                                                                 \
        MessageBoxA(NULL, "Out of memory", "Error", MB_OK);                   \
        exit(1);                                                              \
    }                                                                         \
    v->data = p; v->cap = c;                                                  \
}                                                                             \
GX_INLINE TYPE* NAME##_push(NAME* v) {                                        \
    NAME##_reserve(v, v->size + 1);                                           \
    return &v->data[v->size++];                                               \
}                                                                             \
GX_INLINE void NAME##_pushv(NAME* v, TYPE val) { *NAME##_push(v) = val; }

GX_INLINE size_t gxHashU64(unsigned long long k) {
    unsigned long long h = k;
    h ^= h >> 33; h *= 0xff51afd7ed558ccdULL;
    h ^= h >> 33; h *= 0xc4ceb9fe1a85ec53ULL;
    h ^= h >> 33;
    return (size_t)(h & 0xffffffffULL) ^ (size_t)(h >> 32);
}

GX_INLINE size_t gxHashStr(const char* s) {
    size_t h = (size_t)0x811c9dc5u;
    while (*s) {
        h ^= (size_t)(unsigned char)*s++;
        h *= (size_t)16777619u;
    }
    return h;
}

/*======================================================================
 *  3. EasyX compatible public structures
 *====================================================================*/
typedef struct IMAGE {
    int    width, height;  /* pixels of the texture                      */
    int    logW, logH;     /* logical units the image stands for         */
    /* width/height is how many PIXELS the image has; logW/logH is how much
     * of the LOGICAL space it covers.  They are the same for an image made
     * by Resize() / loadimage() / newimage(), but not for one that came out
     * of getimage() on a scaled canvas: a 160 logical unit square holds
     * 240x240 pixels at 1.5x, and putimage(x, y, &img) has to draw 160
     * units, not 240, or the copy comes out one factor too big. */
    GLuint tex;          /* internal: color texture                      */
    GLuint fbo;          /* internal: framebuffer used when drawing in   */
    DWORD  flags;        /* internal: GXIMG_ALPHA when the image has alpha */
    DWORD  magic;        /* internal: guards against uninitialised IMAGEs  */
} IMAGE;
#define GXIMG_MAGIC 0x47584D31u   /* "GXM1" */

GX_INLINE bool gxImageOk(const IMAGE* img) {
    return (img != NULL) && (img->magic == GXIMG_MAGIC) && (img->tex != 0);
}

typedef struct LINESTYLE {
    DWORD  style;
    DWORD  thickness;
    DWORD* puserstyle;
    DWORD  userstylecount;
} LINESTYLE;

typedef struct FILLSTYLE {
    int    style;
    long   hatch;
    IMAGE* ppattern;
} FILLSTYLE;

typedef struct MOUSEMSG {
    UINT uMsg;
    bool mkCtrl;
    bool mkShift;
    bool mkLButton;
    bool mkMButton;
    bool mkRButton;
    int  x;
    int  y;
    int  wheel;
} MOUSEMSG;

typedef struct ExMessage {
    USHORT message;
    union {
        struct {
            int  x;
            int  y;
            int  wheel;
            bool ctrl;
            bool shift;
            bool lbutton;
            bool mbutton;
            bool rbutton;
        };
        struct {
            BYTE vkcode;
            BYTE scancode;
            bool extended;
            bool prevdown;
        };
        struct {
            WCHAR ch;
        };
    };
} ExMessage;

/* Internal: baked glyph metrics (all values are DEVICE pixels) */
typedef struct Glyph {
    float u0, v0, u1, v1;
    float w, h;
    float offX, offY;
    float adv;
} Glyph;

typedef struct FontRec {
    HFONT hfont;
    char  face[LF_FACESIZE];
    int   pxH;
    int   weight;
    bool  italic;
    int   mode;      /* GLF_UNLIMITED or GLF_INT, baked in at creation */
} FontRec;

/* Internal: batched vertex / draw command */
typedef struct Vtx   { float x, y, r, g, b, a, u, v; } Vtx;
/* "quads" is 1 when the whole range is a run of 4 vertex quads, which can be
 * drawn with an index buffer (6 indices each) instead of 6 vertices each.
 * 0 means an ordinary triangle list - the fans and strips of the polygon,
 * circle and gradient code, where the vertices are not independent quads. */
typedef struct GLCmd { size_t first, count; GLuint tex; int useTex; int rop;
                       float psx, psy; float pox, poy; int tile;
                       float alpha; int blend; int filter; int quads;
                       /* alphagradpicture() */
                       int vertA;
                       /* miximagec() / miximagei() */
                       int mixMode; float mixW; GLuint tex2;
                       float mixR, mixG, mixB, mixA; } GLCmd;

GX_DEFINE_ARRAY(GxVtxVec, Vtx)
GX_DEFINE_ARRAY(GxCmdVec, GLCmd)
GX_DEFINE_ARRAY(GxFontVec, FontRec)

/*======================================================================
 *  4. Global state
 *====================================================================*/
typedef struct GxTarget { GLuint fbo; GLuint tex; int w, h; } GxTarget;

static HWND   g_gx_hwnd  = NULL;
static HDC    g_gx_hdc   = NULL;
static HGLRC  g_gx_hglrc = NULL;
static int    g_gx_devW = 0, g_gx_devH = 0;      /* current render target size  */
static float  g_gx_logW = 0, g_gx_logH = 0;      /* logical size, EasyX space   */
static float  g_gx_scaleX = 1.f, g_gx_scaleY = 1.f;
/* fixhighdpi(): the factor setaspectratio() is multiplied by, and the
 * factors as the caller passed them.  g_gx_scaleX / g_gx_scaleY hold the
 * effective value (requested * g_gx_dpiFix), which is what everything else in
 * the library reads, so the requested pair has to be kept separately to be
 * able to recompute when the factor changes. */
static float  g_gx_dpiFix = 1.f;              /* 1 = no scaling applied      */
/* The window size the caller asked for, in LOGICAL units - what initgraph()
 * and setwinsize() were given.  The device size is this times g_gx_dpiFix:
 *
 *     g_gx_devW = g_gx_baseW * g_gx_dpiFix
 *
 * Kept because a rebuilt window comes back at the logical size and has to be
 * scaled up again, and scaling is not reversible from the device size alone -
 * once g_gx_devW is 1200 there is nothing left to say whether that was 800 at
 * 150% or 1200 at 100%, so the second rebuild would scale it again. */
static int   g_gx_baseW = 0, g_gx_baseH = 0;
/* variablewinsize(): the window carries WS_THICKFRAME | WS_MAXIMIZEBOX
 * so the user can drag its border and hit the maximise button.  Read by
 * initgraph() when the window is created, so calling the setter before
 * initgraph() needs no extra work, and calling it afterwards rebuilds
 * the frame of the live window instead. */
static bool  g_gx_varWinSize = false;

/* Whether fixhighdpi(true) is in force.  Held separately from g_gx_dpiFix
 * because at 100% the factor is legitimately 1.f, which would be
 * indistinguishable from "off"; gxApplyWindowState() needs to know which
 * one it is.  Default off, so an existing program is unaffected. */
static bool   g_gx_dpiFixOn = false;
static float  g_gx_reqScaleX = 1.f, g_gx_reqScaleY = 1.f;
static float  g_gx_originX = 0.f, g_gx_originY = 0.f;  /* physical pixel offset */
/* The canvas transform, parked while an IMAGE is the working target.
 *
 * An IMAGE has a fixed pixel size, so it is drawn into at 1:1 - EasyX
 * behaves the same way and setaspectratio() / setorigin() only move the
 * window canvas.  The scale and origin the caller set belong to the canvas,
 * so they are kept here and put back when the canvas is selected again.
 *
 * Without this, a live scale would also apply inside the IMAGE: filling a
 * 160 x 160 image at 1.5x draws 240 x 240 into a 160 x 160 surface, and the
 * part that does not fit is cut off - the image ends up showing only its
 * top left corner.  fixhighdpi() is what makes that reachable in practice. */
static float  g_gx_canvasScaleX = 1.f, g_gx_canvasScaleY = 1.f;
static float  g_gx_canvasOriginX = 0.f, g_gx_canvasOriginY = 0.f;
/* The origin as setorigin() was given it, kept for the same reason as
 * g_gx_reqScaleX / Y: the effective value is the request times the DPI
 * factor, and fixhighdpi() has to be able to re-derive it. */
static float  g_gx_reqOriginX = 0.f, g_gx_reqOriginY = 0.f;
static float  g_gx_proj[16];

/* EasyX keeps a global "hwnd" that the host program declares and that
 * initgraph() fills in.  It cannot just be assigned here: the host declares
 * it AFTER including this header, so at the point initgraph() is compiled
 * the name does not exist yet.  Declare it, and the host's own
 * "HWND hwnd;" later becomes the definition that matches - that is legal
 * C and C++ - but only when the host ASKS for it, because a host that never
 * defines hwnd would otherwise fail to link.
 *
 * Define GX_EASYX_HWND before including this header and declare
 * "HWND hwnd;" at file scope, and initgraph() / closegraph() keep it in
 * step with the window.  Without it, use GetHWnd(), which always works. */
#ifdef GX_EASYX_HWND
extern HWND hwnd;
#endif

static bool  g_gx_glReady = false;
static bool  g_gx_userClosed = false;
static bool  g_gx_forceExit = true;          /* false with INIT_NOFORCEEXIT */
static int   g_gx_renderMode = RENDER_AUTO; /* EasyX default: draw == show   */
static bool  g_gx_presentPending = false;   /* gxAutoPresent() owes a frame    */
static bool  g_gx_batchDraw = false;        /* inside BeginBatchDraw() ... */
static int   g_gx_initFlag = INIT_DEFAULT;

static GxTarget g_gx_canvasTarget;           /* window sized off screen canvas */
static GxTarget g_gx_workTarget;             /* the IMAGE selected by SetWorkingImage */
static GxTarget* g_gx_target = &g_gx_canvasTarget;
static IMAGE*    g_gx_workImg = NULL;
/* EasyX style state */
static LINESTYLE g_gx_lineStyle;
static FILLSTYLE g_gx_fillStyle;
static COLORREF  g_gx_fillColor = WHITE;
static COLORREF  g_gx_lineColor = WHITE;
static COLORREF  g_gx_textColor = WHITE;
static int       g_gx_lineWidth = 1;   /* LOGICAL units (GDI pen semantics) */
static COLORREF  g_gx_bkColor   = BLACK;
static int       g_gx_bkMode    = TRANSPARENT;
static int       g_gx_rop2      = R2_COPYPEN;
static int       g_gx_polyMode  = ALTERNATE;
/* logical "current position", in double because the primitives below take
 * floating point coordinates now.  getx() / gety() still report an int,
 * because that is what EasyX returns. */
static double    g_gx_curX = 0, g_gx_curY = 0;
static bool      g_gx_clipOn = false;
static RECT      g_gx_clipRect;

/* text size multiplier (kept from the original easygl.h) */
static float g_gx_textScale = 1.f;

/*--------------------------- message queue ----------------------------*/
#define GX_MSGQ_CAP 256
static ExMessage g_gx_msgq[GX_MSGQ_CAP];
static int g_gx_msgHead = 0, g_gx_msgTail = 0, g_gx_msgCount = 0;

static void gxMsgInit(void) { g_gx_msgHead = g_gx_msgTail = g_gx_msgCount = 0; }

static void gxMsgPush(const ExMessage* m) {
    if (!m) return;
    g_gx_msgq[g_gx_msgTail] = *m;
    g_gx_msgTail = (g_gx_msgTail + 1) % GX_MSGQ_CAP;
    if (g_gx_msgCount == GX_MSGQ_CAP) g_gx_msgHead = (g_gx_msgHead + 1) % GX_MSGQ_CAP;
    else g_gx_msgCount++;
}

static ExMessage* gxMsgAt(int i) {
    return &g_gx_msgq[(g_gx_msgHead + i) % GX_MSGQ_CAP];
}

static void gxMsgPop(void) {
    if (g_gx_msgCount == 0) return;
    g_gx_msgHead = (g_gx_msgHead + 1) % GX_MSGQ_CAP;
    g_gx_msgCount--;
}

static bool gxMsgIsType(UINT m, BYTE filter) {
    BYTE f = 0;
    switch (m) {
    case WM_MOUSEMOVE: case WM_MOUSEWHEEL:
    case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
    case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
    case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
        f = EM_MOUSE; break;
    case WM_KEYDOWN: case WM_KEYUP: case WM_SYSKEYDOWN: case WM_SYSKEYUP:
        f = EM_KEY; break;
    case WM_CHAR: case WM_SYSCHAR: case WM_UNICHAR:
        f = EM_CHAR; break;
    case WM_SETFOCUS: case WM_KILLFOCUS: case WM_CLOSE: case WM_MOVE: case WM_SIZE:
        f = EM_WINDOW; break;
    default: f = 0; break;
    }
    return ((filter == 0xFF) || ((filter & f) != 0));
}

/*------------------------------------------------------------------*/
static void gxOrtho(float m[16], float l, float r, float b, float t) {
    float w = r - l, h = t - b;
    memset(m, 0, sizeof(float) * 16);
    /* A zero sized target (setorigin() before initgraph(), or a 0x0 IMAGE)
     * would divide by zero and poison the matrix with inf / NaN. */
    m[0]  = (w != 0.f) ? (2.f / w) : 0.f;
    m[12] = (w != 0.f) ? (-(r + l) / w) : 0.f;
    m[5]  = (h != 0.f) ? (2.f / h) : 0.f;
    m[13] = (h != 0.f) ? (-(t + b) / h) : 0.f;
    m[10] = -1.f;              m[14] = 0.f;
    m[15] = 1.f;
}

static void gxUpdateProj(void) {
    float ox, oy;
    g_gx_logW = g_gx_devW / g_gx_scaleX;
    g_gx_logH = g_gx_devH / g_gx_scaleY;
    ox = (g_gx_scaleX > 0.f) ? (g_gx_originX / g_gx_scaleX) : 0.f;
    oy = (g_gx_scaleY > 0.f) ? (g_gx_originY / g_gx_scaleY) : 0.f;
    gxOrtho(g_gx_proj, -ox, g_gx_logW - ox, g_gx_logH - oy, -oy);
}

/* Re-select the active render target.  Has to run whenever an IMAGE is
 * resized or re-created while it is the working image, otherwise the cached
 * FBO / width / height keep pointing at a deleted framebuffer. */
static void gxSyncWorkTarget(void) {
    if (g_gx_workImg && gxImageOk(g_gx_workImg)) {
        g_gx_workTarget.fbo = g_gx_workImg->fbo;
        g_gx_workTarget.tex = g_gx_workImg->tex;
        g_gx_workTarget.w   = g_gx_workImg->width;
        g_gx_workTarget.h   = g_gx_workImg->height;
        g_gx_target = &g_gx_workTarget;
        /* The IMAGE is 1:1: its pixels ARE the coordinate space.  The
         * canvas transform stays parked in g_gx_canvasScaleX / ... and is put
         * back below when the canvas is selected again. */
        g_gx_scaleX = g_gx_scaleY = 1.f;
        g_gx_originX = g_gx_originY = 0.f;
    } else {
        g_gx_workImg = NULL;
        g_gx_target  = &g_gx_canvasTarget;
        g_gx_scaleX  = g_gx_canvasScaleX;
        g_gx_scaleY  = g_gx_canvasScaleY;
        g_gx_originX = g_gx_canvasOriginX;
        g_gx_originY = g_gx_canvasOriginY;
    }
    g_gx_devW = g_gx_target->w;
    g_gx_devH = g_gx_target->h;
}

static void gxPresent(void);             /* defined in section 17 */
static void gxFlush(void);               /* defined in section 6  */
static void gxApplyWindowState(void);    /* defined in section 17 */
static void gxFpsTick(void);             /* defined in section 17a */
static void gxRestoreFilter(void);       /* defined in section 16c */
static void gxRestoreDpiFix(void);       /* defined in section 10c */

/*======================================================================
 *  5. Window procedure and message pump
 *====================================================================*/
/* device = logical * scale + origin, so logical = (device - origin) / scale.
 * floorf(), not a plain cast: a cast truncates towards zero and would map
 * -0.5 to 0, i.e. it is not the inverse of the projection for negative
 * coordinates. */
GX_INLINE int gxDevToLogX(int x) {
    float s = (g_gx_scaleX > 0.f) ? g_gx_scaleX : 1.f;
    return (int)floorf(((float)x - g_gx_originX) / s);
}
GX_INLINE int gxDevToLogY(int y) {
    float s = (g_gx_scaleY > 0.f) ? g_gx_scaleY : 1.f;
    return (int)floorf(((float)y - g_gx_originY) / s);
}

static void gxFillMouse(ExMessage* m, UINT msg, LPARAM l, int wheel) {
    memset(m, 0, sizeof(*m));
    m->message = (USHORT)msg;
    m->x = gxDevToLogX((short)LOWORD(l));
    m->y = gxDevToLogY((short)HIWORD(l));
    m->wheel = wheel;
    m->ctrl   = (GetKeyState(VK_CONTROL) < 0);
    m->shift  = (GetKeyState(VK_SHIFT) < 0);
    m->lbutton = (GetKeyState(VK_LBUTTON) < 0);
    m->mbutton = (GetKeyState(VK_MBUTTON) < 0);
    m->rbutton = (GetKeyState(VK_RBUTTON) < 0);
}

/* WM_SIZE has to rebuild the canvas, but gxResizeCanvas() is defined far
 * below, next to the rest of the resize code.  Declared here so the call
 * in gxWndProc() is not an implicit one - an implicit declaration is
 * typed int(void), which then clashes with the real static definition
 * ("static declaration follows non-static declaration"). */
static void gxResizeCanvas(int w, int h);

static LRESULT CALLBACK gxWndProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    ExMessage em;
    switch (m) {
    case WM_CLOSE:
        if (g_gx_forceExit) {
            g_gx_userClosed = true;
            DestroyWindow(h);
        } else {
            memset(&em, 0, sizeof(em));
            em.message = (USHORT)WM_CLOSE;
            gxMsgPush(&em);
        }
        return 0;
    case WM_DESTROY:
        /* No PostQuitMessage(): WM_QUIT is dropped by gxPump() so that
         * closegraph() -> initgraph() can rebuild the window. */
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(h, &ps);
        EndPaint(h, &ps);
        /* Dragging the caption or a border puts DefWindowProc into its own
         * modal loop, which suspends the program's loop entirely (see the
         * note above gxPump()).  Nothing presents during that time, so a
         * window that gets invalidated would show garbage.  Re-present the
         * last frame here so it stays correct - the picture is frozen
         * because the program itself is not running, which no library can
         * change on a single threaded window. */
        if (g_gx_glReady && !g_gx_batchDraw && g_gx_renderMode == RENDER_AUTO)
            gxPresent();
        return 0;
    }
    case WM_MOUSEMOVE: case WM_LBUTTONDOWN: case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK: case WM_RBUTTONDOWN: case WM_RBUTTONUP:
    case WM_RBUTTONDBLCLK: case WM_MBUTTONDOWN: case WM_MBUTTONUP:
    case WM_MBUTTONDBLCLK:
        gxFillMouse(&em, m, l, 0);
        gxMsgPush(&em);
        return 0;
    case WM_MOUSEWHEEL: {
        POINT pt;
        pt.x = (short)LOWORD(l); pt.y = (short)HIWORD(l);
        ScreenToClient(h, &pt);
        gxFillMouse(&em, WM_MOUSEWHEEL,
                    MAKELONG((WORD)pt.x, (WORD)pt.y),
                    (int)(short)HIWORD(w));
        gxMsgPush(&em);
        return 0;
    }
    case WM_KEYDOWN: case WM_KEYUP: case WM_SYSKEYDOWN: case WM_SYSKEYUP: {
        memset(&em, 0, sizeof(em));
        em.message = (USHORT)m;
        em.vkcode = (BYTE)w;
        em.scancode = (BYTE)((l >> 16) & 0xFF);
        em.extended = ((l & 0x01000000L) != 0);
        em.prevdown = ((l & 0x40000000L) != 0);
        em.ctrl = (GetKeyState(VK_CONTROL) < 0);
        em.shift = (GetKeyState(VK_SHIFT) < 0);
        gxMsgPush(&em);
        return 0;
    }
    case WM_CHAR: case WM_SYSCHAR: {
        memset(&em, 0, sizeof(em));
        em.message = (USHORT)WM_CHAR;
        em.ch = (WCHAR)w;
        em.ctrl = (GetKeyState(VK_CONTROL) < 0);
        em.shift = (GetKeyState(VK_SHIFT) < 0);
        gxMsgPush(&em);
        return 0;
    }
    case WM_SETFOCUS: case WM_KILLFOCUS: {
        memset(&em, 0, sizeof(em));
        em.message = (USHORT)m;
        gxMsgPush(&em);
        return 0;
    }
    case WM_SIZE: {
        /* Reachable in two ways: the user dragging a border (only when
         * variablewinsize(true) gave the window one), and setwinsize() /
         * fixhighdpi() moving it programmatically.  Either way the window
         * has already changed, so the canvas has to follow it.
         *
         * SIZE_MINIMIZED reports a 0x0 client area - shrinking the canvas
         * to that would throw the picture away and then fail to allocate,
         * so it is skipped and the old size is kept until the window is
         * restored, at which point WM_SIZE arrives again. */
        if (g_gx_glReady && g_gx_hwnd && w != SIZE_MINIMIZED) {
            int nw = (int)LOWORD(l), nh = (int)HIWORD(l);
            if (nw >= 1 && nh >= 1) {
                gxResizeCanvas(nw, nh);
                /* Keep the logical size in step: it is what a rebuilt
                 * window starts from, and what setwinsize() left behind is
                 * now stale. */
                g_gx_baseW = (int)((float)nw / g_gx_dpiFix + 0.5f);
                g_gx_baseH = (int)((float)nh / g_gx_dpiFix + 0.5f);
            }
        }
        memset(&em, 0, sizeof(em));
        em.message = (USHORT)WM_SIZE;
        /* Nothing else is filled in.  The size deliberately does NOT ride
         * in x / y: ExMessage has no width / height field, and stuffing an
         * unrelated field would make the struct mean something different
         * here than it does everywhere else.  Read the new size with
         * getwidth() / getheight() - they report the LOGICAL extent, which
         * is the number a resize handler wants, and when the resize was
         * skipped (minimised) they report the size that was kept. */
        gxMsgPush(&em);
        return 0;
    }
    }
    return DefWindowProcA(h, m, w, l);
}

static void gxPump(void) {
    MSG msg;
    /* Show the frame gxAutoPresent() still owes.  Every caller is about to
     * wait for input, and a throttled present would otherwise stay
     * invisible until the next event arrives. */
    if (g_gx_presentPending && g_gx_renderMode == RENDER_AUTO &&
        !g_gx_batchDraw && g_gx_glReady) {
        g_gx_presentPending = false;
        gxFlush();
        gxPresent();
    }
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) continue;
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    if (g_gx_userClosed) exit(0);   /* only the X button (or ESC) exits */
}

/*======================================================================
 *  5b. Console window
 *====================================================================*/
/* The console window of this process, or NULL for a /SUBSYSTEM:WINDOWS
 * program (one linked with -mwindows).  GetConsoleWindow() is resolved
 * dynamically because an old SDK hides it while _WIN32_WINNT is left at
 * its 0x0400 default. */
static HWND gxConsoleWindow(void) {
    HMODULE hKernel;
    typedef HWND (WINAPI *PFN_GCW)(void);
    PFN_GCW pGetConsoleWindow;
    hKernel = GetModuleHandleA("kernel32.dll");
    if (!hKernel) return NULL;
    pGetConsoleWindow = (PFN_GCW)GetProcAddress(hKernel, "GetConsoleWindow");
    if (!pGetConsoleWindow) return NULL;
    return pGetConsoleWindow();
}

/* True when there is a console window belonging to this process.  A console
 * whose stdout is redirected to a file or a pipe still has one, and it is
 * still the black window EasyX hides, so redirection is not tested here. */
static bool gxHasConsole(void) { return gxConsoleWindow() != NULL; }

static bool g_gx_consoleHidden = false;

/* Hide / show the console.  This deliberately hides the window instead of
 * detaching the process from it: detaching would break printf(), output to
 * stderr and conio's getch() / kbhit(), which a lot of EasyX programs use
 * for debugging and for the closing "press any key". */
static void gxConsoleShow(bool show) {
    HWND cw = gxConsoleWindow();
    if (!cw) return;
    ShowWindow(cw, show ? SW_SHOW : SW_HIDE);
    g_gx_consoleHidden = !show;
}

/* EasyX has no counterpart; these are easygl extensions so a program can
 * override what initgraph() decided, e.g. to read printf() output. */
static void showconsole(void) { gxConsoleShow(true); }
static void hideconsole(void) { gxConsoleShow(false); }

/*======================================================================
 *  5c. Keyboard through the GRAPHICS window
 *
 * conio's getch() / kbhit() read the CONSOLE.  That works right up to the
 * moment initgraph() hides the console - which this library does by
 * default - because a hidden console window cannot take the keyboard
 * focus: getch() then blocks forever, the main thread stops pumping the
 * graphics window and Windows reports it as "not responding".
 *
 * These two read the queued WM_CHAR messages instead, so they work with
 * the console hidden and keep the window alive (gxPump() runs inside).
 * Note the semantics are those of WM_CHAR: printable characters plus a
 * few control keys (\r, \b, \t, ESC).  Arrow / function keys arrive as
 * WM_KEYDOWN and produce no WM_CHAR, so they are not returned here - use
 * getmessage() / peekmessage() with the vkcode field for those.
 *====================================================================*/
static int gx_kbhit(void) {
    int i;
    gxPump();
    for (i = 0; i < g_gx_msgCount; i++) {
        ExMessage* e = gxMsgAt(i);
        if (e->message == WM_CHAR) return 1;
    }
    return 0;
}

static int gx_getch(void) {
    for (;;) {
        int i;
        gxPump();
        for (i = 0; i < g_gx_msgCount; i++) {
            ExMessage* e = gxMsgAt(i);
            if (e->message == WM_CHAR) {
                int res = (int)e->ch;
                /* Drop everything up to and including this key, so a
                 * WM_CHAR that was already waiting is not read twice. */
                while (g_gx_msgCount > 0) {
                    ExMessage* h2 = gxMsgAt(0);
                    bool same = (h2 == e);
                    gxMsgPop();
                    if (same) break;
                }
                return res;
            }
        }
        Sleep(1);
    }
}

/* Porting aid: define GX_GETCH_FROM_WINDOW before including this header and
 * plain getch() / kbhit() resolve to the two functions above, so EasyX code
 * that read the keyboard through conio starts working with the console
 * hidden.  Off by default because it changes the meaning of two names that
 * <conio.h> also declares. */
#ifdef GX_GETCH_FROM_WINDOW
#define getch  gx_getch
#define kbhit  gx_kbhit
#endif

/*======================================================================
 *  6. Shaders, program, batch renderer
 *====================================================================*/
static GLuint g_gx_prog = 0, g_gx_vbo = 0, g_gx_blitVbo = 0;
/* Index buffer for quad runs, plus the pattern it is filled with.
 * A quad is 4 vertices and 6 indices; the pattern repeats for every quad in
 * the buffer, so it only depends on the absolute vertex index and can be
 * built once and grown on demand:
 *     quad j (vertices 4j..4j+3) -> 4j, 4j+1, 4j+2,  4j, 4j+2, 4j+3
 * which is exactly the two triangles the old 6 vertex version emitted.
 * GL_UNSIGNED_INT, not _SHORT: a single frame may pass 65536 vertices. */
static GLuint        g_gx_ibo = 0;
static unsigned int* g_gx_idx = NULL;
static size_t        g_gx_idxQuads = 0;   /* how many quads g_gx_idx holds    */
static size_t        g_gx_iboQuads = 0;   /* how many the GL buffer can hold */
static size_t        g_gx_iboUpTo  = 0;   /* how many of those are uploaded   */
/* True while the batch being built is a run of quads.  Mixing the two in one
 * command is impossible - the index pattern would walk off the end of a fan -
 * so gxV() and gxQuad*() each end the batch before switching. */
static bool g_gx_quadRun = false;
/* How many Vtx the vertex buffer was last allocated for.  glBufferData()
 * re-allocates the store every call, and the driver may either really
 * reallocate or stall waiting for the GPU to finish with the old one.
 * Keeping the capacity lets gxFlush() use glBufferSubData() instead, which
 * only copies into storage that is already there - the common path (the
 * buffer is reused frame after frame) makes no allocation at all. */
static size_t g_gx_vboCap = 0;
static GLuint g_gx_fbo = 0, g_gx_canvasTex = 0, g_gx_atlasTex = 0;
static GLuint g_gx_hatchTex[6];
static GLint  g_gx_uProj = -1, g_gx_uUseTex = -1, g_gx_uPatScale = -1, g_gx_uNoBlend = -1;
static GLint  g_gx_uPatOff = -1, g_gx_uAlpha = -1;
/* The corner opacity uniform, see GX_FS. */
static GLint  g_gx_uVertA = -1;
/* miximagec() / miximagei().  uTex2 is a SAMPLER, so it holds a texture
 * unit number (1) rather than a texture; it is set once at link time. */
static GLint  g_gx_uTex2 = -1, g_gx_uMixMode = -1, g_gx_uMixW = -1, g_gx_uMixColor = -1;

/* A full screen pass the caller compiles themselves, see setpostshader().
 * 0 means "none": gxPresent() then draws with g_gx_prog exactly as it always
 * did, so the feature costs nothing until it is used. */
static GLuint g_gx_postProg = 0;
static GLint  g_gx_postProj = -1, g_gx_postTex = -1, g_gx_postTexel = -1, g_gx_postTime = -1;

static const char* GX_VS =
    "#version 120\n"
    "attribute vec2 aPos;\n"
    "attribute vec4 aColor;\n"
    "attribute vec2 aUV;\n"
    "uniform mat4 uProj;\n"
    "varying vec4 vColor;\n"
    "varying vec2 vUV;\n"
    "varying vec2 vPos;\n"
    "void main(){ vColor=aColor; vUV=aUV; vPos=aPos; gl_Position=uProj*vec4(aPos,0.0,1.0); }\n";

/* uVertA is what alphagradpicture() needs.  It
 * defaults to "off", so the extra work in the shader is one compare per
 * fragment for every other draw - and both calls are done on the GPU.
 *
 *   uVertA   1   c.a *= vColor.a                   four corner opacities
 *
 * vColor is a varying, so the four corner alphas of the quad are
 * interpolated across the face by the rasteriser for free.  That is exactly
 * a linear alpha ramp - "opaque at the top, gone at the bottom" costs one
 * quad, not a loop over scanlines.
 *
 * It is gated on uUseTex != 0 on purpose when the source is a texture:
 * without one c IS vColor, so multiplying by vColor.a again would square
 * the alpha and a solid shape would fade far too fast.  For the flat
 * colour version the quad is drawn with uUseTex 0 and the gate is skipped,
 * because there the vertex alpha is the only alpha there is. */
static const char* GX_FS =
    "#version 120\n"
    "uniform sampler2D uTex;\n"
    "uniform int uUseTex;\n"
    "uniform vec2 uPatScale;\n"
    "uniform vec2 uPatOff;\n"
    "uniform int uNoBlend;\n"
    "uniform float uAlpha;\n"
    "uniform int uVertA;\n"
    "uniform sampler2D uTex2;\n"
    "uniform int uMixMode;\n"
    "uniform float uMixW;\n"
    "uniform vec4 uMixColor;\n"
    "varying vec4 vColor;\n"
    "varying vec2 vUV;\n"
    "varying vec2 vPos;\n"
    "void main(){\n"
    "  vec4 c = vColor;\n"
    "  if(uUseTex==1) c.a *= texture2D(uTex,vUV).a;\n"
    "  else if(uUseTex==2) c = texture2D(uTex,vUV);\n"
    "  else if(uUseTex==3) c.a *= texture2D(uTex,vPos*uPatScale+uPatOff).a;\n"
    "  else if(uUseTex==4) c = vec4(1.0-texture2D(uTex,vUV).rgb, texture2D(uTex,vUV).a);\n"
    "  if(uMixMode!=0){\n"
    "    vec4 d = (uMixMode==1) ? uMixColor : texture2D(uTex2,vUV);\n"
    "    vec4 sp = vec4(c.rgb*c.a, c.a);\n"
    "    vec4 dp = vec4(d.rgb*d.a, d.a);\n"
    "    vec4 mp = mix(sp, dp, uMixW);\n"
    "    c = (mp.a>0.001) ? vec4(mp.rgb/mp.a, mp.a) : vec4(0.0,0.0,0.0,0.0);\n"
    "  }\n"
    "  c.a *= uAlpha;\n"
    "  if(uVertA==1 && uUseTex!=0) c.a *= vColor.a;\n"
    "  if(uNoBlend==1){ if(c.a<0.5) discard; c.a=1.0; }\n"
    "  gl_FragColor = c;\n"
    "}\n";

static GLuint gxCompile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    GLint ok = 0;
    char buf[512];
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        memset(buf, 0, sizeof(buf));
        glGetShaderInfoLog(s, 511, NULL, buf);
        MessageBoxA(NULL, buf, "Shader Error", MB_OK);
    }
    return s;
}

static void gxCreateProgram(void) {
    GLuint vs, fs;
    GLint ok = 0;
    char buf[512];
    vs = gxCompile(GL_VERTEX_SHADER, GX_VS);
    fs = gxCompile(GL_FRAGMENT_SHADER, GX_FS);
    g_gx_prog = glCreateProgram();
    glAttachShader(g_gx_prog, vs);
    glAttachShader(g_gx_prog, fs);
    glBindAttribLocation(g_gx_prog, 0, "aPos");
    glBindAttribLocation(g_gx_prog, 1, "aColor");
    glBindAttribLocation(g_gx_prog, 2, "aUV");
    glLinkProgram(g_gx_prog);
    glGetProgramiv(g_gx_prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        memset(buf, 0, sizeof(buf));
        glGetProgramInfoLog(g_gx_prog, 511, NULL, buf);
        MessageBoxA(NULL, buf, "Program Error", MB_OK);
    }
    glDeleteShader(vs);
    glDeleteShader(fs);
    glUseProgram(g_gx_prog);
    g_gx_uProj = glGetUniformLocation(g_gx_prog, "uProj");
    g_gx_uUseTex = glGetUniformLocation(g_gx_prog, "uUseTex");
    g_gx_uPatScale = glGetUniformLocation(g_gx_prog, "uPatScale");
    g_gx_uNoBlend  = glGetUniformLocation(g_gx_prog, "uNoBlend");
    g_gx_uPatOff   = glGetUniformLocation(g_gx_prog, "uPatOff");
    g_gx_uAlpha    = glGetUniformLocation(g_gx_prog, "uAlpha");
    g_gx_uVertA    = glGetUniformLocation(g_gx_prog, "uVertA");
    g_gx_uTex2     = glGetUniformLocation(g_gx_prog, "uTex2");
    g_gx_uMixMode  = glGetUniformLocation(g_gx_prog, "uMixMode");
    g_gx_uMixW     = glGetUniformLocation(g_gx_prog, "uMixW");
    g_gx_uMixColor = glGetUniformLocation(g_gx_prog, "uMixColor");
    glUniform1i(glGetUniformLocation(g_gx_prog, "uTex"), 0);
    /* Without this uTex2 defaults to unit 0, so miximagei() would sample
     * the FIRST image twice and the cross fade would do nothing at all. */
    glUniform1i(g_gx_uTex2, 1);
    glUniform1i(g_gx_uVertA, 0);
    glUniform2f(g_gx_uPatScale, 1.f / 8.f, -1.f / 8.f);
    glUniform2f(g_gx_uPatOff, 0.f, 0.f);
    glUniform1f(g_gx_uAlpha, 1.f);
    glUniform1i(g_gx_uNoBlend, 0);
}

/*======================================================================
 *  6b. User supplied post processing shader
 *
 *  gxPresent() finishes by drawing one quad carrying the finished canvas.
 *  setpostshader() swaps the fragment shader of that one draw, which is
 *  enough for anything that only needs to transform the finished picture:
 *  colour grading, blur, scanlines, vignettes, distortion, or a field
 *  computed per pixel on the GPU.
 *
 *  It deliberately does NOT touch g_gx_prog, so every EasyX drawing call
 *  (circle, putimage, outtextxy ...) keeps its own well defined behaviour.
 *  Replacing that program would mean reimplementing the uUseTex branches
 *  that solid fills, textures, hatch patterns and text all depend on.
 *
 *  The vertex shader is always the built in GX_VS.  The quad covers the
 *  whole viewport, so the shader gets:
 *
 *      varying vec2 vUV;          0..1 over the canvas, (0,0) bottom left
 *      varying vec2 vPos;         -1..1 clip space, handy for effects
 *                                 that are centred on the window
 *      uniform sampler2D uTex;    the finished canvas, texture unit 0
 *      uniform vec2  uTexel;      1/size of the canvas, for neighbours
 *      uniform float uTime;       seconds since the first frame
 *      uniform mat4  uProj;       identity during this pass
 *
 *  Any other uniform is the caller's: look it up with getpostloc() and
 *  feed it with setpost1f() and friends.  #version 120 only - this is a
 *  compatibility profile context, so use texture2D() and gl_FragColor.
 */
static void gxPostDestroy(void) {
    if (g_gx_postProg) glDeleteProgram(g_gx_postProg);
    g_gx_postProg = 0;
    g_gx_postProj = g_gx_postTex = g_gx_postTexel = g_gx_postTime = -1;
}

/* Compile fsSrc as the post processing pass.  NULL or "" turns it off and
 * restores the plain blit.  Returns true on success; on a compile or link
 * failure the driver log is shown and false is returned. */
static bool setpostshader(const char* fsSrc) {
    GLuint vs, fs, prog;
    GLint ok = 0;
    char buf[512];
    if (!g_gx_glReady) return false;
    gxFlush();
    gxPostDestroy();
    if (!fsSrc || !*fsSrc) { glUseProgram(g_gx_prog); return true; }

    vs = gxCompile(GL_VERTEX_SHADER, GX_VS);
    fs = gxCompile(GL_FRAGMENT_SHADER, fsSrc);
    /* gxCompile() has already shown the log, so just bail out quietly. */
    glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
    if (!ok) { glDeleteShader(vs); glDeleteShader(fs); return false; }

    prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    /* Same slots as the built in program.  The renderer enables 0/1/2 once
     * and never touches them again, so a program that let the linker pick
     * its own indices would read garbage. */
    glBindAttribLocation(prog, 0, "aPos");
    glBindAttribLocation(prog, 1, "aColor");
    glBindAttribLocation(prog, 2, "aUV");
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!ok) {
        memset(buf, 0, sizeof(buf));
        glGetProgramInfoLog(prog, 511, NULL, buf);
        MessageBoxA(NULL, buf, "Post Shader Error", MB_OK);
        glDeleteProgram(prog);
        return false;
    }
    g_gx_postProg  = prog;
    g_gx_postProj  = glGetUniformLocation(prog, "uProj");
    g_gx_postTex   = glGetUniformLocation(prog, "uTex");
    g_gx_postTexel = glGetUniformLocation(prog, "uTexel");
    g_gx_postTime  = glGetUniformLocation(prog, "uTime");
    glUseProgram(prog);
    /* uTex would default to unit 0 anyway, but say it: unit 0 is what the
     * canvas is bound to during the present. */
    if (g_gx_postTex >= 0) glUniform1i(g_gx_postTex, 0);
    glUseProgram(g_gx_prog);
    return true;
}

/* Location of one of the caller's own uniforms, or -1 if the shader does
 * not declare it (the compiler drops uniforms that are never read). */
static int getpostloc(const char* name) {
    if (!g_gx_postProg || !name) return -1;
    return (int)glGetUniformLocation(g_gx_postProg, name);
}

/* Feed a uniform.  loc comes from getpostloc(); anything negative is
 * ignored, so a shader can be swapped without guarding every call. */
static void setpost1f(int loc, float x) {
    if (!g_gx_postProg || loc < 0) return;
    glUseProgram(g_gx_postProg); glUniform1f((GLint)loc, x); glUseProgram(g_gx_prog);
}
static void setpost2f(int loc, float x, float y) {
    if (!g_gx_postProg || loc < 0) return;
    glUseProgram(g_gx_postProg); glUniform2f((GLint)loc, x, y); glUseProgram(g_gx_prog);
}
static void setpost3f(int loc, float x, float y, float z) {
    if (!g_gx_postProg || loc < 0) return;
    glUseProgram(g_gx_postProg); glUniform3f((GLint)loc, x, y, z); glUseProgram(g_gx_prog);
}
static void setpost4f(int loc, float x, float y, float z, float w) {
    if (!g_gx_postProg || loc < 0) return;
    glUseProgram(g_gx_postProg); glUniform4f((GLint)loc, x, y, z, w); glUseProgram(g_gx_prog);
}
static void setpost1i(int loc, int v) {
    if (!g_gx_postProg || loc < 0) return;
    glUseProgram(g_gx_postProg); glUniform1i((GLint)loc, v); glUseProgram(g_gx_prog);
}

/* Bind an IMAGE as an extra texture for the shader to sample.  Unit 0 is
 * the canvas, so use 1 and up, and point a sampler uniform at the same
 * number with setpost1i(). */
static void setposttex(int unit, IMAGE* img) {
    if (!g_gx_postProg || unit < 1 || unit > 7) return;
    glActiveTexture((GLenum)(GL_TEXTURE0 + unit));
    glBindTexture(GL_TEXTURE_2D, img ? img->tex : 0);
    glActiveTexture(GL_TEXTURE0);
}

/* Blend modes for setblendmode().  EasyX has no equivalent: it always uses
 * ordinary source alpha blending.  The extra modes are what GDI / image
 * editors expose and are handy for light and shadow effects:
 *
 *   GX_BLEND_ALPHA  src*a + dst*(1-a)   the default, standard transparency
 *   GX_BLEND_ADD    src*a + dst         additive: glow, fire, light
 *   GX_BLEND_SUB    dst - src*a         subtractive: shadow, darkening
 *   GX_BLEND_MUL    dst * src           multiply: tinting, colour filters
 *   GX_BLEND_SCREEN 1-(1-src)*(1-dst)   screen: brightening, never blows out
 *   GX_BLEND_NONE   src                 replace, ignores alpha entirely
 *
 * They all need blending enabled, so like alpha they are ignored outside
 * R2_COPYPEN (the other ROP modes run GL colour logic instead). */
#define GX_BLEND_ALPHA   0
#define GX_BLEND_ADD     1
#define GX_BLEND_SUB     2
#define GX_BLEND_MUL     3
#define GX_BLEND_SCREEN  4
#define GX_BLEND_NONE    5

static GxVtxVec g_gx_vbuf;
static GxCmdVec g_gx_cmds;
static size_t   g_gx_cmdStart = 0;
static GLuint   g_gx_curTex = 0;
static int      g_gx_curUseTex = 0;
static int      g_gx_curRop = R2_COPYPEN;
static float    g_gx_curAlpha = 1.f;        /* alpha of the running batch     */
static float    g_gx_alpha = 1.f;           /* setalpha(): default for drawing */
/* miximagec() / miximagei(): the second source of the cross fade and how far
 * to travel towards it.  Per command state, so changing any of it closes the
 * open batch - same as vertA. */
static int    g_gx_curMixMode = 0;      /* 0 off, 1 flat colour, 2 second image */
static float  g_gx_curMixW = 0.f;       /* 0 = all first source, 1 = all second */
static float  g_gx_curMixR = 0.f, g_gx_curMixG = 0.f;
static float  g_gx_curMixB = 0.f, g_gx_curMixA = 1.f;
static GLuint g_gx_curTex2 = 0;         /* the second image, on GL_TEXTURE1 */
/* IMAGE texture filtering, see setimagefilter().  Declared here because
 * gxFlush() (section 6) reads it on every draw.
 * 0 = NEAREST (crisp, the default), 1 = LINEAR (smooth). */
static int  g_gx_imgFilter = 0;
static float    g_gx_curPatSx = 0.125f, g_gx_curPatSy = -0.125f;
static float    g_gx_curPatOx = 0.f, g_gx_curPatOy = 0.f;
static int      g_gx_curPatTile = 0;
static int      g_gx_curBlend = GX_BLEND_ALPHA;   /* of the running batch */
/* IMAGE filtering is recorded PER COMMAND, exactly like tile / blend /
 * alpha, and applied unconditionally at draw time.  It cannot be a global
 * that gxFlush() re-reads, because the filter is a property of the texture
 * object: two commands sharing one texture that were queued before and
 * after a setimagefilter() call would otherwise both get the LAST value. */
static int      g_gx_curFilter = 0;               /* of the running batch   */
static int      g_gx_blend    = GX_BLEND_ALPHA;   /* setblendmode()        */

/* Corner opacity state, see GX_FS.  Like tile / blend / alpha it is
 * recorded per command: two commands queued either side of a change must
 * not share one value, so any change closes the open batch.
 *
 * It is ONE SHOT.  There is no setvertalpha() to match: the two calls that
 * use it take the four corner values as arguments, so leaving it on would
 * silently fade the next unrelated putimage().  Both clear it again before
 * returning. */
static int      g_gx_curVertA   = 0;              /* of the running batch */



/* Apply the clip box.
 *
 * g_gx_clipRect is stored in LOGICAL coordinates, so the clip region follows
 * setorigin() and setaspectratio() exactly like every other EasyX
 * coordinate.  GetRgnBox() reports the bounding rectangle the GDI way, with
 * an exclusive right / bottom edge (CreateRectRgn(0,0,100,100) covers the
 * pixels 0..99), hence there is no "+1" here.  Only the bounding box is
 * used, so a non rectangular HRGN clips to its rectangle. */
static void gxApplyClip(void) {
    long x0, x1, y0, y1;
    if (!g_gx_glReady) return;
    if (!g_gx_clipOn) { glDisable(GL_SCISSOR_TEST); return; }
    x0 = (long)floorf((float)g_gx_clipRect.left   * g_gx_scaleX + g_gx_originX);
    x1 = (long)ceilf ((float)g_gx_clipRect.right  * g_gx_scaleX + g_gx_originX);
    y0 = (long)floorf((float)g_gx_clipRect.top    * g_gx_scaleY + g_gx_originY);
    y1 = (long)ceilf ((float)g_gx_clipRect.bottom * g_gx_scaleY + g_gx_originY);
    /* Clamp before flipping: the old code only clamped x0 and the bottom
     * edge, so a region left of or above the target produced a negative
     * glScissor() width / height (GL_INVALID_VALUE, box ignored). */
    if (x0 < 0)      x0 = 0;
    if (y0 < 0)      y0 = 0;
    if (x0 > g_gx_devW) x0 = g_gx_devW;
    if (y0 > g_gx_devH) y0 = g_gx_devH;
    if (x1 < x0) x1 = x0;
    if (y1 < y0) y1 = y0;
    if (x1 > g_gx_devW) x1 = g_gx_devW;
    if (y1 > g_gx_devH) y1 = g_gx_devH;
    glEnable(GL_SCISSOR_TEST);
    /* glScissor() counts y from the bottom of the render target. */
    glScissor((GLint)x0, (GLint)((long)g_gx_devH - y1),
              (GLsizei)(x1 - x0), (GLsizei)(y1 - y0));
}

static void gxBindTarget(void) {
    if (!g_gx_glReady) return;
    glBindFramebuffer(GL_FRAMEBUFFER, g_gx_target->fbo);
    glViewport(0, 0, g_gx_target->w, g_gx_target->h);
    glUniformMatrix4fv(g_gx_uProj, 1, GL_FALSE, g_gx_proj);
    gxApplyClip();
}

/* Translate an EasyX / GDI ROP2 code into a GL logic op.
 * P is the pen (the incoming fragment), D the destination pixel:
 *   R2_MASKNOTPEN  ~P & D      R2_MASKPENNOT   P & ~D
 *   R2_MERGENOTPEN ~P | D      R2_MERGEPENNOT  P | ~D
 * The two "NOT" flavours of AND and OR used to be swapped, which made four
 * of the sixteen modes behave like a different one. */
static GLenum gxRop2ToLogicOp(int rop2) {
    switch (rop2) {
    case R2_BLACK:       return (GLenum)GL_CLEAR;          /* 0             */
    case R2_WHITE:       return (GLenum)GL_SET;            /* 1             */
    case R2_NOP:         return (GLenum)GL_NOOP;           /* D             */
    case R2_NOT:         return (GLenum)GL_INVERT;         /* ~D            */
    case R2_COPYPEN:     return (GLenum)GL_COPY;           /* P             */
    case R2_NOTCOPYPEN:  return (GLenum)GL_COPY_INVERTED;  /* ~P            */
    case R2_MERGEPEN:    return (GLenum)GL_OR;             /* P | D         */
    case R2_MERGEPENNOT: return (GLenum)GL_OR_REVERSE;     /* P | ~D        */
    case R2_MERGENOTPEN: return (GLenum)GL_OR_INVERTED;    /* ~P | D        */
    case R2_MASKPEN:     return (GLenum)GL_AND;            /* P & D         */
    case R2_MASKPENNOT:  return (GLenum)GL_AND_REVERSE;    /* P & ~D        */
    case R2_MASKNOTPEN:  return (GLenum)GL_AND_INVERTED;   /* ~P & D        */
    case R2_XORPEN:      return (GLenum)GL_XOR;            /* P ^ D         */
    case R2_NOTXORPEN:   return (GLenum)GL_EQUIV;          /* ~(P ^ D)      */
    case R2_NOTMERGEPEN: return (GLenum)GL_NOR;            /* ~(P | D)      */
    case R2_NOTMASKPEN:  return (GLenum)GL_NAND;           /* ~(P & D)      */
    default:             return (GLenum)GL_COPY;           /* unknown -> P  */
    }
}

/* Reject an out of range ROP2 code instead of feeding it to glLogicOp(). */
GX_INLINE int gxRop2Fix(int rop2) {
    return (rop2 >= R2_BLACK && rop2 <= R2_WHITE) ? rop2 : R2_COPYPEN;
}

/* Turn a GX_BLEND_* value into glBlendFunc / glBlendEquation.
 *
 * glBlendEquation() is GL 1.2 (imaging) / 1.4 core, and the GL/gl.h that
 * ships with MinGW is only GL 1.1: the name is not declared there, so
 * calling it outright does not compile.  It also cannot simply be added
 * with DECLGL(), because some SDKs *do* declare it and then the static
 * function pointer would clash with the real declaration.  So it gets its
 * own prefixed pointer, loaded by hand in gxLoadGL().
 * The GL_FUNC_* constants are not in GL 1.1 either - hence the numbers. */
#ifndef GL_FUNC_ADD
#define GX_GL_FUNC_ADD            0x8006
#define GX_GL_FUNC_REVERSE_SUB    0x800B
#else
#define GX_GL_FUNC_ADD            GL_FUNC_ADD
#define GX_GL_FUNC_REVERSE_SUB    GL_FUNC_REVERSE_SUBTRACT
#endif

static void gxSetBlendEquation(int mode) {
    switch (mode) {
    case GX_BLEND_ADD:
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        break;
    case GX_BLEND_SUB:
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        break;
    case GX_BLEND_MUL:
        glBlendFunc(GL_DST_COLOR, GL_ZERO);
        break;
    case GX_BLEND_SCREEN:
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
        break;
    case GX_BLEND_NONE:
        glBlendFunc(GL_ONE, GL_ZERO);
        break;
    default:                       /* GX_BLEND_ALPHA */
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        break;
    }
    /* GL_FUNC_ADD is the default equation, so every mode except SUB works
     * even when the pointer is missing; only SUB (REVERSE_SUBTRACT) needs
     * it, and it degrades to ADD rather than failing. */
    if (gxBlendEquation)
        gxBlendEquation((mode == GX_BLEND_SUB) ? GX_GL_FUNC_REVERSE_SUB
                                               : GX_GL_FUNC_ADD);
}

static void gxSetRopState(int rop2) {
    int rop = gxRop2Fix(rop2);
    if (!g_gx_glReady) return;
    /* Outside R2_COPYPEN blending is off, so a textured fragment has to be
     * alpha tested (uNoBlend) - otherwise every glyph or transparent pixel
     * would be painted as an opaque block. */
    glUniform1i(g_gx_uNoBlend, (rop == R2_COPYPEN) ? 0 : 1);
    if (rop == R2_COPYPEN) {
        glDisable(GL_COLOR_LOGIC_OP);
        glEnable(GL_BLEND);
        gxSetBlendEquation(g_gx_curBlend);
    } else {
        glDisable(GL_BLEND);
        glEnable(GL_COLOR_LOGIC_OP);
        glLogicOp(gxRop2ToLogicOp(rop));
    }
}

static void gxEndCmd(void) {
    if (g_gx_vbuf.size > g_gx_cmdStart) {
        GLCmd c;
        c.first = g_gx_cmdStart;
        c.count = g_gx_vbuf.size - g_gx_cmdStart;
        c.tex = g_gx_curTex;
        c.useTex = g_gx_curUseTex;
        c.rop = g_gx_curRop;
        c.alpha = g_gx_curAlpha;
        c.psx = g_gx_curPatSx;
        c.psy = g_gx_curPatSy;
        c.pox = g_gx_curPatOx;
        c.poy = g_gx_curPatOy;
        c.tile = g_gx_curPatTile;
        c.blend = g_gx_curBlend;
        c.filter = g_gx_curFilter;
        c.vertA = g_gx_curVertA;
        c.mixMode = g_gx_curMixMode;
        c.mixW = g_gx_curMixW;
        c.tex2 = g_gx_curTex2;
        c.mixR = g_gx_curMixR; c.mixG = g_gx_curMixG;
        c.mixB = g_gx_curMixB; c.mixA = g_gx_curMixA;
        /* g_gx_quadRun still describes the batch that is being closed here:
         * gxV() and gxQuadMode() both end it BEFORE flipping the flag. */
        c.quads = g_gx_quadRun ? 1 : 0;
        GxCmdVec_pushv(&g_gx_cmds, c);
    }
    /* Round the next batch up to a multiple of 4 vertices.
     *
     * The index pattern is precomputed for absolute vertex numbers, quad j
     * covering vertices 4j..4j+3, so a quad run has to start on a multiple
     * of 4 - otherwise the pattern would read the previous batch's vertices.
     * At most three padding vertices per batch, and they sit between the two
     * commands, so no command covers them and they are never drawn. */
    while ((g_gx_vbuf.size & 3u) != 0u) {
        Vtx* t = GxVtxVec_push(&g_gx_vbuf);
        t->x = t->y = 0.f;
        t->r = t->g = t->b = t->a = 0.f;
        t->u = t->v = 0.f;
    }
    g_gx_cmdStart = g_gx_vbuf.size;
}

/* Grow the precomputed index pattern to cover at least "quads" quads. */
static void gxEnsureIdx(size_t quads) {
    size_t cap, j;
    if (quads <= g_gx_idxQuads) return;
    cap = (g_gx_idxQuads == 0) ? 1024 : g_gx_idxQuads;
    while (cap < quads) {
        if (cap > ((size_t)-1) / 2) { cap = quads; break; }
        cap *= 2;
    }
    {
        unsigned int* p = (unsigned int*)realloc(g_gx_idx, cap * 6u * sizeof(unsigned int));
        if (!p) { MessageBoxA(NULL, "Out of memory (index buffer)", "Error", MB_OK); exit(1); }
        g_gx_idx = p;
    }
    for (j = g_gx_idxQuads; j < cap; j++) {
        unsigned int b = (unsigned int)(j * 4u);
        g_gx_idx[j * 6u + 0] = b;     g_gx_idx[j * 6u + 1] = b + 1u;
        g_gx_idx[j * 6u + 2] = b + 2u;
        g_gx_idx[j * 6u + 3] = b;     g_gx_idx[j * 6u + 4] = b + 2u;
        g_gx_idx[j * 6u + 5] = b + 3u;
    }
    g_gx_idxQuads = cap;
}

GX_INLINE void gxSetRop(int rop) {
    if (rop != g_gx_curRop) {
        gxEndCmd();
        g_gx_curRop = rop;
    }
}

/* Same idea as gxSetRop(): alpha is per command, so changing it has to
 * close the batch that is still open. */
GX_INLINE void gxSetAlpha(float a) {
    if (a < 0.f) a = 0.f;
    if (a > 1.f) a = 1.f;
    if (a != g_gx_curAlpha) {
        gxEndCmd();
        g_gx_curAlpha = a;
    }
}

GX_INLINE void gxSetPat(float sx, float sy, float ox, float oy, int tile) {
    if (sx != g_gx_curPatSx || sy != g_gx_curPatSy ||
        ox != g_gx_curPatOx || oy != g_gx_curPatOy ||
        tile != g_gx_curPatTile) {
        gxEndCmd();
        g_gx_curPatSx = sx;
        g_gx_curPatSy = sy;
        g_gx_curPatOx = ox;
        g_gx_curPatOy = oy;
        g_gx_curPatTile = tile;
    }
}

static void gxSetTex(GLuint tex, int useTex) {
    if (tex != g_gx_curTex || useTex != g_gx_curUseTex) {
        gxEndCmd();
        g_gx_curTex = tex;
        g_gx_curUseTex = useTex;
    }
}

/* Same idea as gxSetRop() / gxSetAlpha(): the blend equation is per
 * command, so changing it has to close the batch that is still open. */
GX_INLINE void gxSetBlend(int mode) {
    if (mode < 0 || mode > GX_BLEND_NONE) mode = GX_BLEND_ALPHA;
    if (mode != g_gx_curBlend) {
        gxEndCmd();
        g_gx_curBlend = mode;
    }
}

GX_INLINE void setblendmode(int mode) {
    g_gx_blend = (mode >= GX_BLEND_ALPHA && mode <= GX_BLEND_NONE)
              ? mode : GX_BLEND_ALPHA;
    gxSetBlend(g_gx_blend);
}
GX_INLINE int getblendmode(void) { return g_gx_blend; }

/* implemented in section 18b (EasyX GetImageBuffer support) */
static void gxSyncImgBufs(void);
static void gxImgBufDrop(IMAGE* img);
static void gxImgBufDropAll(void);

static void gxFlush(void) {
    size_t i;
    /* Nothing to submit before initgraph() (or after closegraph()): the GL
     * entry points are not loaded yet, so just drop the batch. */
    if (!g_gx_glReady) {
        GxVtxVec_clear(&g_gx_vbuf);
        GxCmdVec_clear(&g_gx_cmds);
        g_gx_cmdStart = 0;
        return;
    }
    gxEndCmd();
    gxSyncImgBufs();      /* upload IMAGE buffers edited through GetImageBuffer() */
    if (g_gx_cmds.size == 0) { GxVtxVec_clear(&g_gx_vbuf); g_gx_cmdStart = 0; return; }
    /* Upload the index pattern before drawing.  It only ever grows by being
     * appended to, so only the part that is new has to be sent; a frame that
     * reuses the same capacity uploads nothing at all. */
    {
        size_t maxV = 0, need;
        for (i = 0; i < g_gx_cmds.size; i++) {
            if (g_gx_cmds.data[i].quads) {
                size_t end = g_gx_cmds.data[i].first + g_gx_cmds.data[i].count;
                if (end > maxV) maxV = end;
            }
        }
        need = (maxV + 3u) / 4u;
        if (need > 0) {
            gxEnsureIdx(need);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_gx_ibo);
            if (need > g_gx_iboQuads) {
                glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                             (GLsizei)(need * 6u * sizeof(unsigned int)),
                             NULL, GL_DYNAMIC_DRAW);
                g_gx_iboQuads = need;
                g_gx_iboUpTo  = 0;      /* the store was reallocated */
            }
            if (need > g_gx_iboUpTo) {
                glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,
                                (GXGLintptr)(g_gx_iboUpTo * 6u),
                                (GXGLsizeiptr)((need - g_gx_iboUpTo) * 6u
                                               * sizeof(unsigned int)),
                                g_gx_idx + g_gx_iboUpTo * 6u);
                g_gx_iboUpTo = need;
            }
        }
    }
    gxBindTarget();
    glBindBuffer(GL_ARRAY_BUFFER, g_gx_vbo);
    /* glBufferSubData() when the existing store is large enough, because
     * glBufferData() re-allocates.  Round the new capacity up so a buffer
     * that keeps growing by a few vertices per frame does not reallocate
     * on every one of them. */
    if (g_gx_vbuf.size > g_gx_vboCap) {
        size_t cap = (g_gx_vboCap == 0) ? 4096 : g_gx_vboCap;
        while (cap < g_gx_vbuf.size) {
            if (cap > ((size_t)-1) / 2) { cap = g_gx_vbuf.size; break; }
            cap *= 2;
        }
        glBufferData(GL_ARRAY_BUFFER, (GLsizei)(cap * sizeof(Vtx)),
                     NULL, GL_DYNAMIC_DRAW);
        g_gx_vboCap = cap;
    }
    if (g_gx_vbuf.size != 0)
        glBufferSubData(GL_ARRAY_BUFFER, 0,
                        (GLsizei)(g_gx_vbuf.size * sizeof(Vtx)), g_gx_vbuf.data);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vtx), (const void*)0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vtx), (const void*)8);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vtx), (const void*)24);
    /* Uniforms and GL enable flags are sticky, but a whole frame of plain
     * fills repeats the same handful of values for every command - and
     * glUniform*() / glBindTexture() are not free when they are called
     * thousands of times per frame.  Skip the ones that did not change.
     *
     * "first" forces a full set on the first command of every flush: GL
     * state is global and gxPresent(), gxClearFbo() and rotateimage() all
     * touch it directly in between, so a cache that survived across flushes
     * would silently leave the wrong blend or texture bound. */
    {
        int   lastRop  = 0;
        int   lastBlend = 0;
        int   lastUseTex = 0;
        int   lastFilter = 0;
        float lastPsx = 0.f, lastPsy = 0.f;
        float lastPox = 0.f, lastPoy = 0.f;
        float lastAlpha = 0.f;
        GLuint lastTex = 0;
        int   lastVertA = 0;
        int   lastMixMode = 0;
        float lastMixW = 0.f;
        float lastMixR = 0.f, lastMixG = 0.f;
        float lastMixB = 0.f, lastMixA = 0.f;
        GLuint lastTex2 = 0;
        bool  first = true;

        for (i = 0; i < g_gx_cmds.size; i++) {
            GLCmd* c = &g_gx_cmds.data[i];
            if (first || c->rop != lastRop || c->blend != lastBlend) {
                gxSetRopState(c->rop);
                /* gxSetRopState() reads g_gx_curBlend, so it has to be set
                 * before the call rather than after it. */
                g_gx_curBlend = c->blend;
                if (gxRop2Fix(c->rop) == R2_COPYPEN) gxSetBlendEquation(c->blend);
                lastRop = c->rop; lastBlend = c->blend;
            } else {
                g_gx_curBlend = c->blend;
            }
            if (first || c->psx != lastPsx || c->psy != lastPsy) {
                glUniform2f(g_gx_uPatScale, c->psx, c->psy);
                lastPsx = c->psx; lastPsy = c->psy;
            }
            if (first || c->pox != lastPox || c->poy != lastPoy) {
                glUniform2f(g_gx_uPatOff, c->pox, c->poy);
                lastPox = c->pox; lastPoy = c->poy;
            }
            if (first || c->alpha != lastAlpha) {
                glUniform1f(g_gx_uAlpha, c->alpha);
                lastAlpha = c->alpha;
            }
            if (first || c->tex != lastTex) {
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, c->tex);
                lastTex = c->tex;
                /* The filter belongs to the TEXTURE, not to the command, so
                 * binding a different one makes the cached value meaningless:
                 * force it to be set again.  Missing this is what two
                 * putimage() calls on the same image with different filters
                 * would hit - the second would silently keep the first's. */
                lastFilter = -1;
            }
            /* useTex 2 is putimage().  The filter comes from the command,
             * not from the current global: two commands can share one
             * texture but carry different filters (that is the whole point
             * of setimagefilter()), so it is set whenever the value differs
             * from whatever is in force on the bound texture.  The glyph
             * atlas (1) and the hatch patterns (3) keep their own filtering
             * and are never touched here. */
            if (c->useTex == 2 && c->filter != lastFilter) {
                GLint f = c->filter ? GL_LINEAR : GL_NEAREST;
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, f);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, f);
                lastFilter = c->filter;
            }
            if (c->useTex == 3 && c->tile) {
                /* A BS_PATTERN brush shares its texture with putimage(),
                 * which needs CLAMP - a brush has to tile, so switch to
                 * REPEAT for this draw only.  Hatch textures are born
                 * REPEAT and must not be touched, hence the c->tile flag.
                 * The two glTexParameteri() are unconditional: the wrap mode
                 * is put back right after the draw, so it never survives to
                 * the next command and cannot be cached. */
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
            }
            if (first || c->useTex != lastUseTex) {
                glUniform1i(g_gx_uUseTex, c->useTex);
                lastUseTex = c->useTex;
            }
            if (first || c->vertA != lastVertA) {
                glUniform1i(g_gx_uVertA, c->vertA);
                lastVertA = c->vertA;
            }
            if (first || c->mixMode != lastMixMode) {
                glUniform1i(g_gx_uMixMode, c->mixMode);
                lastMixMode = c->mixMode;
            }
            if (c->mixMode != 0) {
                if (first || c->mixW != lastMixW) {
                    glUniform1f(g_gx_uMixW, c->mixW);
                    lastMixW = c->mixW;
                }
                if (c->mixMode == 1
                    && (first || c->mixR != lastMixR || c->mixG != lastMixG
                        || c->mixB != lastMixB || c->mixA != lastMixA)) {
                    glUniform4f(g_gx_uMixColor, c->mixR, c->mixG,
                                c->mixB, c->mixA);
                    lastMixR = c->mixR; lastMixG = c->mixG;
                    lastMixB = c->mixB; lastMixA = c->mixA;
                }
                if (c->mixMode == 2 && (first || c->tex2 != lastTex2)) {
                    glActiveTexture(GL_TEXTURE1);
                    glBindTexture(GL_TEXTURE_2D, c->tex2);
                    glActiveTexture(GL_TEXTURE0);
                    lastTex2 = c->tex2;
                }
            }
            if (c->quads) {
                /* 6 indices per quad; the offset into the index buffer is
                 * quad number first/4 times 6, and glDrawElements() takes it
                 * in bytes when an element buffer is bound. */
                glDrawElements(GL_TRIANGLES,
                               (GLsizei)(c->count / 4u * 6u),
                               GL_UNSIGNED_INT,
                               (const void*)((c->first / 4u) * 6u
                                             * sizeof(unsigned int)));
            } else {
                glDrawArrays(GL_TRIANGLES, (GLint)c->first, (GLint)c->count);
            }
            if (c->useTex == 3 && c->tile) {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
            }
            first = false;
        }
    }
    GxCmdVec_clear(&g_gx_cmds);
    GxVtxVec_clear(&g_gx_vbuf);
    g_gx_cmdStart = 0;
    g_gx_quadRun = false;      /* the next batch starts as a triangle list */
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    g_gx_curRop = g_gx_rop2;
    g_gx_curAlpha = g_gx_alpha;        /* invariant: the setalpha() level when idle */
    g_gx_curBlend = g_gx_blend;        /* invariant: the setblendmode() mode too  */
    g_gx_curFilter = g_gx_imgFilter;   /* invariant: the setimagefilter() mode    */
    gxSetRopState(R2_COPYPEN);   /* invariant: plain blending when idle */
}

/*======================================================================
 *  6a. OpenGL context version
 *====================================================================*/
static int g_gx_reqGLMajor = 3, g_gx_reqGLMinor = 3;   /* what initgraph() asks for */
static int g_gx_glMajor = 0, g_gx_glMinor = 0;         /* what it actually got      */

/* Ask for a specific OpenGL context version.  Must be called BEFORE
 * initgraph(); once the context exists the request is ignored.
 *
 * Will this make drawing faster?  Almost certainly not, and it is worth
 * being blunt about why:
 *
 *   - Frames here are bounded by fill rate, memory bandwidth and how much
 *     work the CPU submits.  A higher version number changes none of them.
 *     Every draw goes through one shader and one glDrawArrays() call; 4.6
 *     does not execute that path faster than 3.3.
 *   - Worse, "newest available" on Windows usually means a CORE profile
 *     context, and this library would break on one: the shaders are
 *     #version 120 and use varying / texture2D / gl_FragColor (all removed
 *     in core), and nothing ever binds a VAO, which core requires.  A core
 *     context would give a black window and GL_INVALID_OPERATION.
 *
 * So the request is always for the COMPATIBILITY profile, and the useful
 * reasons to call this are feature access (a newer extension, a debug
 * context) - not speed.  For speed see gradtriangle() (fewer draw calls),
 * setimagebuffermode(GX_IMGBUF_DISCARD) (no read back) and setaasamples(0).
 *
 * The request is best effort: if the driver refuses, creation falls back to
 * 3.3 and then to the default context, so this can never break a program.
 * Compare getglmajor()/getglminor() against what you asked for to find out
 * whether it took effect. */
GX_INLINE void setglversion(int major, int minor) {
    if (g_gx_glReady) return;                  /* too late: context exists */
    if (major < 1) major = 1;
    if (minor < 0) minor = 0;
    g_gx_reqGLMajor = major;
    g_gx_reqGLMinor = minor;
}
GX_INLINE int getglmajor(void) { return g_gx_glMajor; }
GX_INLINE int getglminor(void) { return g_gx_glMinor; }

/*======================================================================
 *  6b. Multisample anti aliasing (MSAA)
 *====================================================================*/
/* The canvas is normally an FBO with a plain texture attached.  With MSAA
 * on it becomes an FBO with a multisampled RENDERBUFFER attached, and
 * g_gx_canvasTarget.fbo - the one every draw call binds - points at that
 * instead.  Drawing code is untouched; the only thing that has to know is
 * anything that READS the canvas, because glReadPixels() and texture
 * sampling cannot touch a multisample attachment.  Those go through
 * gxReadFbo(), which resolves first. */
static GLuint g_gx_msaaFbo = 0, g_gx_msaaRb = 0;
static int    g_gx_msaaW = 0, g_gx_msaaH = 0;
static int    g_gx_aaSamples = 0;          /* requested level, 0 = off */

static void gxMsaaDestroy(void) {
    if (g_gx_canvasTarget.fbo && g_gx_canvasTarget.fbo == g_gx_msaaFbo)
        g_gx_canvasTarget.fbo = g_gx_fbo;     /* never leave a dangling binding */
    if (g_gx_msaaRb)  { glDeleteRenderbuffers(1, &g_gx_msaaRb);  g_gx_msaaRb = 0; }
    if (g_gx_msaaFbo) { glDeleteFramebuffers(1, &g_gx_msaaFbo);  g_gx_msaaFbo = 0; }
    g_gx_msaaW = g_gx_msaaH = 0;
    glBindFramebuffer(GL_FRAMEBUFFER, g_gx_canvasTarget.fbo);
}

static void gxMsaaCreate(void) {
    int n = g_gx_aaSamples;
    if (!g_gx_glReady || n < 2) return;
    if (!glRenderbufferStorageMultisample || !glBlitFramebuffer) return;
    if (g_gx_canvasTarget.w < 1 || g_gx_canvasTarget.h < 1) return;
    gxMsaaDestroy();        /* idempotent: it is called on every recreate */
    glGenFramebuffers(1, &g_gx_msaaFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, g_gx_msaaFbo);
    glGenRenderbuffers(1, &g_gx_msaaRb);
    glBindRenderbuffer(GL_RENDERBUFFER, g_gx_msaaRb);
    /* There is no query for "which counts work": an unsupported one simply
     * leaves the framebuffer incomplete.  So walk down from the request
     * (4 -> 2) and take the first that is accepted. */
    for (; n >= 2; n /= 2) {
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, (GLsizei)n, GL_RGBA8,
                                         (GLsizei)g_gx_canvasTarget.w,
                                         (GLsizei)g_gx_canvasTarget.h);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                  GL_RENDERBUFFER, g_gx_msaaRb);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE)
            break;
    }
    if (n < 2) { gxMsaaDestroy(); return; }   /* driver refuses: stay off */
    g_gx_msaaW = g_gx_canvasTarget.w;
    g_gx_msaaH = g_gx_canvasTarget.h;
    g_gx_canvasTarget.fbo = g_gx_msaaFbo;
    glBindFramebuffer(GL_FRAMEBUFFER, g_gx_msaaFbo);
}

/* Fold the samples into the plain texture the rest of the library reads. */
static void gxMsaaResolve(void) {
    if (!g_gx_msaaFbo || g_gx_canvasTarget.fbo != g_gx_msaaFbo) return;
    if (g_gx_msaaW != g_gx_canvasTarget.w || g_gx_msaaH != g_gx_canvasTarget.h) {
        gxMsaaDestroy();                 /* canvas was resized: rebuild */
        gxMsaaCreate();
        if (!g_gx_msaaFbo) return;
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, g_gx_msaaFbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_gx_fbo);
    glBlitFramebuffer(0, 0, g_gx_msaaW, g_gx_msaaH, 0, 0, g_gx_msaaW, g_gx_msaaH,
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, g_gx_msaaFbo);
}

/* Every glBindFramebuffer() that is followed by a read goes through this. */
GX_INLINE GLuint gxReadFbo(GLuint fbo) {
    if (fbo && fbo == g_gx_msaaFbo) { gxMsaaResolve(); return g_gx_fbo; }
    return fbo;
}

/* EasyX has no counterpart.  Level is a sample count: 0 or 1 turns anti
 * aliasing off, 4 is the usual value, 8/16 cost more for little gain.
 * Safe to call before initgraph() - it takes effect once GL is up. */
GX_INLINE void setaasamples(int n) {
    g_gx_aaSamples = (n < 0) ? 0 : n;
    if (!g_gx_glReady) return;
    gxMsaaDestroy();
    gxMsaaCreate();
}
GX_INLINE int getaasamples(void) { return g_gx_aaSamples; }

/* RENDER_AUTO: show what has been drawn so far.  A time based throttle keeps
 * a tight loop from burning one SwapBuffers() per primitive, and an open
 * batch (BeginBatchDraw() ... EndBatchDraw()) suppresses the auto present. */
static void gxAutoPresent(void) {
    static DWORD s_lastTick = 0;
    DWORD now;
    if (g_gx_renderMode != RENDER_AUTO || !g_gx_glReady || g_gx_batchDraw) return;
    now = GetTickCount();
    if (s_lastTick != 0 && (now - s_lastTick) < 8u) {
        /* Throttled - but do not drop the frame: gxPump() shows the one
         * that is owed before the caller blocks on input, otherwise the
         * last primitive of a burst would only appear after the next
         * mouse move or key press. */
        g_gx_presentPending = true;
        return;
    }
    s_lastTick = now;
    g_gx_presentPending = false;
    gxFlush();
    gxPresent();
}

/* 300000 vertices is 9.2 MB.  Big, but one upload beats several: at 50000 a
 * 120000 vertex frame uploads three times instead of once, and since
 * glBufferSubData() reuses the store there is nothing to gain by splitting
 * it up.  (Lowered to 50000 at one point, then put back - see changelog.) */
#define GX_FLUSH_VERTS 300000

GX_INLINE void gxCheckFlush(void) {
    if (g_gx_vbuf.size > GX_FLUSH_VERTS) gxFlush();
    gxAutoPresent();
}

/* Alpha of a COLORREF as GL coverage, 0..1, where 1.f is solid and 0.f is
 * invisible.  The colour byte is a transparency, so it is inverted here -
 * see the note inside.  Every RGB() colour lands on exactly 1.f. */
GX_INLINE float gxAlphaOf(COLORREF c) {
    /* The alpha byte is a TRANSPARENCY: 0 = fully opaque, 255 = fully
     * transparent.  GL wants the opposite (its alpha is coverage, 1 =
     * solid), so it is inverted here - this is the single place where the
     * COLORREF convention becomes the GL one.
     *
     *   0x00BBGGRR (every RGB() colour) -> 1.f  solid
     *   0x80......                      -> 0.5f half
     *   0xFF......                      -> 0.f  invisible
     */
    return (float)(255 - (int)GetAValue(c)) / 255.f;
}

/* alphagradpicture() is the only caller: 1 = take the fragment alpha from
 * the four corner vertices, 0 = ignore it.  It is per command, so changing
 * it has to close the open batch, same as the other state setters. */
GX_INLINE void gxSetVertA(int vertA) {
    if (vertA != g_gx_curVertA) {
        gxEndCmd();
        g_gx_curVertA = vertA;
    }
}

/* Byte to float colour conversion.  Written as a divide rather than a
 * multiply by 1/255: once GetRValue() has expanded into shifts and masks
 * the compiler folds the constant anyway, and measured either way the two
 * are indistinguishable - the multiply version came out 2% SLOWER. */
#define GX_BYTE_TO_FLOAT(v) ((float)(v) / 255.f)

/* Lowest level: append one vertex and nothing else.  gxV() and gxQuad*() are
 * the two callers, and they are the ones that decide what the batch is. */
GX_INLINE void gxVPush(float x, float y, COLORREF c, float u, float v) {
    Vtx* t = GxVtxVec_push(&g_gx_vbuf);
    t->x = x; t->y = y;
    t->r = GX_BYTE_TO_FLOAT(GetRValue(c));
    t->g = GX_BYTE_TO_FLOAT(GetGValue(c));
    t->b = GX_BYTE_TO_FLOAT(GetBValue(c));
    t->a = gxAlphaOf(c);
    t->u = u; t->v = v;
}

/* Ordinary triangle list: the fans of circle/ellipse/pie, the triangle of
 * gxTri(), the polygon strips.  Not quads, so it ends a quad run first -
 * one command cannot hold both kinds. */
GX_INLINE void gxV(float x, float y, COLORREF c, float u, float v) {
    if (g_gx_quadRun && g_gx_vbuf.size > g_gx_cmdStart) gxEndCmd();
    g_gx_quadRun = false;
    gxVPush(x, y, c, u, v);
}

/* Start or continue a run of quads.  Ends whatever the batch was, so the
 * switch is always on a command boundary. */
GX_INLINE void gxQuadMode(void) {
    if (!g_gx_quadRun && g_gx_vbuf.size > g_gx_cmdStart) gxEndCmd();
    g_gx_quadRun = true;
}

/* Axis aligned rectangle (continuous coordinates, right/bottom exclusive).
 * Four vertices, not six: the index buffer turns them into the same two
 * triangles.  gxV() would do the same thing but would also break the run. */
GX_INLINE void gxQuad(float x0, float y0, float x1, float y1, COLORREF c) {
    gxQuadMode();
    gxVPush(x0, y0, c, 0, 0); gxVPush(x1, y0, c, 0, 0);
    gxVPush(x1, y1, c, 0, 0); gxVPush(x0, y1, c, 0, 0);
}

/* Arbitrary quadrilateral */
GX_INLINE void gxQuad4(float ax, float ay, float bx, float by,
                       float cx, float cy, float dx, float dy, COLORREF c) {
    gxQuadMode();
    gxVPush(ax, ay, c, 0, 0); gxVPush(bx, by, c, 0, 0);
    gxVPush(cx, cy, c, 0, 0); gxVPush(dx, dy, c, 0, 0);
}

/* Textured quad (used by putimage). u/v are normalised image coordinates. */
GX_INLINE void gxQuadTex(float x0, float y0, float x1, float y1,
                         float u0, float v0, float u1, float v1, COLORREF c) {
    gxQuadMode();
    gxVPush(x0, y0, c, u0, v0); gxVPush(x1, y0, c, u1, v0);
    gxVPush(x1, y1, c, u1, v1); gxVPush(x0, y1, c, u0, v1);
}

/*======================================================================
 *  7. Hatch brush patterns (8x8, alpha only, GL_REPEAT)
 *====================================================================*/
static bool gxHatchPixel(int style, int x, int y) {
    switch (style) {
    case HS_HORIZONTAL: return (y == 0) || (y == 4);
    case HS_VERTICAL:   return (x == 0) || (x == 4);
    case HS_FDIAGONAL:  return (((x + y) & 3) == 0);
    case HS_BDIAGONAL:  return (((x + 7 - y) & 3) == 0);
    case HS_CROSS:      return (y == 0) || (y == 4) || (x == 0) || (x == 4);
    case HS_DIAGCROSS:  return (((x + y) & 3) == 0) || (((x + 7 - y) & 3) == 0);
    default:            return false;
    }
}

static GLuint gxGetHatchTex(int hatch) {
    unsigned char px[8 * 8 * 4];
    int i;
    GLuint tex = 0;
    if (hatch < HS_HORIZONTAL || hatch > HS_DIAGCROSS) hatch = HS_HORIZONTAL;
    if (g_gx_hatchTex[hatch]) return g_gx_hatchTex[hatch];
    memset(px, 0, sizeof(px));
    for (i = 0; i < 64; i++) {
        int x = i % 8, y = i / 8;
        if (gxHatchPixel(hatch, x, y)) {
            px[i * 4 + 0] = 255; px[i * 4 + 1] = 255;
            px[i * 4 + 2] = 255; px[i * 4 + 3] = 255;
        }
    }
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 8, 8, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    g_gx_hatchTex[hatch] = tex;
    return tex;
}

/*======================================================================
 *  8. Glyph atlas (rasterised with GDI so fonts match EasyX exactly)
 *====================================================================*/
#define GX_ATLAS_W 2048
#define GX_ATLAS_H 2048
#define GX_ATLAS_BYTES ((size_t)GX_ATLAS_W * (size_t)GX_ATLAS_H * 4u)
#define GX_CP_MASK 0x1FFFFFull

static GxFontVec g_gx_fonts;
static unsigned char* g_gx_atlas = NULL;
static int  g_gx_packX = 0, g_gx_packY = 0, g_gx_packRowH = 0;
static HDC  g_gx_fontDC = NULL;
static LOGFONTA g_gx_font;                 /* current font (ANSI face name) */

typedef struct GxStrSlot { char* key; int val; int used; } GxStrSlot;
typedef struct GxStrMap  { GxStrSlot* slots; size_t cap; size_t count; } GxStrMap;
static GxStrMap g_gx_fontIds;

typedef struct GxGlyphSlot { unsigned long long key; Glyph val; int used; } GxGlyphSlot;
typedef struct GxGlyphMap  { GxGlyphSlot* slots; size_t cap; size_t count; } GxGlyphMap;
static GxGlyphMap g_gx_glyphs;

static void gxStrMapFree(GxStrMap* m) {
    size_t i;
    for (i = 0; i < m->cap; i++)
        if (m->slots[i].used) free(m->slots[i].key);
    free(m->slots);
    m->slots = NULL; m->cap = 0; m->count = 0;
}

static void gxStrMapGrow(GxStrMap* m, size_t newcap) {
    GxStrSlot* ns; GxStrSlot* old; size_t i, oldcap;
    if (newcap < 16) newcap = 16;
    ns = (GxStrSlot*)calloc(newcap, sizeof(GxStrSlot));
    if (!ns) { MessageBoxA(NULL, "Out of memory", "Error", MB_OK); exit(1); }
    old = m->slots; oldcap = m->cap;
    for (i = 0; i < oldcap; i++) {
        size_t j, k;
        if (!old[i].used) continue;
        j = gxHashStr(old[i].key) & (newcap - 1);
        for (k = 0; k < newcap; k++) {
            size_t idx = (j + k) & (newcap - 1);
            if (!ns[idx].used) { ns[idx].used = 1; ns[idx].key = old[i].key; ns[idx].val = old[i].val; break; }
        }
    }
    free(old);
    m->slots = ns; m->cap = newcap;
}

static bool gxStrMapGet(GxStrMap* m, const char* key, int* out) {
    size_t j, k;
    if (m->cap == 0) return false;
    j = gxHashStr(key) & (m->cap - 1);
    for (k = 0; k < m->cap; k++) {
        size_t idx = (j + k) & (m->cap - 1);
        if (!m->slots[idx].used) return false;
        if (strcmp(m->slots[idx].key, key) == 0) { if (out) *out = m->slots[idx].val; return true; }
    }
    return false;
}

static void gxStrMapSet(GxStrMap* m, const char* key, int val) {
    size_t j, k;
    if (m->cap == 0 || (m->count + 1) * 10 >= m->cap * 7)
        gxStrMapGrow(m, m->cap ? m->cap * 2 : 64);
    j = gxHashStr(key) & (m->cap - 1);
    for (k = 0; k < m->cap; k++) {
        size_t idx = (j + k) & (m->cap - 1);
        if (!m->slots[idx].used) {
            char* dup = (char*)malloc(strlen(key) + 1);
            if (!dup) { MessageBoxA(NULL, "Out of memory", "Error", MB_OK); exit(1); }
            strcpy(dup, key);
            m->slots[idx].used = 1; m->slots[idx].key = dup;
            m->slots[idx].val = val; m->count++;
            return;
        }
        if (strcmp(m->slots[idx].key, key) == 0) { m->slots[idx].val = val; return; }
    }
}

static void gxGlyphMapFree(GxGlyphMap* m) {
    free(m->slots);
    m->slots = NULL; m->cap = 0; m->count = 0;
}

static void gxGlyphMapClear(GxGlyphMap* m) {
    if (m->slots) memset(m->slots, 0, m->cap * sizeof(GxGlyphSlot));
    m->count = 0;
}

static void gxGlyphMapGrow(GxGlyphMap* m, size_t newcap) {
    GxGlyphSlot* ns; GxGlyphSlot* old; size_t i, oldcap;
    if (newcap < 256) newcap = 256;
    ns = (GxGlyphSlot*)calloc(newcap, sizeof(GxGlyphSlot));
    if (!ns) { MessageBoxA(NULL, "Out of memory", "Error", MB_OK); exit(1); }
    old = m->slots; oldcap = m->cap;
    for (i = 0; i < oldcap; i++) {
        size_t j, k;
        if (!old[i].used) continue;
        j = gxHashU64(old[i].key) & (newcap - 1);
        for (k = 0; k < newcap; k++) {
            size_t idx = (j + k) & (newcap - 1);
            if (!ns[idx].used) { ns[idx] = old[i]; break; }
        }
    }
    free(old);
    m->slots = ns; m->cap = newcap;
}

static Glyph* gxGlyphMapFind(GxGlyphMap* m, unsigned long long key) {
    size_t j, k;
    if (m->cap == 0) return NULL;
    j = gxHashU64(key) & (m->cap - 1);
    for (k = 0; k < m->cap; k++) {
        size_t idx = (j + k) & (m->cap - 1);
        if (!m->slots[idx].used) return NULL;
        if (m->slots[idx].key == key) return &m->slots[idx].val;
    }
    return NULL;
}

static void gxGlyphMapSet(GxGlyphMap* m, unsigned long long key, const Glyph* val) {
    size_t j, k;
    if (m->cap == 0 || (m->count + 1) * 10 >= m->cap * 7)
        gxGlyphMapGrow(m, m->cap ? m->cap * 2 : 1024);
    j = gxHashU64(key) & (m->cap - 1);
    for (k = 0; k < m->cap; k++) {
        size_t idx = (j + k) & (m->cap - 1);
        if (!m->slots[idx].used) {
            m->slots[idx].used = 1; m->slots[idx].key = key;
            m->slots[idx].val = *val; m->count++;
            return;
        }
        if (m->slots[idx].key == key) { m->slots[idx].val = *val; return; }
    }
}

static void gxInitFontDefault(void) {
    memset(&g_gx_font, 0, sizeof(g_gx_font));
    g_gx_font.lfHeight = 16;
    g_gx_font.lfWidth = 0;
    g_gx_font.lfWeight = FW_NORMAL;
    g_gx_font.lfCharSet = DEFAULT_CHARSET;
    g_gx_font.lfOutPrecision = OUT_DEFAULT_PRECIS;
    g_gx_font.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    g_gx_font.lfQuality = DEFAULT_QUALITY;
    g_gx_font.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
    strcpy(g_gx_font.lfFaceName, "System");
}

/*--------------------------- codepage helpers --------------------------*/
/* The code page used to turn the caller's char* strings (text passed to
 * outtextxyA() and friends, font face names, file paths) into the UTF-16
 * that Windows wants, and back again.
 *
 * It defaults to CP_ACP, the system ANSI code page - which is what an
 * ordinary EasyX program gets, and is the right thing for a Chinese
 * Windows where that is GBK.  setglcp() changes it, which is the only way
 * to draw text that is in some other encoding.
 *
 * Byte strings (char*) are decoded with this code page and nothing else -
 * there is no guessing.  A GBK string can be valid UTF-8 by accident: the
 * bytes CB AB D4 B2 D7 B6 are perfectly ordinary GBK text, yet they decode
 * cleanly as three UTF-8 two-byte sequences, so the old try-UTF-8-first
 * rule silently produced U+02CB U+0532 U+05D6 instead.  Text that happened
 * to form a legal UTF-8 stream was mangled while its neighbours were fine,
 * which is impossible to diagnose from the outside.
 *
 * So setglcp(936) makes GBK text work, and setglcp(CP_UTF8) makes UTF-8
 * text work.  Wide strings (wchar_t*) never reach this code at all - they
 * go straight to the W flavours - so L"..." is always unambiguous.
 *
 * Define GX_GUESS_UTF8 to put the old try-UTF-8-first behaviour back.
 * Define GX_DEFAULT_CODEPAGE to change the starting value. */
#ifndef GX_DEFAULT_CODEPAGE
#define GX_DEFAULT_CODEPAGE CP_ACP
#endif
static UINT g_gx_codePage = GX_DEFAULT_CODEPAGE;

GX_INLINE void setglcp(UINT cp) { g_gx_codePage = cp ? cp : CP_ACP; }
GX_INLINE UINT getglcp(void)    { return g_gx_codePage; }

static WCHAR* gxDupWideFromBytes(const char* s) {
    int n;
    WCHAR* out;
    if (!s) return NULL;
    /* Decode with the code page the program selected, which defaults to
     * the active ANSI code page (GBK, Big5, ...).  No UTF-8 attempt:
     * see the note at g_gx_codePage. */
#ifdef GX_GUESS_UTF8
    n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, NULL, 0);
    if (n <= 0)
#endif
        n = MultiByteToWideChar(g_gx_codePage, 0, s, -1, NULL, 0);
    if (n <= 0) { out = (WCHAR*)malloc(sizeof(WCHAR)); if (out) out[0] = 0; return out; }
    out = (WCHAR*)malloc((size_t)n * sizeof(WCHAR));
    if (!out) { MessageBoxA(NULL, "Out of memory", "Error", MB_OK); exit(1); }
#ifdef GX_GUESS_UTF8
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, out, n) <= 0)
#endif
    {
        if (MultiByteToWideChar(g_gx_codePage, 0, s, -1, out, n) <= 0) out[0] = 0;
    }
    return out;
}

static char* gxDupBytesFromWide(const WCHAR* w) {
    int n;
    char* out;
    if (!w) return NULL;
    n = WideCharToMultiByte(g_gx_codePage, 0, w, -1, NULL, 0, NULL, NULL);
    if (n <= 0) { out = (char*)malloc(1); if (out) out[0] = 0; return out; }
    out = (char*)malloc((size_t)n);
    if (!out) { MessageBoxA(NULL, "Out of memory", "Error", MB_OK); exit(1); }
    if (WideCharToMultiByte(g_gx_codePage, 0, w, -1, out, n, NULL, NULL) <= 0) out[0] = 0;
    return out;
}

/* Encode one Unicode code point as UTF-16 (returns 1 or 2 units). */
GX_INLINE int gxUtf16Encode(unsigned long cp, WCHAR out[2]) {
    if (cp > 0xFFFFul) {
        cp -= 0x10000ul;
        out[0] = (WCHAR)(0xD800u + (cp >> 10));
        out[1] = (WCHAR)(0xDC00u + (cp & 0x3FFu));
        return 2;
    }
    out[0] = (WCHAR)cp;
    out[1] = 0;
    return 1;
}

/* Decode the next code point of a UTF-16 string. */
GX_INLINE unsigned long gxUtf16Next(const WCHAR* s, int i, int len, int* adv) {
    unsigned long c = (unsigned long)(unsigned short)s[i];
    if (c >= 0xD800ul && c <= 0xDBFFul && i + 1 < len) {
        unsigned long c2 = (unsigned long)(unsigned short)s[i + 1];
        if (c2 >= 0xDC00ul && c2 <= 0xDFFFul) {
            *adv = 2;
            return 0x10000ul + ((c - 0xD800ul) << 10) + (c2 - 0xDC00ul);
        }
    }
    *adv = 1;
    return c;
}

/*------------------------ glyph rasteriser mode ------------------------*/
/* Declared here because gxGetFontId() below bakes it into the HFONT.
 * Only two behaviours exist; each is spelt three ways, so code can use
 * whichever term reads best at the call site.
 *
 *   GLF_UNLIMITED / GLF_FLOAT / GLF_SMOOTH   (0)
 *       GDI antialiases the glyph, the pen advances by the exact
 *       fractional amount, and quads may land between pixels.  Smooth
 *       motion, soft edges, correct at any setaspectratio().
 *
 *   GLF_INT / GLF_EASYX / GLF_BLOCKY         (1)   <-- the default
 *       GDI is asked for NONANTIALIASED_QUALITY and the coverage levels
 *       are snapped to 0 or 255, so every pixel of a glyph is either
 *       full ink or fully absent: text is drawn in exactly ONE colour,
 *       with no grey fringe along curves and diagonals.  Glyph origins
 *       and advances are additionally rounded to whole device pixels so
 *       the bitmap lands on the pixel grid 1:1.  This is what EasyX /
 *       GDI look like, hence the alias.
 *       Trade off: no sub-pixel motion, and spacing is quantised.
 *
 * "SMOOTH" and "BLOCKY" describe how it looks, "FLOAT" and "INT" how the
 * pen is aligned, "UNLIMITED" and "EASYX" where the behaviour comes from. */
#define GLF_UNLIMITED   0        /* the pen is not snapped            */
#define GLF_FLOAT       0        /* alias: same value, same behaviour */
#define GLF_SMOOTH      0        /* alias: antialiased, soft edges    */
#define GLF_INT         1        /* the pen snaps to device pixels    */
#define GLF_EASYX       1        /* alias: what GDI / EasyX look like */
#define GLF_BLOCKY      1        /* alias: hard edges, one colour     */

static int g_gx_fontMode = GLF_INT;

/*------------------------------ glyph baking ---------------------------*/
static unsigned char* gxEnsureAtlas(void) {
    if (!g_gx_atlas) {
        g_gx_atlas = (unsigned char*)malloc(GX_ATLAS_BYTES);
        if (!g_gx_atlas) { MessageBoxA(NULL, "Out of memory (glyph atlas)", "Error", MB_OK); exit(1); }
        memset(g_gx_atlas, 0, GX_ATLAS_BYTES);
    }
    return g_gx_atlas;
}

static void gxUploadAtlas(int x, int y, int w, int h, const unsigned char* src) {
    if (!g_gx_atlasTex) return;
    glBindTexture(GL_TEXTURE_2D, g_gx_atlasTex);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, src);
}

static void gxResetAtlas(void) {
    gxFlush();                 /* the atlas is dropped, so flush old draws */
    gxGlyphMapClear(&g_gx_glyphs);
    g_gx_packX = g_gx_packY = g_gx_packRowH = 0;
    memset(gxEnsureAtlas(), 0, GX_ATLAS_BYTES);
    if (g_gx_atlasTex) {
        glBindTexture(GL_TEXTURE_2D, g_gx_atlasTex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GX_ATLAS_W, GX_ATLAS_H, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, g_gx_atlas);
    }
}

GX_INLINE float gxInvScaleX(void) { return (g_gx_scaleX > 0.f) ? (1.f / g_gx_scaleX) : 1.f; }
GX_INLINE float gxInvScaleY(void) { return (g_gx_scaleY > 0.f) ? (1.f / g_gx_scaleY) : 1.f; }

/* Logical -> DEVICE pixel, the exact inverse used by the renderer:
 *   device = logical * scale + origin
 * The pixel that covers a logical point is floorf() of that value, and a
 * logical *rectangle* [a,b) covers the device pixels
 * [floorf(a*scale+origin), ceilf(b*scale+origin)).
 * (int) truncation is not usable here: it rounds -0.5 up to 0. */
GX_INLINE int gxLogToDevX(float x) {
    return (int)floorf(x * g_gx_scaleX + g_gx_originX);
}
GX_INLINE int gxLogToDevY(float y) {
    return (int)floorf(y * g_gx_scaleY + g_gx_originY);
}

GX_INLINE void settextscale(float s) { g_gx_textScale = (s > 0.05f && s < 20.f) ? s : 1.f; }
GX_INLINE float gettextscale(void) { return g_gx_textScale; }

static int gxGetFontId(const LOGFONTA* lf) {
    double sy = (g_gx_scaleY > 0.f) ? (double)g_gx_scaleY : 1.0;
    double want = fabs((double)lf->lfHeight) * sy * (double)g_gx_textScale + 0.5;
    int px, id;
    char key[352];
    FontRec rec;
    LOGFONTA lf2;
    if (want > 2147483000.0) want = 2147483000.0;
    px = (int)want;
    if (px < 1) px = 1;
    /* The renderer mode has to be part of the key: GLF_INT and
     * GLF_UNLIMITED bake different bitmaps, so they cannot share an HFONT.
     * Without this the cache returned the first one that was created and
     * setfontrenderer() appeared to do nothing at all. */
    sprintf(key, "%s|%d|%d|%d|%d|%d|%d|%d", lf->lfFaceName, px,
            (int)lf->lfWeight, lf->lfItalic ? 1 : 0, (int)lf->lfCharSet,
            lf->lfUnderline ? 1 : 0, lf->lfStrikeOut ? 1 : 0, g_gx_fontMode);
    if (gxStrMapGet(&g_gx_fontIds, key, &id)) return id;

    memset(&rec, 0, sizeof(rec));
    strcpy(rec.face, lf->lfFaceName);
    rec.pxH = px;
    rec.weight = (int)lf->lfWeight;
    rec.italic = lf->lfItalic ? true : false;
    rec.mode = g_gx_fontMode;

    lf2 = *lf;
    lf2.lfHeight = (lf->lfHeight >= 0) ? px : -px;   /* EasyX: positive = cell height */
    lf2.lfWidth = 0;
    lf2.lfEscapement = 0;        /* rotated text is not supported by the atlas */
    lf2.lfOrientation = 0;
    /* GLF_INT asks GDI for an unhinted, non antialiased raster: glyphs come
     * out as pure ink or pure background, so a stem or a curve is drawn in
     * exactly one colour with no grey pixels along the edges. */
    lf2.lfQuality = (g_gx_fontMode == GLF_INT) ? NONANTIALIASED_QUALITY
                                            : ANTIALIASED_QUALITY;
    rec.hfont = CreateFontIndirectA(&lf2);

    GxFontVec_pushv(&g_gx_fonts, rec);
    id = (int)g_gx_fonts.size - 1;
    gxStrMapSet(&g_gx_fontIds, key, id);
    return id;
}

static Glyph gxGetGlyph(int fid, unsigned long cp) {
    unsigned long long key = (((unsigned long long)fid) << 21) | (cp & GX_CP_MASK);
    Glyph* hit;
    Glyph gl;
    FontRec* fr;
    HGDIOBJ oldF, oldB, oldF2;
    TEXTMETRICW tm;
    ABC abc;
    SIZE sz;
    int adv, cw, chh, px, py, row, i, nlen, left;
    bool bin;
    unsigned char* rgba;
    const unsigned char* src;
    void* bits = NULL;
    BITMAPINFO bi;
    HBITMAP dib;
    HDC memdc;
    WCHAR buf[4];

    hit = gxGlyphMapFind(&g_gx_glyphs, key);
    if (hit) return *hit;

    if (!g_gx_fontDC) g_gx_fontDC = CreateCompatibleDC(NULL);
    fr = &g_gx_fonts.data[fid];
    oldF = SelectObject(g_gx_fontDC, fr->hfont);
    GetTextMetricsW(g_gx_fontDC, &tm);

    nlen = gxUtf16Encode(cp, buf);
    buf[nlen] = 0;
    left = 0;
    if (nlen == 1 && GetCharABCWidthsW(g_gx_fontDC, buf[0], buf[0], &abc)) {
        adv = abc.abcA + abc.abcB + abc.abcC;
        cw = abc.abcB + 4;
        left = abc.abcA;
    } else {
        GetTextExtentPoint32W(g_gx_fontDC, buf, nlen, &sz);
        adv = (int)sz.cx;
        cw = (int)sz.cx + 4;
    }
    chh = tm.tmHeight + 4;
    if (cw < 1) cw = 1;
    if (chh < 1) chh = 1;

    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = cw;
    bi.bmiHeader.biHeight = -chh;               /* top-down */
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    dib = CreateDIBSection(g_gx_fontDC, &bi, DIB_RGB_COLORS, &bits, NULL, 0);
    memdc = CreateCompatibleDC(g_gx_fontDC);
    oldB = SelectObject(memdc, dib);
    /* A memory DC starts with the default (raster) font, so the font has to
     * be selected again - otherwise the ink is rasterised at ~16 px while
     * cw/chh/adv come from the requested font. */
    oldF2 = SelectObject(memdc, fr->hfont);
    memset(bits, 0, (size_t)cw * (size_t)chh * 4);
    SetTextColor(memdc, RGB(255, 255, 255));
    SetBkColor(memdc, RGB(0, 0, 0));
    SetBkMode(memdc, OPAQUE);
    TextOutW(memdc, 2 - left, 2, buf, nlen);

    rgba = (unsigned char*)malloc((size_t)cw * (size_t)chh * 4);
    if (!rgba) { MessageBoxA(NULL, "Out of memory", "Error", MB_OK); exit(1); }
    src = (const unsigned char*)bits;
    /* GLF_INT wants one colour and nothing else: the coverage grey levels
     * GDI produces are snapped to fully on or fully off, so no pixel is
     * ever a blend of the text colour and the background.  Some drivers
     * still hand back a few intermediate levels even with
     * NONANTIALIASED_QUALITY, hence the threshold here as well. */
    bin = (fr->mode == GLF_INT);
    for (i = 0; i < cw * chh; i++) {
        int b = src[i * 4 + 0], gg = src[i * 4 + 1], r = src[i * 4 + 2];
        int a = (299 * r + 587 * gg + 114 * b) / 1000;
        if (bin)      a = (a >= 128) ? 255 : 0;
        else if (a > 255) a = 255;
        rgba[i * 4 + 0] = 255; rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255; rgba[i * 4 + 3] = (unsigned char)a;
    }
    SelectObject(memdc, oldF2);
    SelectObject(memdc, oldB);
    DeleteDC(memdc);
    DeleteObject(dib);
    SelectObject(g_gx_fontDC, oldF);

    gxEnsureAtlas();
    if (g_gx_packX + cw > GX_ATLAS_W) { g_gx_packX = 0; g_gx_packY += g_gx_packRowH; g_gx_packRowH = 0; }
    if (g_gx_packY + chh > GX_ATLAS_H) gxResetAtlas();
    px = g_gx_packX; py = g_gx_packY;
    g_gx_packX += cw;
    if (chh > g_gx_packRowH) g_gx_packRowH = chh;

    for (row = 0; row < chh; row++)
        memcpy(&g_gx_atlas[((py + row) * GX_ATLAS_W + px) * 4],
               &rgba[row * cw * 4], (size_t)cw * 4);
    gxUploadAtlas(px, py, cw, chh, rgba);
    free(rgba);

    gl.u0 = (float)px / (float)GX_ATLAS_W;
    gl.v0 = (float)py / (float)GX_ATLAS_H;
    gl.u1 = (float)(px + cw) / (float)GX_ATLAS_W;
    gl.v1 = (float)(py + chh) / (float)GX_ATLAS_H;
    gl.w = (float)cw;
    gl.h = (float)chh;
    gl.offX = (float)left - 2.f;
    gl.offY = -2.f;
    gl.adv = (float)adv;
    gxGlyphMapSet(&g_gx_glyphs, key, &gl);
    return gl;
}

/* Height of the current font, in LOGICAL units. */
static int gxFontHeightLogical(int fid) {
    TEXTMETRICW tm;
    HGDIOBJ oldF;
    if (!g_gx_fontDC) g_gx_fontDC = CreateCompatibleDC(NULL);
    oldF = SelectObject(g_gx_fontDC, g_gx_fonts.data[fid].hfont);
    GetTextMetricsW(g_gx_fontDC, &tm);
    SelectObject(g_gx_fontDC, oldF);
    return (int)((double)tm.tmHeight * gxInvScaleY() + 0.5);
}

/* Font output quality, used to switch GDI antialiasing off for GLF_INT.
 * A very old SDK may not carry NONANTIALIASED_QUALITY. */
#ifndef DEFAULT_QUALITY
#define DEFAULT_QUALITY 0
#endif
#ifndef DRAFT_QUALITY
#define DRAFT_QUALITY 1
#endif
#ifndef PROOF_QUALITY
#define PROOF_QUALITY 2
#endif
#ifndef NONANTIALIASED_QUALITY
#define NONANTIALIASED_QUALITY 3
#endif
#ifndef ANTIALIASED_QUALITY
#define ANTIALIASED_QUALITY 4
#endif

/*======================================================================
 *  8b. Font rasteriser mode
 *====================================================================*/
/* How glyph quads are placed:
 *
 *   GLF_UNLIMITED (default) - the pen advances by the exact fractional
 *       advance and quads land on fractional device pixels.  GL filters
 *       the atlas across the subpixel offset, so text moves smoothly and
 *       stays correct at any setaspectratio().  Slightly soft edges are
 *       the price - a glyph can straddle two pixel rows and each row gets
 *       a partial sample.
 *
 *   GLF_INT - every glyph origin is snapped to a whole DEVICE pixel and
 *       each advance is rounded to a whole device pixel, so the bitmap is
 *       blitted 1:1 onto the pixel grid.  This is what EasyX / GDI do,
 *       and it is the sharpest possible result: crisp stems, no filtering
 *       blur.  The trade off is that text can no longer move in
 *       sub-pixel steps, and with a fractional setaspectratio() the
 *       spacing is quantised, so long lines drift a little from the
 *       true typographic width.
 *
 * Note that snapping is done in DEVICE space (after the scale and origin
 * are applied), which is the only place a "pixel" is meaningful - the
 * same x in logical units is not a whole pixel at every scale. */
GX_INLINE void setfontmode(int mode) {
    g_gx_fontMode = (mode == GLF_INT) ? GLF_INT : GLF_UNLIMITED;
}
GX_INLINE int getfontmode(void) { return g_gx_fontMode; }

/*======================================================================
 *  9. Text API (A = byte string, W = UTF-16 string)
 *====================================================================*/
/* Measure a UTF-16 run. len < 0 -> NUL terminated. */
static void gxMeasureW(const WCHAR* w, int len, int* outW, int* outH) {
    int fid, i;
    float adv = 0.f;
    int total = (len < 0) ? (int)wcslen(w) : len;
    if (!w || total <= 0) { if (outW) *outW = 0; if (outH) *outH = 0; return; }
    fid = gxGetFontId(&g_gx_font);
    for (i = 0; i < total; ) {
        unsigned long cp;
        float a;
        int step;
        cp = gxUtf16Next(w, i, total, &step);
        a = gxGetGlyph(fid, cp).adv;
        /* GLF_INT rounds each advance the same way the drawing loop does,
         * so textwidth() agrees with where the last glyph actually ends. */
        adv += (g_gx_fontMode == GLF_INT) ? floorf(a + 0.5f) : a;
        i += step;
    }
    if (outW) *outW = (int)(adv * gxInvScaleX() + 0.5f);
    if (outH) *outH = gxFontHeightLogical(fid);
}

/* Draw a UTF-16 run with the current text color / background mode. */
static void gxDrawW(double x, double y, const WCHAR* w, int len) {
    int fid, i, total = (len < 0) ? (int)wcslen(w) : len;
    float invX, invY, penX, penY;
    float sx = 1.f, sy = 1.f, invXs = 1.f, invYs = 1.f;
    float devX = 0.f, devY = 0.f;
    int adv = 0, hgt = 0;
    if (!w || total <= 0 || !g_gx_glReady) return;
    gxMeasureW(w, len, &adv, &hgt);
    if (g_gx_bkMode == OPAQUE) {
        gxSetTex(0, 0);
        gxQuad((float)x, (float)y, (float)(x + adv), (float)(y + hgt), g_gx_bkColor);
    }
    fid = gxGetFontId(&g_gx_font);
    /* Bake every glyph first so the vertex run below is not interrupted. */
    for (i = 0; i < total; ) {
        int step;
        gxGetGlyph(fid, gxUtf16Next(w, i, total, &step));
        i += step;
    }
    gxSetTex(g_gx_atlasTex, 1);
    invX = gxInvScaleX();
    invY = gxInvScaleY();
    if (g_gx_fontMode == GLF_INT) {
        /* Accumulate the pen in DEVICE pixels.  gxInvScaleX/Y() fall back
         * to 1.f for a negative scale, which would break this mapping, so
         * the true reciprocal is used here: the projection really is
         * device = logical * scale + origin for any sign. */
        sx = (g_gx_scaleX != 0.f) ? g_gx_scaleX : 1.f;
        sy = (g_gx_scaleY != 0.f) ? g_gx_scaleY : 1.f;
        invXs = 1.f / sx;
        invYs = 1.f / sy;
        devX = floorf((float)x * sx + g_gx_originX);
        devY = floorf((float)y * sy + g_gx_originY);
    } else {
        penX = (float)x;
        penY = (float)y;
    }
    for (i = 0; i < total; ) {
        Glyph g;
        int step;
        g = gxGetGlyph(fid, gxUtf16Next(w, i, total, &step));
        if (g.w > 0.f && g.h > 0.f) {
            float x0, y0, x1, y1;
            COLORREF c = g_gx_textColor;
            if (g_gx_fontMode == GLF_INT) {
                /* device -> logical, so the projection lands the quad
                 * exactly back on the integer device pixel. */
                x0 = (devX + g.offX - g_gx_originX) * invXs;
                y0 = (devY + g.offY - g_gx_originY) * invYs;
                x1 = x0 + g.w * invXs;
                y1 = y0 + g.h * invYs;
            } else {
                x0 = penX + g.offX * invX;
                y0 = penY + g.offY * invY;
                x1 = x0 + g.w * invX;
                y1 = y0 + g.h * invY;
            }
            gxQuadTex(x0, y0, x1, y1, g.u0, g.v0, g.u1, g.v1, c);
        }
        if (g_gx_fontMode == GLF_INT) devX += floorf(g.adv + 0.5f);
        else                       penX += g.adv * invX;
        i += step;
    }
    gxSetTex(0, 0);
    gxCheckFlush();
}

/*------------------------- A / W implementations -----------------------*/
static int textwidthA(const char* str) {
    WCHAR* w; int r = 0;
    if (!str || !*str) return 0;
    w = gxDupWideFromBytes(str);
    gxMeasureW(w, -1, &r, NULL);
    free(w);
    return r;
}
static int textwidthW(const WCHAR* str) {
    int r = 0;
    if (!str || !*str) return 0;
    gxMeasureW(str, -1, &r, NULL);
    return r;
}
static int textheightA(const char* str) {
    WCHAR* w; int r = 0;
    if (!str) return 0;
    w = gxDupWideFromBytes(str);
    gxMeasureW(w, -1, NULL, &r);
    free(w);
    return r;
}
static int textheightW(const WCHAR* str) {
    int r = 0;
    if (!str) return 0;
    gxMeasureW(str, -1, NULL, &r);
    return r;
}

static void outtextxyA(double x, double y, const char* str) {
    WCHAR* w;
    if (!str || !*str) return;
    w = gxDupWideFromBytes(str);
    gxDrawW(x, y, w, -1);
    free(w);
}
static void outtextxyW(double x, double y, const WCHAR* str) {
    if (!str || !*str) return;
    gxDrawW(x, y, str, -1);
}

static void outtextA(const char* str) {
    WCHAR* w;
    int adv = 0;
    if (!str || !*str) return;
    w = gxDupWideFromBytes(str);
    gxDrawW(g_gx_curX, g_gx_curY, w, -1);
    gxMeasureW(w, -1, &adv, NULL);
    free(w);
    g_gx_curX += adv;
}
static void outtextW(const WCHAR* str) {
    int adv = 0;
    if (!str || !*str) return;
    gxDrawW(g_gx_curX, g_gx_curY, str, -1);
    gxMeasureW(str, -1, &adv, NULL);
    g_gx_curX += adv;
}

/*======================================================================
 * 10. Fill / line style state and primitive helpers
 *====================================================================*/
#ifndef BS_SOLID
#define BS_SOLID      0
#define BS_NULL       1
#define BS_HATCHED    2
#define BS_PATTERN    3
#define BS_DIBPATTERN 5
#endif
#ifndef HS_HORIZONTAL
#define HS_HORIZONTAL 0
#define HS_VERTICAL   1
#define HS_FDIAGONAL  2
#define HS_BDIAGONAL  3
#define HS_CROSS      4
#define HS_DIAGCROSS  5
#endif
#ifndef PS_SOLID
#define PS_SOLID       0
#define PS_DASH        1
#define PS_DOT         2
#define PS_DASHDOT     3
#define PS_DASHDOTDOT  4
#define PS_NULL        5
#define PS_USERSTYLE   7
#endif
#ifndef PS_STYLE_MASK
#define PS_STYLE_MASK  0x0000000FL
#endif
#ifndef PS_ENDCAP_MASK
#define PS_ENDCAP_MASK 0x00000F00L
#endif
#ifndef PS_JOIN_MASK
#define PS_JOIN_MASK   0x0000F000L
#endif
#ifndef ALTERNATE
#define ALTERNATE 1
#define WINDING   2
#endif
#ifndef TRANSPARENT
#define TRANSPARENT 1
#define OPAQUE      2
#endif
#ifndef R2_BLACK
#define R2_BLACK       1
#define R2_NOTMERGEPEN 2
#define R2_MASKNOTPEN  3
#define R2_NOTCOPYPEN  4
#define R2_MASKPENNOT  5
#define R2_NOT         6
#define R2_XORPEN      7
#define R2_NOTMASKPEN  8
#define R2_MASKPEN     9
#define R2_NOTXORPEN   10
#define R2_NOP         11
#define R2_MERGENOTPEN 12
#define R2_COPYPEN     13
#define R2_MERGEPENNOT 14
#define R2_MERGEPEN    15
#define R2_WHITE       16
#endif

/* Tile a brush pattern in LOGICAL space.
 *
 * The sampler works on vPos (the interpolated logical coordinate), so
 *   uv = vPos * (1 / patternSize) + origin / (patternSize * scale)
 * makes one tile cover patternSize logical units: the brush now follows
 * setorigin() and grows with setaspectratio() like every other geometry.
 * The y term is negated because logical y points down while the hatch
 * bitmap is stored the GDI way (first row on top).
 * The old code sampled gl_FragCoord (device pixels, y up), which anchored
 * the pattern in device space and ignored both. */
GX_INLINE void gxSetPatLogical(float patW, float patH, int tile) {
    if (!(patW > 0.f)) patW = 1.f;
    if (!(patH > 0.f)) patH = 1.f;
    gxSetPat(1.f / patW, -1.f / patH,
             g_gx_originX * gxInvScaleX() / patW,
             -g_gx_originY * gxInvScaleY() / patH, tile);
}

static bool gxBeginFill(void) {
    if (g_gx_fillStyle.style == BS_NULL) return false;
    if (g_gx_fillStyle.style == BS_HATCHED) {
        GLuint t = gxGetHatchTex((int)g_gx_fillStyle.hatch);
        gxSetPatLogical(8.f, 8.f, 0);      /* hatch textures are REPEAT */
        gxSetTex(t, 3);
    } else if ((g_gx_fillStyle.style == BS_PATTERN || g_gx_fillStyle.style == BS_DIBPATTERN)
               && gxImageOk(g_gx_fillStyle.ppattern)) {
        IMAGE* im = g_gx_fillStyle.ppattern;
        if (im->width > 0 && im->height > 0) {
            /* An IMAGE texture is CLAMP (putimage() needs that), so the
             * draw has to switch it to REPEAT and back. */
            gxSetPatLogical((float)im->width, (float)im->height, 1);
            gxSetTex(im->tex, 3);
        } else {
            gxSetTex(0, 0);
        }
    } else {
        gxSetTex(0, 0);
    }
    return true;
}

GX_INLINE void setfillcolor(COLORREF c) {
    g_gx_fillColor = c;   /* the style stays untouched, as in EasyX */
}
GX_INLINE void setlinecolor(COLORREF c) { g_gx_lineColor = c; }
/* Persistent alpha for everything that follows, 0..255.  This is the knob
 * behind the a-prefixed calls; they set it for one primitive and put back
 * whatever was there.  EasyX has no equivalent. */
GX_INLINE void setalpha(BYTE a) {
    /* a is a transparency: 0 = solid, 255 = invisible. */
    g_gx_alpha = (float)(255 - (int)a) / 255.f;
    gxSetAlpha(g_gx_alpha);
}
GX_INLINE BYTE getalpha(void) {
    /* g_gx_alpha is GL coverage; turn it back into the transparency that
     * setalpha() takes, so getalpha() round-trips setalpha() exactly. */
    int a = 255 - (int)(g_gx_alpha * 255.f + 0.5f);
    return (BYTE)((a < 0) ? 0 : ((a > 255) ? 255 : a));
}

GX_INLINE void settextcolor(COLORREF c) { g_gx_textColor = c; }
GX_INLINE void setbkcolor(COLORREF c)   { g_gx_bkColor = c; }
GX_INLINE void setbkmode(int mode)      { g_gx_bkMode = (mode == OPAQUE) ? OPAQUE : TRANSPARENT; }
GX_INLINE COLORREF getfillcolor(void)   { return g_gx_fillColor; }
GX_INLINE COLORREF getlinecolor(void)   { return g_gx_lineColor; }
GX_INLINE COLORREF gettextcolor(void)   { return g_gx_textColor; }
GX_INLINE COLORREF getbkcolor(void)     { return g_gx_bkColor; }
GX_INLINE int getbkmode(void)           { return g_gx_bkMode; }
GX_INLINE void setrop2(int rop) {
    g_gx_rop2 = gxRop2Fix(rop);
    gxSetRop(g_gx_rop2);   /* gxSetRop() closes the command that is still open */
}
GX_INLINE int getrop2(void)             { return g_gx_rop2; }
GX_INLINE void setpolyfillmode(int m)   { g_gx_polyMode = (m == WINDING) ? WINDING : ALTERNATE; }
GX_INLINE int getpolyfillmode(void)     { return g_gx_polyMode; }
GX_INLINE void setorigin(double x, double y) {
    /* Vertices are queued in logical space and the projection is uploaded
     * when the batch is drawn, so anything still queued has to go out with
     * the old origin. */
    gxFlush();
    /* Same parking as setaspectratio(): the origin is a canvas property and
     * an IMAGE stays at 1:1 with no offset.
     *
     * The offset is in the same 96 dpi units as every other coordinate, so
     * it takes the DPI factor too - otherwise setorigin(300, 200) would put
     * the drawing at a third of the way into a window that
     * setaspectratio() had just made 1.5x bigger. */
    g_gx_reqOriginX    = (float)x;
    g_gx_reqOriginY    = (float)y;
    g_gx_canvasOriginX = (float)x * g_gx_dpiFix;
    g_gx_canvasOriginY = (float)y * g_gx_dpiFix;
    if (!g_gx_workImg) {
        g_gx_originX = g_gx_canvasOriginX;
        g_gx_originY = g_gx_canvasOriginY;
    }
    gxUpdateProj();
    if (g_gx_glReady) {
        glUniformMatrix4fv(g_gx_uProj, 1, GL_FALSE, g_gx_proj);
        gxBindTarget();      /* re-apply the clip box for the new origin */
    }
}
/* EasyX: getorigin(int* x, int* y) reads the origin back.  Either pointer
 * may be NULL.  The value is in device pixels because that is the unit
 * setorigin() takes. */
/* Reads the EFFECTIVE offset back, i.e. the request times the DPI factor,
 * in device pixels - the same convention getaspectratio() uses.  To get
 * what was passed in, divide by gethighdpiscale(). */
GX_INLINE void getorigin(int* x, int* y) {
    if (x) *x = (int)g_gx_originX;
    if (y) *y = (int)g_gx_originY;
}
GX_INLINE void getfont(LOGFONTA* f) { if (f) *f = g_gx_font; }
GX_INLINE void setfont(const LOGFONTA* f) { if (f) g_gx_font = *f; }
GX_INLINE HWND GetHWnd(void) { return g_gx_hwnd; }

/*======================================================================
 *  6b. DPI and dialog owner
 *
 *  Everything that has to follow the system DPI goes through getdpi(), so
 *  there is one place to change if a finer source is ever wanted (per
 *  monitor DPI, for instance).
 *====================================================================*/
/* LOGPIXELSY is a wingdi.h constant; a slim SDK may not carry it and the
 * value is fixed by Windows, so it is filled in when missing. */
#ifndef LOGPIXELSY
#define LOGPIXELSY   90
#endif

/* The vertical DPI of the primary display: 96 at 100%, 144 at 150%,
 * 192 at 200%.  Only Y is read because Windows scales both axes by the
 * same factor.
 *
 * GetDeviceCaps() is called directly rather than GetDpiForWindow() /
 * GetDpiForSystem(): those two are Win10 1607+ and would need the
 * load-by-pointer dance, which buys nothing here. */
static int getdpi(void) {
    HDC dc;
    int dpi = 96;
    dc = GetDC(NULL);
    if (dc) {
        int v = GetDeviceCaps(dc, LOGPIXELSY);
        if (v > 0) dpi = v;
        ReleaseDC(NULL, dc);
    }
    return dpi;
}

/* Scale a 96 dpi measurement to the current DPI.  MulDiv() rounds, so
 * 20 -> 30 at 150% and 40 at 200%, keeping things on whole pixels. */
GX_INLINE int gxIbS(int v) { return MulDiv(v, getdpi(), 96); }

/* Screen scale estimate, wrapped for callers.
 *
 * This is NOT the same thing as getdpi() and the two are deliberately kept
 * apart:
 *
 *   getdpi()             -> the real DPI, 96 / 120 / 144 / 192.  The unit
 *                           every measurement in the library is scaled
 *                           with.  No side effects.
 *   getinitscreenscale() -> a rough multiplier derived from the screen
 *                           HEIGHT in pixels (SM_CYSCREEN / 1000), so
 *                           1080p gives 1.08 and 4K gives 2.16.  It guesses
 *                           at "how big is this display" and knows nothing
 *                           about the actual scaling setting, so on a 4K
 *                           laptop at 150% it says 2.16 where the true
 *                           factor is 1.5.  It also makes the process DPI
 *                           aware as a side effect.
 *
 * Kept separate so the two never get substituted for one another.
 *
 * Cached because of that side effect: SetProcessDPIAware() is process wide
 * and cannot be undone, so it is worth calling once rather than on every
 * enquiry.  The answer cannot change afterwards either. */
static float gxInitScreenScale(void) {
    HMODULE hUser = GetModuleHandleA("user32.dll");
    if (hUser) {
        typedef BOOL (WINAPI *PFN_SPDA)(void);
        PFN_SPDA pSetDPIAware = (PFN_SPDA)GetProcAddress(hUser, "SetProcessDPIAware");
        if (pSetDPIAware) pSetDPIAware();   /* Vista+; no-op on failure */
    }
    float m = (float)(GetSystemMetrics(SM_CYSCREEN) / 1000.0);
    if (!(m > 0.5f)) m = 1.f;              /* NaN / absurdly small -> 1.0 */
    if (m > 3.f)     m = 3.f;              /* cap: >3 would ask for a huge FBO */
    return m;
}

static float g_gx_screenScale = 0.f;

GX_INLINE float getinitscreenscale(void) {
    if (g_gx_screenScale == 0.f) g_gx_screenScale = gxInitScreenScale();
    return g_gx_screenScale;
}

/* The window a modal dialog should belong to.
 *
 *   1. the graphics window, when there is one - it is the program's main
 *      window, so the dialog comes up over it;
 *   2. otherwise the console, so a dialog raised before initgraph() (or
 *      after closegraph()) still has an owner instead of being able to
 *      appear behind whatever the user is looking at;
 *   3. otherwise NULL, which is what MessageBox() has always been given.
 *
 * An owner also disables the window behind it while the dialog is up,
 * which is the behaviour a modal dialog is supposed to have. */
static HWND gxDialogOwner(void) {
    HWND h = g_gx_hwnd;
    if (h) return h;
    h = gxConsoleWindow();
    if (h) return h;
    return NULL;
}
/* A negative aspect ratio flips an axis, so g_gx_logW / g_gx_logH are negative
 * while the logical window still spans that many units: report the size. */
GX_INLINE int getwidth(void)  { return (int)(fabsf(g_gx_logW) + 0.5f); }
GX_INLINE int getheight(void) { return (int)(fabsf(g_gx_logH) + 0.5f); }
GX_INLINE int getx(void) { return (int)g_gx_curX; }
GX_INLINE int gety(void) { return (int)g_gx_curY; }
GX_INLINE void moveto(double x, double y) { g_gx_curX = x; g_gx_curY = y; }
GX_INLINE void moverel(double dx, double dy) { g_gx_curX += dx; g_gx_curY += dy; }
GX_INLINE void setrendermode(int mode) { g_gx_renderMode = (mode == RENDER_AUTO) ? RENDER_AUTO : RENDER_MANUAL; }
GX_INLINE int getrendermode(void) { return g_gx_renderMode; }

static void getlinestyle(LINESTYLE* pstyle) {
    if (!pstyle) return;
    pstyle->style = g_gx_lineStyle.style;
    pstyle->thickness = g_gx_lineStyle.thickness;
    pstyle->puserstyle = g_gx_lineStyle.puserstyle;
    pstyle->userstylecount = g_gx_lineStyle.userstylecount;
}
static void getfillstyle(FILLSTYLE* pstyle) {
    if (!pstyle) return;
    pstyle->style = g_gx_fillStyle.style;
    pstyle->hatch = g_gx_fillStyle.hatch;
    pstyle->ppattern = g_gx_fillStyle.ppattern;
}

static void gxSetLineStyle1(int style) {
    g_gx_lineStyle.style = (DWORD)style;
    g_gx_lineStyle.thickness = 1;
    g_gx_lineStyle.puserstyle = NULL;
    g_gx_lineStyle.userstylecount = 0;
    g_gx_lineWidth = (int)g_gx_lineStyle.thickness;
    if (g_gx_lineWidth < 1) g_gx_lineWidth = 1;
}
static void gxSetLineStyle2(int style, int thickness) {
    g_gx_lineStyle.style = (DWORD)style;
    g_gx_lineStyle.thickness = (DWORD)thickness;
    g_gx_lineStyle.puserstyle = NULL;
    g_gx_lineStyle.userstylecount = 0;
    g_gx_lineWidth = thickness < 1 ? 1 : thickness;   /* GDI maps width 0 to 1 */
}
static void gxSetLineStyle3(int style, int thickness, const DWORD* puserstyle) {
    gxSetLineStyle2(style, thickness);
    g_gx_lineStyle.puserstyle = (DWORD*)puserstyle;
}
static void gxSetLineStyle4(int style, int thickness, const DWORD* puserstyle,
                            DWORD userstylecount) {
    gxSetLineStyle3(style, thickness, puserstyle);
    g_gx_lineStyle.userstylecount = userstylecount;
}
static void gxSetLineStylePtr(const LINESTYLE* p) {
    if (!p) return;
    g_gx_lineStyle = *p;
    g_gx_lineWidth = (int)p->thickness;
    if (g_gx_lineWidth < 1) g_gx_lineWidth = 1;
}
static void gxSetFillStyle1(int style) {
    switch (style) {
    case BS_SOLID: case BS_NULL: case BS_HATCHED:
    case BS_PATTERN: case BS_DIBPATTERN:
        g_gx_fillStyle.style = style;
        g_gx_fillStyle.hatch = 0;
        g_gx_fillStyle.ppattern = NULL;
        return;
    default:
        /* EasyX also allows setfillstyle(<colour>) as a shortcut. */
        setfillcolor((COLORREF)style);
        return;
    }
}
static void gxSetFillStyle2(int style, long hatch) {
    g_gx_fillStyle.style = style;
    g_gx_fillStyle.hatch = hatch;
    g_gx_fillStyle.ppattern = NULL;
}
static void gxSetFillStyle3(int style, long hatch, IMAGE* ppattern) {
    g_gx_fillStyle.style = style;
    g_gx_fillStyle.hatch = hatch;
    g_gx_fillStyle.ppattern = ppattern;
}
static void gxSetFillStylePtr(const FILLSTYLE* p) { if (p) g_gx_fillStyle = *p; }
static void gxSetFillStyleColor(COLORREF c) { setfillcolor(c); }

/*======================================================================
 * 11. Basic primitives
 *====================================================================*/
static void putpixel(int x, int y, COLORREF c) {
    gxSetTex(0, 0);
    gxQuad((float)x, (float)y, (float)(x + 1), (float)(y + 1), c);
    gxCheckFlush();
}

static COLORREF getpixel(int x, int y) {
    unsigned char px[4];
    int dx = gxLogToDevX((float)x);
    int dy = gxLogToDevY((float)y);
    if (!g_gx_glReady) return BLACK;
    gxFlush();
    glBindFramebuffer(GL_FRAMEBUFFER, gxReadFbo(g_gx_target->fbo));
    if (dx < 0 || dy < 0 || dx >= g_gx_target->w || dy >= g_gx_target->h) return BLACK;
    memset(px, 0, sizeof(px));
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(dx, g_gx_target->h - 1 - dy, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    return RGB(px[0], px[1], px[2]);
}

/* Segment count for a circle of radius r (LOGICAL units).  The tessellation
 * is decided by the radius on the SCREEN, so the scale has to be folded in:
 * at scale 3 a logical radius of 10 really is a 30 device pixel circle and
 * needs three times as many segments to stay smooth. */
static int gxNSeg(float r) {
    float s = (fabsf(g_gx_scaleX) > fabsf(g_gx_scaleY)) ? fabsf(g_gx_scaleX)
                                                  : fabsf(g_gx_scaleY);
    int n;
    if (!(s > 0.f)) s = 1.f;
    if (r < 0.f) r = -r;
    n = (int)(3.14159f * r * s);
    if (n < 12) n = 12;
    if (n > 240) n = 240;
    return n;
}

/* Solid disc (float radius, reused internally) */
static void gxDisc(float cx, float cy, float R, COLORREF c) {
    int n, i;
    float px, py;
    if (R <= 0.f) return;
    n = gxNSeg(R);
    px = cx + R; py = cy;
    for (i = 1; i <= n; i++) {
        float a = (float)i * 2.f * 3.14159265f / (float)n;
        float qx = cx + cosf(a) * R, qy = cy + sinf(a) * R;
        gxV(cx, cy, c, 0, 0);
        gxV(px, py, c, 0, 0);
        gxV(qx, qy, c, 0, 0);
        px = qx; py = qy;
    }
}
/* End cap styles, shared by strokepolyline() and by the thick pen used by
 * line().  Defined here (with a guard) because gxThickLine() below needs them
 * and the stroke section sits much further down. */
#ifndef GX_CAP_BUTT
#define GX_CAP_BUTT    0
#define GX_CAP_ROUND   1
#define GX_CAP_SQUARE  2
#endif
/* Defined here (not in the stroke section) because gxThickLine() below reads
 * it and C++ has no tentative definitions, so it cannot be forward declared.
 * Default ROUND matches the round caps a GDI geometric pen draws. */
static int g_gx_strokeCap = GX_CAP_ROUND;

/* Half of a disc, centred on a segment end: (dx,dy) is the unit vector that
 * points OUT of the end, so the arc covers only what sticks out past the flat
 * cross section.  The other half is already painted by the quad, and drawing
 * both blends a translucent pen twice over a whole disc at every endpoint,
 * which reads as a dark round blob capping each end of the line.
 * The two arc ends land exactly on the cross section of the quad, so the
 * pieces share an edge: no seam, and no pixel is covered twice. */
static void gxCapHalf(float cx, float cy, float R,
                      float dx, float dy, COLORREF c) {
    float nx = -dy, ny = dx;
    float px, py;
    int n, i;
    if (R <= 0.f) return;
    n = gxNSeg(R) / 2;
    if (n < 6) n = 6;
    /* a = +pi/2 -> centre + R*n, a = -pi/2 -> centre - R*n: the two ends of
     * the cross section.  a = 0 is the outermost point, centre + R*d. */
    px = cx + nx * R; py = cy + ny * R;
    for (i = 1; i <= n; i++) {
        float a  = 1.5707963f - 3.14159265f * (float)i / (float)n;
        float sa = sinf(a), ca = cosf(a);
        float qx = cx + R * (sa * nx + ca * dx);
        float qy = cy + R * (sa * ny + ca * dy);
        gxV(cx, cy, c, 0, 0);
        gxV(px, py, c, 0, 0);
        gxV(qx, qy, c, 0, 0);
        px = qx; py = qy;
    }
}


/*======================================================================
 * 11b. Dashed pens
 *
 * PS_DASH / PS_DOT / PS_DASHDOT / PS_DASHDOTDOT / PS_USERSTYLE used to be
 * stored and then ignored, so every dashed pen drew a solid line.
 * The dash lengths are the GDI "system" styles (same numbers OOXML calls
 * sysDash / sysDot / sysDashDot / sysDashDotDot), in units of one pen
 * width - so the pattern grows with the thickness argument of
 * setlinestyle(), like a geometric GDI pen.
 *====================================================================*/
#define GX_DASH_MAX 32                  /* GDI caps a user array at 16 */
static const DWORD g_gx_dashPresetDASH[]       = { 3, 1 };
static const DWORD g_gx_dashPresetDOT[]        = { 1, 1 };
static const DWORD g_gx_dashPresetDASHDOT[]    = { 3, 1, 1, 1 };
static const DWORD g_gx_dashPresetDASHDOTDOT[] = { 3, 1, 1, 1, 1, 1 };
static DWORD g_gx_dashBuf[GX_DASH_MAX];
static int   g_gx_dashIdx = 0;             /* index into the pattern        */
static float g_gx_dashPos = 0.f;           /* distance used inside that dash */

/* Active dash array, or NULL for a solid (or invisible) pen. */
static const DWORD* gxDashPattern(int* outCount) {
    const DWORD* p = NULL;
    int n = 0, i;
    DWORD sum = 0;
    switch ((int)(g_gx_lineStyle.style & PS_STYLE_MASK)) {
    case PS_DASH:
        p = g_gx_dashPresetDASH;       n = (int)(sizeof(g_gx_dashPresetDASH) / sizeof(DWORD));
        break;
    case PS_DOT:
        p = g_gx_dashPresetDOT;        n = (int)(sizeof(g_gx_dashPresetDOT) / sizeof(DWORD));
        break;
    case PS_DASHDOT:
        p = g_gx_dashPresetDASHDOT;    n = (int)(sizeof(g_gx_dashPresetDASHDOT) / sizeof(DWORD));
        break;
    case PS_DASHDOTDOT:
        p = g_gx_dashPresetDASHDOTDOT; n = (int)(sizeof(g_gx_dashPresetDASHDOTDOT) / sizeof(DWORD));
        break;
    case PS_USERSTYLE:
        if (g_gx_lineStyle.puserstyle && g_gx_lineStyle.userstylecount > 0) {
            n = (int)g_gx_lineStyle.userstylecount;
            if (n > 16) n = 16;
            for (i = 0; i < n; i++) g_gx_dashBuf[i] = g_gx_lineStyle.puserstyle[i];
            /* GDI: an odd count makes the pattern reverse when it wraps,
             * which is the same as simply repeating it twice. */
            if (n & 1) {
                for (i = 0; i < n; i++) g_gx_dashBuf[n + i] = g_gx_dashBuf[i];
                n *= 2;
            }
            p = g_gx_dashBuf;
        }
        break;
    default:
        break;                          /* PS_SOLID, PS_NULL, ... */
    }
    if (!p || n < 2) { *outCount = 0; return NULL; }
    for (i = 0; i < n; i++) sum += p[i];
    if (sum == 0) { *outCount = 0; return NULL; }   /* would never advance */
    *outCount = n;
    return p;
}

GX_INLINE bool gxDashOn(void) {
    int n = 0;
    return gxDashPattern(&n) != NULL;
}

/* Start a new figure: GDI restarts the dash phase for every figure and
 * keeps it running across the segments of that figure. */
GX_INLINE void gxDashReset(void) { g_gx_dashIdx = 0; g_gx_dashPos = 0.f; }

/* One flat capped segment, no end caps, no dashing. */
static void gxSegRaw(float x1, float y1, float x2, float y2, COLORREF c, float w) {
    float dx = x2 - x1, dy = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);
    float hw = w * 0.5f;
    float nx, ny;
    if (len < 0.0001f) {
        gxDisc(x1, y1, hw, c);
        return;
    }
    nx = -dy / len * hw; ny = dx / len * hw;
    gxQuad4(x1 + nx, y1 + ny, x2 + nx, y2 + ny,
            x2 - nx, y2 - ny, x1 - nx, y1 - ny, c);
}

/* Outline a whole path in ONE fill (see the stroke section).  Forward
 * declared here because rectangle()/roundrect()/polygon() below want it. */
static void gxStrokePathLine(const float* p, int n, bool closed);

/* Thick segment with flat caps (GDI pen semantics), honouring the dash. */
static void gxThickLine(float x1, float y1, float x2, float y2, COLORREF c,
                       float w, bool caps) {
    float dx = x2 - x1, dy = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);
    float hw = w * 0.5f;
    const DWORD* pat;
    int n;
    if (len < 0.0001f) {
        /* A degenerate segment only paints when the pen is not in a gap. */
        pat = gxDashPattern(&n);
        if (!pat || (g_gx_dashIdx & 1) == 0) gxDisc(x1, y1, hw, c);
        return;
    }
    pat = gxDashPattern(&n);
    if (!pat) {
        gxSegRaw(x1, y1, x2, y2, c, w);
        /* End caps follow setstrokecap().  None of the three overlaps the
         * ribbon: BUTT stops on the cross section, ROUND adds only the outer
         * half disc (see gxCapHalf()), SQUARE extends the ribbon by hw.  A
         * full disc would blend a translucent pen twice over every endpoint,
         * which reads as a dark round blob capping each end of the line. */
        if (caps && w >= 3.f) {
            float ux = dx / len, uy = dy / len;
            if (g_gx_strokeCap == GX_CAP_ROUND) {
                gxCapHalf(x1, y1, hw, -ux, -uy, c);
                gxCapHalf(x2, y2, hw,  ux,  uy, c);
            } else if (g_gx_strokeCap == GX_CAP_SQUARE) {
                /* A butt ribbon hw long, sharing the cross section edge. */
                gxSegRaw(x1 - ux * hw, y1 - uy * hw, x1, y1, c, w);
                gxSegRaw(x2, y2, x2 + ux * hw, y2 + uy * hw, c, w);
            }
            /* GX_CAP_BUTT: nothing past the cross section. */
        }
        return;
    }
    {
        float unit = (w > 1.f) ? w : 1.f;    /* dash length unit */
        float ux = dx / len, uy = dy / len;
        float t = 0.f;
        int guard = 0;
        while (t < len && guard++ < 200000) {
            int   on   = ((g_gx_dashIdx & 1) == 0);
            float want = (float)pat[g_gx_dashIdx] * unit;
            float left = want - g_gx_dashPos;
            float step;
            if (left <= 0.f) {
                /* Zero length entry: skip it, otherwise the loop stalls. */
                g_gx_dashPos = 0.f;
                g_gx_dashIdx = (g_gx_dashIdx + 1) % n;
                continue;
            }
            if (left > len - t) {
                step = len - t;
                g_gx_dashPos += step;
            } else {
                step = left;
                g_gx_dashPos = 0.f;
                g_gx_dashIdx = (g_gx_dashIdx + 1) % n;
            }
            if (on && step > 0.f)
                gxSegRaw(x1 + ux * t,         y1 + uy * t,
                         x1 + ux * (t + step), y1 + uy * (t + step), c, w);
            t += step;
        }
    }
}

static void line(double x1, double y1, double x2, double y2) {
    if (g_gx_lineStyle.style == PS_NULL) return;
    gxDashReset();
    gxSetTex(0, 0);
    gxThickLine((float)x1, (float)y1, (float)x2, (float)y2,
                g_gx_lineColor, (float)g_gx_lineWidth, true);
    g_gx_curX = x2; g_gx_curY = y2;
    gxCheckFlush();
}
static void lineto(double x, double y)  { line(g_gx_curX, g_gx_curY, x, y); }
static void linerel(double dx, double dy){ line(g_gx_curX, g_gx_curY, g_gx_curX + dx, g_gx_curY + dy); }

/* The three EasyX naming families:
 *   rectangle()      - outline only, in the line colour
 *   solidrectangle() - fill only, no outline, in the fill colour
 *   fillrectangle()  - fill AND outline (GDI calls this one "fill")
 * The fill* functions sit before the outline ones, hence these. */
static void rectangle(double l, double t, double r, double b);
static void circle(double x, double y, double r);
static void ellipse(double l, double t, double r, double b);
static void pie(double l, double t, double r, double b, double st, double en);
static void roundrect(double l, double t, double r, double b, double rw, double rh);

/* Filled rectangle: EasyX right/bottom coordinates are inclusive. */
static void solidrectangle(double l, double t, double r, double b) {
    double tmp;
    if (l > r) { tmp = l; l = r; r = tmp; }
    if (t > b) { tmp = t; t = b; b = tmp; }
    if (gxBeginFill())
        gxQuad((float)l, (float)t, (float)(r + 1), (float)(b + 1), g_gx_fillColor);
    gxCheckFlush();
}
/* Fill plus outline.  The outline goes on top, so it is not buried under
 * the fill - same order GDI uses, and the one fillpolygon() already had. */
static void fillrectangle(double l, double t, double r, double b) {
    solidrectangle(l, t, r, b);
    rectangle(l, t, r, b);
}

static void rectangle(double l, double t, double r, double b) {
    float w = (float)g_gx_lineWidth;
    COLORREF c = g_gx_lineColor;
    if (g_gx_lineStyle.style == PS_NULL) return;
    gxDashReset();
    gxSetTex(0, 0);
    {
        float p[8];
        p[0] = (float)l; p[1] = (float)t;
        p[2] = (float)r; p[3] = (float)t;
        p[4] = (float)r; p[5] = (float)b;
        p[6] = (float)l; p[7] = (float)b;
        gxStrokePathLine(p, 4, true);
    }
    gxCheckFlush();
}

static void solidcircle(double x, double y, double r) {
    if (r <= 0) return;
    if (gxBeginFill()) gxDisc(x + 0.5f, y + 0.5f, r + 0.5f, g_gx_fillColor);
    gxCheckFlush();
}
GX_INLINE void fillcircle(double x, double y, double r) {
    solidcircle(x, y, r);
    circle(x, y, r);
}

/* Defined below; a dashed circle is stroked through the arc routine. */
static void gxEllipseArc(double l, double t, double r, double b, double a0, double a1,
                         bool withSpokes);

static void circle(double x, double y, double r) {
    float cx, cy, R, hw, ri, ro, pi, pii, po, po2;
    int n, i;
    if (r <= 0 || g_gx_lineStyle.style == PS_NULL) return;
    /* The ring below cannot be dashed, so a dashed circle is stroked as a
     * closed polyline instead (same centre and radius). */
    if (gxDashOn()) {
        gxEllipseArc(x - r, y - r, x + r, y + r, 0.0, 6.28318530718, false);
        return;
    }
    gxSetTex(0, 0);
    cx = x + 0.5f; cy = y + 0.5f; R = r + 0.5f;
    hw = g_gx_lineWidth * 0.5f;
    ri = R - hw; ro = R + hw;
    if (ri < 0.f) ri = 0.f;
    n = gxNSeg(ro);
    pi = cx + ri; pii = cy; po = cx + ro; po2 = cy;
    for (i = 1; i <= n; i++) {
        float a = (float)i * 2.f * 3.14159265f / (float)n;
        float ca = cosf(a), sa = sinf(a);
        float qi = cx + ca * ri, qii = cy + sa * ri;
        float qo = cx + ca * ro, qo2 = cy + sa * ro;
        gxQuad4(pi, pii, po, po2, qo, qo2, qi, qii, g_gx_lineColor);
        pi = qi; pii = qii; po = qo; po2 = qo2;
    }
    gxCheckFlush();
}

/*--------------------------- ellipse helpers --------------------------*/
static void gxEllipsePts(float cx, float cy, float rx, float ry,
                         double a0, double a1, int n, float* outX, float* outY) {
    int i;
    double span = a1 - a0;
    for (i = 0; i <= n; i++) {
        double t = a0 + span * (double)i / (double)n;
        outX[i] = cx + (float)(cos(t) * rx);
        outY[i] = cy + (float)(sin(t) * ry);
    }
}

static void gxSolidEllipseSector(double l, double t, double r, double b,
                                 double a0, double a1, bool closed) {
    float cx, cy, rx, ry;
    int n, i;
    float px, py, qx, qy;
    if (gxBeginFill()) {
        if (l > r) { double tmp = l; l = r; r = tmp; }
        if (t > b) { double tmp = t; t = b; b = tmp; }
        cx = (l + r + 1) * 0.5f; cy = (t + b + 1) * 0.5f;
        rx = (r - l + 1) * 0.5f; ry = (b - t + 1) * 0.5f;
        if (rx <= 0.f || ry <= 0.f) { gxCheckFlush(); return; }
        if (a1 - a0 >= 6.28318530718) { a0 = 0.0; a1 = 6.28318530718; }
        n = (int)((a1 - a0) * (rx > ry ? rx : ry) * 0.5 + 8);
        if (n < 12) n = 12;
        if (n > 360) n = 360;
        px = cx + (float)(cos(a0) * rx); py = cy + (float)(sin(a0) * ry);
        for (i = 1; i <= n; i++) {
            double tt = a0 + (a1 - a0) * (double)i / (double)n;
            qx = cx + (float)(cos(tt) * rx); qy = cy + (float)(sin(tt) * ry);
            gxV(cx, cy, g_gx_fillColor, 0, 0);
            gxV(px, py, g_gx_fillColor, 0, 0);
            gxV(qx, qy, g_gx_fillColor, 0, 0);
            px = qx; py = qy;
        }
        if (closed && (a1 - a0) < 6.28318530718) {
            qx = cx + (float)(cos(a1) * rx); qy = cy + (float)(sin(a1) * ry);
            gxV(cx, cy, g_gx_fillColor, 0, 0);
            gxV(px, py, g_gx_fillColor, 0, 0);
            gxV(qx, qy, g_gx_fillColor, 0, 0);
        }
    }
    gxCheckFlush();
}

static void gxEllipseArc(double l, double t, double r, double b, double a0, double a1, bool withSpokes) {
    float cx, cy, rx, ry;
    float w = (float)g_gx_lineWidth;
    int n, i;
    float px, py, qx, qy;
    if (l > r) { double tmp = l; l = r; r = tmp; }
    if (t > b) { double tmp = t; t = b; b = tmp; }
    if (g_gx_lineStyle.style == PS_NULL) return;
    gxDashReset();
    gxSetTex(0, 0);
    cx = (l + r + 1) * 0.5f; cy = (t + b + 1) * 0.5f;
    rx = (r - l + 1) * 0.5f; ry = (b - t + 1) * 0.5f;
    if (rx <= 0.f || ry <= 0.f) return;
    if (a1 - a0 >= 6.28318530718) { a0 = 0.0; a1 = 6.28318530718; }
    n = (int)((a1 - a0) * (rx > ry ? rx : ry) * 0.5 + 8);
    if (n < 12) n = 12;
    if (n > 360) n = 360;
    px = cx + (float)(cos(a0) * rx); py = cy + (float)(sin(a0) * ry);
    for (i = 1; i <= n; i++) {
        double tt = a0 + (a1 - a0) * (double)i / (double)n;
        qx = cx + (float)(cos(tt) * rx); qy = cy + (float)(sin(tt) * ry);
        gxThickLine(px, py, qx, qy, g_gx_lineColor, w, false);
        px = qx; py = qy;
    }
    if (withSpokes && (a1 - a0) < 6.28318530718) {
        gxThickLine(cx, cy, cx + (float)(cos(a0) * rx), cy + (float)(sin(a0) * ry), g_gx_lineColor, w, true);
        gxThickLine(cx, cy, cx + (float)(cos(a1) * rx), cy + (float)(sin(a1) * ry), g_gx_lineColor, w, true);
    }
    gxCheckFlush();
}

GX_INLINE void ellipse(double l, double t, double r, double b) {
    gxEllipseArc(l, t, r, b, 0.0, 6.28318530718, false);
}
GX_INLINE void solidellipse(double l, double t, double r, double b) {
    gxSolidEllipseSector(l, t, r, b, 0.0, 6.28318530718, false);
}
GX_INLINE void fillellipse(double l, double t, double r, double b) {
    solidellipse(l, t, r, b);
    ellipse(l, t, r, b);
}
GX_INLINE void arc(double l, double t, double r, double b, double st, double en) {
    gxEllipseArc(l, t, r, b, st, en, false);
}
GX_INLINE void pie(double l, double t, double r, double b, double st, double en) {
    gxEllipseArc(l, t, r, b, st, en, true);
}
GX_INLINE void solidpie(double l, double t, double r, double b, double st, double en) {
    gxSolidEllipseSector(l, t, r, b, st, en, true);
}
GX_INLINE void fillpie(double l, double t, double r, double b, double st, double en) {
    solidpie(l, t, r, b, st, en);
    pie(l, t, r, b, st, en);
}

/*------------------------- rounded rectangles -------------------------*/
static void gxRoundRectPath(double l, double t, double r, double b, double rw, double rh,
                            float* ptsX, float* ptsY, int* outN) {
    float cx[4], cy[4];
    int n = 0, i, k;
    int seg = 8;
    if (rw <= 0 || rh <= 0) {
        ptsX[n] = (float)l;      ptsY[n++] = (float)t;
        ptsX[n] = (float)(r + 1); ptsY[n++] = (float)t;
        ptsX[n] = (float)(r + 1); ptsY[n++] = (float)(b + 1);
        ptsX[n] = (float)l;      ptsY[n++] = (float)(b + 1);
        *outN = n;
        return;
    }
    cx[0] = (float)(l + rw);      cy[0] = (float)(t + rh);
    cx[1] = (float)(r + 1 - rw);  cy[1] = (float)(t + rh);
    cx[2] = (float)(r + 1 - rw);  cy[2] = (float)(b + 1 - rh);
    cx[3] = (float)(l + rw);      cy[3] = (float)(b + 1 - rh);
    for (k = 0; k < 4; k++) {
        double a0, a1;
        float rx = (float)rw, ry = (float)rh;
        if (k == 0) { a0 = 3.14159265; a1 = 4.71238898; }
        else if (k == 1) { a0 = 4.71238898; a1 = 6.28318531; }
        else if (k == 2) { a0 = 0.0; a1 = 1.57079633; }
        else { a0 = 1.57079633; a1 = 3.14159265; }
        for (i = 0; i <= seg; i++) {
            double tt = a0 + (a1 - a0) * (double)i / (double)seg;
            ptsX[n] = cx[k] + (float)(cos(tt) * rx);
            ptsY[n] = cy[k] + (float)(sin(tt) * ry);
            n++;
        }
    }
    *outN = n;
}

static void gxRoundRect(double l, double t, double r, double b, double rw, double rh, bool filled) {
    float ptsX[64], ptsY[64];
    int n = 0, i;
    if (l > r) { double tmp = l; l = r; r = tmp; }
    if (t > b) { double tmp = t; t = b; b = tmp; }
    gxRoundRectPath(l, t, r, b, rw, rh, ptsX, ptsY, &n);
    if (n < 3) return;
    if (filled) {
        if (gxBeginFill()) {
            for (i = 1; i < n - 1; i++) {
                gxV(ptsX[0], ptsY[0], g_gx_fillColor, 0, 0);
                gxV(ptsX[i], ptsY[i], g_gx_fillColor, 0, 0);
                gxV(ptsX[i + 1], ptsY[i + 1], g_gx_fillColor, 0, 0);
            }
        }
    } else {
        if (g_gx_lineStyle.style != PS_NULL) {
            float w = (float)g_gx_lineWidth;
            gxDashReset();
            gxSetTex(0, 0);
            {
                float pp[128];
                for (i = 0; i < n && i < 64; i++) {
                    pp[i * 2]     = ptsX[i];
                    pp[i * 2 + 1] = ptsY[i];
                }
                gxStrokePathLine(pp, n, true);
            }
        }
    }
    gxCheckFlush();
}

GX_INLINE void roundrect(double l, double t, double r, double b, double rw, double rh) {
    gxRoundRect(l, t, r, b, rw, rh, false);
}
GX_INLINE void solidroundrect(double l, double t, double r, double b, double rw, double rh) {
    gxRoundRect(l, t, r, b, rw, rh, true);
}
GX_INLINE void fillroundrect(double l, double t, double r, double b, double rw, double rh) {
    solidroundrect(l, t, r, b, rw, rh);
    roundrect(l, t, r, b, rw, rh);
}

/*------------------------------ polygons ------------------------------*/
/* A floating point POINT.  Win32 has POINT (LONG), POINTS (SHORT) and
 * POINTFX (FIXED) but no plain float one, so it is defined here.
 *
 * The layout is what makes this cheap: two floats and no padding, so a
 * POINTF array IS an x0,y0,x1,y1,... float array, and can be handed to the
 * cores below with no conversion at all (that is what the assert checks).
 * polygonf() / solidpolygonf() / fillpolygonf() / polylinef() are the
 * POINTF forms of the EasyX functions. */
typedef struct tagPOINTF { float x; float y; } POINTF;

#if defined(__cplusplus)
#  define GX_STATIC_ASSERT(e, m) static_assert(e, m)
#else
#  define GX_STATIC_ASSERT(e, m) _Static_assert(e, m)   /* C11 */
#endif
GX_STATIC_ASSERT(sizeof(POINTF) == 2 * sizeof(float),
                 "POINTF must be two adjacent floats - the polygon cores "
                 "read an array of it as interleaved x,y floats");

/* How many floats of unpacking buffer fit on the stack (128 points).  A
 * bigger polygon falls back to malloc. */
#define GX_POLY_STACK_FLOATS  256

/* Scanline polygon fill.
 *
 * The old version fanned triangles from the centroid, which only works for
 * convex polygons: any concave vertex produced paint outside the shape (and
 * holes were filled too).  It also ignored setpolyfillmode() completely,
 * because g_gx_polyMode was never read.
 *
 * One scanline step is one DEVICE pixel expressed in logical units
 * (gxInvScaleY()), so the fill is exact at any scale.  Spans are built from
 * the sorted edge crossings, then combined with the even-odd rule
 * (ALTERNATE) or the non-zero winding rule (WINDING).
 *
 * The core takes an interleaved x0,y0,x1,y1,... float array rather than a
 * POINT array, so that both POINT (via an unpacking copy) and POINTF (via a
 * straight cast) can share it. */
static void gxPolyFillXY(const float* p, int n) {
    float ymin, ymax, span, step, yTop, yBot, yMid;
    float* xs;
    int*   dir;
    int i, rowN;
    if (p == NULL || n < 3) return;
    if (!gxBeginFill()) return;

    ymin = ymax = p[1];
    for (i = 1; i < n; i++) {
        float v = p[i * 2 + 1];
        if (v < ymin) ymin = v;
        if (v > ymax) ymax = v;
    }
    span = ymax - ymin;
    if (!(span > 0.f)) return;              /* flat polygon: no area */

    step = gxInvScaleY();                   /* 1 device pixel, logical units */
    if (!(step > 0.f)) step = span;
    if (step > span) step = span;
    if (span / step > 20000.f) step = span / 20000.f;   /* runaway guard */

    xs  = (float*)malloc(sizeof(float) * (size_t)n);
    dir = (int*)malloc(sizeof(int) * (size_t)n);
    if (!xs || !dir) { free(xs); free(dir); return; }

    rowN = 0;
    for (yTop = ymin; yTop < ymax; yTop += step) {
        int nx = 0, k;
        yBot = yTop + step;
        if (yBot > ymax) yBot = ymax;
        yMid = (yTop + yBot) * 0.5f;
        for (i = 0; i < n; i++) {
            int j = (i + 1) % n;
            float y1 = p[i * 2 + 1], y2 = p[j * 2 + 1];
            float x1 = p[i * 2],     x2 = p[j * 2];
            float lo, hi;
            if (y1 == y2) continue;                     /* horizontal edge */
            lo = (y1 < y2) ? y1 : y2;
            hi = (y1 < y2) ? y2 : y1;
            if (yMid < lo || yMid >= hi) continue;      /* half open in y */
            xs[nx] = x1 + (yMid - y1) / (y2 - y1) * (x2 - x1);
            dir[nx] = (y2 > y1) ? 1 : -1;
            nx++;
        }
        if (nx >= 2) {
            /* insertion sort on x (n is small, and it is stable enough) */
            for (i = 1; i < nx; i++) {
                float kx = xs[i];
                int   kd = dir[i];
                int   h  = i - 1;
                while (h >= 0 && xs[h] > kx) {
                    xs[h + 1] = xs[h]; dir[h + 1] = dir[h]; h--;
                }
                xs[h + 1] = kx; dir[h + 1] = kd;
            }
            if (g_gx_polyMode == WINDING) {
                int   wind = 0;
                float sx = 0.f;
                for (k = 0; k < nx; k++) {
                    int prev = wind;
                    wind += dir[k];
                    if (prev == 0 && wind != 0) {
                        sx = xs[k];
                    } else if (prev != 0 && wind == 0 && xs[k] > sx) {
                        gxQuad(sx, yTop, xs[k], yBot, g_gx_fillColor);
                    }
                }
            } else {                                    /* ALTERNATE */
                for (k = 0; k + 1 < nx; k += 2) {
                    if (xs[k + 1] > xs[k])
                        gxQuad(xs[k], yTop, xs[k + 1], yBot, g_gx_fillColor);
                }
            }
        }
        if (((++rowN) & 255) == 0 && g_gx_vbuf.size > 200000u) gxFlush();
    }
    free(xs);
    free(dir);
    gxCheckFlush();
}

static void gxPolyStrokeXY(const float* p, int n, bool closed) {
    float w = (float)g_gx_lineWidth;
    int i, lim = closed ? n : n - 1;
    if (g_gx_lineStyle.style == PS_NULL) return;
    gxDashReset();
    gxSetTex(0, 0);
    gxStrokePathLine(p, n, closed);
    gxCheckFlush();
}

/* POINT is {LONG x; LONG y;}, so it has to be unpacked into the interleaved
 * float form first.  That is one O(n) pass in front of an O(n * rows) fill,
 * so it costs nothing worth measuring.  *heap is what the caller must free
 * afterwards, and is NULL when the stack buffer was big enough. */
static float* gxPolyUnpack(const POINT* pts, int n, float* stack, float** heap) {
    float* p;
    int i;
    *heap = NULL;
    if (!pts || n < 2) return NULL;
    if ((size_t)n * 2u <= (size_t)GX_POLY_STACK_FLOATS) {
        p = stack;
    } else {
        p = (float*)malloc(sizeof(float) * (size_t)n * 2u);
        if (!p) return NULL;
        *heap = p;
    }
    for (i = 0; i < n; i++) {
        p[i * 2]     = (float)pts[i].x;
        p[i * 2 + 1] = (float)pts[i].y;
    }
    return p;
}

static void gxPolyFill(const POINT* pts, int n) {
    float stack[GX_POLY_STACK_FLOATS];
    float* heap;
    float* p = gxPolyUnpack(pts, n, stack, &heap);
    if (!p) return;
    gxPolyFillXY(p, n);
    free(heap);
}
static void gxPolyStroke(const POINT* pts, int n, bool closed) {
    float stack[GX_POLY_STACK_FLOATS];
    float* heap;
    float* p = gxPolyUnpack(pts, n, stack, &heap);
    if (!p) return;
    gxPolyStrokeXY(p, n, closed);
    free(heap);
}

/* The POINTF forms.  No unpacking: the array already is interleaved floats,
 * and each member is a float object accessed through a float lvalue, which
 * is exactly what the effective type rule allows. */
static void gxPolyFillF(const POINTF* pts, int n) {
    gxPolyFillXY(pts ? (const float*)pts : NULL, n);
}
static void gxPolyStrokeF(const POINTF* pts, int n, bool closed) {
    gxPolyStrokeXY(pts ? (const float*)pts : NULL, n, closed);
}

/* easygl extension (no EasyX equivalent): a triangle with its own colour
 * at each corner.  The shader already interpolates a per vertex colour, so
 * this costs three vertices - where painting the same gradient through
 * GetImageBuffer() costs a whole canvas read back plus upload. */
static void gradtriangle(double x0, double y0, COLORREF c0,
                         double x1, double y1, COLORREF c1,
                         double x2, double y2, COLORREF c2) {
    gxSetTex(0, 0);
    gxV((float)x0, (float)y0, c0, 0.f, 0.f);
    gxV((float)x1, (float)y1, c1, 0.f, 0.f);
    gxV((float)x2, (float)y2, c2, 0.f, 0.f);
    gxCheckFlush();
}

/* EasyX extension: a rectangle with its own colour at each corner, in the
 * order top left, top right, bottom right, bottom left.  Same route as
 * gradtriangle() - two triangles, four vertices, the GPU interpolates - so
 * a gradient background or a shaded panel costs one draw call instead of a
 * per pixel loop.
 * Passing the same colour twice gives a two tone (vertical / horizontal /
 * diagonal) gradient, and four equal colours are just a flat rectangle. */
static void gradrectangle(double l, double t, double r, double b,
                          COLORREF cTL, COLORREF cTR,
                          COLORREF cBR, COLORREF cBL) {
    double tmp;
    COLORREF s;
    if (l > r) { tmp = l; l = r; r = tmp; s = cTL; cTL = cTR; cTR = s;
                 s = cBL; cBL = cBR; cBR = s; }
    if (t > b) { tmp = t; t = b; b = tmp; s = cTL; cTL = cBL; cBL = s;
                 s = cTR; cTR = cBR; cBR = s; }
    gxSetTex(0, 0);
    /* EasyX right/bottom are inclusive, so the quad spans [l, r+1). */
    gxQuadMode();
    gxVPush((float)l,         (float)t,         cTL, 0.f, 0.f);
    gxVPush((float)(r + 1),   (float)t,         cTR, 0.f, 0.f);
    gxVPush((float)(r + 1),   (float)(b + 1),   cBR, 0.f, 0.f);
    gxVPush((float)l,         (float)(b + 1),   cBL, 0.f, 0.f);
    gxCheckFlush();
}

static void fillpolygon(const POINT* pts, int n) {
    if (pts == NULL || n < 3) return;
    gxPolyFill(pts, n);
    gxPolyStroke(pts, n, true);
}
static void solidpolygon(const POINT* pts, int n) {
    if (pts == NULL || n < 3) return;
    gxPolyFill(pts, n);
}
static void polygon(const POINT* pts, int n) {
    if (pts == NULL || n < 2) return;
    gxPolyStroke(pts, n, true);
}
static void polyline(const POINT* pts, int n) {
    if (pts == NULL || n < 2) return;
    gxPolyStroke(pts, n, false);
}

/* The same four with POINTF, for shapes that land between pixels (slow
 * smooth motion, fractional setaspectratio()).  Same rules as the POINT
 * family: polygonf() is the outline, solidpolygonf() the fill, and
 * fillpolygonf() both. */
static void fillpolygonf(const POINTF* pts, int n) {
    if (pts == NULL || n < 3) return;
    gxPolyFillF(pts, n);
    gxPolyStrokeF(pts, n, true);
}
static void solidpolygonf(const POINTF* pts, int n) {
    if (pts == NULL || n < 3) return;
    gxPolyFillF(pts, n);
}

/*------------------------------- triangles -----------------------------
 * triangle() / solidtriangle() / filltriangle() take the three corners
 * loose instead of through a POINT array, so a one off triangle needs no
 * array and no temporary.  The naming follows the rest of the library:
 *   triangle()       outline only
 *   solidtriangle()  fill only
 *   filltriangle()   fill, then outline
 *
 * solidtriangle() deliberately does NOT go through the scanline filler: a
 * triangle can never self intersect, so three plain vertices are enough.
 * It still goes through gxBeginFill(), so BS_NULL, BS_HATCHED and
 * BS_PATTERN behave exactly as they do for fillpolygon().  Degenerate input
 * (two coincident corners, or three collinear ones) covers no pixel, as in
 * EasyX. */
static void solidtriangle(double x1, double y1,
                          double x2, double y2,
                          double x3, double y3) {
    COLORREF c = g_gx_fillColor;
    if (!gxBeginFill()) return;
    gxV((float)x1, (float)y1, c, 0.f, 0.f);
    gxV((float)x2, (float)y2, c, 0.f, 0.f);
    gxV((float)x3, (float)y3, c, 0.f, 0.f);
    gxCheckFlush();
}
static void triangle(double x1, double y1,
                     double x2, double y2,
                     double x3, double y3) {
    POINTF p[3];
    p[0].x = (float)x1; p[0].y = (float)y1;
    p[1].x = (float)x2; p[1].y = (float)y2;
    p[2].x = (float)x3; p[2].y = (float)y3;
    gxPolyStrokeF(p, 3, true);
}
static void filltriangle(double x1, double y1,
                         double x2, double y2,
                         double x3, double y3) {
    solidtriangle(x1, y1, x2, y2, x3, y3);
    triangle(x1, y1, x2, y2, x3, y3);
}

/*--------------------------- stroked polyline ---------------------------
 * EasyX has no stroke primitive.  setlinestyle(PS_SOLID, thickness) draws
 * every segment as its own quad, so where the path bends the quads overlap
 * and a translucent colour gets blended twice there - which is exactly the
 * dark seam that showed up at every joint of a thick line.
 *
 * strokepolylinef() builds ONE closed outline for the whole ribbon: both
 * sides offset from the spine, plus the arcs that round off the end caps
 * and the outer side of every bend.  It is filled once, so each pixel of
 * the ribbon is covered exactly once and a translucent fill comes out with
 * an even alpha however sharply the path doubles back on itself.
 *
 * The outline self intersects whenever the path turns sharply, so it is
 * filled under the WINDING rule; under the default ALTERNATE a fold comes
 * out as a hole.  The caller's fill mode is put back afterwards.
 *
 * Caps and joins carry their usual stroke meaning:
 *   GX_CAP_BUTT    the ribbon ends on the cross section at the end point
 *   GX_CAP_ROUND   a half disc of radius width/2 is added
 *   GX_CAP_SQUARE  the ribbon runs width/2 past the end point
 *   GX_JOIN_BEVEL  the two offset sides are joined straight across
 *   GX_JOIN_ROUND  an arc of radius width/2 rounds off the outer side
 *   GX_JOIN_MITER  the two offset sides are extended to where they meet,
 *                  falling back to BEVEL past GX_STROKE_MITER_MAX
 * The defaults are ROUND/ROUND.  Width is the full width of the ribbon. */
#ifndef GX_CAP_BUTT
#define GX_CAP_BUTT    0
#define GX_CAP_ROUND   1
#define GX_CAP_SQUARE  2
#endif
#ifndef GX_JOIN_MITER
#define GX_JOIN_MITER  0
#define GX_JOIN_ROUND  1
#define GX_JOIN_BEVEL  2
#endif

#define GX_STROKE_CAP_SEGS   8    /* half disc: smooth enough at these radii */
#define GX_STROKE_JOIN_SEGS  12   /* upper bound on bend segments          */
#define GX_STROKE_JOIN_TOL   0.2f /* outer bend chord error, in pixels     */
#define GX_STROKE_FOLD_EPS   1e-4f /* |cross| below this with dot<0 = fold  */
#define GX_STROKE_MITER_MAX  4.f  /* a spike longer than this falls back to BEVEL */

/* g_gx_strokeCap is defined near gxThickLine() - see the note there. */
static int g_gx_strokeJoin = GX_JOIN_ROUND;

GX_INLINE void setstrokecap(int c) {
    g_gx_strokeCap = (c >= GX_CAP_BUTT && c <= GX_CAP_SQUARE) ? c : GX_CAP_ROUND;
}
GX_INLINE void setstrokejoin(int j) {
    g_gx_strokeJoin = (j >= GX_JOIN_MITER && j <= GX_JOIN_BEVEL) ? j : GX_JOIN_ROUND;
}
GX_INLINE int getstrokecap(void)  { return g_gx_strokeCap; }
GX_INLINE int getstrokejoin(void) { return g_gx_strokeJoin; }

/* Unit normal of one segment: the segment direction turned a quarter turn. */
static void gxStrokeNormal(float ax, float ay, float* mx, float* my) {
    float l = sqrtf(ax * ax + ay * ay);
    if (l < 1e-9f) { ax = 1.f; ay = 0.f; l = 1.f; }
    *mx = -ay / l;
    *my =  ax / l;
}
static void gxStrokePush(POINTF* o, int* n, int cap, float x, float y) {
    if (*n < cap) { o[*n].x = x; o[*n].y = y; (*n)++; }
}
/* How many segments an outer bend of |ad| radians needs at radius r so that
 * the chord error stays under GX_STROKE_JOIN_TOL pixels.
 *
 * A circular arc of half angle t drawn as one chord sags by r*(1-cos t), so
 * with k segments the sag is r*(1-cos(ad/2k)); solving that for the largest
 * allowed t gives the count.  The old code used a fixed ceiling of 4, which
 * left a crescent up to a pixel deep unfilled on sharp bends - visible as a
 * notch on every hard turn of a wide translucent ribbon. */
/* A path that doubles back on itself is a 180 degree turn.  For an exact
 * fold cr comes out as -0.0f, and -0.0f < 0.0f is false, so neither of the
 * two sign tests fired and the bend got no arc at all - the ribbon ended in
 * a flat chord and lost the half disc at the turn.  dot < 0 says the two
 * segments really do oppose, and that is the case to catch. */
static bool gxStrokeIsFold(float cr, float dot) {
    return (fabsf(cr) <= GX_STROKE_FOLD_EPS) && (dot < 0.f);
}

static int gxStrokeJoinSegs(float ad, float r) {
    float tol, half, k;
    int   s;
    if (!(ad > 0.f) || !(r > 0.f)) return 1;
    tol = GX_STROKE_JOIN_TOL / (r > 1.f ? r : 1.f);
    if (tol >= 1.f) return 1;                     /* bend smaller than a pixel */
    half = acosf(1.f - tol);                      /* max half angle per segment */
    if (!(half > 0.f)) return GX_STROKE_JOIN_SEGS;
    k = ad / (2.f * half);
    s = (int)ceilf(k);
    if (s < 1) s = 1;
    if (s > GX_STROKE_JOIN_SEGS) s = GX_STROKE_JOIN_SEGS;
    return s;
}

/* Interior points of an arc; the caller has already emitted both ends. */
static void gxStrokeArc(POINTF* o, int* n, int cap, float cx, float cy,
                        float r, float a0, float delta, int segs) {
    int k;
    for (k = 1; k < segs; k++) {
        float a = a0 + delta * (float)k / (float)segs;
        gxStrokePush(o, n, cap, cx + cosf(a) * r, cy + sinf(a) * r);
    }
}

/* Where the two offset sides of a bend meet when they are extended.  V is the
 * spine vertex; a spike longer than GX_STROKE_MITER_MAX is refused so the
 * caller falls back to BEVEL. */
static bool gxStrokeMiter(float ax, float ay, float d1x, float d1y,
                          float bx, float by, float d2x, float d2y,
                          float vx, float vy, float hw, float* ox, float* oy) {
    float den = d1x * d2y - d1y * d2x;
    float t, px, py, dx, dy, lim;
    if (fabsf(den) < 1e-6f) return false;
    t  = ((bx - ax) * d2y - (by - ay) * d2x) / den;
    px = ax + d1x * t;
    py = ay + d1y * t;
    dx = px - vx;
    dy = py - vy;
    lim = GX_STROKE_MITER_MAX * hw;
    if (dx * dx + dy * dy > lim * lim) return false;
    *ox = px;
    *oy = py;
    return true;
}

/* The outline of the ribbon: out along the +n side, round the tail, back
 * along the -n side, round the head.  Returns the number of points. */
static int gxStrokeOutline(const POINTF* sp, const POINTF* nm, int m,
                           float hw, POINTF* out, int ocap) {
    int n = 0, nseg = m - 1, i, j;
    float d0x, d0y, dlx, dly, l;

    d0x = sp[1].x - sp[0].x;  d0y = sp[1].y - sp[0].y;
    l = sqrtf(d0x * d0x + d0y * d0y);
    if (l < 1e-9f) { d0x = 1.f; d0y = 0.f; l = 1.f; }
    d0x /= l; d0y /= l;
    dlx = sp[m-1].x - sp[m-2].x;  dly = sp[m-1].y - sp[m-2].y;
    l = sqrtf(dlx * dlx + dly * dly);
    if (l < 1e-9f) { dlx = d0x; dly = d0y; l = 1.f; }
    dlx /= l; dly /= l;

    /* ---- +n side, head -> tail ---- */
    if (g_gx_strokeCap == GX_CAP_SQUARE)
        gxStrokePush(out, &n, ocap, sp[0].x + nm[0].x * hw - d0x * hw,
                                    sp[0].y + nm[0].y * hw - d0y * hw);
    gxStrokePush(out, &n, ocap, sp[0].x + nm[0].x * hw, sp[0].y + nm[0].y * hw);

    for (i = 0; i + 1 < nseg; i++) {
        float m1x = nm[i].x,     m1y = nm[i].y;
        float m2x = nm[i+1].x,   m2y = nm[i+1].y;
        float d1x = sp[i+1].x - sp[i].x,     d1y = sp[i+1].y - sp[i].y;
        float d2x = sp[i+2].x - sp[i+1].x,   d2y = sp[i+2].y - sp[i+1].y;
        float l1 = sqrtf(d1x*d1x + d1y*d1y), l2 = sqrtf(d2x*d2x + d2y*d2y);
        float cr, dot, delta, ad, a0;
        int   segs = 1;
        if (l1 < 1e-9f) { d1x = 1.f; d1y = 0.f; l1 = 1.f; }
        if (l2 < 1e-9f) { d2x = 1.f; d2y = 0.f; l2 = 1.f; }
        d1x /= l1; d1y /= l1; d2x /= l2; d2y /= l2;
        cr  = d1x * d2y - d1y * d2x;
        dot = d1x * d2x + d1y * d2y;
        delta = atan2f(cr, dot);
        ad = fabsf(delta);
        if (cr < -GX_STROKE_FOLD_EPS || gxStrokeIsFold(cr, dot)) {
                                              /* +n is the outer side here */
            segs = gxStrokeJoinSegs(ad, hw);
        }
        gxStrokePush(out, &n, ocap, sp[i+1].x + m1x * hw, sp[i+1].y + m1y * hw);
        if (cr < -GX_STROKE_FOLD_EPS || gxStrokeIsFold(cr, dot)) {
            if (g_gx_strokeJoin == GX_JOIN_ROUND) {
                a0 = atan2f(m1y, m1x);
                gxStrokeArc(out, &n, ocap, sp[i+1].x, sp[i+1].y, hw, a0, delta, segs);
            } else if (g_gx_strokeJoin == GX_JOIN_MITER) {
                float ox, oy;
                if (gxStrokeMiter(sp[i].x   + m1x*hw, sp[i].y   + m1y*hw, d1x, d1y,
                                  sp[i+2].x + m2x*hw, sp[i+2].y + m2y*hw, d2x, d2y,
                                  sp[i+1].x, sp[i+1].y, hw, &ox, &oy))
                    gxStrokePush(out, &n, ocap, ox, oy);
            }
        }
        gxStrokePush(out, &n, ocap, sp[i+1].x + m2x * hw, sp[i+1].y + m2y * hw);
    }
    gxStrokePush(out, &n, ocap, sp[m-1].x + nm[nseg-1].x * hw,
                              sp[m-1].y + nm[nseg-1].y * hw);

    /* ---- round the tail ---- */
    if (g_gx_strokeCap == GX_CAP_ROUND) {
        gxStrokeArc(out, &n, ocap, sp[m-1].x, sp[m-1].y, hw,
                    atan2f(nm[nseg-1].y, nm[nseg-1].x), -3.14159265f, GX_STROKE_CAP_SEGS);
    } else if (g_gx_strokeCap == GX_CAP_SQUARE) {
        gxStrokePush(out, &n, ocap, sp[m-1].x + nm[nseg-1].x*hw + dlx*hw,
                                    sp[m-1].y + nm[nseg-1].y*hw + dly*hw);
        gxStrokePush(out, &n, ocap, sp[m-1].x - nm[nseg-1].x*hw + dlx*hw,
                                    sp[m-1].y - nm[nseg-1].y*hw + dly*hw);
    }
    gxStrokePush(out, &n, ocap, sp[m-1].x - nm[nseg-1].x * hw,
                              sp[m-1].y - nm[nseg-1].y * hw);

    /* ---- -n side, tail -> head ---- */
    for (i = nseg - 2; i >= 0; i--) {
        float m1x = nm[i].x,     m1y = nm[i].y;
        float m2x = nm[i+1].x,   m2y = nm[i+1].y;
        float d1x = sp[i+1].x - sp[i].x,     d1y = sp[i+1].y - sp[i].y;
        float d2x = sp[i+2].x - sp[i+1].x,   d2y = sp[i+2].y - sp[i+1].y;
        float l1 = sqrtf(d1x*d1x + d1y*d1y), l2 = sqrtf(d2x*d2x + d2y*d2y);
        float cr, dot, delta, ad, a0;
        int   segs = 1;
        if (l1 < 1e-9f) { d1x = 1.f; d1y = 0.f; l1 = 1.f; }
        if (l2 < 1e-9f) { d2x = 1.f; d2y = 0.f; l2 = 1.f; }
        d1x /= l1; d1y /= l1; d2x /= l2; d2y /= l2;
        cr  = d1x * d2y - d1y * d2x;
        dot = d1x * d2x + d1y * d2y;
        delta = atan2f(cr, dot);
        ad = fabsf(delta);
        if (cr > GX_STROKE_FOLD_EPS) {        /* -n is the outer side here */
            segs = gxStrokeJoinSegs(ad, hw);
        }
        gxStrokePush(out, &n, ocap, sp[i+1].x - m2x * hw, sp[i+1].y - m2y * hw);
        if (cr > GX_STROKE_FOLD_EPS) {
            if (g_gx_strokeJoin == GX_JOIN_ROUND) {
                a0 = atan2f(-m1y, -m1x);
                for (j = segs - 1; j >= 1; j--) {        /* walking backwards */
                    float a = a0 + delta * (float)j / (float)segs;
                    gxStrokePush(out, &n, ocap, sp[i+1].x + cosf(a)*hw,
                                                sp[i+1].y + sinf(a)*hw);
                }
            } else if (g_gx_strokeJoin == GX_JOIN_MITER) {
                float ox, oy;
                if (gxStrokeMiter(sp[i+2].x - m2x*hw, sp[i+2].y - m2y*hw, -d2x, -d2y,
                                  sp[i].x   - m1x*hw, sp[i].y   - m1y*hw, -d1x, -d1y,
                                  sp[i+1].x, sp[i+1].y, hw, &ox, &oy))
                    gxStrokePush(out, &n, ocap, ox, oy);
            }
        }
        gxStrokePush(out, &n, ocap, sp[i+1].x - m1x * hw, sp[i+1].y - m1y * hw);
    }
    gxStrokePush(out, &n, ocap, sp[0].x - nm[0].x * hw, sp[0].y - nm[0].y * hw);

    /* ---- round the head ---- */
    if (g_gx_strokeCap == GX_CAP_ROUND) {
        gxStrokeArc(out, &n, ocap, sp[0].x, sp[0].y, hw,
                    atan2f(-nm[0].y, -nm[0].x), -3.14159265f, GX_STROKE_CAP_SEGS);
    } else if (g_gx_strokeCap == GX_CAP_SQUARE) {
        gxStrokePush(out, &n, ocap, sp[0].x - nm[0].x*hw - d0x*hw,
                                    sp[0].y - nm[0].y*hw - d0y*hw);
        gxStrokePush(out, &n, ocap, sp[0].x + nm[0].x*hw - d0x*hw,
                                    sp[0].y + nm[0].y*hw - d0y*hw);
    }
    return n;
}

/*--------------------------- stroke family ---------------------------
 * The stroke functions paint a RIBBON of the given width along a path, in
 * ONE fill, with the end caps and the corner joins that setstrokecap()
 * and setstrokejoin() select:
 *
 *   strokepolyline(pts, n [, w])       open ribbon, POINT[]
 *   strokepolylinef(pts, n [, w])      open ribbon, POINTF[]
 *   strokepolygon(pts, n [, w])        closed loop, POINT[]
 *   strokepolygonf(pts, n [, w])       closed loop, POINTF[]
 *   fillstrokepolygon(pts, n [, w])    filled and then rimmed, POINT[]
 *   fillstrokepolygonf(pts, n [, w])   filled and then rimmed, POINTF[]
 *
 * Why the width is an argument instead of "whatever setlinestyle() says":
 * a ribbon and a pen are not the same thing.  setlinestyle() also carries
 * the DASH pattern, and stroke*() has no dashes at all - it always paints
 * one solid ribbon, which is the entire point of it (see the note at the
 * top of this section).  So the pen supplies the DEFAULT width - its
 * thickness, i.e. what setlinestyle(PS_SOLID, n) set - and the last
 * argument overrides it.  These two are the same shape at the same width:
 *
 *   setlinestyle(PS_SOLID, 26);
 *   strokepolygon(p, 5);          /  strokepolygon(p, 5, 26.0);
 *
 * Every one of them paints with the LINE colour (setlinecolor()), because
 * a ribbon is a STROKE, exactly like polygon() / polyline() do.  The fill
 * colour is only used by fillstrokepolygon(), and only for its inner part.
 * A closed ribbon gets a join at the closing corner and NO end caps.
 */

/* Build and fill the ribbon along pts[0..n-1]; closed comes back to the
 * first point.  Repeated points are dropped: a zero length segment has no
 * normal and the geometry above divides by the length. */
static void gxStrokeRibbon(const POINTF* pts, int n, double width, bool closed) {
    POINTF  *sp, *nm, *out;
    float    hw, eps;
    int      m, i, nseg, cnt, ocap, oldFill, oldCap, oldStyle;
    COLORREF oldCol;

    if (pts == NULL || n < 2 || !(width > 0.0)) return;
    hw = (float)(width * 0.5);
    if (!(hw > 0.f)) return;
    eps = (hw > 1.f ? hw : 1.f) * 1e-4f;

    /* n + 3: a closed path appends two points (see below). */
    sp = (POINTF*)malloc((size_t)(n + 3) * sizeof(POINTF));
    if (!sp) return;
    m = 0;
    for (i = 0; i < n; i++) {
        if (m > 0 && fabsf(pts[i].x - sp[m-1].x) < eps &&
                     fabsf(pts[i].y - sp[m-1].y) < eps) continue;
        sp[m++] = pts[i];
    }
    if (closed) {
        /* An explicit closing point repeats the first one - drop it. */
        if (m > 2 && fabsf(sp[m-1].x - sp[0].x) < eps &&
                     fabsf(sp[m-1].y - sp[0].y) < eps) m--;
        if (m < 3) { free(sp); return; }
        /* TWO extra points, not one.  Closing with just sp[0] leaves the
         * corner at pts[0] as the junction of the head and the tail, and
         * gxStrokeOutline() only rounds the corners that sit strictly inside
         * the point list - so a closed figure came out with one sharp,
         * unjoined corner while every other one was round.  Carrying the
         * path one segment past the start turns pts[0] into an interior
         * corner like all the others; the two flat BUTT ends then land on
         * pts[0] and pts[1], which are themselves rounded joins, so nothing
         * is left open.
         * The first segment is therefore walked twice.  The outline is one
         * polygon filled once under WINDING, so the overlap is covered once
         * and a translucent ribbon keeps an even tone there. */
        sp[m++] = sp[0];
        sp[m++] = sp[1];
    }
    if (m < 2) { free(sp); return; }

    nseg = m - 1;
    nm = (POINTF*)malloc((size_t)nseg * sizeof(POINTF));
    if (!nm) { free(sp); return; }
    for (i = 0; i < nseg; i++)
        gxStrokeNormal(sp[i+1].x - sp[i].x, sp[i+1].y - sp[i].y, &nm[i].x, &nm[i].y);

    ocap = 2 * m + 2 * (m - 1) * GX_STROKE_JOIN_SEGS + 2 * (GX_STROKE_CAP_SEGS + 2) + 8;
    out = (POINTF*)malloc((size_t)ocap * sizeof(POINTF));
    if (!out) { free(sp); free(nm); return; }

    oldCap = g_gx_strokeCap;
    if (closed) g_gx_strokeCap = GX_CAP_BUTT;
    cnt = gxStrokeOutline(sp, nm, m, hw, out, ocap);
    g_gx_strokeCap = oldCap;

    if (cnt >= 3) {
        oldFill  = getpolyfillmode();
        oldStyle = g_gx_fillStyle.style;
        oldCol   = g_gx_fillColor;
        g_gx_fillStyle.style = BS_SOLID;      /* the outline is always solid */
        g_gx_fillColor = g_gx_lineColor;         /* a ribbon is a STROKE */
        setpolyfillmode(WINDING);
        gxPolyFillF(out, cnt);
        setpolyfillmode(oldFill);
        g_gx_fillStyle.style = oldStyle;
        g_gx_fillColor = oldCol;
    }
    free(sp); free(nm); free(out);
}

/* The POINT spelling: POINT[] is unpacked into POINTF[] and handed to the
 * core above, so the two spellings cannot drift apart. */
static void gxStrokeRibbonP(const POINT* pts, int n, double width, bool closed) {
    POINTF* f;
    int     i;
    if (pts == NULL || n < 2) return;
    f = (POINTF*)malloc((size_t)n * sizeof(POINTF));
    if (!f) return;
    for (i = 0; i < n; i++) { f[i].x = (float)pts[i].x; f[i].y = (float)pts[i].y; }
    gxStrokeRibbon(f, n, width, closed);
    free(f);
}

/* The arity forms.  User code calls the name without a suffix; the _2
 * forms take the width from the pen, the _3 forms from the caller.  C++
 * picks by overload and C picks by counting the arguments. */
static void gx_spl_2(const POINT* p, int n)             { gxStrokeRibbonP(p, n, (double)g_gx_lineWidth, false); }
static void gx_spl_3(const POINT* p, int n, double w)   { gxStrokeRibbonP(p, n, w, false); }
static void gx_splf_2(const POINTF* p, int n)           { gxStrokeRibbon(p, n, (double)g_gx_lineWidth, false); }
static void gx_splf_3(const POINTF* p, int n, double w) { gxStrokeRibbon(p, n, w, false); }

static void gx_spg_2(const POINT* p, int n)             { gxStrokeRibbonP(p, n, (double)g_gx_lineWidth, true); }
static void gx_spg_3(const POINT* p, int n, double w)   { gxStrokeRibbonP(p, n, w, true); }
static void gx_spgf_2(const POINTF* p, int n)           { gxStrokeRibbon(p, n, (double)g_gx_lineWidth, true); }
static void gx_spgf_3(const POINTF* p, int n, double w) { gxStrokeRibbon(p, n, w, true); }

/* Fill and then rim: the inner part takes the FILL colour and the rim the
 * LINE colour, so this one fills the polygon with setfillcolor() and then
 * rims it with setlinecolor().  The ribbon is centred on the path, so its
 * inner half lies over the fill: with an opaque colour that is invisible, with a
 * translucent one those pixels are covered twice and come out stronger.
 * Call solidpolygon() and strokepolygon() separately if that matters. */
static void gx_fspg_2(const POINT* p, int n)            { solidpolygon(p, n);  gxStrokeRibbonP(p, n, (double)g_gx_lineWidth, true); }
static void gx_fspg_3(const POINT* p, int n, double w)  { solidpolygon(p, n);  gxStrokeRibbonP(p, n, w, true); }
static void gx_fspgf_2(const POINTF* p, int n)          { solidpolygonf(p, n); gxStrokeRibbon(p, n, (double)g_gx_lineWidth, true); }
static void gx_fspgf_3(const POINTF* p, int n, double w){ solidpolygonf(p, n); gxStrokeRibbon(p, n, w, true); }

/* Outline a whole path in ONE fill, in the LINE colour, with no per segment
 * end caps.  This is what rectangle(), roundrect() and polygon() use now:
 * they used to draw one thick segment per side, and every segment added its
 * own round cap, so a translucent pen blended twice over each corner and
 * every joint along a flattened curve - which read as dark round blobs on
 * top of the line.  A dashed pen still walks segment by segment (the dash
 * phase needs that) and those segments are flat capped, for the same
 * reason.  Closed paths get a join at the closing point, never a cap. */
static void gxStrokePathLine(const float* p, int n, bool closed) {
    float    w = (float)g_gx_lineWidth;
    POINTF  *sp, *nm, *out;
    float    hw, eps;
    int      i, m, nseg, cnt, ocap;
    int      oldFill, oldStyle, oldCap;
    COLORREF oldCol;

    if (p == NULL || n < 2 || g_gx_lineStyle.style == PS_NULL) return;
    if (gxDashOn()) {
        int lim = closed ? n : n - 1;
        gxDashReset();
        gxSetTex(0, 0);
        for (i = 0; i < lim; i++) {
            int j = (i + 1) % n;
            gxThickLine(p[i * 2], p[i * 2 + 1],
                        p[j * 2], p[j * 2 + 1], g_gx_lineColor, w, false);
        }
        gxCheckFlush();
        return;
    }
    hw = w * 0.5f;
    if (!(hw > 0.f)) return;
    eps = (hw > 1.f ? hw : 1.f) * 1e-4f;

    sp = (POINTF*)malloc((size_t)(n + 1) * sizeof(POINTF));
    if (!sp) return;
    m = 0;
    for (i = 0; i < n; i++) {
        if (m > 0 && fabsf(p[i * 2] - sp[m - 1].x) < eps &&
                     fabsf(p[i * 2 + 1] - sp[m - 1].y) < eps) continue;
        sp[m].x = p[i * 2]; sp[m].y = p[i * 2 + 1]; m++;
    }
    if (closed && m >= 3) { sp[m] = sp[0]; m++; }   /* the closing join */
    if (m < 2) { free(sp); return; }

    nseg = m - 1;
    nm = (POINTF*)malloc((size_t)nseg * sizeof(POINTF));
    if (!nm) { free(sp); return; }
    for (i = 0; i < nseg; i++)
        gxStrokeNormal(sp[i + 1].x - sp[i].x, sp[i + 1].y - sp[i].y,
                       &nm[i].x, &nm[i].y);

    ocap = 2 * m + 2 * (m - 1) * GX_STROKE_JOIN_SEGS
               + 2 * (GX_STROKE_CAP_SEGS + 2) + 8;
    out = (POINTF*)malloc((size_t)ocap * sizeof(POINTF));
    if (!out) { free(sp); free(nm); return; }

    oldCap = g_gx_strokeCap;
    if (closed) g_gx_strokeCap = GX_CAP_BUTT;
    cnt = gxStrokeOutline(sp, nm, m, hw, out, ocap);
    g_gx_strokeCap = oldCap;

    if (cnt >= 3) {
        oldFill  = getpolyfillmode();
        oldStyle = g_gx_fillStyle.style;
        oldCol   = g_gx_fillColor;
        g_gx_fillStyle.style = BS_SOLID;      /* the outline is always solid */
        g_gx_fillColor = g_gx_lineColor;
        setpolyfillmode(WINDING);
        gxPolyFillF(out, cnt);
        setpolyfillmode(oldFill);
        g_gx_fillStyle.style = oldStyle;
        g_gx_fillColor = oldCol;
    }
    free(sp); free(nm); free(out);
}

static void polygonf(const POINTF* pts, int n) {
    if (pts == NULL || n < 2) return;
    gxPolyStrokeF(pts, n, true);
}
static void polylinef(const POINTF* pts, int n) {
    if (pts == NULL || n < 2) return;
    gxPolyStrokeF(pts, n, false);
}

/*----------------------------- flood fill -----------------------------*/
#ifndef FLOODFILLBORDER
#define FLOODFILLBORDER  0
#define FLOODFILLSURFACE 1
#endif

static void gxFloodFill(int x, int y, COLORREF color, int filltype) {
    unsigned char* buf = NULL;
    unsigned char* vis = NULL;
    int* stack = NULL;
    int w, h, dx, dy, sp = 0, cap;
    COLORREF target = BLACK;
    int surface = (filltype == FLOODFILLSURFACE);

    if (!g_gx_glReady) return;
    gxFlush();
    w = g_gx_target->w; h = g_gx_target->h;
    if (w <= 0 || h <= 0) return;

    buf = (unsigned char*)malloc((size_t)w * (size_t)h * 4);
    vis = (unsigned char*)calloc((size_t)w * (size_t)h, 1);
    if (!buf || !vis) {
        MessageBoxA(NULL, "Out of memory (floodfill)", "Error", MB_OK);
        free(buf); free(vis);
        return;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, gxReadFbo(g_gx_target->fbo));
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, buf);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);

    /* Bounds are checked in DEVICE space: with a non default origin the
     * logical coordinate system starts at a negative value, so the old
     * "x < 0 || x >= getwidth()" test rejected valid seed points. */
    dx = gxLogToDevX((float)x);
    dy = gxLogToDevY((float)y);
    if (dx < 0 || dx >= w || dy < 0 || dy >= h) { free(buf); free(vis); return; }
    dy = h - 1 - dy;                 /* glReadPixels row (bottom up) */
    {
        unsigned char* q = &buf[(dy * w + dx) * 4];
        target = RGB(q[0], q[1], q[2]);
    }
    if (surface) {
        if (target == color) { free(buf); free(vis); return; }
    } else {
        if (target == color) { free(buf); free(vis); return; }
    }

    cap = (h + 8) * 8;
    stack = (int*)malloc(sizeof(int) * (size_t)cap);
    if (!stack) { free(buf); free(vis); return; }
    stack[sp++] = dx;
    stack[sp++] = dy;

    while (sp > 0) {
        int sy, sx, xl, xr, i;
        sy = stack[--sp];
        sx = stack[--sp];
        if (sy < 0 || sy >= h || sx < 0 || sx >= w) continue;
        if (vis[sy * w + sx]) continue;
        {
            unsigned char* q = &buf[(sy * w + sx) * 4];
            COLORREF cc = RGB(q[0], q[1], q[2]);
            int ok = surface ? (cc == target) : (cc != color);
            if (!ok) continue;
        }
        xl = sx;
        while (xl > 0) {
            unsigned char* q = &buf[(sy * w + (xl - 1)) * 4];
            COLORREF cc = RGB(q[0], q[1], q[2]);
            int ok = surface ? (cc == target) : (cc != color);
            if (!ok || vis[sy * w + (xl - 1)]) break;
            xl--;
        }
        xr = sx;
        while (xr < w - 1) {
            unsigned char* q = &buf[(sy * w + (xr + 1)) * 4];
            COLORREF cc = RGB(q[0], q[1], q[2]);
            int ok = surface ? (cc == target) : (cc != color);
            if (!ok || vis[sy * w + (xr + 1)]) break;
            xr++;
        }
        for (i = xl; i <= xr; i++) vis[sy * w + i] = 1;
        if (xr >= xl) {
            float fy = (float)(h - 1 - sy) * gxInvScaleY();
            float fx0 = (float)xl * gxInvScaleX();
            float fx1 = (float)(xr + 1) * gxInvScaleX();
            float fy1 = fy + gxInvScaleY();
            if (gxBeginFill())
                gxQuad(fx0 - g_gx_originX * gxInvScaleX(), fy - g_gx_originY * gxInvScaleY(),
                       fx1 - g_gx_originX * gxInvScaleX(), fy1 - g_gx_originY * gxInvScaleY(),
                       g_gx_fillColor);
        }
        for (i = xl; i <= xr; i++) {
            int ny2;
            for (ny2 = sy - 1; ny2 <= sy + 1; ny2 += 2) {
                unsigned char* q;
                COLORREF cc;
                int ok;
                if (ny2 < 0 || ny2 >= h) continue;
                if (vis[ny2 * w + i]) continue;
                q = &buf[(ny2 * w + i) * 4];
                cc = RGB(q[0], q[1], q[2]);
                ok = surface ? (cc == target) : (cc != color);
                if (ok && sp + 2 <= cap) { stack[sp++] = i; stack[sp++] = ny2; }
            }
        }
    }
    free(stack);
    free(vis);
    free(buf);
    gxCheckFlush();
}

/*======================================================================
 * 11b. Corner opacity draws: alphagradpicture()
 *====================================================================*/
/* One image, four corner opacities.  It is ONE textured quad: the corner
 * values ride in the vertex alpha, which the rasteriser interpolates, so
 * "opaque at the top, gone at the bottom" is a linear ramp along both axes
 * at once and costs one draw call - not a loop over scanlines.
 *
 * That is the whole reason it exists.  The same result through
 * GetImageBuffer() means glReadPixels() every time, which stalls the
 * pipeline and measured at a few hundred calls a second.
 *
 *   alphagradpicture(1, 1, 0, 0, &img)   solid at the top, gone below
 *   alphagradpicture(0, 1, 1, 0, &img)   gone on the left, solid on the right
 *   alphagradpicture(t, t, t, t, &img)   the whole picture at coverage t
 *
 * Crossing an image with a flat colour or with a second image is NOT here.
 * Both are one putimage() plus one overlay and need nothing special:
 *
 *   image x colour   putimage(x, y, &img);
 *                    gradrectangle(x, y, x + w - 1, y + h - 1,
 *                                  ARGB(a, r, g, b), ...same four times);
 *
 *   image x image    putimage(x, y, &imgA);
 *                    alphagradpicture(x, y, t, t, t, t, &imgB);
 *
 * gradrectangle() takes ARGB colours, so the overlay is any opacity you
 * like, and the four corners being equal is just "the whole rectangle at
 * that opacity".  Two ordinary calls beat one dedicated one. */

/* The plain colour version, for callers who just want the value.  No GPU
 * involved - it is four multiplies.
 *
 * Opacity, not alpha, is what gets averaged.  In this library the high
 * byte of a COLORREF is how TRANSPARENT a colour is (see section 3), so
 * averaging those bytes would make a mix of two opaque colours come out
 * half see through.  Averaging coverage instead is what "half way between
 * solid red and solid blue" is supposed to mean. */
COLORREF mixcolor(double w1, COLORREF c1, double w2, COLORREF c2) {
    double sum = w1 + w2;
    double t, k1, k2, op1, op2, op;
    int r, g, b, a;
    if (sum <= 0.0) return 0;
    t  = w2 / sum;
    k1 = 1.0 - t;
    k2 = t;
    r = (int)((double)GetRValue(c1) * k1 + (double)GetRValue(c2) * k2 + 0.5);
    g = (int)((double)GetGValue(c1) * k1 + (double)GetGValue(c2) * k2 + 0.5);
    b = (int)((double)GetBValue(c1) * k1 + (double)GetBValue(c2) * k2 + 0.5);
    /* gxAlphaOf() hands back coverage, so go back the same way. */
    op1 = (double)gxAlphaOf(c1);
    op2 = (double)gxAlphaOf(c2);
    op  = op1 * k1 + op2 * k2;
    if (r < 0) r = 0; if (r > 255) r = 255;
    if (g < 0) g = 0; if (g > 255) g = 255;
    if (b < 0) b = 0; if (b > 255) b = 255;
    a = 255 - (int)(op * 255.0 + 0.5);
    if (a < 0) a = 0; if (a > 255) a = 255;
    return ARGB((BYTE)a, (BYTE)r, (BYTE)g, (BYTE)b);
}

/* gxPutImage() lives in section 12, below this one. */
static void gxPutImage(int dx, int dy, int dw, int dh,
                       const IMAGE* img, int sx, int sy, DWORD rop);

/* The draw behind alphagradpicture().
 *
 * dw/dh <= 0 means "the image's own logical size", the same rule
 * putimage() uses.  dw/dh <= 0 after that (a zero sized image) draws
 * nothing rather than producing a degenerate quad. */
static void gxAlphaGrad(double dx, double dy, double dw, double dh,
                        const IMAGE* img,
                        double aTL, double aTR, double aBR, double aBL) {
    float al[4];
    COLORREF vc[4];
    float x0, y0, x1, y1;
    int i;

    if (!gxImageOk(img) || !g_gx_glReady) return;
    if (dw <= 0) dw = (img->logW > 0) ? img->logW : img->width;
    if (dh <= 0) dh = (img->logH > 0) ? img->logH : img->height;
    if (dw <= 0 || dh <= 0) return;

    x0 = (float)dx;            y0 = (float)dy;
    x1 = (float)(dx + dw);     y1 = (float)(dy + dh);

    gxSetVertA(1);
    gxSetTex(img->tex, 2);
    gxQuadMode();
    al[0] = (float)aTL; al[1] = (float)aTR;
    al[2] = (float)aBR; al[3] = (float)aBL;
    for (i = 0; i < 4; i++) {
        float a = al[i];
        if (!(a > 0.f)) a = 0.f;      /* also catches NaN */
        if (a > 1.f)    a = 1.f;
        /* vColor.a is coverage and the high byte of a COLORREF is the
         * opposite of that, hence the 1 - a.  RGB is unused: uUseTex 2
         * replaces the whole colour with the texel. */
        vc[i] = ARGB((BYTE)((1.f - a) * 255.f + 0.5f), 255, 255, 255);
    }
    /* Corner order is TL, TR, BR, BL - the same winding gxQuadTex() uses.
     *
     * v is flipped the way gxPutImage() flips it: an FBO texture is stored
     * bottom up, so v = 0 is the image's BOTTOM row and v = 1 is its top.
     * Handing v = 0 to the top edge draws the picture upside down. */
    gxVPush(x0, y0, vc[0], 0.f, 1.f);
    gxVPush(x1, y0, vc[1], 1.f, 1.f);
    gxVPush(x1, y1, vc[2], 1.f, 0.f);
    gxVPush(x0, y1, vc[3], 0.f, 0.f);
    gxCheckFlush();
    gxSetVertA(0);
}

/* Four corner opacities, 1 = solid and 0 = gone, in the order
 * top left, top right, bottom right, bottom left.  The rasteriser
 * interpolates them, so this is a linear ramp in both axes at once and it
 * costs exactly one quad.  The classic use is a sprite fading out towards
 * one edge - water, fog, a shadow under a character.
 *
 * The 5 argument spelling draws at the current target's origin, which is
 * the one you want when the target is an IMAGE: it covers the whole
 * thing.  The 7 argument one puts it where you say.  Both take the size
 * from the image, as putimage(x, y, &img) does. */
static void gx_agp_5(double aTL, double aTR, double aBR, double aBL,
                     const IMAGE* img) {
    gxAlphaGrad(0, 0, 0, 0, img, aTL, aTR, aBR, aBL);
}
static void gx_agp_7(double dx, double dy, double aTL, double aTR,
                     double aBR, double aBL, const IMAGE* img) {
    gxAlphaGrad(dx, dy, 0, 0, img, aTL, aTR, aBR, aBL);
}

/* miximagec() / miximagei() are the only callers.  Mode 1 crosses into a
 * flat colour, mode 2 into a second image on GL_TEXTURE1. */
GX_INLINE void gxSetMix(int mode, float w, COLORREF c, GLuint tex2) {
    float r = GX_BYTE_TO_FLOAT(GetRValue(c));
    float g = GX_BYTE_TO_FLOAT(GetGValue(c));
    float b = GX_BYTE_TO_FLOAT(GetBValue(c));
    float a = gxAlphaOf(c);
    if (mode != g_gx_curMixMode || w != g_gx_curMixW || tex2 != g_gx_curTex2
        || r != g_gx_curMixR || g != g_gx_curMixG
        || b != g_gx_curMixB || a != g_gx_curMixA) {
        gxEndCmd();
        g_gx_curMixMode = mode;
        g_gx_curMixW = w;
        g_gx_curTex2 = tex2;
        g_gx_curMixR = r; g_gx_curMixG = g;
        g_gx_curMixB = b; g_gx_curMixA = a;
    }
}

/* The draw behind miximagec() / miximagei().
 *
 * img2 non NULL means "cross into a second image" (mode 2), otherwise the
 * flat colour is used (mode 1).  Each source is stretched over the whole
 * rectangle with its own 0..1 UV, the way putimage() with a different size
 * does it, and v is flipped because an FBO texture is stored bottom up.
 *
 * The blend is PREMULTIPLIED, and that is the entire point when either
 * source is transparent.  A straight mix() of unpremultiplied texels lets
 * the colour of an invisible pixel - whatever was left in the texture,
 * usually black - bleed into the result: half of a fully transparent red
 * plus half of a solid green comes out as a muddy olive at half coverage,
 * where it should be pure green at half coverage.  Premultiplying first
 * gives the transparent pixel weight zero, exactly as it should have. */
static void gxMixDraw(double dx, double dy, double dw, double dh,
                      const IMAGE* img, const IMAGE* img2, COLORREF flat,
                      double w1, double w2) {
    double sum;
    float t;
    float x0, y0, x1, y1;

    if (!gxImageOk(img) || !g_gx_glReady) return;
    if (img2 && !gxImageOk(img2)) return;
    if (dw <= 0) dw = (img->logW > 0) ? img->logW : img->width;
    if (dh <= 0) dh = (img->logH > 0) ? img->logH : img->height;
    if (dw <= 0 || dh <= 0) return;
    /* Relative weights: (1, a, 3, b) is 25% a and 75% b, not 1 plus 3. */
    sum = w1 + w2;
    if (!(sum > 0.0)) return;        /* both zero: nothing to draw */
    t = (float)(w2 / sum);
    if (!(t > 0.f)) t = 0.f;
    if (t > 1.f)    t = 1.f;

    x0 = (float)dx;        y0 = (float)dy;
    x1 = (float)(dx + dw); y1 = (float)(dy + dh);

    gxSetMix(img2 ? 2 : 1, t, flat, img2 ? img2->tex : 0);
    gxSetTex(img->tex, 2);
    gxQuadMode();
    gxVPush(x0, y0, 0, 0.f, 1.f);
    gxVPush(x1, y0, 0, 1.f, 1.f);
    gxVPush(x1, y1, 0, 1.f, 0.f);
    gxVPush(x0, y1, 0, 0.f, 0.f);
    gxCheckFlush();
    /* One shot: the mix must not survive into whatever is drawn next. */
    gxSetMix(0, 0.f, 0, 0);
}

static void gx_mixc_4(double w1, const IMAGE* img, double w2, COLORREF c) {
    gxMixDraw(0, 0, 0, 0, img, NULL, c, w1, w2);
}
static void gx_mixc_6(double dx, double dy, double w1, const IMAGE* img,
                      double w2, COLORREF c) {
    gxMixDraw(dx, dy, 0, 0, img, NULL, c, w1, w2);
}
static void gx_mixi_4(double w1, const IMAGE* a, double w2, const IMAGE* b) {
    gxMixDraw(0, 0, 0, 0, a, b, 0, w1, w2);
}
static void gx_mixi_6(double dx, double dy, double w1, const IMAGE* a,
                      double w2, const IMAGE* b) {
    gxMixDraw(dx, dy, 0, 0, a, b, 0, w1, w2);
}

/*======================================================================
 * 12. IMAGE support
 *====================================================================*/
#define GXIMG_ALPHA 0x1u

static void gxImageDestroy(IMAGE* img) {
    if (!img) return;
    /* EasyX declares IMAGE as a plain struct, so "IMAGE img;" leaves it full
     * of garbage.  The magic field makes that case harmless. */
    if (img->magic != GXIMG_MAGIC) {
        memset(img, 0, sizeof(*img));
        return;
    }
    if (g_gx_glReady) gxFlush();   /* drop commands still using this texture */
    gxImgBufDrop(img);
    if (img->fbo) glDeleteFramebuffers(1, &img->fbo);
    if (img->tex) glDeleteTextures(1, &img->tex);
    memset(img, 0, sizeof(*img));
}


/* Clear one framebuffer to a flat colour.
 *
 * glTexImage2D(..., NULL) and glRenderbufferStorage*() both leave the new
 * storage UNDEFINED - the driver is allowed to hand back memory that still
 * holds whatever was there before.  Presenting a canvas that was only just
 * allocated therefore shows speckles, which is why a program used to need a
 * cleardevice() straight after initgraph() before it looked right.  EasyX
 * hands over a window that is already in the background colour.
 *
 * glClear() is affected by the scissor box and by GL_COLOR_LOGIC_OP (an
 * active XOR rop2 would invert the fill), so both are put into a known
 * state; blending does not affect glClear().  The target the caller was
 * drawing into is rebound afterwards - and with it the viewport and the
 * clip box, which this temporarily overrode. */
static void gxClearFbo(GLuint fbo, int w, int h, COLORREF c, float a) {
    if (!g_gx_glReady || !fbo || w < 1 || h < 1) return;
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, w, h);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_COLOR_LOGIC_OP);
    glClearColor(GetRValue(c) / 255.f, GetGValue(c) / 255.f,
                 GetBValue(c) / 255.f, a);
    glClear(GL_COLOR_BUFFER_BIT);
    gxBindTarget();
}

static void gxImageAlloc(IMAGE* img, int w, int h) {
    if (!img || w < 1 || h < 1) return;
    gxImageDestroy(img);
    glGenTextures(1, &img->tex);
    glBindTexture(GL_TEXTURE_2D, img->tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    {
        /* Honour setimagefilter() at creation time; gxFlush() re-applies it
         * on draw, so switching the mode later still takes effect. */
        GLint f = g_gx_imgFilter ? GL_LINEAR : GL_NEAREST;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, f);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, f);
    }
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glGenFramebuffers(1, &img->fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, img->fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, img->tex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        glBindFramebuffer(GL_FRAMEBUFFER, g_gx_canvasTarget.fbo);
        glDeleteFramebuffers(1, &img->fbo);
        img->fbo = 0;
        return;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, g_gx_target ? g_gx_target->fbo : 0);
    img->width = w; img->height = h;
    /* A freshly sized image has no scale history: its pixels ARE the units
     * it covers.  getimage() overwrites these two when it knows better. */
    img->logW = w; img->logH = h;
    img->magic = GXIMG_MAGIC;
    if (g_gx_workImg == img) gxSyncWorkTarget();   /* the target was replaced */
    /* The texture was allocated with NULL data, so it holds undefined
     * pixels until something is drawn into it - a Resize() that is only
     * partly painted used to blit speckles.  Opaque black, which is what
     * EasyX hands back for a fresh image.  After gxSyncWorkTarget(), so the
     * binding it restores is the right one. */
    gxClearFbo(img->fbo, w, h, BLACK, 1.f);
}

GX_INLINE void Resize(IMAGE* pImg, int width, int height) {
    gxImageAlloc(pImg, width, height);
}

static void gxClearImage(IMAGE* img, COLORREF c) {
    GLuint prev;
    if (!img || !img->fbo || !g_gx_glReady) return;
    prev = (g_gx_target ? g_gx_target->fbo : 0);
    gxFlush();
    glBindFramebuffer(GL_FRAMEBUFFER, img->fbo);
    glViewport(0, 0, img->width, img->height);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(GetRValue(c) / 255.f, GetGValue(c) / 255.f, GetBValue(c) / 255.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, prev);
    gxBindTarget();
}

/* Upload a top-down or bottom-up 32bpp pixel block into an IMAGE. */
static void gxImageUpload(IMAGE* img, const unsigned char* px, int w, int h, bool bottomUp) {
    if (!img || !px || w < 1 || h < 1) return;
    gxImageAlloc(img, w, h);
    if (!img->tex) return;
    glBindTexture(GL_TEXTURE_2D, img->tex);
    if (bottomUp) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px);
    } else {
        int row;
        unsigned char* flip = (unsigned char*)malloc((size_t)w * (size_t)h * 4);
        if (!flip) { MessageBoxA(NULL, "Out of memory", "Error", MB_OK); return; }
        for (row = 0; row < h; row++)
            memcpy(&flip[row * w * 4], &px[(h - 1 - row) * w * 4], (size_t)w * 4);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, flip);
        free(flip);
    }
}

/* Read a bottom-up RGBA block out of a render target. */
static unsigned char* gxReadTarget(GLuint fbo, int x, int y, int w, int h) {
    unsigned char* px;
    if (!g_gx_glReady || w < 1 || h < 1) return NULL;
    px = (unsigned char*)malloc((size_t)w * (size_t)h * 4);
    if (!px) { MessageBoxA(NULL, "Out of memory", "Error", MB_OK); return NULL; }
    gxFlush();
    glBindFramebuffer(GL_FRAMEBUFFER, gxReadFbo(fbo));
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glBindFramebuffer(GL_FRAMEBUFFER, g_gx_target ? g_gx_target->fbo : 0);
    return px;
}

static void gxGetImageFrom(IMAGE* dst, GLuint srcFbo, int srcW, int srcH,
                           int x, int y, int w, int h) {
    unsigned char* px;
    int cx = x, cy = y, cw = w, chh = h;
    if (!dst) return;
    if (cw < 0) cw = srcW;
    if (chh < 0) chh = srcH;
    if (cx < 0) cx = 0;
    if (cy < 0) cy = 0;
    if (cw > srcW - cx) cw = srcW - cx;
    if (chh > srcH - cy) chh = srcH - cy;
    if (cw < 1 || chh < 1) return;
    /* glReadPixels origin is bottom-left, source rect origin is top-left. */
    px = gxReadTarget(srcFbo, cx, srcH - cy - chh, cw, chh);
    if (!px) return;
    gxImageUpload(dst, px, cw, chh, true);
    free(px);
}

/* getimage() takes LOGICAL coordinates; gxGetImageFrom() works on DEVICE
 * pixels.  The conversion used to be missing, so with setaspectratio(2, 2)
 * a request for (10,20,100,50) silently returned the top left quarter of the
 * rectangle - and with setorigin() it returned a region shifted by the
 * origin.  w / h <= 0 keeps the "rest of the target" meaning. */
static void gxGetImage5(IMAGE* dst, int x, int y, int w, int h) {
    int dx, dy, dw, dh;
    dx = gxLogToDevX((float)x);
    dy = gxLogToDevY((float)y);
    dw = (w <= 0) ? -1 : ((int)ceilf((float)(x + w) * g_gx_scaleX + g_gx_originX) - dx);
    dh = (h <= 0) ? -1 : ((int)ceilf((float)(y + h) * g_gx_scaleY + g_gx_originY) - dy);
    if (dw < 0 && w > 0) dw = 0;
    if (dh < 0 && h > 0) dh = 0;
    gxGetImageFrom(dst, g_gx_target->fbo, g_gx_target->w, g_gx_target->h, dx, dy, dw, dh);
    /* How many logical units the grab stands for, which is what
     * putimage(x, y, &img) has to draw.  Without this the copy came out one
     * scale factor too large: 160 units were read as 240 pixels, and 240 was
     * then read back as 240 units.  w / h <= 0 means "the rest", so fall
     * back to the logical extent in that case. */
    if (dst->magic == GXIMG_MAGIC) {
        if (w > 0 && h > 0) {
            dst->logW = w;
            dst->logH = h;
        } else {
            dst->logW = (int)(g_gx_logW + 0.5f);
            dst->logH = (int)(g_gx_logH + 0.5f);
        }
    }
}
static void gxGetImage6(IMAGE* dst, const IMAGE* src, int x, int y, int w, int h) {
    if (!src) return;
    gxGetImageFrom(dst, src->fbo, src->width, src->height, x, y, w, h);
}

/*----------------------------------------------------------------*/
/* putimage: draw src (or a sub rect of it) into the current target. */
static void gxPutImage(int dx, int dy, int dw, int dh,
                       const IMAGE* src, int sx, int sy, DWORD rop) {
    float u0, v0, u1, v1;
    int mode = 2;
    int rop2 = R2_COPYPEN;
    int sw, sh;                  /* source rect actually read (clipped)     */
    bool whole;                  /* dw/dh were omitted: draw the whole image */
    IMAGE  copy;                 /* used only when dst and src are the same */
    const IMAGE* realSrc = src;
    if (!gxImageOk(src) || !g_gx_glReady) return;
    /* Drawing an image into itself - SetWorkingImage(&img) followed by
     * putimage(..., &img) - binds one texture as both the framebuffer
     * attachment and the sampler, which is a feedback loop: the result is
     * undefined per the GL spec and in practice comes out as garbage or a
     * solid block.  rotateimage() and flipimage() already guard against it;
     * this path did not.  Read through a copy instead. */
    if (g_gx_target && g_gx_target->fbo == realSrc->fbo) {
        memset(&copy, 0, sizeof(copy));
        gxGetImageFrom(&copy, realSrc->fbo, realSrc->width, realSrc->height,
                       0, 0, realSrc->width, realSrc->height);
        if (!gxImageOk(&copy)) return;
        copy.logW = (realSrc->logW > 0) ? realSrc->logW : realSrc->width;
        copy.logH = (realSrc->logH > 0) ? realSrc->logH : realSrc->height;
        realSrc = &copy;
    }
    /* Remember BEFORE dw is overwritten: the flag is what later decides how
     * many source pixels are read, and testing dw <= 0 after the assignment
     * is always false, which read only logW pixels of a wider texture and
     * cropped the picture. */
    whole = (dw <= 0 && dh <= 0);
    /* logW / logH, not width / height: the pixel count is how much detail
     * the image has, the logical size is how much of the space it covers,
     * and drawing has to use the latter.  They differ for anything that
     * came out of getimage() on a scaled canvas. */
    if (dw <= 0) dw = (realSrc->logW > 0) ? realSrc->logW : realSrc->width;
    if (dh <= 0) dh = (realSrc->logH > 0) ? realSrc->logH : realSrc->height;
    if (sx < 0) sx = 0;
    if (sy < 0) sy = 0;
    /* Clip the source rectangle: UVs outside [0,1] hit the CLAMP filter and
     * would smear the border pixels over the whole quad. */
    if (sx >= realSrc->width || sy >= realSrc->height) {
        if (realSrc != src) gxImageDestroy(&copy);
        return;
    }
    /* sw / sh is how many source PIXELS are read and must be clipped (a UV
     * past 1.0 hits the CLAMP filter and smears the border pixel).
     * dw / dh is how big it is DRAWN and must NOT be clipped - that IS the
     * stretch.  Clamping dw with the source size made
     * putimage(0, 0, 800, 600, &smallImg, 0, 0) silently draw at 1:1, so
     * scaled sprites, thumbnails and full screen blits all came out tiny.
     *
     * The two also have to be decided independently, because they are in
     * different units: with the 3/4 argument form the whole image is drawn,
     * so sw is the full PIXEL width while dw is the LOGICAL width - and for
     * an image out of getimage() on a scaled canvas those differ (240 pixels
     * covering 160 units).  Mixing them read only 240/240ths of nothing:
     * sw = dw = 160 against a 240 pixel texture stopped the UVs at 0.667 and
     * two thirds of the picture was cut off. */
    sw = whole ? realSrc->width  : dw;
    sh = whole ? realSrc->height : dh;
    if (sw > realSrc->width  - sx) sw = realSrc->width  - sx;
    if (sh > realSrc->height - sy) sh = realSrc->height - sy;
    if (sw < 1 || sh < 1) {
        if (realSrc != src) gxImageDestroy(&copy);
        return;
    }
    u0 = (float)sx / (float)realSrc->width;
    u1 = (float)(sx + sw) / (float)realSrc->width;
    v0 = 1.f - (float)sy / (float)realSrc->height;
    v1 = 1.f - (float)(sy + sh) / (float)realSrc->height;

    switch (rop) {
    case SRCCOPY:     mode = 2; rop2 = R2_COPYPEN;     break;  /* S         */
    case NOTSRCCOPY:  mode = 4; rop2 = R2_COPYPEN;     break;  /* ~S        */
    case SRCPAINT:    mode = 2; rop2 = R2_MERGEPEN;    break;  /* S | D     */
    case SRCAND:      mode = 2; rop2 = R2_MASKPEN;     break;  /* S & D     */
    case SRCINVERT:   mode = 2; rop2 = R2_XORPEN;      break;  /* S ^ D     */
    case SRCERASE:    mode = 2; rop2 = R2_MASKPENNOT;  break;  /* S & ~D    */
    case NOTSRCERASE: mode = 2; rop2 = R2_NOTMERGEPEN; break;  /* ~(S | D)  */
    case MERGEPAINT:  mode = 2; rop2 = R2_MERGENOTPEN; break;  /* ~S | D    */
    case BLACKNESS:   mode = 0; rop2 = R2_BLACK;       break;  /* 0         */
    case WHITENESS:   mode = 0; rop2 = R2_WHITE;       break;  /* 1         */
    case DSTINVERT:   mode = 0; rop2 = R2_NOT;         break;  /* ~D        */
    default:          mode = 2; rop2 = R2_COPYPEN;     break;  /* S         */
    }
    gxSetRop(rop2);
    gxSetTex(realSrc->tex, mode);
    gxQuadTex((float)dx, (float)dy, (float)(dx + dw), (float)(dy + dh),
              u0, v0, u1, v1, WHITE);
    gxSetRop(g_gx_rop2);
    gxCheckFlush();
    if (realSrc != src) gxImageDestroy(&copy);
}

static void gxPutImage3(int x, int y, const IMAGE* img) {
    gxPutImage(x, y, 0, 0, img, 0, 0, SRCCOPY);
}
static void gxPutImage4(int x, int y, const IMAGE* img, DWORD rop) {
    gxPutImage(x, y, 0, 0, img, 0, 0, rop);
}
static void gxPutImage7(int dx, int dy, int dw, int dh, const IMAGE* img, int sx, int sy) {
    gxPutImage(dx, dy, dw, dh, img, sx, sy, SRCCOPY);
}
static void gxPutImage8(int dx, int dy, int dw, int dh, const IMAGE* img, int sx, int sy, DWORD rop) {
    gxPutImage(dx, dy, dw, dh, img, sx, sy, rop);
}

/*======================================================================
 * 13. Working image
 *====================================================================*/
/* Renamed from SetWorkingImage() so the public name can be a dispatch
 * macro in C (SetWorkingImage() with no argument means "back to the
 * window", which EasyX allows and a plain C function cannot): a macro
 * called SetWorkingImage would otherwise be expanded inside its own
 * definition.  The C++ build gets real overloads instead. */
static void gxSetWorkingImage(IMAGE* pImg) {
    gxFlush();
    if (!pImg) {
        g_gx_workImg = NULL;
    } else {
        if (!gxImageOk(pImg) || pImg->width < 1 || pImg->height < 1) return;
        /* Leaving the canvas: remember its transform, because the IMAGE
         * about to be selected runs at 1:1.  Only on the way in - while an
         * IMAGE is selected the live values are already 1:1 and copying
         * them back would throw the canvas transform away. */
        if (!g_gx_workImg) {
            g_gx_canvasScaleX  = g_gx_scaleX;
            g_gx_canvasScaleY  = g_gx_scaleY;
            g_gx_canvasOriginX = g_gx_originX;
            g_gx_canvasOriginY = g_gx_originY;
        }
        g_gx_workImg = pImg;
    }
    gxSyncWorkTarget();
    gxUpdateProj();
    gxBindTarget();
}
static IMAGE* GetWorkingImage(void) { return g_gx_workImg; }

/*======================================================================
 * 14. Image file I/O (BMP via Win32, JPG/GIF/PNG/ICO via GDI+ if present)
 *====================================================================*/
typedef struct GxGdipStartupInput {
    UINT32 GdiplusVersion;
    void*  DebugEventCallback;
    BOOL   SuppressBackgroundThread;
    BOOL   SuppressExternalCodecs;
} GxGdipStartupInput;

typedef int (WINAPI* GXPFN_GPSTARTUP)(ULONG_PTR*, const GxGdipStartupInput*, void*);
typedef int (WINAPI* GXPFN_GPSHUTDOWN)(ULONG_PTR);
typedef int (WINAPI* GXPFN_GPLOADFILE)(const WCHAR*, void**);
typedef int (WINAPI* GXPFN_GPFROMHBMP)(HBITMAP, void*, void**);
typedef int (WINAPI* GXPFN_GPTOHBMP)(void*, HBITMAP*, COLORREF);
typedef int (WINAPI* GXPFN_GPDISPOSE)(void*);
typedef int (WINAPI* GXPFN_GPSAVE)(void*, const WCHAR*, const CLSID*, const void*);
typedef int (WINAPI* GXPFN_GPGETW)(void*, UINT*);
typedef int (WINAPI* GXPFN_GPGETH)(void*, UINT*);

static HMODULE   g_gx_gpDll = NULL;
static ULONG_PTR g_gx_gpToken = 0;
static GXPFN_GPLOADFILE g_gx_gpLoadFile = 0;
static GXPFN_GPFROMHBMP g_gx_gpFromHbm = 0;
static GXPFN_GPTOHBMP   g_gx_gpToHbm = 0;
static GXPFN_GPDISPOSE  g_gx_gpDispose = 0;
static GXPFN_GPSAVE     g_gx_gpSave = 0;
static GXPFN_GPGETW     g_gx_gpGetW = 0;
static GXPFN_GPGETH     g_gx_gpGetH = 0;

static const CLSID gxClsidPNG = { 0x557CF406, 0x1A04, 0x11D3,
    { 0x9A, 0x73, 0x00, 0x00, 0xF8, 0x1E, 0xF3, 0x2E } };
static const CLSID gxClsidJPG = { 0x557CF401, 0x1A04, 0x11D3,
    { 0x9A, 0x73, 0x00, 0x00, 0xF8, 0x1E, 0xF3, 0x2E } };
static const CLSID gxClsidBMP = { 0x557CF400, 0x1A04, 0x11D3,
    { 0x9A, 0x73, 0x00, 0x00, 0xF8, 0x1E, 0xF3, 0x2E } };

static bool gxGdipInit(void) {
    if (g_gx_gpDll) return true;
    g_gx_gpDll = LoadLibraryA("gdiplus.dll");
    if (!g_gx_gpDll) return false;
    {
        GXPFN_GPSTARTUP pStart = (GXPFN_GPSTARTUP)GetProcAddress(g_gx_gpDll, "GdiplusStartup");
        GXPFN_GPSHUTDOWN pStop = (GXPFN_GPSHUTDOWN)GetProcAddress(g_gx_gpDll, "GdiplusShutdown");
        if (!pStart) { FreeLibrary(g_gx_gpDll); g_gx_gpDll = NULL; return false; }
        g_gx_gpLoadFile = (GXPFN_GPLOADFILE)GetProcAddress(g_gx_gpDll, "GdipLoadImageFromFile");
        g_gx_gpFromHbm  = (GXPFN_GPFROMHBMP)GetProcAddress(g_gx_gpDll, "GdipCreateBitmapFromHBITMAP");
        g_gx_gpToHbm    = (GXPFN_GPTOHBMP)GetProcAddress(g_gx_gpDll, "GdipCreateHBITMAPFromBitmap");
        g_gx_gpDispose  = (GXPFN_GPDISPOSE)GetProcAddress(g_gx_gpDll, "GdipDisposeImage");
        g_gx_gpSave     = (GXPFN_GPSAVE)GetProcAddress(g_gx_gpDll, "GdipSaveImageToFile");
        g_gx_gpGetW     = (GXPFN_GPGETW)GetProcAddress(g_gx_gpDll, "GdipGetImageWidth");
        g_gx_gpGetH     = (GXPFN_GPGETH)GetProcAddress(g_gx_gpDll, "GdipGetImageHeight");
        {
            GxGdipStartupInput in;
            memset(&in, 0, sizeof(in));
            in.GdiplusVersion = 1;
            if (pStart(&g_gx_gpToken, &in, NULL) != 0) {
                FreeLibrary(g_gx_gpDll); g_gx_gpDll = NULL; return false;
            }
        }
        (void)pStop;
    }
    return true;
}

/* Convert an HBITMAP into a bottom-up 32bpp RGBA block. */
static unsigned char* gxBitsFromHBmp(HBITMAP hbm, int* outW, int* outH) {
    BITMAP bm;
    BITMAPINFOHEADER bi;
    unsigned char* px;
    HDC dc;
    int w, h;
    if (!hbm) return NULL;
    memset(&bm, 0, sizeof(bm));
    if (!GetObjectA(hbm, sizeof(bm), &bm)) return NULL;
    if (bm.bmBitsPixel != 32) {
        /* Let GDI convert to 32 bpp by blitting into a DIB section. */
        HDC src = CreateCompatibleDC(NULL);
        HDC dst;
        unsigned char* tmp;
        HBITMAP dib;
        void* bits = NULL;
        int row;
        if (bm.bmWidth < 1 || bm.bmHeight < 1) { DeleteDC(src); return NULL; }
        memset(&bi, 0, sizeof(bi));
        bi.biSize = sizeof(bi);
        bi.biWidth = bm.bmWidth;
        bi.biHeight = bm.bmHeight;
        bi.biPlanes = 1;
        bi.biBitCount = 32;
        bi.biCompression = BI_RGB;
        dib = CreateDIBSection(NULL, (BITMAPINFO*)&bi, DIB_RGB_COLORS, &bits, NULL, 0);
        if (!dib || !bits) { if (dib) DeleteObject(dib); DeleteDC(src); return NULL; }
        dst = CreateCompatibleDC(NULL);
        SelectObject(src, hbm);
        SelectObject(dst, dib);
        BitBlt(dst, 0, 0, bm.bmWidth, bm.bmHeight, src, 0, 0, SRCCOPY);
        tmp = (unsigned char*)malloc((size_t)bm.bmWidth * (size_t)bm.bmHeight * 4);
        if (!tmp) { DeleteDC(dst); DeleteDC(src); DeleteObject(dib); return NULL; }
        for (row = 0; row < bm.bmHeight; row++) {
            int x;
            const unsigned char* s2 = (const unsigned char*)bits + (size_t)row * bm.bmWidth * 4;
            unsigned char* d2 = tmp + (size_t)(bm.bmHeight - 1 - row) * bm.bmWidth * 4;
            for (x = 0; x < bm.bmWidth; x++) {
                unsigned char b = s2[x * 4 + 0], g2 = s2[x * 4 + 1], r = s2[x * 4 + 2];
                d2[x * 4 + 0] = r; d2[x * 4 + 1] = g2;
                d2[x * 4 + 2] = b; d2[x * 4 + 3] = 255;
            }
        }
        DeleteDC(dst); DeleteDC(src); DeleteObject(dib);
        *outW = bm.bmWidth; *outH = bm.bmHeight;
        return tmp;
    }
    w = bm.bmWidth; h = bm.bmHeight;
    if (w < 1 || h < 1) return NULL;
    px = (unsigned char*)malloc((size_t)w * (size_t)h * 4);
    if (!px) return NULL;
    memset(&bi, 0, sizeof(bi));
    bi.biSize = sizeof(bi);
    bi.biWidth = w;
    bi.biHeight = h;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    dc = CreateCompatibleDC(NULL);
    if (GetDIBits(dc, hbm, 0, h, px, (BITMAPINFO*)&bi, DIB_RGB_COLORS)) {
        int i;
        for (i = 0; i < w * h; i++) {
            unsigned char b = px[i * 4 + 0], g2 = px[i * 4 + 1], r = px[i * 4 + 2];
            px[i * 4 + 0] = r; px[i * 4 + 1] = g2; px[i * 4 + 2] = b;
        }
    } else {
        free(px); DeleteDC(dc); return NULL;
    }
    DeleteDC(dc);
    *outW = w; *outH = h;
    return px;
}

static bool gxLoadImageFile(const WCHAR* file, IMAGE* img, int w, int h, bool resize) {
    HBITMAP hbm = NULL;
    unsigned char* px = NULL;
    int iw = 0, ih = 0;
    bool ok = false;

    if (gxGdipInit() && g_gx_gpLoadFile) {
        void* gpImg = NULL;
        if (g_gx_gpLoadFile(file, &gpImg) == 0 && gpImg) {
            HBITMAP hb = NULL;
            if (g_gx_gpToHbm && g_gx_gpToHbm(gpImg, &hb, 0) == 0 && hb) {
                px = gxBitsFromHBmp(hb, &iw, &ih);
                DeleteObject(hb);
            }
            if (g_gx_gpDispose) g_gx_gpDispose(gpImg);
        }
    }
    if (!px) {
        char pathA[MAX_PATH];
        if (WideCharToMultiByte(CP_ACP, 0, file, -1, pathA, MAX_PATH, NULL, NULL) > 0) {
            hbm = (HBITMAP)LoadImageA(NULL, pathA, IMAGE_BITMAP, 0, 0,
                                      LR_LOADFROMFILE | LR_CREATEDIBSECTION);
            if (hbm) {
                px = gxBitsFromHBmp(hbm, &iw, &ih);
                DeleteObject(hbm);
            }
        }
    }
    if (!px || iw < 1 || ih < 1) { free(px); return false; }
    {
        /* GDI+ / GDI may hand back a fully transparent alpha channel; in that
         * case the image is opaque and has to be forced to alpha = 255. */
        int i, hasAlpha = 0;
        for (i = 0; i < iw * ih; i++) {
            if (px[i * 4 + 3] != 0) { hasAlpha = 1; break; }
        }
        if (!hasAlpha) {
            for (i = 0; i < iw * ih; i++) px[i * 4 + 3] = 255;
        }
        gxImageUpload(img, px, iw, ih, true);
        if (hasAlpha) img->flags |= GXIMG_ALPHA;
        else img->flags &= ~GXIMG_ALPHA;
        ok = (img->tex != 0);
    }
    free(px);
    if (ok && w > 0 && h > 0 && (resize || w != iw || h != ih)) {
        IMAGE tmp;
        memset(&tmp, 0, sizeof(tmp));
        gxImageAlloc(&tmp, w, h);
        if (tmp.tex && img->tex) {
            DWORD flags = img->flags;
            gxFlush();
            {
                /* Stretch the loaded bitmap into the requested size. */
                GxTarget t;
                float sx1 = g_gx_scaleX, sy1 = g_gx_scaleY, ox = g_gx_originX, oy = g_gx_originY;
                int sw = g_gx_devW, sh = g_gx_devH;
                t.fbo = tmp.fbo; t.tex = tmp.tex; t.w = w; t.h = h;
                g_gx_target = &t;
                g_gx_devW = w; g_gx_devH = h;
                g_gx_scaleX = g_gx_scaleY = 1.f;
                g_gx_originX = g_gx_originY = 0.f;
                gxUpdateProj();
                glBindFramebuffer(GL_FRAMEBUFFER, tmp.fbo);
                glViewport(0, 0, w, h);
                glDisable(GL_SCISSOR_TEST);
                gxSetRopState(R2_COPYPEN);
                glClearColor(0.f, 0.f, 0.f, 0.f);
                glClear(GL_COLOR_BUFFER_BIT);
                glUniformMatrix4fv(g_gx_uProj, 1, GL_FALSE, g_gx_proj);
                gxSetRop(R2_COPYPEN);
                gxSetTex(img->tex, 2);
                gxQuadTex(0.f, 0.f, (float)w, (float)h, 0.f, 1.f, 1.f, 0.f, WHITE);
                gxFlush();
                g_gx_scaleX = sx1; g_gx_scaleY = sy1;
                g_gx_originX = ox; g_gx_originY = oy;
                gxSyncWorkTarget();
                g_gx_devW = sw; g_gx_devH = sh;
                gxUpdateProj();
                gxBindTarget();
            }
            gxImageDestroy(img);
            img->tex = tmp.tex; img->fbo = tmp.fbo;
            img->width = w; img->height = h;
            img->flags = flags;
            img->magic = GXIMG_MAGIC;   /* gxImageDestroy() cleared it */
        } else {
            gxImageDestroy(&tmp);
        }
    }
    return ok;
}

static bool gxSaveImageFile(const WCHAR* file, const IMAGE* img) {
    HBITMAP hbm = NULL;
    unsigned char* px;
    unsigned char* flip;
    BITMAPINFOHEADER bi;
    BITMAPINFO* pbi;
    HDC dc;
    void* bits = NULL;
    int w, h, row;
    bool ok = false;

    /* A NULL image means "the drawing window", the same rule EasyX uses for
     * saveimage(file) and saveimage(file, NULL).  It used to fall through to
     * gxImageOk(NULL) == false, so the single argument form silently saved
     * nothing at all. */
    if (img) {
        if (!gxImageOk(img) || img->width < 1 || img->height < 1) return false;
        w = img->width; h = img->height;
        px = gxReadTarget(img->fbo ? img->fbo : g_gx_target->fbo, 0, 0, w, h);
    } else {
        if (!g_gx_glReady || !g_gx_target || g_gx_target->w < 1 || g_gx_target->h < 1)
            return false;
        w = g_gx_target->w; h = g_gx_target->h;
        px = gxReadTarget(g_gx_target->fbo, 0, 0, w, h);
    }
    if (!px) return false;
    /* glReadPixels is bottom-up while a DIB section is bottom-up as well, so
     * the rows can be copied straight through after the R/B swap. */
    flip = (unsigned char*)malloc((size_t)w * (size_t)h * 4);
    if (!flip) { free(px); return false; }
    for (row = 0; row < h; row++) {
        int x;
        const unsigned char* s2 = px + (size_t)row * w * 4;
        unsigned char* d2 = flip + (size_t)row * w * 4;
        for (x = 0; x < w; x++) {
            d2[x * 4 + 0] = s2[x * 4 + 2];
            d2[x * 4 + 1] = s2[x * 4 + 1];
            d2[x * 4 + 2] = s2[x * 4 + 0];
            d2[x * 4 + 3] = 255;
        }
    }
    memset(&bi, 0, sizeof(bi));
    bi.biSize = sizeof(bi);
    bi.biWidth = w;
    bi.biHeight = h;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    dc = CreateCompatibleDC(NULL);
    pbi = (BITMAPINFO*)malloc(sizeof(BITMAPINFOHEADER) + 16);
    if (!pbi) { free(flip); free(px); DeleteDC(dc); return false; }
    memset(pbi, 0, sizeof(BITMAPINFOHEADER) + 16);
    memcpy(&pbi->bmiHeader, &bi, sizeof(bi));
    hbm = CreateDIBSection(dc, pbi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (hbm && bits) memcpy(bits, flip, (size_t)w * (size_t)h * 4);
    free(pbi);
    free(flip);

    if (hbm) {
        int ext = -1, i;
        for (i = 0; file[i]; i++) if (file[i] == L'.') ext = i;
        if (ext >= 0) {
            WCHAR e0 = file[ext + 1];
            bool isBmp = (e0 == L'b' || e0 == L'B');
            if (!isBmp && gxGdipInit() && g_gx_gpFromHbm && g_gx_gpSave && g_gx_gpDispose) {
                void* gpImg = NULL;
                if (g_gx_gpFromHbm(hbm, NULL, &gpImg) == 0 && gpImg) {
                    const CLSID* cls = &gxClsidPNG;
                    if (e0 == L'j' || e0 == L'J') cls = &gxClsidJPG;
                    if (g_gx_gpSave(gpImg, file, cls, NULL) == 0) ok = true;
                    g_gx_gpDispose(gpImg);
                }
            }
        }
        if (!ok) {
            /* Fall back to a plain 24/32 bpp .bmp file. */
            BITMAPFILEHEADER bf;
            BITMAPINFOHEADER bh;
            DWORD pixels = (DWORD)w * (DWORD)h * 4;
            DWORD written = 0;
            HANDLE f = CreateFileW(file, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                                   FILE_ATTRIBUTE_NORMAL, NULL);
            if (f != INVALID_HANDLE_VALUE) {
                memset(&bf, 0, sizeof(bf));
                bf.bfType = 0x4D42;
                bf.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
                bf.bfSize = bf.bfOffBits + pixels;
                memset(&bh, 0, sizeof(bh));
                bh.biSize = sizeof(bh);
                bh.biWidth = w; bh.biHeight = h;
                bh.biPlanes = 1; bh.biBitCount = 32;
                bh.biCompression = BI_RGB;
                bh.biSizeImage = pixels;
                WriteFile(f, &bf, sizeof(bf), &written, NULL);
                WriteFile(f, &bh, sizeof(bh), &written, NULL);
                WriteFile(f, bits, pixels, &written, NULL);
                CloseHandle(f);
                ok = true;
            }
        }
        DeleteObject(hbm);
    }
    DeleteDC(dc);
    free(px);
    return ok;
}

static bool gx_loadimg2(IMAGE* img, const WCHAR* f) {
    return gxLoadImageFile(f, img, 0, 0, false);
}
static bool gx_loadimg3(IMAGE* img, const WCHAR* f, int w) {
    return gxLoadImageFile(f, img, w, w, false);
}
static bool gx_loadimg4(IMAGE* img, const WCHAR* f, int w, int h) {
    return gxLoadImageFile(f, img, w, h, false);
}
static bool gx_loadimg5(IMAGE* img, const WCHAR* f, int w, int h, bool resize) {
    return gxLoadImageFile(f, img, w, h, resize);
}
static bool gx_saveimg1(const WCHAR* f) {
    return gxSaveImageFile(f, g_gx_workImg ? g_gx_workImg : NULL);
}
static bool gx_saveimg2(const WCHAR* f, const IMAGE* img) {
    return gxSaveImageFile(f, img);
}

/*======================================================================
 * 12b. rotateimage
 *====================================================================*/
/* Switch an IMAGE texture between NEAREST (fast) and LINEAR (smooth). */
static void gxTexFilter(GLuint tex, GLenum filter) {
    if (!tex || !g_gx_glReady) return;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLint)filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLint)filter);
}

/* Rotate src by rad radian into dst.
 * EasyX (and every GDI based rotation) counts a positive angle clockwise,
 * because the y axis points down.  In screen space that is
 *     x' =  x * cos(rad) - y * sin(rad)
 *     y' =  x * sin(rad) + y * cos(rad)
 * (the previous matrix rotated the other way round). */
static void gxRotateImage(IMAGE* dst, IMAGE* src, double rad, COLORREF bk,
                          bool autosize, bool smooth) {
    (void)bk;               /* EasyX keeps it; the uncovered area is
                             * transparent here, not a solid colour */
    double c, s;
    float cx, cy, w, h;
    float lw, lh, pxPerUnit;
    int dw, dh, i;
    int dwPix, dhPix;            /* the destination size in PIXELS          */
    int savedW, savedH;
    float savedScaleX, savedScaleY, savedOX, savedOY;
    GxTarget t;
    IMAGE tmp;
    IMAGE* realSrc;

    if (!dst || !gxImageOk(src) || !g_gx_glReady) return;
    c = cos(rad); s = sin(rad);

    /* rotateimage(&img, &img, ...) reads and writes the same texture, so the
     * source has to be duplicated before the destination is touched. */
    memset(&tmp, 0, sizeof(tmp));
    realSrc = src;
    if (dst == src) {
        gxGetImageFrom(&tmp, src->fbo, src->width, src->height,
                       0, 0, src->width, src->height);
        if (!gxImageOk(&tmp)) return;
        /* The copy is a fresh image, so its logical size would fall back to
         * the pixel count.  Carry the source's over. */
        tmp.logW = (src->logW > 0) ? src->logW : src->width;
        tmp.logH = (src->logH > 0) ? src->logH : src->height;
        realSrc = &tmp;
    }

    /* Measured on realSrc, not src: they are the same picture but only the
     * former is guaranteed to have come from getimage() with its logical
     * size intact. */
    w  = (float)realSrc->width;  h  = (float)realSrc->height;
    /* Logical extent of the source and how many pixels that is per unit.
     * The quad below is emitted in the destination's PIXEL space (it writes
     * the texture directly), so the logical size has to be carried through
     * separately - see the note at the Resize() calls. */
    lw = (float)((realSrc->logW > 0) ? realSrc->logW : realSrc->width);
    lh = (float)((realSrc->logH > 0) ? realSrc->logH : realSrc->height);
    pxPerUnit = (lw > 0.f) ? ((float)realSrc->width / lw) : 1.f;

    /* The sizes are worked out in LOGICAL units and then turned into pixels,
     * so that a rotation does not change how big the picture is drawn.  Resize
     * () sets logW / logH to the pixel count, which is right for a fresh
     * image but wrong here: rotating a 240 pixel image that covers 160 units
     * has to keep covering 160 units, or putimage() of the result comes out
     * one scale factor too large - exactly the bug gxPutImage() was fixed for.
     *
     * Everything that follows draws in the destination's PIXEL space, so it
     * needs dwPix / dhPix.  Feeding it the LOGICAL dw / dh instead is what
     * cropped the result: the viewport and the centre were a factor too
     * small, so only the top left corner of the texture was ever written.
     */
    if (autosize) {
        double ax = fabs(c) * (double)lw + fabs(s) * (double)lh;
        double ay = fabs(s) * (double)lw + fabs(c) * (double)lh;
        dw = (int)(ax + 0.5);
        dh = (int)(ay + 0.5);
        if (dw < 1) dw = 1;
        if (dh < 1) dh = 1;
        dst->logW = dw; dst->logH = dh;
        dwPix = (int)((float)dw * pxPerUnit + 0.5f);
        dhPix = (int)((float)dh * pxPerUnit + 0.5f);
        if (dwPix < 1) dwPix = 1;
        if (dhPix < 1) dhPix = 1;
        Resize(dst, dwPix, dhPix);
        dst->logW = dw; dst->logH = dh;   /* Resize() just reset these */
    } else {
        if (dst->width < 1 || dst->height < 1)
            Resize(dst, src->width, src->height);
        dwPix = dst->width; dhPix = dst->height;
        /* The caller kept the pixel size, so the logical size is whatever
         * fitted in it - for a src from getimage() that is src->logW, not
         * the pixel count. */
        dw = (src->logW > 0) ? src->logW : dwPix;
        dh = (src->logH > 0) ? src->logH : dhPix;
        dst->logW = dw; dst->logH = dh;
    }
    if (!dst->fbo) { gxImageDestroy(&tmp); return; }

    gxFlush();
    savedW = g_gx_devW; savedH = g_gx_devH;
    savedScaleX = g_gx_scaleX; savedScaleY = g_gx_scaleY;
    savedOX = g_gx_originX; savedOY = g_gx_originY;

    t.fbo = dst->fbo; t.tex = dst->tex; t.w = dwPix; t.h = dhPix;
    g_gx_target = &t;
    g_gx_devW = dwPix; g_gx_devH = dhPix;
    g_gx_scaleX = g_gx_scaleY = 1.f;
    g_gx_originX = g_gx_originY = 0.f;
    gxUpdateProj();
    glBindFramebuffer(GL_FRAMEBUFFER, dst->fbo);
    glViewport(0, 0, dwPix, dhPix);
    glDisable(GL_SCISSOR_TEST);
    gxSetRopState(R2_COPYPEN);   /* the blit must not inherit a logic op */
    /* The area the rotated image does not cover used to be filled with the
     * background colour at full alpha, so it came out as a black (or
     * coloured) corner even when the destination was meant to be a sprite
     * with transparent surroundings.  Clear to alpha 0 instead: the RGB
     * values are then irrelevant, and putimage() of the result lets the
     * background show through. */
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUniformMatrix4fv(g_gx_uProj, 1, GL_FALSE, g_gx_proj);

    if (smooth) gxTexFilter(realSrc->tex, GL_LINEAR);
    cx = (float)dwPix * 0.5f; cy = (float)dhPix * 0.5f;
    {
        float lx[4], ly[4], X[4], Y[4], U[4], V[4];
        lx[0] = -w * 0.5f; ly[0] = -h * 0.5f;
        lx[1] =  w * 0.5f; ly[1] = -h * 0.5f;
        lx[2] =  w * 0.5f; ly[2] =  h * 0.5f;
        lx[3] = -w * 0.5f; ly[3] =  h * 0.5f;
        for (i = 0; i < 4; i++) {
            X[i] = cx + (float)(lx[i] * c - ly[i] * s);
            Y[i] = cy + (float)(lx[i] * s + ly[i] * c);
            U[i] = (lx[i] + w * 0.5f) / w;
            V[i] = 1.f - (ly[i] + h * 0.5f) / h;
        }
        gxSetRop(R2_COPYPEN);
        gxSetTex(realSrc->tex, 2);
        gxQuadMode();
        gxVPush(X[0], Y[0], WHITE, U[0], V[0]);
        gxVPush(X[1], Y[1], WHITE, U[1], V[1]);
        gxVPush(X[2], Y[2], WHITE, U[2], V[2]);
        gxVPush(X[3], Y[3], WHITE, U[3], V[3]);
    }
    gxFlush();
    if (smooth) gxTexFilter(realSrc->tex, GL_NEAREST);
    gxImageDestroy(&tmp);

    g_gx_scaleX = savedScaleX; g_gx_scaleY = savedScaleY;
    g_gx_originX = savedOX; g_gx_originY = savedOY;
    gxSyncWorkTarget();
    g_gx_devW = savedW; g_gx_devH = savedH;
    gxUpdateProj();
    gxBindTarget();
}

static void gx_rotimg3(IMAGE* dst, IMAGE* src, double rad) {
    gxRotateImage(dst, src, rad, BLACK, false, true);
}
static void gx_rotimg4(IMAGE* dst, IMAGE* src, double rad, COLORREF bk) {
    gxRotateImage(dst, src, rad, bk, false, true);
}
static void gx_rotimg5(IMAGE* dst, IMAGE* src, double rad, COLORREF bk, bool autosize) {
    gxRotateImage(dst, src, rad, bk, autosize, true);
}
static void gx_rotimg6(IMAGE* dst, IMAGE* src, double rad, COLORREF bk,
                       bool autosize, bool highquality) {
    gxRotateImage(dst, src, rad, bk, autosize, highquality);
}

/*======================================================================
 * 15. Message / keyboard input
 *====================================================================*/
static bool gxPeekEx(ExMessage* m, BYTE filter, bool remove) {
    int i;
    if (!m) return false;
    gxPump();
    for (i = 0; i < g_gx_msgCount; i++) {
        ExMessage* e = gxMsgAt(i);
        if (gxMsgIsType((UINT)e->message, filter)) {
            *m = *e;
            if (remove) {
                /* Drop everything in front of the match as well, keeping the
                 * queue order intact (EasyX removes the message it returns). */
                while (g_gx_msgCount > 0) {
                    ExMessage* h2 = gxMsgAt(0);
                    bool same = (h2 == e);
                    gxMsgPop();
                    if (same) break;
                }
            }
            return true;
        }
    }
    return false;
}

static void gxMsgToMouse(const ExMessage* e, MOUSEMSG* m) {
    memset(m, 0, sizeof(*m));
    m->uMsg = (UINT)e->message;
    m->x = e->x; m->y = e->y; m->wheel = e->wheel;
    m->mkCtrl = e->ctrl; m->mkShift = e->shift;
    m->mkLButton = e->lbutton; m->mkMButton = e->mbutton; m->mkRButton = e->rbutton;
}

static bool gxPeekMouse(MOUSEMSG* m, BYTE filter, bool remove) {
    ExMessage e;
    if (!m) return false;
    if (!gxPeekEx(&e, (filter == 0xFF) ? (BYTE)EM_MOUSE : filter, remove)) return false;
    gxMsgToMouse(&e, m);
    return true;
}

/* EasyX blocks in getmessage() until a message is available; only
 * peekmessage() is non blocking.  WaitMessage() sleeps until the thread's
 * queue receives something, and the loop is needed because a wake up may be
 * caused by a message the filter does not accept (WM_PAINT, a timer, ...).
 * gxPump() runs every pass, so a frame that gxAutoPresent() still owes is
 * shown before the caller goes to sleep - otherwise the picture would lag
 * one input event behind. */
static bool gxWaitMsgEx(ExMessage* m, BYTE filter, bool remove) {
    if (!m) return false;
    for (;;) {
        gxPump();                       /* gxPeekEx() pumps again, harmless */
        if (gxPeekEx(m, filter, remove)) return true;
        if (!g_gx_hwnd) return false;      /* no window: never block */
        WaitMessage();
    }
}

static bool gxWaitMsgMouse(MOUSEMSG* m, BYTE filter) {
    if (!m) return false;
    for (;;) {
        gxPump();
        if (gxPeekMouse(m, filter, true)) return true;
        if (!g_gx_hwnd) return false;
        WaitMessage();
    }
}

static ExMessage gxGetMsgEx(BYTE filter) {
    ExMessage m;
    memset(&m, 0, sizeof(m));
    gxWaitMsgEx(&m, filter, true);
    return m;
}

static MOUSEMSG gxGetMsgMouse(BYTE filter) {
    MOUSEMSG m;
    memset(&m, 0, sizeof(m));
    gxWaitMsgMouse(&m, (filter == 0xFF) ? (BYTE)EM_MOUSE : filter);
    return m;
}

/* variablewinsize(): the WM_SIZE watchers.
 *
 * Pure convenience - nothing here that peekmessage(&m, EX_WINDOW) and a
 * test against WM_SIZE cannot already do - but EX_WINDOW also covers
 * WM_MOVE, WM_SETFOCUS, WM_KILLFOCUS and WM_CLOSE, so the caller has to
 * remember to compare m.message against WM_SIZE, and that is the one step
 * everybody forgets.
 *
 * Deliberately no ExMessage anywhere in the signature: this call answers
 * one question only - has a resize arrived yet - and the thing the caller
 * actually wants, the new size, is what getwidth() / getheight() report
 * right afterwards.  Handing back an ExMessage would mean smuggling the
 * size in through some unrelated field, since ExMessage has no width /
 * height, and that is a worse API than just reading getwidth().
 *
 * Removal follows gxPeekEx(): everything in front of the match goes too,
 * so the queue keeps its order. */
static bool gxPeekVarMsg(bool remove) {
    int i;
    gxPump();
    for (i = 0; i < g_gx_msgCount; i++) {
        ExMessage* e = gxMsgAt(i);
        if (e->message == (USHORT)WM_SIZE) {
            if (remove) {
                while (g_gx_msgCount > 0) {
                    ExMessage* h2 = gxMsgAt(0);
                    bool same = (h2 == e);
                    gxMsgPop();
                    if (same) break;
                }
            }
            return true;
        }
    }
    return false;
}

/* The blocking half.  Waits for a WM_SIZE and nothing else - which means
 * it waits forever if the window never changes size, i.e. when
 * variablewinsize() is off and nothing calls setwinsize() / fixhighdpi().
 * That is the same contract getmessage() has, but it is a much easier way
 * to hang a program, so gxPeekVarMsg() is the one to reach for in a loop
 * that also has to keep drawing.
 *
 * Returns nothing on purpose: a blocking call has no interesting answer to
 * hand back - it either waited, or the window went away - and the thing the
 * caller actually wants, the new size, is what getwidth() / getheight()
 * report afterwards.  The matched message is consumed, and so is anything
 * queued in front of it, which is what gxPeekVarMsg() has always done.
 *
 * No ExMessage is handed back either - see gxPeekVarMsg(). */
static void gxWaitVarMsg(void) {
    for (;;) {
        gxPump();
        if (gxPeekVarMsg(true)) return;
        if (!g_gx_hwnd) return;            /* no window: never block */
        WaitMessage();
    }
}

static void gx_flushmsg_all(void) { gxMsgInit(); gxPump(); gxMsgInit(); }
GX_INLINE void FlushMouseMsgBuffer(void) { gx_flushmsg_all(); }
GX_INLINE bool MouseHit(void) {
    ExMessage m;
    return gxPeekEx(&m, EM_MOUSE, false);
}
/* EasyX has both spellings; the lower case one is an alias. */
GX_INLINE bool mousehit(void) { return MouseHit(); }

/* EasyX: getmousepos(int* x, int* y) reports where the pointer is right
 * now.  It polls instead of waiting for an event, so it works while the
 * mouse is still, and unlike getmessage() it never blocks.
 *
 * The coordinates are LOGICAL, matching ExMessage.x / ExMessage.y: the
 * raw cursor is a screen position, so it goes through ScreenToClient()
 * and then the device -> logical transform. */
GX_INLINE void getmousepos(int* x, int* y) {
    POINT pt;
    pt.x = 0; pt.y = 0;
    if (g_gx_hwnd) {
        if (GetCursorPos(&pt)) ScreenToClient(g_gx_hwnd, &pt);
    } else {
        if (x) *x = 0;
        if (y) *y = 0;
        return;
    }
    if (x) *x = gxDevToLogX((float)pt.x);
    if (y) *y = gxDevToLogY((float)pt.y);
}
GX_INLINE MOUSEMSG GetMouseMsg(void) { return gxGetMsgMouse((BYTE)EM_MOUSE); }


/*--------------------------- InputBox --------------------------------*/
/* Default client WIDTH of the dialog.  Macro rather than a local in
 * gxInputBoxExW() because WM_PAINT falls back to it for the prompt.
 * GX_IB_H is the height the dialog used to be pinned to; the height is now
 * worked out from the prompt, the edit box and the button row, and only
 * grows when the caller's height= asks for more. */
#define GX_IB_W   420
#define GX_IB_H   170

static HWND  g_gx_ibWnd = NULL;
static HWND  g_gx_ibEdit = NULL;
static bool  g_gx_ibDone = false;
static bool  g_gx_ibOk = false;
static WCHAR g_gx_ibText[256];
/* The prompt, stashed so WM_PAINT can draw it.  Painting it straight onto
 * a GetDC() at creation time does not work: ShowWindow() then repaints the
 * window, and DefWindowProc() clears the client area with the class brush
 * (COLOR_BTNFACE), wiping the text out.  Anything a window shows has to be
 * drawn from WM_PAINT, or it disappears on the first repaint. */
static WCHAR g_gx_ibPrompt[512];

/*--------------------------- UI font ---------------------------------*/
/* The font a real Windows dialog uses.
 *
 * GetStockObject(DEFAULT_GUI_FONT) is MS Sans Serif, a bitmap font that
 * dates from Windows 3, and it is what made this dialog look "wrong": it
 * is not what anything else on the system uses.  The system message font,
 * NONCLIENTMETRICS.lfMessageFont, is the one MessageBox(), every common
 * dialog and every property sheet use - Segoe UI 9 on Vista and later.
 *
 * Cached in a static because the controls keep the handle after the call
 * that set it returns; deleting it would leave them with a dangling font.
 * It is deliberately never freed: one HFONT for the life of the process,
 * reclaimed by Windows at exit. */
#ifndef SPI_GETNONCLIENTMETRICS
#define SPI_GETNONCLIENTMETRICS   0x0029
#endif

/*--------------------------- DPI scaling ------------------------------*/
/* Both the font and the layout follow the system DPI through getdpi() /
 * gxIbS() above.  The font came first - NONCLIENTMETRICS.lfMessageFont is
 * already scaled - which left a 150% screen drawing a 13.5 px Segoe UI
 * into a 24 px edit box, so the text overflowed the control. */

/*-------------------- localised button captions ----------------------*/
/* A MessageBox shows "OK" in English Windows and the local equivalent in
 * every other language, without the program doing anything about it: those
 * captions are string resources inside user32.dll and Windows picks the one
 * matching the UI language.  A dialog built by hand has to fetch them
 * itself, or its buttons stay English on a non English system.
 *
 * The ids are stable back to Windows 95 but they are not a documented
 * contract, so every lookup falls back to English if it fails. */
#define GX_IDS_OK       800
#define GX_IDS_CANCEL   801

static bool gxSysCaption(UINT id, const WCHAR* fallback,
                         WCHAR* out, int n) {
    HMODULE hUser;
    out[0] = 0;
    if (n < 1) return false;
    hUser = GetModuleHandleA("user32.dll");
    if (hUser && LoadStringW(hUser, id, out, n) > 0) return true;
    /* LoadStringW takes a character count and always NUL terminates. */
    while (*fallback && n > 1) { *out++ = *fallback++; n--; }
    *out = 0;
    return false;
}

static HFONT g_gx_ibFont = NULL;
static int   g_gx_ibFontDpi = 0;

static HFONT gxUiFont(void) {
    NONCLIENTMETRICSA ncm;
    int dpi = getdpi();
    /* The cached font was built for whatever DPI the last call saw.  That
     * changes when the window is dragged to another monitor, so a mismatch
     * rebuilds it.  Deleting the old one is safe: the controls that held
     * it were destroyed when their dialog closed. */
    if (g_gx_ibFont && g_gx_ibFontDpi == dpi) return g_gx_ibFont;
    if (g_gx_ibFont) { DeleteObject(g_gx_ibFont); g_gx_ibFont = NULL; }

    memset(&ncm, 0, sizeof(ncm));
    ncm.cbSize = (UINT)sizeof(ncm);
    /* SystemParametersInfo() wants the size the running system expects,
     * not the one the SDK was built against, so a refusal here is normal
     * on an older WINVER rather than a real failure.  MS Shell Dlg below
     * covers that case. */
    if (!SystemParametersInfoA(SPI_GETNONCLIENTMETRICS,
                               (UINT)sizeof(ncm), &ncm, 0)) {
        /* MS Shell Dlg is the logical name a dialog template carries; the
         * font mapper substitutes Segoe UI (10 / 11) or Microsoft Sans
         * Serif (2000 / XP) for it.  Built through CreateFontIndirectA so
         * both routes go through the one call. */
        LOGFONTA lf;
        memset(&lf, 0, sizeof(lf));
        lf.lfHeight         = -gxIbS(12);          /* 9 pt at 96 dpi  */
        lf.lfWeight         = FW_NORMAL;
        lf.lfCharSet        = DEFAULT_CHARSET;
        lf.lfOutPrecision   = OUT_DEFAULT_PRECIS;
        lf.lfClipPrecision  = CLIP_DEFAULT_PRECIS;
        lf.lfQuality        = DEFAULT_QUALITY;
        lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
        strcpy(lf.lfFaceName, "MS Shell Dlg");
        g_gx_ibFont = CreateFontIndirectA(&lf);
    } else {
        g_gx_ibFont = CreateFontIndirectA(&ncm.lfMessageFont);
    }
    if (g_gx_ibFont) { g_gx_ibFontDpi = dpi; return g_gx_ibFont; }
    g_gx_ibFontDpi = 0;
    return (HFONT)GetStockObject(DEFAULT_GUI_FONT);   /* last resort */
}

/* Virtual key codes.  winuser.h defines them, but their values are fixed
 * by Windows, so they are filled in for a slim SDK that does not. */
#ifndef VK_RETURN
#define VK_RETURN   0x0D
#endif
#ifndef VK_ESCAPE
#define VK_ESCAPE   0x1B
#endif

/* How many pixels high the prompt needs, wrapped at innerW.  A screen DC
 * is enough to measure with, so this runs before the dialog exists - the
 * answer decides how tall the dialog has to be and where the edit box
 * goes. */
static int gxIbPromptHeight(const WCHAR* p, int innerW) {
    HDC dc;
    HFONT oldF;
    RECT tr;
    int h;
    if (!p || !p[0] || innerW <= 0) return 0;
    dc = GetDC(NULL);
    if (!dc) return 0;
    oldF = (HFONT)SelectObject(dc, gxUiFont());
    tr.left = 0; tr.top = 0; tr.right = innerW; tr.bottom = 0;
    h = DrawTextW(dc, p, -1, &tr,
                  DT_LEFT | DT_TOP | DT_NOPREFIX | DT_WORDBREAK | DT_CALCRECT);
    SelectObject(dc, oldF);
    ReleaseDC(NULL, dc);
    return h;
}

/* Close the dialog with a result.  Shared by the buttons, Enter and
 * Escape so there is one place that reads the text back. */
static void gxIbFinish(bool ok) {
    if (!g_gx_ibWnd) return;
    if (ok && g_gx_ibEdit) GetWindowTextW(g_gx_ibEdit, g_gx_ibText, 255);
    g_gx_ibOk = ok;
    g_gx_ibDone = true;
    DestroyWindow(g_gx_ibWnd);
}

static LRESULT CALLBACK gxInputBoxProc(HWND h, UINT m, WPARAM w, LPARAM l) {
    switch (m) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        if (g_gx_ibPrompt[0]) {
            HFONT oldF = (HFONT)SelectObject(dc, gxUiFont());
            RECT tr;
            int mg = gxIbS(20);
            /* Measured here from the real client width rather than read
             * back from a static: the prompt used to be drawn into a hard
             * coded 20..GX_IB_W-20 by 18..52 band, which clipped anything
             * longer than one line and ignored the width= the caller asked
             * for.  Same flags gxIbPromptHeight() measures with, or the
             * text it measured and the text drawn here would not agree. */
            GetClientRect(h, &tr);
            tr.left   = mg;
            tr.top    = gxIbS(18);
            tr.right  = (tr.right > mg * 2) ? tr.right - mg
                                            : tr.left + gxIbS(GX_IB_W) - mg * 2;
            tr.bottom = tr.top + gxIbPromptHeight(g_gx_ibPrompt, tr.right - tr.left);
            /* Without TRANSPARENT the text gets an opaque rectangle in the
             * DC background colour - a white block on the grey dialog. */
            SetBkMode(dc, TRANSPARENT);
            DrawTextW(dc, g_gx_ibPrompt, -1, &tr,
                      DT_LEFT | DT_TOP | DT_NOPREFIX | DT_WORDBREAK);
            SelectObject(dc, oldF);
        }
        EndPaint(h, &ps);
        return 0;
    }
    case WM_COMMAND:
        if (LOWORD(w) == 1) { gxIbFinish(true);  return 0; }   /* OK     */
        if (LOWORD(w) == 2) { gxIbFinish(false); return 0; }   /* Cancel */
        return 0;
    case WM_DESTROY:
        g_gx_ibDone = true;
        return 0;
    case WM_CLOSE:
        g_gx_ibOk = false;
        g_gx_ibDone = true;
        DestroyWindow(h);
        return 0;
    }
    return DefWindowProcA(h, m, w, l);
}

/* Throw away every keyboard message that is still in flight.
 *
 * This is what makes "press P to rename" behave the same every time.  The
 * trigger is normally seen through GetAsyncKeyState(), which reads the
 * physical key and not the message queue, so the program can open the
 * dialog BEFORE the WM_KEYDOWN / WM_CHAR that belongs to that very
 * keystroke has been dispatched.  Whether it has or not depends on how
 * often the caller pumps, which is why the same key press produced
 * different results on different runs:
 *
 *   already dispatched   - it went to the graphics window and was pushed
 *                          into the library queue, so it never reaches the
 *                          edit box and the default text stays;
 *   still in the queue   - the edit box has the focus by the time it is
 *                          dispatched, so the letter is typed INTO it and
 *                          replaces the default: "p" instead of the name.
 *
 * Draining both queues removes the second case.  Only keyboard messages
 * go - mouse and window messages are kept, so a click or a move that
 * happened while the dialog was opening is not lost.
 *
 * Called with the dialog up and the edit box focused, i.e. late enough to
 * catch anything that arrived during creation as well.  What it cannot do
 * is stop a key that is still HELD, because auto repeat keeps making new
 * messages after this point - that half is gxIbNoteHeld(). */
static void gxIbDrainKeys(void) {
    MSG msg;
    ExMessage keep[GX_MSGQ_CAP];
    int i, nk = 0;
    while (PeekMessageA(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE)) {
        /* dropped: it belongs to the key that opened this dialog */
    }
    for (i = 0; i < g_gx_msgCount; i++) {
        if (!gxMsgIsType(gxMsgAt(i)->message, (BYTE)(EM_KEY | EM_CHAR)))
            keep[nk++] = *gxMsgAt(i);
    }
    gxMsgInit();
    for (i = 0; i < nk; i++) gxMsgPush(&keep[i]);
}

/* Which keys were physically down at the moment the dialog opened - in
 * practice the one that opened it.  Draining the queues is not enough on its
 * own: a key that is still HELD keeps producing messages, and auto repeat
 * sends them to whatever has the focus, which by then is the edit box.  So
 * "press P to rename" typed "p" (or "ppppp") into the box whenever the
 * caller happened to open the dialog before the key came back up.
 *
 * Every keyboard message is dropped while any of these keys is down.  That
 * is short - it ends the moment the key comes up - and it cannot swallow
 * real typing, because the user is still holding the trigger key. */
static int g_gx_ibHeldKey[8];
static int g_gx_ibHeldN = 0;

static void gxIbNoteHeld(void) {
    int v;
    g_gx_ibHeldN = 0;
    /* From VK_BACK up: the mouse buttons below it never arrive as a
     * keyboard message, and a click that opened the dialog would only put
     * them in the list for no reason. */
    for (v = 0x08; v < 0x100 && g_gx_ibHeldN < 8; v++) {
        if ((GetAsyncKeyState(v) & 0x8000) != 0) g_gx_ibHeldKey[g_gx_ibHeldN++] = v;
    }
}

/* Drop from the list every key that has since come back up, and say whether
 * any is still down.  Re-reading the PHYSICAL state rather than waiting for
 * the WM_KEYUP is what keeps this from getting stuck: a keyup that was
 * dispatched before the dialog existed would otherwise leave the key in the
 * list for the rest of the dialog, and the user would never be able to type
 * that letter again. */
static bool gxIbStillHolding(void) {
    int i, n = 0;
    for (i = 0; i < g_gx_ibHeldN; i++) {
        if ((GetAsyncKeyState(g_gx_ibHeldKey[i]) & 0x8000) != 0)
            g_gx_ibHeldKey[n++] = g_gx_ibHeldKey[i];
    }
    g_gx_ibHeldN = n;
    return n > 0;
}

/* ---------- window icon ---------------------------------------------------
 * Windows does NOT put an exe's embedded icon in the title bar by itself:
 * the window class has to carry it.  Leaving hIcon NULL (as this used to)
 * makes every easygl window fall back to the generic .exe icon even when
 * the program has a perfectly good icon compiled in - which is why EasyX
 * builds showed the app icon and easygl builds did not.
 *
 * Explorer picks the RT_GROUP_ICON resource with the lowest id as "the"
 * exe icon, so we enumerate and use the same one.  Nothing here fails:
 * if the module has no icon at all we fall back to IDI_APPLICATION. */

#define GX_RT_GROUP_ICON_A  ((LPCSTR)((ULONG_PTR)14))

struct GxIconPick { int id; };

static BOOL CALLBACK gxPickIconProc(HMODULE hMod, LPCSTR type, LPSTR name,
                                    LONG_PTR lParam)
{
    struct GxIconPick *p = (struct GxIconPick *)lParam;
    (void)hMod; (void)type;
    if (IS_INTRESOURCE(name)) {           /* numeric id, not a string name */
        int id = (int)(WORD)(ULONG_PTR)name;
        if (p->id < 0 || id < p->id) p->id = id;
    }
    return TRUE;                          /* keep enumerating */
}

static int gxPickIconId(HINSTANCE hInst)
{
    struct GxIconPick pick;
    pick.id = -1;
    if (hInst)
        EnumResourceNamesA(hInst, GX_RT_GROUP_ICON_A,
                           gxPickIconProc, (LONG_PTR)&pick);
    return pick.id;
}

static HICON gxLoadAppIcon(HINSTANCE hInst)
{
    int id = gxPickIconId(hInst);
    HICON ic = NULL;
    if (id >= 0) {
        ic = (HICON)LoadImageA(hInst, MAKEINTRESOURCEA(id), IMAGE_ICON,
                               GetSystemMetrics(SM_CXICON),
                               GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
        if (!ic) ic = LoadIconA(hInst, MAKEINTRESOURCEA(id));
    }
    if (!ic) ic = LoadIconA(NULL, (LPCSTR)IDI_APPLICATION);
    return ic;
}

static HICON gxLoadAppIconSm(HINSTANCE hInst)
{
    int id = gxPickIconId(hInst);
    HICON ic = NULL;
    if (id >= 0)
        ic = (HICON)LoadImageA(hInst, MAKEINTRESOURCEA(id), IMAGE_ICON,
                               GetSystemMetrics(SM_CXSMICON),
                               GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    /* NULL is fine here - the class then derives the small icon from hIcon */
    return ic;
}

/* The full dialog.  prompt / title / def are the text, def pre-fills the
 * edit box; width / height are the WINDOW size in pixels, 0 meaning "work
 * it out"; bHideCancelBtn is EasyX's own eighth parameter - TRUE hides the
 * Cancel button, so the dialog ends up with a single OK.  EasyX defaults it
 * to true, so that is what every shorter form gets.
 *
 * Older EasyX headers called the same parameter bOnlyOK.  The two names
 * mean the same thing from opposite ends (hide Cancel == only OK), so the
 * value a program passes is identical either way - only the spelling of the
 * argument changed. */
static bool gxInputBoxExW(const WCHAR* prompt, const WCHAR* title,
                          const WCHAR* def, WCHAR* out, int outLen,
                          int width, int height, bool bHideCancelBtn) {
    HINSTANCE hInst = GetModuleHandleA(NULL);
    RECT rc, fr;
    int ww, wh, m, promptH, ey, eh, bw, bh, bg, by, fw = 0, fh = 0;
    static bool ibClassReg = false;
    HWND hOk = NULL, hCancel = NULL;
    MSG msg;

    if (!out || outLen < 1) return false;
    out[0] = 0;
    if (!ibClassReg) {
        WNDCLASSEXA wc;
        memset(&wc, 0, sizeof(wc));
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = gxInputBoxProc;
        wc.hInstance = hInst;
        wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = "GX_InputBox";
        wc.hIcon   = gxLoadAppIcon(hInst);
        wc.hIconSm = gxLoadAppIconSm(hInst);
        RegisterClassExA(&wc);
        ibClassReg = true;
    }
    /* width / height are the window size, like EasyX - not the client
     * area.  AdjustWindowRect() on an empty rectangle gives back exactly
     * the frame and caption it will add, so subtracting that leaves the
     * client size the caller's pixels really buy. */
    memset(&fr, 0, sizeof(fr));
    AdjustWindowRect(&fr, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE);
    fw = fr.right - fr.left;
    fh = fr.bottom - fr.top;
    if (fw < 0) fw = 0;
    if (fh < 0) fh = 0;

    /* Every measurement below is a 96 dpi number put through gxIbS(), so
     * the dialog is the same size in real terms at 150% or 200%. */
    m  = gxIbS(20);                        /* side margin          */
    ww = gxIbS(GX_IB_W);
    if (width > 0) {
        int cw = width - fw;
        if (cw < gxIbS(160)) cw = gxIbS(160);   /* never clip the controls */
        ww = cw;
    }
    promptH = gxIbPromptHeight(prompt, ww - m * 2);
    if (promptH < gxIbS(16)) promptH = gxIbS(16);
    ey  = gxIbS(18) + promptH + gxIbS(12); /* edit box top         */
    eh  = gxIbS(24);                       /* edit box height      */
    bw  = gxIbS(80); bh = gxIbS(26);       /* button size          */
    bg  = gxIbS(10);                       /* gap between buttons  */
    by  = ey + eh + gxIbS(16);             /* button top           */
    wh  = by + bh + gxIbS(16);             /* client height        */
    if (height > 0) {
        int ch = height - fh;
        if (ch > wh) { wh = ch; by = wh - bh - gxIbS(16); }
    }
    memset(&rc, 0, sizeof(rc));
    rc.right = ww; rc.bottom = wh;
    AdjustWindowRect(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE);
    g_gx_ibDone = false;
    g_gx_ibOk = false;
    g_gx_ibText[0] = 0;
    g_gx_ibWnd = CreateWindowExA(0, "GX_InputBox", "InputBox",
                              WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
                              CW_USEDEFAULT, CW_USEDEFAULT,
                              rc.right - rc.left, rc.bottom - rc.top,
                              gxDialogOwner(), NULL, hInst, NULL);
    if (!g_gx_ibWnd) return false;
    SetWindowTextW(g_gx_ibWnd, title ? title : L"InputBox");
    g_gx_ibEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                               WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                               m, ey, ww - m * 2, eh, g_gx_ibWnd, NULL, hInst, NULL);
    if (g_gx_ibEdit) {
        /* nMaxCount is the size of the caller's buffer: telling the edit
         * control about it is the only way to stop the user typing past
         * the end of it.  EasyX counts characters, so the NUL is excluded
         * the same way it is on the way out. */
        SendMessageA(g_gx_ibEdit, EM_LIMITTEXT,
                     (WPARAM)(outLen > 1 ? outLen - 1 : 0), 0);
        if (def && def[0]) SetWindowTextW(g_gx_ibEdit, def);
    }
    /* Captions come from user32.dll, so they read the same as a MessageBox
     * would in whatever language the system is set to.  Created with the W
     * flavour so the text is not squeezed through the current ANSI code
     * page on the way in. */
    {
        WCHAR okTxt[64], cancelTxt[64];
        gxSysCaption(GX_IDS_OK,     L"OK",     okTxt,     64);
        if (bHideCancelBtn) {
            /* Cancel is hidden, which is EasyX's default: one button,
             * centred where the pair would have been.  The only way out of
             * the dialog other than OK is then closing it, and that
             * reports false like Cancel does. */
            hOk = CreateWindowExW(0, L"BUTTON", okTxt,
                                  WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                  (ww - bw) / 2, by, bw, bh,
                                  g_gx_ibWnd, (HMENU)1, hInst, NULL);
        } else {
            gxSysCaption(GX_IDS_CANCEL, L"Cancel", cancelTxt, 64);
            hOk = CreateWindowExW(0, L"BUTTON", okTxt,
                                  WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
                                  ww / 2 - bw - bg / 2, by, bw, bh,
                                  g_gx_ibWnd, (HMENU)1, hInst, NULL);
            hCancel = CreateWindowExW(0, L"BUTTON", cancelTxt,
                                      WS_CHILD | WS_VISIBLE,
                                      ww / 2 + bg / 2, by, bw, bh,
                                      g_gx_ibWnd, (HMENU)2, hInst, NULL);
        }
    }
    /* Each control has to be TOLD the font.  Setting it on the edit box
     * alone left both buttons on the system default, so the edit field and
     * the buttons were rendered in two different faces - the "odd font"
     * the dialog used to have. */
    {
        HFONT f = gxUiFont();
        if (g_gx_ibEdit) SendMessageA(g_gx_ibEdit, WM_SETFONT, (WPARAM)f, MAKELONG(TRUE, 0));
        if (hOk)      SendMessageA(hOk,      WM_SETFONT, (WPARAM)f, MAKELONG(TRUE, 0));
        if (hCancel)  SendMessageA(hCancel,  WM_SETFONT, (WPARAM)f, MAKELONG(TRUE, 0));
    }
    /* Only stashed here - WM_PAINT draws it.  See g_gx_ibPrompt above. */
    if (prompt) {
        int i;
        for (i = 0; i < 511 && prompt[i]; i++) g_gx_ibPrompt[i] = prompt[i];
        g_gx_ibPrompt[i] = 0;
    } else {
        g_gx_ibPrompt[0] = 0;
    }
    ShowWindow(g_gx_ibWnd, SW_SHOW);
    SetForegroundWindow(g_gx_ibWnd);
    if (g_gx_ibEdit) {
        /* SELECT THE WHOLE default, the way a browser's address bar or a
         * rename field does it: the text is there to be accepted or
         * replaced, so the first keystroke must replace it outright.
         * Leaving the caret at the end instead means typing APPENDS to the
         * default - someone who wants "Earth" has to select or backspace
         * "New Planet" away first, and a click that lands in the middle of
         * the text silently produces something neither of them wanted.
         * EM_SETSEL with (0, -1) is "everything": the end position is
         * clamped to the text length, so an empty box is a no-op. */
        SetFocus(g_gx_ibEdit);
        SendMessageA(g_gx_ibEdit, EM_SETSEL, (WPARAM)0, (LPARAM)-1);
    } else {
        SetFocus(g_gx_ibWnd);
    }
    /* After the edit box has the focus and before the loop starts: see
     * gxIbDrainKeys() and gxIbNoteHeld(). */
    gxIbDrainKeys();
    gxIbNoteHeld();
    while (!g_gx_ibDone) {
        if (GetMessageA(&msg, NULL, 0, 0) <= 0) break;
        /* Anything the key that opened the dialog is still producing - the
         * auto repeat, and the keyup that ends it - is dropped here: see
         * gxIbNoteHeld().  It runs BEFORE the Enter / Escape block below,
         * so a held Enter or Escape cannot confirm or cancel the dialog
         * before the user has even seen it. */
        if (g_gx_ibHeldN > 0 &&
            (msg.message == WM_KEYDOWN   || msg.message == WM_SYSKEYDOWN ||
             msg.message == WM_CHAR      || msg.message == WM_SYSCHAR ||
             msg.message == WM_DEADCHAR  || msg.message == WM_SYSDEADCHAR ||
             msg.message == WM_KEYUP     || msg.message == WM_SYSKEYUP)) {
            (void)gxIbStillHolding();
            continue;
        }
        /* Enter and Escape are handled here rather than left to the edit
         * control.  This window is built by hand instead of from a dialog
         * template, so there is no IsDialogMessage() in the loop to turn
         * Enter into a click on the default button, and BS_DEFPUSHBUTTON
         * only draws the thicker border - it does not respond to keys on
         * its own.  Unhandled, the edit control swallows Enter (it beeps,
         * or does nothing) and the dialog sits there.
         *
         * Enter is only taken when the edit box or the dialog itself has
         * focus: if a button has it, that button owns Enter already and
         * must be allowed to fire - otherwise Enter on Cancel would
         * confirm instead of cancelling.  Escape always cancels. */
        if (msg.message == WM_KEYDOWN &&
            (msg.hwnd == g_gx_ibWnd || (HWND)GetParent(msg.hwnd) == g_gx_ibWnd)) {
            if (msg.wParam == VK_ESCAPE) { gxIbFinish(false); continue; }
            if (msg.wParam == VK_RETURN &&
                (msg.hwnd == g_gx_ibEdit || msg.hwnd == g_gx_ibWnd)) {
                gxIbFinish(true);
                continue;
            }
        }
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
    g_gx_ibWnd = NULL;
    g_gx_ibEdit = NULL;
    if (g_gx_ibOk) {
        int i;
        for (i = 0; i < outLen - 1 && g_gx_ibText[i]; i++) out[i] = g_gx_ibText[i];
        out[i] = 0;
    }
    return g_gx_ibOk;
}

/* The old four argument form: no default text, auto sized, EasyX's own
 * default of one OK button. */
static bool gxInputBoxW(const WCHAR* prompt, const WCHAR* title,
                        WCHAR* out, int outLen) {
    return gxInputBoxExW(prompt, title, NULL, out, outLen, 0, 0, true);
}

static bool gx_inputbox_w(const WCHAR* prompt, const WCHAR* title, WCHAR* out, int n) {
    return gxInputBoxW(prompt, title, out, n);
}

/*======================================================================
 * 16. drawtext
 *====================================================================*/
/* Returns what EasyX documents: the text height, or - for DT_VCENTER /
 * DT_BOTTOM - the distance from pRect->top down to the bottom of the text.
 * 0 means nothing was drawn. */
static int gx_drawtext(const WCHAR* str, int len, const RECT* pr, UINT fmt,
                       double px, double py, bool usePos) {
    int tw = 0, th = 0;
    double x, y;
    RECT r;
    if (!str || !*str) return 0;
    gxMeasureW(str, len, &tw, &th);
    if (usePos) {
        x = px; y = py;
    } else {
        r = *pr;
        x = r.left;
        y = r.top;
        if (fmt & DT_RIGHT)        x = r.right - tw;
        else if (fmt & DT_CENTER)  x = (r.left + r.right - tw) / 2;
        if (fmt & DT_BOTTOM)       y = r.bottom - th;
        else if (fmt & DT_VCENTER) y = (r.top + r.bottom - th) / 2;
    }
    gxDrawW(x, y, str, len);
    if (!usePos && (fmt & (DT_VCENTER | DT_BOTTOM))) return (y - r.top) + th;
    return th;
}

/*======================================================================
 * 17. Canvas / framebuffer / lifetime
 *====================================================================*/
static void gxCreateCanvas(void) {
    glGenTextures(1, &g_gx_canvasTex);
    glBindTexture(GL_TEXTURE_2D, g_gx_canvasTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, g_gx_devW, g_gx_devH, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    glGenFramebuffers(1, &g_gx_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, g_gx_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, g_gx_canvasTex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        MessageBoxA(NULL, "Cannot create FBO (OpenGL 3.0+ required)",
                    "OpenGL Error", MB_OK);
        exit(1);
    }
    g_gx_canvasTarget.fbo = g_gx_fbo;
    g_gx_canvasTarget.tex = g_gx_canvasTex;
    g_gx_canvasTarget.w = g_gx_devW;
    g_gx_canvasTarget.h = g_gx_devH;
    g_gx_target = &g_gx_canvasTarget;
    glBindFramebuffer(GL_FRAMEBUFFER, g_gx_fbo);
    gxApplyWindowState();   /* MSAA now, vsync once g_gx_glReady is set */

    glGenTextures(1, &g_gx_atlasTex);
    glBindTexture(GL_TEXTURE_2D, g_gx_atlasTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GX_ATLAS_W, GX_ATLAS_H, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, gxEnsureAtlas());
}

static void gxCreateBuffers(void) {
    static const float blit[] = {
        /* x, y, r,g,b,a, u, v */
        -1.f, -1.f, 1,1,1,1, 0.f, 0.f,
         1.f, -1.f, 1,1,1,1, 1.f, 0.f,
         1.f,  1.f, 1,1,1,1, 1.f, 1.f,
        -1.f, -1.f, 1,1,1,1, 0.f, 0.f,
         1.f,  1.f, 1,1,1,1, 1.f, 1.f,
        -1.f,  1.f, 1,1,1,1, 0.f, 1.f
    };
    glGenBuffers(1, &g_gx_vbo);
    glGenBuffers(1, &g_gx_ibo);
    glGenBuffers(1, &g_gx_blitVbo);
    glBindBuffer(GL_ARRAY_BUFFER, g_gx_blitVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(blit), blit, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

static void gxPresent(void) {
    static const float ident[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
    static DWORD postT0 = 0;          /* origin for the uTime uniform */
    if (!g_gx_glReady) return;
    gxFlush();
    gxMsaaResolve();            /* the blit below samples g_gx_canvasTex */
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    /* The back buffer has the size of the *window* (the canvas), not the
     * size of the IMAGE that may currently be the working target. */
    glViewport(0, 0, g_gx_canvasTarget.w, g_gx_canvasTarget.h);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_COLOR_LOGIC_OP);
    glDisable(GL_BLEND);
    if (g_gx_postProg) {
        /* The one draw the caller's shader sees.  uProj is identity here
         * because the blit quad is already in clip space. */
        if (!postT0) postT0 = GetTickCount();
        glUseProgram(g_gx_postProg);
        if (g_gx_postProj >= 0) glUniformMatrix4fv(g_gx_postProj, 1, GL_FALSE, ident);
        if (g_gx_postTexel >= 0)
            glUniform2f(g_gx_postTexel, 1.f / (float)g_gx_canvasTarget.w,
                        1.f / (float)g_gx_canvasTarget.h);
        if (g_gx_postTime >= 0)
            glUniform1f(g_gx_postTime, (float)(GetTickCount() - postT0) * 0.001f);
    } else {
        glUniformMatrix4fv(g_gx_uProj, 1, GL_FALSE, ident);
    }
    glBindBuffer(GL_ARRAY_BUFFER, g_gx_blitVbo);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vtx), (const void*)0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vtx), (const void*)8);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vtx), (const void*)24);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_gx_canvasTex);
    /* uUseTex belongs to g_gx_prog, so it must not be touched while the post
     * program is current; that program binds uTex to unit 0 itself. */
    if (!g_gx_postProg) glUniform1i(g_gx_uUseTex, 2);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    if (g_gx_postProg) glUseProgram(g_gx_prog);
    glEnable(GL_BLEND);
    SwapBuffers(g_gx_hdc);
    gxFpsTick();          /* count the frame and honour settargetfps() */
    glBindFramebuffer(GL_FRAMEBUFFER, g_gx_target ? g_gx_target->fbo : 0);
    glViewport(0, 0, g_gx_target->w, g_gx_target->h);
    glUniformMatrix4fv(g_gx_uProj, 1, GL_FALSE, g_gx_proj);
    gxApplyClip();
}

static void gxDestroyGL(void) {
    int i;
    gxImgBufDropAll();
    if (!g_gx_glReady) return;
    gxMsaaDestroy();          /* before g_gx_fbo goes away */
    GxVtxVec_clear(&g_gx_vbuf);
    GxCmdVec_clear(&g_gx_cmds);
    g_gx_cmdStart = 0;
    if (g_gx_vbo) glDeleteBuffers(1, &g_gx_vbo);
    if (g_gx_ibo) glDeleteBuffers(1, &g_gx_ibo);
    if (g_gx_blitVbo) glDeleteBuffers(1, &g_gx_blitVbo);
    g_gx_vboCap = 0;
    free(g_gx_idx);
    g_gx_idx = NULL;
    g_gx_idxQuads = g_gx_iboQuads = g_gx_iboUpTo = 0;
    g_gx_quadRun = false;
    if (g_gx_fbo) glDeleteFramebuffers(1, &g_gx_fbo);
    if (g_gx_canvasTex) glDeleteTextures(1, &g_gx_canvasTex);
    if (g_gx_atlasTex) glDeleteTextures(1, &g_gx_atlasTex);
    for (i = 0; i < 6; i++) {
        if (g_gx_hatchTex[i]) { glDeleteTextures(1, &g_gx_hatchTex[i]); g_gx_hatchTex[i] = 0; }
    }
    gxPostDestroy();
    if (g_gx_prog) glDeleteProgram(g_gx_prog);
    g_gx_vbo = g_gx_blitVbo = g_gx_fbo = g_gx_canvasTex = g_gx_atlasTex = g_gx_prog = 0;
    memset(&g_gx_canvasTarget, 0, sizeof(g_gx_canvasTarget));
    g_gx_glReady = false;
}

static void gxFreeText(void) {
    size_t i;
    for (i = 0; i < g_gx_fonts.size; i++)
        if (g_gx_fonts.data[i].hfont) DeleteObject(g_gx_fonts.data[i].hfont);
    GxFontVec_free(&g_gx_fonts);
    gxStrMapFree(&g_gx_fontIds);
    gxGlyphMapFree(&g_gx_glyphs);
    if (g_gx_fontDC) { DeleteDC(g_gx_fontDC); g_gx_fontDC = NULL; }
    free(g_gx_atlas);
    g_gx_atlas = NULL;
    g_gx_packX = g_gx_packY = g_gx_packRowH = 0;
}

/* Reset the whole drawing state to the EasyX defaults.  dropMessages also
 * empties the input queue, which initgraph() wants but graphdefaults()
 * must not do. */
static void gxInitState(bool dropMessages) {
    gxInitFontDefault();
    if (dropMessages) gxMsgInit();
    g_gx_curX = g_gx_curY = 0;
    g_gx_originX = g_gx_originY = 0.f;
    g_gx_scaleX = g_gx_scaleY = 1.f;
    g_gx_canvasScaleX = g_gx_canvasScaleY = 1.f;
    g_gx_canvasOriginX = g_gx_canvasOriginY = 0.f;
    g_gx_reqOriginX = g_gx_reqOriginY = 0.f;
    /* Matches the reset above: with no setaspectratio() pending, a later
     * fixhighdpi() has to re-derive from 1, not from a stale request. */
    g_gx_reqScaleX = g_gx_reqScaleY = 1.f;
    g_gx_clipOn = false;
    gxUpdateProj();
    g_gx_fillStyle.style = BS_SOLID;
    g_gx_fillStyle.hatch = 0;
    g_gx_fillStyle.ppattern = NULL;
    g_gx_lineStyle.style = PS_SOLID;
    g_gx_lineStyle.thickness = 1;
    g_gx_lineStyle.puserstyle = NULL;
    g_gx_lineStyle.userstylecount = 0;
    g_gx_lineWidth = 1;
    gxDashReset();
    g_gx_rop2 = R2_COPYPEN;
    g_gx_curRop = R2_COPYPEN;
    g_gx_fillColor = WHITE;
    g_gx_lineColor = WHITE;
    g_gx_textColor = WHITE;
    g_gx_bkColor   = BLACK;
    g_gx_bkMode    = TRANSPARENT;
    g_gx_polyMode  = ALTERNATE;
    g_gx_alpha     = 1.f;
    g_gx_curAlpha  = 1.f;
    g_gx_blend     = GX_BLEND_ALPHA;
    g_gx_curBlend  = GX_BLEND_ALPHA;
    g_gx_batchDraw = false;
    /* g_gx_renderMode is deliberately NOT reset here: graphdefaults() shares
     * this function, and EasyX lists only the view, the current point, the
     * colours, the line style, the fill style and the font as "defaults" -
     * the render mode belongs to the window and survives.  initgraph()
     * establishes it from the INIT_RENDERMANUAL flag. */
    if (g_gx_glReady) {
        glUniformMatrix4fv(g_gx_uProj, 1, GL_FALSE, g_gx_proj);
        gxBindTarget();
    }
}

/* Stop Windows from ever flagging this process's windows as hung.
 *
 * The "not responding" state is not a diagnosis, it is a timeout: DWM sends
 * the window a WM_NULL and, if the answer does not come back within a few
 * seconds, it stops asking, freezes the frame, greys the caption and puts
 * "(Not Responding)" in the title bar.  The window is not dead - it just did
 * not answer that one message in time, and a program that is busy drawing or
 * waiting reaches that legitimately.  Worse, the ghosted frame is frozen, so
 * anything drawn afterwards is not shown.
 *
 * DisableProcessWindowsGhosting() turns that off for the whole process.
 * Nothing else changes: the window still gets its messages, still draws,
 * still answers the mouse.  Vista and later; resolved by pointer, so on
 * anything older this is simply a no-op.  Define GX_ALLOW_GHOSTING to keep
 * the Windows default. */
static void gxNoGhosting(void) {
    HMODULE hUser;
    typedef void (WINAPI *PFN_DPWG)(void);
    PFN_DPWG pDisable;
#ifdef GX_ALLOW_GHOSTING
    return;
#else
    hUser = GetModuleHandleA("user32.dll");
    if (!hUser) return;
    pDisable = (PFN_DPWG)GetProcAddress(hUser, "DisableProcessWindowsGhosting");
    if (pDisable) pDisable();
#endif
}

static HWND gxInitGraph(int w, int h) {
    HINSTANCE hInst;
    DWORD style;
    RECT rc;
    PIXELFORMATDESCRIPTOR pfd;
    int pf;
    HGLRC tmp;
    PFN_WGLCREATECTXATTRIBS wglCreateContextAttribsARB;

    /* Before the window exists.  A process is only ever ghosted through its
     * windows, and the flag is read when a window is created, so calling
     * this after CreateWindowEx() is too late for the window that matters. */
    gxNoGhosting();

    if (w < 1) w = 1;
    if (h < 1) h = 1;

    if (g_gx_hwnd) {
        gxDestroyGL();
        gxFreeText();
        if (g_gx_hglrc) { wglMakeCurrent(NULL, NULL); wglDeleteContext(g_gx_hglrc); g_gx_hglrc = NULL; }
        if (g_gx_hdc) { ReleaseDC(g_gx_hwnd, g_gx_hdc); g_gx_hdc = NULL; }
        DestroyWindow(g_gx_hwnd);
        g_gx_hwnd = NULL;
    }
    gxInitState(true);
    /* EasyX starts in RENDER_AUTO, i.e. every primitive is shown as it is
     * drawn and a plain loop works without BeginBatchDraw().  This used to
     * be RENDER_MANUAL, which left the window blank (the canvas was drawn
     * but never presented). */
    g_gx_renderMode = (g_gx_initFlag & INIT_RENDERMANUAL) ? RENDER_MANUAL : RENDER_AUTO;

    hInst = GetModuleHandleA(NULL);
    {
        static bool clsReg = false;
        if (!clsReg) {
            WNDCLASSEXA wc;
            memset(&wc, 0, sizeof(wc));
            wc.cbSize = sizeof(wc);
            wc.lpfnWndProc = gxWndProc;
            wc.hInstance = hInst;
            wc.hCursor = LoadCursorA(NULL, (LPCSTR)IDC_ARROW);
            wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
            wc.style = CS_DBLCLKS | CS_OWNDC;
            wc.lpszClassName = "GX_OpenGL_Window";
            wc.hIcon   = gxLoadAppIcon(hInst);
            wc.hIconSm = gxLoadAppIconSm(hInst);
            RegisterClassExA(&wc);
            clsReg = true;
        }
    }

    style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    if (g_gx_initFlag & INIT_NOBORDER) style = WS_POPUP;
    /* EX_NOMINIMIZE removes the button, INIT_MINIMIZE (easygl) only starts
     * the window iconified - the two are unrelated. */
    if (g_gx_initFlag & NOMINIMIZE) style &= ~WS_MINIMIZEBOX;
    /* variablewinsize(true) called before initgraph(): build the window
     * with the frame already on, so there is no redraw on startup.  It wins
     * over EX_NOMINIMIZE on the maximise button - see the note there. */
    if (g_gx_varWinSize) style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
    rc.left = 0; rc.top = 0; rc.right = w; rc.bottom = h;
    AdjustWindowRect(&rc, style, FALSE);
    g_gx_hwnd = CreateWindowExA(0, "GX_OpenGL_Window", "OpenGL", style,
                             CW_USEDEFAULT, CW_USEDEFAULT,
                             rc.right - rc.left, rc.bottom - rc.top,
                             NULL, NULL, hInst, NULL);
    if (!g_gx_hwnd) {
        MessageBoxA(NULL, "Failed to create window", "Error", MB_OK);
        exit(1);
    }
    if (g_gx_initFlag & NOCLOSE) {
        HMENU sm = GetSystemMenu(g_gx_hwnd, FALSE);
        if (sm) EnableMenuItem(sm, SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);
    }
    /* EasyX shows the console only when EX_SHOWCONSOLE asks for it.  Most
     * MinGW builds are /SUBSYSTEM:CONSOLE because nobody passes -mwindows,
     * so without this the run leaves a black cmd window sitting behind the
     * graphics window.  Hiding is skipped when there is no console at all
     * (a -mwindows program), which is why gxHasConsole() is tested.
     *
     * CAUTION: hiding the console breaks every program that reads the
     * keyboard through conio.  A hidden console window cannot take the
     * focus, so getch() / _getch() block forever; the main thread then
     * stops pumping the graphics window and Windows reports it as
     * "not responding".  printf() output disappears as well.
     * Define GX_NO_AUTO_HIDE_CONSOLE before including this header to turn
     * the automatic hiding off (per call, initgraph(w, h, EX_SHOWCONSOLE)
     * or a later showconsole() still works). */
    if (g_gx_initFlag & SHOWCONSOLE) gxConsoleShow(true);
#ifndef GX_NO_AUTO_HIDE_CONSOLE
    else if (gxHasConsole())      gxConsoleShow(false);
#endif
    g_gx_hdc = GetDC(g_gx_hwnd);

    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cAlphaBits = 8;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pf = ChoosePixelFormat(g_gx_hdc, &pfd);
    if (!pf || !SetPixelFormat(g_gx_hdc, pf, &pfd)) {
        MessageBoxA(NULL, "Failed to set pixel format", "Error", MB_OK);
        exit(1);
    }

    tmp = wglCreateContext(g_gx_hdc);
    wglMakeCurrent(g_gx_hdc, tmp);
    wglCreateContextAttribsARB =
        (PFN_WGLCREATECTXATTRIBS)wglGetProcAddress("wglCreateContextAttribsARB");
    if (wglCreateContextAttribsARB) {
        int attribs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
            WGL_CONTEXT_MINOR_VERSION_ARB, 3,
            WGL_CONTEXT_FLAGS_ARB, 0,
            WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
            0, 0
        };
        /* Try the requested version, then 3.3 (what this library needs for
         * its framebuffers), then give up and keep the default context. */
        int req[4];
        int n = 0, k;
        req[n++] = g_gx_reqGLMajor; req[n++] = g_gx_reqGLMinor;
        if (g_gx_reqGLMajor != 3 || g_gx_reqGLMinor != 3) { req[n++] = 3; req[n++] = 3; }
        for (k = 0; k < n; k += 2) {
            HGLRC rc2;
            attribs[1] = req[k];
            attribs[3] = req[k + 1];
            rc2 = wglCreateContextAttribsARB(g_gx_hdc, NULL, attribs);
            if (rc2) {
                wglMakeCurrent(NULL, NULL);
                wglDeleteContext(tmp);
                tmp = rc2;
                g_gx_glMajor = req[k];
                g_gx_glMinor = req[k + 1];
                break;
            }
        }
    }
    if (g_gx_glMajor == 0) { g_gx_glMajor = 1; g_gx_glMinor = 1; }   /* default ctx */
    g_gx_hglrc = tmp;
    wglMakeCurrent(g_gx_hdc, g_gx_hglrc);

    gxLoadGL();
    if (!glCreateShader || !glBindBuffer || !glGenFramebuffers || !glVertexAttribPointer) {
        MessageBoxA(NULL, "Failed to load OpenGL entry points (OpenGL 3.0+ driver required)",
                    "Error", MB_OK);
        exit(1);
    }
    g_gx_devW = w; g_gx_devH = h;
    /* The size initgraph() was asked for, before any DPI scaling: a rebuilt
     * window starts here and gxRestoreDpiFix() scales it up again. */
    g_gx_baseW = w; g_gx_baseH = h;
    g_gx_scaleX = g_gx_scaleY = 1.f;
    g_gx_canvasScaleX = g_gx_canvasScaleY = 1.f;
    g_gx_canvasOriginX = g_gx_canvasOriginY = 0.f;
    g_gx_reqOriginX = g_gx_reqOriginY = 0.f;
    gxUpdateProj();

    gxCreateProgram();
    gxCreateBuffers();
    gxCreateCanvas();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_COLOR_LOGIC_OP);
    glBindFramebuffer(GL_FRAMEBUFFER, g_gx_fbo);
    glViewport(0, 0, g_gx_devW, g_gx_devH);
    glUniformMatrix4fv(g_gx_uProj, 1, GL_FALSE, g_gx_proj);

    g_gx_glReady = true;
    gxApplyWindowState();   /* reapply vsync / MSAA to the new window */
    g_gx_target = &g_gx_canvasTarget;
    g_gx_workImg = NULL;
    /* gxCreateCanvas() allocated the canvas texture with NULL data, so its
     * contents are undefined: present it as it stands and the window shows
     * whatever that memory last held, which is the speckle that used to
     * force a cleardevice() right after initgraph().  EasyX hands over a
     * window already in the background colour, so clear to g_gx_bkColor here.
     *
     * It has to run after gxApplyWindowState(): with MSAA on the canvas
     * framebuffer is the multisample renderbuffer, and that is undefined
     * too - clearing the plain texture first would only be overwritten by
     * the next resolve.  Before ShowWindow(), so nothing is ever shown. */
    gxClearFbo(g_gx_canvasTarget.fbo, g_gx_canvasTarget.w, g_gx_canvasTarget.h,
               g_gx_bkColor, 1.f);
#ifdef GX_EASYX_HWND
    hwnd = g_gx_hwnd;
#endif
    if (!(g_gx_initFlag & INIT_HIDE)) ShowWindow(g_gx_hwnd, SW_SHOW);
    if (g_gx_initFlag & INIT_MINIMIZE) ShowWindow(g_gx_hwnd, SW_MINIMIZE);
    UpdateWindow(g_gx_hwnd);
    SetForegroundWindow(g_gx_hwnd);   /* IsWindowActive() relies on it */
    SetFocus(g_gx_hwnd);
    gxPump();
    return g_gx_hwnd;
}

static HWND gx_initgraph2(int w, int h) {
    g_gx_initFlag = INIT_DEFAULT;
    return gxInitGraph(w, h);
}
static HWND gx_initgraph3(int w, int h, int flag) {
    g_gx_initFlag = flag;
    return gxInitGraph(w, h);
}

static void closegraph(void) {
    /* Give the console back: a program that ends with closegraph(); _getch();
     * would otherwise wait on a window that is no longer visible. */
    if (g_gx_consoleHidden) gxConsoleShow(true);
    gxImgBufDropAll();
    gxDestroyGL();
    gxFreeText();
    if (g_gx_hglrc) { wglMakeCurrent(NULL, NULL); wglDeleteContext(g_gx_hglrc); g_gx_hglrc = NULL; }
    if (g_gx_hdc) { ReleaseDC(g_gx_hwnd, g_gx_hdc); g_gx_hdc = NULL; }
    if (g_gx_hwnd) { DestroyWindow(g_gx_hwnd); g_gx_hwnd = NULL; }
#ifdef GX_EASYX_HWND
    hwnd = NULL;
#endif
    g_gx_workImg = NULL;
    g_gx_target = &g_gx_canvasTarget;
}

static void cleardevice(void) {
    if (!g_gx_glReady) return;
    GxVtxVec_clear(&g_gx_vbuf);   /* clear the canvas -> drop pending primitives */
    GxCmdVec_clear(&g_gx_cmds);
    g_gx_cmdStart = 0;
    glBindFramebuffer(GL_FRAMEBUFFER, g_gx_target ? g_gx_target->fbo : 0);
    glViewport(0, 0, g_gx_target->w, g_gx_target->h);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_COLOR_LOGIC_OP);
    glEnable(GL_BLEND);
    /* The background colour, not hard coded black: setbkcolor(WHITE);
     * cleardevice(); has to leave a white canvas, which is what EasyX does
     * and what the clear*() family has always done here. */
    glClearColor(GetRValue(g_gx_bkColor) / 255.f, GetGValue(g_gx_bkColor) / 255.f,
                 GetBValue(g_gx_bkColor) / 255.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    gxApplyClip();
}

static void setaspectratio(float sx, float sy) {
    /* Reject 0 and NaN, but keep NEGATIVE factors: EasyX allows them and
     * setaspectratio(1, -1) (usually together with setorigin(0, h)) is the
     * documented way of turning y upwards.  "x != x" is the NaN test; the
     * plain "!(x > 0.f)" test used to throw those flips away. */
    if (sx != sx || sx == 0.f) sx = 1.f;
    if (sy != sy || sy == 0.f) sy = 1.f;
    gxFlush();               /* same reason as setorigin() */
    /* Stored twice: the requested pair, so fixhighdpi() can re-derive the
     * effective one, and the effective one, which is the requested pair
     * times the DPI factor.  Negative factors keep their sign because the
     * factor is always positive. */
    g_gx_reqScaleX = sx;
    g_gx_reqScaleY = sy;
    /* Belongs to the canvas.  Applied at once when the canvas is current;
     * otherwise it waits in g_gx_canvasScaleX / Y until it is again. */
    g_gx_canvasScaleX = sx * g_gx_dpiFix;
    g_gx_canvasScaleY = sy * g_gx_dpiFix;
    if (!g_gx_workImg) {
        g_gx_scaleX = g_gx_canvasScaleX;
        g_gx_scaleY = g_gx_canvasScaleY;
    }
    gxUpdateProj();
    if (g_gx_glReady) {
        glUniformMatrix4fv(g_gx_uProj, 1, GL_FALSE, g_gx_proj);
        gxBindTarget();      /* the clip box scales with the coordinate space */
    }
}

/*--------------------------- high DPI fix ----------------------------*/
/* Window style / position flags used by the resize below.  Present in any
 * winuser.h, but the values are fixed by Windows, so a slim SDK is covered. */
#ifndef GWL_STYLE
#define GWL_STYLE       (-16)
#endif
#ifndef SWP_NOMOVE
#define SWP_NOMOVE      0x0002
#endif
#ifndef SWP_NOZORDER
#define SWP_NOZORDER    0x0004
#endif
#ifndef SWP_NOACTIVATE
#define SWP_NOACTIVATE  0x0010
#endif

/* Resize the graphics window.
 *
 * setwinsize(w, h) is the public name; gxResizeMainWindow() is the same
 * thing under its internal one.  The size is in DEVICE pixels - the size of
 * the window on screen and of the canvas behind it - which is a different
 * quantity from the LOGICAL extent getwidth() / getheight() report: with
 * setaspectratio(2, 2) in force an 800 x 600 window has a 400 x 300 logical
 * space.  Setting the window size does not change the scale, so it does
 * change the logical extent.
 *
 * Anything sized off the window follows: the canvas texture, the MSAA
 * buffer, the projection and the viewport.
 *
 * It lives next to the DPI code because that is what needed it first
 * (fixhighdpi() changes the window size), but it is a general facility. */
static void gxResizeMainWindow(int w, int h);
/* The canvas half of a resize, with no SetWindowPos(): gxWndProc()
 * calls it from WM_SIZE, where the window has already changed and
 * moving it again would re-enter this code forever. */
static void gxResizeCanvas(int w, int h);
GX_INLINE void setwinsize(int w, int h) {
    /* The logical size, so it is what a later rebuild scales from. */
    g_gx_baseW = w; g_gx_baseH = h;
    gxResizeMainWindow((int)((float)w * g_gx_dpiFix + 0.5f),
                       (int)((float)h * g_gx_dpiFix + 0.5f));
}

/* Read the window size back.  Either pointer may be NULL, the same
 * convention as getorigin() / getaspectratio().
 *
 * getwinsize() is the inverse of setwinsize() and therefore reports LOGICAL
 * units, so setwinsize(1024, 768); getwinsize(&w, &h); returns 1024x768 at
 * any DPI.  Returning g_gx_devW here instead made the pair asymmetric: at 150%
 * the same round trip gave back 1536x1152.
 *
 * getwindevsize() is the device form - the real pixel size of the window and
 * of the canvas texture, which is what Win32 calls and a glReadPixels() on
 * the canvas see. */
GX_INLINE void getwinsize(int* w, int* h) {
    if (w) *w = (int)((float)g_gx_devW / g_gx_dpiFix + 0.5f);
    if (h) *h = (int)((float)g_gx_devH / g_gx_dpiFix + 0.5f);
}
GX_INLINE void getwindevsize(int* w, int* h) {
    if (w) *w = g_gx_devW;
    if (h) *h = g_gx_devH;
}

/* Give the window a resizable frame (WS_THICKFRAME | WS_MAXIMIZEBOX), or
 * take it away again.
 *
 * EXPERIMENTAL.  It works, but it makes the window size something the
 * program no longer controls, and everything in this library was written
 * on the assumption that initgraph() settles the size once and for all.
 * Read the notes below before using it.
 *
 * May be called before or after initgraph():
 *   - before: the flag is remembered and the window is created with the
 *     frame already in place, so there is no visible flicker;
 *   - after:  the frame of the live window is changed with SetWindowLong()
 *     + SWP_FRAMECHANGED.  The client area keeps its size, so no resize
 *     happens on this call.
 *
 * What the program has to do
 * --------------------------
 *   int w, h;
 *   getwinsize(&w, &h);        - the current LOGICAL size, every frame
 *
 * The canvas is rebuilt to the new size, so getwinsize() / getwindevsize()
 * and getwidth() / getheight() all follow the window.  Nothing is scaled:
 * the picture is not stretched to fit, the drawing surface simply becomes
 * bigger or smaller, so layout has to be computed from the current size
 * rather than from the numbers passed to initgraph().  A program that
 * caches those numbers once will draw into the wrong place after a resize.
 *
 * The canvas is cleared to the background colour - see gxResizeCanvas() for
 * why it is the whole canvas and not only the part the window gained.  In
 * other words a resize discards the picture, so anything drawn outside the
 * main loop has to be drawn again.  In RENDER_AUTO (the default) a loop that
 * redraws every frame needs no extra code.
 *
 * A WM_SIZE is pushed to the message queue on every resize, so a program
 * that wants an event instead of polling can watch for it with
 * peekmessage(&m, EX_WINDOW) and compare m.message against WM_SIZE - or
 * simply call peekvariablemsg() / waitvariablemsg(), which do exactly that.
 * Neither one carries the size: read it with getwidth() / getheight().
 *
 * Order
 * -----
 *   Calling this BEFORE initgraph() is the clean way: the window is then
 *   built with the frame already on, so nothing has to be re-measured.
 *   Calling it afterwards still works - the frame grows outward so the
 *   client area (and therefore the coordinate space) is kept - but it is
 *   a second SetWindowPos() and, on a window that cannot grow, a canvas
 *   rebuild.
 *
 * Limits
 * ------
 *   - Dragging a border puts DefWindowProc() into a modal loop that
 *     suspends the program, so nothing is presented mid-drag; the frame is
 *     repainted by the WM_PAINT handler and the picture resumes when the
 *     drag ends.
 *   - Nothing clamps the size.  A very small window makes g_gx_devW / g_gx_devH
 *     small, which is legal but may make the content unreadable, and a
 *     canvas has to be allocated at whatever size is asked for.
 *   - The maximise button is turned on even when initgraph() was given
 *     EX_NOMINIMIZE; this call is the more specific request and wins. */
/* Make the live window's frame agree with g_gx_varWinSize.  Split out of
 * variablewinsize() because a rebuilt window needs the frame back too: it
 * is a per-window property exactly like vsync, MSAA, the image filter and
 * the DPI fix, so gxApplyWindowState() - the "reapply everything after a
 * rebuild" hook - calls this along with them.  Without it the flag would
 * only ever be honoured by the one CreateWindowEx() call inside
 * gxInitGraph(), and any other rebuild would silently drop the frame.
 *
 * Idempotent, which is what makes it safe to run on every rebuild: when
 * the style already matches, the rect asked for below is the one the
 * window already has, so SetWindowPos() changes nothing, no WM_SIZE
 * arrives and the canvas is left alone. */
static void gxRestoreVarWin(void) {
    LONG st;
    RECT rc, cr;
    if (!g_gx_hwnd) return;          /* no window yet: initgraph() reads the flag */

    st = GetWindowLongA(g_gx_hwnd, GWL_STYLE);
    if (g_gx_varWinSize) st |=  (LONG)(WS_THICKFRAME | WS_MAXIMIZEBOX);
    else              st &= ~(LONG)(WS_THICKFRAME | WS_MAXIMIZEBOX);
    /* Already what was asked for: leave the window completely alone.  This
     * is the case on every rebuild that did not change the setting, and it
     * is what keeps gxApplyWindowState() from resizing a window that is
     * already correct - a SetWindowPos() here would be harmless but it is
     * one more round trip through the window manager on each initgraph(). */
    if (st == GetWindowLongA(g_gx_hwnd, GWL_STYLE)) return;
    SetWindowLongA(g_gx_hwnd, GWL_STYLE, st);

    /* The frame is not the same thickness as the one it replaces: a window
     * that had WS_CAPTION carried the fixed 3 px frame (WS_DLGFRAME comes
     * along with WS_CAPTION), and WS_THICKFRAME swaps in the sizable 4 px
     * one.  So changing the style changes the non-client area, and with
     * NOMOVE | NOSIZE - which is what used to be passed here - the window
     * rect stays put and the CLIENT AREA shrinks by the difference on every
     * side.
     *
     * That leaves the canvas and the surface it is presented into a few
     * pixels apart, and because the GL viewport is anchored at the
     * bottom-left the strip the canvas no longer reaches shows up along the
     * top and the right - painted with the window class background, which
     * is a black brush.  It survives until something really resizes the
     * window, because only WM_SIZE resyncs the two.
     *
     * Two things fix it.  First, ask for a window rect that yields the SAME
     * client area under the NEW style, so the frame grows outward instead
     * of eating into the picture; that keeps the coordinate space the
     * program is already using.  Second, re-measure afterwards and rebuild
     * the canvas to whatever the window manager actually handed back - a
     * maximised window or one pinned against the screen edge cannot grow,
     * and in that case the canvas has to follow the client instead. */
    SetRect(&rc, 0, 0, g_gx_devW > 0 ? g_gx_devW : 1, g_gx_devH > 0 ? g_gx_devH : 1);
    AdjustWindowRect(&rc, (DWORD)st, FALSE);
    SetWindowPos(g_gx_hwnd, NULL, 0, 0, rc.right - rc.left, rc.bottom - rc.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE |
                 SWP_FRAMECHANGED);

    if (g_gx_glReady && GetClientRect(g_gx_hwnd, &cr)) {
        int nw = cr.right - cr.left, nh = cr.bottom - cr.top;
        if (nw >= 1 && nh >= 1 && (nw != g_gx_devW || nh != g_gx_devH)) {
            gxResizeCanvas(nw, nh);
            /* Same bookkeeping the WM_SIZE handler does: the logical size
             * has to follow the device size, or getwidth() lies. */
            g_gx_baseW = (int)((float)nw / g_gx_dpiFix + 0.5f);
            g_gx_baseH = (int)((float)nh / g_gx_dpiFix + 0.5f);
        }
    }
}

GX_INLINE void variablewinsize(bool enable) {
    g_gx_varWinSize = (enable != 0);
    /* No window yet: initgraph() reads g_gx_varWinSize while it builds the
     * window, so the frame is in place from the start and nothing has to be
     * re-measured.  With a window already up, change it in place. */
    gxRestoreVarWin();
}

/* The get side of variablewinsize(): whether the window currently carries
 * a resizable frame.  Reflects the last variablewinsize() call - it is the
 * flag, not a query of the live window style, so it is true from the moment
 * variablewinsize(true) returns even before initgraph() builds the window.
 *
 * No arguments, so it needs no overload and lives out here where both the
 * C and the C++ half of the header can see it. */
GX_INLINE bool getvariablewinsize(void) { return g_gx_varWinSize; }

static void gxResizeCanvas(int w, int h) {
    if (!g_gx_glReady) return;
    if (w < 1 || h < 1) return;
    if (w == g_gx_devW && h == g_gx_devH) return;     /* nothing to do */

    gxFlush();                    /* queued primitives carry the old size */
    gxMsaaDestroy();              /* sized off the canvas: rebuilt below  */

    g_gx_devW = w; g_gx_devH = h;
    /* Same texture id, new storage: glTexImage2D() redefines it. */
    glBindTexture(GL_TEXTURE_2D, g_gx_canvasTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    g_gx_canvasTarget.w = w;
    g_gx_canvasTarget.h = h;

    gxUpdateProj();
    gxMsaaCreate();               /* picks the new canvas size up itself  */
    gxBindTarget();
    /* glTexImage2D() redefined the ALL of the storage, so the whole canvas
     * is undefined again - not just the strip the window gained.  Painting
     * only the new area would leave the old one holding whatever the driver
     * handed back, so the clear covers everything.  Consequence: a resize
     * throws the picture away and the program has to redraw, which is what
     * EasyX does too.  Same reason as the clear in initgraph(): no
     * speckles, and in the background colour. */
    gxClearFbo(g_gx_canvasTarget.fbo, g_gx_canvasTarget.w, g_gx_canvasTarget.h,
               g_gx_bkColor, 1.f);
    if (g_gx_glReady) glUniformMatrix4fv(g_gx_uProj, 1, GL_FALSE, g_gx_proj);
}

static void gxResizeMainWindow(int w, int h) {
    DWORD style;
    RECT rc;
    if (!g_gx_glReady || !g_gx_hwnd) return;
    if (w < 1 || h < 1) return;
    if (w == g_gx_devW && h == g_gx_devH) return;     /* nothing to do */

    style = (DWORD)GetWindowLongA(g_gx_hwnd, GWL_STYLE);
    SetRect(&rc, 0, 0, w, h);
    AdjustWindowRect(&rc, style, FALSE);
    SetWindowPos(g_gx_hwnd, NULL, 0, 0, rc.right - rc.left, rc.bottom - rc.top,
                 SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    /* SetWindowPos() is synchronous: WM_SIZE has already run and rebuilt
     * the canvas by the time it returns, so this is only a fallback for the
     * case where the window did not actually move (same size, or a failed
     * call).  gxResizeCanvas() returns at once when the size is unchanged. */
    gxResizeCanvas(w, h);
}

/* On a scaled display a logical unit is still one 96 dpi pixel unless the
 * program accounts for the scaling itself, so a window laid out in logical
 * coordinates comes out physically small on a 4K panel at 150%.
 *
 * fixhighdpi() folds the system DPI into setaspectratio(): from then on
 *
 *     setaspectratio(2.0f, 2.0f)   on a 150% display (getdpi() == 144)
 *
 * behaves as setaspectratio(3.0f, 3.0f), because 144 / 96 == 1.5.  At
 * 100% the factor is exactly 1 and nothing changes, so the call is safe to
 * leave in unconditionally.
 *
 * The factor is applied to setaspectratio() only.  Nothing else in the
 * library is touched - a program that never calls setaspectratio() keeps
 * its 1:1 mapping unless it calls fixhighdpi(), which then scales the
 * default too.
 *
 * Default is OFF, so an existing program is unaffected.  Once switched on
 * it survives a window rebuild: gxApplyWindowState() reapplies it.  A
 * program that wants it from the start still has to call it after
 * initgraph(), because initgraph() resets the coordinate space. */
/* On / off in one call.  The state is remembered in g_gx_dpiFixOn, which is
 * what makes it survive a window rebuild: gxApplyWindowState() calls
 * gxRestoreDpiFix(), so a program that turned this on once keeps it on
 * across initgraph() / Resize() without doing anything. */
/* Work out the new factor and apply both halves of it: the scale, and the
 * window size that has to follow.  Returns false when nothing changed. */
static bool gxApplyDpiFix(bool on) {
    float newFix;
    int   nw, nh;

    newFix = on ? ((float)getdpi() / 96.f) : 1.f;
    if (!(newFix > 0.f)) newFix = 1.f;          /* nonsense DPI -> off  */

    g_gx_dpiFixOn = on;
    g_gx_dpiFix   = newFix;
    g_gx_canvasScaleX = g_gx_reqScaleX * newFix;
    g_gx_canvasScaleY = g_gx_reqScaleY * newFix;
    g_gx_canvasOriginX = g_gx_reqOriginX * newFix;
    g_gx_canvasOriginY = g_gx_reqOriginY * newFix;
    if (!g_gx_workImg) {
        g_gx_scaleX  = g_gx_canvasScaleX;
        g_gx_scaleY  = g_gx_canvasScaleY;
        g_gx_originX = g_gx_canvasOriginX;
        g_gx_originY = g_gx_canvasOriginY;
    }

    /* The window is sized from the request, not from its present size.
     *
     *   800 x 600 at 100%  ->  factor 1.0  ->  800 x 600 device pixels
     *   800 x 600 at 150%  ->  factor 1.5  -> 1200 x 900 device pixels
     *
     * Both are 8.3 inches across, which is the point: without this the
     * window would come out a third smaller on the scaled display.
     *
     * Deriving it from g_gx_baseW / g_gx_baseH rather than from the current
     * g_gx_devW / g_gx_devH is what makes it repeatable.  A rebuilt window comes
     * back at the logical size, and the old "multiply by how much the factor
     * changed" form had already applied its factor, so it saw 1.5 == 1.5 and
     * concluded there was nothing to do - the window stayed small.  Scaling
     * the request instead is idempotent: the same inputs give the same
     * window every time, however often it is called. */
    nw = (int)((float)g_gx_baseW * newFix + 0.5f);
    nh = (int)((float)g_gx_baseH * newFix + 0.5f);
    if (nw < 1) nw = g_gx_baseW;
    if (nh < 1) nh = g_gx_baseH;
    gxResizeMainWindow(nw, nh);   /* device pixels - not setwinsize(), which
                                   * would multiply by the factor again */
    gxUpdateProj();
    if (g_gx_glReady) {
        glUniformMatrix4fv(g_gx_uProj, 1, GL_FALSE, g_gx_proj);
        gxBindTarget();          /* the clip box scales with the space */
    }
    return true;
}

static void fixhighdpi_b(bool on) {
    if (!gxApplyDpiFix(on)) return;
    /* Re-derive from the requested pair, so switching either way takes
     * effect on the current setaspectratio() immediately. */
    setaspectratio(g_gx_reqScaleX, g_gx_reqScaleY);
}

/* Re-apply after a window rebuild.  Only when switched on - otherwise
 * there is nothing to restore.  The DPI is read again because the window
 * may have landed on a different monitor. */
static void gxRestoreDpiFix(void) {
    if (!g_gx_dpiFixOn) return;
    /* Same path as the switch itself: a rebuilt window starts at the plain
     * size, so both the scale and the window have to be redone. */
    gxApplyDpiFix(true);
    setaspectratio(g_gx_reqScaleX, g_gx_reqScaleY);
}

/* Back to 1:1 - read as fixhighdpi(false).  Kept because the pair reads
 * better next to each other. */
GX_INLINE void unfixhighdpi(void) { fixhighdpi_b(false); }

/* fixhighdpi()      -> on   (the usual call)
 * fixhighdpi(true)  -> on
 * fixhighdpi(false) -> off
 *
 * C++ gets two overloads; C11 gets the argument counting macro, which is
 * how the rest of the library fakes EasyX's default arguments. */
#if defined(__cplusplus)
static void fixhighdpi(bool on) { fixhighdpi_b(on); }
static void fixhighdpi(void)    { fixhighdpi_b(true); }
#else
static void gx_fixhighdpi_0(void)    { fixhighdpi_b(true); }
static void gx_fixhighdpi_1(bool on) { fixhighdpi_b(on); }
#define fixhighdpi(...) GX_DISPATCH(gx_fixhighdpi_, __VA_ARGS__)
#endif

/* The factor in force: 1 when off, 1.5 at 150%, 2 at 200%. */
GX_INLINE float gethighdpiscale(void) { return g_gx_dpiFix; }
/* Whether it is switched on at all, which is a different question from
 * what the factor happens to be (1.f at 100% looks like "off"). */
GX_INLINE bool ishighdpi(void) { return g_gx_dpiFixOn; }

static void setcliprgn(HRGN hrgn) {
    DWORD res;
    /* The scissor box is a GL state, so it applies to the whole pending
     * batch: flush first, otherwise queued primitives are clipped by the
     * *new* region. */
    gxFlush();
    if (!hrgn) { g_gx_clipOn = false; gxApplyClip(); return; }
    res = GetRgnBox(hrgn, &g_gx_clipRect);
    if (res == NULLREGION || res == ERROR) { g_gx_clipOn = false; gxApplyClip(); return; }
    if (g_gx_clipRect.right <= g_gx_clipRect.left || g_gx_clipRect.bottom <= g_gx_clipRect.top) {
        g_gx_clipOn = false; gxApplyClip(); return;   /* empty region */
    }
    g_gx_clipOn = true;
    gxApplyClip();
}
static void clearcliprgn(void) {
    gxFlush();
    g_gx_clipOn = false;
    gxApplyClip();
}

/* EasyX: getcliprgn(HRGN hrgn) fills hrgn with the region that is in
 * force, or with the empty region when clipping is off.
 *
 * The stored box is in LOGICAL coordinates, so it is mapped through the
 * current origin and scale before being handed back - setcliprgn() takes
 * logical coordinates too, so getcliprgn() feeding setcliprgn() round
 * trips exactly.  Only the bounding box is available: setcliprgn() never
 * kept more than that. */
static void getcliprgn(HRGN hrgn) {
    long x0, y0, x1, y1;
    if (!hrgn) return;
    if (!g_gx_clipOn) { SetRectRgn(hrgn, 0, 0, 0, 0); return; }
    x0 = gxLogToDevX((float)g_gx_clipRect.left);
    y0 = gxLogToDevY((float)g_gx_clipRect.top);
    x1 = gxLogToDevX((float)g_gx_clipRect.right);
    y1 = gxLogToDevY((float)g_gx_clipRect.bottom);
    if (x1 < x0) { long t = x0; x0 = x1; x1 = t; }
    if (y1 < y0) { long t = y0; y0 = y1; y1 = t; }
    SetRectRgn(hrgn, (int)x0, (int)y0, (int)x1, (int)y1);
}

/*======================================================================
 * Viewport (EasyX setviewport / getviewport)
 *====================================================================*/
/* A viewport is an origin shift plus an optional clip, so it needs its own
 * copy of the clip flag: getcliprgn() has no way to say "there is no
 * region", and getviewport() has to be able to report clip = false. */
static bool      g_gx_vpOn = false;
static int       g_gx_vpL = 0, g_gx_vpT = 0, g_gx_vpR = 0, g_gx_vpB = 0;
static bool      g_gx_vpClip = false;

static void setviewport(int left, int top, int right, int bottom, int clip) {
    long lx, ly;
    gxFlush();
    if (right < left)  { int t = right; right = left;  left = t; }
    if (bottom < top)  { int t = bottom; bottom = top;  top = t; }
    setorigin(left, top);       /* multiplies by the DPI factor itself */
    /* g_gx_clipRect is LOGICAL.  The arguments are in the same 96 dpi units
     * setorigin() takes, so the far corner becomes DEVICE pixels first and
     * gxDevToLog*() maps it back through the (already scaled) origin.
     * At a factor of 1 this is exactly what it was before. */
    lx = gxDevToLogX((int)((float)right  * g_gx_dpiFix + 0.5f));
    ly = gxDevToLogY((int)((float)bottom * g_gx_dpiFix + 0.5f));
    g_gx_vpOn = true;
    g_gx_vpL = left; g_gx_vpT = top; g_gx_vpR = right; g_gx_vpB = bottom;
    g_gx_vpClip = (clip != 0);
    if (g_gx_vpClip) {
        g_gx_clipRect.left = 0;
        g_gx_clipRect.top = 0;
        g_gx_clipRect.right = (long)lx;
        g_gx_clipRect.bottom = (long)ly;
        g_gx_clipOn = true;
    } else {
        g_gx_clipOn = false;
    }
    gxApplyClip();
}
static void getviewport(int* left, int* top, int* right, int* bottom,
                        int* clip) {
    if (left)   *left   = g_gx_vpL;
    if (top)    *top    = g_gx_vpT;
    if (right)  *right  = g_gx_vpR;
    if (bottom) *bottom = g_gx_vpB;
    if (clip)   *clip   = g_gx_vpClip ? 1 : 0;
}

static void setwindowtextA(const char* s) { if (g_gx_hwnd) SetWindowTextA(g_gx_hwnd, s); }
static void setwindowtextW(const WCHAR* s) { if (g_gx_hwnd) SetWindowTextW(g_gx_hwnd, s); }

/*------------------- window / cursor helpers (easygl) ------------------*/
/* Pure Win32 one liners that EasyX programs otherwise have to reach for
 * windows.h themselves.  None of them touch the render state, so no flush
 * is needed. */

/* Hide the mouse cursor inside the graphics window (ShowCursor keeps a
 * counter, so the calls have to be paired - each hide needs one show). */
GX_INLINE void hidecursor(void)  { while (ShowCursor(FALSE) >= 0) ; }
GX_INLINE void showcursor(void)  { while (ShowCursor(TRUE)  <  0) ; }

/* Translucent window: 0 = invisible, 255 = opaque.  Windows only layers a
 * top level window after WS_EX_LAYERED is set, and that has to be done
 * once; SetLayeredWindowAttributes() with LWA_ALPHA then does the rest.
 * Fails silently on a window that cannot be layered. */

/* SetLayeredWindowAttributes() / GetLayeredWindowAttributes() are declared by
 * winuser.h only for _WIN32_WINNT >= 0x0500.  The WINVER block at the top of
 * this file normally covers that, but it cannot when the user's own code
 * includes <windows.h> first with a low WINVER - windows.h has an include
 * guard and will not be read a second time, so the prototypes stay hidden and
 * the calls below would be implicit declarations (an outright error in GCC 14
 * and later).  Both are therefore resolved at run time through
 * GetProcAddress(); they have been exported from user32.dll since Windows
 * 2000, so they are always present. */
typedef BOOL (WINAPI *GX_PFN_SetLayeredWindowAttributes)(HWND, COLORREF, BYTE, DWORD);
typedef BOOL (WINAPI *GX_PFN_GetLayeredWindowAttributes)(HWND, COLORREF*, BYTE*, DWORD*);
static GX_PFN_SetLayeredWindowAttributes gxGetSLWA(void) {
    static GX_PFN_SetLayeredWindowAttributes pfn = NULL;
    static int probed = 0;
    if (!probed) {
        probed = 1;
        pfn = (GX_PFN_SetLayeredWindowAttributes)(void*)
              GetProcAddress(GetModuleHandleA("user32.dll"),
                             "SetLayeredWindowAttributes");
    }
    return pfn;
}
static GX_PFN_GetLayeredWindowAttributes gxGetGLWA(void) {
    static GX_PFN_GetLayeredWindowAttributes pfn = NULL;
    static int probed = 0;
    if (!probed) {
        probed = 1;
        pfn = (GX_PFN_GetLayeredWindowAttributes)(void*)
              GetProcAddress(GetModuleHandleA("user32.dll"),
                             "GetLayeredWindowAttributes");
    }
    return pfn;
}

/* The parameter is a TRANSPARENCY, like every other alpha in this library:
 * 0 = the window is fully opaque, 255 = fully transparent (invisible).
 * Win32's SetLayeredWindowAttributes() wants the opposite (its bAlpha is
 * opacity), so the value is inverted on the way in and out - the same
 * inversion gxAlphaOf() does for colours. */
GX_INLINE void setwindowalpha(BYTE alpha) {
    BYTE opacity = (BYTE)(255 - (int)alpha);
    GX_PFN_SetLayeredWindowAttributes pfn;
    if (!g_gx_hwnd) return;
    SetWindowLongA(g_gx_hwnd, GWL_EXSTYLE,
                   GetWindowLongA(g_gx_hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
    pfn = gxGetSLWA();
    if (pfn) pfn(g_gx_hwnd, 0, opacity, LWA_ALPHA);
}
/* Returns the same transparency that setwindowalpha() takes: 0 = opaque,
 * 255 = invisible.  A window that is not layered at all is opaque, i.e. 0. */
GX_INLINE BYTE getwindowalpha(void) {
    BYTE a = 255;
    DWORD f = 0;
    GX_PFN_GetLayeredWindowAttributes pfn;
    if (!g_gx_hwnd) return ALPHA_OPAQUE;
    pfn = gxGetGLWA();
    if (!pfn) return ALPHA_OPAQUE;
    if (!pfn(g_gx_hwnd, NULL, &a, &f)) return ALPHA_OPAQUE;
    if (!(f & LWA_ALPHA)) return ALPHA_OPAQUE;
    return (BYTE)(255 - (int)a);
}

/* Keep the window in front of every other window (or release it again). */
GX_INLINE void setwindowtopmost(bool on) {
    if (!g_gx_hwnd) return;
    SetWindowPos(g_gx_hwnd, on ? HWND_TOPMOST : HWND_NOTOPMOST,
                 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}
GX_INLINE bool getwindowtopmost(void) {
    if (!g_gx_hwnd) return false;
    return (GetWindowLongA(g_gx_hwnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
}

static void BeginBatchDraw(void) { g_gx_batchDraw = true; }
static void gx_endbatch4(int l, int t, int r, int b) {
    (void)l; (void)t; (void)r; (void)b;
    g_gx_batchDraw = false;
    gxFlush(); gxPump(); gxPresent();
}

typedef BOOL (WINAPI *PFNWGLSWAPINTERVALEXT)(int);
static PFNWGLSWAPINTERVALEXT g_gx_wglSwapIntervalEXT = 0;
/* Two separate values, mirroring setaasamples():
 *   g_gx_reqVsync - what the program asked for.  Survives closegraph(), so
 *                initgraph() can reapply it to the new window.
 *   g_gx_vsyncOn  - what actually took effect, reported by getvsync().
 * This matters because wglSwapIntervalEXT() belongs to a HDC: a new window
 * has a new HDC, which starts at the driver default (usually vsync ON).
 * Without reapplying, setvsync(false) silently reverted on every recreate. */
static bool g_gx_reqVsync = false;
static bool g_gx_vsyncOn = false;

static bool setvsync(bool on) {
    g_gx_reqVsync = on;
    if (!g_gx_glReady || !g_gx_hglrc) return false;   /* applied by initgraph() */
    if (!g_gx_wglSwapIntervalEXT)
        g_gx_wglSwapIntervalEXT =
            (PFNWGLSWAPINTERVALEXT)wglGetProcAddress("wglSwapIntervalEXT");
    if (!g_gx_wglSwapIntervalEXT) { g_gx_vsyncOn = false; return false; }
    g_gx_vsyncOn = (g_gx_wglSwapIntervalEXT(on ? 1 : 0) != FALSE);
    return g_gx_vsyncOn;
}
static bool getvsync(void) { return g_gx_vsyncOn; }

/* Called once the context exists, and again after every recreate: push the
 * requested settings that live on per-window objects back onto them. */
static void gxApplyWindowState(void) {
    /* First, because it can change the client area and everything below
     * sizes off it: gxRestoreDpiFix() resizes the window, and it has to
     * resize to the size the window really has once the frame is the one
     * the program asked for.  A no-op when the style already matches. */
    gxRestoreVarWin();
    setvsync(g_gx_reqVsync);
    gxMsaaCreate();
    /* Same treatment as vsync and MSAA: a rebuilt context must come back
     * with the filter the program asked for.  g_gx_imgFilter survives
     * closegraph(); g_gx_curFilter is the value commands are tagged with, and
     * it has to agree with it again after a rebuild. */
    gxRestoreFilter();
    gxRestoreDpiFix();
}

/*======================================================================
 * 16b. Image mirroring (easygl extensions)
 *====================================================================*/
/* flipimage(dst, src) turns an image upside down, mirrorimage(dst, src)
 * left to right.  EasyX can only rotate, never mirror, so a flipped sprite
 * normally means keeping a second copy of the artwork.
 *
 * Both are a single textured quad into dst's framebuffer with the texture
 * coordinates swapped - no CPU pixel work and no read back.  The v axis
 * runs bottom-up in the texture (that is how gxImageUpload() stores it),
 * so a normal blit uses v0 = 1 (top) .. v1 = 0 (bottom); flipping is
 * therefore swapping v, and mirroring is swapping u. */
static void gxMirrorTo(IMAGE* dst, const IMAGE* src, bool horz) {
    IMAGE* saveWork;
    float sX, sY, oX, oY;
    float w, h, u0, u1, v0, v1;
    if (!dst || !gxImageOk(src)) return;
    if (src->width < 1 || src->height < 1) return;
    if (dst == src) {                 /* in place: mirror through a copy */
        IMAGE tmp;
        memset(&tmp, 0, sizeof(tmp));
        gxImageAlloc(&tmp, src->width, src->height);
        if (!gxImageOk(&tmp)) return;
        gxMirrorTo(&tmp, src, horz);
        gxMirrorTo(dst, &tmp, horz);
        gxImageDestroy(&tmp);
        return;
    }
    gxImageAlloc(dst, src->width, src->height);
    if (!gxImageOk(dst)) return;
    /* A mirror is the same picture, so it covers the same logical area. */
    dst->logW = src->logW;
    dst->logH = src->logH;

    gxFlush();
    saveWork = g_gx_workImg;
    sX = g_gx_scaleX; sY = g_gx_scaleY; oX = g_gx_originX; oY = g_gx_originY;
    /* The quad is emitted in dst's own pixel space, so the caller's
     * setorigin() / setaspectratio() must not be applied on top of it. */
    g_gx_scaleX = 1.f; g_gx_scaleY = 1.f; g_gx_originX = 0.f; g_gx_originY = 0.f;
    g_gx_workImg = dst;
    gxSyncWorkTarget();
    gxUpdateProj();
    gxBindTarget();
    if (g_gx_glReady) glUniformMatrix4fv(g_gx_uProj, 1, GL_FALSE, g_gx_proj);

    /* A freshly allocated texture holds whatever was in the memory, and a
     * source with transparent parts leaves those parts alone because the
     * blit runs with blending on - so the garbage showed through as speckles
     * behind a rotated sprite.  Clearing to alpha 0 first means the uncovered
     * pixels are transparent instead of random. */
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);

    w = (float)src->width; h = (float)src->height;
    u0 = 0.f; u1 = 1.f; v0 = 1.f; v1 = 0.f;      /* a normal blit */
    if (horz) { u0 = 1.f; u1 = 0.f; }
    else      { v0 = 0.f; v1 = 1.f; }
    gxSetRop(R2_COPYPEN);
    gxSetTex(src->tex, 2);
    gxQuadTex(0.f, 0.f, w, h, u0, v0, u1, v1, WHITE);
    gxSetTex(0, 0);
    gxSetRop(g_gx_rop2);
    gxFlush();

    g_gx_scaleX = sX; g_gx_scaleY = sY; g_gx_originX = oX; g_gx_originY = oY;
    g_gx_workImg = saveWork;
    gxSyncWorkTarget();
    gxUpdateProj();
    gxBindTarget();
    if (g_gx_glReady) glUniformMatrix4fv(g_gx_uProj, 1, GL_FALSE, g_gx_proj);
}

GX_INLINE void flipimage(IMAGE* dst, const IMAGE* src) {
    gxMirrorTo(dst, src, false);      /* vertical: upside down */
}
GX_INLINE void mirrorimage(IMAGE* dst, const IMAGE* src) {
    gxMirrorTo(dst, src, true);       /* horizontal: left to right */
}

/*======================================================================
 * 17a. Frame timing (easygl extensions)
 *====================================================================*/
/* EasyX has no frame counter: programs time themselves.  These three are
 * deliberately tiny - a couple of QueryPerformanceCounter() calls per
 * presented frame - and settargetfps() is the useful one, because with
 * vsync off an unthrottled loop burns a whole core doing nothing.
 *
 * getfps() / getframetime() report a smoothed average, not the last frame:
 * an instantaneous value jitters too much to be read off a HUD. */
static LARGE_INTEGER g_gx_fpsFreq, g_gx_fpsLast, g_gx_fpsMark;
static int    g_gx_fpsFrames = 0;
static double g_gx_fpsValue = 0.0, g_gx_fpsMs = 0.0;
static double g_gx_targetFps = 0.0;   /* 0 = unlimited */
static bool   g_gx_fpsInit = false;

static void gxFpsInit(void) {
    if (g_gx_fpsInit) return;
    QueryPerformanceFrequency(&g_gx_fpsFreq);
    QueryPerformanceCounter(&g_gx_fpsLast);
    g_gx_fpsMark = g_gx_fpsLast;
    g_gx_fpsInit = true;
}

/* Called once per presented frame: keep the counters, then - if a target
 * was set - sleep away the rest of the frame budget. */
static void gxFpsTick(void) {
    LARGE_INTEGER now;
    double el;
    if (!g_gx_fpsInit) gxFpsInit();
    if (g_gx_fpsFreq.QuadPart <= 0) return;
    QueryPerformanceCounter(&now);
    g_gx_fpsFrames++;
    /* Average over ~0.25 s so the number is stable but still responsive. */
    el = (double)(now.QuadPart - g_gx_fpsMark.QuadPart) / (double)g_gx_fpsFreq.QuadPart;
    if (el >= 0.25) {
        g_gx_fpsValue = (double)g_gx_fpsFrames / el;
        g_gx_fpsMs    = el * 1000.0 / (double)g_gx_fpsFrames;
        g_gx_fpsMark  = now;
        g_gx_fpsFrames = 0;
    }
    if (g_gx_targetFps > 0.0) {
        double want = 1.0 / g_gx_targetFps;
        double used = (double)(now.QuadPart - g_gx_fpsLast.QuadPart)
                      / (double)g_gx_fpsFreq.QuadPart;
        if (used < want) {
            DWORD ms = (DWORD)((want - used) * 1000.0);
            if (ms > 0) Sleep(ms);
        }
    }
    g_gx_fpsLast = now;
    QueryPerformanceCounter(&g_gx_fpsLast);   /* the sleep is not counted */
}

GX_INLINE double getfps(void)       { return g_gx_fpsValue; }
GX_INLINE double getframetime(void) { return g_gx_fpsMs; }

/* Cap the frame rate in software.  fps <= 0 removes the cap.  With vsync
 * on this is redundant (the display already sets the pace) and harmless:
 * the target is only ever slept towards, never waited past. */
GX_INLINE void settargetfps(double fps) {
    g_gx_targetFps = (fps > 0.0 && fps < 10000.0) ? fps : 0.0;
    gxFpsInit();
}
GX_INLINE double gettargetfps(void) { return g_gx_targetFps; }

/*======================================================================
 * 17b. Helpers used by the EasyX compatible dispatch macros
 *====================================================================*/
static const WCHAR* gxWiden(const char* s) {
    static WCHAR buf[4][1024];
    static int idx = 0;
    int n;
    WCHAR* out;
    if (!s) return L"";
    idx = (idx + 1) & 3;
    out = buf[idx];
#ifdef GX_GUESS_UTF8
    n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, s, -1, out, 1024);
    if (n <= 0)
#endif
        n = MultiByteToWideChar(g_gx_codePage, 0, s, -1, out, 1024);
    if (n <= 0) out[0] = 0;
    return out;
}

static int gx_drawtext_wrapW(double x, double y, const WCHAR* str) {
    RECT r;
    r.left = x; r.top = y; r.right = x; r.bottom = y;
    return gx_drawtext(str, -1, &r, DT_LEFT | DT_TOP, x, y, true);
}
static int gx_drawtext_wrapA(double x, double y, const char* str) {
    return gx_drawtext_wrapW(x, y, gxWiden(str));
}
static int gx_drawtext_rectW(const WCHAR* str, const RECT* pr, UINT fmt) {
    return gx_drawtext(str, -1, pr, fmt, 0, 0, false);
}
static int gx_drawtext_rectA(const char* str, const RECT* pr, UINT fmt) {
    return gx_drawtext_rectW(gxWiden(str), pr, fmt);
}

static ExMessage gxGetMsgFrom(ExMessage* m) {
    ExMessage out;
    memset(&out, 0, sizeof(out));
    if (m) {
        gxWaitMsgEx(m, 0xFF, true);
        out = *m;
    }
    return out;
}
static MOUSEMSG gxGetMsgMouseFrom(MOUSEMSG* m) {
    MOUSEMSG out;
    memset(&out, 0, sizeof(out));
    if (m) {
        gxWaitMsgMouse(m, (BYTE)EM_MOUSE);
        out = *m;
    }
    return out;
}
/* The filtered pointer forms, i.e. getmessage(&msg, EX_KEY): the message
 * is written into the caller's struct and only a message matching the
 * filter wakes the call up.  Without these the two argument form threw the
 * ADDRESS away and waited on (BYTE)&msg instead, so it either returned
 * nothing at all or the wrong kind of message.  0xFF means "any". */
static ExMessage gxGetMsgFromF(ExMessage* m, BYTE filter) {
    ExMessage out;
    memset(&out, 0, sizeof(out));
    if (m) {
        gxWaitMsgEx(m, filter, true);
        out = *m;
    }
    return out;
}
static MOUSEMSG gxGetMsgMouseFromF(MOUSEMSG* m, BYTE filter) {
    MOUSEMSG out;
    memset(&out, 0, sizeof(out));
    if (m) {
        gxWaitMsgMouse(m, (filter == 0xFF) ? (BYTE)EM_MOUSE : filter);
        out = *m;
    }
    return out;
}

/* Full EasyX signature: (out, nMaxCount, prompt, title, pDefault, width,
 * height, bHideCancelBtn).  A NULL string is the same as EasyX's NULL default
 * argument, width / height of 0 mean "size it yourself", and bHideCancelBtn
 * defaults to true (hide Cancel, i.e. a single OK button). */
static bool gx_ibW(WCHAR* out, int n, const WCHAR* prompt, const WCHAR* title,
                   const WCHAR* def, int width, int height, bool bHideCancelBtn) {
    return gxInputBoxExW(prompt ? prompt : L"", title ? title : L"InputBox",
                         def ? def : L"", out, n, width, height, bHideCancelBtn);
}
static bool gx_ibA(char* out, int n, const char* prompt, const char* title,
                   const char* def, int width, int height, bool bHideCancelBtn) {
    WCHAR* buf;
    bool ok;
    if (!out || n < 1) return false;
    out[0] = 0;
    buf = (WCHAR*)malloc(sizeof(WCHAR) * (size_t)(n + 2));
    if (!buf) return false;
    buf[0] = 0;
    ok = gxInputBoxExW(prompt ? gxWiden(prompt) : L"",
                       title  ? gxWiden(title)  : L"InputBox",
                       def    ? gxWiden(def)    : L"",
                       buf, n + 1, width, height, bHideCancelBtn);
    if (ok) {
        /* n is BYTES but the user typed CHARACTERS: in GBK one Han
         * character costs two bytes, so a full edit box does not always
         * fit.  WideCharToMultiByte() then fails instead of returning a
         * half written character, which is why the old code dropped the
         * whole answer.  Give back a character less until it fits, so the
         * tail is cut on a character boundary. */
        int len = WideCharToMultiByte(g_gx_codePage, 0, buf, -1, out, n, NULL, NULL);
        if (len <= 0) {
            int k = 0;
            while (buf[k]) k++;
            len = 0;
            while (k > 0 && len <= 0) {
                k--;
                len = WideCharToMultiByte(g_gx_codePage, 0, buf, k, out, n, NULL, NULL);
            }
            if (len <= 0)      out[0] = 0;
            else if (len >= n) out[n - 1] = 0;
            else               out[len] = 0;
        }
    } else {
        out[0] = 0;
    }
    free(buf);
    return ok;
}

static int gx_msgboxW(const WCHAR* text, const WCHAR* cap, UINT type) {
    /* Owner: the graphics window if there is one, else the console, else
     * nothing.  See gxDialogOwner(). */
    return MessageBoxW(gxDialogOwner(), text ? text : L"", cap ? cap : L"", type);
}
static int gx_msgboxA(const char* text, const char* cap, UINT type) {
    return MessageBoxA(gxDialogOwner(), text ? text : "", cap ? cap : "", type);
}

/*======================================================================
 * 18. EasyX compatible name dispatch
 *
 * EasyX relies on default arguments and overloads, neither of which exist
 * in C11.  Every such API is emulated here: the macro counts its arguments
 * and picks an implementation, and string arguments are routed to the A or
 * the W flavour with _Generic (so LPCTSTR / TCHAR / _T("...") work exactly
 * like in EasyX).
 *====================================================================*/
#define GX_CAT2(a,b) GX_CAT2_(a,b)
#define GX_CAT2_(a,b) a##b



/*======================================================================
 * 18b. EasyX functions this library did not have
 *
 * EasyX sources call a number of APIs that simply did not exist here:
 * the clear* family, polybezier, settextstyle / gettextstyle,
 * GetImageBuffer, setcapture, flushmessage(filter), PeekMouseMsg,
 * FlushBatchDraw(rect), GetEasyXVer, the HSL / HSV colour helpers and the
 * BGI leftovers of graphics.h.  Everything is implemented in this section;
 * the public EasyX names are attached in the dispatch block below.
 *====================================================================*/

/*----------------------------- device --------------------------------*/
static GX_UNUSED void getaspectratio(float* pxasp, float* pyasp) {
    if (pxasp) *pxasp = g_gx_scaleX;
    if (pyasp) *pyasp = g_gx_scaleY;
}

/* EasyX: restore every device setting to its default value. */
static GX_UNUSED void graphdefaults(void) {
    gxFlush();
    gxInitState(false);
}

/*----------------------- clear* (fill with bk colour) ----------------*/
/* EasyX draws the clear* family with the current BACKGROUND colour, solid
 * and without a border, whatever the current brush is. */
static void gxBkBegin(FILLSTYLE* saveFs, COLORREF* saveFc) {
    gxFlush();
    *saveFs = g_gx_fillStyle;
    *saveFc = g_gx_fillColor;
    g_gx_fillStyle.style = BS_SOLID;
    g_gx_fillStyle.hatch = 0;
    g_gx_fillStyle.ppattern = NULL;
    g_gx_fillColor = g_gx_bkColor;
}
static void gxBkEnd(const FILLSTYLE* saveFs, COLORREF saveFc) {
    g_gx_fillStyle = *saveFs;
    g_gx_fillColor = saveFc;
    gxCheckFlush();
}

static GX_UNUSED void clearrectangle(double l, double t, double r, double b) {
    FILLSTYLE fs; COLORREF fc;
    gxBkBegin(&fs, &fc);
    solidrectangle(l, t, r, b);
    gxBkEnd(&fs, fc);
}
static GX_UNUSED void clearcircle(double x, double y, double r) {
    FILLSTYLE fs; COLORREF fc;
    gxBkBegin(&fs, &fc);
    solidcircle(x, y, r);
    gxBkEnd(&fs, fc);
}
static GX_UNUSED void clearellipse(double l, double t, double r, double b) {
    FILLSTYLE fs; COLORREF fc;
    gxBkBegin(&fs, &fc);
    solidellipse(l, t, r, b);
    gxBkEnd(&fs, fc);
}
static GX_UNUSED void clearpie(double l, double t, double r, double b,
                               double st, double en) {
    FILLSTYLE fs; COLORREF fc;
    gxBkBegin(&fs, &fc);
    solidpie(l, t, r, b, st, en);
    gxBkEnd(&fs, fc);
}
static GX_UNUSED void clearroundrect(double l, double t, double r, double b,
                                     double rw, double rh) {
    FILLSTYLE fs; COLORREF fc;
    gxBkBegin(&fs, &fc);
    solidroundrect(l, t, r, b, rw, rh);
    gxBkEnd(&fs, fc);
}
static GX_UNUSED void clearpolygon(const POINT* pts, int n) {
    FILLSTYLE fs; COLORREF fc;
    gxBkBegin(&fs, &fc);
    solidpolygon(pts, n);
    gxBkEnd(&fs, fc);
}

/*----------------------------- polybezier ----------------------------*/
/* Flatten one cubic segment into short straight pieces. */
static void gxBezierSeg(float x0, float y0, float x1, float y1,
                        float x2, float y2, float x3, float y3,
                        COLORREF c, float w) {
    float px = x0, py = y0;
    float chord = fabsf(x1 - x0) + fabsf(y1 - y0)
                + fabsf(x2 - x1) + fabsf(y2 - y1)
                + fabsf(x3 - x2) + fabsf(y3 - y2);
    int steps = (int)(chord * 0.25f) + 6;
    int i;
    if (steps < 6) steps = 6;
    if (steps > 240) steps = 240;
    for (i = 1; i <= steps; i++) {
        float t = (float)i / (float)steps;
        float u = 1.f - t;
        float bx = u * u * u * x0 + 3.f * u * u * t * x1
                 + 3.f * u * t * t * x2 + t * t * t * x3;
        float by = u * u * u * y0 + 3.f * u * u * t * y1
                 + 3.f * u * t * t * y2 + t * t * t * y3;
        gxThickLine(px, py, bx, by, c, w, false);
        px = bx; py = by;
    }
}

/* EasyX: polybezier(const POINT* points, int num), GDI control point
 * layout - points 0, 3, 6, ... are anchors, the two between them are the
 * control points of that cubic. */
static GX_UNUSED void polybezier(const POINT* pts, int n) {
    float w = (float)g_gx_lineWidth;
    int i;
    if (!pts || n < 4) return;
    if (g_gx_lineStyle.style == PS_NULL) return;
    gxDashReset();
    gxSetTex(0, 0);
    for (i = 0; i + 3 < n; i += 3)
        gxBezierSeg((float)pts[i].x,     (float)pts[i].y,
                    (float)pts[i + 1].x, (float)pts[i + 1].y,
                    (float)pts[i + 2].x, (float)pts[i + 2].y,
                    (float)pts[i + 3].x, (float)pts[i + 3].y,
                    g_gx_lineColor, w);
    gxCheckFlush();
}

/*--------------------------- text style ------------------------------*/
static void gxSetTextStyleW(int h, int wdth, const WCHAR* face,
                            int esc, int orient, int weight,
                            int italic, int underline, int strike,
                            BYTE cs, BYTE op, BYTE cp, BYTE q, BYTE pf) {
    LOGFONTA lf;
    memset(&lf, 0, sizeof(lf));
    lf.lfHeight         = h;
    lf.lfWidth          = wdth;
    lf.lfEscapement     = esc;
    lf.lfOrientation    = orient;
    lf.lfWeight         = weight;
    lf.lfItalic         = (BYTE)(italic    ? 1 : 0);
    lf.lfUnderline      = (BYTE)(underline ? 1 : 0);
    lf.lfStrikeOut      = (BYTE)(strike    ? 1 : 0);
    lf.lfCharSet        = cs;
    lf.lfOutPrecision   = op;
    lf.lfClipPrecision  = cp;
    lf.lfQuality        = q;
    lf.lfPitchAndFamily = pf;
    if (face && face[0]) {
        char* ansi = gxDupBytesFromWide(face);
        if (ansi) {
            strncpy(lf.lfFaceName, ansi, LF_FACESIZE - 1);
            free(ansi);
        }
    }
    if (lf.lfFaceName[0] == 0) strcpy(lf.lfFaceName, "System");
    g_gx_font = lf;
}
static void gxSetTextStyleA(int h, int wdth, const char* face,
                            int esc, int orient, int weight,
                            int italic, int underline, int strike,
                            BYTE cs, BYTE op, BYTE cp, BYTE q, BYTE pf) {
    WCHAR* wf = face ? gxDupWideFromBytes(face) : NULL;
    gxSetTextStyleW(h, wdth, wf, esc, orient, weight, italic, underline,
                    strike, cs, op, cp, q, pf);
    free(wf);
}
/* settextstyle(nHeight, nWidth, lpszFace) */
static void gxSetTextStyle3W(int h, int wdth, const WCHAR* face) {
    gxSetTextStyleW(h, wdth, face, 0, 0, FW_NORMAL, 0, 0, 0,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    DEFAULT_QUALITY, (BYTE)(DEFAULT_PITCH | FF_DONTCARE));
}
static void gxSetTextStyle3A(int h, int wdth, const char* face) {
    WCHAR* wf = face ? gxDupWideFromBytes(face) : NULL;
    gxSetTextStyle3W(h, wdth, wf);
    free(wf);
}
/* settextstyle(h, w, face, esc, orient, weight, italic, underline, strike) */
static void gxSetTextStyle9W(int h, int wdth, const WCHAR* face,
                             int esc, int orient, int weight,
                             int italic, int underline, int strike) {
    gxSetTextStyleW(h, wdth, face, esc, orient, weight, italic, underline,
                    strike, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                    CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                    (BYTE)(DEFAULT_PITCH | FF_DONTCARE));
}
static void gxSetTextStyle9A(int h, int wdth, const char* face,
                             int esc, int orient, int weight,
                             int italic, int underline, int strike) {
    WCHAR* wf = face ? gxDupWideFromBytes(face) : NULL;
    gxSetTextStyle9W(h, wdth, wf, esc, orient, weight, italic, underline,
                     strike);
    free(wf);
}
static GX_UNUSED void gxSetTextStylePtr(const LOGFONTA* p) { setfont(p); }
static GX_UNUSED void gettextstyle(LOGFONTA* f) { getfont(f); }

/* single character overloads: outtextxy(x, y, 'A') */
static GX_UNUSED void gx_outtextxy_ch(double x, double y, int c) {
    char b[2];
    b[0] = (char)c; b[1] = 0;
    outtextxyA(x, y, b);
}
static GX_UNUSED int gx_textwidth_ch(int c) {
    char b[2];
    b[0] = (char)c; b[1] = 0;
    return textwidthA(b);
}
static GX_UNUSED int gx_textheight_ch(int c) {
    char b[2];
    b[0] = (char)c; b[1] = 0;
    return textheightA(b);
}
static GX_UNUSED void gx_outtext_ch(int c) {
    char b[2];
    b[0] = (char)c; b[1] = 0;
    outtextA(b);
}

/*-------------------------- image buffer -----------------------------*/
/* Scratch buffers for the pixel round trip.  Allocating one per frame
 * churns megabytes through the heap, so they are cached and grown on
 * demand; two slots because gxGetImageBuffer() reads into one while
 * gxSyncImgBufs() may still be holding the other. */
static unsigned char* gxScratch(int slot, size_t bytes) {
    static unsigned char* s_buf[2] = { 0, 0 };
    static size_t s_cap[2] = { 0, 0 };
    if (slot < 0) {                       /* release both */
        free(s_buf[0]); s_buf[0] = 0; s_cap[0] = 0;
        free(s_buf[1]); s_buf[1] = 0; s_cap[1] = 0;
        return 0;
    }
    if (bytes > s_cap[slot]) {
        unsigned char* p = (unsigned char*)realloc(s_buf[slot], bytes);
        if (!p) return 0;
        s_buf[slot] = p;
        s_cap[slot] = bytes;
    }
    return s_buf[slot];
}

/* Texture filtering for IMAGE draws.  MSAA only smooths the edges of
 * geometry - it does nothing for what is INSIDE a texture, which is where
 * most of the visible aliasing in a 2D game comes from (scaled sprites,
 * rotated images).  EasyX / GDI always stretch bitmaps with a filter; this
 * library made every IMAGE NEAREST so that 1:1 blits stay crisp, which is
 * right for tile maps but rough for scaled art.  setimagefilter(true)
 * switches to LINEAR: smooth magnified and minified images, at the cost of
 * slightly soft 1:1 blits. */

/*------------------------ filter mode ------------------------*/
/* Be used in setimagefilter() */
#define GLL_NEAREST    0
#define GLL_LINEAR     1

/* setimagefilter() / getimagefilter() are defined below these two, so they
 * need declaring before they can be called. */
static void setimagefilter(bool smooth);
static bool getimagefilter(void);

/* setimagefilter() / getimagefilter() take and return a bool, which is
 * awkward next to the GLL_* constants above.  These two speak in the
 * constants instead, so filtering reads like every other mode in this
 * library: setfontmode(GLF_INT), setblendmode(GX_BLEND_ADD),
 * setfiltermode(GLL_LINEAR). */
/* Re-assert the filter after a context rebuild (initgraph() goes through
 * gxApplyWindowState()).  g_gx_imgFilter survives closegraph() because it is a
 * plain static, so this only has to bring the per-command value back in line
 * with it. */
static void gxRestoreFilter(void) {
    g_gx_curFilter = g_gx_imgFilter;
}

GX_INLINE void setfiltermode(int mode) {
    setimagefilter(mode == GLL_LINEAR);
}
GX_INLINE int getfiltermode(void) {
    return getimagefilter() ? GLL_LINEAR : GLL_NEAREST;
}

GX_INLINE void setimagefilter(bool smooth) {
    int n = smooth ? 1 : 0;
    if (n == g_gx_curFilter) return;
    /* Close the open batch BEFORE switching, so the primitives queued so
     * far keep the filter they were drawn with.  Without this, two
     * putimage() calls on the same texture merge into one command and both
     * end up with whichever filter was set last - which is why "NEAREST on
     * the left, LINEAR on the right" used to look identical. */
    gxEndCmd();
    g_gx_curFilter = n;
    g_gx_imgFilter = n;
}
GX_INLINE bool getimagefilter(void) { return g_gx_imgFilter != 0; }

/* GX_IMGBUF_DISCARD tells GetImageBuffer() to skip the GPU -> CPU read
 * back, for callers that overwrite every pixel anyway (software
 * renderers, per pixel effects, ...).  It turns a read back + flip +
 * upload into a single upload. */
#define GX_IMGBUF_READ     0   /* default: read the target back (EasyX)   */
#define GX_IMGBUF_DISCARD  1   /* caller overwrites everything: no read   */
static bool g_gx_imgBufDiscard = false;

GX_INLINE void setimagebuffermode(int mode) {
    g_gx_imgBufDiscard = (mode == GX_IMGBUF_DISCARD);
}
GX_INLINE int getimagebuffermode(void) {
    return g_gx_imgBufDiscard ? GX_IMGBUF_DISCARD : GX_IMGBUF_READ;
}

/* EasyX: DWORD* GetImageBuffer(IMAGE* pImg = NULL) - a CPU side copy of
 * the pixels, laid out top-down with one 0x00BBGGRR DWORD per pixel.
 *
 * This library renders through OpenGL, so there is no permanent CPU copy:
 * the buffer is materialised on the first call and written back to the
 * texture whenever the batch is flushed (gxFlush), i.e. at the next draw
 * or FlushBatchDraw().  Edits therefore become visible one frame later at
 * the latest, which matches what a double buffered EasyX program sees. */
typedef struct GxImgBuf { IMAGE* img; DWORD* data; int w, h; int dirty; } GxImgBuf;
GX_DEFINE_ARRAY(GxImgBufVec, GxImgBuf)
static GxImgBufVec g_gx_imgBufs;

static GxImgBuf* gxImgBufFind(IMAGE* img) {
    size_t i;
    for (i = 0; i < g_gx_imgBufs.size; i++)
        if (g_gx_imgBufs.data[i].img == img) return &g_gx_imgBufs.data[i];
    return NULL;
}
static void gxImgBufDrop(IMAGE* img) {
    size_t i;
    for (i = 0; i < g_gx_imgBufs.size; i++) {
        if (g_gx_imgBufs.data[i].img != img) continue;
        free(g_gx_imgBufs.data[i].data);
        g_gx_imgBufs.data[i] = g_gx_imgBufs.data[g_gx_imgBufs.size - 1];
        g_gx_imgBufs.size--;
        return;
    }
}
static void gxImgBufDropAll(void) {
    size_t i;
    for (i = 0; i < g_gx_imgBufs.size; i++) free(g_gx_imgBufs.data[i].data);
    GxImgBufVec_clear(&g_gx_imgBufs);
}
static void gxSyncImgBufs(void) {
    size_t i;
    unsigned char* flip;
    GLuint tex;
    if (!g_gx_glReady) return;
    for (i = 0; i < g_gx_imgBufs.size; i++) {
        GxImgBuf* b = &g_gx_imgBufs.data[i];
        int row;
        if (!b->dirty || !b->data || b->w < 1 || b->h < 1) continue;
        if (b->img) {
            if (!gxImageOk(b->img) || b->img->width != b->w
                || b->img->height != b->h) { b->dirty = 0; continue; }
            tex = b->img->tex;
        } else {
            if (g_gx_target->w != b->w || g_gx_target->h != b->h) { b->dirty = 0; continue; }
            tex = g_gx_target->tex;
        }
        if (!tex) { b->dirty = 0; continue; }
        /* alpha is 0 in the client buffer but the texture needs 255; the
         * flip and the alpha store are fused for the same cache reason as
         * in gxGetImageBuffer(), and the scratch buffer is reused. */
        flip = gxScratch(1, (size_t)b->w * (size_t)b->h * 4);
        if (!flip) { b->dirty = 0; continue; }
        for (row = 0; row < b->h; row++) {
            unsigned char* dst = &flip[(size_t)(b->h - 1 - row) * (size_t)b->w * 4];
            unsigned char* src = ((unsigned char*)b->data)
                               + (size_t)row * (size_t)b->w * 4;
            size_t k;
            memcpy(dst, src, (size_t)b->w * 4);
            for (k = 3; k < (size_t)b->w * 4; k += 4) dst[k] = 255;
        }
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, b->w, b->h,
                        GL_RGBA, GL_UNSIGNED_BYTE, flip);
        b->dirty = 0;
    }
}
static DWORD* gxGetImageBuffer(IMAGE* img) {
    GxImgBuf* b;
    unsigned char* tmp;
    unsigned char* dst;
    GLuint fbo;
    int w, h, row;
    if (!g_gx_glReady) return NULL;
    gxSyncImgBufs();     /* land edits from a previous call first */
    gxFlush();
    if (img && gxImageOk(img)) {
        fbo = img->fbo; w = img->width; h = img->height;
    } else {
        img = NULL;
        fbo = g_gx_target->fbo; w = g_gx_target->w; h = g_gx_target->h;
    }
    if (!fbo || w < 1 || h < 1) return NULL;
    b = gxImgBufFind(img);
    if (!b) {
        GxImgBuf nb;
        memset(&nb, 0, sizeof(nb));
        nb.img = img;
        GxImgBufVec_pushv(&g_gx_imgBufs, nb);
        b = &g_gx_imgBufs.data[g_gx_imgBufs.size - 1];
    }
    if (!b->data || b->w != w || b->h != h) {
        free(b->data);
        b->data = (DWORD*)malloc((size_t)w * (size_t)h * 4);
        b->w = w; b->h = h;
        if (!b->data) return NULL;
    }
    if (!g_gx_imgBufDiscard) {
        tmp = gxScratch(0, (size_t)w * (size_t)h * 4);
        if (!tmp) return NULL;
        glBindFramebuffer(GL_FRAMEBUFFER, gxReadFbo(fbo));
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, tmp);
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
        glBindFramebuffer(GL_FRAMEBUFFER, g_gx_target->fbo);
        /* glReadPixels is bottom-up, the client buffer is top-down, and an
         * EasyX buffer carries alpha 0.  Clearing alpha is fused into the
         * flip so each row is still in cache - a second full pass over the
         * image measured about twice as long. */
        dst = (unsigned char*)b->data;
        for (row = 0; row < h; row++) {
            unsigned char* d = &dst[(size_t)row * (size_t)w * 4];
            const unsigned char* sr = &tmp[(size_t)(h - 1 - row) * (size_t)w * 4];
            size_t k;
            memcpy(d, sr, (size_t)w * 4);
            for (k = 3; k < (size_t)w * 4; k += 4) d[k] = 0;
        }
    }
    b->dirty = 1;                 /* even discarded: it has to be uploaded */
    return b->data;
}
/* EasyX: HDC GetImageHDC(IMAGE* pImg = NULL).  A GL texture has no GDI
 * device context, so this always reports "unavailable" - GDI helpers
 * (TextOut, BitBlt, ...) are not supported on top of this renderer. */
static HDC gxGetImageHDC(IMAGE* img) { (void)img; return NULL; }

/*---------------------------- messages --------------------------------*/
static void gx_flushmsg0(void) { gx_flushmsg_all(); }
static void gx_flushmsg1(BYTE filter) {
    ExMessage keep[GX_MSGQ_CAP];
    int i, nk = 0;
    gxPump();
    if (filter == 0xFF) { gxMsgInit(); return; }
    for (i = 0; i < g_gx_msgCount; i++)
        if (!gxMsgIsType(gxMsgAt(i)->message, filter))
            keep[nk++] = *gxMsgAt(i);
    gxMsgInit();
    for (i = 0; i < nk; i++) gxMsgPush(&keep[i]);
}
static GX_UNUSED void setcapture(void) { if (g_gx_hwnd) SetCapture(g_gx_hwnd); }
static GX_UNUSED void releasecapture(void) { ReleaseCapture(); }
static GX_UNUSED bool gx_peekmousemsg1(MOUSEMSG* m) {
    return gxPeekMouse(m, (BYTE)EM_MOUSE, true);
}
static GX_UNUSED bool gx_peekmousemsg2(MOUSEMSG* m, bool rmv) {
    return gxPeekMouse(m, (BYTE)EM_MOUSE, rmv);
}

/*----------------------------- batch ----------------------------------*/
static void gx_flushbatch0(void) { gxPump(); gxPresent(); }
static void gx_flushbatch4(int l, int t, int r, int b) {
    (void)l; (void)t; (void)r; (void)b;
    gxPump(); gxPresent();
}

/*--------------------------- version string ---------------------------*/
static GX_UNUSED const char* GetEasyGLVer(void) { return EASYGL_VERSION; }
#ifndef GetEasyXVer
#define GetEasyXVer GetEasyGLVer
#endif

/*--------------------- OpenGL / adapter information --------------------*/
/* glGetString() hands back a pointer into driver owned memory: it must not
 * be freed, it stays valid until the context is destroyed, and each of the
 * selectors has its own buffer so asking for one does not clobber another.
 * Before initgraph() there is no context at all, hence the "n/a". */
static const char* gxGLStr(int name) {
    const unsigned char* s;
    if (!g_gx_glReady || !gxGetString) return "n/a";
    s = gxGetString((GLenum)name);
    return s ? (const char*)s : "n/a";
}
GX_INLINE const char* getvendorinfo(void)  { return gxGLStr(GL_VENDOR); }
GX_INLINE const char* getrendererinfo(void){ return gxGLStr(GL_RENDERER); }
GX_INLINE const char* getglver(void)       { return gxGLStr(GL_VERSION); }
GX_INLINE const char* getglslver(void)     { return gxGLStr(GL_SHADING_LANGUAGE_VERSION); }

/* The adapter name and the current display mode come from Win32, not from
 * GL, so they are available even without a context.  Both are looked up
 * once and then cached. */
static char g_gx_adapterName[160];
static char g_gx_displayMode[160];
static bool g_gx_adapterDone = false;

static void gxAdapterOnce(void) {
    DISPLAY_DEVICEA dd;
    DEVMODEA dm;
    if (g_gx_adapterDone) return;
    g_gx_adapterDone = true;
    strcpy(g_gx_adapterName, "n/a");
    strcpy(g_gx_displayMode, "n/a");
    memset(&dd, 0, sizeof(dd));
    dd.cb = (DWORD)sizeof(dd);
    if (EnumDisplayDevicesA(0, 0, &dd, 0))
        strncpy(g_gx_adapterName, dd.DeviceString, sizeof(g_gx_adapterName) - 1);
    memset(&dm, 0, sizeof(dm));
    dm.dmSize = (WORD)sizeof(dm);
    if (EnumDisplaySettingsA(0, ENUM_CURRENT_SETTINGS, &dm))
        sprintf(g_gx_displayMode, "%lux%lu %luHz %lubpp",
                (unsigned long)dm.dmPelsWidth, (unsigned long)dm.dmPelsHeight,
                (unsigned long)dm.dmDisplayFrequency,
                (unsigned long)dm.dmBitsPerPel);
}
GX_INLINE const char* getadapterinfo(void) { gxAdapterOnce(); return g_gx_adapterName; }
GX_INLINE const char* getdisplaymode(void) { gxAdapterOnce(); return g_gx_displayMode; }

/*------------------- graphics.h (BGI) leftovers -----------------------*/
static GX_UNUSED void bar(double l, double t, double r, double b) {
    solidrectangle(l, t, r, b);
}
static GX_UNUSED void bar3d(double l, double t, double r, double b, int depth, bool topflag) {
    POINT p[4];
    COLORREF save = g_gx_fillColor;
    bar(l, t, r, b);
    if (depth == 0) return;
    g_gx_fillColor = save;
    p[0].x = (LONG)r;            p[0].y = (LONG)t;
    p[1].x = (LONG)(r + depth);  p[1].y = (LONG)(t - depth);
    p[2].x = (LONG)(r + depth);  p[2].y = (LONG)(b - depth);
    p[3].x = (LONG)r;            p[3].y = (LONG)b;
    solidpolygon(p, 4);
    polygon(p, 4);
    if (topflag) {
        p[0].x = (LONG)l;            p[0].y = (LONG)t;
        p[1].x = (LONG)(l + depth);  p[1].y = (LONG)(t - depth);
        p[2].x = (LONG)(r + depth);  p[2].y = (LONG)(t - depth);
        p[3].x = (LONG)r;            p[3].y = (LONG)t;
        solidpolygon(p, 4);
        polygon(p, 4);
    }
}
/* drawpoly / fillpoly take a flat int array: x0, y0, x1, y1, ... */
static POINT* gxPolyFromInts(const int* src, int n) {
    POINT* p;
    int i;
    if (!src || n < 2) return NULL;
    p = (POINT*)malloc(sizeof(POINT) * (size_t)n);
    if (!p) return NULL;
    for (i = 0; i < n; i++) {
        p[i].x = (LONG)src[i * 2];
        p[i].y = (LONG)src[i * 2 + 1];
    }
    return p;
}
static GX_UNUSED void drawpoly(int numpoints, const int* polypoints) {
    POINT* p = gxPolyFromInts(polypoints, numpoints);
    if (!p) return;
    polygon(p, numpoints);
    free(p);
}
static GX_UNUSED void fillpoly(int numpoints, const int* polypoints) {
    POINT* p = gxPolyFromInts(polypoints, numpoints);
    if (!p) return;
    solidpolygon(p, numpoints);
    free(p);
}
static GX_UNUSED COLORREF getcolor(void) { return getlinecolor(); }
static GX_UNUSED void setcolor(COLORREF color) { setlinecolor(color); }
static GX_UNUSED int getmaxx(void) { return getwidth() - 1; }
static GX_UNUSED int getmaxy(void) { return getheight() - 1; }
static GX_UNUSED void setwritemode(int mode) {
    if (mode == XOR_PUT) { setrop2(R2_XORPEN); return; }
    if (mode >= R2_BLACK && mode <= R2_WHITE) { setrop2(mode); return; }
    setrop2(R2_COPYPEN);        /* COPY_PUT and anything unknown */
}

/*-------------------------- colour models -----------------------------*/
static float gxHue2RGB(float p, float q, float h) {
    while (h < 0.f)    h += 360.f;
    while (h >= 360.f) h -= 360.f;
    if (h < 60.f)  return p + (q - p) * h / 60.f;
    if (h < 180.f) return q;
    if (h < 240.f) return p + (q - p) * (240.f - h) / 60.f;
    return p;
}
static GX_UNUSED COLORREF HSLtoRGB(float H, float S, float L) {
    float r, g, b, p, q;
    if (S <= 0.f) { r = g = b = L; }
    else {
        q = (L < 0.5f) ? (L * (1.f + S)) : (L + S - L * S);
        p = 2.f * L - q;
        r = gxHue2RGB(p, q, H + 120.f);
        g = gxHue2RGB(p, q, H);
        b = gxHue2RGB(p, q, H - 120.f);
    }
    if (r < 0.f) r = 0.f; if (r > 1.f) r = 1.f;
    if (g < 0.f) g = 0.f; if (g > 1.f) g = 1.f;
    if (b < 0.f) b = 0.f; if (b > 1.f) b = 1.f;
    return RGB((BYTE)(r * 255.f + 0.5f), (BYTE)(g * 255.f + 0.5f),
               (BYTE)(b * 255.f + 0.5f));
}
static GX_UNUSED COLORREF HSVtoRGB(float H, float S, float V) {
    float r, g, b, p, q, t, f;
    int i;
    if (S <= 0.f) { r = g = b = V; }
    else {
        while (H < 0.f)    H += 360.f;
        while (H >= 360.f) H -= 360.f;
        H /= 60.f;
        i = (int)H;
        f = H - (float)i;
        p = V * (1.f - S);
        q = V * (1.f - S * f);
        t = V * (1.f - S * (1.f - f));
        switch (i) {
        case 0: r = V; g = t; b = p; break;
        case 1: r = q; g = V; b = p; break;
        case 2: r = p; g = V; b = t; break;
        case 3: r = p; g = q; b = V; break;
        case 4: r = t; g = p; b = V; break;
        default: r = V; g = p; b = q; break;
        }
    }
    if (r < 0.f) r = 0.f; if (r > 1.f) r = 1.f;
    if (g < 0.f) g = 0.f; if (g > 1.f) g = 1.f;
    if (b < 0.f) b = 0.f; if (b > 1.f) b = 1.f;
    return RGB((BYTE)(r * 255.f + 0.5f), (BYTE)(g * 255.f + 0.5f),
               (BYTE)(b * 255.f + 0.5f));
}
static GX_UNUSED COLORREF RGBtoGRAY(COLORREF rgb) {
    int g = (299 * (int)GetRValue(rgb) + 587 * (int)GetGValue(rgb)
             + 114 * (int)GetBValue(rgb)) / 1000;
    if (g < 0) g = 0;
    if (g > 255) g = 255;
    return RGB((BYTE)g, (BYTE)g, (BYTE)g);
}
static GX_UNUSED void RGBtoHSL(COLORREF rgb, float* H, float* S, float* L) {
    float r = (float)GetRValue(rgb) / 255.f;
    float g = (float)GetGValue(rgb) / 255.f;
    float b = (float)GetBValue(rgb) / 255.f;
    float mx = (r > g) ? ((r > b) ? r : b) : ((g > b) ? g : b);
    float mn = (r < g) ? ((r < b) ? r : b) : ((g < b) ? g : b);
    float h = 0.f, s = 0.f, l = (mx + mn) * 0.5f;
    float d = mx - mn;
    if (d > 0.f) {
        s = (l > 0.5f) ? (d / (2.f - mx - mn)) : (d / (mx + mn));
        if (mx == r)      h = (g - b) / d + ((g < b) ? 6.f : 0.f);
        else if (mx == g) h = (b - r) / d + 2.f;
        else              h = (r - g) / d + 4.f;
        h *= 60.f;
    }
    if (H) *H = h;
    if (S) *S = s;
    if (L) *L = l;
}
static GX_UNUSED void RGBtoHSV(COLORREF rgb, float* H, float* S, float* V) {
    float r = (float)GetRValue(rgb) / 255.f;
    float g = (float)GetGValue(rgb) / 255.f;
    float b = (float)GetBValue(rgb) / 255.f;
    float mx = (r > g) ? ((r > b) ? r : b) : ((g > b) ? g : b);
    float mn = (r < g) ? ((r < b) ? r : b) : ((g < b) ? g : b);
    float h = 0.f, s = 0.f, d = mx - mn;
    if (d > 0.f) {
        s = (mx > 0.f) ? (d / mx) : 0.f;
        if (mx == r)      h = (g - b) / d + ((g < b) ? 6.f : 0.f);
        else if (mx == g) h = (b - r) / d + 2.f;
        else              h = (r - g) / d + 4.f;
        h *= 60.f;
    }
    if (H) *H = h;
    if (S) *S = s;
    if (V) *V = mx;
}

/* HSL and HSV share the same hue; only saturation and the lightness /
 * value axis differ, so these two convert without going through RGB.
 *
 *   HSL -> HSV :  V = L + S * min(L, 1 - L)
 *                 S' = (V == 0) ? 0 : 2 * (1 - L / V)
 *   HSV -> HSL :  L = V * (1 - S / 2)
 *                 S' = (L == 0 || L == 1) ? 0 : (V - L) / min(L, 1 - L)
 *
 * Both guard the degenerate cases so an achromatic colour (S = 0) or a
 * pure black / white endpoint can never divide by zero. */
static GX_UNUSED void HSLtoHSV(float H, float S, float L,
                               float* pS, float* pV) {
    float v, s;
    (void)H;                /* hue is the same in both models */
    if (S < 0.f) S = 0.f; if (S > 1.f) S = 1.f;
    if (L < 0.f) L = 0.f; if (L > 1.f) L = 1.f;
    v = L + S * ((L < 1.f - L) ? L : (1.f - L));
    s = (v <= 0.f) ? 0.f : (2.f * (1.f - L / v));
    if (s < 0.f) s = 0.f; if (s > 1.f) s = 1.f;
    if (pS) *pS = s;
    if (pV) *pV = v;
}
static GX_UNUSED void HSVtoHSL(float H, float S, float V,
                               float* pS, float* pL) {
    float l, s, m;
    if (S < 0.f) S = 0.f; if (S > 1.f) S = 1.f;
    if (V < 0.f) V = 0.f; if (V > 1.f) V = 1.f;
    l = V * (1.f - S * 0.5f);
    if (l <= 0.f || l >= 1.f) {
        s = 0.f;
    } else {
        m = (l < 1.f - l) ? l : (1.f - l);
        s = (m <= 0.f) ? 0.f : ((V - l) / m);
    }
    if (s < 0.f) s = 0.f; if (s > 1.f) s = 1.f;
    if (pS) *pS = s;
    if (pL) *pL = l;
    (void)H;                    /* hue is unchanged by the conversion */
}

#ifdef __cplusplus

/*======================================================================
 * C++ mode.
 *
 * g++ does not know _Generic (that is a C11 feature), so the whole
 * dispatch layer is replaced by plain C++ overloads.  C++ also has real
 * default arguments and overloading, which is exactly what EasyX itself
 * uses, so the public API ends up identical to EasyX here.
 * Every overload just forwards to the same A / W implementation that the
 * C macros pick with _Generic, so both compilers behave the same way.
 *====================================================================*/

static inline int textwidth(const char* s)    { return textwidthA(s); }
static inline int textwidth(const WCHAR* s)   { return textwidthW(s); }
static inline int textheight(const char* s)   { return textheightA(s); }
static inline int textheight(const WCHAR* s)  { return textheightW(s); }

static inline void outtext(const char* s)   { outtextA(s); }
static inline void outtext(const WCHAR* s)  { outtextW(s); }

static inline void outtextxy(double x, double y, const char* s) { outtextxyA(x, y, s); }
static inline void outtextxy(double x, double y, const WCHAR* s){ outtextxyW(x, y, s); }
static inline void outtextxy(double x, double y, char c)   { gx_outtextxy_ch(x, y, c); }
static inline void outtextxy(double x, double y, WCHAR c) { gx_outtextxy_ch(x, y, (int)c); }
static inline int  textwidth(char c)  { return gx_textwidth_ch((int)c); }
static inline int  textwidth(WCHAR c) { return gx_textwidth_ch((int)c); }
static inline int  textheight(char c)  { return gx_textheight_ch((int)c); }
static inline int  textheight(WCHAR c) { return gx_textheight_ch((int)c); }
static inline void outtext(char c)  { gx_outtext_ch((int)c); }
static inline void outtext(WCHAR c) { gx_outtext_ch((int)c); }

static inline int drawtext(const char* s, const RECT* pr, UINT fmt) {
    return gx_drawtext_rectA(s, pr, fmt);
}
static inline int drawtext(const WCHAR* s, const RECT* pr, UINT fmt) {
    return gx_drawtext_rectW(s, pr, fmt);
}
static inline int drawtext(double x, double y, const char* s){ return gx_drawtext_wrapA(x, y, s); }
static inline int drawtext(double x, double y, const WCHAR* s){ return gx_drawtext_wrapW(x, y, s); }

/* settextstyle: the four EasyX overloads */
static inline void settextstyle(int h, int w, const char* face) {
    gxSetTextStyle3A(h, w, face);
}
static inline void settextstyle(int h, int w, const WCHAR* face) {
    gxSetTextStyle3W(h, w, face);
}
static inline void settextstyle(int h, int w, const char* face, int esc,
                                int orient, int weight, bool italic,
                                bool underline, bool strike) {
    gxSetTextStyle9A(h, w, face, esc, orient, weight, italic ? 1 : 0,
                     underline ? 1 : 0, strike ? 1 : 0);
}
static inline void settextstyle(int h, int w, const WCHAR* face, int esc,
                                int orient, int weight, bool italic,
                                bool underline, bool strike) {
    gxSetTextStyle9W(h, w, face, esc, orient, weight, italic ? 1 : 0,
                     underline ? 1 : 0, strike ? 1 : 0);
}
static inline void settextstyle(int h, int w, const char* face, int esc,
                                int orient, int weight, bool italic,
                                bool underline, bool strike, BYTE cs,
                                BYTE op, BYTE cp, BYTE q, BYTE pf) {
    gxSetTextStyleA(h, w, face, esc, orient, weight, italic ? 1 : 0,
                    underline ? 1 : 0, strike ? 1 : 0, cs, op, cp, q, pf);
}
static inline void settextstyle(int h, int w, const WCHAR* face, int esc,
                                int orient, int weight, bool italic,
                                bool underline, bool strike, BYTE cs,
                                BYTE op, BYTE cp, BYTE q, BYTE pf) {
    gxSetTextStyleW(h, w, face, esc, orient, weight, italic ? 1 : 0,
                    underline ? 1 : 0, strike ? 1 : 0, cs, op, cp, q, pf);
}
static inline void settextstyle(const LOGFONTA* f) { gxSetTextStylePtr(f); }

static inline DWORD* GetImageBuffer(IMAGE* pImg = NULL) {
    return gxGetImageBuffer(pImg);
}
static inline HDC GetImageHDC(IMAGE* pImg = NULL) { return gxGetImageHDC(pImg); }
static inline void flushmessage(BYTE filter = 0xFF) { gx_flushmsg1(filter); }
/* EasyX declares setviewport / getviewport with clip defaulting to 1, so a
 * call that leaves it out has to compile too. */
static inline void setviewport(int l, int t, int r, int b) {
    setviewport(l, t, r, b, 1);
}
static inline void getviewport(int* l, int* t, int* r, int* b) {
    getviewport(l, t, r, b, (int*)0);
}
static inline bool PeekMouseMsg(MOUSEMSG* m, bool bRemoveMsg = true) {
    return gxPeekMouse(m, (BYTE)EM_MOUSE, bRemoveMsg);
}
static inline void FlushBatchDraw() { gx_flushbatch0(); }
static inline void FlushBatchDraw(int l, int t, int r, int b) {
    gx_flushbatch4(l, t, r, b);
}

static inline void setwindowtext(const char* s)  { setwindowtextA(s); }
static inline void setwindowtext(const WCHAR* s) { setwindowtextW(s); }


/*------------------------------ images --------------------------------*/
static inline bool loadimage(IMAGE* img, const char* f) {
    return gx_loadimg2(img, gxWiden(f));
}
static inline bool loadimage(IMAGE* img, const WCHAR* f) {
    return gx_loadimg2(img, f);
}
static inline bool loadimage(IMAGE* img, const char* f, int w) {
    return gx_loadimg3(img, gxWiden(f), w);
}
static inline bool loadimage(IMAGE* img, const WCHAR* f, int w) {
    return gx_loadimg3(img, f, w);
}
static inline bool loadimage(IMAGE* img, const char* f, int w, int h) {
    return gx_loadimg4(img, gxWiden(f), w, h);
}
static inline bool loadimage(IMAGE* img, const WCHAR* f, int w, int h) {
    return gx_loadimg4(img, f, w, h);
}
static inline bool loadimage(IMAGE* img, const char* f, int w, int h, bool resize) {
    return gx_loadimg5(img, gxWiden(f), w, h, resize);
}
static inline bool loadimage(IMAGE* img, const WCHAR* f, int w, int h, bool resize) {
    return gx_loadimg5(img, f, w, h, resize);
}

static inline bool saveimage(const char* f)  { return gx_saveimg1(gxWiden(f)); }
static inline bool saveimage(const WCHAR* f) { return gx_saveimg1(f); }
static inline bool saveimage(const char* f, const IMAGE* img)  { return gx_saveimg2(gxWiden(f), img); }
static inline bool saveimage(const WCHAR* f, const IMAGE* img) { return gx_saveimg2(f, img); }
/* The reversed order - see the note above gx_si2r().  EasyX does not offer
 * it; it is accepted here for the same reason the C macro does. */
static inline bool saveimage(const IMAGE* img, const char* f)  { return gx_saveimg2(gxWiden(f), img); }
static inline bool saveimage(const IMAGE* img, const WCHAR* f) { return gx_saveimg2(f, img); }

static inline void putimage(int x, int y, const IMAGE* img) {
    gxPutImage3(x, y, img);
}
static inline void putimage(int x, int y, const IMAGE* img, DWORD rop) {
    gxPutImage4(x, y, img, rop);
}
static inline void putimage(int dx, int dy, int dw, int dh, const IMAGE* img, int sx, int sy) {
    gxPutImage7(dx, dy, dw, dh, img, sx, sy);
}
static inline void putimage(int dx, int dy, int dw, int dh, const IMAGE* img, int sx, int sy, DWORD rop) {
    gxPutImage8(dx, dy, dw, dh, img, sx, sy, rop);
}

/* The corner opacity draw.  Five/seven arguments, the same pair as the C
 * spelling; C++ picks by arity, which is why the two share a name. */
static inline void alphagradpicture(double aTL, double aTR, double aBR,
                                    double aBL, const IMAGE* img) {
    gx_agp_5(aTL, aTR, aBR, aBL, img);
}
static inline void alphagradpicture(double dx, double dy, double aTL, double aTR,
                                    double aBR, double aBL, const IMAGE* img) {
    gx_agp_7(dx, dy, aTL, aTR, aBR, aBL, img);
}

/* Crossing one image into another source.  The weights are RELATIVE:
 * (1, img, 3, c) is 25% image and 75% colour.  The blend is premultiplied,
 * so a transparent part of either source contributes no colour at all. */
static inline void miximagec(double w1, const IMAGE* img, double w2, COLORREF c) {
    gx_mixc_4(w1, img, w2, c);
}
static inline void miximagec(double dx, double dy, double w1, const IMAGE* img,
                             double w2, COLORREF c) {
    gx_mixc_6(dx, dy, w1, img, w2, c);
}
static inline void miximagei(double w1, const IMAGE* a, double w2, const IMAGE* b) {
    gx_mixi_4(w1, a, w2, b);
}
static inline void miximagei(double dx, double dy, double w1, const IMAGE* a,
                             double w2, const IMAGE* b) {
    gx_mixi_6(dx, dy, w1, a, w2, b);
}

static inline void getimage(IMAGE* dst, int x, int y, int w, int h) {
    gxGetImage5(dst, x, y, w, h);
}
static inline void getimage(IMAGE* dst, const IMAGE* src, int x, int y, int w, int h) {
    gxGetImage6(dst, src, x, y, w, h);
}

static inline void rotateimage(IMAGE* dst, IMAGE* src, double rad) {
    gx_rotimg3(dst, src, rad);
}
static inline void rotateimage(IMAGE* dst, IMAGE* src, double rad, COLORREF bk) {
    gx_rotimg4(dst, src, rad, bk);
}
static inline void rotateimage(IMAGE* dst, IMAGE* src, double rad, COLORREF bk, bool autosize) {
    gx_rotimg5(dst, src, rad, bk, autosize);
}
static inline void rotateimage(IMAGE* dst, IMAGE* src, double rad, COLORREF bk,
                        bool autosize, bool highquality) {
    gx_rotimg6(dst, src, rad, bk, autosize, highquality);
}

/* EasyX: HWND initgraph(int width, int height, int flag = 0) */
static inline HWND initgraph(int w, int h) { return gx_initgraph2(w, h); }
static inline HWND initgraph(int w, int h, int flag) { return gx_initgraph3(w, h, flag); }

static inline void EndBatchDraw() { gx_endbatch4(0, 0, 0, 0); }
static inline void EndBatchDraw(int l, int t, int r, int b) { gx_endbatch4(l, t, r, b); }

/*--------------------------- line / fill style -------------------------*/
static inline void setlinestyle(int style) { gxSetLineStyle1(style); }
static inline void setlinestyle(int style, int thickness) { gxSetLineStyle2(style, thickness); }
static inline void setlinestyle(int style, int thickness, const DWORD* puserstyle) {
    gxSetLineStyle3(style, thickness, puserstyle);
}
static inline void setlinestyle(int style, int thickness, const DWORD* puserstyle,
                         DWORD userstylecount) {
    gxSetLineStyle4(style, thickness, puserstyle, userstylecount);
}
static inline void setlinestyle(const LINESTYLE* p) { gxSetLineStylePtr(p); }

static inline void setfillstyle(int style) { gxSetFillStyle1(style); }
static inline void setfillstyle(int style, long hatch) { gxSetFillStyle2(style, hatch); }
static inline void setfillstyle(int style, long hatch, IMAGE* ppattern) {
    gxSetFillStyle3(style, hatch, ppattern);
}
static inline void setfillstyle(const FILLSTYLE* p) { gxSetFillStylePtr(p); }

static inline void floodfill(int x, int y, COLORREF color) {
    gxFloodFill(x, y, color, FLOODFILLBORDER);
}
static inline void floodfill(int x, int y, COLORREF color, int filltype) {
    gxFloodFill(x, y, color, filltype);
}

/*------------------------------- input --------------------------------*/
static inline bool peekmessage(ExMessage* m) { return gxPeekEx(m, 0xFF, true); }
static inline bool peekmessage(ExMessage* m, BYTE f) { return gxPeekEx(m, f, true); }
static inline bool peekmessage(ExMessage* m, BYTE f, bool rmv) { return gxPeekEx(m, f, rmv); }
static inline bool peekmessage(MOUSEMSG* m) { return gxPeekMouse(m, 0xFF, true); }
static inline bool peekmessage(MOUSEMSG* m, BYTE f) { return gxPeekMouse(m, f, true); }
static inline bool peekmessage(MOUSEMSG* m, BYTE f, bool rmv) { return gxPeekMouse(m, f, rmv); }

static inline ExMessage getmessage() { return gxGetMsgEx(0xFF); }
static inline ExMessage getmessage(int filter) { return gxGetMsgEx((BYTE)filter); }
static inline ExMessage getmessage(BYTE filter) { return gxGetMsgEx(filter); }
static inline ExMessage getmessage(ExMessage* m) { return gxGetMsgFrom(m); }
static inline MOUSEMSG  getmessage(MOUSEMSG* m) { return gxGetMsgMouseFrom(m); }

/* getmessage(&msg, EX_KEY): filtered, into the caller's struct. */
static inline ExMessage getmessage(ExMessage* m, BYTE filter) {
    return gxGetMsgFromF(m, filter);
}
static inline ExMessage getmessage(ExMessage* m, int filter) {
    return gxGetMsgFromF(m, (BYTE)filter);
}
static inline MOUSEMSG  getmessage(MOUSEMSG* m, BYTE filter) {
    return gxGetMsgMouseFromF(m, filter);
}
static inline MOUSEMSG  getmessage(MOUSEMSG* m, int filter) {
    return gxGetMsgMouseFromF(m, (BYTE)filter);
}

/* variablewinsize(): the WM_SIZE watchers.  peekvariablemsg() is the one
 * for a render loop - it never blocks, and it answers with a bool: has a
 * resize arrived yet.  waitvariablemsg() is the blocking half and returns
 * nothing at all; it just parks the thread until the window changes size,
 * which never happens if variablewinsize() is off.
 *
 * peekvariablemsg() with no argument just answers "has one arrived yet",
 * leaving the message in the queue; peekvariablemsg(true) also removes it.
 * No ExMessage is involved - the new size is what getwidth() / getheight()
 * report afterwards. */
static inline bool peekvariablemsg()              { return gxPeekVarMsg(false); }
static inline bool peekvariablemsg(bool rmv)      { return gxPeekVarMsg(rmv); }

static inline void waitvariablemsg()               { gxWaitVarMsg(); }

/* SetWorkingImage(&img) draws into the IMAGE; SetWorkingImage() with no
 * argument goes back to the window. */
static inline void SetWorkingImage(IMAGE* pImg) { gxSetWorkingImage(pImg); }
static inline void SetWorkingImage()            { gxSetWorkingImage(NULL); }

/* InputBox(out, nMaxCount, pPrompt, pTitle, pDefault, width, height,
 *          bHideCancelBtn).  The 5th argument is EasyX's pDefault - a STRING, not
 *          a width: the old "int" overload for it was what stopped
 *          InputBox(s, 10, "prompt", "title", "default") from compiling. */
static inline bool inputbox(char* out, int n) {
    return gx_ibA(out, n, 0, 0, 0, 0, 0, true);
}
static inline bool inputbox(char* out, int n, const char* p) {
    return gx_ibA(out, n, p, 0, 0, 0, 0, true);
}
static inline bool inputbox(char* out, int n, const char* p, const char* t) {
    return gx_ibA(out, n, p, t, 0, 0, 0, true);
}
static inline bool inputbox(char* out, int n, const char* p, const char* t, const char* d) {
    return gx_ibA(out, n, p, t, d, 0, 0, true);
}
static inline bool inputbox(char* out, int n, const char* p, const char* t, const char* d, int w) {
    return gx_ibA(out, n, p, t, d, w, 0, true);
}
static inline bool inputbox(char* out, int n, const char* p, const char* t, const char* d, int w, int h) {
    return gx_ibA(out, n, p, t, d, w, h, true);
}
static inline bool inputbox(char* out, int n, const char* p, const char* t, const char* d, int w, int h, bool bHideCancelBtn) {
    return gx_ibA(out, n, p, t, d, w, h, bHideCancelBtn);
}
static inline bool inputbox(WCHAR* out, int n) {
    return gx_ibW(out, n, 0, 0, 0, 0, 0, true);
}
static inline bool inputbox(WCHAR* out, int n, const WCHAR* p) {
    return gx_ibW(out, n, p, 0, 0, 0, 0, true);
}
static inline bool inputbox(WCHAR* out, int n, const WCHAR* p, const WCHAR* t) {
    return gx_ibW(out, n, p, t, 0, 0, 0, true);
}
static inline bool inputbox(WCHAR* out, int n, const WCHAR* p, const WCHAR* t, const WCHAR* d) {
    return gx_ibW(out, n, p, t, d, 0, 0, true);
}
static inline bool inputbox(WCHAR* out, int n, const WCHAR* p, const WCHAR* t, const WCHAR* d, int w) {
    return gx_ibW(out, n, p, t, d, w, 0, true);
}
static inline bool inputbox(WCHAR* out, int n, const WCHAR* p, const WCHAR* t, const WCHAR* d, int w, int h) {
    return gx_ibW(out, n, p, t, d, w, h, true);
}
static inline bool inputbox(WCHAR* out, int n, const WCHAR* p, const WCHAR* t, const WCHAR* d, int w, int h, bool bHideCancelBtn) {
    return gx_ibW(out, n, p, t, d, w, h, bHideCancelBtn);
}
#define InputBox inputbox

static inline int messagebox(const char* text, const char* cap, UINT type) {
    return gx_msgboxA(text, cap, type);
}
static inline int messagebox(const WCHAR* text, const WCHAR* cap, UINT type) {
    return gx_msgboxW(text, cap, type);
}
static inline int messagebox(HWND h, const char* text, const char* cap, UINT type) {
    (void)h; return gx_msgboxA(text, cap, type);
}
static inline int messagebox(HWND h, const WCHAR* text, const WCHAR* cap, UINT type) {
    (void)h; return gx_msgboxW(text, cap, type);
}


/* The stroke family.  The width is the optional last argument: without it
 * the pen width of setlinestyle() is used.  POINT and POINTF, open and
 * closed, plus the two that fill and then rim.  All paint with the fill
 * colour - see the note above gxStrokeRibbon(). */
static inline void strokepolyline(const POINT* p, int n)                { gx_spl_2(p, n); }
static inline void strokepolyline(const POINT* p, int n, double w)      { gx_spl_3(p, n, w); }
static inline void strokepolylinef(const POINTF* p, int n)              { gx_splf_2(p, n); }
static inline void strokepolylinef(const POINTF* p, int n, double w)    { gx_splf_3(p, n, w); }
static inline void strokepolygon(const POINT* p, int n)                 { gx_spg_2(p, n); }
static inline void strokepolygon(const POINT* p, int n, double w)       { gx_spg_3(p, n, w); }
static inline void strokepolygonf(const POINTF* p, int n)               { gx_spgf_2(p, n); }
static inline void strokepolygonf(const POINTF* p, int n, double w)     { gx_spgf_3(p, n, w); }
static inline void fillstrokepolygon(const POINT* p, int n)             { gx_fspg_2(p, n); }
static inline void fillstrokepolygon(const POINT* p, int n, double w)   { gx_fspg_3(p, n, w); }
static inline void fillstrokepolygonf(const POINTF* p, int n)           { gx_fspgf_2(p, n); }
static inline void fillstrokepolygonf(const POINTF* p, int n, double w) { gx_fspgf_3(p, n, w); }

#else /* !__cplusplus */

/*======================================================================
 * C11 mode: EasyX relies on default arguments and overloads, neither of
 * which exist in C.  Every such API is emulated here: the macro counts
 * its arguments and picks an implementation, and string arguments are
 * routed to the A or the W flavour with _Generic (so TCHAR / _T("...")
 * behave exactly like they do in EasyX).
 *====================================================================*/

/* The original counter only tolerated 8 arguments: GX_HAS_COMMA() started
 * returning the 9th *call* argument instead of 0/1, so any EasyX API with
 * more than 8 parameters (settextstyle has 14) expanded to garbage. */
#define GX_ARG17(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,N,...) N
#define GX_HAS_COMMA(...) GX_ARG17(__VA_ARGS__, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 0, 0)
#define GX_TRIGGER(...) ,
#define GX_ISE_AD(a,d) GX_CAT2(GX_ISE_AD_, GX_CAT2(a,d))
#define GX_ISE_AD_00 0
#define GX_ISE_AD_01 1
#define GX_ISE_AD_10 0
#define GX_ISE_AD_11 0
#define GX_ISE(...) GX_ISE_AD(GX_HAS_COMMA(__VA_ARGS__), \
                              GX_HAS_COMMA(GX_TRIGGER __VA_ARGS__ ()))

#define GX_ARG_N(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10,_11,_12,_13,_14,_15,_16,N,...) N

/* Counting the arguments of one GX_DISPATCH() call.
 *
 * The hard part is telling "no argument at all" from "one argument": an
 * empty __VA_ARGS__ behaves exactly like a single empty argument, so a
 * plain positional counter cannot see the difference, and the usual
 * TRIGGER trick (GX_HAS_COMMA(GX_TRIGGER __VA_ARGS__ ())) mistakes a
 * *single argument whose first token is an opening parenthesis* for an
 * empty list.  That is why GetImageBuffer(NULL) used to expand to
 * gx_ibuf_0(NULL) and fail with
 *     macro "gx_ibuf_0" passed 1 arguments, but takes just 0
 * NULL expands to ((void*)0), so GX_TRIGGER really is followed by '('.
 *
 * No purely ISO C11 trick can tell those two apart, hence:
 *   - GNU mode             -> ", ##__VA_ARGS__" (the comma disappears)
 *   - GCC >= 8 / clang >= 9 -> __VA_OPT__
 *   - anything else        -> the TRIGGER counter, with the limitation
 *                             above: write GetImageBuffer() rather than
 *                             GetImageBuffer(NULL) on such a compiler.
 */
#if defined(__GNUC__) && !defined(__STRICT_ANSI__)
#define GX_NARG_PAD(...) 0, ##__VA_ARGS__
#define GX_NARG_I(...)   GX_ARG_N(__VA_ARGS__)
#define GX_NARG(...)     GX_NARG_I(GX_NARG_PAD(__VA_ARGS__),                  \
                             15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,-1,-2)
#elif (defined(__GNUC__) && __GNUC__ >= 8) ||                                 \
      (defined(__clang__) && __clang_major__ >= 9)
#define GX_NARG(...) GX_ARG_N(__VA_ARGS__ __VA_OPT__(,)                       \
                             16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1, 0)
#else
#define GX_NARG1(...) GX_ARG_N(__VA_ARGS__, 16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1, 0)
#define GX_NARG_IF(c, ...) GX_CAT2(GX_NARG_C, c)(__VA_ARGS__)
#define GX_NARG_C1(...) 0            /* empty argument list            */
#define GX_NARG_C0(...) GX_NARG1(__VA_ARGS__)
#define GX_NARG(...) GX_NARG_IF(GX_ISE(__VA_ARGS__), __VA_ARGS__)
#endif

#define GX_DISPATCH(PREFIX, ...) GX_CAT2(PREFIX, GX_NARG(__VA_ARGS__))(__VA_ARGS__)

/*------------------------- string A/W routing -------------------------*/
/* Pick the A or the W entry point from the type of the string argument. */


#define GX_STRSEL(s, FA, FW)                                                  \
    _Generic(((s) + 0),                                                        \
        char*:              FA((const char*)(s)),                              \
        const char*:        FA((const char*)(s)),                              \
        WCHAR*:             FW((const WCHAR*)(s)),                             \
        const WCHAR*:       FW((const WCHAR*)(s)))

/* EasyX also has TCHAR overloads, i.e. textwidth('A') / outtextxy(x,y,'A').
 * In C a character literal is an int, so it lands on "default". */
#define GX_STRSEL_CH(s, FA, FW, FC)                                           \
    _Generic(((s) + 0),                                                        \
        char*:              FA((const char*)(s)),                              \
        const char*:        FA((const char*)(s)),                              \
        WCHAR*:             FW((const WCHAR*)(s)),                             \
        const WCHAR*:       FW((const WCHAR*)(s)),                             \
        default:            FC((int)(size_t)(s)))
#define textwidth(s)     GX_STRSEL_CH(s, textwidthA, textwidthW, gx_textwidth_ch)
#define textheight(s)    GX_STRSEL_CH(s, textheightA, textheightW, gx_textheight_ch)
#define outtext(s)       GX_STRSEL_CH(s, outtextA, outtextW, gx_outtext_ch)
#define outtextxy(x, y, s)                                                    \
    _Generic(((s) + 0),                                                        \
        char*:        outtextxyA((x), (y), (const char*)(s)),                  \
        const char*:  outtextxyA((x), (y), (const char*)(s)),                  \
        WCHAR*:       outtextxyW((x), (y), (const WCHAR*)(s)),                 \
        const WCHAR*: outtextxyW((x), (y), (const WCHAR*)(s)),                 \
        default:      gx_outtextxy_ch((x), (y), (int)(size_t)(s)))
#define setwindowtext(s) GX_STRSEL(s, setwindowtextA, setwindowtextW)

/* drawtext(str, rect, format)  /  drawtext(x, y, str) */
#define GX_DT_POS(a, b, c)                                                    \
    _Generic(((c) + 0),                                                        \
        char*:        gx_drawtext_wrapA((int)(size_t)(a), (int)(size_t)(b), (const char*)(size_t)(c)), \
        const char*:  gx_drawtext_wrapA((int)(size_t)(a), (int)(size_t)(b), (const char*)(size_t)(c)), \
        WCHAR*:       gx_drawtext_wrapW((int)(size_t)(a), (int)(size_t)(b), (const WCHAR*)(size_t)(c)), \
        const WCHAR*: gx_drawtext_wrapW((int)(size_t)(a), (int)(size_t)(b), (const WCHAR*)(size_t)(c)), \
        default:      0)
#define gx_dt3(a, b, c)                                                       \
    _Generic(((a) + 0),                                                        \
        char*:        gx_drawtext_rectA((const char*)(size_t)(a), (const RECT*)(size_t)(b), (UINT)(size_t)(c)), \
        const char*:  gx_drawtext_rectA((const char*)(size_t)(a), (const RECT*)(size_t)(b), (UINT)(size_t)(c)), \
        WCHAR*:       gx_drawtext_rectW((const WCHAR*)(size_t)(a), (const RECT*)(size_t)(b), (UINT)(size_t)(c)), \
        const WCHAR*: gx_drawtext_rectW((const WCHAR*)(size_t)(a), (const RECT*)(size_t)(b), (UINT)(size_t)(c)), \
        default:      GX_DT_POS(a, b, c))
#define drawtext(...) GX_DISPATCH(gx_dt, __VA_ARGS__)

/*------------------------------ images --------------------------------*/
#define gx_li2(img, f)                                                        \
    _Generic(((f) + 0),                                                        \
        char*:        gx_loadimg2((img), gxWiden((const char*)(f))),           \
        const char*:  gx_loadimg2((img), gxWiden((const char*)(f))),           \
        WCHAR*:       gx_loadimg2((img), (const WCHAR*)(f)),                   \
        const WCHAR*: gx_loadimg2((img), (const WCHAR*)(f)))
#define gx_li3(img, f, w)                                                     \
    _Generic(((f) + 0),                                                        \
        char*:        gx_loadimg3((img), gxWiden((const char*)(f)), (w)),      \
        const char*:  gx_loadimg3((img), gxWiden((const char*)(f)), (w)),      \
        WCHAR*:       gx_loadimg3((img), (const WCHAR*)(f), (w)),              \
        const WCHAR*: gx_loadimg3((img), (const WCHAR*)(f), (w)))
#define gx_li4(img, f, w, h)                                                  \
    _Generic(((f) + 0),                                                        \
        char*:        gx_loadimg4((img), gxWiden((const char*)(f)), (w), (h)),  \
        const char*:  gx_loadimg4((img), gxWiden((const char*)(f)), (w), (h)),  \
        WCHAR*:       gx_loadimg4((img), (const WCHAR*)(f), (w), (h)),         \
        const WCHAR*: gx_loadimg4((img), (const WCHAR*)(f), (w), (h)))
#define gx_li5(img, f, w, h, r)                                               \
    _Generic(((f) + 0),                                                        \
        char*:        gx_loadimg5((img), gxWiden((const char*)(f)), (w), (h), (r)), \
        const char*:  gx_loadimg5((img), gxWiden((const char*)(f)), (w), (h), (r)), \
        WCHAR*:       gx_loadimg5((img), (const WCHAR*)(f), (w), (h), (r)),    \
        const WCHAR*: gx_loadimg5((img), (const WCHAR*)(f), (w), (h), (r)))
#define loadimage(...) GX_DISPATCH(gx_li, __VA_ARGS__)

#define gx_si1(f)                                                             \
    _Generic(((f) + 0),                                                        \
        char*:        gx_saveimg1(gxWiden((const char*)(f))),                  \
        const char*:  gx_saveimg1(gxWiden((const char*)(f))),                  \
        WCHAR*:       gx_saveimg1((const WCHAR*)(f)),                          \
        const WCHAR*: gx_saveimg1((const WCHAR*)(f)))
/* saveimage(img, file) - the reversed order.
 * EasyX only accepts (file, img), but getimage() takes the IMAGE first, so
 * the swap is an easy slip - and the diagnostic for it was unreadable:
 * "_Generic selector of type 'struct IMAGE *' is not compatible with any
 * association".  Both orders are accepted instead; which one it is follows
 * from the type of the FIRST argument, which is unambiguous because a file
 * name is never an IMAGE and an IMAGE is never a file name.
 * The trailing default: arms are never taken.  They exist because a
 * _Generic whose selector matches no association is an error even in an
 * arm that was not selected, and the arms that are not taken still have
 * to type check.
 */
#define gx_si2r(img, f)                                                       \
    _Generic(((f) + 0),                                                        \
        char*:        gx_saveimg2(gxWiden((const char*)(f)), (img)),           \
        const char*:  gx_saveimg2(gxWiden((const char*)(f)), (img)),           \
        WCHAR*:       gx_saveimg2((const WCHAR*)(f), (img)),                   \
        const WCHAR*: gx_saveimg2((const WCHAR*)(f), (img)),                   \
        default:      0)
#define gx_si2(f, img)                                                        \
    _Generic(((f) + 0),                                                        \
        char*:        gx_saveimg2(gxWiden((const char*)(f)), (const IMAGE*)(img)), \
        const char*:  gx_saveimg2(gxWiden((const char*)(f)), (const IMAGE*)(img)), \
        WCHAR*:       gx_saveimg2((const WCHAR*)(f), (const IMAGE*)(img)),     \
        const WCHAR*: gx_saveimg2((const WCHAR*)(f), (const IMAGE*)(img)),     \
        IMAGE*:       gx_si2r((const IMAGE*)(f), (img)),                       \
        const IMAGE*: gx_si2r((const IMAGE*)(f), (img)),                       \
        default:      0)
#define saveimage(...) GX_DISPATCH(gx_si, __VA_ARGS__)


#define putimage(...)    GX_DISPATCH(gxPutImage, __VA_ARGS__)
#define alphagradpicture(...) GX_DISPATCH(gx_agp_, __VA_ARGS__)
#define miximagec(...)        GX_DISPATCH(gx_mixc_, __VA_ARGS__)
#define miximagei(...)        GX_DISPATCH(gx_mixi_, __VA_ARGS__)
#define strokepolyline(...)     GX_DISPATCH(gx_spl_, __VA_ARGS__)
#define strokepolylinef(...)    GX_DISPATCH(gx_splf_, __VA_ARGS__)
#define strokepolygon(...)      GX_DISPATCH(gx_spg_, __VA_ARGS__)
#define strokepolygonf(...)     GX_DISPATCH(gx_spgf_, __VA_ARGS__)
#define fillstrokepolygon(...)  GX_DISPATCH(gx_fspg_, __VA_ARGS__)
#define fillstrokepolygonf(...) GX_DISPATCH(gx_fspgf_, __VA_ARGS__)
#define getimage(...)    GX_DISPATCH(gxGetImage, __VA_ARGS__)
#define rotateimage(...) GX_DISPATCH(gx_rotimg, __VA_ARGS__)
#define initgraph(...)   GX_DISPATCH(gx_initgraph, __VA_ARGS__)
#define EndBatchDraw(...) GX_DISPATCH(gx_endbatch, __VA_ARGS__)
#define gx_endbatch0() gx_endbatch4(0, 0, 0, 0)

/*--------------------------- line / fill style -------------------------*/
#define gx_ls_1(p)                                                            \
    _Generic(((p) + 0),                                                        \
        LINESTYLE*:       gxSetLineStylePtr((const LINESTYLE*)(p)),            \
        const LINESTYLE*: gxSetLineStylePtr((const LINESTYLE*)(p)),            \
        default:          gxSetLineStyle1((int)(size_t)(p)))
#define gx_ls_2(a, b) gxSetLineStyle2((a), (b))
#define gx_ls_3(a, b, c) gxSetLineStyle3((a), (b), (c))
#define gx_ls_4(a, b, c, d) gxSetLineStyle4((a), (b), (c), (d))
#define setlinestyle(...) GX_DISPATCH(gx_ls_, __VA_ARGS__)

#define gx_fs_1(p)                                                            \
    _Generic(((p) + 0),                                                        \
        FILLSTYLE*:       gxSetFillStylePtr((const FILLSTYLE*)(p)),            \
        const FILLSTYLE*: gxSetFillStylePtr((const FILLSTYLE*)(p)),            \
        default:          gxSetFillStyle1((int)(size_t)(p)))
#define gx_fs_2(a, b) gxSetFillStyle2((a), (b))
#define gx_fs_3(a, b, c) gxSetFillStyle3((a), (b), (c))
#define setfillstyle(...) GX_DISPATCH(gx_fs_, __VA_ARGS__)

#define gx_ff3(x, y, c) gxFloodFill((x), (y), (c), FLOODFILLBORDER)
#define gx_ff4(x, y, c, t) gxFloodFill((x), (y), (c), (t))
#define floodfill(...) GX_DISPATCH(gx_ff, __VA_ARGS__)

/* setviewport / getviewport take an optional trailing clip argument
 * (EasyX defaults it to 1).  C has no default arguments, so dispatch on
 * the argument count. */
#define gx_vp4(l, t, r, b)      setviewport((l), (t), (r), (b), 1)
#define gx_vp5(l, t, r, b, c)   setviewport((l), (t), (r), (b), (c))
#define setviewport(...) GX_DISPATCH(gx_vp, __VA_ARGS__)
#define gx_gvp4(l, t, r, b)     getviewport((l), (t), (r), (b), (int*)0)
#define gx_gvp5(l, t, r, b, c)  getviewport((l), (t), (r), (b), (c))
#define getviewport(...) GX_DISPATCH(gx_gvp, __VA_ARGS__)

/*------------------------------- input --------------------------------*/
#define gx_pm_1(m)                                                            \
    _Generic(((m) + 0),                                                        \
        ExMessage*:       gxPeekEx((ExMessage*)(m), 0xFF, true),               \
        const ExMessage*: gxPeekEx((ExMessage*)(m), 0xFF, true),               \
        MOUSEMSG*:        gxPeekMouse((MOUSEMSG*)(m), 0xFF, true),             \
        const MOUSEMSG*:  gxPeekMouse((MOUSEMSG*)(m), 0xFF, true))
#define gx_pm_2(m, f)                                                         \
    _Generic(((m) + 0),                                                        \
        ExMessage*:       gxPeekEx((ExMessage*)(m), (f), true),                \
        const ExMessage*: gxPeekEx((ExMessage*)(m), (f), true),                \
        MOUSEMSG*:        gxPeekMouse((MOUSEMSG*)(m), (f), true),              \
        const MOUSEMSG*:  gxPeekMouse((MOUSEMSG*)(m), (f), true))
#define gx_pm_3(m, f, r)                                                      \
    _Generic(((m) + 0),                                                        \
        ExMessage*:       gxPeekEx((ExMessage*)(m), (f), (r)),                 \
        const ExMessage*: gxPeekEx((ExMessage*)(m), (f), (r)),                 \
        MOUSEMSG*:        gxPeekMouse((MOUSEMSG*)(m), (f), (r)),               \
        const MOUSEMSG*:  gxPeekMouse((MOUSEMSG*)(m), (f), (r)))
#define peekmessage(...) GX_DISPATCH(gx_pm_, __VA_ARGS__)

#define gx_gm_0() gxGetMsgEx(0xFF)
#define gx_gm_1(f)                                                            \
    _Generic(((f) + 0),                                                        \
        ExMessage*:       gxGetMsgFrom((ExMessage*)(f)),                       \
        const ExMessage*: gxGetMsgFrom((ExMessage*)(f)),                       \
        MOUSEMSG*:        gxGetMsgMouseFrom((MOUSEMSG*)(f)),                   \
        const MOUSEMSG*:  gxGetMsgMouseFrom((MOUSEMSG*)(f)),                   \
        default:          gxGetMsgEx((BYTE)(size_t)(f)))
/* Two arguments: getmessage(&msg, EX_KEY) writes the message into msg and
 * waits for one matching the filter.  The pointer used to fall into the
 * integer branch, so the address was cast to a BYTE and the filter was
 * dropped - the call never woke on the key it asked for.  A plain integer
 * first argument keeps its old meaning (the filter itself). */
#define gx_gm_2(f, r)                                                           \
    _Generic(((f) + 0),                                                         \
        ExMessage*:       gxGetMsgFromF((ExMessage*)(f), (BYTE)(size_t)(r)),    \
        const ExMessage*: gxGetMsgFromF((ExMessage*)(f), (BYTE)(size_t)(r)),    \
        MOUSEMSG*:        gxGetMsgMouseFromF((MOUSEMSG*)(f), (BYTE)(size_t)(r)),\
        const MOUSEMSG*:  gxGetMsgMouseFromF((MOUSEMSG*)(f), (BYTE)(size_t)(r)),\
        default:          gxGetMsgEx((BYTE)(size_t)(f)))
#define getmessage(...) GX_DISPATCH(gx_gm_, __VA_ARGS__)

/* variablewinsize(): the WM_SIZE watchers, C11 form.  Two shapes: no
 * argument = has one arrived (leaves it queued), one argument = say whether
 * to remove.  Both answer with a bool, and neither touches ExMessage - the
 * new size is what getwidth() / getheight() report afterwards.
 *
 * waitvariablemsg() takes no argument and returns nothing - it just blocks
 * until the window changes size. */
#define gx_pvm_0()      gxPeekVarMsg(false)
#define gx_pvm_1(r)     gxPeekVarMsg((r))
#define peekvariablemsg(...) GX_DISPATCH(gx_pvm_, __VA_ARGS__)

#define waitvariablemsg() gxWaitVarMsg()

/* InputBox(out, nMaxCount, pPrompt, pTitle, pDefault, width, height,
 *          bHideCancelBtn) - eight arguments, and every trailing one optional:
 * pPrompt / pTitle / pDefault NULL, width / height 0 (auto size), bHideCancelBtn
 * true (a single OK button), which is what EasyX defaults them to.
 * Only two to five arguments used to be dispatchable, so anything that
 * passed pDefault or further did not compile at all. */
#define GX_IB_CALL_A(o, n, p, t, d, w, h, b)                                   \
    gx_ibA((char*)(o), (n), (const char*)(p), (const char*)(t),                \
           (const char*)(d), (w), (h), (b))
#define GX_IB_CALL_W(o, n, p, t, d, w, h, b)                                   \
    gx_ibW((WCHAR*)(o), (n), (const WCHAR*)(p), (const WCHAR*)(t),             \
           (const WCHAR*)(d), (w), (h), (b))
#define gx_ib_2(o, n)                                                          \
    _Generic(((o) + 0),                                                        \
        char*:  GX_IB_CALL_A(o, n, 0, 0, 0, 0, 0, true),                       \
        WCHAR*: GX_IB_CALL_W(o, n, 0, 0, 0, 0, 0, true))
#define gx_ib_3(o, n, p)                                                       \
    _Generic(((o) + 0),                                                        \
        char*:  GX_IB_CALL_A(o, n, p, 0, 0, 0, 0, true),                       \
        WCHAR*: GX_IB_CALL_W(o, n, p, 0, 0, 0, 0, true))
#define gx_ib_4(o, n, p, t)                                                    \
    _Generic(((o) + 0),                                                        \
        char*:  GX_IB_CALL_A(o, n, p, t, 0, 0, 0, true),                       \
        WCHAR*: GX_IB_CALL_W(o, n, p, t, 0, 0, 0, true))
#define gx_ib_5(o, n, p, t, d)                                                 \
    _Generic(((o) + 0),                                                        \
        char*:  GX_IB_CALL_A(o, n, p, t, d, 0, 0, true),                       \
        WCHAR*: GX_IB_CALL_W(o, n, p, t, d, 0, 0, true))
#define gx_ib_6(o, n, p, t, d, w)                                              \
    _Generic(((o) + 0),                                                        \
        char*:  GX_IB_CALL_A(o, n, p, t, d, w, 0, true),                       \
        WCHAR*: GX_IB_CALL_W(o, n, p, t, d, w, 0, true))
#define gx_ib_7(o, n, p, t, d, w, h)                                           \
    _Generic(((o) + 0),                                                        \
        char*:  GX_IB_CALL_A(o, n, p, t, d, w, h, true),                       \
        WCHAR*: GX_IB_CALL_W(o, n, p, t, d, w, h, true))
#define gx_ib_8(o, n, p, t, d, w, h, b)                                        \
    _Generic(((o) + 0),                                                        \
        char*:  GX_IB_CALL_A(o, n, p, t, d, w, h, (bool)(b)),                  \
        WCHAR*: GX_IB_CALL_W(o, n, p, t, d, w, h, (bool)(b)))
#define inputbox(...) GX_DISPATCH(gx_ib_, __VA_ARGS__)
#define InputBox(...) GX_DISPATCH(gx_ib_, __VA_ARGS__)

/*--------------------------- working image ---------------------------*/
/* SetWorkingImage() / SetWorkingImage(&img): zero or one argument, which
 * a plain C function cannot offer.  NULL is what "no argument" means to
 * gxSetWorkingImage() - it clears the IMAGE and restores the window. */
/* gx_swi_0 still takes (...) even though GX_NARG() now counts correctly:
 * on a compiler with neither ", ##__VA_ARGS__" nor __VA_OPT__ the old
 * TRIGGER counter is used, and there SetWorkingImage(NULL) really does
 * land on the zero-argument macro.  Absorbing the argument keeps that
 * case compiling, and the result is the same either way - NULL is the
 * only value that can land here and it means "the window". */
#define gx_swi_0(...)   gxSetWorkingImage((IMAGE*)NULL)
#define gx_swi_1(p)     gxSetWorkingImage((IMAGE*)(p))
#define SetWorkingImage(...) GX_DISPATCH(gx_swi_, __VA_ARGS__)

#define gx_mb_3(a, b, c)                                                      \
    _Generic(((a) + 0),                                                        \
        char*:        gx_msgboxA((const char*)(a), (const char*)(b), (c)),     \
        const char*:  gx_msgboxA((const char*)(a), (const char*)(b), (c)),     \
        WCHAR*:       gx_msgboxW((const WCHAR*)(a), (const WCHAR*)(b), (c)),   \
        const WCHAR*: gx_msgboxW((const WCHAR*)(a), (const WCHAR*)(b), (c)))
#define gx_mb_4(h2, a, b, c) gx_mb_3(a, b, c)
#define messagebox(...) GX_DISPATCH(gx_mb_, __VA_ARGS__)

/*------------------------- EasyX additions ---------------------------*/
/* settextstyle(LOGFONT*) / (h, w, face) / (... 9 ...) / (... 14 ...) */
#define GX_TS_FACE(h, wd, f, PREFIX)                                          \
    _Generic(((f) + 0),                                                        \
        char*:        PREFIX##A((h), (wd), (const char*)(f)),                  \
        const char*:  PREFIX##A((h), (wd), (const char*)(f)),                  \
        WCHAR*:       PREFIX##W((h), (wd), (const WCHAR*)(f)),                 \
        const WCHAR*: PREFIX##W((h), (wd), (const WCHAR*)(f)))
#define gx_ts_1(p) gxSetTextStylePtr((const LOGFONTA*)(size_t)(p))
#define gx_ts_3(h, wd, f) GX_TS_FACE(h, wd, f, gxSetTextStyle3)
#define gx_ts_9(h, wd, f, e, o, we, it, un, sk)                               \
    _Generic(((f) + 0),                                                        \
        char*:        gxSetTextStyle9A((h), (wd), (const char*)(f),            \
                          (e), (o), (we), (it), (un), (sk)),                   \
        const char*:  gxSetTextStyle9A((h), (wd), (const char*)(f),            \
                          (e), (o), (we), (it), (un), (sk)),                   \
        WCHAR*:       gxSetTextStyle9W((h), (wd), (const WCHAR*)(f),           \
                          (e), (o), (we), (it), (un), (sk)),                   \
        const WCHAR*: gxSetTextStyle9W((h), (wd), (const WCHAR*)(f),           \
                          (e), (o), (we), (it), (un), (sk)))
#define gx_ts_14(h, wd, f, e, o, we, it, un, sk, cs, op, cp, q, pf)           \
    _Generic(((f) + 0),                                                        \
        char*:        gxSetTextStyleA((h), (wd), (const char*)(f),             \
                          (e), (o), (we), (it), (un), (sk),                    \
                          (BYTE)(cs), (BYTE)(op), (BYTE)(cp), (BYTE)(q), (BYTE)(pf)), \
        const char*:  gxSetTextStyleA((h), (wd), (const char*)(f),             \
                          (e), (o), (we), (it), (un), (sk),                    \
                          (BYTE)(cs), (BYTE)(op), (BYTE)(cp), (BYTE)(q), (BYTE)(pf)), \
        WCHAR*:       gxSetTextStyleW((h), (wd), (const WCHAR*)(f),            \
                          (e), (o), (we), (it), (un), (sk),                    \
                          (BYTE)(cs), (BYTE)(op), (BYTE)(cp), (BYTE)(q), (BYTE)(pf)), \
        const WCHAR*: gxSetTextStyleW((h), (wd), (const WCHAR*)(f),            \
                          (e), (o), (we), (it), (un), (sk),                    \
                          (BYTE)(cs), (BYTE)(op), (BYTE)(cp), (BYTE)(q), (BYTE)(pf)))
#define settextstyle(...) GX_DISPATCH(gx_ts_, __VA_ARGS__)

/* GetImageBuffer() / GetImageBuffer(img), same for GetImageHDC */
#define gx_ibuf_0() gxGetImageBuffer((IMAGE*)0)
#define gx_ibuf_1(p) gxGetImageBuffer((IMAGE*)(size_t)(p))
#define GetImageBuffer(...) GX_DISPATCH(gx_ibuf_, __VA_ARGS__)
#define gx_ihdc_0() gxGetImageHDC((IMAGE*)0)
#define gx_ihdc_1(p) gxGetImageHDC((IMAGE*)(size_t)(p))
#define GetImageHDC(...) GX_DISPATCH(gx_ihdc_, __VA_ARGS__)

/* flushmessage() / flushmessage(filter) */
#define flushmessage(...) GX_DISPATCH(gx_flushmsg, __VA_ARGS__)

/* PeekMouseMsg(msg) / PeekMouseMsg(msg, remove) */
#define PeekMouseMsg(...) GX_DISPATCH(gx_peekmousemsg, __VA_ARGS__)

/* FlushBatchDraw() / FlushBatchDraw(l, t, r, b) */
#define FlushBatchDraw(...) GX_DISPATCH(gx_flushbatch, __VA_ARGS__)

#endif /* __cplusplus */


/* GL enums that a stock GL 1.1 gl.h does not define.  easygl only needs a
 * handful of them itself, so the rest arrive with the mesh interface. */
#ifndef GL_FRAMEBUFFER_BINDING
#define GL_FRAMEBUFFER_BINDING            0x8CA6
#endif
#ifndef GL_CURRENT_PROGRAM
#define GL_CURRENT_PROGRAM                0x8B8D
#endif
#ifndef GL_ARRAY_BUFFER_BINDING
#define GL_ARRAY_BUFFER_BINDING           0x8894
#endif
#ifndef GL_ACTIVE_TEXTURE
#define GL_ACTIVE_TEXTURE                 0x84E0
#endif
#ifndef GL_TEXTURE_BINDING_2D
#define GL_TEXTURE_BINDING_2D             0x8069
#endif
#ifndef GL_VERTEX_ATTRIB_ARRAY_ENABLED
#define GL_VERTEX_ATTRIB_ARRAY_ENABLED    0x8622
#endif
#ifndef GL_DEPTH_COMPONENT24
#define GL_DEPTH_COMPONENT24              0x81A6
#endif
#ifndef GL_DEPTH_COMPONENT16
#define GL_DEPTH_COMPONENT16              0x81A5
#endif
#ifndef GL_DEPTH_ATTACHMENT
#define GL_DEPTH_ATTACHMENT               0x8D00
#endif
#ifndef GL_RENDERBUFFER
#define GL_RENDERBUFFER                   0x8D41
#endif
#ifndef GL_ELEMENT_ARRAY_BUFFER
#define GL_ELEMENT_ARRAY_BUFFER           0x8893
#endif
#ifndef GL_TRIANGLE_FAN
#define GL_TRIANGLE_FAN                   0x0006
#endif

/*======================================================================
 *  20. GPU mesh interface  (EASYGL extension, no EasyX counterpart)
 *====================================================================*/
/* Why this exists
 * ---------------
 * A software rasteriser has to pay for every pixel on every frame: shade
 * it, then hand the finished frame to the GPU as a texture.  A 1280x720
 * canvas is 921600 pixels, so that is 3.7 MB of upload per frame on top of
 * the CPU shading -- and it is the reason a voxel world crawls while the
 * same hardware runs Minecraft at hundreds of frames per second.
 *
 * The world is static.  That is what makes the fix possible: upload the
 * geometry once, then let the GPU replay it with one draw call per frame.
 * No CPU shading, no upload, no per pixel work at all.
 *
 * Three calls a frame:
 *
 *     if (gxmeshbegin()) {
 *         gxmeshdraw(mesh, mvp, atlas);
 *         gxmeshend();
 *     } else {
 *         ... software fallback ...
 *     }
 *
 * gxmeshbegin() returns false forever if the driver cannot do it, so the
 * fallback is a normal supported path and not dead code.
 *
 * State hygiene
 * -------------
 * easygl keeps vertex attribute arrays 0/1/2 enabled and pointed at its
 * own vertex buffer, and OpenGL ignores the fixed function arrays whenever
 * a generic array of the same index is enabled.  A mesh therefore gets its
 * own VAO, which carries its own attribute bindings and cannot be clobbered
 * by easygl's calls.  Where VAOs are unavailable the enable bits are saved
 * and restored instead.  Everything else -- program, framebuffer, viewport,
 * viewport, blend, depth, texture unit -- is saved and restored too, so
 * easygl's 2D drawing is unaffected.
 */

typedef struct GXMESH {
    GLuint vao, vbo, ibo;      /* GL names; vao is 0 where unsupported     */
    int    nv, ni;             /* vertex count, index count                */
} GXMESH;

/* Vertex layout: x,y,z, u,v, r,g,b -- 8 floats, 32 bytes, interleaved.
 * r/g/b are a multiplier in 0..1, so per face lighting costs nothing. */
#define GXMESH_STRIDE 8

static GLuint g_gx_gmProg = 0;
static GLint  g_gx_gmUmvp = -1, g_gx_gmUtex = -1, g_gx_gmUuseTex = -1, g_gx_gmUflip = -1;
static GLint  g_gx_gmUeye = -1, g_gx_gmUfog = -1, g_gx_gmUfogCol = -1;
/* g_gx_gmFbo is where geometry is rasterised.  With MSAA on it owns two
 * multisampled RENDERBUFFERs and g_gx_gmFboR is the single sample FBO that
 * holds g_gx_gmTex; meshend() resolves into it before compositing.  With
 * MSAA off g_gx_gmFbo attaches g_gx_gmTex directly and g_gx_gmFboR is 0,
 * which is exactly what this used to do. */
static GLuint g_gx_gmFbo = 0, g_gx_gmTex = 0, g_gx_gmRb = 0, g_gx_gmQuad = 0;
static GLuint g_gx_gmFboR = 0, g_gx_gmRbC = 0;
static int    g_gx_gmSamples = 0;         /* samples the target was built with */
static int    g_gx_gmW = 0, g_gx_gmH = 0;
static int    g_gx_gmTried = 0, g_gx_gmOk = 0;
static GLuint g_gx_gmQuadVao = 0;         /* the composite quad's own VAO     */
static int    g_gx_gmIn = 0;              /* 1 while a 3D pass is open        */
static int    g_gx_gmFlip = 0;            /* 0 = image already upright        */
static float  g_gx_gmFogNear = 1.0e30f, g_gx_gmFogFar = 2.0e30f;
static float  g_gx_gmFogCol[3] = { 0.588f, 0.725f, 0.882f };
static float  g_gx_gmEye[3] = { 0, 0, 0 };

static const char* GX_GM_VS =
    "#version 120\n"
    "attribute vec3 aPos;\n"
    "attribute vec2 aUV;\n"
    "attribute vec3 aColor;\n"
    "uniform mat4 uMVP;\n"
    "uniform float uFlip;\n"
    "uniform vec3 uEye;\n"
    "varying vec2 vUV;\n"
    "varying vec3 vColor;\n"
    "varying float vDist;\n"
    "void main(){\n"
    "  vUV = aUV; vColor = aColor;\n"
    "  vDist = distance(aPos, uEye);\n"
    "  vec4 p = uMVP * vec4(aPos, 1.0);\n"
    "  p.y *= uFlip;\n"
    "  gl_Position = p;\n"
    "}\n";

static const char* GX_GM_FS =
    "#version 120\n"
    "uniform sampler2D uTex;\n"
    "uniform int uUseTex;\n"
    "uniform vec2 uFog;\n"
    "uniform vec3 uFogCol;\n"
    "varying vec2 vUV;\n"
    "varying vec3 vColor;\n"
    "varying float vDist;\n"
    "void main(){\n"
    "  vec4 t = texture2D(uTex, vUV);\n"
    "  vec3 c = (uUseTex == 1) ? t.rgb * vColor : vColor;\n"
    /* Alpha is the composite mask, not the texture's: meshbegin() clears the
     * offscreen target to alpha 0 and meshend() blends it over whatever 2D
     * drawing is already on the canvas, so a fragment that took its alpha
     * from the texture would blend itself away and leave only the sky. */
    "  float a = 1.0;\n"
    "  float f = clamp((vDist - uFog.x) / max(uFog.y - uFog.x, 0.0001), 0.0, 1.0);\n"
    "  f *= f;\n"
    "  gl_FragColor = vec4(mix(c, uFogCol, f), a);\n"
    "}\n";

/* ---- saved GL state ------------------------------------------------ */
typedef struct GxMeshSave {
    GLint prog, fbo, vp[4], abuf, tex2d, activeTex;
    GLint blend, depth, cull, scissor, depthFunc, depthMask, ibuf;
    GLint attrOn[3];
} GxMeshSave;

/* Saved across meshbegin() .. meshend().  Declared here, not with the
 * other statics, because the type above has to exist first. */
static GxMeshSave g_gx_gmSave;

static void gxmesh_push(GxMeshSave* s) {
    glGetIntegerv(GL_CURRENT_PROGRAM, &s->prog);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &s->fbo);
    glGetIntegerv(GL_VIEWPORT, s->vp);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &s->abuf);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &s->activeTex);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &s->tex2d);
    s->blend    = glIsEnabled(GL_BLEND);
    s->depth    = glIsEnabled(GL_DEPTH_TEST);
    s->cull     = glIsEnabled(GL_CULL_FACE);
    s->scissor  = glIsEnabled(GL_SCISSOR_TEST);
    glGetIntegerv(GL_DEPTH_FUNC, &s->depthFunc);
    glGetIntegerv(GL_DEPTH_WRITEMASK, &s->depthMask);
    if (!glBindVertexArray) {
        glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &s->attrOn[0]);
        glGetVertexAttribiv(1, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &s->attrOn[1]);
        glGetVertexAttribiv(2, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &s->attrOn[2]);
    } else { s->attrOn[0] = s->attrOn[1] = s->attrOn[2] = 0; }
}

static void gxmesh_pop(const GxMeshSave* s) {
    glUseProgram((GLuint)s->prog);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)s->fbo);
    glViewport(s->vp[0], s->vp[1], s->vp[2], s->vp[3]);
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)s->abuf);
    if (s->blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (s->depth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (s->cull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (s->scissor) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    glDepthFunc((GLenum)s->depthFunc);
    glDepthMask((GLboolean)s->depthMask);
    glActiveTexture((GLenum)s->activeTex);
    glBindTexture(GL_TEXTURE_2D, (GLuint)s->tex2d);
    if (!glBindVertexArray) {
        glBindVertexArray(0);
        if (s->attrOn[0]) glEnableVertexAttribArray(0);
        if (s->attrOn[1]) glEnableVertexAttribArray(1);
        if (s->attrOn[2]) glEnableVertexAttribArray(2);
    } else glBindVertexArray(0);
    /* easygl's pointers are still bound to its own VBO; nothing above
     * overwrote them because a VAO of 0 was current while we drew. */
}

/* ---- lazy setup ---------------------------------------------------- */
static int gxmesh_setup(void) {
    if (g_gx_gmTried) return g_gx_gmOk;
    g_gx_gmTried = 1;
    if (!g_gx_glReady || !glCreateProgram) { g_gx_gmOk = 0; return 0; }

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    GLint ok = 0;
    char log[512];
    memset(log, 0, sizeof(log));
    glShaderSource(vs, 1, &GX_GM_VS, NULL);
    glCompileShader(vs);
    glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        glGetShaderInfoLog(vs, 511, NULL, log);
        fprintf(stderr, "gxmesh: vertex shader\n%s\n", log);
        glDeleteShader(vs); glDeleteShader(fs); return 0;
    }
    ok = 0;
    glShaderSource(fs, 1, &GX_GM_FS, NULL);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        glGetShaderInfoLog(fs, 511, NULL, log);
        fprintf(stderr, "gxmesh: fragment shader\n%s\n", log);
        glDeleteShader(vs); glDeleteShader(fs); return 0;
    }
    g_gx_gmProg = glCreateProgram();
    glAttachShader(g_gx_gmProg, vs);
    glAttachShader(g_gx_gmProg, fs);
    /* GLSL 120 has no layout qualifier: without this the driver picks the
     * slots and the geometry reads whatever the disabled attributes
     * default to.  This exact omission produced an all white window. */
    glBindAttribLocation(g_gx_gmProg, 0, "aPos");
    glBindAttribLocation(g_gx_gmProg, 1, "aUV");
    glBindAttribLocation(g_gx_gmProg, 2, "aColor");
    glLinkProgram(g_gx_gmProg);
    glGetProgramiv(g_gx_gmProg, GL_LINK_STATUS, &ok);
    glDeleteShader(vs); glDeleteShader(fs);
    if (!ok) {
        glGetProgramInfoLog(g_gx_gmProg, 511, NULL, log);
        fprintf(stderr, "gxmesh: link\n%s\n", log);
        glDeleteProgram(g_gx_gmProg); g_gx_gmProg = 0; return 0;
    }
    g_gx_gmUmvp    = glGetUniformLocation(g_gx_gmProg, "uMVP");
    g_gx_gmUtex    = glGetUniformLocation(g_gx_gmProg, "uTex");
    g_gx_gmUuseTex = glGetUniformLocation(g_gx_gmProg, "uUseTex");
    g_gx_gmUflip   = glGetUniformLocation(g_gx_gmProg, "uFlip");
    g_gx_gmUeye    = glGetUniformLocation(g_gx_gmProg, "uEye");
    g_gx_gmUfog    = glGetUniformLocation(g_gx_gmProg, "uFog");
    g_gx_gmUfogCol = glGetUniformLocation(g_gx_gmProg, "uFogCol");

    /* full screen quad in clip space: pos3, uv2, rgb3 */
    static const float quad[4 * 8] = {
        -1, -1, 0,   0, 0,   1, 1, 1,
         1, -1, 0,   1, 0,   1, 1, 1,
         1,  1, 0,   1, 1,   1, 1, 1,
        -1,  1, 0,   0, 1,   1, 1, 1,
    };
    glGenBuffers(1, &g_gx_gmQuad);
    glBindBuffer(GL_ARRAY_BUFFER, g_gx_gmQuad);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    /* The composite quad needs its own VAO.  Setting the pointers with
     * VAO 0 current would overwrite easygl's own bindings, which are only
     * restored by enable bits, not by pointers. */
    if (glGenVertexArrays) {
        glGenVertexArrays(1, &g_gx_gmQuadVao);
        if (g_gx_gmQuadVao) {
            glBindVertexArray(g_gx_gmQuadVao);
            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1);
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, GXMESH_STRIDE * sizeof(float), (const void*)0);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, GXMESH_STRIDE * sizeof(float), (const void*)12);
            glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, GXMESH_STRIDE * sizeof(float), (const void*)20);
            glBindVertexArray(0);
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    g_gx_gmOk = 1;
    return 1;
}

static void gxmesh_target_free(void) {
    if (g_gx_gmFboR) { glDeleteFramebuffers(1, &g_gx_gmFboR); g_gx_gmFboR = 0; }
    if (g_gx_gmFbo)  { glDeleteFramebuffers(1, &g_gx_gmFbo);  g_gx_gmFbo  = 0; }
    if (g_gx_gmRbC)  { glDeleteRenderbuffers(1, &g_gx_gmRbC); g_gx_gmRbC  = 0; }
    if (g_gx_gmRb)   { glDeleteRenderbuffers(1, &g_gx_gmRb);  g_gx_gmRb   = 0; }
    if (g_gx_gmTex)  { glDeleteTextures(1, &g_gx_gmTex);      g_gx_gmTex  = 0; }
}

/* Depth attachment: 24 bit first, 16 is the fall back an old driver may
 * insist on.  samples >= 2 asks for a multisampled one, which it has to
 * be whenever the colour attachment is: GL refuses a framebuffer that
 * mixes multisampled and single sampled attachments. */
static int gxmesh_depth(int w, int h, int samples) {
    static const GLenum fmts[2] = { GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT16 };
    int i;
    if (g_gx_gmRb) { glDeleteRenderbuffers(1, &g_gx_gmRb); g_gx_gmRb = 0; }
    glGenRenderbuffers(1, &g_gx_gmRb);
    glBindRenderbuffer(GL_RENDERBUFFER, g_gx_gmRb);
    for (i = 0; i < 2; i++) {
        if (samples >= 2 && glRenderbufferStorageMultisample)
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, (GLsizei)samples,
                                             fmts[i], (GLsizei)w, (GLsizei)h);
        else
            glRenderbufferStorage(GL_RENDERBUFFER, fmts[i], (GLsizei)w, (GLsizei)h);
        if (glGetError() == GL_NO_ERROR) return 1;
    }
    return 0;
}

/* The offscreen target is rebuilt whenever the canvas changes size or the
 * requested sample count does.  It needs a depth attachment because the
 * canvas framebuffer has none.
 *
 * setaasamples() used to have no effect on this pass at all: the geometry
 * never touched the canvas framebuffer, which is the only one MSAA was
 * ever applied to.  With samples >= 2 the target becomes two multisampled
 * renderbuffers plus g_gx_gmFboR, the single sample FBO that meshend()
 * resolves into.  With 0 it is the one FBO with the texture attached,
 * exactly as before. */
static int gxmesh_target(int w, int h) {
    int want, s;
    if (w < 1 || h < 1) return 0;
    want = (g_gx_aaSamples >= 2 && glRenderbufferStorageMultisample && glBlitFramebuffer)
           ? g_gx_aaSamples : 0;
    if (g_gx_gmFbo && w == g_gx_gmW && h == g_gx_gmH && want == g_gx_gmSamples) return 1;

    gxmesh_target_free();
    /* meshend() always samples g_gx_gmTex, so it exists either way: with
     * MSAA on it is the resolve destination rather than the draw target. */
    glGenTextures(1, &g_gx_gmTex);
    glBindTexture(GL_TEXTURE_2D, g_gx_gmTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glBindTexture(GL_TEXTURE_2D, 0);

    /* No query for "which counts work": an unsupported one leaves the
     * framebuffer incomplete, so walk down 4 -> 2 like the canvas does. */
    for (s = want; s >= 2; s /= 2) {
        GLuint rbC = 0;
        glGenRenderbuffers(1, &rbC);
        glBindRenderbuffer(GL_RENDERBUFFER, rbC);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, (GLsizei)s, GL_RGBA8,
                                         (GLsizei)w, (GLsizei)h);
        if (glGetError() == GL_NO_ERROR && gxmesh_depth(w, h, s)) {
            glGenFramebuffers(1, &g_gx_gmFbo);
            glBindFramebuffer(GL_FRAMEBUFFER, g_gx_gmFbo);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                      GL_RENDERBUFFER, rbC);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                      GL_RENDERBUFFER, g_gx_gmRb);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
                g_gx_gmRbC = rbC;
                glGenFramebuffers(1, &g_gx_gmFboR);
                glBindFramebuffer(GL_FRAMEBUFFER, g_gx_gmFboR);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                       GL_TEXTURE_2D, g_gx_gmTex, 0);
                if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
                    glBindFramebuffer(GL_FRAMEBUFFER, 0);
                    glBindRenderbuffer(GL_RENDERBUFFER, 0);
                    g_gx_gmSamples = s; g_gx_gmW = w; g_gx_gmH = h;
                    return 1;
                }
            }
        }
        /* this count did not take: drop it and try half as many */
        if (rbC) glDeleteRenderbuffers(1, &rbC);
        if (g_gx_gmFboR) { glDeleteFramebuffers(1, &g_gx_gmFboR); g_gx_gmFboR = 0; }
        if (g_gx_gmFbo)  { glDeleteFramebuffers(1, &g_gx_gmFbo);  g_gx_gmFbo  = 0; }
        if (g_gx_gmRb)   { glDeleteRenderbuffers(1, &g_gx_gmRb);  g_gx_gmRb   = 0; }
    }

    /* MSAA off, or the driver refused every count: one FBO, texture drawn
     * straight into it. */
    if (!gxmesh_depth(w, h, 0)) { gxmesh_target_free(); return 0; }
    glGenFramebuffers(1, &g_gx_gmFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, g_gx_gmFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_gx_gmTex, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, g_gx_gmRb);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        gxmesh_target_free(); return 0;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    g_gx_gmSamples = 0; g_gx_gmW = w; g_gx_gmH = h;
    return 1;
}

/* ---- public -------------------------------------------------------- */

/* Upload static geometry.  verts is nv*8 floats, idx is ni indices.
 * Returns NULL if GL is not up.  Upload once, draw every frame. */
GX_INLINE GXMESH* gxcreatemesh(const float* verts, int nv,
                               const unsigned int* idx, int ni) {
    GXMESH* m;
    if (!gxmesh_setup() || nv <= 0 || ni <= 0 || !verts || !idx) return NULL;
    m = (GXMESH*)calloc(1, sizeof(GXMESH));
    if (!m) return NULL;
    m->nv = nv; m->ni = ni;

    if (glGenVertexArrays) {
        glGenVertexArrays(1, &m->vao);
        if (m->vao) glBindVertexArray(m->vao);
    }
    glGenBuffers(1, &m->vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
    glBufferData(GL_ARRAY_BUFFER, (GXGLsizeiptr)((size_t)nv * GXMESH_STRIDE * sizeof(float)),
                 verts, GL_STATIC_DRAW);
    glGenBuffers(1, &m->ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GXGLsizeiptr)((size_t)ni * sizeof(unsigned int)),
                 idx, GL_STATIC_DRAW);

    if (m->vao) {
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, GXMESH_STRIDE * sizeof(float), (const void*)0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, GXMESH_STRIDE * sizeof(float), (const void*)12);
        glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, GXMESH_STRIDE * sizeof(float), (const void*)20);
        glBindVertexArray(0);
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    return m;
}

GX_INLINE void gxfreemesh(GXMESH* m) {
    if (!m) return;
    if (m->vao) glDeleteVertexArrays(1, &m->vao);
    if (m->vbo) glDeleteBuffers(1, &m->vbo);
    if (m->ibo) glDeleteBuffers(1, &m->ibo);
    free(m);
}

/* Fog and eye.  Eye is only used to work out the fog distance. */
GX_INLINE void gxmeshfog(float nearD, float farD, int r, int g, int b) {
    g_gx_gmFogNear = nearD; g_gx_gmFogFar = (farD > nearD) ? farD : nearD + 0.001f;
    g_gx_gmFogCol[0] = (float)r / 255.f;
    g_gx_gmFogCol[1] = (float)g / 255.f;
    g_gx_gmFogCol[2] = (float)b / 255.f;
}
GX_INLINE void gxmesheye(float x, float y, float z) {
    g_gx_gmEye[0] = x; g_gx_gmEye[1] = y; g_gx_gmEye[2] = z;
}
/* Flip the finished image vertically if it comes out upside down. */
GX_INLINE void gxmeshflip(int on) { g_gx_gmFlip = (on != 0) ? -1 : 1; }

/* Column major 4x4 helpers, GL convention. */
GX_INLINE void gxmeshperspective(float out[16], float fovyRad, float aspect,
                                 float n, float f) {
    float t = 1.0f / tanf(fovyRad * 0.5f);
    memset(out, 0, sizeof(float) * 16);
    out[0] = t / aspect; out[5] = t;
    /* Column major: out[col*4+row].  Row 3 is (0,0,-1,0) so that
     * w_clip = -z_view; the 2fn/(n-f) term belongs in row 2 col 3.
     * The two were the wrong way round, which gave every vertex
     * w = 1 and put the whole scene outside the depth range. */
    out[10] = (f + n) / (n - f); out[11] = -1.0f;
    out[14] = (2.0f * f * n) / (n - f);
}
GX_INLINE void gxmeshlookat(float out[16],
                            float ex, float ey, float ez,
                            float cx, float cy, float cz,
                            float ux, float uy, float uz) {
    float fx = cx - ex, fy = cy - ey, fz = cz - ez;
    float rl = sqrtf(fx * fx + fy * fy + fz * fz);
    if (rl < 1e-6f) { fx = 0; fy = 0; fz = -1; rl = 1; }
    fx /= rl; fy /= rl; fz /= rl;
    float sx = fy * uz - fz * uy, sy = fz * ux - fx * uz, sz = fx * uy - fy * ux;
    rl = sqrtf(sx * sx + sy * sy + sz * sz);
    if (rl < 1e-6f) { sx = 1; sy = 0; sz = 0; rl = 1; }
    sx /= rl; sy /= rl; sz /= rl;
    float vx = sy * fz - sz * fy, vy = sz * fx - sx * fz, vz = sx * fy - sy * fx;
    out[0] = sx; out[1] = vx; out[2] = -fx; out[3] = 0;
    out[4] = sy; out[5] = vy; out[6] = -fy; out[7] = 0;
    out[8] = sz; out[9] = vz; out[10] = -fz; out[11] = 0;
    out[12] = -(sx * ex + sy * ey + sz * ez);
    out[13] = -(vx * ex + vy * ey + vz * ez);
    out[14] = (fx * ex + fy * ey + fz * ez);
    out[15] = 1;
}
GX_INLINE void gxmeshmatmul(float out[16], const float a[16], const float b[16]) {
    for (int c = 0; c < 4; c++)
        for (int r = 0; r < 4; r++) {
            float s = 0;
            for (int k = 0; k < 4; k++) s += a[k * 4 + r] * b[c * 4 + k];
            out[c * 4 + r] = s;
        }
}

/* ---- the 3D pass ---------------------------------------------------
 *
 * Three phases, because a world is more than one mesh:
 *
 *   meshbegin()  bind the offscreen target and clear it once
 *   meshdraw()   add geometry, as many calls as you like
 *   meshend()    composite the finished image onto the canvas once
 *
 * The earlier single-call form cleared and composited on every mesh, so a
 * chunked world could only ever show its last chunk.  The clear colour is
 * written with alpha 0 and the composite blends, so whatever was already
 * on the canvas -- a gradient sky drawn with ordinary 2D calls -- shows
 * through wherever no geometry was drawn.
 */
static void gxMeshPtrs(void) {
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, GXMESH_STRIDE * sizeof(float), (const void*)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, GXMESH_STRIDE * sizeof(float), (const void*)12);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, GXMESH_STRIDE * sizeof(float), (const void*)20);
}
/* Only needed where VAOs do not exist: there the pointers live in VAO 0,
 * which is the one easygl draws through. */
static void gxMeshPtrsRestore(void) {
    glBindBuffer(GL_ARRAY_BUFFER, g_gx_vbo);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vtx), (const void*)0);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Vtx), (const void*)8);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vtx), (const void*)24);
}

GX_INLINE bool gxmeshbegin(void) {
    if (!gxmesh_setup()) return false;
    if (!gxmesh_target(g_gx_canvasTarget.w, g_gx_canvasTarget.h)) return false;
    if (g_gx_gmIn) return true;
    g_gx_gmIn = 1;
    gxmesh_push(&g_gx_gmSave);
    if (g_gx_batchDraw) gxFlush();     /* never interleave with 2D commands */
    glBindFramebuffer(GL_FRAMEBUFFER, g_gx_gmFbo);
    glViewport(0, 0, g_gx_gmW, g_gx_gmH);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);             /* backfaces already removed */
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glClearColor(g_gx_gmFogCol[0], g_gx_gmFogCol[1], g_gx_gmFogCol[2], 0.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glUseProgram(g_gx_gmProg);
    if (g_gx_gmUflip >= 0)    glUniform1f(g_gx_gmUflip, (g_gx_gmFlip == 0) ? 1.0f : (float)g_gx_gmFlip);
    if (g_gx_gmUeye >= 0)     glUniform3f(g_gx_gmUeye, g_gx_gmEye[0], g_gx_gmEye[1], g_gx_gmEye[2]);
    if (g_gx_gmUfog >= 0)     glUniform2f(g_gx_gmUfog, g_gx_gmFogNear, g_gx_gmFogFar);
    if (g_gx_gmUfogCol >= 0)  glUniform3f(g_gx_gmUfogCol, g_gx_gmFogCol[0], g_gx_gmFogCol[1], g_gx_gmFogCol[2]);
    if (g_gx_gmUtex >= 0)     glUniform1i(g_gx_gmUtex, 0);
    return true;
}

GX_INLINE void gxmeshdraw(GXMESH* m, const float* mvp16, IMAGE* tex) {
    if (!g_gx_gmIn || !g_gx_gmOk || !m || !mvp16) return;
    glBindFramebuffer(GL_FRAMEBUFFER, g_gx_gmFbo);
    glViewport(0, 0, g_gx_gmW, g_gx_gmH);
    glUseProgram(g_gx_gmProg);
    if (g_gx_gmUmvp >= 0) glUniformMatrix4fv(g_gx_gmUmvp, 1, GL_FALSE, mvp16);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, (tex && gxImageOk(tex)) ? tex->tex : 0);
    if (g_gx_gmUuseTex >= 0) glUniform1i(g_gx_gmUuseTex, (tex && gxImageOk(tex)) ? 1 : 0);

    if (m->vao) glBindVertexArray(m->vao);
    else { glBindBuffer(GL_ARRAY_BUFFER, m->vbo); gxMeshPtrs(); }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->ibo);
    glDrawElements(GL_TRIANGLES, (GLsizei)m->ni, GL_UNSIGNED_INT, (const void*)0);
    if (m->vao) glBindVertexArray(0);
}

GX_INLINE void gxmeshend(void) {
    if (!g_gx_gmIn) return;
    /* Fold the samples into g_gx_gmTex before it is used as a texture.
     * Averaging is what the resolve is for: an edge pixel that was half
     * covered comes out half transparent, which is exactly what the
     * composite below wants. */
    if (g_gx_gmFboR) {
        glDisable(GL_SCISSOR_TEST);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, g_gx_gmFbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_gx_gmFboR);
        glBlitFramebuffer(0, 0, g_gx_gmW, g_gx_gmH, 0, 0, g_gx_gmW, g_gx_gmH,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, g_gx_canvasTarget.fbo);
    glViewport(0, 0, g_gx_canvasTarget.w, g_gx_canvasTarget.h);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(g_gx_gmProg);
    {
        static const float ident[16] = { 1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1 };
        if (g_gx_gmUmvp >= 0) glUniformMatrix4fv(g_gx_gmUmvp, 1, GL_FALSE, ident);
    }
    if (g_gx_gmUflip >= 0)   glUniform1f(g_gx_gmUflip, 1.0f);
    if (g_gx_gmUuseTex >= 0) glUniform1i(g_gx_gmUuseTex, 1);
    /* Fog must not tint the composite: push the range out of reach. */
    if (g_gx_gmUfog >= 0)    glUniform2f(g_gx_gmUfog, 1.0e30f, 2.0e30f);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_gx_gmTex);
    if (g_gx_gmQuadVao) {
        glBindVertexArray(g_gx_gmQuadVao);
    } else {
        glBindBuffer(GL_ARRAY_BUFFER, g_gx_gmQuad);
        gxMeshPtrs();
    }
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    if (g_gx_gmQuadVao) glBindVertexArray(0);
    else gxMeshPtrsRestore();
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    gxmesh_pop(&g_gx_gmSave);
    g_gx_gmIn = 0;
}

/* Re-upload the contents of an existing mesh.  Orphaning first lets the
 * driver hand back a new block instead of stalling on the old one, which
 * is what a chunk rebuild wants: the shape changes, the object does not. */
GX_INLINE bool gxmeshupdate(GXMESH* m, const float* verts, int nv,
                            const unsigned int* idx, int ni) {
    if (!m || nv <= 0 || ni <= 0 || !verts || !idx) return false;
    if (m->vao) glBindVertexArray(m->vao);
    glBindBuffer(GL_ARRAY_BUFFER, m->vbo);
    glBufferData(GL_ARRAY_BUFFER, 0, NULL, GL_STATIC_DRAW);
    glBufferData(GL_ARRAY_BUFFER, (GXGLsizeiptr)((size_t)nv * GXMESH_STRIDE * sizeof(float)),
                 verts, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m->ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 0, NULL, GL_STATIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GXGLsizeiptr)((size_t)ni * sizeof(unsigned int)),
                 idx, GL_STATIC_DRAW);
    if (m->vao) glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    m->nv = nv; m->ni = ni;
    return true;
}

/* Gribb-Hartmann: the six frustum planes of a column major MVP, laid out
 * as 6 * (a,b,c,d).  Row r of the matrix is m[r], m[4+r], m[8+r], m[12+r]. */
GX_INLINE void gxmeshfrustum(const float m[16], float pl[24]) {
    static const int s[6][2] = { {1,0}, {-1,0}, {1,1}, {-1,1}, {1,2}, {-1,2} };
    for (int i = 0; i < 6; i++) {
        int sg = s[i][0], k = s[i][1];
        pl[i*4+0] = m[0*4+3] + (float)sg * m[0*4+k];
        pl[i*4+1] = m[1*4+3] + (float)sg * m[1*4+k];
        pl[i*4+2] = m[2*4+3] + (float)sg * m[2*4+k];
        pl[i*4+3] = m[3*4+3] + (float)sg * m[3*4+k];
    }
}
GX_INLINE bool gxmeshaabbinfrustum(const float pl[24],
                                   float mnx, float mny, float mnz,
                                   float mxx, float mxy, float mxz) {
    for (int i = 0; i < 6; i++) {
        float a = pl[i*4+0], b = pl[i*4+1], c = pl[i*4+2], d = pl[i*4+3];
        float px = (a >= 0) ? mxx : mnx;
        float py = (b >= 0) ? mxy : mny;
        float pz = (c >= 0) ? mxz : mnz;
        if (a * px + b * py + c * pz + d < 0.0f) return false;
    }
    return true;
}


/*======================================================================
 * 20. Public 3D mesh interface
 *====================================================================
 * The names above are gx* internals.  These are the ones a program
 * spells.  See section 19 for what the pass does and why.
 *
 *   createmesh(verts, nv, idx, ni)   upload geometry ONCE
 *   meshbegin() / meshdraw() / meshend()   one pass, many meshes
 *   meshvisible(mvp, box)            per chunk frustum test
 *
 * verts is nv * 8 floats:  x, y, z, u, v, r, g, b
 *   r/g/b are a 0..1 multiplier on the texture, so per face lighting is
 *   free.  idx is ni unsigned ints, three per triangle.
 * mvp is 16 floats, COLUMN MAJOR (OpenGL order).  meshperspective(),
 * meshlookat() and meshmatmul() all speak that order, so a program never
 * has to transpose anything by hand.
 */
GX_INLINE GXMESH* createmesh(const float* verts, int nv,
                             const unsigned int* idx, int ni) {
    return gxcreatemesh(verts, nv, idx, ni);
}
GX_INLINE bool updatemesh(GXMESH* m, const float* verts, int nv,
                          const unsigned int* idx, int ni) {
    return gxmeshupdate(m, verts, nv, idx, ni);
}
GX_INLINE void freemesh(GXMESH* m) { gxfreemesh(m); }

GX_INLINE bool meshavailable(void) { return gxmesh_setup() ? true : false; }
GX_INLINE bool meshbegin(void)     { return gxmeshbegin(); }
GX_INLINE void meshdraw(GXMESH* m, const float* mvp16, IMAGE* tex) {
    gxmeshdraw(m, mvp16, tex);
}
GX_INLINE void meshend(void) { gxmeshend(); }

GX_INLINE void meshfog(float nearD, float farD, COLORREF col) {
    gxmeshfog(nearD, farD, (int)((col) & 255), (int)(((col) >> 8) & 255),
              (int)(((col) >> 16) & 255));
}
GX_INLINE void mesheye(float x, float y, float z) { gxmesheye(x, y, z); }
GX_INLINE void meshflip(bool on) { gxmeshflip(on ? 1 : 0); }
/* Samples the 3D target was actually built with: 0 when MSAA is off or the
 * driver refused it.  Can be lower than getaasamples() because the target
 * walks 4 -> 2 the same way the canvas does. */
GX_INLINE int getmeshsamples(void) { return g_gx_gmSamples; }

GX_INLINE void meshperspective(float out[16], float fovyRad, float aspect,
                               float nearZ, float farZ) {
    gxmeshperspective(out, fovyRad, aspect, nearZ, farZ);
}
GX_INLINE void meshlookat(float out[16],
                          float ex, float ey, float ez,
                          float cx, float cy, float cz,
                          float ux, float uy, float uz) {
    gxmeshlookat(out, ex, ey, ez, cx, cy, cz, ux, uy, uz);
}
GX_INLINE void meshmatmul(float out[16], const float a[16], const float b[16]) {
    gxmeshmatmul(out, a, b);
}
/* True when the axis aligned box is at least partly inside the frustum of
 * a column major MVP.  Cheap enough to call per chunk per frame. */
GX_INLINE bool meshvisible(const float mvp16[16],
                           float mnx, float mny, float mnz,
                           float mxx, float mxy, float mxz) {
    float pl[24];
    gxmeshfrustum(mvp16, pl);
    return gxmeshaabbinfrustum(pl, mnx, mny, mnz, mxx, mxy, mxz);
}

/*------------------- canvas and GL state introspection ----------------*/
/* The canvas is sized in device pixels, which is not what getwidth() and
 * getheight() report when DPI scaling or a non default aspect ratio is in
 * effect.  Anything that wants to blit its own rendered frame into the canvas
 * needs the real numbers, and it cannot reach g_gx_canvasTarget - that symbol
 * is static to this header, which is exactly the point of naming it that way.
 * Hence these four.  All of them are safe to call before initgraph().        */
GX_INLINE bool         isglready(void)        { return g_gx_glReady != 0; }
GX_INLINE int          getcanvaswidth(void)   { return g_gx_canvasTarget.w; }
GX_INLINE int          getcanvasheight(void)  { return g_gx_canvasTarget.h; }
GX_INLINE unsigned int getcanvastex(void)     { return (unsigned int)g_gx_canvasTarget.tex; }
GX_INLINE unsigned int getcanvasfbo(void)     { return (unsigned int)g_gx_canvasTarget.fbo; }

#endif /* EASYGL_H */

