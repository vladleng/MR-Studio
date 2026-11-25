#!/usr/bin/env python3
"""MR Live track instrument icon set — 30 original 24 px glyphs (MR UDS line style).

Additive pack; does not rebuild the base kit or the fader/control concept pack.
Owns only:
  svg/icons/live_track/*.svg
  catalogs/mr_live_track_instruments_v1.json
  previews/MR_Live_Track_Icons_Overview.svg
  previews/live_track_icons.html
plus its own entries in manifest.json (other entries preserved verbatim).

Style: one 24x24 grid, transparent background, 20x20 optical field, silhouette
stroke 1.6, secondary detail 1.3, round caps, single neutral colour (#DAE5F4)
so the host can tint normal/selected/disabled. Original vectors only; no fonts,
rasters, filters, scripts, external references or third-party artwork.

Run from any directory:  python assets/ui/mr-uds/build_live_track_icons.py
"""
from pathlib import Path
import html
import json
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent
PACK = 'mr_live_track_instruments_v1'
OUT_SVG = ROOT / 'svg' / 'icons' / 'live_track'
SOURCE = 'Original MR Studio vector, MR Live track instrument icon set (2026-11-17)'
LICENSE = 'Same as project; original artwork, no third-party material'

LINE = '#DAE5F4'
SIL = 1.6
DET = 1.3

# Preview palette (MR UDS reference tokens)
BG = '#111720'
WINDOW = '#171E28'
SURFACE = '#192333'
RAISED = '#222C3C'
EDGE = '#2D394E'
BRIGHT = '#DAE5F4'
STROKE = '#A9BFDC'
MUTED = '#7188AB'
ACCENT = '#91BBEE'
DISABLED = '#5A6E8C'


def path(d, sw=SIL, color=LINE):
    return f'<path d="{d}" fill="none" stroke="{color}" stroke-width="{sw}" ' \
           'stroke-linecap="round" stroke-linejoin="round"/>'


def circle(cx, cy, r, sw=DET, color=LINE):
    return f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="none" stroke="{color}" ' \
           f'stroke-width="{sw}"/>'


def ellipse(cx, cy, rx, ry, sw=DET, color=LINE):
    return f'<ellipse cx="{cx}" cy="{cy}" rx="{rx}" ry="{ry}" fill="none" stroke="{color}" ' \
           f'stroke-width="{sw}"/>'


def rect(x, y, w, h, rx=0, sw=DET, color=LINE):
    return f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="none" ' \
           f'stroke="{color}" stroke-width="{sw}"/>'


def line(x1, y1, x2, y2, sw=DET, color=LINE):
    return path(f'M{x1} {y1} L{x2} {y2}', sw, color)


def group(transform, children):
    return [f'<g transform="{transform}">'] + list(children) + ['</g>']


# --------------------------------------------------------------------------
# Geometry. One dominant silhouette plus a few identifying details, inside the
# 2..22 optical field. Guitars are drawn upright and rotated so the body stays
# a readable mass instead of a squeezed column.
# --------------------------------------------------------------------------

def vocal_mic():
    return [
        circle(12, 7.4, 4.3, SIL),
        line(8.1, 5.9, 15.9, 5.9, DET),
        line(10.5, 4.1, 10.5, 8.7, DET),
        line(13.5, 4.1, 13.5, 8.7, DET),
        path('M9.9 11.4 L10.8 20.5 H13.2 L14.1 11.4', SIL),
        line(10.5, 14.9, 13.5, 14.9, DET),
    ]


def backing_vocals():
    return [
        circle(7.4, 8.0, 3.1, SIL),
        path('M6.1 10.3 L6.7 19.6 H8.1 L8.7 10.3', SIL),
        line(6.2, 6.9, 8.6, 6.9, DET),
        circle(16.6, 8.0, 3.1, SIL),
        path('M15.3 10.3 L15.9 19.6 H17.3 L17.9 10.3', SIL),
        line(15.4, 6.9, 17.8, 6.9, DET),
    ]


def choir():
    return [
        circle(6.2, 8.4, 1.9, SIL),
        circle(12, 7.4, 2.1, SIL),
        circle(17.8, 8.4, 1.9, SIL),
        path('M3.4 19.8 C3.4 15.4 4.6 13.6 6.2 13.6 C7.8 13.6 9.0 15.4 9.0 19.8', SIL),
        path('M9.4 19.8 C9.4 14.6 10.6 12.4 12 12.4 C13.4 12.4 14.6 14.6 14.6 19.8', SIL),
        path('M15.0 19.8 C15.0 15.4 16.2 13.6 17.8 13.6 C19.4 13.6 20.6 15.4 20.6 19.8', SIL),
    ]


def electric_guitar():
    body = [
        path('M10 11.8 C12.6 11.8 14.5 13.1 14.5 15.2 C14.5 16.5 13.8 17.2 14.0 18.3 '
             'C14.2 19.8 13.1 20.8 11.6 20.8 L8.4 20.8 C6.9 20.8 5.8 19.8 6.0 18.3 '
             'C6.2 17.2 5.5 16.5 5.5 15.2 C5.5 13.1 7.4 11.8 10 11.8 Z', SIL),
        line(9.0, 12.0, 9.0, 5.6, SIL),
        line(11.0, 12.0, 11.0, 5.6, SIL),
        path('M8.5 5.6 H11.5 V2.9 H8.5 Z', SIL),
        line(9.0, 7.8, 11.0, 7.8, DET),
        line(9.0, 9.4, 11.0, 9.4, DET),
        rect(7.3, 14.4, 5.4, 1.3, 0.6, DET),
        rect(7.6, 16.3, 4.8, 1.2, 0.6, DET),
        rect(8.2, 18.3, 3.6, 1.0, 0.5, DET),
        circle(12.9, 18.9, 0.6, DET),
    ]
    return group('translate(4.6 -0.5) rotate(-38 10 16.4)', body)


