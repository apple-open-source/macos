/*
    File:        MBCBoardMTLViewMouse.mm
    Contains:    Handle mouse coordinate transformations
    Copyright:    © 2002-2012 by Apple Inc., all rights reserved.

    IMPORTANT: This Apple software is supplied to you by Apple Computer,
    Inc.  ("Apple") in consideration of your agreement to the following
    terms, and your use, installation, modification or redistribution of
    this Apple software constitutes acceptance of these terms.  If you do
    not agree with these terms, please do not use, install, modify or
    redistribute this Apple software.
    
    In consideration of your agreement to abide by the following terms,
    and subject to these terms, Apple grants you a personal, non-exclusive
    license, under Apple's copyrights in this original Apple software (the
    "Apple Software"), to use, reproduce, modify and redistribute the
    Apple Software, with or without modifications, in source and/or binary
    forms; provided that if you redistribute the Apple Software in its
    entirety and without modifications, you must retain this notice and
    the following text and disclaimers in all such redistributions of the
    Apple Software.  Neither the name, trademarks, service marks or logos
    of Apple Inc. may be used to endorse or promote products
    derived from the Apple Software without specific prior written
    permission from Apple.  Except as expressly stated in this notice, no
    other rights or licenses, express or implied, are granted by Apple
    herein, including but not limited to any patent rights that may be
    infringed by your derivative works or by other works in which the
    Apple Software may be incorporated.
    
    The Apple Software is provided by Apple on an "AS IS" basis.  APPLE
    MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION
    THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND
    FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS
    USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.
    
    IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT,
    INCIDENTAL OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
    PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
    PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE,
    REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE,
    HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING
    NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN
    ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#import "MBCBoardMTLViewMouse.h"
#import "MBCBoardWin.h"
#import "MBCController.h"
#import "MBCDebug.h"
#import "MBCInteractivePlayer.h"
#import "MBCMetalCamera.h"
#import "MBCMetalRenderer.h"

#import <algorithm>

using std::min;
using std::max;

@implementation MBCBoardMTLView (Mouse)

#define MOVE_BOARD_ELEVATION_ROTATION_SPEED 16.f
#define MOVE_BOARD_AZIMUTH_ROTATION_SPEED   16.f
#define MOVE_BOARD_AZIMUTH_SNAP_ROUND       2.0f
#define MOVE_BOARD_DRAG_THRESHOLD           0.0f

#pragma mark - Mouse Event Processing

- (NSRect)approximateBoundsOfSquare:(MBCSquare)square {
    // kBoardRadius = 40 -> 40 / 8 = 5
    const float kSquare = 4.5f;

    MBCMetalCamera *camera = self.renderer.camera;
    
    // Project square position from model space to screen space
    MBCPosition pos = [self squareToPosition:square];

    // Four points of the square to screen (p0, p1, p2, p3)
    pos[0] -= kSquare;
    pos[2] -= kSquare;
    NSPoint p0 = [camera projectPositionFromModelToScreen:pos];
    
    pos[0] += 2.0f * kSquare;
    NSPoint p1 = [camera projectPositionFromModelToScreen:pos];
    
    pos[2] += 2.0f * kSquare;
    NSPoint p2 = [camera projectPositionFromModelToScreen:pos];

    pos[0] -= 2.0f * kSquare;
    NSPoint p3 = [camera projectPositionFromModelToScreen:pos];

    NSRect r;
    if (p1.x > p0.x) {
        r.origin.x = max(p0.x, p3.x);
        r.size.width = min(p1.x, p2.x) - r.origin.x;
    } else {
        r.origin.x = max(p1.x, p2.x);
        r.size.width = min(p0.x, p3.x) - r.origin.x;
    }
    if (p2.y > p1.y) {
        r.origin.y = max(p0.y, p1.y);
        r.size.height = min(p2.y, p3.y) - r.origin.y;
    } else {
        r.origin.y = max(p2.y, p3.y);
        r.size.height = min(p0.y, p1.y) - r.origin.y;
    }

    return [self convertRectFromBacking:r];
}

- (MBCPosition)mouseToPosition:(NSPoint)mouse {
    if (MBCDebug::LogMouse()) {
        fprintf(stderr, "[%.0f,%.0f] ", mouse.x, mouse.y);
    }
    
    mouse = [self convertPointToBacking:mouse];
    MBCMetalCamera *camera = self.renderer.camera;
    vector_float2 screenPos = {static_cast<float>(mouse.x), static_cast<float>(mouse.y)};
    
    return [camera unProjectPositionFromScreenToModel:screenPos fromView:self];
}

- (MBCPosition)mouseToPositionIgnoringY:(NSPoint)mouse {
    mouse = [self convertPointToBacking:mouse];
    MBCMetalCamera *camera = self.renderer.camera;
    vector_float2 screenPosition = { static_cast<float>(mouse.x), static_cast<float>(mouse.y) };
    
    return [camera unProjectPositionFromScreenToModel:screenPosition knownY:0.f];
}

- (MBCPosition)eventToPosition:(NSEvent *)event {
    NSPoint p = [event locationInWindow];
    NSPoint l = [self convertPoint:p fromView:nil];

    return [self mouseToPosition:l];
}

- (void)mouseEntered:(NSEvent *)theEvent {
    if (MBCDebug::LogMouse()) {
        fprintf(stderr, "mouseEntered\n");
    }
    
    [[self window] setAcceptsMouseMovedEvents:YES];
    [[self window] makeFirstResponder:self];
}

- (void)mouseExited:(NSEvent *)theEvent {
    if (MBCDebug::LogMouse()) {
        fprintf(stderr, "mouseExited\n");
    }
    
    [[self window] setAcceptsMouseMovedEvents:NO];
}

- (void)updateMouseCursorForEvent:(NSEvent *)event {
    MBCPosition pos = [self eventToPosition:event];
    float pxa = fabs(pos[0]);
    float pza = fabs(pos[2]);
    NSCursor *cursor = _arrowCursor;

    if (_inBoardManipulation) {
        cursor = _grabbingHandCursor;
    } else if (pxa > kBoardRadius || pza > kBoardRadius) {
        if (pxa < kBoardRadius + kBorderWidthMTL + 0.1f && pza < kBoardRadius + kBorderWidthMTL + 0.1f) {
            // On the boarder, show the hand cursor
            cursor = _pointingHandCursor;
        }
    }
    [cursor set];
}

- (void)mouseMoved:(NSEvent *)event {
    if (MBCDebug::LogMouse()) {
        fprintf(stderr, "mouseMoved\n");
    }
    
    [self updateMouseCursorForEvent:event];
}

- (void)mouseDown:(NSEvent *)event {
    if (MBCDebug::LogMouse()) {
        fprintf(stderr, "mouseDown\n");
    }
    
    MBCSquare previouslyPicked = _pickedSquare;

    NSPoint mousePosition = [event locationInWindow];
    NSPoint mouseWindowPosition = [self convertPoint:mousePosition fromView:nil];
    //
    // On mousedown, determine the point on the board surface that
    // corresponds to the mouse location by the frontmost Z value, but
    // then pretend that the click happened at board surface level. Weirdly
    // enough, this seems to give the most natural feeling mouse behavior.
    //
    
    // Convert mouse position from screen space to model space
    MBCPosition pos = [self mouseToPosition:mouseWindowPosition];
    _selectedDestination = [self positionToSquareOrRegion:&pos];
    
    switch (_selectedDestination) {
        case kInvalidSquare:
            return;
        case kWhitePromoSquare:
        case kBlackPromoSquare:
            return;
        case kBorderRegion:
            _inBoardManipulation = YES;
            _previousMousePosition = mouseWindowPosition;
            _currentMousePosition = mouseWindowPosition;
            _rawAzimuth = self.azimuth;
            [self updateMouseCursorForEvent:event];

            // Process mouse events at specified interval
            [NSEvent startPeriodicEventsAfterDelay:0.008f withPeriod:0.008f];
            break;
        default:
            if (!_wantMouse || _inAnimation || pos[1] < 0.1) {
                return;
            }
            if (_selectedDestination == _pickedSquare) {
                //
                // When trying to move a large piece by clicking the destination, the piece
                // sometimes can hide the destination. We try again by ignoring y.
                //
                MBCPosition altPos = [self mouseToPositionIgnoringY:mouseWindowPosition];
                MBCSquare altDest = [self positionToSquareOrRegion:&altPos];
                if (altDest < kSyntheticSquare) {
                    pos = altPos;
                    _selectedDestination = altDest;
                }
            }
            
            //
            // Let interactive player decide whether we hit one of their pieces
            //
            [_interactive startSelection:_selectedDestination];
            if (!_selectedPiece) { // Apparently not...
                return;
            }
            break;
    }
    
    pos[1] = 0.0f;
    gettimeofday(&_lastMouseDragUpdate, NULL);
    _lastSelectedPosition = pos;
    
    [self drawNow];
    
    NSDate * whenever = [NSDate distantFuture];
    for (bool goOn = true; goOn; ) {
        event = [NSApp nextEventMatchingMask: NSEventMaskPeriodic | NSEventMaskLeftMouseUp | NSEventMaskLeftMouseDragged
                                   untilDate:whenever
                                      inMode:NSEventTrackingRunLoopMode
                                     dequeue:YES];
        
        switch ([event type]) {
            case NSEventTypePeriodic:
            case NSEventTypeLeftMouseDragged:
                // update camera and redraw if needed
                [self dragAndRedraw:event forceRedraw:NO];
                break;
            case NSEventTypeLeftMouseUp: {
                // update camera and redraw if needed
                [self dragAndRedraw:event forceRedraw:YES];
                [_controller setAngle:self.elevation spin:self.azimuth];
                [_interactive endSelection:_selectedDestination animate:NO];
                if (_pickedSquare == previouslyPicked) {
                    _pickedSquare = kInvalidSquare; // Toggle pick
                }
                goOn = false;
                if (_inBoardManipulation) {
                    _inBoardManipulation = NO;
                    [self updateMouseCursorForEvent:event];
                    [NSEvent stopPeriodicEvents];
                }
                break;
            }
            default:
                /* Ignore any other kind of event. */
                break;
        }
    }
    _selectedDestination = kInvalidSquare;
}

