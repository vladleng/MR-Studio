"""Additive concept pack. Python 3 stdlib; never imports/runs build_kit.py.

Only owns svg/{controls,icons}/concept_pack, its catalog/tokens and gallery.
Merges its own entries into manifest.json, preserving other entries verbatim.
Run from any working directory: python path/to/build_concept_pack.py
"""
from pathlib import Path
import html
import json
import math
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parent
PACK = 'fader_control_track_utility_v1'
P = dict(bg='#111720', surface='#192333', raised='#2D394E', line='#A9BFDC',
         muted='#7188AB', faint='#4B5F7B', bright='#DAE5F4', cyan='#00B8EF',
         blue='#91BBEE', violet='#B45DF0', red='#F04459', amber='#E2B44A',
         green='#24D67C', yellow='#F0D447')
ASSETS = []
REUSE = []


class SVG:
    def __init__(self, w, h):
        self.w, self.h, self.parts = w, h, []

    def el(self, tag, **attrs):
        values = ' '.join(f'{k.replace("_", "-")}="{html.escape(str(v), quote=True)}"'
                          for k, v in attrs.items())
        self.parts.append(f'<{tag} {values}/>')

    def path(self, d, c='line', sw=1.6, fill='none', **attrs):
        self.el('path', d=d, fill=P.get(fill, fill), stroke=P.get(c, c),
                stroke_width=sw, stroke_linecap='round', stroke_linejoin='round', **attrs)

    def line(self, x, y, xx, yy, c='line', sw=1):
        self.path(f'M{x:g} {y:g} L{xx:g} {yy:g}', c, sw)

    def rect(self, x, y, w, h, fill='none', c='line', rx=2, sw=1):
        self.el('rect', x=x, y=y, width=w, height=h, rx=rx,
                fill=P.get(fill, fill), stroke=P.get(c, c), stroke_width=sw)

    def circle(self, x, y, r, fill='none', c='line', sw=1.6):
        self.el('circle', cx=x, cy=y, r=r, fill=P.get(fill, fill),
                stroke=P.get(c, c), stroke_width=sw)

    def xml(self, name):
        title = html.escape(name.replace('_', ' '))
        return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{self.w}" height="{self.h}" '
                f'viewBox="0 0 {self.w} {self.h}">\n<title>MR Studio {title}</title>\n'
                + '\n'.join(self.parts) + '\n</svg>\n')


def save(group, name, s, **meta):
    path = Path('svg') / group / 'concept_pack' / (name + '.svg')
    data = s.xml(name)
    ET.fromstring(data)
    dst = ROOT / path
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_text(data, encoding='utf-8')
    ASSETS.append(dict(id=f'{group}/concept_pack/{name}', file=path.as_posix(),
                       size=[s.w, s.h], pack=PACK, **meta))


def reuse(role, path, size=(24, 24)):
    assert (ROOT / path).is_file(), path
    REUSE.append(dict(role=role, file=path, size=list(size)))


# Canonical existing semantics: refer to originals rather than duplicating them.
for role, name in dict(vocal='mic', settings='settings', power='power', add_track='plus',
                       routing='routing', instrument='sine', link='link').items():
    reuse(role, f'svg/icons/{name}.svg')
for name in ('play', 'stop', 'record', 'rewind', 'fast-forward', 'loop', 'pause', 'previous', 'next'):
    reuse(f'transport_{name.replace("-", "_")}', f'../icons/transport/{name}.svg')