def acoustic_guitar():
    body = [
        path('M10 11.6 C7.9 11.6 6.5 12.6 6.5 13.9 C6.5 14.9 7.4 15.3 7.4 15.9 '
             'C7.4 16.6 5.8 17.0 5.8 18.4 C5.8 20.0 7.6 21.0 10 21.0 C12.4 21.0 '
             '14.2 20.0 14.2 18.4 C14.2 17.0 12.6 16.6 12.6 15.9 C12.6 15.3 13.5 14.9 '
             '13.5 13.9 C13.5 12.6 12.1 11.6 10 11.6 Z', SIL),
        circle(10, 16.9, 1.5, SIL),
        line(8.6, 19.4, 11.4, 19.4, DET),
        line(9.0, 12.0, 9.0, 4.9, SIL),
        line(11.0, 12.0, 11.0, 4.9, SIL),
        path('M8.6 4.9 H11.4 V3.0 H8.6 Z', SIL),
        line(7.8, 3.6, 8.6, 3.6, DET),
        line(11.4, 3.6, 12.2, 3.6, DET),
        line(9.0, 7.6, 11.0, 7.6, DET),
        line(9.0, 9.4, 11.0, 9.4, DET),
    ]
    return group('translate(3.6 0.2) rotate(-34 10 16.2)', body)


def bass_guitar():
    body = [
        path('M10 11.4 C13.0 11.4 15.0 12.9 14.9 15.1 C14.8 16.6 14.0 17.4 14.1 18.6 '
             'C14.2 20.1 13.0 21.0 11.2 21.0 L8.8 21.0 C7.0 21.0 5.8 20.1 5.9 18.6 '
             'C6.0 17.4 5.2 16.6 5.1 15.1 C5.0 12.9 7.0 11.4 10 11.4 Z', SIL),
        line(9.1, 11.8, 9.1, 3.8, SIL),
        line(10.9, 11.8, 10.9, 3.8, SIL),
        path('M8.7 3.8 L8.3 1.9 H11.7 L11.3 3.8', SIL),
        line(7.9, 2.8, 8.6, 2.8, DET),
        line(7.9, 3.5, 8.6, 3.5, DET),
        line(11.4, 2.8, 12.1, 2.8, DET),
        line(11.4, 3.5, 12.1, 3.5, DET),
        rect(7.4, 14.6, 5.2, 1.2, 0.6, DET),
        rect(7.6, 16.5, 4.8, 1.1, 0.55, DET),
        line(8.8, 18.6, 11.2, 18.6, DET),
    ]
    return group('translate(4.4 0.4) rotate(-30 10 16)', body)


def banjo():
    return [
        circle(8.8, 16.8, 4.7, SIL),
        circle(8.8, 16.8, 3.5, DET),
        path('M12.4 13.6 L17.4 4.8', SIL),
        path('M13.8 14.4 L18.8 5.6', SIL),
        path('M17.4 4.8 L18.0 2.6 L20.6 3.6 L18.8 5.6 Z', SIL),
        line(18.6, 2.5, 19.4, 1.8, DET),
        line(20.0, 3.2, 20.8, 2.5, DET),
        line(9.8, 16.1, 11.2, 15.4, DET),
    ]


def violin():
    return [
        path('M12 10.6 C10.2 10.6 8.8 11.5 8.8 12.7 C8.8 13.7 9.6 14.2 9.6 14.9 '
             'C9.6 15.7 8.5 16.2 8.5 17.6 C8.5 19.5 10.0 20.9 12 20.9 C14.0 20.9 '
             '15.5 19.5 15.5 17.6 C15.5 16.2 14.4 15.7 14.4 14.9 C14.4 14.2 15.2 13.7 '
             '15.2 12.7 C15.2 11.5 13.8 10.6 12 10.6 Z', SIL),
        path('M9.9 14.4 C10.4 15.2 10.3 16.0 9.8 16.6', DET),
        path('M14.1 14.4 C13.6 15.2 13.7 16.0 14.2 16.6', DET),
        line(11.0, 10.9, 11.0, 5.0, SIL),
        line(13.0, 10.9, 13.0, 5.0, SIL),
        circle(12, 4.0, 1.2, SIL),
        path('M11.3 3.3 C10.8 4.0 11.6 4.7 12.4 4.4', DET),
        line(11.4, 19.7, 12, 18.6, DET),
        line(12.6, 19.7, 12, 18.6, DET),
    ]