- (void)mouseUp:(NSEvent *)event {
    if (MBCDebug::LogMouse()) {
        fprintf(stderr, "mouseUp\n");
    }
    
    if (!_wantMouse || _inAnimation) {
        return;
    }

    MBCPiece promo;
    if (_selectedDestination == kWhitePromoSquare) {
        promo = [_board defaultPromotion:YES];
    } else if (_selectedDestination == kBlackPromoSquare) {
        promo = [_board defaultPromotion:NO];
    } else if (_pickedSquare != kInvalidSquare) {
        [_interactive startSelection:_pickedSquare];
        [_interactive endSelection:_selectedDestination animate:YES];
        return;
    } else {
        return;
    }
    
    switch (promo) {
        case QUEEN:
            if (_variant == kVarSuicide)
                promo = KING;    // King promotion is very popular in suicide
            else
                promo = KNIGHT; // Second most useful
            break;
        case KING: // Suicide only
            promo = KNIGHT;
            break;
        case KNIGHT:
            promo = ROOK;
            break;
        case ROOK:
            promo = BISHOP;
            break;
        case BISHOP:
            promo = QUEEN;
            break;
    }
    
    BOOL selectedWhite = (_selectedDestination == kWhitePromoSquare);
    [_board setDefaultPromotion:promo
                            for:selectedWhite];

    [self needsUpdate];
}