# Faders: fixed-state samples at 55%; layer files permit a host to position
# the fill and thumb independently. No embedded value strings or hit targets.
STATES = ('normal', 'hover', 'active', 'focus', 'automation', 'disabled')
for family, w, h, horizontal in (('micro', 40, 200, False),
                                ('precision', 56, 240, False),
                                ('slide', 224, 40, True)):
    for state in STATES:
        s = SVG(w, h)
        c = 'violet' if state == 'automation' else 'muted' if state == 'disabled' else 'cyan'
        track = SVG(w, h)
        fill = SVG(w, h)
        thumb = SVG(20 if horizontal else 18, 26 if horizontal else 28)
        if horizontal:
            track.rect(8, 16, w-16, 8, 'surface', 'muted', 4)
            for x in range(16, w-8, 16):
                for a, b in ((5, 10), (30, 35)):
                    track.line(x, a, x, b, 'muted')
            pos = round(12+.55*(w-24))
            fill.line(12, 20, pos, 20, c, 3)
            px, py = pos-10, 7
        else:
            track.rect(w/2-4, 8, 8, h-16, 'surface', 'muted', 4)
            for y in range(16, h-8, 12):
                track.line(w/2-14, y, w/2-9, y, 'muted')
                track.line(w/2+9, y, w/2+14, y, 'muted')
            pos = round(h-12+.55*(24-h))
            fill.line(w/2, h-12, w/2, pos, c, 3)
            px, py = w/2-9, pos-14
        if state == 'automation':
            # Diamond silhouette distinguishes automation without relying on hue.
            tw, th = thumb.w, thumb.h
            thumb.path(f'M{tw/2} 3 L{tw-3} {th/2} L{tw/2} {th-3} L3 {th/2} Z',
                       c, 1.6, 'surface')
            thumb.circle(tw/2, th/2, 2, c, c, 1)
        else:
            thumb.rect(3, 3, thumb.w-6, thumb.h-6, 'raised',
                       'bright' if state in ('hover', 'active', 'focus') else 'line', 3, 1.5)
            thumb.line(6, thumb.h/2, thumb.w-6, thumb.h/2,
                       c if state != 'disabled' else 'muted', 1.5)
        s.parts.extend(track.parts)
        if state != 'disabled':
            s.parts.extend(fill.parts)
        s.parts.append(f'<g transform="translate({px} {py})">'+''.join(thumb.parts)+'</g>')
        if state == 'focus':
            s.rect(1.5, 1.5, w-3, h-3, 'none', 'blue', 5, 1.5)
        if state == 'active':
            s.circle(w-5 if horizontal else w/2, 5, 2, c, c, 1)
        save('controls', f'fader_{family}_{state}', s, family=f'fader_{family}',
             state=state, kind='state_sample', sample_normalized=.55,
             thumb_origin=[px, py], axis='x' if horizontal else 'y')
        save('controls', f'fader_{family}_thumb_{state}', thumb,
             family=f'fader_{family}', state=state, kind='layer')
        if state == 'normal':
            save('controls', f'fader_{family}_track', track, family=f'fader_{family}', kind='layer')
            save('controls', f'fader_{family}_fill_sample', fill,
                 family=f'fader_{family}', kind='layer', sample_normalized=.55)


# Scales are tick geometry only. Host owns locale, units and actual dB law.
for name, w, h, horizontal, fine in (('db_vertical', 32, 200, False, False),
                                    ('fine_vertical', 24, 200, False, True),
                                    ('db_horizontal', 200, 32, True, False),
                                    ('fine_horizontal', 200, 24, True, True)):
    s = SVG(w, h)
    n = 32 if fine else 8
    for i in range(n+1):
        p = 8+i*((w if horizontal else h)-16)/n
        length = 12 if i % 4 == 0 else 6
        if horizontal:
            s.line(p, 4, p, 4+length)
        else:
            s.line(w-4-length, p, w-4, p)
    save('controls', f'scale_{name}', s, family='scales', kind='layer',
         label_policy='Host draws labels from the real parameter mapping; ticks have no dB law.')


# LED meter samples. A live meter must supply measured values and ballistics.
for role, levels in dict(empty=(0, 0), low=(5, 7), mid=(12, 14), high=(19, 18),
                         peak=(24, 24), stereo=(18, 16), input=(10, 12),
                         output=(15, 17), rms=(9, 9), clip=(24, 24)).items():
    s = SVG(40, 156)
    for ch, level in enumerate(levels):
        for i in range(24):
            c = ('red' if role in ('peak', 'clip') or i >= 22 else
                 'amber' if i >= 17 else 'green' if i >= 9 else 'cyan')
            s.rect(7+ch*16, 146-i*6, 9, 4, c if i < level else 'surface',
                   c if i < level else 'faint', .4, .5)
    save('controls', f'meter_{role}', s, family='meters', kind='state_sample',
         state=role, measured_data=False, segments_per_channel=24)