def cello():
    return [
        path('M12 8.4 C9.8 8.4 8.2 9.5 8.2 11.0 C8.2 12.2 9.1 12.8 9.1 13.6 '
             'C9.1 14.5 7.8 15.1 7.8 16.7 C7.8 19.0 9.6 20.6 12 20.6 C14.4 20.6 '
             '16.2 19.0 16.2 16.7 C16.2 15.1 14.9 14.5 14.9 13.6 C14.9 12.8 15.8 12.2 '
             '15.8 11.0 C15.8 9.5 14.2 8.4 12 8.4 Z', SIL),
        path('M9.5 13.2 C10.0 14.0 9.9 14.8 9.4 15.4', DET),
        path('M14.5 13.2 C14.0 14.0 14.1 14.8 14.6 15.4', DET),
        line(11.2, 8.7, 11.2, 3.6, SIL),
        line(12.8, 8.7, 12.8, 3.6, SIL),
        circle(12, 2.9, 1.1, SIL),
        line(12, 20.8, 12, 22.5, SIL),
        line(11.0, 22.5, 13.0, 22.5, DET),
    ]


def upright_bass():
    return [
        path('M12 10.4 C15.6 10.4 18.4 12.1 18.8 14.8 C19.2 17.2 17.6 18.3 17.8 19.7 '
             'C18.0 21.2 15.4 22.0 12 22.0 C8.6 22.0 6.0 21.2 6.2 19.7 C6.4 18.3 4.8 17.2 '
             '5.2 14.8 C5.6 12.1 8.4 10.4 12 10.4 Z', SIL),
        path('M8.2 14.2 C7.8 15.2 7.9 16.2 8.3 17.0', DET),
        path('M15.8 14.2 C16.2 15.2 16.1 16.2 15.7 17.0', DET),
        line(11.3, 10.7, 11.3, 4.6, SIL),
        line(12.7, 10.7, 12.7, 4.6, SIL),
        circle(12, 3.9, 1.1, SIL),
        line(9.8, 5.4, 11.3, 5.2, DET),
        line(9.8, 6.6, 11.3, 6.4, DET),
        line(14.2, 5.4, 12.7, 5.2, DET),
        line(14.2, 6.6, 12.7, 6.4, DET),
        line(12, 22.2, 12, 23.0, SIL),
    ]


def grand_piano():
    return [
        path('M4.0 18.0 H20.0 V12.2 C20.0 8.0 16.4 4.4 12.0 4.4 H4.0 Z', SIL),
        line(4.0, 15.6, 20.0, 15.6, DET),
        line(5.8, 15.8, 5.8, 17.8, DET),
        line(7.8, 15.8, 7.8, 17.8, DET),
        line(9.8, 15.8, 9.8, 17.8, DET),
        line(11.8, 15.8, 11.8, 17.8, DET),
        line(13.8, 15.8, 13.8, 17.8, DET),
        line(15.8, 15.8, 15.8, 17.8, DET),
        line(17.8, 15.8, 17.8, 17.8, DET),
        path('M6.6 15.4 C6.6 10.8 9.4 6.4 14.2 6.4', DET),
    ]


def electric_piano():
    return [
        rect(3.0, 8.2, 18.0, 7.8, 1.4, SIL),
        line(4.6, 12.4, 19.4, 12.4, DET),
        line(6.0, 12.4, 6.0, 15.4, DET),
        line(7.6, 12.4, 7.6, 15.4, DET),
        line(9.2, 12.4, 9.2, 15.4, DET),
        line(10.8, 12.4, 10.8, 15.4, DET),
        line(12.4, 12.4, 12.4, 15.4, DET),
        line(14.0, 12.4, 14.0, 15.4, DET),
        line(15.6, 12.4, 15.6, 15.4, DET),
        line(17.2, 12.4, 17.2, 15.4, DET),
        line(6.0, 16.0, 5.0, 20.6, SIL),
        line(18.0, 16.0, 19.0, 20.6, SIL),
    ]


def organ():
    return [
        rect(3.4, 4.4, 17.2, 16.0, 1.6, SIL),
        line(6.6, 5.8, 6.6, 8.0, SIL),
        line(9.2, 5.8, 9.2, 8.0, SIL),
        line(11.8, 5.8, 11.8, 8.0, SIL),
        line(14.4, 5.8, 14.4, 8.0, SIL),
        line(17.0, 5.8, 17.0, 8.0, SIL),
        rect(5.4, 9.8, 13.2, 1.8, 0.4, DET),
        rect(5.4, 12.8, 13.2, 1.8, 0.4, DET),
        line(8.2, 15.6, 8.2, 18.4, DET),
        line(10.4, 15.6, 10.4, 18.4, DET),
        line(12.6, 15.6, 12.6, 18.4, DET),
        line(14.8, 15.6, 14.8, 18.4, DET),
    ]


def accordion():
    return [
        rect(2.8, 6.2, 18.4, 11.6, 1.8, SIL),
        path('M8.4 7.4 L9.8 8.8 L8.4 10.2 L9.8 11.6 L8.4 13.0 L9.8 14.4 L8.4 15.8', DET),
        path('M13.4 7.4 L12.0 8.8 L13.4 10.2 L12.0 11.6 L13.4 13.0 L12.0 14.4 L13.4 15.8', DET),
        line(14.6, 8.6, 18.2, 8.6, DET),
        line(14.6, 10.2, 18.2, 10.2, DET),
        line(14.6, 11.8, 18.2, 11.8, DET),
        line(14.6, 13.4, 18.2, 13.4, DET),
        line(14.6, 15.0, 18.2, 15.0, DET),
        circle(5.2, 9.4, 0.55, DET),
        circle(5.2, 11.4, 0.55, DET),
        circle(5.2, 13.4, 0.55, DET),
        circle(6.4, 9.4, 0.55, DET),
        circle(6.4, 11.4, 0.55, DET),
        circle(6.4, 13.4, 0.55, DET),
    ]