- (void)dragAndRedraw:(NSEvent *)event forceRedraw:(BOOL)force {
    if ([event type] != NSEventTypePeriodic) {
        // Not a periodic event.  Periodic events occur while mouse is down.
        NSPoint mousePosition = [event locationInWindow];
        NSPoint mouseWindowPosition = [self convertPoint:mousePosition fromView:nil];
        _currentMousePosition = mouseWindowPosition;
        
        if (!_inAnimation) {
            //
            // On drag, we can use a fairly fast interpolation to determine
            // the 3D coordinate using the y where we touched the piece
            //
            mouseWindowPosition = [self convertPointToBacking:mouseWindowPosition];
            MBCMetalCamera *camera = self.renderer.camera;
            vector_float2 screenPos = {static_cast<float>(mouseWindowPosition.x), static_cast<float>(mouseWindowPosition.y)};
            
            _selectedPosition = [camera unProjectPositionFromScreenToModel:screenPos knownY:0.f];
            
            [self snapToSquare:&_selectedPosition];
        }
    }
    struct timeval now;
    gettimeofday(&now, NULL);
    NSTimeInterval dt = now.tv_sec - _lastMouseDragUpdate.tv_sec + 0.000001 * (now.tv_usec - _lastMouseDragUpdate.tv_usec);
    
    if (force) {
        [self needsUpdate];
    } else if (_selectedDestination == kBorderRegion) {
        // Update azimuth and elevation if changed, which will also update camera's view matrix.
        float dx = _currentMousePosition.x - _previousMousePosition.x;
        float dy = _currentMousePosition.y - _previousMousePosition.y;
#if FULL_DIAGONAL_MOVES
        BOOL mustDraw = NO;
        if (fabs(dx) > MOVE_BOARD_DRAG_THRESHOLD) {
            _rawAzimuth += dx * dt * MOVE_BOARD_AZIMUTH_ROTATION_SPEED;
            _rawAzimuth = fmod(_rawAzimuth + 360.0f, 360.0f);
            
            float azimuth = _rawAzimuth;
            float angle = fmod((azimuth = _rawAzimuth), 90.0f);
            if (angle < MOVE_BOARD_AZIMUTH_SNAP_ROUND) {
                azimuth -= angle;
            } else if (angle > 90.0f - MOVE_BOARD_AZIMUTH_SNAP_ROUND) {
                azimuth += 90.0f - angle;
            }
            self.azimuth = azimuth;
            mustDraw = YES;
        }
        if (fabs(dy) > MOVE_BOARD_DRAG_THRESHOLD) {
            float elevation = self.elevation;
            elevation -= dy * dt * MOVE_BOARD_ELEVATION_ROTATION_SPEED;
            elevation = max(kMinElevation, min(kMaxElevation, elevation));
            self.elevation = elevation;
            mustDraw = YES;
        }
        if (mustDraw) {
            _lastMouseDragUpdate = now;
            [self drawNow];
        }
#else
        if (fabs(dx) > fabs(dy) && fabs(dx) > MOVE_BOARD_DRAG_THRESHOLD) {
            // Dragging horizontally
            _rawAzimuth += dx * dt * MOVE_BOARD_AZIMUTH_ROTATION_SPEED;
            _rawAzimuth = fmod(_rawAzimuth + 360.0f, 360.0f);
            
            float azimuth = _rawAzimuth;
            float angle = fmod(azimuth, 90.0f);
            if (angle < MOVE_BOARD_AZIMUTH_SNAP_ROUND) {
                azimuth -= angle;
            } else if (angle > 90.0f - MOVE_BOARD_AZIMUTH_SNAP_ROUND) {
                azimuth += 90.0f - angle;
            }
            self.azimuth = azimuth;
            _lastMouseDragUpdate = now;
            [self drawNow];
            _previousMousePosition = _currentMousePosition;
        } else if (fabs(dy) > MOVE_BOARD_DRAG_THRESHOLD) {
            // Dragging vertically
            float elevation = self.elevation;
            elevation -= dy *dt * MOVE_BOARD_ELEVATION_ROTATION_SPEED;
            elevation = max(kMinElevation, min(kMaxElevation, elevation));
            self.elevation = elevation;
            _lastMouseDragUpdate  = now;
            [self drawNow];
            _previousMousePosition = _currentMousePosition;
        }
#endif
    } else {
        MBCPosition delta = _selectedPosition - _lastSelectedPosition;
        float d2 = delta[0] * delta[0] + delta[2] * delta[2];

        if (d2 > 25.0f || (d2 > 1.0f && dt > 0.02)) {
            _selectedDestination = [self positionToSquare:&_selectedPosition];
            _lastMouseDragUpdate = now;
            [self drawNow];
        }
    }
}