# Button M/S glyphs are paths; text/numbers are owned by the host elsewhere.
for role, c in (('mute', 'red'), ('solo', 'amber'), ('record', 'red')):
    for on in (False, True):
        s = SVG(40, 36)
        s.rect(2, 2, 36, 32, 'surface', c if on else 'muted', 5, 1.2)
        cc = c if on else 'line'
        if role == 'mute':
            s.path('M13 25 V11 L20 20 L27 11 V25', cc, 2)
        elif role == 'solo':
            s.path('M26 12 C12 6 10 18 20 18 C32 18 26 31 13 24', cc, 2)
        else:
            s.circle(20, 18, 7, cc if on else 'none', cc, 2)
        if on:
            s.line(16, 30, 24, 30, cc, 2)
        save('controls', f'button_{role}_{"on" if on else "off"}', s,
             family='channel_buttons', state='on' if on else 'off')

for state in ('default', 'focus', 'automation', 'disabled', 'unit_menu', 'error'):
    s = SVG(96, 36)
    c = dict(focus='cyan', automation='violet', disabled='faint', error='red').get(state, 'line')
    s.rect(2, 2, 92, 32, 'surface', c, 5, 1.5 if state != 'default' else 1)
    if state == 'unit_menu':
        s.path('M79 15 L83 19 L87 15', 'line', 1.5)
        s.line(72, 8, 72, 28, 'faint')
    if state == 'automation':
        s.path('M7 18 L10 15 L13 18 L10 21 Z', c, 1, c)
    if state == 'error':
        s.path('M85 10 V19 M85 24 V25', c, 2)
    save('controls', f'value_capsule_{state}', s, family='value_capsules', state=state,
         text_inset=[18 if state == 'automation' else 8, 6, 52 if state in ('unit_menu', 'error') else 70, 24])

for state, angle in (('center', -90), ('left', -140), ('right', -40), ('active', -40), ('disabled', -90)):
    s = SVG(64, 64)
    for i in range(19):
        a = math.radians(135+i*15)
        s.line(32+27*math.cos(a), 32+27*math.sin(a),
               32+29*math.cos(a), 32+29*math.sin(a), 'faint')
    c = 'violet' if state == 'active' else 'muted' if state == 'disabled' else 'cyan'
    s.circle(32, 32, 23, 'surface', c if state == 'active' else 'line', 1.5)
    a = math.radians(angle)
    s.line(32+7*math.cos(a), 32+7*math.sin(a), 32+20*math.cos(a), 32+20*math.sin(a), c, 3)
    save('controls', f'pan_{state}', s, family='pan', state=state, kind='state_sample')

for state in ('normal', 'hover', 'pressed', 'focus', 'selected', 'disabled'):
    s = SVG(40, 40)
    s.rect(2, 2, 36, 36, 'raised' if state in ('pressed', 'selected') else 'surface',
           'blue' if state in ('hover', 'focus', 'selected') else 'faint' if state == 'disabled' else 'muted', 5)
    if state == 'focus':
        s.rect(.75, .75, 38.5, 38.5, 'none', 'blue', 6, 1.5)
    if state == 'selected':
        s.line(15, 34, 25, 34, 'cyan', 2)
    save('controls', f'toolbar_button_{state}', s, family='toolbar_buttons', state=state,
         icon_slot=[8, 8, 24, 24])

# Shape is a reusable frame; glyph remains the existing canonical transport.
s = SVG(56, 56)
s.circle(28, 28, 25, 'surface', 'line', 1.5)
save('controls', 'transport_button_frame', s, family='transport', icon_slot=[16, 16, 24, 24])

for style in ('solid', 'dotted', 'dashed'):
    for horizontal in (True, False):
        s = SVG(120, 8) if horizontal else SVG(8, 120)
        d = 'M4 4 H116' if horizontal else 'M4 4 V116'
        extra = {} if style == 'solid' else {'stroke_dasharray': '1 5' if style == 'dotted' else '6 5'}
        s.path(d, 'muted', 1, **extra)
        save('controls', f'divider_{"horizontal" if horizontal else "vertical"}_{style}', s,
             family='dividers', kind='layout_sample')