def analog_synth():
    return [
        rect(2.8, 6.6, 18.4, 12.4, 2.0, SIL),
        circle(7.2, 10.2, 1.3, SIL),
        circle(12.0, 10.2, 1.3, SIL),
        circle(16.8, 10.2, 1.3, SIL),
        line(4.6, 14.4, 19.4, 14.4, DET),
        line(6.6, 14.4, 6.6, 16.8, DET),
        line(8.8, 14.4, 8.8, 16.8, DET),
        line(11.0, 14.4, 11.0, 16.8, DET),
        line(13.2, 14.4, 13.2, 16.8, DET),
        line(15.4, 14.4, 15.4, 16.8, DET),
        line(17.6, 14.4, 17.6, 16.8, DET),
    ]


def modular_synth():
    return [
        rect(3.0, 8.2, 18.0, 7.4, 1.0, SIL),
        circle(6.0, 10.8, 1.0, DET),
        circle(9.2, 10.8, 1.0, DET),
        circle(12.4, 10.8, 1.0, DET),
        circle(15.6, 10.8, 1.0, DET),
        circle(7.6, 13.6, 1.0, DET),
        circle(13.8, 13.6, 1.0, DET),
        path('M6.0 10.8 C5.0 4.8 15.8 4.8 15.6 10.8', SIL),
        path('M9.2 10.8 C10.0 6.6 12.0 6.4 12.4 10.8', DET),
    ]


def sampler():
    return [
        rect(3.2, 6.4, 17.6, 11.6, 1.6, SIL),
        rect(5.0, 8.4, 8.4, 4.2, 0.6, DET),
        path('M6.0 10.5 L7.0 9.2 L8.0 11.6 L9.0 8.8 L10.0 11.2 L11.0 9.8 L12.0 10.5', DET),
        rect(14.4, 8.6, 2.6, 2.6, 0.5, DET),
        rect(17.2, 8.6, 2.6, 2.6, 0.5, DET),
        rect(14.4, 11.6, 2.6, 2.6, 0.5, DET),
        rect(17.2, 11.6, 2.6, 2.6, 0.5, DET),
        circle(6.4, 15.6, 0.9, DET),
        circle(9.0, 15.6, 0.9, DET),
    ]


def drum_kit():
    return [
        circle(12, 15.6, 4.2, SIL),
        circle(12, 15.6, 3.0, DET),
        ellipse(8.6, 8.6, 2.4, 1.8, SIL),
        ellipse(15.4, 8.6, 2.4, 1.8, SIL),
        line(3.4, 5.6, 3.4, 10.2, DET),
        ellipse(3.4, 5.0, 2.0, 0.65, DET),
        line(20.6, 5.6, 20.6, 10.2, DET),
        ellipse(20.6, 5.0, 2.0, 0.65, DET),
    ]


def kick_drum():
    return [
        circle(12, 12.4, 6.0, SIL),
        circle(12, 12.4, 4.9, DET),
        circle(14.7, 14.3, 1.1, DET),
        line(12, 6.4, 12, 7.5, DET),
        line(12, 17.3, 12, 18.4, DET),
        line(6.4, 12.4, 7.5, 12.4, DET),
        line(16.5, 12.4, 17.6, 12.4, DET),
        line(9.0, 17.8, 7.8, 21.0, SIL),
        line(15.0, 17.8, 16.2, 21.0, SIL),
    ]


def snare_drum():
    return [
        ellipse(12, 8.2, 5.8, 2.0, SIL),
        ellipse(12, 16.2, 5.8, 2.0, SIL),
        line(6.2, 8.2, 6.2, 16.2, SIL),
        line(17.8, 8.2, 17.8, 16.2, SIL),
        line(6.2, 11.2, 7.6, 11.2, DET),
        line(16.4, 11.2, 17.8, 11.2, DET),
        line(17.8, 14.0, 19.4, 14.0, DET),
        path('M9.6 17.6 L10.2 18.8 L10.8 17.6 L11.4 18.8 L12.0 17.6', DET),
    ]


def hihat():
    return [
        ellipse(12, 8.6, 5.4, 0.9, SIL),
        ellipse(12, 10.2, 5.4, 0.9, SIL),
        line(12, 5.8, 12, 8.6, DET),
        circle(12, 5.4, 0.5, DET),
        line(12, 10.2, 12, 19.8, SIL),
        path('M12 19.8 L9.2 22.2 M12 19.8 L14.8 22.2', SIL),
        path('M12 18.6 L16.6 20.8', DET),
    ]


def percussion():
    return [
        ellipse(7.2, 7.4, 2.5, 0.9, SIL),
        path('M4.7 7.4 C4.7 11.4 5.3 14.8 7.2 14.8 C9.1 14.8 9.7 11.4 9.7 7.4', SIL),
        ellipse(15.6, 9.6, 2.5, 0.9, SIL),
        path('M13.1 9.6 C13.1 14.0 13.7 18.0 15.6 18.0 C17.5 18.0 18.1 14.0 18.1 9.6', SIL),
    ]


def drum_machine():
    out = [rect(3.0, 6.4, 18.0, 12.8, 1.8, SIL)]
    for row in (8.2, 11.2, 14.2):
        for col in (5.4, 8.6, 11.8, 15.0):
            out.append(rect(col, row, 2.4, 2.4, 0.5, DET))
    return out