#pragma mark - Key Event Processing
- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void)keyDown:(NSEvent *)event {
    NSString * chr = [event characters];
    if ([chr length] != 1) {
        return; // Ignore
    }
    
    switch (char ch = [chr characterAtIndex:0]) {
        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
        case 'G':
        case 'H':
            ch = tolower(ch);
            // Fall through
        case 'b':
        case 'a':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
        case 'g':
        case 'h':
        case '=':
            if (ch == 'b' && _keyBuffer == '=') {
                goto promotion_piece;
            }
            if (_wantMouse) {
                _keyBuffer = ch;
            } else {
                NSBeep();
            }
            break;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
            if (_wantMouse && isalpha(_keyBuffer)) {
                MBCSquare sq = Square(_keyBuffer, ch - '0');
                if (_pickedSquare != kInvalidSquare) {
                    [_interactive startSelection:_pickedSquare];
                    [_interactive endSelection:sq animate:YES];
                } else {
                    [_interactive startSelection:sq];
                    [self clickPiece];
                }
            } else
                NSBeep();
            _keyBuffer = 0;
            break;
        case '\177':    // Delete
        case '\r':
            if (_keyBuffer) {
                _keyBuffer = 0;
            } else if (_pickedSquare != kInvalidSquare) {
                [_interactive endSelection:_pickedSquare animate:NO];
                _pickedSquare = kInvalidSquare;
                [self needsUpdate];
            }
            break;
        case 'K':
            if (_variant != kVarSuicide) {
                NSBeep();
                break;
            }
            // Fall through
        case 'Q':
        case 'N':
        case 'R':
            ch = tolower(ch);
            // Fall through
        case 'k':
            if (_variant != kVarSuicide) {
                NSBeep();
                break;
            }
            // Fall through
        case 'q':
        case 'n':
        case 'r':
        promotion_piece:
            if (_keyBuffer == '=') {
                const char * kPiece = " kqbnr";
                [_board setDefaultPromotion:strchr(kPiece, ch)-kPiece for:YES];
                [_board setDefaultPromotion:strchr(kPiece, ch)-kPiece for:NO];
                [self needsUpdate];
            } else {
                NSBeep();
            }
            _keyBuffer = 0;
            break;
        default:
            //
            // Propagate ESC etc.
            //
            [super keyDown:event];
            break;
    }
}

@end