for px in (4, 8, 16, 24):
    s = SVG(px, px)
    s.rect(.5, .5, px-1, px-1, 'raised', 'faint', min(3, px/4), 1)
    save('controls', f'spacer_{px}', s, family='spacers', kind='layout_sample')
for horizontal in (True, False):
    s = SVG(120, 8) if horizontal else SVG(8, 120)
    s.path('M4 4 H116' if horizontal else 'M4 4 V116', 'line', 2)
    save('controls', f'divider_{"horizontal" if horizontal else "vertical"}_2px', s,
         family='dividers', kind='layout_sample')

# Number tags: backgrounds only; no baked track numbers or unit strings.
for color in ('cyan', 'blue', 'violet', 'red', 'amber', 'yellow', 'green', 'surface'):
    s = SVG(32, 32)
    s.rect(1.5, 1.5, 29, 29, color, 'line' if color == 'surface' else color, 5)
    save('controls', f'track_tag_{color}', s, family='track_tags',
         text_slot=[4, 4, 24, 24], text_color=P['bright'] if color == 'surface' else P['bg'])


def glyph(name, compact=False):
    s = SVG(24, 24)
    if name == 'track_drums':
        s.el('ellipse', cx=12, cy=12, rx=5, ry=2, fill='none', stroke=P['line'], stroke_width=1.6)
        s.path('M7 12 V19 C7 22 17 22 17 19 V12 M4 5 V19 M20 5 V19')
        s.path('M1.5 5 L4 3.5 L6.5 5 L4 6.5 Z M17.5 5 L20 3.5 L22.5 5 L20 6.5 Z')
    elif name in ('track_bass', 'track_guitar'):
        s.path('M14 10 L20 2 L22 4 L16 12 C21 18 13 24 8 21 C3 18 5 13 9 12 L11 13 Z',
               'cyan' if name == 'track_bass' else 'amber')
        s.path('M10 17 L18 6', 'cyan' if name == 'track_bass' else 'amber', 1)
        if not compact:
            s.circle(11, 17, 1.6, 'none', 'cyan' if name == 'track_bass' else 'amber', 1)
    elif name in ('track_piano', 'track_synth'):
        c = 'line' if name == 'track_piano' else 'cyan'
        s.rect(2, 5, 20, 14, 'none', c, 1, 1.6)
        for x in (7, 12, 17):
            s.line(x, 6, x, 18, c, 1)
        for x in ((6, 11, 16) if compact else (5, 9, 15, 19)):
            s.rect(x, 6, 1.5, 6, c, c, 0, .5)
    elif name == 'track_fx':
        s.path('M15 2 Q15 10 22 10 Q15 10 15 19 Q15 10 8 10 Q15 10 15 2 Z', 'violet', 1, 'violet')
        s.path('M5 12 Q5 17 9 17 Q5 17 5 22 Q5 17 1 17 Q5 17 5 12 Z', 'violet', 1, 'violet')
    elif name == 'track_backing_vocals':
        s.circle(12, 7, 3)
        s.path('M6 21 V18 C6 12 18 12 18 18 V21 Z M2 20 V17 Q2 13 5 13 M22 20 V17 Q22 13 19 13')
        if not compact:
            s.circle(3, 10, 1.5)
            s.circle(21, 10, 1.5)
    elif name == 'track_master':
        for i, h in enumerate((7, 12, 17, 21)):
            s.rect(3+i*5, 23-h, 2.5, h-1, 'green', 'green', 0, .5)
    elif name == 'folder':
        s.path('M2 7 V4 H9 L12 7 H22 V21 H2 Z')
    elif name == 'folder_open':
        s.path('M2 18 V4 H9 L12 7 H21 V10 M2 21 L5 11 H23 L20 21 Z')
    elif name == 'search':
        s.circle(10, 10, 7)
        s.line(15, 15, 22, 22, sw=1.6)
    elif name == 'mixer_params':
        for y, x in ((5, 9), (12, 16), (19, 7)):
            s.line(2, y, x-3, y, sw=1.6)
            s.line(x+3, y, 22, y, sw=1.6)
            s.circle(x, y, 2.5)
    elif name == 'delete':
        s.path('M4 6 H20 M9 3 H15 M6 6 L7 22 H17 L18 6 M10 10 V18 M14 10 V18')
    elif name == 'duplicate':
        s.rect(8, 8, 13, 14, 'none', 'line', 2, 1.6)
        s.path('M16 5 V2 H3 V17 H5')
    elif name in ('more', 'effect_chain'):
        for x in (4, 12, 20):
            s.circle(x, 12, 1.8, 'line' if name == 'more' else 'none', 'line', 1.6)
    elif name == 'plugin_fx':
        s.path('M3 20 V4 H11 M3 11 H9 M14 4 L22 20 M22 4 L14 20', sw=1.8)
    elif name == 'third_party':
        s.path('M12 2 L18 5 V12 L12 15 L6 12 V5 Z M6 12 L12 15 V22 L6 19 L1 22 V15 Z M18 12 L23 15 V22 L18 19 L12 22 M12 2 V9 L18 5 M12 9 L6 5 M18 19 V12 M6 19 V12')
    elif name.startswith('automation_'):
        role = name.removeprefix('automation_')
        if role == 'off':
            s.circle(12, 12, 8, c='muted')
            s.path('M3 21 L21 3', 'line', 1.8)
        else:
            c = dict(normal='blue', read='green', write='red', latch='amber', touch='violet')[role]
            points = {'normal': [(3, 19), (11, 11), (21, 5)],
                      'read': [(3, 19), (11, 11), (21, 5)],
                      'write': [(3, 19), (11, 11), (21, 5)],
                      'latch': [(3, 19), (10, 9), (21, 9)],
                      'touch': [(3, 19), (11, 9), (16, 9), (21, 16)]}[role]
            s.path('M'+' L'.join(f'{x} {y}' for x, y in points), c, 1.6)
            for x, y in (points if not compact else (points[0], points[-1])):
                s.circle(x, y, 2, 'none' if role == 'read' else c, c, 1.2)
            if role == 'write':
                s.path('M17 19 L21 15 L22 16 L18 20 Z', c, 1, c)
    elif name == 'send':
        s.path('M2 12 H21 M15 5 L22 12 L15 19')
    elif name == 'bus':
        s.el('ellipse', cx=12, cy=5, rx=8, ry=3, fill='none', stroke=P['line'], stroke_width=1.6)
        s.path('M4 5 V19 C4 23 20 23 20 19 V5 M4 12 C4 16 20 16 20 12')
    elif name == 'group':
        s.circle(9, 10, 6)
        s.circle(15, 15, 6)
    elif name == 'output':
        s.path('M14 3 H3 V21 H14 M10 12 H22 M17 7 L22 12 L17 17')
    elif name == 'marker':
        s.path('M5 22 V2 L21 7 L5 12', 'red')
    elif name in ('region', 'locator'):
        s.path('M5 3 H19 L12 20 Z M12 20 V23', 'cyan' if name == 'region' else 'line')
    elif name == 'playhead':
        s.path('M8 2 H16 L12 7 Z', 'line', 1, 'line')
        s.path('M12 8 V23', 'line', 1, stroke_dasharray='2 2')
    elif name == 'loop_range':
        s.path('M4 3 V21 M4 12 H9 M20 3 V21 M15 12 H20', 'blue', 2)
    elif name == 'favorite':
        s.path('M12 2 L15 9 L22 10 L17 15 L18 22 L12 18 L6 22 L7 15 L2 10 L9 9 Z')
    elif name == 'recent':
        s.circle(12, 12, 9)
        s.path('M12 5 V12 L17 15')
    elif name == 'cloud':
        s.path('M6 20 C-1 20 0 10 6 10 C6 0 20 0 20 11 C26 12 24 20 19 20 Z')
    elif name == 'drive':
        s.path('M3 16 L7 3 H17 L21 16 V22 H3 Z M3 16 H21 M17 19 H18')
    elif name in ('lock', 'unlock'):
        s.rect(4, 10, 16, 12, 'none', 'line', 2, 1.6)
        s.path('M8 10 V6 C8 0 16 0 16 6'+(' V10' if name == 'lock' else ''))
        s.circle(12, 15, 1, 'line', 'line', 1)
        s.line(12, 16, 12, 18, sw=1.6)
    elif name in ('show', 'hide'):
        s.path('M1 12 Q12 -3 23 12 Q12 27 1 12 Z')
        s.circle(12, 12, 3.2)
        if name == 'hide':
            s.path('M3 22 L21 2', 'line', 2)
    elif name == 'grid':
        for x in (3, 10, 17):
            for y in (3, 10, 17):
                s.rect(x, y, 4, 4, 'none', 'line', .3, 1.2)
    elif name == 'list':
        for y in (5, 12, 19):
            s.circle(3, y, 1, 'line', 'line', 1)
            s.line(8, y, 22, y, sw=1.6)
    else:
        raise ValueError(name)
    return s