def trumpet():
    return [
        line(3.0, 14.0, 13.4, 14.0, SIL),
        line(3.0, 13.0, 3.0, 15.0, SIL),
        line(1.8, 14.0, 3.0, 14.0, SIL),
        line(8.4, 10.6, 8.4, 14.0, SIL),
        line(10.0, 10.6, 10.0, 14.0, SIL),
        line(11.6, 10.6, 11.6, 14.0, SIL),
        line(7.9, 10.6, 8.9, 10.6, DET),
        line(9.5, 10.6, 10.5, 10.6, DET),
        line(11.1, 10.6, 12.1, 10.6, DET),
        path('M13.4 14.9 L20.0 11.2 C20.9 10.8 21.2 9.8 20.8 8.9 L19.2 5.6 '
             'C18.9 4.9 18.0 4.7 17.4 5.3 L13.4 13.2 Z', SIL),
    ]


def saxophone():
    return [
        path('M14.6 3.0 L15.6 3.8 L15.4 5.2', SIL),
        path('M15.4 5.2 C15.4 8.2 13.2 9.2 13.2 12.2 L13.2 15.2', SIL),
        path('M13.2 15.2 C13.2 18.0 10.6 19.4 8.6 18.6 C6.6 17.8 5.8 15.2 6.8 13.4', SIL),
        path('M6.8 13.4 L3.2 12.2 C2.2 11.8 1.6 12.8 2.0 13.6 L3.4 16.4 C3.9 17.3 5.0 17.5 5.8 16.9', SIL),
        circle(14.2, 8.2, 0.8, DET),
        circle(14.2, 10.4, 0.8, DET),
        circle(14.0, 12.6, 0.8, DET),
    ]


def flute():
    return [
        line(2.8, 13.4, 19.8, 13.4, SIL),
        line(2.8, 12.2, 2.8, 14.6, SIL),
        line(19.8, 12.6, 19.8, 14.2, SIL),
        ellipse(6.6, 13.4, 1.0, 0.5, DET),
        rect(9.4, 12.0, 1.6, 2.8, 0.8, DET),
        rect(11.6, 12.0, 1.6, 2.8, 0.8, DET),
        rect(13.8, 12.0, 1.6, 2.8, 0.8, DET),
        rect(16.0, 12.0, 1.6, 2.8, 0.8, DET),
    ]


def clarinet():
    return [
        line(10.6, 4.6, 10.6, 15.2, SIL),
        line(13.4, 4.6, 13.4, 15.2, SIL),
        path('M10.6 4.6 H13.4', SIL),
        path('M10.6 4.6 L11.4 2.6 H12.6 L13.4 4.6', SIL),
        path('M10.6 15.2 C10.6 17.4 9.4 19.2 7.8 20.0 H16.2 C14.6 19.2 13.4 17.4 13.4 15.2', SIL),
        line(13.4, 6.8, 14.6, 6.8, DET),
        line(13.4, 8.6, 14.6, 8.6, DET),
        line(13.4, 10.4, 14.6, 10.4, DET),
        line(13.4, 12.2, 14.6, 12.2, DET),
    ]


def harp():
    return [
        line(4.2, 4.4, 4.2, 20.4, SIL),
        path('M4.2 4.4 C10.6 3.4 16.8 6.2 19.8 9.6', SIL),
        line(4.2, 20.4, 19.8, 9.6, SIL),
        line(4.2, 20.4, 7.6, 20.4, SIL),
        line(7.0, 4.9, 7.0, 18.4, DET),
        line(9.6, 5.0, 9.6, 16.7, DET),
        line(12.2, 5.7, 12.2, 15.0, DET),
        line(14.8, 6.9, 14.8, 13.3, DET),
        line(17.4, 8.6, 17.4, 11.6, DET),
    ]


def timpani():
    return [
        ellipse(12, 11.2, 7.2, 1.8, SIL),
        path('M4.8 11.2 C4.8 16.6 8.6 20.2 12 20.2 C15.4 20.2 19.2 16.6 19.2 11.2', SIL),
        line(9.4, 19.2, 8.2, 21.6, DET),
        line(14.6, 19.2, 15.8, 21.6, DET),
        path('M4.4 5.0 L9.4 9.4', DET),
        circle(10.1, 10.1, 1.2, SIL),
        path('M19.6 5.0 L14.6 9.4', DET),
        circle(13.9, 10.1, 1.2, SIL),
    ]


def midi_controller():
    return [
        rect(2.6, 9.0, 18.8, 9.6, 1.8, SIL),
        rect(4.2, 10.8, 1.6, 4.6, 0.8, DET),
        rect(6.4, 10.8, 1.6, 4.6, 0.8, DET),
        line(9.0, 13.6, 20.0, 13.6, DET),
        line(10.4, 13.6, 10.4, 17.0, DET),
        line(12.0, 13.6, 12.0, 17.0, DET),
        line(13.6, 13.6, 13.6, 17.0, DET),
        line(15.2, 13.6, 15.2, 17.0, DET),
        line(16.8, 13.6, 16.8, 17.0, DET),
        line(18.4, 13.6, 18.4, 17.0, DET),
    ]


ICONS = [
    ('vocal_mic', 'voice', 'Вокал', 'Vocal', vocal_mic),
    ('backing_vocals', 'voice', 'Бэк-вокал', 'Backing vocals', backing_vocals),
    ('choir', 'voice', 'Хор', 'Choir', choir),
    ('electric_guitar', 'strings', 'Электрогитара', 'Electric guitar', electric_guitar),
    ('acoustic_guitar', 'strings', 'Акустическая гитара', 'Acoustic guitar', acoustic_guitar),
    ('bass_guitar', 'strings', 'Бас-гитара', 'Bass guitar', bass_guitar),
    ('banjo', 'strings', 'Банджо', 'Banjo', banjo),
    ('violin', 'strings', 'Скрипка', 'Violin', violin),
    ('cello', 'strings', 'Виолончель', 'Cello', cello),
    ('upright_bass', 'strings', 'Контрабас', 'Upright bass', upright_bass),
    ('grand_piano', 'keys', 'Рояль', 'Grand piano', grand_piano),
    ('electric_piano', 'keys', 'Электропиано', 'Electric piano', electric_piano),
    ('organ', 'keys', 'Орган', 'Organ', organ),
    ('accordion', 'keys', 'Аккордеон', 'Accordion', accordion),
    ('analog_synth', 'keys', 'Аналоговый синтезатор', 'Analog synth', analog_synth),
    ('modular_synth', 'keys', 'Модульный синтезатор', 'Modular synth', modular_synth),
    ('sampler', 'keys', 'Сэмплер', 'Sampler', sampler),
    ('drum_kit', 'drums', 'Ударная установка', 'Drum kit', drum_kit),
    ('kick_drum', 'drums', 'Бочка', 'Kick drum', kick_drum),
    ('snare_drum', 'drums', 'Малый барабан', 'Snare drum', snare_drum),
    ('hihat', 'drums', 'Хай-хэт', 'Hi-hat', hihat),
    ('percussion', 'drums', 'Перкуссия', 'Percussion', percussion),
    ('drum_machine', 'drums', 'Драм-машина', 'Drum machine', drum_machine),
    ('trumpet', 'winds', 'Труба', 'Trumpet', trumpet),
    ('saxophone', 'winds', 'Саксофон', 'Saxophone', saxophone),
    ('flute', 'winds', 'Флейта', 'Flute', flute),
    ('clarinet', 'winds', 'Кларнет', 'Clarinet', clarinet),
    ('harp', 'other', 'Арфа', 'Harp', harp),
    ('timpani', 'other', 'Литавры', 'Timpani', timpani),
    ('midi_controller', 'other', 'MIDI-клавиатура', 'MIDI controller', midi_controller),
]

GROUPS = [
    ('voice', 'Голос', 'Voice'),
    ('strings', 'Струнные', 'Strings'),
    ('keys', 'Клавишные и синтез', 'Keys and synth'),
    ('drums', 'Барабаны', 'Drums'),
    ('winds', 'Духовые', 'Winds'),
    ('other', 'Другое', 'Other'),
]


def svg_doc(title, children):
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24">\n'
            f'<title>{html.escape(title)}</title>\n' + '\n'.join(children) + '\n</svg>\n')


def inner_markup(doc):
    return '\n'.join(doc.splitlines()[2:-1])


def build_assets():
    OUT_SVG.mkdir(parents=True, exist_ok=True)
    docs = {}
    assets = []
    for name, g, ru, en, fn in ICONS:
        doc = svg_doc(f'MR Live track {en}', fn())
        ET.fromstring(doc)
        (OUT_SVG / f'{name}.svg').write_text(doc, encoding='utf-8')
        docs[name] = doc
        assets.append({
            'id': f'icons/live_track/{name}',
            'file': f'svg/icons/live_track/{name}.svg',
            'size': [24, 24],
            'pack': PACK,
            'family': 'live_track_instruments',
            'group': g,
            'label': en,
            'label_ru': ru,
            'state': 'normal',
            'kind': 'glyph',
            'lod': 'standard_24',
            'source': SOURCE,
            'license': LICENSE,
        })
    return docs, assets