NAMES = ('track_drums track_bass track_piano track_guitar track_synth track_fx '
         'track_backing_vocals track_master folder folder_open search mixer_params delete '
         'duplicate more effect_chain plugin_fx third_party automation_normal automation_read '
         'automation_write automation_latch automation_touch automation_off send bus group '
         'output marker region locator playhead loop_range favorite recent cloud drive lock '
         'unlock show hide grid list').split()
for name in NAMES:
    family = 'tracks' if name.startswith('track_') else 'automation' if name.startswith('automation_') else 'utilities'
    save('icons', name, glyph(name), family=family, kind='glyph', lod='standard_24')
    if family == 'tracks':
        s = SVG(16, 16)
        s.parts.append('<g transform="scale(0.666666667)">'+''.join(glyph(name, True).parts)+'</g>')
        save('icons', f'{name}_compact', s, family=family, kind='glyph', lod='compact_16')


# Deterministic decorative waveform silhouettes; never presented as audio data.
for n, color in enumerate(('cyan', 'violet', 'blue', 'amber')):
    s = SVG(160, 56)
    s.rect(1, 1, 158, 54, 'surface', 'faint', 5)
    for x in range(5, 156, 2):
        if n == 3:
            amp = 2+19*math.exp(-((x-7) % 30)/7)*(0.7+0.3*abs(math.sin(x*1.7)))
        else:
            amp = 2+20*abs(math.sin(x*.067+n)*math.sin(x*1.91))*math.sin(math.pi*x/160)
        s.line(x, 28-amp, x, 28+amp, color, 1)
    save('controls', f'waveform_{("audio", "vocal", "instrument", "loop")[n]}_illustration',
         s, family='waveforms', kind='illustration', measured_data=False)