def build_catalog(assets):
    catalog = {
        'name': 'MR Live track instrument icons',
        'version': '1.0',
        'pack': PACK,
        'authority': 'MR UDS visual language; MR Live track headers',
        'license': LICENSE,
        'source': SOURCE,
        'grid': '24x24 viewBox, 20x20 optical field, transparent background',
        'style': 'Studio-One-like single-object pictograms: one dominant silhouette, '
                 'few identifying details, no frames, no baked state colour',
        'stroke': {'silhouette': SIL, 'detail': DET},
        'color': {'normal': LINE, 'note': 'host tints selected/disabled; icon files stay neutral'},
        'integration': 'Assets only. Runtime adoption in MR Live track headers is a separate '
                       'approved slice; nothing here changes model, persistence, DSP or layout.',
        'assets': assets,
    }
    (ROOT / 'catalogs').mkdir(exist_ok=True)
    (ROOT / 'catalogs' / f'{PACK}.json').write_text(
        json.dumps(catalog, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')


def merge_manifest(assets):
    path = ROOT / 'manifest.json'
    manifest = json.loads(path.read_text(encoding='utf-8'))
    manifest['assets'] = [a for a in manifest['assets'] if a.get('pack') != PACK] + assets
    manifest['total_asset_svg'] = len(manifest['assets'])
    path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
    return manifest


# --------------------------------------------------------------------------
# Previews: vector contact sheet + offline HTML gallery (actual-size strips and
# host tint states). Labels live in HTML/SVG text, never inside the assets.
# --------------------------------------------------------------------------

def overview_svg(assets, docs):
    width, gap = 1440, 12
    card_w, card_h, cols = 264, 150, 5
    x0 = (width - (cols * card_w + (cols - 1) * gap)) // 2
    y = 150
    parts = [f'<rect width="{width}" height="100%" fill="{BG}"/>',
             f'<path d="M36 104 H1404" stroke="{EDGE}" stroke-width="1"/>',
             f'<text x="40" y="55" font-size="30" fill="{BRIGHT}">MR Live — иконки инструментов треков</text>',
             f'<text x="40" y="85" font-size="14" fill="{STROKE}">'
             '30 оригинальных глифов 24 px · MR UDS: светлый контур 1.6 / детали 1.3 · '
             'нейтральный цвет, состояния тонирует хост</text>']
    for g, ru, en in GROUPS:
        entries = [a for a in assets if a['group'] == g]
        parts.append(f'<text x="40" y="{y}" font-size="14" letter-spacing="2" fill="{ACCENT}">'
                     f'{html.escape(ru.upper())} · {len(entries)}</text>')
        y += 22
        rows = (len(entries) + cols - 1) // cols
        for i, a in enumerate(entries):
            cx = x0 + (i % cols) * (card_w + gap)
            cy = y + (i // cols) * (card_h + gap)
            name = a['id'].split('/')[-1]
            parts.append(f'<rect x="{cx}" y="{cy}" width="{card_w}" height="{card_h}" rx="9" '
                         f'fill="{SURFACE}" stroke="{EDGE}" stroke-width="1"/>')
            parts.append(f'<g transform="translate({cx + (card_w - 56) / 2:g} {cy + 20}) scale(2.3333)">'
                         + inner_markup(docs[name]) + '</g>')
            parts.append(f'<text x="{cx + card_w / 2:g}" y="{cy + 108}" text-anchor="middle" '
                         f'font-size="13" fill="{BRIGHT}">{html.escape(a["label_ru"])}</text>')
            parts.append(f'<text x="{cx + card_w / 2:g}" y="{cy + 126}" text-anchor="middle" '
                         f'font-size="10.5" fill="{MUTED}">{html.escape(name)}</text>')
        y += rows * (card_h + gap) + 14
    parts.append(f'<text x="40" y="{y + 14}" font-size="12" fill="{MUTED}">'
                 'Оригинальные векторы, без растров, шрифтов и внешних ссылок. '
                 'Интеграция в заголовки треков MR Live — отдельный согласованный этап.</text>')
    height = y + 40
    data = (f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
            f'viewBox="0 0 {width} {height}"><g font-family="Segoe UI, system-ui, sans-serif">'
            + ''.join(parts) + '</g></svg>\n')
    (ROOT / 'previews' / 'MR_Live_Track_Icons_Overview.svg').write_text(data, encoding='utf-8')
    return height


def inline_svg(doc, size, color=None):
    body = doc.replace('width="24" height="24"', f'width="{size}" height="{size}"')
    if color:
        body = body.replace(LINE, color)
    return body.strip()


def gallery_html(assets, docs):
    def cards():
        out = []
        for g, ru, en in GROUPS:
            entries = [a for a in assets if a['group'] == g]
            out.append(f'<h2>{html.escape(ru)} <span class="count">{len(entries)}</span></h2>')
            out.append('<div class="grid">')
            for a in entries:
                name = a['id'].split('/')[-1]
                out.append(
                    '<figure class="card">'
                    f'<div class="glyph">{inline_svg(docs[name], 44)}</div>'
                    f'<figcaption><b>{html.escape(a["label_ru"])}</b>'
                    f'<code>{html.escape(name)}</code></figcaption></figure>')
            out.append('</div>')
        return '\n'.join(out)

    def strip(size, sample):
        cells = []
        for name in sample:
            cells.append(f'<span class="cell" style="height:{max(size + 16, 40)}px">'
                         f'{inline_svg(docs[name], size)}</span>')
        return ''.join(cells)

    sample = [a['id'].split('/')[-1] for a in assets]
    tint = ''.join(
        f'<div class="state"><span class="stateicon">{inline_svg(docs[n], 32, c)}</span>'
        f'<b>{html.escape(t)}</b><code>{html.escape(n)}</code></div>'
        for t, c, n in (('Normal', LINE, 'vocal_mic'),
                        ('Selected / focus', ACCENT, 'electric_guitar'),
                        ('Disabled', DISABLED, 'grand_piano')))
    page = f'''<!doctype html>
<html lang="ru"><meta charset="utf-8"><meta name="viewport" content="width=device-width">
<title>MR Live — иконки инструментов треков</title>
<style>
*{{box-sizing:border-box}}
body{{margin:0;background:{BG};color:{BRIGHT};font:14px/1.5 "Segoe UI",system-ui,sans-serif}}
main{{max-width:1440px;margin:0 auto;padding:0 40px 56px}}
header{{padding:40px 0 24px;border-bottom:1px solid {EDGE}}}
.eyebrow{{font-size:11px;letter-spacing:2.4px;color:{ACCENT};text-transform:uppercase;margin:0 0 12px}}
h1{{font-size:34px;font-weight:600;letter-spacing:-.2px;margin:0 0 12px}}
.lead{{margin:0;max-width:820px;color:{STROKE};font-size:15px}}
.meta{{display:flex;flex-wrap:wrap;gap:28px;margin:22px 0 0;padding:0;list-style:none}}
.meta li{{font-size:12px;color:{MUTED}}}
.meta b{{display:block;font-size:18px;color:{BRIGHT};font-weight:600}}
h2{{font-size:13px;letter-spacing:1.8px;text-transform:uppercase;color:{STROKE};
   font-weight:600;margin:40px 0 14px;display:flex;align-items:baseline;gap:10px}}
.count{{font-size:12px;letter-spacing:0;color:{MUTED};font-weight:500}}
.grid{{display:grid;grid-template-columns:repeat(6,minmax(0,1fr));gap:12px}}
.card{{margin:0;background:{SURFACE};border:1px solid {EDGE};border-radius:10px;
   padding:14px 12px 12px;display:flex;flex-direction:column;gap:10px;transition:border-color .15s}}
.card:hover,.card:focus-within{{border-color:{ACCENT}}}
.glyph{{height:48px;display:flex;align-items:center;justify-content:center}}
figcaption{{display:flex;flex-direction:column;gap:2px;min-width:0}}
figcaption b{{font-size:12.5px;font-weight:600;line-height:1.25}}
code{{font-family:"Cascadia Mono",Consolas,monospace;font-size:10px;color:{MUTED};
   overflow:hidden;text-overflow:ellipsis;white-space:nowrap}}
.strip{{background:{WINDOW};border:1px solid {EDGE};border-radius:10px;padding:10px 6px;
   display:flex;align-items:center;gap:0;overflow:hidden;margin:0 0 10px}}
.cell{{flex:1 1 0;display:flex;align-items:center;justify-content:center;border-right:1px solid rgba(75,95,123,.35)}}
.cell:last-child{{border-right:0}}
.state{{background:{SURFACE};border:1px solid {EDGE};border-radius:10px;padding:16px;
   display:flex;align-items:center;gap:14px;flex-wrap:wrap}}
.stateicon{{width:40px;display:flex;justify-content:center}}
.state b{{font-size:12.5px}}
.state code{{margin-left:auto}}
.note{{margin:14px 0 0;font-size:12px;color:{MUTED};max-width:900px}}
footer{{margin-top:44px;padding-top:18px;border-top:1px solid {EDGE};
   display:flex;justify-content:space-between;gap:24px;font-size:12px;color:{MUTED};flex-wrap:wrap}}
@media(max-width:900px){{.grid{{grid-template-columns:repeat(3,minmax(0,1fr))}}main{{padding:0 20px 40px}}}}
</style>
<main>
<header>
<p class="eyebrow">MR UDS · asset pack · MR Live</p>
<h1>Иконки инструментов для треков MR Live</h1>
<p class="lead">Тридцать глифов 24×24 в спокойной линейной манере Studio One: один крупный
силуэт, пара опознавательных деталей, общий контур. Цвет нейтральный — состояние подбирает хост.</p>
<ul class="meta">
<li><b>30</b>глифов, 6 семейств</li>
<li><b>24 px</b>сетка, поле 20×20</li>
<li><b>1.6 / 1.3</b>контур / детали</li>
<li><b>0</b>растров, шрифтов и скриптов</li>
</ul>
</header>

<h2>Набор <span class="count">30 глифов</span></h2>
{cards()}

<h2>Фактический размер <span class="count">24 px и 16 px</span></h2>
<div class="strip">{strip(24, sample[:15])}</div>
<div class="strip">{strip(16, sample[:15])}</div>
<div class="strip">{strip(16, sample[15:])}</div>
<p class="note">Нижние полосы имитируют плотность заголовка трека: 15 иконок в ряд на тёмной
поверхности. Проверяются на различимость на 100 % масштабе, без увеличения.</p>

<h2>Состояния <span class="count">тонирование, не отдельные файлы</span></h2>
<div class="state">{tint}</div>
<p class="note">Геометрия одна; акцент выделения — {ACCENT}, отключённое — {DISABLED}.
Приглушённые заливки не используются: значение несёт форма, а не только цвет.</p>

<h2>Грамматика <span class="count">как читать набор</span></h2>
<div class="grid" style="grid-template-columns:repeat(3,minmax(0,1fr))">
<figure class="card"><figcaption><b>Один силуэт</b><span class="note">Инструмент узнаётся по
внешнему контуру; внутренние линии только объясняют, что это за инструмент.</span></figcaption></figure>
<figure class="card"><figcaption><b>Общий ритм</b><span class="note">Оптическое поле 20×20,
контур 1.6, детали 1.3, круглые концы, базовая линия снизу — глифы держат строй в ряду.</span></figcaption></figure>
<figure class="card"><figcaption><b>Без рамок</b><span class="note">Ни плашек, ни фона, ни
свечения: иконка живёт в заголовке трека и получает цвет от темы.</span></figcaption></figure>
</div>

<footer>
<span>Источник: {SOURCE}. Лицензия: {LICENSE}.</span>
<span>Файлы: <code>svg/icons/live_track/</code> · каталог <code>catalogs/{PACK}.json</code></span>
</footer>
</main></html>
'''
    (ROOT / 'previews' / 'live_track_icons.html').write_text(page, encoding='utf-8')


def main():
    docs, assets = build_assets()
    build_catalog(assets)
    manifest = merge_manifest(assets)
    height = overview_svg(assets, docs)
    gallery_html(assets, docs)
    print(f'PASS: {len(assets)} icons, manifest total {manifest["total_asset_svg"]}, '
          f'overview {height}px')


if __name__ == '__main__':
    main()