# Catalog records exact ownership, semantics and reuse instead of copying icons.
catalog = dict(name='MR Studio Faders, Controls, Tracks & Utilities', version='1.0',
               pack=PACK, license='Original vector artwork authored for MR Studio; no third-party artwork embedded.',
               references=['images/2fc6086f-00b5-4b1a-92e5-b780074e56a0.png',
                           'images/5463ba7e-e3b0-4dd3-9a99-133245b32b59.png'],
               integration='Static asset library only. Host owns input, accessibility, values, units, mapping and DSP.',
               assets=ASSETS, reused=REUSE)
(ROOT/'catalogs').mkdir(exist_ok=True)
(ROOT/'catalogs'/f'{PACK}.json').write_text(json.dumps(catalog, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
(ROOT/'tokens'/'concept_pack.json').write_text(json.dumps(dict(
    palette=P, scope='Concept pack only; does not change ui::Theme', icon_stroke=1.6,
    spacing=[4, 8, 16, 24], host_minimum_target=32,
    mapping_to_uds=dict(cyan='accent', blue='accent', violet='violet', red='red',
                        amber='amber', green='green', yellow='amber'),
    fader_geometry=dict(micro=dict(axis='y', start=188, end=12),
                        precision=dict(axis='y', start=228, end=12),
                        slide=dict(axis='x', start=12, end=212)),
    fader_formula='position = start + normalized * (end - start); normalized is supplied by host mapping.',
    fader_sample='Static examples are 0.55, rounded to logical pixels; not dB values.'), indent=2)+'\n', encoding='utf-8')
manifest = json.loads((ROOT/'manifest.json').read_text(encoding='utf-8'))
manifest['assets'] = [a for a in manifest['assets'] if a.get('pack') != PACK] + ASSETS
manifest['version'] = '0.2'
manifest['total_asset_svg'] = len(manifest['assets'])
manifest['extension_catalog'] = f'catalogs/{PACK}.json'
(ROOT/'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')

# Static, offline, accessible gallery. Labels are HTML, never baked into assets.
page = '''<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width">
<title>MR Studio — Concept SVG Library</title><style>
*{box-sizing:border-box}body{margin:0;background:#111720;color:#DAE5F4;font:14px system-ui,sans-serif}
header,main{max-width:1500px;margin:auto;padding:24px}header{border-bottom:1px solid #4B5F7B}
h1{font-size:30px;font-weight:500;margin:0 0 8px}h1 b{color:#00B8EF}p{color:#A9BFDC;line-height:1.6}
a{color:#91BBEE}h2{text-transform:uppercase;letter-spacing:2px;font-size:16px;margin:32px 0 16px;color:#91BBEE}
section{display:grid;grid-template-columns:repeat(auto-fill,minmax(154px,1fr));gap:10px}
figure{margin:0;background:#192333;border:1px solid #34465E;border-radius:8px;overflow:hidden}
.art{height:180px;display:flex;align-items:center;justify-content:center;padding:12px;background:#111C29}
.art img{max-width:100%;max-height:100%;object-fit:contain}figcaption{padding:10px;font-size:11px;min-height:48px;overflow-wrap:anywhere}
.glyph .art{height:96px}.glyph img{width:24px;height:24px}.glyph.compact img{width:16px;height:16px}
.badge{color:#7188AB}nav{display:flex;gap:14px;flex-wrap:wrap} .scales{display:flex;gap:24px;align-items:center;flex-wrap:wrap}
.scales span{display:flex;align-items:center;gap:12px}button{font:inherit}
</style><header><h1><b>MR</b> Studio — Faders, Controls, Tracks & Utilities</h1>
<p>Original vector assets · v1.0 · transparent canvases · offline catalog<br>
State samples and decorative waveforms are design references. Values, labels and live data belong to the host.</p>'''
families = list(dict.fromkeys(a['family'] for a in ASSETS))
page += '<nav>'+''.join(f'<a href="#{f}">{f.replace("_", " ")}</a>' for f in families)+'</nav></header><main>'
for family in families:
    page += f'<h2 id="{family}">{family.replace("_", " ")}</h2><section>'
    for a in (a for a in ASSETS if a['family'] == family):
        label = a['id'].split('/')[-1]
        css = 'glyph compact' if a.get('lod') == 'compact_16' else 'glyph' if a.get('kind') == 'glyph' else ''
        page += (f'<figure class="{css}"><div class="art"><img src="../{a["file"]}" alt="{label}"></div>'
                 f'<figcaption><a href="../{a["file"]}">{label}</a><br><span class="badge">'
                 f'{a["size"][0]} × {a["size"][1]} · {a.get("kind", "state")}</span></figcaption></figure>')
    page += '</section>'
page += '<h2>Existing canonical icons — reused</h2><section>'
for a in REUSE:
    page += (f'<figure class="glyph"><div class="art"><img src="../{a["file"]}" alt="{a["role"]}"></div>'
             f'<figcaption>{a["role"]}<br><a href="../{a["file"]}">Original SVG</a></figcaption></figure>')
page += '</section><h2>Software scale samples · 100 / 125 / 150 / 200%</h2><div class="scales">'
for scale in (1, 1.25, 1.5, 2):
    path = next(a['file'] for a in ASSETS if a['id'].endswith('/track_drums'))
    page += f'<span>{scale*100:g}% <img src="../{path}" width="{24*scale:g}" height="{24*scale:g}" alt="drums scale"></span>'
page += f'</div><p>{len(ASSETS)} new SVG files; {len(REUSE)} canonical references. '
page += 'No runtime integration. <a href="index.html">Original library</a> · <a href="object_palette.html">Object palette / 24 clip colors</a></p></main></html>'
(ROOT/'previews'/'concept_pack.html').write_text(page, encoding='utf-8')
print(f'{len(ASSETS)} SVG assets; {len(REUSE)} reused references; manifest total {len(manifest["assets"])}')

# Keep the canonical offline viewer in sync with every SVG folder.
from build_svg_gallery import build_gallery
build_gallery()
